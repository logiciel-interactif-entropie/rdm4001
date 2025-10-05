#include "signal.hpp"

namespace rdm {
static ClosureId lastClosureId = 0;
ClosureId __newClosureId() { return lastClosureId++; }

}  // namespace rdm
