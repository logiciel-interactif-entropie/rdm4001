#include "physics.hpp"

#include <bullet/BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <bullet/BulletCollision/CollisionDispatch/btCollisionConfiguration.h>
#include <bullet/BulletCollision/CollisionDispatch/btCollisionDispatcher.h>

#include "LinearMath/btIDebugDraw.h"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "logging.hpp"
#include "scheduler.hpp"
#include "settings.hpp"
#include "world.hpp"

namespace rdm {
class PhysicsJob : public SchedulerJob {
  PhysicsWorld* world;

 public:
  virtual double getFrameRate() { return PHYSICS_FRAMERATE; }

  PhysicsJob(PhysicsWorld* _world) : SchedulerJob("Physics"), world(_world) {}

  virtual Result step() {
    world->stepWorld();
    return Stepped;
  }
};

class DebugDrawer : public btIDebugDraw {
  rdm::gfx::Engine* engine;
  int debugMode;
  std::shared_ptr<rdm::gfx::Material> lineMaterial;
  std::unique_ptr<rdm::gfx::BaseBuffer> lineBuffer;
  std::unique_ptr<rdm::gfx::BaseArrayPointers> lineArrayPointers;

 public:
  virtual void drawLine(const btVector3& from, const btVector3& to,
                        const btVector3& color) {
    gfx::BaseProgram* bp = lineMaterial->prepareDevice(engine->getDevice(), 0);

    bp->setParameter(
        "from", gfx::DtVec3,
        gfx::BaseProgram::Parameter{.vec3 = BulletHelpers::fromVector3(from)});
    bp->setParameter(
        "to", gfx::DtVec3,
        gfx::BaseProgram::Parameter{.vec3 = BulletHelpers::fromVector3(to)});
    bp->setParameter(
        "color", gfx::DtVec3,
        gfx::BaseProgram::Parameter{.vec3 = BulletHelpers::fromVector3(color)});
    bp->bind();
    lineArrayPointers->bind();
    engine->getDevice()->draw(lineBuffer.get(), gfx::DtFloat,
                              gfx::BaseDevice::Lines, 2);
  }

  virtual void drawContactPoint(const btVector3& pointB,
                                const btVector3& normalB, btScalar distance,
                                int lifeTime, const btVector3& color) {
    gfx::BaseProgram* bp = lineMaterial->prepareDevice(engine->getDevice(), 0);

    bp->setParameter("from", gfx::DtVec3,
                     gfx::BaseProgram::Parameter{
                         .vec3 = BulletHelpers::fromVector3(pointB)});
    distance *= 10;
    bp->setParameter(
        "to", gfx::DtVec3,
        gfx::BaseProgram::Parameter{
            .vec3 = BulletHelpers::fromVector3(pointB + (normalB * distance))});
    bp->setParameter(
        "color", gfx::DtVec3,
        gfx::BaseProgram::Parameter{.vec3 = BulletHelpers::fromVector3(color)});
    bp->bind();
    lineArrayPointers->bind();
    engine->getDevice()->draw(lineBuffer.get(), gfx::DtFloat,
                              gfx::BaseDevice::Lines, 2);
  }

  virtual void reportErrorWarning(const char* warningString) {}

  virtual void draw3dText(const btVector3& location, const char* text) {}

  virtual void setDebugMode(int debugMode) { this->debugMode = debugMode; }

  virtual int getDebugMode() const { return debugMode; }

  DebugDrawer(rdm::gfx::Engine* engine) {
    this->engine = engine;
    lineMaterial =
        engine->getMaterialCache()->getOrLoad("DbgPhysicsLine").value();
    lineBuffer = engine->getDevice()->createBuffer();
    float lb[] = {0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    lineBuffer->upload(rdm::gfx::BaseBuffer::Array,
                       rdm::gfx::BaseBuffer::StaticDraw, sizeof(lb), lb);
    lineArrayPointers = engine->getDevice()->createArrayPointers();
    lineArrayPointers->addAttrib(rdm::gfx::BaseArrayPointers::Attrib(
        rdm::gfx::DtFloat, 0, 3, sizeof(float) * 3, 0, lineBuffer.get()));
    lineArrayPointers->upload();
  }
};

PhysicsWorld::PhysicsWorld(World* world) {
  world->getScheduler()->addJob(new PhysicsJob(this));

  debugDrawInit = false;

  collisionConfiguration.reset(new btDefaultCollisionConfiguration());
  dispatcher.reset(new btCollisionDispatcher(collisionConfiguration.get()));
  overlappingPairCache.reset(new btDbvtBroadphase());
  solver.reset(new btSequentialImpulseConstraintSolver);
  dynamicsWorld.reset(
      new btDiscreteDynamicsWorld(dispatcher.get(), overlappingPairCache.get(),
                                  solver.get(), collisionConfiguration.get()));
  dynamicsWorld->setGravity(btVector3(0, -10, 0));
  stepSimulation = true;

  Log::printf(LOG_DEBUG, "Initialized physics world");
}

static CVar r_physics("r_physics", "0", CVARF_SAVE | CVARF_GLOBAL);

void PhysicsWorld::initializeDebugDraw(rdm::gfx::Engine* engine) {
  std::scoped_lock l(mutex);

  debugDraw.reset(new DebugDrawer(engine));

  dynamicsWorld->setDebugDrawer(debugDraw.get());
  debugDrawInit = true;
}

void PhysicsWorld::stepWorld() {
  {
    std::scoped_lock l(mutex);
    debugDrawEnabled = r_physics.getInt() != 0;
    if (debugDrawInit) switch (r_physics.getInt()) {
        case 0:
          break;
        default:
        case 1:
          dynamicsWorld->getDebugDrawer()->setDebugMode(
              btIDebugDraw::DBG_DrawAabb | btIDebugDraw::DBG_DrawText);
          break;
        case 2:
          dynamicsWorld->getDebugDrawer()->setDebugMode(
              btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb);
          break;
        case 3:
          dynamicsWorld->getDebugDrawer()->setDebugMode(
              btIDebugDraw::DBG_DrawNormals);
          break;
        case 4:
          dynamicsWorld->getDebugDrawer()->setDebugMode(
              btIDebugDraw::DBG_DrawContactPoints);
          break;
        case 5:
          dynamicsWorld->getDebugDrawer()->setDebugMode(
              btIDebugDraw::DBG_MAX_DEBUG_DRAW_MODE);
          break;
      }

    if (stepSimulation) dynamicsWorld->stepSimulation(PHYSICS_FRAMERATE, 10);
  }

  if (stepSimulation) physicsStepping.fire();
}
};  // namespace rdm
