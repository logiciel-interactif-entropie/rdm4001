#include "world.hpp"

#include <chrono>
#include <format>

#include "fun.hpp"
#include "input.hpp"
#include "logging.hpp"
#include "physics.hpp"
#include "scheduler.hpp"
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

World::World(WorldConstructorSettings settings) {
  title = "A rdm presentation";
  name = settings.name;

  scheduler.reset(new Scheduler());
  scheduler->addJob(new WorldJob(this));
  scheduler->addJob(new WorldTitleJob(this));

  scriptContext.reset(new script::Context(this));
  if (settings.physics) physics.reset(new PhysicsWorld(this));
  if (settings.network) networkManager.reset(new network::NetworkManager(this));

  this->game = settings.game;
  running = true;

  Input::singleton()->quitSignal.listen([this](InputObject o) {
    Log::printf(LOG_DEBUG, "Received quit signal, waiting for worldLock");
    std::scoped_lock lock(worldLock);
    running = false;
  });
}

World::~World() {
  Log::printf(LOG_DEBUG,
              "Dtor on world, running = %s. We hope to see you soon.",
              running ? "true" : "false");
  setRunning(false);
  getScheduler()->waitToWrapUp();
}

void World::tick() {
  stepping.fire();

  stepped.fire();
}

void World::setTitle(std::string title) { this->title = title; }
}  // namespace rdm
