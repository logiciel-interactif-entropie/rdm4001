#include "roadtrip.hpp"

#include "america.hpp"
#include "network/network.hpp"
#include "pawn.hpp"
#include "settings.hpp"
namespace rt {
void RoadTrip::initialize() {
  rdm::WorldConstructorSettings& settings = getWorldConstructorSettings();
  settings.network = true;

  // if (rdm::Settings::singleton()->getHintDs()) {
  startServer();
  //} else {
  //  startClient();
  //}
}

void RoadTrip::addEntityConstructors(rdm::network::NetworkManager* manager) {
  manager->setPassword("RoadTripCanamerica");
  manager->registerConstructor(rdm::network::EntityConstructor<Pawn>, "Pawn");
  manager->registerConstructor(rdm::network::EntityConstructor<America>,
                               "America");
  manager->setPlayerType("Pawn");
}

struct HostSettings {
  int port = 793;
};
static HostSettings settings;

void RoadTrip::initializeClient() {
  world->setTitle("Road Trip");

  addEntityConstructors(world->getNetworkManager());

  static enum { MainMenu, Connect, Host } mode = MainMenu;

  gfxEngine->renderStepped.listen([this] {
    ImGui::Begin("Road Trip");
    static int port = 7938;
    switch (world->getNetworkManager()->getLocalPeer().type) {
      case rdm::network::Peer::Undifferentiated:
        ImGui::Text("Connecting...");
        break;
      default:
      case rdm::network::Peer::Unconnected:
        switch (mode) {
          case MainMenu:
            if (ImGui::Button("Connect")) mode = Connect;
            if (ImGui::Button("Host")) mode = Host;
            break;
          case Connect:
            static char inIp[64] = "127.0.0.1";
            ImGui::InputText("IP", inIp, 64);
            ImGui::InputInt("Port", &port);
            if (ImGui::Button("Connect")) {
              world->getNetworkManager()->connect(inIp, port);
            }
            if (ImGui::Button("Cancel")) mode = MainMenu;
            break;
          case Host:
            ImGui::InputInt("Port", &port);
            if (ImGui::Button("Host")) {
              lateInitServer();
              world->getNetworkManager()->connect("127.0.0.1", 7938);
            }
            if (ImGui::Button("Cancel")) mode = MainMenu;
            break;
        }
        ImGui::Begin("Copyright");
        ImGui::Text("%s", copyright());
        ImGui::End();
        break;
      case rdm::network::Peer::ConnectedPlayer:
        if (ImGui::Button("Disconnect")) {
          world->getNetworkManager()->requestDisconnect();
          if (worldServer) stopServer();
        }
        break;
    }
    ImGui::End();
  });

  world->stepped.listen([] {

  });
}

void RoadTrip::initializeServer() {
  worldServer->setTitle("Road Trip");

  addEntityConstructors(worldServer->getNetworkManager());
  worldServer->getNetworkManager()->start();

  worldServer->getNetworkManager()->instantiate("America");
}
}  // namespace rt
