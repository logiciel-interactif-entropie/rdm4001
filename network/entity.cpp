#include "entity.hpp"

#include <string>

#include "game.hpp"
#include "world.hpp"

namespace rdm::network {
RDM_REFLECTION_BEGIN_DESCRIBED(Entity)
RDM_REFLECTION_PROPERTY_INT(Entity, Id, &Entity::getEntityId, NULL);
RDM_REFLECTION_PROPERTY_STRING(Entity, Type, &Entity::getTypeName, NULL);
RDM_REFLECTION_PROPERTY_STRING(Entity, Info, &Entity::getEntityInfo, NULL);
RDM_REFLECTION_PROPERTY_OBJECT(Entity, World, &Entity::getWorld, NULL);
RDM_REFLECTION_END_DESCRIBED()

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

Entity* Entity::getRemoteEntity() {}

World* Entity::getWorld() { return this->manager->getWorld(); }
Game* Entity::getGame() { return this->manager->getGame(); }
gfx::Engine* Entity::getGfxEngine() { return this->manager->getGfxEngine(); }
}  // namespace rdm::network
