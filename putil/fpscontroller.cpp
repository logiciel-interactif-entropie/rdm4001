#include "fpscontroller.hpp"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>

#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "LinearMath/btVector3.h"
#include "alc.h"
#include "gfx/imgui/imgui.h"
#include "input.hpp"
#include "logging.hpp"
#include "physics.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#ifndef DISABLE_EASY_PROFILER
#include <easy/profiler.h>
#endif

#define FPS_CONTROLLER_FRONT -1, 0, 0

namespace rdm::putil {
FpsControllerSettings::FpsControllerSettings() {
  capsuleHeight = 46.f;
  capsuleRadius = 16.f;
  capsuleMass = 1.f;
  maxSpeed = 260.f;
  maxAccel = 20.f;
  friction = 4.0f;
  stopSpeed = 40.f;
}

FpsController::FpsController(PhysicsWorld* world,
                             FpsControllerSettings settings) {
  this->world = world;
  this->settings = settings;
  btCapsuleShape* shape =
      new btCapsuleShapeZ(settings.capsuleRadius, settings.capsuleHeight);

  btTransform transform = btTransform::getIdentity();
  transform.setOrigin(btVector3(-30.0, 30.0, 200.0));
  motionState = new btDefaultMotionState(transform);
  btVector3 inertia;
  shape->calculateLocalInertia(settings.capsuleMass, inertia);
  btRigidBody::btRigidBodyConstructionInfo rbInfo(settings.capsuleMass,
                                                  motionState, shape, inertia);
  rigidBody.reset(new btRigidBody(rbInfo));
  {
    std::scoped_lock l(world->mutex);
    world->getWorld()->addRigidBody(rigidBody.get());
    stepJob = world->physicsStepping.listen([this] { physicsStep(); });
  }

  rigidBody->setUserPointer(this);
  rigidBody->setUserIndex(PHYSICS_INDEX_PLAYER);

  shape->setUserPointer(this);
  shape->setUserIndex(PHYSICS_INDEX_PLAYER);

  cameraPitch = 0.f;
  cameraYaw = 0.f;
  localPlayer = true;
  enable = true;

  moveVel = glm::vec2(0.0);

  rigidBody->setAngularFactor(btVector3(0, 0, 1));
  rigidBody->setRestitution(0.0);
  rigidBody->setFriction(0.1);
}

FpsController::~FpsController() {
  world->getWorld()->removeRigidBody(rigidBody.get());
  world->physicsStepping.removeListener(stepJob);
}

void FpsController::imguiDebug() {
  btTransform transform;
  motionState->getWorldTransform(transform);
  ImGui::Begin("FPS Controller");
  ImGui::Text("Pos: %f, %f, %f", transform.getOrigin().x(),
              transform.getOrigin().y(), transform.getOrigin().z());
  ImGui::Text("Vel: %f, %f, %f", rigidBody->getLinearVelocity().x(),
              rigidBody->getLinearVelocity().y(),
              rigidBody->getLinearVelocity().z());
  ImGui::Text("Enable: %s", enable ? "true" : "false");
  ImGui::Text("Local Player: %s", localPlayer ? "true" : "false");
  ImGui::End();
}

void FpsController::teleport(glm::vec3 p) {
  std::scoped_lock l(m);

  networkPosition = p;
  btTransform& transform = rigidBody->getWorldTransform();
  transform.setOrigin(BulletHelpers::toVector3(p));
  rigidBody->setWorldTransform(transform);
  rigidBody->setLinearVelocity(btVector3(0.0, 0.0, 0.0));
  rigidBody->setAngularVelocity(btVector3(0.0, 0.0, 0.0));
}

void FpsController::moveGround(btVector3& vel, glm::vec2 wishdir) {
  float speed = vel.length();
  float control = speed < settings.stopSpeed ? settings.stopSpeed : speed;
  float newspeed = speed - PHYSICS_FRAMERATE * settings.friction * control;

  if (Input::singleton()->isKeyDown(' ')) {
    vel += btVector3(0, 0, 50);
    jumping = true;
    return;
  }

  if (newspeed >= 0) {
    newspeed /= speed;

    vel *= newspeed;
  }

  float currentSpeed = vel.dot(btVector3(wishdir.x, wishdir.y, 0.0));
  float addSpeed =
      std::clamp(settings.maxSpeed - currentSpeed, 0.f, settings.maxAccel);
  vel += addSpeed * btVector3(wishdir.x, wishdir.y, 0.0);
}

void FpsController::moveAir(btVector3& vel, glm::vec2 wishdir) {
  float wishSpeed = (btVector3(wishdir.x, wishdir.y, 0.0) + vel).norm();
  if (wishSpeed > 30) wishSpeed = 30;
  float currentSpeed = vel.dot(btVector3(wishdir.x, wishdir.y, 0.0));
  float addSpeed = wishSpeed - currentSpeed;
  if (addSpeed <= 0) return;
  float accelSpeed = settings.maxAccel * wishSpeed * PHYSICS_FRAMERATE;
  if (accelSpeed > addSpeed) accelSpeed = addSpeed;
  vel += accelSpeed * btVector3(wishdir.x, wishdir.y, 0.0);
}

void FpsController::updateCamera(gfx::Camera& camera) {
  btTransform transform;
  motionState->getWorldTransform(transform);

  glm::vec3 origin =
      BulletHelpers::fromVector3(transform.getOrigin()) + glm::vec3(0, 0, 17);

  if (localPlayer) {
    glm::vec2 mouseDelta = Input::singleton()->getMouseDelta();
    cameraPitch -= mouseDelta.x * (M_PI / 180.0);
    cameraYaw -= mouseDelta.y * (M_PI / 180.0);
  }

  glm::quat yawQuat = glm::angleAxis(cameraYaw, glm::vec3(0.f, 1.f, 0.f));
  glm::quat pitchQuat = glm::angleAxis(cameraPitch, glm::vec3(0.f, 0.f, 1.f));
  cameraView = glm::toMat3(pitchQuat * yawQuat);
  moveView = glm::toMat3(pitchQuat);

  glm::vec3 forward = glm::vec3(-1, 0, 0);
  camera.setPosition(origin);
  camera.setTarget(origin + (cameraView * forward));
  camera.setUp(glm::vec3(0, 0, 1));
  camera.setNear(1.0);
  camera.setFar(65535.f);
}

void FpsController::detectGrounded() {
  btTransform& transform = rigidBody->getWorldTransform();
  btVector3 start =
      transform.getOrigin() + btVector3(0, 0, -settings.capsuleHeight / 2.0);
  btVector3 end = start + btVector3(0, 0, -19);
  btDynamicsWorld::ClosestRayResultCallback callback(start, end);
  world->getWorld()->rayTest(start, end, callback);

  grounded = (callback.m_collisionObject != NULL);
  if (grounded) jumping = false;
}

void FpsController::physicsStep() {
#ifndef DISABLE_EASY_PROFILER
  EASY_FUNCTION("FpsController::physicsStep");
#endif

  if (!enable) {
    btTransform& transform = rigidBody->getWorldTransform();
    transform.setOrigin(btVector3(0.f, 0.f, 0.f));
    rigidBody->setLinearVelocity(btVector3(0.f, 0.f, 0.f));
    return;
  }

  std::scoped_lock l(m);

  btTransform& transform = rigidBody->getWorldTransform();
  float dist = glm::distance(networkPosition,
                             BulletHelpers::fromVector3(transform.getOrigin()));
  //  Log::printf(LOG_DEBUG, "%f", dist);

  btVector3 vel = rigidBody->getLinearVelocity();

  if (grounded && (vel.length() > 5.0)) {
    anim = Walk;
  } else if (grounded) {
    anim = Idle;
  } else {
    anim = Fall;
  }

  if (localPlayer) {
#ifndef DISABLE_EASY_PROFILER
    EASY_BLOCK("Local Player Calculation");
#endif

    // Log::printf(LOG_DEBUG, "%.2f, %.2f, %.2f", transform.getOrigin().x(),
    // transform.getOrigin().y(), transform.getOrigin().z());

    Input::Axis* fbA = Input::singleton()->getAxis("ForwardBackward");
    Input::Axis* lrA = Input::singleton()->getAxis("LeftRight");

    front =
        (btVector3(FPS_CONTROLLER_FRONT) * BulletHelpers::toMat3(cameraView))
            .normalize();

    glm::vec2 wishdir =
        glm::vec2(moveView * glm::vec3(-fbA->value, -lrA->value, 0.0));
    accel = wishdir;

    // modelled after Quake 1 movement
    grounded ? moveGround(vel, wishdir) : moveAir(vel, wishdir);

    if (!vel.fuzzyZero()) rigidBody->activate(true);

    rigidBody->setLinearVelocity(vel);
    btTransform& transform = rigidBody->getWorldTransform();

    /*if (dist > 16.f) {
      Log::printf(LOG_DEBUG, "%f", dist);
      Log::printf(LOG_DEBUG, "%f, %f, %f", networkPosition.x, networkPosition.y,
                  networkPosition.z);
      transform.setOrigin(BulletHelpers::toVector3(networkPosition));
      }*/

    transform.setBasis(BulletHelpers::toMat3(moveView));
  }

  detectGrounded();
}

void FpsController::serialize(network::BitStream& stream) {
  btTransform transform;
  getMotionState()->getWorldTransform(transform);
  btVector3FloatData vectorData;
  transform.getOrigin().serialize(vectorData);
  stream.write<btVector3FloatData>(vectorData);

  btMatrix3x3FloatData matrixData;
  transform.getBasis().serialize(matrixData);
  stream.write<btMatrix3x3FloatData>(matrixData);
  rigidBody->getLinearVelocity().serialize(vectorData);
  stream.write<btVector3FloatData>(vectorData);

  stream.write<float>(cameraYaw);
  stream.write<float>(cameraPitch);
}

void FpsController::deserialize(network::BitStream& stream, bool backend) {
  btVector3 origin;
  origin.deSerialize(stream.read<btVector3FloatData>());

  btMatrix3x3 basis;
  basis.deSerialize(stream.read<btMatrix3x3FloatData>());

  btVector3 velocity;
  velocity.deSerialize(stream.read<btVector3FloatData>());

  float cameraYaw = stream.read<float>();
  float cameraPitch = stream.read<float>();

  if (backend) {
    btTransform& bodyTransform = rigidBody->getWorldTransform();
    float dist =
        glm::distance(BulletHelpers::fromVector3(bodyTransform.getOrigin()),
                      BulletHelpers::fromVector3(origin));
    // Log::printf(LOG_DEBUG, "Prediction diff: %f", dist);
    if (dist > velocity.length() * 2.f) {
      Log::printf(LOG_DEBUG, "Resetting position");
      networkPosition = BulletHelpers::fromVector3(bodyTransform.getOrigin());
    } else {
      networkPosition = BulletHelpers::fromVector3(origin);
    }
  } else {
    networkPosition = BulletHelpers::fromVector3(origin);
  }

  if (!localPlayer) {
    btTransform& bodyTransform = rigidBody->getWorldTransform();
    bodyTransform.setOrigin(BulletHelpers::toVector3(networkPosition));
    bodyTransform.setBasis(basis);

    rigidBody->setLinearVelocity(velocity);
    rigidBody->setAngularVelocity(btVector3(0.0, 0.0, 0.0));
    if (enable) rigidBody->activate(true);
    rigidBody->setWorldTransform(bodyTransform);

    glm::quat yawQuat = glm::angleAxis(cameraYaw, glm::vec3(0.f, 1.f, 0.f));
    glm::quat pitchQuat = glm::angleAxis(cameraPitch, glm::vec3(0.f, 0.f, 1.f));
    glm::mat3 frontMat3 = glm::toMat3(pitchQuat * yawQuat);
    glm::vec3 front = frontMat3 * glm::vec3(FPS_CONTROLLER_FRONT);

    this->cameraYaw = cameraYaw;
    this->cameraPitch = cameraPitch;
    this->front = BulletHelpers::toVector3(front);
  } else {
    btTransform& ourTransform = rigidBody->getWorldTransform();
    float dist = glm::distance(
        networkPosition, BulletHelpers::fromVector3(ourTransform.getOrigin()));
    if (dist > velocity.length() * 2.f) ourTransform.setOrigin(origin);
  }
}

};  // namespace rdm::putil
