#include "world.hpp"

#include <chrono>
#include <format>

#include "fun.hpp"
#include "game.hpp"
#include "input.hpp"
#include "localization.hpp"
#include "logging.hpp"
#include "object.hpp"
#include "object_property.hpp"
#include "physics.hpp"
#include "scheduler.hpp"
#include "script/script.hpp"
#include "settings.hpp"

namespace rdm {
class WorldJob : public SchedulerJob {
  World* world;

 public:
  WorldJob(World* world) : SchedulerJob("World", false), world(world) {}

  virtual Result step() {
    using namespace std::chrono_literals;
    if (!world->getRunning()) return Cancel;

    world->time = getStats().time;

    if (getStats().schedulerId == 0) Input::singleton()->flushEvents();
    world->tick();

    return Stepped;
  }

  virtual void error(std::exception& e) { world->running = false; }
};

static CVar cl_showstats("cl_showstats", "0", CVARF_SAVE | CVARF_GLOBAL);

class WorldTitleJob : public SchedulerJob {
  World* world;
  std::string oldTitle;

 public:
  WorldTitleJob(World* world) : SchedulerJob("WorldTitleJob"), world(world) {
    oldTitle = "";
  }

  virtual double getFrameRate() { return 1.0 / 20.0; }

  virtual Result step() {
    std::string title = world->title;
    if (cl_showstats.getBool()) {
      std::string fpsStatus = "";
      if (SchedulerJob* worldJob = world->scheduler->getJob("World")) {
        fpsStatus += std::format("W: {:0.2f}",
                                 1.0 / worldJob->getStats().getAvgDeltaTime());
      }
      if (SchedulerJob* physicsJob = world->scheduler->getJob("Physics")) {
        fpsStatus += std::format(
            " P: {:0.2f}", 1.0 / physicsJob->getStats().getAvgDeltaTime());
      }
      if (SchedulerJob* renderJob = world->scheduler->getJob("Render")) {
        fpsStatus += std::format(" R: {:0.2f}",
                                 1.0 / renderJob->getStats().getAvgDeltaTime());
      }
      if (SchedulerJob* networkJob = world->scheduler->getJob("Network")) {
        fpsStatus += std::format(
            " N: {:0.2f}", 1.0 / networkJob->getStats().getAvgDeltaTime());
      }
      world->changingTitle.fire(std::format("{} ({})", title, fpsStatus));

    } else {
      if (oldTitle != title) {
        world->changingTitle.fire(title);
        oldTitle = title;
      }
    }

    return Stepped;
  }
};

RDM_REFLECTION_BEGIN_DESCRIBED(World);
RDM_REFLECTION_PROPERTY_STRING(World, Title, &World::getTitle,
                               &World::setTitle);
RDM_REFLECTION_PROPERTY_STRING(World, Name, &World::getName, NULL);
RDM_REFLECTION_PROPERTY_OBJECT(World, Game, &World::getGame, NULL);
RDM_REFLECTION_PROPERTY_EVENT(World, Stepped, &World::getLuaStepped);
// RDM_REFLECTION_PROPERTY_FLOAT(
//  World, Time, [](World* world) { return world->getWorldJob(); }, NULL);
RDM_REFLECTION_END_DESCRIBED();

World::World(WorldConstructorSettings settings) {
  title = "A rdm presentation";
  name = settings.name;

  scheduler.reset(new Scheduler());
  worldJob = scheduler->addJob(new WorldJob(this));
  scheduler->addJob(new WorldTitleJob(this));

  if (settings.physics) physics.reset(new PhysicsWorld(this));
  if (settings.network) networkManager.reset(new network::NetworkManager(this));

  this->game = settings.game;
  running = true;

  Input::singleton()->quitSignal.listen([this](InputObject o) {
    Log::printf(LOG_DEBUG, "Received quit signal, waiting for worldLock");
    std::scoped_lock lock(worldLock);
    running = false;
  });

  scriptContext.reset(new script::ScriptContext(this));

  stepped.listen([this] { luaStepped.fire([](lua_State* L) { return 0; }); });
}

World::~World() {
  Log::printf(LOG_DEBUG, "Dtor on world, running = %s. %s",
              running ? "true" : "false",
              Lc(RDM_WORLD_DTOR_GOODBYE, "We hope to see you soon."));
  setRunning(false);
  getScheduler()->waitToWrapUp();
}

void World::tick() {
  stepping.fire();

  stepped.fire();
}

void World::setTitle(std::string title) { this->title = title; }
}  // namespace rdm
