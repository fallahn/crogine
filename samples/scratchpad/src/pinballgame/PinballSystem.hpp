#pragma once

#include "PinballConsts.hpp"

#include <box2d/box2d.h>

#include <crogine/ecs/System.hpp>

//units are MKS
struct Pinball final
{
    float mass = 0.08f;
    float radius = PinballRadius;
    float resitituion = 0.6f;
    b2BodyId bodyId = {};
    b2ShapeId shapeId = {};
};

class PinballSystem final : public cro::System
{
public:
    explicit PinballSystem(cro::MessageBus&);

    ~PinballSystem();

    PinballSystem(const PinballSystem&) = delete;
    PinballSystem(PinballSystem&&) = delete;

    const PinballSystem& operator = (const PinballSystem&) = delete;
    PinballSystem& operator = (PinballSystem&&) = delete;

    void process(float) override;

    void createTable(); //TODO could pass some external params etc

    b2JointId flipperLeft = {};
    b2JointId flipperRight = {};
    b2BodyId flipperLeftBody = {};
    b2BodyId flipperRightBody = {};

private:
    b2WorldId m_physicsWorld;

    void onEntityAdded(cro::Entity) override;
    void onEntityRemoved(cro::Entity) override;
};
