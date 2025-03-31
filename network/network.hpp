#pragma once
#include <enet/enet.h>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "crc_hash.hpp"
#include "defs.hpp"
#include "entity.hpp"
#include "player.hpp"
#include "signal.hpp"

#define NETWORK_STREAM_META 0
#define NETWORK_STREAM_ENTITY 1
#define NETWORK_STREAM_EVENT 1
#define NETWORK_STREAM_MAX 4

#define NETWORK_DISCONNECT_FORCED 0
#define NETWORK_DISCONNECT_USER 1
#define NETWORK_DISCONNECT_TIMEOUT 2

namespace rdm {
class World;
class Game;
}  // namespace rdm

namespace rdm::network {
typedef uint16_t CustomEventID;
typedef std::vector<std::pair<CustomEventID, BitStream*>> CustomEventList;

struct Peer {
  enum Type {
    ConnectedPlayer,
    Machine,
    Unconnected,
    Undifferentiated,
  };

  ENetPeer* peer;
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

typedef std::function<Entity*(NetworkManager*, EntityId)>
    EntityConstructorFunction;

class NetworkManager {
  friend class NetworkJob;

  ENetHost* host;
  Peer localPeer;
  std::map<int, Peer> peers;
  World* world;
  Game* game;
  gfx::Engine* gfxEngine;
  bool backend;

  EntityId lastId;
  int lastPeerId;
  size_t ticks;

  float distributedTime;
  float nextDtPacket;
  float latency;

  std::string playerType;
  std::string password;
  std::string userPassword;
  std::string username;

  std::map<std::string, EntityConstructorFunction> constructors;
  std::unordered_map<EntityId, std::unique_ptr<Entity>> entities;

  CustomEventList queuedEvents;
  std::vector<std::string> pendingCvars;
  std::vector<EntityId> pendingUpdates;
  std::vector<EntityId> pendingUpdatesUnreliable;
  std::vector<std::pair<std::string, std::string>> pendingRconCommands;

  std::chrono::time_point<std::chrono::steady_clock> lastTick;
  ClosureId cvarChangingUpdate;

 public:
  typedef Signal<BitStream&> CustomEventSignal;

  NetworkManager(World* world);
  ~NetworkManager();

  void setGame(Game* game) { this->game = game; };
  Game* getGame() { return game; }

  float getDistributedTime() { return distributedTime; };
  float getLatency() { return latency; }

  void handleDisconnect();
  void setPassword(std::string password) { this->password = password; };
  void setUserPassword(std::string userPassword) {
    this->userPassword = userPassword;
  }

  World* getWorld() { return world; }

  Signal<std::string> remoteDisconnect;

  enum PacketId {
    DisconnectPacket,       // S -> C
    NewIdPacket,            // S -> C
    DelIdPacket,            // S -> C
    NewPeerPacket,          // S -> C
    DelPeerPacket,          // S -> C
    PeerInfoPacket,         // S -> C
    DeltaIdPacket,          // S -> C, C -> S
    SignalPacket,           // S -> C, C -> S
    DistributedTimePacket,  // S -> C
    RconPacket,             // C -> S
    CvarPacket,             // S -> C, C -> S
    EventPacket,            // S -> C, C -> S

    WelcomePacket = PROTOCOL_VERSION,  // S -> C, beginning of handshake
    AuthenticatePacket,                // C -> S
  };

  void service();

  void start(int port = 7938);

  void connect(std::string address, int port = 7938);
  void requestDisconnect();

  void deleteEntity(EntityId id);
  Entity* instantiate(std::string typeName, int id = -1);
  void registerConstructor(EntityConstructorFunction func,
                           std::string typeName);
  Entity* findEntityByType(std::string typeName);
  std::vector<Entity*> findEntitiesByType(std::string typeName);

  std::map<int, Peer> getPeers() { return peers; }
  Peer* getPeerById(int id);

  Entity* getEntityById(EntityId id);

  void addPendingUpdate(EntityId id) { pendingUpdates.push_back(id); };
  void addPendingUpdateUnreliable(EntityId id) {
    pendingUpdatesUnreliable.push_back(id);
  };
  void setGfxEngine(gfx::Engine* engine) { gfxEngine = engine; }
  gfx::Engine* getGfxEngine() { return gfxEngine; }

  void setPlayerType(std::string type) { playerType = type; };
  std::string getPlayerType() { return playerType; }
  Peer& getLocalPeer() { return localPeer; }
  bool isBackend() { return backend; }

  void setUsername(std::string username) { this->username = username; };
  void listEntities();

  void sendRconCommand(std::string password, std::string command) {
    pendingRconCommands.push_back({password, command});
  }

  void sendCustomEvent(CustomEventID id, BitStream& stream);
  // server only, send to player
  void sendCustomEvent(int peerId, CustomEventID id, BitStream& stream);

  CustomEventSignal& getSignal(CustomEventID id) { return customSignals[id]; }

  ClosureId addCustomEventListener(CustomEventID id,
                                   CustomEventSignal::Function listener);
  void removeCustomEvent(CustomEventID id, ClosureId cId);

  static void initialize();
  static void deinitialize();

 private:
  std::unordered_map<CustomEventID, CustomEventSignal> customSignals;
};
}  // namespace rdm::network
