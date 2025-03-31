#include "network.hpp"

#include <enet/enet.h>

#include <chrono>
#include <stdexcept>

#include "console.hpp"
#include "fun.hpp"
#include "game.hpp"
#include "logging.hpp"
#include "network/bitstream.hpp"
#include "network/entity.hpp"
#include "scheduler.hpp"
#include "settings.hpp"
#include "world.hpp"

#ifndef DISABLE_OBZ
#define RDM_COMPILE
#include "obz.hpp"
#endif

#ifndef DISABLE_EASY_PROFILER
#include <easy/profiler.h>
#endif

static const char* disconnectReasons[] = {"Disconnect by engine",
                                          "Disconnect by user", "Timeout"};

namespace rdm::network {
Peer::Peer() {
  playerEntity = NULL;
  peer = NULL;
}

static CVar net_rate("net_rate", "60.0",
                     CVARF_SAVE | CVARF_NOTIFY | CVARF_REPLICATE |
                         CVARF_GLOBAL);
static CVar net_service("net_service", "1", CVARF_SAVE | CVARF_GLOBAL);

#ifdef NDEBUG
static CVar rcon_password("rcon_password", "", CVARF_SAVE | CVARF_GLOBAL);
#else
static CVar rcon_password("rcon_password", "RCON_DEBUG",
                          CVARF_SAVE | CVARF_GLOBAL);
#endif

static ConsoleCommand rcon(
    "rcon", "rcon [password] [command]",
    "send remote console command to server, requires that server has "
    "rcon_password set and you have the same value",
    [](Game* game, ConsoleArgReader reader) {
      if (!game->getWorldConstructorSettings().network)
        throw std::runtime_error("network disabled");
      if (!game->getWorld()) throw std::runtime_error("Must be client");
      game->getWorld()->getNetworkManager()->sendRconCommand(reader.next(),
                                                             reader.rest());
    });

static ConsoleCommand spawn(
    "spawn", "spawn [entity]", "spawns entity of class",
    [](Game* game, ConsoleArgReader reader) {
      if (!game->getWorldConstructorSettings().network)
        throw std::runtime_error("network disabled");
      if (!game->getServerWorld())
        throw std::runtime_error("Must be hosting server");
      Entity* entity = game->getServerWorld()->getNetworkManager()->instantiate(
          reader.next());
      if (!entity)
        throw std::runtime_error("entity == NULL, type may not exist");
      Log::printf(LOG_INFO, "%i", entity->getEntityId());
    });

static ConsoleCommand bot(
    "bot", "bot", "creates bot", [](Game* game, ConsoleArgReader reader) {
      if (!game->getWorldConstructorSettings().network)
        throw std::runtime_error("network disabled");
      if (!game->getServerWorld())
        throw std::runtime_error("Must be hosting server");
      Player* player = dynamic_cast<Player*>(
          game->getServerWorld()->getNetworkManager()->instantiate(
              game->getServerWorld()->getNetworkManager()->getPlayerType()));
      if (!player) throw std::runtime_error("player == NULL");
      player->remotePeerId.set(-1);
      player->displayName.set(std::format("Bot{}", rand() % 100));
      Log::printf(LOG_INFO, "%i", player->getEntityId());
    });

static ConsoleCommand entity(
    "entity", "entity [id]", "gets info of entity id",
    [](Game* game, ConsoleArgReader reader) {
      if (!game->getWorldConstructorSettings().network)
        throw std::runtime_error("network disabled");
      if (!game->getServerWorld())
        throw std::runtime_error("Must be hosting server");
      EntityId entityid = std::atoi(reader.next().c_str());
      Entity* entity =
          game->getServerWorld()->getNetworkManager()->getEntityById(entityid);
      if (!entity) throw std::runtime_error("entity == NULL, id may not exist");
      Log::printf(LOG_INFO, "info of entity %i", entityid);
      Log::printf(LOG_INFO, "%s", entity->getEntityInfo().c_str());
      if (game->getWorld()) {
        entity = game->getWorld()->getNetworkManager()->getEntityById(entityid);
        if (entity) {
          Log::printf(LOG_INFO, "client info of entity %i", entityid);
          Log::printf(LOG_INFO, "%s", entity->getEntityInfo().c_str());
        }
      }
    });

static ConsoleCommand entities(
    "entities", "entities", "lists all entities",
    [](Game* game, ConsoleArgReader reader) {
      if (!game->getWorldConstructorSettings().network)
        throw std::runtime_error("network disabled");
      if (!game->getServerWorld())
        throw std::runtime_error("Must be hosting server");

      game->getServerWorld()->getNetworkManager()->listEntities();
    });

class NetworkJob : public SchedulerJob {
  NetworkManager* netmanager;

 public:
  NetworkJob(NetworkManager* netmanager)
      : SchedulerJob("Network"), netmanager(netmanager) {}

  virtual double getFrameRate() { return 1.0 / net_rate.getFloat(); }

  virtual Result step() {
    if (netmanager->backend) {
      netmanager->distributedTime = netmanager->getWorld()->getTime();
    } else {
      netmanager->distributedTime += getStats().totalDeltaTime;
    }
    netmanager->service();
    return Stepped;
  }
};

NetworkManager::NetworkManager(World* world) {
  host = NULL;
  this->world = world;
  world->getScheduler()->addJob(new NetworkJob(this));

  localPeer.type = Peer::Unconnected;
  localPeer.peerId = -2;
  lastId = 0;
  lastPeerId = 0;
  ticks = 0;
  latency = 0.f;

  playerType = "";

  username = Fun::getSystemUsername();
  nextDtPacket = 0.0;

  cvarChangingUpdate =
      Settings::singleton()->cvarChanging.listen([this](std::string name) {
        CVar* cvar = Settings::singleton()->getCvar(name.c_str());
        if (isBackend()) {
          if (cvar->getFlags() & CVARF_REPLICATE) {
            pendingCvars.push_back(name);
          }
        } else {
        }
      });

  password = "RDMRDMRDM";
  userPassword = "";
}

void NetworkManager::listEntities() {
  for (auto& entity : entities) {
    Log::printf(LOG_INFO, "%i - %s", entity.first,
                entity.second->getTypeName());
  }
}

void NetworkManager::requestDisconnect() {
  BitStream disconnectMessage;
  disconnectMessage.write<PacketId>(DisconnectPacket);
  disconnectMessage.write<int>(0);
  enet_peer_send(localPeer.peer, 0,
                 disconnectMessage.createPacket(ENET_PACKET_FLAG_RELIABLE));
}

NetworkManager::~NetworkManager() {
  Settings::singleton()->cvarChanging.removeListener(cvarChangingUpdate);

  if (host) {
    ENetEvent event;
    if (backend) {
      BitStream shutdownMessage;
      shutdownMessage.write<PacketId>(DisconnectPacket);
      shutdownMessage.writeString("Server is shutting down");
      enet_host_broadcast(
          host, 0, shutdownMessage.createPacket(ENET_PACKET_FLAG_RELIABLE));
      enet_host_service(host, &event,
                        1);  // service to flush disconnect packet
    } else {
      if (localPeer.type != Peer::Unconnected) {
        enet_peer_disconnect(localPeer.peer, NETWORK_DISCONNECT_FORCED);
        enet_host_service(host, &event,
                          1);  // service to flush disconnect packet
        handleDisconnect();
      }
    }
    enet_host_destroy(host);
  }
  host = NULL;
}

static CVar sv_dtrate("sv_dtrate", "0.5", CVARF_SAVE | CVARF_GLOBAL);

void NetworkManager::service() {
  if (!host) return;

#ifndef DISABLE_EASY_PROFILER
  EASY_FUNCTION();
#endif

  ENetEvent event;

#ifndef DISABLE_EASY_PROFILER
  EASY_BLOCK("Network Service");
#endif
  while (enet_host_service(host, &event, net_service.getInt()) > 0) {
#ifndef DISABLE_EASY_PROFILER
    EASY_BLOCK("Handle Service");
#endif
    switch (event.type) {
      case ENET_EVENT_TYPE_RECEIVE: {
        try {
          Peer* remotePeer = (Peer*)event.peer->data;
          BitStream stream(event.packet->data, event.packet->dataLength);
          PacketId packetId = stream.read<PacketId>();
          try {
            switch (packetId) {
              case WelcomePacket:
                if (backend) {
                  throw std::runtime_error("Received WelcomePacket on backend");
                } else {
                  Log::printf(LOG_DEBUG, "Received WelcomePacket");
                  localPeer.peerId = stream.read<int>();
                  size_t _ticks = stream.read<size_t>();
                  Log::printf(LOG_DEBUG, "Tick diff %i vs %i (%i)", ticks,
                              _ticks, _ticks - ticks);
                  distributedTime = stream.read<float>();

#ifndef DISABLE_OBZ
                  // doesn't do anything yet but will verify official servers
                  obz::ObzCrypt::singleton()->readCryptPacket(stream, false);
#endif

                  ticks = _ticks;

                  BitStream authenticateStream;
                  authenticateStream.write<PacketId>(AuthenticatePacket);
                  authenticateStream.writeString(this->username);
                  authenticateStream.writeString(password);
                  authenticateStream.writeString(userPassword);

#ifndef DISABLE_OBZ
                  obz::ObzCrypt::singleton()->writeCryptPacket(
                      authenticateStream, false);
#endif

                  enet_peer_send(localPeer.peer, NETWORK_STREAM_META,
                                 authenticateStream.createPacket(
                                     ENET_PACKET_FLAG_RELIABLE));
                }
                break;
              case AuthenticatePacket:
                if (backend) {
                  std::string username = stream.readString();
                  std::string password = stream.readString();
                  std::string userPassword = stream.readString();

#ifndef DISABLE_OBZ
                  obz::ObzCrypt::singleton()->readCryptPacket(stream, true);
#endif

                  if (!userPassword.empty())
                    if (userPassword != this->userPassword) {
                      Log::printf(LOG_INFO, "%s failed userPassword",
                                  username.c_str());
                      break;  // deny user
                    }

                  if (password != this->password) {
                    Log::printf(LOG_INFO, "%s failed password",
                                username.c_str());
                    break;  // deny user
                  } else {
                    Log::printf(LOG_DEBUG, "%s passed password",
                                username.c_str());
                  }

                  Log::printf(LOG_INFO, "%s authenticating", username.c_str());
                  remotePeer->type = Peer::ConnectedPlayer;
                  if (playerType.empty()) {
                    throw std::runtime_error(
                        "Please use NetworkManager::setPlayerType to set the "
                        "player type to class which derives "
                        "rdm::network::Player");
                  }
                  remotePeer->playerEntity = (Player*)instantiate(playerType);
                  remotePeer->playerEntity->remotePeerId.set(
                      remotePeer->peerId);
                  remotePeer->playerEntity->displayName.set(username);

                  BitStream newPeerPacket;
                  newPeerPacket.write<PacketId>(NewPeerPacket);
                  newPeerPacket.write<int>(remotePeer->peerId);
                  newPeerPacket.write<EntityId>(
                      remotePeer->playerEntity->getEntityId());
                  enet_host_broadcast(
                      host, NETWORK_STREAM_META,
                      newPeerPacket.createPacket(ENET_PACKET_FLAG_RELIABLE));

                  for (auto& e : entities)
                    remotePeer->pendingNewIds.push_back(e.first);
                } else {
                  throw std::runtime_error(
                      "Received AuthenticatePacket on frontend");
                }
                break;
              case NewIdPacket:
                if (backend) {
                  throw std::runtime_error("Received NewIdPacket on backend");
                } else {
                  int numEntities = stream.read<int>();
                  for (int i = 0; i < numEntities; i++) {
                    EntityId id = stream.read<EntityId>();
                    std::string typeName = stream.readString();
                    Entity* ent = instantiate(typeName, id);
                    // ent->deserialize(stream);

                    Log::printf(LOG_DEBUG, "created %s", typeName.c_str());
                  }
                }
                break;
              case DelIdPacket:
                if (backend) {
                  throw std::runtime_error("Received DelIdPacket on backend");
                } else {
                  int numEntities = stream.read<int>();
                  for (int i = 0; i < numEntities; i++) {
                    EntityId id = stream.read<EntityId>();
                    deleteEntity(id);
                  }
                }
                break;
              case DeltaIdPacket: {
                int numEntities = stream.read<int>();
                int i;
                Entity* ent;
                try {
                  for (i = 0; i < numEntities; i++) {
                    EntityId id = stream.read<EntityId>();
                    if (entities.find(id) == entities.end()) {
                      Log::printf(LOG_DEBUG, "invalid entity %i", id);
                      throw std::runtime_error("invalid entity id");
                    }
                    ent = entities[id].get();
                    if (!ent) {
                      Log::printf(LOG_DEBUG, "%i == NULL", id);
                      throw std::runtime_error("ent == NULL");
                    }

                    BitStream::Context context = BitStream::Generic;
                    if (backend) {
                      bool allowed = false;
                      if (Player* player = dynamic_cast<Player*>(ent)) {
                        if (player->remotePeerId.get() == remotePeer->peerId) {
                          allowed = true;
                          context = BitStream::FromClientLocal;
                        }
                      } else if (ent->getOwnership(remotePeer)) {
                        allowed = true;
                      }

                      if (!allowed) {
                        Log::printf(
                            LOG_ERROR,
                            "Peer %i attempted to modify %s (%i, reliable: %s)",
                            remotePeer->peerId, ent->getTypeName(),
                            ent->getEntityId(),
                            event.packet->flags & ENET_PACKET_FLAG_RELIABLE
                                ? "yes"
                                : "no");
                        throw std::runtime_error(
                            "Peer not allowed to modify entity");
                      } else {
                        context = BitStream::FromClientLocal;
                      }
                    } else {
                      if (ent->getOwnership(&localPeer)) {
                        context = BitStream::FromServerLocal;
                      } else {
                        context = BitStream::FromServer;
                      }
                    }
                    stream.setContext(context);

                    if (event.packet->flags & ENET_PACKET_FLAG_RELIABLE) {
                      ent->deserialize(stream);
                      if (backend) addPendingUpdate(id);
                    } else {
                      ent->deserializeUnreliable(stream);
                      if (backend) addPendingUpdateUnreliable(id);
                    }
                  }
                } catch (std::exception& e) {
                  if (!ent) {
                    Log::printf(LOG_ERROR, "%s Error decoding entity %i: (%s)",
                                isBackend() ? "Backend" : "Frontend", i,
                                e.what());
                  } else {
                    Log::printf(
                        LOG_ERROR,
                        "%s Error decoding entity %s:%i (num: %i, what: %s)",
                        isBackend() ? "Backend" : "Frontend",
                        ent->getTypeName(), ent->getEntityId(), i, e.what());
                  }
                }
              } break;
              case NewPeerPacket:
                if (backend) {
                  throw std::runtime_error("Received NewPeerPacket on backend");
                } else {
                  int peerId = stream.read<int>();
                  Peer np;
                  np.peer = NULL;
                  np.playerEntity = dynamic_cast<Player*>(
                      getEntityById(stream.read<EntityId>()));
                  np.peerId = peerId;
                  Log::printf(LOG_DEBUG, "NewPeerPacket peerId %i", np.peerId);
                  peers[np.peerId] = np;
                }
                break;
              case DelPeerPacket:
                if (backend) {
                  throw std::runtime_error("Received DelPeerPacket on backend");
                } else {
                  int peerId = stream.read<int>();
                  auto it = peers.find(peerId);
                  if (it != peers.end()) {
                    peers.erase(it);
                  } else {
                    throw std::runtime_error(
                        "Attempted to delete non-existent peer");
                  }
                }
                break;
              case DisconnectPacket:
                if (backend) {
                  int reason = stream.read<int>();
                  Log::printf(LOG_INFO,
                              "Remote requested disconnect for %i (%s)", reason,
                              disconnectReasons[reason]);
                  enet_peer_disconnect(remotePeer->peer, reason);
                } else {
                  std::string message = stream.readString();
                  Log::printf(LOG_INFO, "Disconnected from server (%s)",
                              message.c_str());
                  enet_peer_disconnect_now(localPeer.peer, 0);
                  localPeer.type = Peer::Unconnected;
                  handleDisconnect();
                  enet_host_destroy(host);
                  host = NULL;

                  remoteDisconnect.fire(message);
                  return;
                }
                break;
              case SignalPacket:

                break;
              case DistributedTimePacket:
                if (backend)
                  throw std::runtime_error("DistributedTimePacket on backend");
                else {
                  float newTime = stream.read<float>();
                  float diff = newTime - distributedTime;
                  lastTick = std::chrono::steady_clock::now();
                  if (fabsf(diff) > 0.05) {
                    Log::printf(LOG_WARN,
                                "Updating distributedTime (diff: %f, new: %f)",
                                diff, newTime);
                    distributedTime = newTime;
                    latency = diff;
                  }
                  int numPeers = stream.read<int>();
                  for (int i = 0; i < numPeers; i++) {
                    int peer = stream.read<int>();
                    int rtt = stream.read<int>();
                    int ploss = stream.read<int>();

                    auto it = peers.find(peer);
                    if (it != peers.end()) {
                      it->second.roundTripTime = rtt;
                      it->second.packetLoss = ploss;
                    }
                  }
                }
                break;
              case RconPacket:
                if (!backend) {
                  throw std::runtime_error("RconPacket on frontend");
                } else {
                  if (game->getWorld())
                    throw std::runtime_error(
                        "RconPacket on self hosted server");
                  if (rcon_password.getValue().empty()) break;

                  std::string password = stream.readString();
                  std::string command = stream.readString();

                  if (password != rcon_password.getValue()) {
                    Log::printf(LOG_ERROR, "Player attempted password %s",
                                password.c_str());
                    throw std::runtime_error("Bad password");
                  }

                  Log::printf(LOG_INFO, "peer %i] %s", remotePeer->peerId,
                              command.c_str());
                  game->getConsole()->command(command);
                }
                break;
              case EventPacket: {
                CustomEventID id = stream.read<CustomEventID>();
                auto it = customSignals.find(id);
                if (it != customSignals.end()) {
                  if (customSignals[id].size() == 0) {
                    throw std::runtime_error("customSignals[id].size() == 0");
                  }

                  customSignals[id].fire(stream);
                } else {
                  Log::printf(LOG_ERROR, "invalid customSignal id %04x", id);
                  throw std::runtime_error("received invalid customSignal id");
                }
              } break;
              case CvarPacket:
                if (backend) {
                  throw std::runtime_error("CvarPacket on backend");
                } else {
                  int num = stream.read<int>();
                  for (int i = 0; i < num; i++) {
                    std::string name = stream.readString();
                    std::string value = stream.readString();

                    CVar* var = Settings::singleton()->getCvar(name.c_str());
                    if (var && var->getFlags() & CVARF_REPLICATE) {
                      Log::printf(LOG_INFO, "%s -> %s", name.c_str(),
                                  value.c_str());
                      var->setValue(value);
                    }
                  }
                }
                break;
              default:
                Log::printf(LOG_WARN, "%s: Unknown packet %i",
                            backend ? "Backend" : "Frontend", packetId);
                break;
            }
          } catch (std::exception& e) {
            Log::printf(
                LOG_ERROR,
                "%s: Error in ENET_EVENT_TYPE_RECEIVE: %s (packet id: %i)",
                backend ? "Backend" : "Frontend", e.what(), packetId);
          }
        } catch (std::exception& e) {
          Log::printf(LOG_ERROR, "%s: Error in ENET_EVENT_TYPE_RECEIVE: %s",
                      backend ? "Backend" : "Frontend", e.what());
        }
        enet_packet_destroy(event.packet);
      } break;
      case ENET_EVENT_TYPE_DISCONNECT: {
        if (backend) {
          Peer* peer = (Peer*)event.peer->data;
          if (peer) {
            Log::printf(LOG_INFO, "Peer disconnecting (reason: %s)",
                        disconnectReasons[event.data]);

            if (peer->playerEntity) {
              BitStream peerRemoving;
              peerRemoving.write<PacketId>(DelPeerPacket);
              peerRemoving.write<int>(peer->peerId);
              deleteEntity(peer->playerEntity->getEntityId());
              for (auto _peer : peers) {
                if (!_peer.second.playerEntity ||
                    peer->peerId == _peer.second.peerId)
                  continue;
                enet_peer_send(
                    _peer.second.peer, NETWORK_STREAM_META,
                    peerRemoving.createPacket(ENET_PACKET_FLAG_RELIABLE));
              }
            }

            peers.erase(peer->peerId);
          } else {
          }
        } else {
          if (localPeer.type != Peer::Unconnected) {
            remoteDisconnect.fire("ENET_EVENT_TYPE_DISCONNECT");
          }
          handleDisconnect();
          localPeer.type = Peer::Unconnected;
        }
      } break;
      case ENET_EVENT_TYPE_CONNECT: {
        char hostname[64];
        enet_address_get_host(&event.peer->address, hostname, 64);
        Log::printf(LOG_INFO, "%s: Connection from %s:%i",
                    backend ? "Backend" : "Frontend", hostname,
                    event.peer->address.port);

        if (backend) {
          Peer np;
          np.type = Peer::Undifferentiated;
          np.peer = event.peer;
          np.peerId = lastPeerId++;
          np.playerEntity = NULL;
          np.noob = true;

          peers[np.peerId] = np;
          event.peer->data = &peers[np.peerId];

          BitStream welcomePacketStream;
          welcomePacketStream.write<PacketId>(WelcomePacket);
          welcomePacketStream.write<int>(np.peerId);
          welcomePacketStream.write<size_t>(ticks);
          welcomePacketStream.write<float>(distributedTime);

#ifndef DISABLE_OBZ
          obz::ObzCrypt::singleton()->writeCryptPacket(welcomePacketStream,
                                                       true);
#endif

          enet_peer_send(
              event.peer, NETWORK_STREAM_META,
              welcomePacketStream.createPacket(ENET_PACKET_FLAG_RELIABLE));
        } else {
          localPeer.peer = event.peer;
          localPeer.type = Peer::ConnectedPlayer;
        }
      } break;
      default:
        break;
    }
  }
#ifndef DISABLE_EASY_PROFILER
  EASY_END_BLOCK;
#endif

#ifndef DISABLE_EASY_PROFILER
  EASY_BLOCK("Network Tick");
#endif
  for (auto& entity : entities) {
    try {
      entity.second->tick();
    } catch (std::exception& e) {
      Log::printf(LOG_ERROR, "Error ticking entity %s:%i: %s",
                  entity.second->getTypeName(), entity.first, e.what());
    }
  }
#ifndef DISABLE_EASY_PROFILER
  EASY_END_BLOCK;
#endif

  if (backend) {
#ifndef DISABLE_EASY_PROFILER
    EASY_BLOCK("Backend Peer Management");
#endif
    for (auto& peer : peers) {
      std::vector<int> pendingUpdates;

      if (int pendingNewIds = peer.second.pendingNewIds.size()) {
        BitStream newIdStream;
        newIdStream.write<PacketId>(NewIdPacket);
        newIdStream.write<int>(pendingNewIds);
        for (auto id : peer.second.pendingNewIds) {
          newIdStream.write<EntityId>(id);
          Entity* ent = entities[id].get();
          newIdStream.writeString(ent->getTypeName());
          pendingUpdates.push_back(id);
          // ent->serialize(newIdStream);
        }
        enet_peer_send(peer.second.peer, NETWORK_STREAM_ENTITY,
                       newIdStream.createPacket(ENET_PACKET_FLAG_RELIABLE));
        peer.second.pendingNewIds.clear();
      }

      if (int pendingDelIds = peer.second.pendingDelIds.size()) {
        BitStream delIdStream;
        delIdStream.write<PacketId>(DelIdPacket);
        delIdStream.write<int>(pendingDelIds);
        for (auto id : peer.second.pendingDelIds) {
          delIdStream.write<EntityId>(id);
        }
        enet_peer_send(peer.second.peer, NETWORK_STREAM_ENTITY,
                       delIdStream.createPacket(ENET_PACKET_FLAG_RELIABLE));
        peer.second.pendingDelIds.clear();
      }

      if (int _pendingUpdates = pendingUpdates.size()) {
        BitStream deltaIdStream, deltaIdStreamUnreliable;
        deltaIdStream.write<PacketId>(DeltaIdPacket);
        deltaIdStreamUnreliable.write<PacketId>(DeltaIdPacket);
        deltaIdStream.write<int>(_pendingUpdates);
        deltaIdStreamUnreliable.write<int>(_pendingUpdates);
        for (auto id : pendingUpdates) {
          deltaIdStream.write<EntityId>(id);
          deltaIdStreamUnreliable.write<EntityId>(id);

          Entity* ent = entities[id].get();

          BitStream::Context ctxt = BitStream::ToClient;
          if (ent->getOwnership(&peer.second)) ctxt = BitStream::ToClientLocal;
          deltaIdStream.setContext(ctxt);
          deltaIdStreamUnreliable.setContext(ctxt);

          ent->serialize(deltaIdStream);
          ent->serializeUnreliable(deltaIdStreamUnreliable);
        }
        ENetPacket* packet =
            deltaIdStream.createPacket(ENET_PACKET_FLAG_RELIABLE);
        ENetPacket* packetUnreliable = deltaIdStream.createPacket(0);
        enet_peer_send(peer.second.peer, NETWORK_STREAM_ENTITY, packet);
        enet_peer_send(peer.second.peer, NETWORK_STREAM_ENTITY,
                       packetUnreliable);
        // need not be cleared because std::vector will clean itself up
      }

      if (int queuedEvents = peer.second.queuedEvents.size()) {
        for (int i = 0; i < queuedEvents; i++) {
          BitStream eventPacket;
          eventPacket.write<PacketId>(EventPacket);
          eventPacket.write<CustomEventID>(peer.second.queuedEvents[i].first);
          eventPacket.writeStream(*peer.second.queuedEvents[i].second);
          delete peer.second.queuedEvents[i].second;
          ENetPacket* packet =
              eventPacket.createPacket(ENET_PACKET_FLAG_RELIABLE);
          enet_peer_send(peer.second.peer, NETWORK_STREAM_EVENT, packet);
        }
        peer.second.queuedEvents.clear();
      }

      if (peer.second.noob && peer.second.playerEntity) {
        {
          std::vector<CVar*> cvars =
              Settings::singleton()->getWithFlag(CVARF_REPLICATE);

          BitStream cvarsPacket;
          cvarsPacket.write<PacketId>(CvarPacket);
          cvarsPacket.write<int>(cvars.size());
          for (auto cvar : cvars) {
            cvarsPacket.writeString(cvar->getName());
            cvarsPacket.writeString(cvar->getValue());
          }

          enet_peer_send(peer.second.peer, NETWORK_STREAM_META,
                         cvarsPacket.createPacket(ENET_PACKET_FLAG_RELIABLE));
        }

        for (auto& _peer : peers) {
          if (!_peer.second.playerEntity || peer.first == _peer.first) continue;
          BitStream newPeerPacket;
          newPeerPacket.write<PacketId>(NewPeerPacket);
          newPeerPacket.write<int>(_peer.first);
          newPeerPacket.write<EntityId>(
              _peer.second.playerEntity->getEntityId());
          enet_peer_send(
              peer.second.peer,
              NETWORK_STREAM_ENTITY,  // even though this is technically a meta
                                      // packet it needs to be in the entity
                                      // stream so the other entities can be
                                      // read by the remote peer in time
              newPeerPacket.createPacket(ENET_PACKET_FLAG_RELIABLE));
        }
        peer.second.noob = false;
      }
    }

    if (int _pendingCvarUpdates = pendingCvars.size()) {
      BitStream cvarUpdateStream;
      cvarUpdateStream.write<PacketId>(CvarPacket);
      cvarUpdateStream.write<int>(_pendingCvarUpdates);
      for (auto var : pendingCvars) {
        CVar* cvar = Settings::singleton()->getCvar(var.c_str());
        if (!cvar) {
          cvarUpdateStream.writeString("");
          cvarUpdateStream.writeString("");
          continue;
        }

        cvarUpdateStream.writeString(cvar->getName());
        cvarUpdateStream.writeString(cvar->getValue());
      }
      pendingCvars.clear();
    }

    if (int _pendingUpdates = pendingUpdates.size()) {
      for (auto& peer : peers) {
        if (peer.second.type != Peer::ConnectedPlayer) continue;
        BitStream deltaIdStream;
        deltaIdStream.write<PacketId>(DeltaIdPacket);
        deltaIdStream.write<int>(_pendingUpdates);
        for (auto id : pendingUpdates) {
          deltaIdStream.write<EntityId>(id);
          Entity* ent = entities[id].get();
          BitStream::Context ctxt = BitStream::ToClient;
          if (ent->getOwnership(&peer.second)) ctxt = BitStream::ToClientLocal;
          deltaIdStream.setContext(ctxt);
          ent->serialize(deltaIdStream);
        }
        ENetPacket* packet =
            deltaIdStream.createPacket(ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer.second.peer, NETWORK_STREAM_ENTITY, packet);
      }
      pendingUpdates.clear();
    }

    if (int _pendingUpdatesUnreliable = pendingUpdatesUnreliable.size()) {
      for (auto& peer : peers) {
        if (peer.second.type != Peer::ConnectedPlayer) continue;
        BitStream deltaIdStream;
        deltaIdStream.write<PacketId>(DeltaIdPacket);
        deltaIdStream.write<int>(_pendingUpdatesUnreliable);
        for (auto id : pendingUpdatesUnreliable) {
          deltaIdStream.write<EntityId>(id);
          Entity* ent = entities[id].get();
          BitStream::Context ctxt = BitStream::ToClient;
          if (ent->getOwnership(&peer.second)) ctxt = BitStream::ToClientLocal;
          deltaIdStream.setContext(ctxt);
          ent->serializeUnreliable(deltaIdStream);
        }
        ENetPacket* packet = deltaIdStream.createPacket(0);
        enet_peer_send(peer.second.peer, NETWORK_STREAM_ENTITY, packet);
      }
      pendingUpdatesUnreliable.clear();
    }

    if (distributedTime > nextDtPacket) {
      nextDtPacket += distributedTime + sv_dtrate.getFloat();

      BitStream timeStream;
      timeStream.write<PacketId>(DistributedTimePacket);
      timeStream.write<float>(distributedTime);

      timeStream.write<int>(peers.size());
      for (auto peer : peers) {
        timeStream.write<int>(peer.first);
        timeStream.write<int>(peer.second.peer->roundTripTime);
        timeStream.write<int>(peer.second.peer->packetLoss);
      }
      enet_host_broadcast(host, NETWORK_STREAM_META,
                          timeStream.createPacket(0));
    }
  } else {
#ifndef DISABLE_EASY_PROFILER
    EASY_BLOCK("Frontend Peer Management");
#endif
    if (localPeer.playerEntity) {
      if (localPeer.playerEntity->remotePeerId.get() != localPeer.peerId) {
        localPeer.playerEntity = nullptr;
      }
    }
    if (!localPeer.playerEntity) {
      for (auto& pair : entities) {
        Entity* entity = pair.second.get();
        if (Player* playerEntity = dynamic_cast<Player*>(entity)) {
          if (playerEntity->remotePeerId.get() == localPeer.peerId) {
            Log::printf(
                LOG_DEBUG, "Found player entity id %i, eid %i, username %s",
                playerEntity->remotePeerId.get(), playerEntity->getEntityId(),
                playerEntity->displayName.get().c_str());
            localPeer.playerEntity = playerEntity;
          }
        }
      }
    }

    for (auto& peer : peers) {
      if (!peer.second.playerEntity) {
        std::vector<Entity*> ents = findEntitiesByType(playerType);
        for (auto ent : ents) {
          Player* player = dynamic_cast<Player*>(ent);
          if (player->remotePeerId.get() == peer.second.peerId) {
            peer.second.playerEntity = player;
            break;
          }
        }
      } else {
        if (peer.second.playerEntity->remotePeerId.get() !=
            peer.second.peerId) {
          peer.second.playerEntity = NULL;
        }
      }
    }

    if (int _pendingUpdates = pendingUpdates.size()) {
      BitStream deltaIdStream;
      deltaIdStream.write<PacketId>(DeltaIdPacket);
      deltaIdStream.write<int>(_pendingUpdates);
      for (auto id : pendingUpdates) {
        deltaIdStream.write<EntityId>(id);
        Entity* ent = entities[id].get();
        BitStream::Context ctxt = BitStream::ToServer;
        if (ent->getOwnership(&localPeer)) ctxt = BitStream::ToServerLocal;
        deltaIdStream.setContext(ctxt);
        ent->serialize(deltaIdStream);
      }
      ENetPacket* packet =
          deltaIdStream.createPacket(ENET_PACKET_FLAG_RELIABLE);
      enet_peer_send(localPeer.peer, 0, packet);
      pendingUpdates.clear();
    }

    if (int _pendingUpdatesUnreliable = pendingUpdatesUnreliable.size()) {
      BitStream deltaIdStream;
      deltaIdStream.write<PacketId>(DeltaIdPacket);
      deltaIdStream.write<int>(_pendingUpdatesUnreliable);
      for (auto id : pendingUpdatesUnreliable) {
        deltaIdStream.write<EntityId>(id);
        Entity* ent = entities[id].get();
        BitStream::Context ctxt = BitStream::ToServer;
        if (ent->getOwnership(&localPeer)) ctxt = BitStream::ToServerLocal;
        deltaIdStream.setContext(ctxt);
        ent->serializeUnreliable(deltaIdStream);
      }
      ENetPacket* packet = deltaIdStream.createPacket(0);
      enet_peer_send(localPeer.peer, 0, packet);
      pendingUpdatesUnreliable.clear();
    }

    if (int _pendingRconCommands = pendingRconCommands.size()) {
      for (auto command : pendingRconCommands) {
        BitStream rconStream;
        rconStream.write<PacketId>(RconPacket);
        rconStream.writeString(command.first);
        rconStream.writeString(command.second);
        enet_peer_send(localPeer.peer, 0,
                       rconStream.createPacket(ENET_PACKET_FLAG_RELIABLE));
      }
      pendingRconCommands.clear();
    }
  }

  ticks++;
}

static CVar sv_maxpeers("sv_maxpeers", "32",
                        CVARF_SAVE | CVARF_REPLICATE | CVARF_GLOBAL);
static CVar net_inbandwidth("net_inbandwidth", "0", CVARF_SAVE | CVARF_GLOBAL);
static CVar net_outbandwidth("net_outbandwidth", "0",
                             CVARF_SAVE | CVARF_GLOBAL);

void NetworkManager::start(int port) {
  if (host) {
    Log::printf(LOG_WARN, "host = %p", host);
    enet_host_destroy(host);
  }
  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = port;
  host = enet_host_create(&address, sv_maxpeers.getInt(), NETWORK_STREAM_MAX,
                          net_inbandwidth.getInt(), net_outbandwidth.getInt());

  backend = true;

  if (host == nullptr) {
    throw std::runtime_error("enet_host_create returned NULL");
  }

  Log::printf(LOG_INFO, "Started server on port %i", port);
}

void NetworkManager::connect(std::string address, int port) {
  if (host) {
    Log::printf(LOG_WARN, "host = %p", host);
    enet_host_destroy(host);
  }
  ENetAddress _address;
  enet_address_set_host(&_address, address.c_str());
  _address.port = port;

  backend = false;

  host = enet_host_create(NULL, 1, NETWORK_STREAM_MAX, net_inbandwidth.getInt(),
                          net_outbandwidth.getInt());
  if (host == nullptr)
    throw std::runtime_error("enet_host_create returned NULL");

  localPeer.peer = enet_host_connect(host, &_address, 2, 0);
  if (localPeer.peer == nullptr)
    throw std::runtime_error("enet_host_connect returned NULL");

  localPeer.type = Peer::Undifferentiated;
  Log::printf(LOG_INFO, "Connecting to %s:%i", address.c_str(), port);
}

Peer* NetworkManager::getPeerById(int id) {
  auto it = peers.find(id);
  if (it != peers.end()) {
    return &peers[id];
  }
  return NULL;
}

void NetworkManager::handleDisconnect() {
  entities.clear();
  peers.clear();
}

void NetworkManager::registerConstructor(EntityConstructorFunction func,
                                         std::string type) {
  Log::printf(LOG_DEBUG, "Registered entity type %s", type.c_str());
  constructors[type] = func;
}

void NetworkManager::deleteEntity(EntityId id) {
  auto it = entities.find(id);
  if (it != entities.end()) {
    if (backend) {
      for (auto& peer : peers) {
        if (peer.second.playerEntity) peer.second.pendingDelIds.push_back(id);
      }
    }
    entities.erase(it);
  } else {
    Log::printf(LOG_ERROR, "Attempt to delete entity id %i", id);
    throw std::runtime_error("Invalid delete entity id");
  }
}

Entity* NetworkManager::instantiate(std::string typeName, int _id) {
  auto it = constructors.find(typeName);
  if (it != constructors.end()) {
    EntityId id = 0;
    if (_id == -1) {
      id = lastId++;
    } else {
      id = _id;
    }
    Entity* ent = constructors[typeName](this, id);
    if (!ent) {
      Log::printf(LOG_ERROR, "Constructor for %s is NULL", typeName.c_str());
      throw std::runtime_error("Could not instantiate entity");
    }
    entities[id].reset(ent);
    if (backend) {
      for (auto& peer : peers) {
        if (peer.second.playerEntity) peer.second.pendingNewIds.push_back(id);
      }
    }
    return entities[id].get();
  } else {
    Log::printf(LOG_ERROR, "Could not instantiate entity of type %s",
                typeName.c_str());
    throw std::runtime_error("Could not instantiate unknown entity");
  }
}

Entity* NetworkManager::findEntityByType(std::string typeName) {
  for (auto& entity : entities) {
    if (entity.second->getTypeName() == typeName) return entity.second.get();
  }
  return NULL;
}

Entity* NetworkManager::getEntityById(EntityId id) {
  auto it = entities.find(id);
  if (it == entities.end()) return NULL;
  return it->second.get();
}

std::vector<Entity*> NetworkManager::findEntitiesByType(std::string typeName) {
  std::vector<Entity*> ret;
  for (auto& entity : entities) {
    if (entity.second->getTypeName() == typeName)
      ret.push_back(entity.second.get());
  }
  return ret;
}

void NetworkManager::sendCustomEvent(CustomEventID id, BitStream& stream) {
  if (isBackend()) {
    for (auto& peer : peers) {
      peer.second.queuedEvents.push_back({id, new BitStream(stream)});
    }
  } else {
  }
}

void NetworkManager::sendCustomEvent(int peerId, CustomEventID id,
                                     BitStream& stream) {}

void NetworkManager::initialize() { enet_initialize(); }

void NetworkManager::deinitialize() { enet_deinitialize(); }
}  // namespace rdm::network
