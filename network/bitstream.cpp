#include "bitstream.hpp"

#include <enet/enet.h>
#include <enet/types.h>
#include <stdlib.h>

#include <cstdint>
#include <stdexcept>

#include "crc_hash.hpp"
#include "logging.hpp"

namespace rdm::network {
BitStream::BitStream() {
  data = 0;
  c = 0;
  size = 0;
}
BitStream::BitStream(BitStream& stream) {
  data = (char*)malloc(stream.size);
  c = 0;
  size = stream.size;
  memcpy(data, stream.data, size);
}
BitStream::BitStream(void* data, size_t size) {
  this->data = (char*)malloc(size);
  this->c = 0;
  this->size = size;
  this->ctxt = Generic;
  memcpy(this->data, data, size);
}

BitStream::~BitStream() {
  if (data) free(data);
}

void BitStream::makeSpaceFor(size_t s) {
  if (size) {
    if (s + c > size) {
      size_t newSize = size * 2;
      size = std::max(s + c, newSize);
      data = (char*)realloc(data, size);
    }
  } else {
    size = s;
    data = (char*)malloc(size);
  }
}

bool BitStream::isSpaceFor(size_t s) {
  if (s + c > size) {
    return false;
    // throw std::runtime_error("Out of space on BitStream");
  }
  return true;
}

void BitStream::writeStream(const BitStream& stream) {
  for (int i = 0; i < stream.size - stream.c; i++) write<char>(stream.data[i]);
}

void BitStream::writeString(std::string s) {
  write<uint16_t>(s.size());
  for (int i = 0; i < s.size(); i++) write<char>(s[i]);
}

std::string BitStream::readString() {
  std::string s;
  uint16_t size = read<uint16_t>();
  s.resize(size);
  for (int i = 0; i < size; i++) s[i] = read<char>();
  return s;
}

void BitStream::writeSignedMessage(SignedMessage msg) {
  write<uint16_t>(msg.data.size());
  for (int i = 0; i < msg.data.size(); i++) write<char>(msg.data[i]);
  writeString(msg.sig);
  writeString(msg.key);
}

SignedMessage BitStream::readSignedMessage() {
  SignedMessage msg;
  uint16_t size = read<uint16_t>();
  msg.data.resize(size);
  for (int i = 0; i < size; i++) msg.data[i] = read<char>();
  msg.sig = readString();
  msg.key = readString();
  return msg;
}

ENetPacket* BitStream::createPacket(enet_uint32 flags) {
  return enet_packet_create(data, c, flags);
}

std::vector<unsigned char> BitStream::getDataVec() {
  std::vector<unsigned char> data;
  data.resize(c);
  memcpy(data.data(), this->data, data.size());
  return data;
}
}  // namespace rdm::network
