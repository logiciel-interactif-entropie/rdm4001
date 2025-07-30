#pragma once
#include <enet/enet.h>

#include <string>

#include "entity.hpp"
#include "network_defs.hpp"

namespace rdm::network {
class Player : public Entity {
 public:
  ReplicateProperty<int> remotePeerId;  // -1 if not controlled, -2 if a bot
  ReplicateProperty<std::string> displayName;

  Player(NetworkManager* manager, EntityId id);

  virtual const char* getTypeName() { return "Player"; };

  bool isLocalPlayer();

  virtual std::string getEntityInfo();

  virtual bool isDirty() { return remotePeerId.isDirty(); }
  virtual bool isBot() { return remotePeerId.get() == -2; }
  virtual void serialize(BitStream& stream);
  virtual void deserialize(BitStream& stream);
};

struct Peer {
  enum Type {
    ConnectedPlayer,
    UnauthenticatedPlayer,
    Machine,
    Unconnected,
    Undifferentiated,
  };

  ENetPeer* peer;
  ENetAddress address;
  int peerId;
  Player* playerEntity;
  Type type;
  bool noob;

  int roundTripTime;
  int packetLoss;

  CustomEventList queuedEvents;
  std::vector<EntityId> pendingNewIds;
  std::vector<EntityId> pendingDelIds;
  std::map<std::string, std::string> localCvarValues;

  Peer();
};
}  // namespace rdm::network
