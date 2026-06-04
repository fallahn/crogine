#include "PinballSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>

namespace
{

}

PinballSystem::PinballSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(PinballSystem)),
    m_physicsWorld  ()
{
    requireComponent<cro::Transform>();
    requireComponent<Pinball>();

    auto worldDef = b2DefaultWorldDef();
    worldDef.gravity = { 0.f, Gravity.y };

    m_physicsWorld = b2CreateWorld(&worldDef);
}

PinballSystem::~PinballSystem()
{
    b2DestroyWorld(m_physicsWorld);
}

//public
void PinballSystem::process(float dt)
{
    b2World_Step(m_physicsWorld, dt, 4);
    for (auto entity : getEntities())
    {
        const auto& pinball = entity.getComponent<Pinball>();

        const b2Vec2 position = b2Body_GetPosition(pinball.bodyId);
        const b2Rot rotation = b2Body_GetRotation(pinball.bodyId);

        auto& tx = entity.getComponent<cro::Transform>();
        tx.setPosition({ position.x, position.y });
        tx.setRotation(b2Rot_GetAngle(rotation));
    }
}

void PinballSystem::createTable()
{
    for (const auto& [pos, size] : WallPosSize)
    {
        b2BodyDef wallBodyDef = b2DefaultBodyDef();
        wallBodyDef.position = { pos.x, pos.y };

        b2BodyId wallId = b2CreateBody(m_physicsWorld, &wallBodyDef);
        b2Polygon wallBox = b2MakeBox(size.x, size.y);

        b2ShapeDef wallShapeDef = b2DefaultShapeDef();
        wallShapeDef.material.restitution = 0.4f;
        b2CreatePolygonShape(wallId, &wallShapeDef, &wallBox);
    }

    //arc radius at top of the table
    const auto points = getArc();
    std::vector<b2Vec2> b2Points;
    for (auto p : points)
    {
        auto& bp = b2Points.emplace_back();
        bp.x = p.x;
        bp.y = p.y;
    }

    b2ChainDef arc = b2DefaultChainDef();
    arc.count = static_cast<std::int32_t>(b2Points.size());
    arc.points = b2Points.data();

    b2BodyDef arcDef = b2DefaultBodyDef();
    b2BodyId arcBody = b2CreateBody(m_physicsWorld, &arcDef);
    b2CreateChain(arcBody, &arc);
}

//private
void PinballSystem::onEntityAdded(cro::Entity e)
{
    const auto& tx = e.getComponent<cro::Transform>();
    auto& pinball = e.getComponent<Pinball>();

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = { tx.getPosition().x, tx.getPosition().y };
    pinball.bodyId = b2CreateBody(m_physicsWorld, &bodyDef);

    b2Circle shape = {};
    shape.center = { 0.f, 0.f };
    shape.radius = pinball.radius;

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 7.762f; //hmm this is actually m^3
    shapeDef.material.friction = 0.3f;
    shapeDef.material.restitution = 0.6f;

    //TODO the world will auto-clean these up
    //however we may end up with loads of redundant
    //shapes if balls are frequently removed
    pinball.shapeId = b2CreateCircleShape(pinball.bodyId, &shapeDef, &shape);


    b2Body_ApplyLinearImpulse(pinball.bodyId, { 0.f, 0.02f }, { 0.f, 0.f }, true);
}

void PinballSystem::onEntityRemoved(cro::Entity e)
{
    auto& pinball = e.getComponent<Pinball>();
    b2DestroyShape(pinball.shapeId, false);
    b2DestroyBody(pinball.bodyId);
}