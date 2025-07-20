#include "sound.hpp"

#include <sndfile.h>

#include <algorithm>

#include "al.h"
#include "alc.h"
#include "console.hpp"
#include "filesystem.hpp"
#include "game.hpp"
#include "glad/glad.h"
#include "scheduler.hpp"
#include "settings.hpp"

namespace rdm {
static CVar snd_master_gain("snd_master_gain", "0.75",
                            CVARF_SAVE | CVARF_GLOBAL);

SoundCache::SoundCache(SoundManager* manager) { this->manager = manager; }

SoundEmitter::SoundEmitter(SoundManager* manager) {
  this->parent = manager;
  alGenSources(1, &source);
  buffer = 0;
  playingSound = NULL;
  playing = false;
  setLooping(false);
  setGain(1.f);
  setPitch(1.f);
  node = 0;
  for (int i = 0; i < 10; i++) streamBuffer[i] = 0;
}

void SoundEmitter::play(Sound* sound) {
  if (sound->getLoadType() == Sound::Buffer ||
      sound->getLoadType() == Sound::Dynamic) {
    if (buffer) alDeleteBuffers(1, &buffer);
    alGenBuffers(1, &buffer);
    sound->uploadData(buffer);
    alSourcei(source, AL_BUFFER, buffer);

    if (sound->getLoadType() == Sound::Dynamic) {
      setLooping(true);
    }
  } else if (sound->getLoadType() == Sound::Stream) {
    if (streamBuffer[0]) alDeleteBuffers(10, streamBuffer);
    alGenBuffers(10, streamBuffer);
    for (int i = 0; i < 10; i++) sound->uploadData(streamBuffer[i]);
    alSourceQueueBuffers(source, 10, streamBuffer);

    Log::printf(LOG_DEBUG, "Playing stream");

    if (looping) alSourcei(source, AL_LOOPING, 0);
  }

  alSourcePlay(source);

  playing = true;
  playingSound = sound;
}

void SoundEmitter::stop() {
  alSourceStop(source);

  playing = false;
  playingSound = NULL;
}

void SoundEmitter::setLooping(bool looping) {
  if (playingSound && playingSound->getLoadType() == Sound::Buffer)
    alSourcei(source, AL_LOOPING, looping);
  this->looping = looping;
}

void SoundEmitter::setPitch(float pitch) {
  alSourcef(source, AL_PITCH, pitch);
  this->pitch = pitch;
}

void SoundEmitter::setGain(float gain) {
  alSourcef(source, AL_GAIN, gain * snd_master_gain.getFloat());
  this->gain = gain;
}

void SoundEmitter::service() {
  if (playingSound && playing) {
    if (playingSound->getLoadType() == Sound::Stream) {
      ALint processedBuffers;
      alGetSourcei(source, AL_BUFFERS_PROCESSED, &processedBuffers);
      while (processedBuffers) {
        ALuint buffer;
        alSourceUnqueueBuffers(source, 1, &buffer);
        if (playingSound->uploadData(buffer, looping)) {
          alSourceQueueBuffers(source, 1, &buffer);
        } else {
          playing = false;
        }
        processedBuffers--;
      }
    }
    /**
     * @todo Support for Sound::Dynamic, they will not play properly at the
     * moment
     */

    if (node) {
      alSource3f(source, AL_POSITION, node->origin.x, node->origin.y,
                 node->origin.z);
      alSource3f(source, AL_VELOCITY, 0.f, 0.f, 0.f);
    }
  }
}

static sf_count_t sndfileLength(void* userdata) {
  common::FileIO* io = (common::FileIO*)userdata;
  return io->fileSize();
}

static sf_count_t sndfileSeek(sf_count_t offset, int whence, void* userdata) {
  common::FileIO* io = (common::FileIO*)userdata;
  return io->seek(offset, whence);
}

static sf_count_t sndfileRead(void* ptr, sf_count_t count, void* userdata) {
  common::FileIO* io = (common::FileIO*)userdata;
  return io->read(ptr, count);
}

static sf_count_t sndfileWrite(const void* ptr, sf_count_t count,
                               void* userdata) {
  common::FileIO* io = (common::FileIO*)userdata;
  return io->write(ptr, count);
}

static sf_count_t sndfileTell(void* userdata) {
  common::FileIO* io = (common::FileIO*)userdata;
  return io->tell();
}

static SF_VIRTUAL_IO sndfileApi = {
    .get_filelen = sndfileLength,
    .seek = sndfileSeek,
    .read = sndfileRead,
    .write = sndfileWrite,
    .tell = sndfileTell,
};

class SoundBuffer : public Sound {
  SF_INFO info;
  SNDFILE* file;

  std::vector<int16_t> data;

 public:
  SoundBuffer(SF_INFO info, SNDFILE* file) {
    this->info = info;
    this->file = file;

    std::vector<int16_t> read;
    read.resize(4096);

    size_t read_size = 0;
    while ((read_size = sf_read_short(file, read.data(), read.size())) != 0) {
      data.insert(data.end(), read.begin(), read.begin() + read_size);
    }
  }

  virtual bool uploadData(ALuint buffer, bool loop) {
    alBufferData(
        buffer, info.channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16,
        &data.front(), data.size() * sizeof(uint16_t), info.samplerate);
    return true;
  }

  virtual LoadType getLoadType() { return Buffer; }

  virtual ~SoundBuffer() { sf_close(file); }
};

class SoundStream : public Sound {
  SF_INFO info;
  SNDFILE* file;

 public:
  SoundStream(SF_INFO info, SNDFILE* file) {
    this->info = info;
    this->file = file;
  }

  virtual bool uploadData(ALuint buffer, bool loop) {
    size_t read_size = 0;
    std::vector<int16_t> read;
    read.resize(4096);
    read_size = sf_read_short(file, read.data(), read.size());
    if (read_size == 0) {
      sf_seek(file, 0, SEEK_SET);
      if (!loop) return false;
      read_size = sf_read_short(file, read.data(), read.size());
    }
    alBufferData(
        buffer, info.channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16,
        &read.front(), read.size() * sizeof(uint16_t), info.samplerate);
    return true;
  }

  virtual LoadType getLoadType() { return Stream; }

  virtual ~SoundStream() { sf_close(file); }
};

std::optional<Sound*> SoundCache::get(std::string name, Sound::LoadType type) {
  auto it = sounds.find(name);
  if (it != sounds.end()) {
    return sounds[name].get();
  } else {
    std::optional<common::FileIO*> fio =
        common::FileSystem::singleton()->getFileIO(name.c_str(), "r");
    if (fio) {
      SF_INFO info;
      SNDFILE* file =
          sf_open_virtual(&sndfileApi, SFM_READ, &info, fio.value());
      if (!file) {
        Log::printf(LOG_ERROR, "sf_open_virtual = NULL, %s", sf_strerror(file));
        throw std::runtime_error("sf_open_virtual = NULL");
      }

      Sound* sound;
      switch (type) {
        case Sound::Stream:
          sound = new SoundStream(info, file);
          break;
        case Sound::Buffer:
        default:
          sound = new SoundBuffer(info, file);
          break;
      }
      sounds[name].reset(sound);
      return sounds[name].get();
    } else {
      Log::printf(LOG_ERROR, "Could not open sound file %s", name.c_str());
      return {};
    }
  }
}

class SoundJob : public SchedulerJob {
  SoundManager* soundManager;

 public:
  SoundJob(SoundManager* manager)
      : SchedulerJob("Sound"), soundManager(manager) {}

  virtual Result step() {
    soundManager->service();
    return Stepped;
  }
};

SoundManager::SoundManager(World* world) {
  this->world = world;
  world->getScheduler()->addJob(new SoundJob(this));
  listenerNode = 0;

  cache.reset(new SoundCache(this));

  device = alcOpenDevice(NULL);
  if (!device) throw std::runtime_error("Could not open OpenAL device");

  ALboolean enumeration = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
  if (enumeration) {
    const ALCchar* devices = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    const ALCchar *device = devices, *next = devices + 1;
    size_t len = 0;

    while (device && *device != '\0' && next && *next != '\0') {
      this->devices.push_back(std::string(device));
      Log::printf(LOG_DEBUG, "Sound Device: %s", device);
      len = strlen(device);
      device += (len + 1);
      next += (len + 2);
    }
  }

  context = alcCreateContext(device, NULL);
  if (!context)
    if (!device) throw std::runtime_error("Could not create OpenAL context");
  alcMakeContextCurrent(context);
  contextMadeCurrent = false;

  alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
}

SoundManager::~SoundManager() { alcCloseDevice(device); }

void SoundManager::printError() {
  ALCenum error;

  error = alGetError();
  if (error != AL_NO_ERROR) {
    Log::printf(LOG_DEBUG, "OpenAL error %x", error);
  }
}

SoundEmitter* SoundManager::newEmitter() {
  SoundEmitter* emitter = new SoundEmitter(this);
  emitters.push_back(emitter);
  return emitter;
}

void SoundManager::delEmitter(SoundEmitter* emitter) {
  auto it = std::find(emitters.begin(), emitters.end(), emitter);
  if (it != emitters.end()) {
    emitters.erase(it);  // Erase IT.
  } else {
    Log::printf(LOG_ERROR, "Could not delete emitter %p", emitter);
  }
}

SoundEmitter::~SoundEmitter() {
  if (parent)
    parent->delEmitter(this);  // since we should always come from a
                               // newEmitter call we should do taht
  alSourceStop(source);
  alDeleteSources(1, &source);
}

void SoundManager::service() {
  if (!contextMadeCurrent) {
    if (!alcMakeContextCurrent(context)) {
      throw std::runtime_error("Could not make context current");
    }
  }

  if (listenerNode) {
    glm::vec3 up = glm::vec3(0, 0, 1) * listenerNode->basis;
    glm::vec3 front = glm::vec3(1, 0, 0) * listenerNode->basis;

    ALfloat listenerOri[] = {front[0], front[1], front[2], up[0], up[1], up[2]};

    glm::vec3 position = listenerNode->origin;
    alListener3f(AL_POSITION, position[0], position[1], position[2]);
    alListener3f(AL_VELOCITY, 0, 0, 0);
    alListenerfv(AL_ORIENTATION, listenerOri);
  }

  for (auto emitter : emitters) emitter->service();
}

void SoundManager::stopAll() {
  for (auto emitter : emitters) emitter->stop();
}

static ConsoleCommand sound_stopall("sound_stopall", "sound_stopall",
                                    "stops all sounds",
                                    [](Game* game, ConsoleArgReader r) {
                                      game->getSoundManager()->stopAll();
                                    });
}  // namespace rdm
