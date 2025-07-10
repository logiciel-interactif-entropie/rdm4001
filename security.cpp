#include "security.hpp"

#include <tomcrypt.h>

#include "console.hpp"
#include "game.hpp"
#include "logging.hpp"
#include "settings.hpp"

#define MAX_RSA_SIZE 4096

extern const ltc_math_descriptor ltm_desc;
const ltc_hash_descriptor& hash_desc = sha512_desc;

namespace rdm {
static CVar security_public_key("security_public_key", "",
                                CVARF_GLOBAL | CVARF_SAVE | CVARF_HIDDEN);
static CVar security_private_key("security_private_key", "",
                                 CVARF_GLOBAL | CVARF_SAVE | CVARF_HIDDEN);

void SecurityManager::generateKeyPair() {
  rsa_key key;
  const int bitsize = 4096;
  int err = rsa_make_key(NULL, prng_idx, bitsize / 8, 65537, &key);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  unsigned char out[bitsize * 5 / 8];
  unsigned long outlen = sizeof(out);
  err = rsa_export(out, &outlen, PK_PRIVATE, &key);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  unsigned char b64_key[65535];
  unsigned long ln = sizeof(b64_key);
  err = base64_encode(out, outlen, b64_key, &ln);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }
  security_private_key.setValue((char*)b64_key);

  outlen = sizeof(out);
  err = rsa_export(out, &outlen, PK_PUBLIC, &key);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  ln = sizeof(b64_key);
  err = base64_encode(out, outlen, b64_key, &ln);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }
  security_public_key.setValue((char*)b64_key);
  rsa_free(&key);
}

bool SecurityManager::verifyKeyPair() {
  std::string test("hahaha");
  SignedMessage msg = sign(std::vector<char>(test.begin(), test.end()));
  return verify(msg);
}

SecurityManager::SecurityManager() {
  ltc_mp = ltm_desc;

  prng_idx = register_prng(&sprng_desc);
  hash_idx = register_hash(&hash_desc);

  if (prng_idx < 0) {
    throw std::runtime_error("SecurityManager CRYPT_INVALID_PRNG");
  }

  if (hash_idx < 0) {
    throw std::runtime_error("SecurityManager CRYPT_INVALID_HASH");
  }

  if (security_public_key.getValue().empty()) {
    Log::printf(LOG_DEBUG, "Generating key pairs");
    generateKeyPair();
  }

  if (!verifyKeyPair()) {
    Log::printf(LOG_ERROR, "Key pair cannot verify itself, regenerating");
    generateKeyPair();
  }
}

bool SecurityManager::verify(SignedMessage msg, std::string _key) {
  unsigned char key_out[MAX_RSA_SIZE * 5 / 8];
  unsigned long key_out_ln = sizeof(key_out);
  int err = base64_decode((unsigned char*)_key.c_str(), _key.size(), key_out,
                          &key_out_ln);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  rsa_key key;
  rsa_import(key_out, key_out_ln, &key);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  unsigned char hash[64];
  hash_state md;
  hash_desc.init(&md);
  hash_desc.process(&md, (unsigned char*)msg.data.data(), msg.data.size());
  hash_desc.done(&md, hash);

  const int padding = LTC_PKCS_1_V1_5;
  const unsigned long saltlen = 0;

  key_out_ln = sizeof(key_out);
  err = base64_decode((unsigned char*)msg.sig.c_str(), msg.sig.size(), key_out,
                      &key_out_ln);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  int stat = 0;
  err = rsa_verify_hash_ex(key_out, key_out_ln, hash, hash_desc.hashsize,
                           padding, hash_idx, saltlen, &stat, &key);
  rsa_free(&key);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  return stat;
}

bool SecurityManager::verify(SignedMessage msg) {
  unsigned char key_out[MAX_RSA_SIZE * 5 / 8];
  unsigned long key_out_ln = sizeof(key_out);
  int err = base64_decode((unsigned char*)msg.key.c_str(), msg.key.size(),
                          key_out, &key_out_ln);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  rsa_key key;
  rsa_import(key_out, key_out_ln, &key);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  unsigned char hash[64];
  hash_state md;
  hash_desc.init(&md);
  hash_desc.process(&md, (unsigned char*)msg.data.data(), msg.data.size());
  hash_desc.done(&md, hash);

  const int padding = LTC_PKCS_1_V1_5;
  const unsigned long saltlen = 0;

  key_out_ln = sizeof(key_out);
  err = base64_decode((unsigned char*)msg.sig.c_str(), msg.sig.size(), key_out,
                      &key_out_ln);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  int stat = 0;
  err = rsa_verify_hash_ex(key_out, key_out_ln, hash, hash_desc.hashsize,
                           padding, hash_idx, saltlen, &stat, &key);
  rsa_free(&key);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  return stat;
}

SignedMessage SecurityManager::sign(char* in, size_t size) {
  SignedMessage msg;
  msg.key = security_public_key.getValue();
  msg.data = std::vector<char>(in, in + size);

  unsigned char hash[64];
  hash_state md;
  hash_desc.init(&md);
  hash_desc.process(&md, (const unsigned char*)in, (unsigned long)size);
  hash_desc.done(&md, hash);

  unsigned char key_out[MAX_RSA_SIZE * 5 / 8];
  unsigned long key_out_ln = sizeof(key_out);
  int err = base64_decode(
      (unsigned char*)security_private_key.getValue().c_str(),
      security_private_key.getValue().size(), key_out, &key_out_ln);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  rsa_key key;
  rsa_import(key_out, key_out_ln, &key);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }

  const int padding = LTC_PKCS_1_V1_5;
  const unsigned long saltlen = 0;

  unsigned char sig[MAX_RSA_SIZE / 8];
  unsigned long siglen = sizeof(sig);
  err = rsa_sign_hash_ex(hash, hash_desc.hashsize, sig, &siglen, padding, NULL,
                         prng_idx, hash_idx, saltlen, &key);
  if (err != CRYPT_OK) {
    Log::printf(LOG_ERROR, "SecurityManager error: %s", error_to_string(err));
    throw std::runtime_error("SecurityManager error");
  }
  rsa_free(&key);

  unsigned char b64_out[65535];
  unsigned long b64_len = sizeof(b64_out);
  base64_encode(sig, siglen, b64_out, &b64_len);
  msg.sig = std::string((char*)b64_out);
  return msg;
}

SignedMessage SecurityManager::sign(std::vector<char> in) {
  return sign(in.data(), in.size());
}

#ifndef NDEBUG
void SecurityManager::dumpSecurityManagerKeys() {
  Log::printf(LOG_INFO, "Private key: %s",
              security_private_key.getValue().c_str());
  Log::printf(LOG_INFO, "Public key: %s",
              security_public_key.getValue().c_str());
}

static ConsoleCommand dump_security_manager_keys(
    "dump_security_manager_keys", "dump_security_manager_keys", "",
    [](Game* game, ConsoleArgReader r) {
      game->getSecurityManager()->dumpSecurityManagerKeys();
    });

static ConsoleCommand regen_security_manager_keys(
    "regen_security_manager_keys", "regen_security_manager_keys", "",
    [](Game* game, ConsoleArgReader r) {
      game->getSecurityManager()->generateKeyPair();
      game->getSecurityManager()->dumpSecurityManagerKeys();
    });
#endif
};  // namespace rdm
