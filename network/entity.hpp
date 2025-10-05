#pragma once
#include <stdint.h>

#include <string>

#include "bitstream.hpp"
#include "object.hpp"
#include "signal.hpp"

namespace rdm {
class World;
class Game;
class ResourceManager;

namespace gfx {
class Engine;
}
}  // namespace rdm

namespace rdm::network {
struct Peer;
typedef uint16_t EntityId;

enum ReplicateReliability {
  Reliable,
  ReliableSequencedUpdates = Reliable,
  ReliableUnsequencedUpdates,
  UnreliableSequencedUpdates,
  Unreliable,
};

template <typename T>
class ReplicateProperty {
  friend class NetworkManager;
  const char* type = typeid(T).name();
  T value;
  bool dirty;

  void setRemote(T v) {
    changingRemotely.fire(value, v);
    changing.fire(value, v);
    value = v;
    dirty = false;
  }

 public:
  // old, new
  Signal<T, T> changingLocally;
  Signal<T, T> changingRemotely;
  Signal<T, T> changing;

  void set(T v) {
    changingLocally.fire(value, v);
    changing.fire(value, v);
    value = v;
    dirty = true;
  }

  T get() { return value; }

  bool isDirty() { return dirty; };
  void clearDirty() { dirty = false; }

  void serialize(BitStream& stream);
  void deserialize(BitStream& stream);

  const char* getType() { return type; }
};

class NetworkManager;
class Entity : public reflection::Object {
  RDM_OBJECT;
  RDM_OBJECT_DEF(Entity, reflection::Object);

  NetworkManager* manager;
  EntityId id;

 public:
  // use NetworkManager::instantiate
  Entity(NetworkManager* manager, EntityId id);
  virtual ~Entity();

  World* getWorld();
  Game* getGame();
  NetworkManager* getManager() { return manager; }
  gfx::Engine* getGfxEngine();

  // use only on local client when hosting
  Entity* getRemoteEntity();
  template <typename T>
  T* getRemoteEntity() {
    return dynamic_cast<T*>(getRemoteEntity());
  };

  EntityId getEntityId() { return id; }

  static void precache(NetworkManager* manager);

  virtual void tick() {};

  virtual void serialize(BitStream& stream) {};
  virtual void deserialize(BitStream& stream) {};

  virtual void serializeUnreliable(BitStream& stream) {};
  virtual void deserializeUnreliable(BitStream& stream) {};

  virtual bool getOwnership(Peer* peer) { return false; }

  virtual bool dirty() { return false; }
  virtual const char* getTypeName() { return "Entity"; };

  virtual std::string getEntityInfo();
};

template <typename T>
Entity* EntityConstructor(NetworkManager* manager, EntityId id) {
  return new T(manager, id);
}
};  // namespace rdm::network
