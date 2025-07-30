#pragma once
#include <future>

#include "player.hpp"

namespace rdm::network {
class NetworkManager;
class IAuthenticationProvider {
  NetworkManager* manager;

 public:
  virtual ~IAuthenticationProvider();

  virtual void sendPeerInfo(Peer* peer,
                            BitStream stream) = 0;  // false if not ready yet
  virtual std::future<bool> verifyPeerInfo(
      Peer* peer,
      BitStream stream) = 0;  // false if denied

  NetworkManager* getManager() { return manager; }
  void setManager(NetworkManager* manager) { this->manager = manager; }

  virtual void serverSetup() = 0;
  virtual void serverDestroy() = 0;
};
};  // namespace rdm::network
