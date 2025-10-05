#include <dirent.h>
#include <lzma.h>
#include <string.h>

#include <cerrno>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <thread>

#include "pak_file.hpp"

struct PakTmpEntry {
  pak::PakFileEntry entry;
};

struct Fm {
  FILE* f;
  std::string path;
};

struct PakTmp {
  FILE* output;
  std::vector<pak::PakFileEntry> entries;
  std::vector<pak::PakFileString> stringEntries;
  std::vector<Fm> toProcess;
  std::mutex m;
  std::string rootDir;
};

static bool compress_lzma(uint8_t* inData, size_t inDataSize, uint8_t** outData,
                          int* outDataSize) {
  lzma_stream strm = LZMA_STREAM_INIT;
  lzma_ret ret = lzma_easy_encoder(&strm, 6, LZMA_CHECK_CRC32);
  if (ret != LZMA_OK) throw std::runtime_error("lzma_easy_encoder != LZMA_OK");
  uint8_t inbuf[BUFSIZ];
  uint8_t outbuf[BUFSIZ];
  strm.next_in = inbuf;
  strm.avail_in = sizeof(inbuf);
  strm.next_out = outbuf;
  strm.avail_out = sizeof(outbuf);
  size_t dataBufSize = BUFSIZ;
  uint8_t* dataBuf = (uint8_t*)malloc(dataBufSize);
  size_t writeCount = 0;
  size_t readCount = 0;
  while (true) {
    lzma_action action = LZMA_RUN;
    if (strm.avail_in == 0 && readCount < inDataSize) {
      strm.next_in = inbuf;
      size_t toRead = std::min(inDataSize - readCount, (size_t)BUFSIZ);
      memcpy(inbuf, inData + readCount, toRead);
      strm.avail_in = toRead;
      readCount += toRead;

      if (readCount >= inDataSize) {
        action = LZMA_FINISH;
      }
    }

    lzma_ret ret = lzma_code(&strm, action);
    if (strm.avail_out == 0 || ret == LZMA_STREAM_END) {
      size_t write_size = sizeof(outbuf) - strm.avail_out;
      if (dataBufSize < writeCount + write_size) {
        dataBufSize *= 2;
        dataBuf = (uint8_t*)realloc(dataBuf, dataBufSize);
      }
      memcpy(dataBuf + writeCount, outbuf, write_size);
      writeCount += write_size;

      strm.next_out = outbuf;
      strm.avail_out = sizeof(outbuf);
    }

    if (ret == LZMA_STREAM_END) break;
    switch (ret) {
      case LZMA_OK:
        break;
      case LZMA_MEM_ERROR:
        fprintf(stderr, "Memory allocation failed\n");
        free(dataBuf);
        return false;
      case LZMA_DATA_ERROR:
        fprintf(stderr, "File size limits exceeded\n");
        free(dataBuf);
        return false;
      default:
        // fprintf(stderr, "Compression failure\n");
        free(dataBuf);
        return false;
    }
  }

  *outData = dataBuf;
  *outDataSize = writeCount;

  lzma_end(&strm);
  return true;
}

void recurse_directory(DIR* dir, std::string path, PakTmp* tmp) {
  struct dirent* dp;

  while ((dp = readdir(dir)) != NULL) {
    std::string _path = path + dp->d_name;
    if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
      DIR* _dir = opendir((tmp->rootDir + _path).c_str());
      if (!_dir) {
        FILE* file = fopen((tmp->rootDir + _path).c_str(), "rb");
        if (!file) {
          printf("Failed opening %s\n", (tmp->rootDir + _path).c_str());
          continue;
        }
        fseek(file, 0, SEEK_END);
        if (ftell(file) == 0) {
          fclose(file);
          continue;
        }
        fseek(file, 0, SEEK_SET);
        Fm f;
        f.f = file;
        f.path = _path;
        tmp->toProcess.push_back(f);
      } else {
        _path += "/";
        recurse_directory(_dir, _path, tmp);
      }
    }
  }
  closedir(dir);
}

struct Progress {
  int count;
  int max;
  bool done;
  std::mutex m;
};

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "pak_file_creator <output file> <directory>\n");
    return 1;
  }

  PakTmp* tmp = new PakTmp;
  tmp->rootDir = std::string(argv[2]) + "/";
  tmp->output = fopen(argv[1], "wb");

  if (!tmp->output) {
    fprintf(stderr, "Could not open %s (%s)", argv[1], strerror(errno));
  }

  pak::PakFileHeader hdr;
  hdr.ident[0] = PAKF_HEADER_0;
  hdr.ident[1] = PAKF_HEADER_1;
  hdr.ident[2] = PAKF_HEADER_2;
  hdr.ident[3] = PAKF_HEADER_3;

  fseek(tmp->output, 0x1000, SEEK_SET);

  DIR* root = opendir(argv[2]);
  if (!root) {
    fprintf(stderr, "could not find directory\n");
    return 1;
  }

  recurse_directory(root, "", tmp);
  printf("%i files to pack", tmp->toProcess.size());

  Progress p;
  p.count = 0;
  p.max = tmp->toProcess.size();
  p.done = false;

  std::thread pbar([&p] {
    int oldCount = 0;
    while (true) {
      if (p.count != oldCount) {
        printf("%0.2f%%\n", ((float)p.count / (float)p.max) * 100.f);
        oldCount = p.count;
      }

      if (p.done) break;

      std::this_thread::yield();
    }
  });
#pragma omp parallel for
  for (auto& f : tmp->toProcess) {
    pak::PakFileEntry entry;
    pak::PakFileString stringEntry;

    fseek(f.f, 0, SEEK_END);
    entry.dataSize = ftell(f.f);
    fseek(f.f, 0, SEEK_SET);

    void* data = malloc(entry.dataSize);
    fread(data, entry.dataSize, 1, f.f);

    // determine best compression

    uint8_t* dataOut;
    int dataOutSize;

#ifdef COMPRESS_LZMA
    if (compress_lzma((uint8_t*)data, entry.dataSize, &dataOut, &dataOutSize)) {
      entry.type = pak::Lzma;
      {
        std::scoped_lock l(tmp->m);
        entry.dataOffset = ftell(tmp->output);
        fwrite(dataOut, dataOutSize, 1, tmp->output);

        stringEntry.stringSize = f.path.size();
        stringEntry.stringOffset = ftell(tmp->output);
        fwrite(f.path.c_str(), stringEntry.stringSize, 1, tmp->output);

        entry.nameStringIdx = tmp->stringEntries.size();
        tmp->stringEntries.push_back(stringEntry);
        tmp->entries.push_back(entry);

        fflush(tmp->output);
      }
      free(dataOut);
      entry.dataSize = dataOutSize;
    } else {
#endif
      entry.type = pak::None;
      {
        std::scoped_lock l(tmp->m);
        entry.dataOffset = ftell(tmp->output);
        fwrite(data, entry.dataSize, 1, tmp->output);

        stringEntry.stringSize = f.path.size();
        stringEntry.stringOffset = ftell(tmp->output);
        fwrite(f.path.c_str(), stringEntry.stringSize, 1, tmp->output);

        entry.nameStringIdx = tmp->stringEntries.size();
        tmp->stringEntries.push_back(stringEntry);
        tmp->entries.push_back(entry);

        fflush(tmp->output);
      }
#ifdef COMPRESS_LZMA
    }
#endif

    {
      std::scoped_lock l(p.m);
      p.count++;
    }

    free(data);
  }

  printf("DONE (%i, %i)\n", p.count, p.max);
  p.done = true;
  pbar.join();

  hdr.numStrings = tmp->stringEntries.size();
  hdr.stringsOffset = ftell(tmp->output);
  for (int i = 0; i < tmp->stringEntries.size(); i++) {
    fwrite(&tmp->stringEntries[i], sizeof(pak::PakFileString), 1, tmp->output);
  }

  hdr.numPakEntries = tmp->entries.size();
  hdr.pakEntriesOffset = ftell(tmp->output);
  for (int i = 0; i < tmp->entries.size(); i++) {
    fwrite(&tmp->entries[i], sizeof(pak::PakFileEntry), 1, tmp->output);
  }

  fseek(tmp->output, 0x0, SEEK_SET);

  fwrite(&hdr, sizeof(pak::PakFileHeader), 1, tmp->output);

  fclose(tmp->output);

  return 0;
}
