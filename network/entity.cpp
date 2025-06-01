#include "entity.hpp"

#include <string>

#include "world.hpp"

namespace rdm::network {
template <>
void ReplicateProperty<std::string>::serialize(BitStream& stream) {
  stream.writeString(value);
}

template <>
void ReplicateProperty<std::string>::deserialize(BitStream& stream) {
  setRemote(stream.readString());
}

template <>
void ReplicateProperty<int>::serialize(BitStream& stream) {
  stream.write(value);
}

template <>
void ReplicateProperty<int>::deserialize(BitStream& stream) {
  setRemote(stream.read<int>());
}

Entity::Entity(NetworkManager* manager, EntityId id) {
  this->id = id;
  this->manager = manager;
}

void Entity::precache(NetworkManager* manager) {}

std::string Entity::getEntityInfo() { return ""; }

Entity::~Entity() {}

World* Entity::getWorld() { return this->manager->getWorld(); }
Game* Entity::getGame() { return this->manager->getGame(); }
gfx::Engine* Entity::getGfxEngine() { return this->manager->getGfxEngine(); }
}  // namespace rdm::network
