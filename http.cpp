#include "http.hpp"

namespace rdm {
static HttpManager* _singleton = 0;

HttpManager::HttpManager() {}

HttpManager* HttpManager::singleton() {
  if (!_singleton) _singleton = new HttpManager();
  return _singleton;
}
};  // namespace rdm
