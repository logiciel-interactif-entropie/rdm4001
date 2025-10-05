#include "pak_file.hpp"

#include <linux/limits.h>
#include <string.h>

#include <cassert>
#include <cstdio>
#include <stdexcept>

#include "filesystem.hpp"
#include "logging.hpp"
namespace pak {
void PakFile::init() {
  if (!io) {
    throw std::runtime_error("No pakfile io");
  }

  PakFileHeader hdr;
  io->read(&hdr, sizeof(hdr));

  if (hdr.ident[0] != PAKF_HEADER_0 || hdr.ident[1] != PAKF_HEADER_1 ||
      hdr.ident[2] != PAKF_HEADER_2 || hdr.ident[3] != PAKF_HEADER_3) {
    throw std::runtime_error("Invalid pak header");
  }

  pakEntries.resize(hdr.numPakEntries);
  io->seek(hdr.pakEntriesOffset, SEEK_SET);
  io->read(pakEntries.data(), hdr.numPakEntries * sizeof(PakFileEntry));

  std::vector<PakFileString> pakStrings;

  pakStrings.resize(hdr.numStrings);
  io->seek(hdr.stringsOffset, SEEK_SET);
  io->read(pakStrings.data(), hdr.numStrings * sizeof(PakFileString));

  for (int i = 0; i < pakStrings.size(); i++) {
    char buf[PATH_MAX];
    memset(buf, 0, sizeof(buf));
    io->seek(pakStrings[i].stringOffset, SEEK_SET);
    io->read(buf, std::min((size_t)PATH_MAX - 1, pakStrings[i].stringSize));
    std::string str(buf);
    pakStringsMap[i] = str;
  }

  for (int i = 0; i < pakEntries.size(); i++) {
    PakFileEntry& entry = pakEntries[i];
    fileNameMap[pakStringsMap[entry.nameStringIdx]] = i;
  }

  isGeneral = false;
}

PakFileIO::PakFileIO(PakFile* file, size_t offset, size_t size,
                     common::FileIO* io) {
  this->offset = offset;
  this->size = size;
  this->cursor = 0;
  this->io = io;
  this->file = file;
}

size_t PakFileIO::seek(size_t pos, int whence) {
  switch (whence) {
    case SEEK_SET:
      cursor = std::min(size, pos);
      break;
    case SEEK_CUR:
      cursor = std::min(size, cursor + pos);
      break;
    case SEEK_END:
      cursor = std::max((size_t)0, size - cursor);
      break;
  }
  return cursor;
}

size_t PakFileIO::fileSize() { return size; }

size_t PakFileIO::tell() { return cursor; }

size_t PakFileIO::read(void* out, size_t size) {
  std::scoped_lock l(file->m);

  size_t sz = std::min(size, this->size - cursor);
  io->seek(offset + cursor, SEEK_SET);
  return io->read(out, sz);
}

std::optional<common::FileIO*> PakFile::getFileIO(const char* path,
                                                  const char* mode) {
  assert(mode[0] == 'r');

  if (fileNameMap.find(path) != fileNameMap.end()) {
    PakFileEntry& entry = pakEntries[fileNameMap[path]];
    switch (entry.type) {
      case None:
        return new PakFileIO(this, entry.dataOffset, entry.dataSize, io);
      default:
        rdm::Log::printf(rdm::LOG_ERROR, "Unsupported compression");
        return {};
    }
  } else {
    return {};
  }
}
}  // namespace pak
