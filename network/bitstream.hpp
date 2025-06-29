#pragma once
#include <enet/enet.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <logging.hpp>
#include <string>
#include <typeinfo>
#include <vector>

#include "security.hpp"
namespace rdm::network {
class BitStreamException : public std::runtime_error {
  friend class BitStream;

  BitStreamException(const char* msg) : std::runtime_error(msg) {}
};

class BitStream {
  char* data;
  size_t size;
  size_t c;

  bool isSpaceFor(size_t s);
  void makeSpaceFor(size_t s);

 public:
  enum Context {
    Generic,

    ToClient,
    ToServer,

    FromClient,
    FromServer,

    ToServerLocal,
    ToClientLocal,

    ToNewClient,

    FromServerLocal,
    FromClientLocal,
  };

  BitStream();
  ~BitStream();

  void* getData() { return data; }
  size_t getSize() { return c; }

  BitStream(void* data, size_t size);
  BitStream(BitStream& stream);

  template <typename T>
  void write(T t) {
    makeSpaceFor(sizeof(T));
    memcpy(&data[c], &t, sizeof(T));
    c += sizeof(T);
  }

  template <typename T>
  T read(
#ifndef NDEBUG
      const std::source_location location = std::source_location::current()
#endif
  ) {
    if (!isSpaceFor(sizeof(T))) {
      rdm::Log::printf(LOG_ERROR, "No space for type %s", typeid(T).name());
#ifndef NDEBUG
      rdm::Log::printf(LOG_ERROR, "Location: %s:%i", location.file_name(),
                       location.line());
#endif
      throw BitStreamException("Out of space on bitstream");
    }
    T t;
    memcpy(&t, &data[c], sizeof(T));
    c += sizeof(T);
    return t;
  }

  void writeStream(const BitStream& stream);

  void writeString(std::string s);
  std::string readString();

  void writeSignedMessage(SignedMessage msg);
  SignedMessage readSignedMessage();

  ENetPacket* createPacket(enet_uint32 flags);
  std::vector<unsigned char> getDataVec();

  Context getContext() { return ctxt; }
  void setContext(Context ctxt) { this->ctxt = ctxt; }

 private:
  Context ctxt;
};  // namespace rdm::network
}  // namespace rdm::network
