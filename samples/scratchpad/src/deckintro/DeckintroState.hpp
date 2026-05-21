//Auto-generated header file for Scratchpad Stub 25/11/2025, 09:58:27

#pragma once

#include "../StateIDs.hpp"

#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>

#include <memory>
#include <array>
#include <vector>

struct BallMotion final : public btMotionState
{
    cro::Entity parent;
    void getWorldTransform(btTransform& worldTrans) const override;
    void setWorldTransform(const btTransform& worldTrans) override;
};

class DeckIntroState final : public cro::State, public cro::GuiClient
{
public:
    DeckIntroState(cro::StateStack&, cro::State::Context);

    ~DeckIntroState();

    cro::StateID getStateID() const override { return States::ScratchPad::DeckIntro; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    cro::EnvironmentMap m_envMap;

    std::unique_ptr<btCollisionConfiguration> m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_collisionDispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphaseInterface;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_constraintSolver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_collisionWorld;

    std::unique_ptr<btSphereShape> m_ballShape;

    struct PlaneShape final
    {
        enum
        {
            Ground, Left,
            Right, Rear,
            Front, Count
        };
    };
    std::array<std::unique_ptr<btStaticPlaneShape>, PlaneShape::Count> m_planeShapes;
    std::array<std::unique_ptr<btRigidBody>, PlaneShape::Count> m_planeBodies;

    std::vector<std::unique_ptr<btRigidBody>> m_ballBodies;
    cro::ModelDefinition m_ballDefinition;

    std::unique_ptr<btSphereShape> m_logoShape;
    std::unique_ptr<btRigidBody> m_logoBody;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();


    void spawnBall(glm::vec3 = glm::vec3(0.f));
    void spawnLogo();
};