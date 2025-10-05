#include "filesystem.hpp"

#include <string.h>

#ifdef __linux
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cstdlib>

#include "logging.hpp"

namespace common {
static FileSystem* _singleton = 0;
FileSystem* FileSystem::singleton() {
  if (!_singleton) _singleton = new FileSystem();
  return _singleton;
}

FileSystem::FileSystem() {
#ifndef NDEBUG
  addApi(new DataFolderAPI("../subprojects/rdm4001/data/"), "data_rdm4001");
#else
  addApi(new DataFolderAPI(), "data_rdm4001");
#endif
  addApi(new DataFolderAPI(), "data_local");
}

void FileSystem::addApi(FileSystemAPI* fapi, std::string uri, int precedence,
                        bool exclusive) {
  if (exclusive) fsApis.clear();
  FSApiInfo& info = fsApis[uri];
  info.precedence = precedence;
  info.api = std::unique_ptr<FileSystemAPI>(fapi);
};

FileSystemAPI* FileSystem::getOwningApi(const char* _path) {
  std::string path(_path);
  auto urisep = path.find("://");
  if (urisep != std::string::npos) {
    std::string uri = path.substr(0, urisep);
    auto api = fsApis.find(uri);
    if (api != fsApis.end()) {
      return api->second.api.get();
    } else {
      rdm::Log::printf(rdm::LOG_WARN, "Could not find URI %s", uri.c_str());
    }
  } else {
    std::vector<FSApiInfo*> infos;
    for (auto& api : fsApis) {
      if (api.second.api->generalFSApi()) infos.push_back(&api.second);
    }
    std::sort(infos.begin(), infos.end(), [](FSApiInfo* a, FSApiInfo* b) {
      return a->precedence > b->precedence;
    });
    for (auto info : infos) {
      if (info->api->getFileExists(_path)) {
        return info->api.get();
      }
    }
  }
  return NULL;
}

std::string FileSystem::sanitizePath(const char* _path) {
  std::string path(_path);
  auto urisep = path.find("://");
  if (urisep != std::string::npos) {
    return path.substr(urisep + 3);
  } else
    return path;
}

OptionalData FileSystem::readFile(const char* path) {
  if (FileSystemAPI* api = getOwningApi(path))
    return api->getFileData(sanitizePath(path).c_str());

  return {};
}

std::optional<FileIO*> FileSystem::getFileIO(const char* path,
                                             const char* mode) {
  if (FileSystemAPI* api = getOwningApi(path))
    return api->getFileIO(sanitizePath(path).c_str(), mode);

  rdm::Log::printf(rdm::LOG_EXTERNAL, "Unable to load file %s", path);
  return {};
}

DataFolderAPI::DataFolderAPI(std::string basedir) {
  if (basedir.empty()) {
#ifndef NDEBUG
    this->basedir = "../data/";
#else
    this->basedir = "data/";
#endif
  } else {
    this->basedir = basedir;
  }
}

char* normalize_path(const char* src, size_t src_len) {
  char* res;
  size_t res_len;

  const char* ptr = src;
  const char* end = &src[src_len];
  const char* next;

  if (src_len == 0 || src[0] != '/') {
    // relative path

    char pwd[PATH_MAX];
    size_t pwd_len;

    if (getcwd(pwd, sizeof(pwd)) == NULL) {
      return NULL;
    }

    pwd_len = strlen(pwd);
    res = (char*)malloc(pwd_len + 1 + src_len + 1);
    memcpy(res, pwd, pwd_len);
    res_len = pwd_len;
  } else {
    res = (char*)malloc((src_len > 0 ? src_len : 1) + 1);
    res_len = 0;
  }

  for (ptr = src; ptr < end; ptr = next + 1) {
    size_t len;
    next = (const char*)memchr(ptr, '/', end - ptr);
    if (next == NULL) {
      next = end;
    }
    len = next - ptr;
    switch (len) {
      case 2:
        if (ptr[0] == '.' && ptr[1] == '.') {
          const char* slash = (const char*)memrchr(res, '/', res_len);
          if (slash != NULL) {
            res_len = slash - res;
          }
          continue;
        }
        break;
      case 1:
        if (ptr[0] == '.') {
          continue;
        }
        break;
      case 0:
        continue;
    }
    res[res_len++] = '/';
    memcpy(&res[res_len], ptr, len);
    res_len += len;
  }

  if (res_len == 0) {
    res[res_len++] = '/';
  }
  res[res_len] = '\0';
  return res;
}

void DataFolderAPI::checkProperDir(const char* path) {
  // TODO: fix function to properly check if paths are not allowed
  /*char real[PATH_MAX];
  realpath((basedir + path).c_str(), real);
  char realbase[PATH_MAX];
  realpath(basedir.c_str(), realbase);

  std::string realbase_s(realbase);
  std::string realpath_s(real);

  for (int i = 0; i < realbase_s.length(); i++)
    if (realpath_s[i] != realbase_s[i])
    throw std::runtime_error("Path outside of base");*/
}

bool DataFolderAPI::getFileExists(const char* path) {
  checkProperDir(path);

  FILE* fp = fopen((basedir + path).c_str(), "r");
  bool ex = fp;
  if (fp) fclose(fp);
  return ex;
}

OptionalData DataFolderAPI::getFileData(const char* path) {
  checkProperDir(path);

  FILE* fp = fopen((basedir + path).c_str(), "rb");
  if (fp) {
    fseek(fp, 0, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<unsigned char> v(sz);
    fread(v.data(), sz, 1, fp);
    fclose(fp);
    return v;
  } else {
    return {};
  }
}

std::optional<FileIO*> DataFolderAPI::getFileIO(const char* path,
                                                const char* mode) {
  checkProperDir(path);

  FILE* fp = fopen((basedir + path).c_str(), mode);
  if (fp) {
    return new DataFileIO(fp);
  } else {
    return {};
  }
}

DataFileIO::DataFileIO(FILE* file) { this->file = file; }

DataFileIO::~DataFileIO() { fclose(file); }

size_t DataFileIO::seek(size_t pos, int whence) {
#ifdef DEBUG_DFIO
  rdm::Log::printf(rdm::LOG_DEBUG, "seek: %i, %i", pos, whence);
#endif
  fseek(file, pos, whence);
  return tell();
}

size_t DataFileIO::fileSize() {
  size_t cur = ftell(file);
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  fseek(file, cur, SEEK_SET);
#ifdef DEBUG_DFIO
  rdm::Log::printf(rdm::LOG_DEBUG, "size: %i", size);
#endif
  return size;
}

size_t DataFileIO::tell() {
#ifdef DEBUG_DFIO
  rdm::Log::printf(rdm::LOG_DEBUG, "tell: %i", ftell(file));
#endif
  return ftell(file);
}

size_t DataFileIO::read(void* out, size_t size) {
  size_t _read = fread(out, 1, size, file);
#ifdef DEBUG_DFIO
  rdm::Log::printf(rdm::LOG_DEBUG, "read: %p, %i (%i)", out, size, _read);
#endif
  return _read;
}

size_t DataFileIO::write(const void* in, size_t size) {
  return fwrite(in, size, 1, file);
}

std::optional<std::string> DataFileIO::getLine() {
  char line[65535];
  if (fgets(line, 65535, file)) return line;
  return {};
}
};  // namespace common
