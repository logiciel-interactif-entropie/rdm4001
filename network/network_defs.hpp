#pragma once
#include <functional>
#include <vector>

#include "entity.hpp"

#define NETWORK_STREAM_META 0
#define NETWORK_STREAM_ENTITY 1
#define NETWORK_STREAM_EVENT 2
#define NETWORK_STREAM_MAX 4

#define NETWORK_DISCONNECT_FORCED 0
#define NETWORK_DISCONNECT_USER 1
#define NETWORK_DISCONNECT_TIMEOUT 2

namespace rdm::network {
typedef uint16_t CustomEventID;

typedef std::vector<std::pair<CustomEventID, BitStream*>> CustomEventList;

typedef std::function<Entity*(NetworkManager*, EntityId)>
    EntityConstructorFunction;
}  // namespace rdm::network
