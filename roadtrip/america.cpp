#include "america.hpp"

#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/mesh.hpp"
#include "network/entity.hpp"
#include "network/network.hpp"
#include "pawn.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

namespace rt {
America::America(rdm::network::NetworkManager* manager,
                 rdm::network::EntityId id)
    : rdm::network::Entity(manager, id) {
  fillLocationInfo();
  turnNumber = 0;
  if (!getManager()->isBackend())
    closure = getGfxEngine()->renderStepped.listen([this] {
      getGfxEngine()
          ->getTextureCache()
          ->getOrLoad2d("dat6/map.png")
          .value()
          .second->setFiltering(rdm::gfx::BaseTexture::Nearest,
                                rdm::gfx::BaseTexture::Nearest);

      rdm::gfx::Primitive* prim =
          getGfxEngine()->getMeshCache()->get(rdm::gfx::Primitive::PlaneZ);

      std::shared_ptr<rdm::gfx::Material> material =
          getGfxEngine()->getMaterialCache()->getOrLoad("RoadTripMap").value();
      rdm::gfx::BaseProgram* program =
          material->prepareDevice(getGfxEngine()->getDevice(), 0);
      program->setParameter(
          "model", rdm::gfx::DtMat4,
          rdm::gfx::BaseProgram::Parameter{
              .matrix4x4 = glm::scale(glm::vec3(963, 515, 1))});
      program->setParameter(
          "texture0", rdm::gfx::DtSampler,
          rdm::gfx::BaseProgram::Parameter{
              .texture = {.slot = 0,
                          .texture = getGfxEngine()
                                         ->getTextureCache()
                                         ->getOrLoad2d("dat6/map.png")
                                         .value()
                                         .second}});
      program->bind();
      prim->render(getGfxEngine()->getDevice());
    });
}

America::~America() {
  if (!getManager()->isBackend())
    getGfxEngine()->renderStepped.removeListener(closure);
}

void America::tick() {
  if (getManager()->isBackend()) {
    std::vector<Entity*> pawns = getManager()->findEntitiesByType("Pawn");
    if (!pawns.size()) return;
    bool allTurnsDone = true;
    for (auto _pawn : pawns) {
      Pawn* pawn = dynamic_cast<Pawn*>(_pawn);
      if (!pawn->isTurnDone()) {
        if (pawn->isVacationed()) continue;

        allTurnsDone = false;
        break;
      }
    }
    if (allTurnsDone) {
      rdm::Log::printf(rdm::LOG_DEBUG, "TURN DONE");
      for (auto _pawn : pawns) {
        Pawn* pawn = dynamic_cast<Pawn*>(_pawn);
        if (pawn->isVacationed()) continue;
        pawn->endTurn();
      }
      turnNumber++;
    }
  }
}

void America::fillLocationInfo() {
  locationInfo[SoCal] =
      LocationInfo{.name = "Southern California",
                   .connectedLocations = {{USA, NorCal}, {Hot, Arizona}},
                   .mapPosition = glm::ivec2(141, 290)};  // START

  locationInfo[NovaScotia] =
      LocationInfo{.name = "Nova Scotia",
                   .connectedLocations = {{Canada, NewBrunswick}},
                   .mapPosition = glm::ivec2(817, 159)};  // END

  locationInfo[NorCal] =
      LocationInfo{.name = "Northern California",
                   .connectedLocations = {{USA, SoCal}, {USA, Oregon}},
                   .mapPosition = glm::ivec2(155, 205)};
  locationInfo[Oregon] =
      LocationInfo{.name = "Oregon",
                   .connectedLocations = {{USA, NorCal}, {USA, Washington}},
                   .mapPosition = glm::ivec2(175, 152)};
  locationInfo[Washington] =
      LocationInfo{.name = "Washington",
                   .connectedLocations = {{USA, Oregon}, {USCIS, Vancouver}},
                   .mapPosition = glm::ivec2(185, 119)};
  locationInfo[Vancouver] = LocationInfo{
      .name = "Vancouver",
      .connectedLocations = {{USCIS, Washington}, {Canada, Alberta}},
      .mapPosition = glm::ivec2(191, 66)};
  locationInfo[Alberta] = LocationInfo{
      .name = "Alberta",
      .connectedLocations = {{Canada, Vancouver}, {Canada, Sasketchewan}},
      .mapPosition = glm::ivec2(295, 49)};
  locationInfo[Sasketchewan] = LocationInfo{
      .name = "Sasketchewan",
      .connectedLocations = {{Canada, Alberta}, {Canada, Manitoba}},
      .mapPosition = glm::ivec2(355, 79)};
  locationInfo[Manitoba] = LocationInfo{
      .name = "Manitoba",
      .connectedLocations = {{Canada, Sasketchewan}, {Canada, Ontario}},
      .mapPosition = glm::ivec2(437, 66)};
  locationInfo[Ontario] =
      LocationInfo{.name = "Ontario",
                   .connectedLocations = {{Canada, Manitoba},
                                          {Canada, Ottawa},
                                          {USCIS, Minnesota}},
                   .mapPosition = glm::ivec2(531, 126)};
  locationInfo[Ottawa] = LocationInfo{.name = "Ottawa",
                                      .connectedLocations = {{Canada, Ontario},
                                                             {Quebec, Montreal},
                                                             {USCIS, Michigan}},
                                      .mapPosition = glm::ivec2(626, 191)};
  locationInfo[Montreal] =
      LocationInfo{.name = "Montreal",
                   .connectedLocations = {{Quebec, Ottawa},
                                          {Quebec, NewBrunswick},
                                          {USCIS, Boston}},
                   .mapPosition = glm::ivec2(682, 163)};
  locationInfo[NewBrunswick] =
      LocationInfo{.name = "New Brunswick",
                   .connectedLocations = {{Quebec, Montreal},
                                          {Canada, NovaScotia},
                                          {USCIS, Boston}},
                   .mapPosition = glm::ivec2(759, 153)};
  locationInfo[Arizona] =
      LocationInfo{.name = "Arizona",
                   .connectedLocations = {{Hot, SoCal}, {Hot, NewMexico}},
                   .mapPosition = glm::ivec2(239, 344)};
  locationInfo[NewMexico] = LocationInfo{
      .name = "New Mexico",
      .connectedLocations = {{Hot, Arizona}, {Hot, Texas}, {Hot, OklahomaCity}},
      .mapPosition = glm::ivec2(301, 353)};

  locationInfo[Texas] = LocationInfo{
      .name = "Texas",
      .connectedLocations = {{Hot, NewMexico}, {Republican, Georgia}},
      .mapPosition = glm::ivec2(422, 415)};
  locationInfo[Georgia] =
      LocationInfo{.name = "Georgia",
                   .connectedLocations = {{Republican, Texas}, {USA, Virginia}},
                   .mapPosition = glm::ivec2(606, 405)};
  locationInfo[Virginia] =
      LocationInfo{.name = "Virginia",
                   .connectedLocations = {{USA, Georgia}, {USA, NewYorkCity}},
                   .mapPosition = glm::ivec2(670, 316)};
  locationInfo[NewYorkCity] =
      LocationInfo{.name = "New York City",
                   .connectedLocations = {{USA, Virginia}, {USA, Boston}},
                   .mapPosition = glm::ivec2(693, 254)};
  locationInfo[Boston] =
      LocationInfo{.name = "Boston",
                   .connectedLocations = {{USA, NewYorkCity},
                                          {USCIS, Montreal},
                                          {USCIS, NewBrunswick}},
                   .mapPosition = glm::ivec2(707, 222)};

  locationInfo[OklahomaCity] = LocationInfo{
      .name = "Oklahoma City",
      .connectedLocations = {{Hot, NewMexico}, {Midwest, Missouri}},
      .mapPosition = glm::ivec2(414, 341)};
  locationInfo[Missouri] =
      LocationInfo{.name = "Missouri",
                   .connectedLocations = {{Midwest, OklahomaCity},
                                          {Midwest, Iowa},
                                          {Midwest, Illinois}},
                   .mapPosition = glm::ivec2(466, 308)};
  locationInfo[Iowa] = LocationInfo{
      .name = "Iowa",
      .connectedLocations = {{Midwest, Missouri}, {Midwest, Minnesota}},
      .mapPosition = glm::ivec2(459, 254)};
  locationInfo[Minnesota] =
      LocationInfo{.name = "Minnesota",
                   .connectedLocations = {{Midwest, Iowa}, {USCIS, Ontario}},
                   .mapPosition = glm::ivec2(452, 186)};
  locationInfo[Illinois] = LocationInfo{
      .name = "Illinois",
      .connectedLocations = {{Midwest, Missouri}, {Midwest, Michigan}},
      .mapPosition = glm::ivec2(525, 275)};
  locationInfo[Michigan] =
      LocationInfo{.name = "Michigan",
                   .connectedLocations = {{Midwest, Illinois}, {USCIS, Ottawa}},
                   .mapPosition = glm::ivec2(571, 236)};

  if (!getManager()->isBackend()) {
    getGfxEngine()->renderStepped.addClosure([this] {
      for (auto& [location, info] : locationInfo) {
        char txname[64];
        snprintf(txname, 64, "dat6/locations/%i.png", location);
        auto t = getGfxEngine()->getTextureCache()->getOrLoad2d(txname);
        if (!t) rdm::Log::printf(rdm::LOG_ERROR, "No texture %s", txname);
        info.logo = t ? t.value().second : 0;
      }
    });
  }
}
}  // namespace rt
