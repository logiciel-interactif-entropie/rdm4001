#include "testgame.hpp"

#include "testsystem.hpp"
namespace test {
TestGame::TestGame() : rdm::Game(true) {}

void TestGame::initialize() { currentTest->initializeGame(this); }
}  // namespace test
