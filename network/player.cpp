#include "player.hpp"

#include <format>

#include "console.hpp"
#include "game.hpp"
#include "network.hpp"
namespace rdm::network {
Player::Player(NetworkManager* manager, EntityId id) : Entity(manager, id) {
  remotePeerId.set(-1);
}

Peer::Peer() {
  playerEntity = NULL;
  peer = NULL;
}

bool Player::isLocalPlayer() {
  if (getManager()->isBackend()) return false;
  return getManager()->getLocalPeer().playerEntity == this;
}

void Player::serialize(BitStream& stream) {
  Entity::serialize(stream);
  remotePeerId.serialize(stream);
  displayName.serialize(stream);
}

void Player::deserialize(BitStream& stream) {
  Entity::deserialize(stream);
  remotePeerId.deserialize(stream);
  displayName.deserialize(stream);
}

std::string Player::getEntityInfo() {
  std::string r = Entity::getEntityInfo();
  r += std::format("\nLocal player: {}\nPlayer id: {}\nUsername: {}",
                   isLocalPlayer() ? "true" : "false", remotePeerId.get(),
                   displayName.get().c_str());
  return r;
}

static ConsoleCommand players(
    "players", "players", "list players connected to server",
    [](Game* game, ConsoleArgReader reader) {
      if (!game->getWorldConstructorSettings().network)
        throw std::runtime_error("network disabled");
      if (game->getWorld()) {
        for (auto entity :
             game->getWorld()->getNetworkManager()->findEntitiesByType(
                 game->getWorld()->getNetworkManager()->getPlayerType())) {
          Player* player = dynamic_cast<Player*>(entity);
          Log::printf(LOG_INFO, "%i - %s (entity: %i)",
                      player->remotePeerId.get(),
                      player->displayName.get().c_str(), player->getEntityId());
        }
      } else if (game->getServerWorld()) {
        for (auto entity :
             game->getServerWorld()->getNetworkManager()->findEntitiesByType(
                 game->getServerWorld()
                     ->getNetworkManager()
                     ->getPlayerType())) {
          Player* player = dynamic_cast<Player*>(entity);
          Log::printf(LOG_INFO, "%i - %s (entity: %i)",
                      player->remotePeerId.get(),
                      player->displayName.get().c_str(), player->getEntityId());
        }
      }
    });
}  // namespace rdm::network
