#pragma once

#include <string>
#include <vector>

namespace rdm {
struct SignedMessage {
  std::vector<char> data;

  // all of these are base64 encoded
  std::string sig;
  std::string key;
};

class SecurityManager {
  int prng_idx;
  int hash_idx;

  bool verifyKeyPair();

 public:
  SecurityManager();

  SignedMessage sign(std::vector<char> in);
  SignedMessage sign(char* in, size_t size);
  bool verify(SignedMessage msg);
  bool verify(SignedMessage msg, std::string key);
  void generateKeyPair();

  std::string getPublicKey();

#ifndef NDEBUG
  void dumpSecurityManagerKeys();
#endif
};
};  // namespace rdm
