#pragma once

#include <bullet/BulletCollision/BroadphaseCollision/btBroadphaseInterface.h>
#include <bullet/BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <bullet/BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <bullet/BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <bullet/BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <bullet/btBulletDynamicsCommon.h>

#include <glm/glm.hpp>
#include <memory>
#include <mutex>

#include "LinearMath/btMatrix3x3.h"
#include "signal.hpp"

#define PHYSICS_FRAMERATE (1.0 / 60.0)

#define PHYSICS_INDEX_WORLD 1
#define PHYSICS_INDEX_PLAYER 2
#define PHYSICS_INDEX_PROP 4

namespace rdm {
namespace gfx {
class Engine;
}

class World;
class PhysicsWorld {
  std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
  std::unique_ptr<btBroadphaseInterface> overlappingPairCache;
  std::unique_ptr<btCollisionDispatcher> dispatcher;
  std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration;
  btAlignedObjectArray<std::unique_ptr<btCollisionShape>> collisionShapes;
  std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld;

  std::unique_ptr<btIDebugDraw> debugDraw;
  bool debugDrawEnabled;
  bool debugDrawInit;
  bool stepSimulation;
  World* world;

 public:
  PhysicsWorld(World* world);

  Signal<> physicsStepping;
  void stepWorld();

  void setStepSimulation(bool s) { stepSimulation = s; };

  bool isDebugDrawEnabled() { return debugDrawEnabled; }
  bool isDebugDrawInitialized() { return debugDrawInit; }
  void initializeDebugDraw(rdm::gfx::Engine* engine);

  btDiscreteDynamicsWorld* getWorld() { return dynamicsWorld.get(); }
  World* getRWorld() { return world; }
  std::mutex mutex;
};

class BulletHelpers {
 public:
  static btVector3 toVector3(glm::vec3 v) { return btVector3(v.x, v.y, v.z); };

  static glm::vec3 fromVector3(btVector3 v) {
    return glm::vec3(v.x(), v.y(), v.z());
  }

  static glm::mat3 fromMat3(btMatrix3x3 m) {
    return glm::mat3(fromVector3(m.getColumn(0)), fromVector3(m.getColumn(1)),
                     fromVector3(m.getColumn(2)));
  }

  static btMatrix3x3 toMat3(glm::mat3 m) {
    return btMatrix3x3(toVector3(m[0]), toVector3(m[1]), toVector3(m[2]));
  }
};
};  // namespace rdm
