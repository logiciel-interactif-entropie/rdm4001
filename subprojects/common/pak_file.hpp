#pragma once
#include <unordered_map>

#include "filesystem.hpp"
namespace pak {
#define PAKF_HEADER_0 'P'
#define PAKF_HEADER_1 'A'
#define PAKF_HEADER_2 'K'
#define PAKF_HEADER_3 'R'

struct __attribute__((packed)) PakFileHeader {
  char ident[4];
  uint64_t pakEntriesOffset;
  uint64_t numPakEntries;
  uint64_t stringsOffset;
  uint64_t numStrings;
};

enum CompressionType {
  None,
  Zstd,
  Lzma,
};

struct __attribute__((packed)) PakFileEntry {
  uint64_t nameStringIdx;
  uint64_t licenseStringIdx;
  CompressionType type;
  uint64_t dataOffset;
  uint64_t dataSize;
};

struct __attribute__((packed)) PakFileString {
  uint64_t stringSize;
  uint64_t stringOffset;
};

class PakFile;
class PakFileIO : public common::FileIO {
  friend class PakFile;

  PakFile* file;
  common::FileIO* io;

  size_t offset;
  size_t size;
  size_t cursor;

  PakFileIO(PakFile* file, size_t offset, size_t size, common::FileIO* io);

 private:
  virtual size_t seek(size_t pos, int whence);
  virtual size_t fileSize();
  virtual size_t tell();

  virtual size_t read(void* out, size_t size);
};

class PakFile : public common::FileSystemAPI {
  common::FileIO* io;

  std::vector<PakFileEntry> pakEntries;
  std::unordered_map<int, std::string> pakStringsMap;
  std::unordered_map<std::string, int> fileNameMap;
  bool isGeneral;

  void init();

 public:
  PakFile(common::FileIO* io) {
    this->io = io;
    init();
  }
  PakFile(const char* path) {
    io = common::FileSystem::singleton()->getFileIO(path, "rb").value();
    init();
  }

  virtual bool generalFSApi() { return isGeneral; }
  virtual std::optional<common::FileIO*> getFileIO(const char* path,
                                                   const char* mode);

  void setGeneralFs(bool f) { isGeneral = f; }

  std::mutex m;
};
}  // namespace pak
