#include "rgame.hpp"

#include <format>

#include "BulletCollision/CollisionShapes/btStaticPlaneShape.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btDefaultMotionState.h"
#include "LinearMath/btMotionState.h"
#include "LinearMath/btVector3.h"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "input.hpp"
#include "physics.hpp"
#include "world.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace rm {
struct REntity {
  int type;
  glm::vec3 position;
  float user0;

  btRigidBody* body;
  btMotionState* state;

  enum MotionType {
    STATIC,  // state = NULL
    DYNAMIC
  } motionType;
};

struct RGamePrivate {
  float cameraPitch;
  float cameraYaw;

  std::shared_ptr<gfx::Material> rayMarchMaterial;
  std::vector<REntity> entities;
  std::mutex entitiesMutex;

  void createBall(PhysicsWorld* world, float radius,
                  glm::vec3 pos = glm::vec3(0.0)) {
    std::scoped_lock l(world->mutex, entitiesMutex);
    REntity e;
    e.position = pos;
    e.type = 2;
    e.user0 = radius;
    btScalar mass(1.0);
    btSphereShape* sphere = new btSphereShape(radius);
    btTransform trans = btTransform::getIdentity();
    trans.setOrigin(btVector3(pos.x, pos.y, pos.z));
    e.state = new btDefaultMotionState(trans);
    btVector3 inertia;
    sphere->calculateLocalInertia(mass, inertia);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, e.state, sphere,
                                                    inertia);
    rbInfo.m_friction = 0.1;
    rbInfo.m_restitution = 1.0;
    e.body = new btRigidBody(rbInfo);
    e.motionType = REntity::DYNAMIC;
    world->getWorld()->addRigidBody(e.body);
    entities.push_back(e);
  }

  void createPlane(PhysicsWorld* world) {
    std::scoped_lock l(world->mutex, entitiesMutex);
    REntity e;
    e.motionType = REntity::STATIC;
    e.type = 1;
    // btStaticPlaneShape* plane = new btStaticPlaneShape(btVector3(0, 1,
    // 0), 2.2);
    btBoxShape* box = new btBoxShape(btVector3(50, 1, 50));
    btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0, NULL, box);
    rbInfo.m_restitution = 1.0;
    e.body = new btRigidBody(rbInfo);
    world->getWorld()->addRigidBody(e.body);
    entities.push_back(e);
  }
};

RGame::RGame() {
  Input::singleton()->newAxis("ForwardBackward", SDLK_W, SDLK_S);
  Input::singleton()->newAxis("LeftRight", SDLK_A, SDLK_D);

  game = new RGamePrivate();
}

RGame::~RGame() { delete game; }

void RGame::initialize() {
  WorldConstructorSettings& settings = getWorldConstructorSettings();
  settings.physics = true;

  startClient();

  std::scoped_lock lock(world->worldLock);
  world->setTitle("Ray-marcher demo");

  world->stepped.listen([this] {
    glm::vec2 mouseDelta = Input::singleton()->getMouseDelta();

    Input::Axis* fbA = Input::singleton()->getAxis("ForwardBackward");
    Input::Axis* lrA = Input::singleton()->getAxis("LeftRight");

    game->cameraPitch += mouseDelta.x * (M_PI / 180.0);
    game->cameraYaw += mouseDelta.y * (M_PI / 180.0);

    glm::quat yawQuat =
        glm::angleAxis(game->cameraYaw, glm::vec3(1.f, 0.f, 0.f));
    glm::quat pitchQuat =
        glm::angleAxis(game->cameraPitch, glm::vec3(0.f, 1.f, 0.f));
    glm::mat3 vm = glm::toMat3(pitchQuat * yawQuat);
    glm::vec3 forward = glm::vec3(0, 0, 1);
    gfx::Camera& cam = gfxEngine->getCamera();
    cam.setUp(glm::vec3(0, 1, 0));

    float speed = 10.0;

    // FIXME: for some reason i had these flipped but the ray marcher glsl still
    // interprets the 'eye' as target and the 'target' as eye
    cam.setPosition(
        cam.getPosition() +
        (vm * glm::vec3(lrA->value, 0.0, fbA->value) * speed * (1.f / 60.f)));
    cam.setTarget(cam.getPosition() + vm * forward);

    if (Input::singleton()->isKeyDown(SDLK_Q)) {
      glm::vec3 pos = glm::vec3(0.0);
      {
        std::scoped_lock l(world->getPhysicsWorld()->mutex);
        btVector3 start = BulletHelpers::toVector3(cam.getPosition());
        btVector3 end = BulletHelpers::toVector3(vm * (100.f * forward));
        btDynamicsWorld::ClosestRayResultCallback callback(start, end);
        world->getPhysicsWorld()->getWorld()->rayTest(start, end, callback);
        pos = BulletHelpers::fromVector3(callback.m_hitPointWorld) -
              ((vm * forward) * 2.f);
      }
      game->createBall(world->getPhysicsWorld(), 1.f, pos);
    }

    for (int i = 0; i < game->entities.size(); i++) {
      REntity& e = game->entities[i];
      btTransform trans;
      if (e.motionType == REntity::DYNAMIC) {
        e.state->getWorldTransform(trans);
        btVector3 origin = trans.getOrigin();

        if (origin.y() < -100.0) {
          world->getPhysicsWorld()->getWorld()->removeRigidBody(e.body);
          game->entities.erase(game->entities.begin() + i);
          i--;
        }
      }
    }
  });

  gfxEngine->initialized.listen([this] {
    game->rayMarchMaterial =
        gfxEngine->getMaterialCache()->getOrLoad("RayMarch").value_or(nullptr);
  });

  game->createPlane(world->getPhysicsWorld());

  gfxEngine->renderStepped.listen([this] {
    gfxEngine->getDevice()->setDepthState(rdm::gfx::BaseDevice::Disabled);
    gfxEngine->getDevice()->setStencilState(rdm::gfx::BaseDevice::Disabled);
    gfxEngine->getDevice()->setCullState(rdm::gfx::BaseDevice::FrontCCW);
    gfxEngine->getDevice()->setBlendState(rdm::gfx::BaseDevice::DDisabled,
                                          rdm::gfx::BaseDevice::DDisabled);
    gfxEngine->renderFullscreenQuad(
        NULL, game->rayMarchMaterial.get(), [this](gfx::BaseProgram* program) {
          std::scoped_lock lock(game->entitiesMutex);
          for (int i = 0; i < game->entities.size(); i++) {
            REntity& e = game->entities[i];

            if (e.motionType == REntity::DYNAMIC) {
              if (!e.body->isActive()) {
                continue;
              }

              // update entity positioning
              btTransform trans;
              e.state->getWorldTransform(trans);
              btVector3 origin = trans.getOrigin();
              e.position = glm::vec3(origin.x(), origin.y(), origin.z());
            }

            std::string prefix = std::format("entities[{}].", i);
            program->setParameter(
                prefix + "position", gfx::DtVec3,
                gfx::BaseProgram::Parameter{.vec3 = e.position});
            program->setParameter(
                prefix + "type", gfx::DtInt,
                gfx::BaseProgram::Parameter{.integer = e.type});
            program->setParameter(
                prefix + "user0", gfx::DtFloat,
                gfx::BaseProgram::Parameter{.number = e.user0});
          }
          program->setParameter("numEntities", gfx::DtInt,
                                gfx::BaseProgram::Parameter{
                                    .integer = (int)game->entities.size()});
          // program->dbgPrintParameters();
        });
  });
}
};  // namespace rm
