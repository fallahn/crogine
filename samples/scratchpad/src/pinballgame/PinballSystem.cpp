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
    const auto createBox =
        [this](glm::vec2 pos, glm::vec2 size, float rotation = 0.f)
        {
            b2BodyDef wallBodyDef = b2DefaultBodyDef();
            wallBodyDef.position = { pos.x, pos.y };
            wallBodyDef.rotation = b2MakeRot(rotation);

            b2BodyId wallId = b2CreateBody(m_physicsWorld, &wallBodyDef);
            b2Polygon wallBox = b2MakeBox(size.x, size.y);

            b2ShapeDef wallShapeDef = b2DefaultShapeDef();
            wallShapeDef.material.restitution = 0.4f;
            wallShapeDef.material.friction = 0.01f;
            b2CreatePolygonShape(wallId, &wallShapeDef, &wallBox);
        };


    //edge walls
    for (const auto& [pos, size] : WallPosSize)
    {
        createBox(pos, size);
    }
    //channel wall
    createBox(ChannelWallPos, ChannelWallSize);


    //arc radius at top of the table
    const auto createChain =
        [this](const std::vector<b2Vec2>& points)
        {
            b2ChainDef arc = b2DefaultChainDef();
            arc.count = static_cast<std::int32_t>(points.size());
            arc.points = points.data();
            //hmm material is const here?? how to set restitution etc?
            b2BodyDef arcDef = b2DefaultBodyDef();
            b2BodyId arcBody = b2CreateBody(m_physicsWorld, &arcDef);
            b2CreateChain(arcBody, &arc);
        };


    const auto points = getArc();
    std::vector<b2Vec2> b2Points;
    for (auto p : points)
    {
        b2Points.push_back(b2Vector(p));
    }
    createChain(b2Points);

    const auto createPoly = 
        [this](const std::vector<b2Vec2>& points, glm::vec2 pos, float radius = 0.f)
        {
            b2BodyDef polyDef = b2DefaultBodyDef();
            polyDef.position = b2Vector(pos);

            b2BodyId polyBody = b2CreateBody(m_physicsWorld, &polyDef);

            b2Hull hull = b2ComputeHull(points.data(), static_cast<std::int32_t>(points.size()));
            b2Polygon poly = b2MakePolygon(&hull, radius);
            b2ShapeDef polyShape = b2DefaultShapeDef();
            polyShape.material.restitution = 0.4f;

            b2CreatePolygonShape(polyBody, &polyShape, &poly);
        };

    //ramp on left
    b2Points.clear();
    for (auto p : RampPoints)
    {
        b2Points.push_back(b2Vector(p));
    }
    createPoly(b2Points, { 0.f, 0.f });


    //bumpers - TODO these need to trigger an impulse on the ball
    b2Points.clear();
    for (const auto p : BumperPoints)
    {
        b2Points.push_back(b2Vector(p));
    }
    createPoly(b2Points, { 0.f, 0.f }, 0.01f);

    std::reverse(b2Points.begin(), b2Points.end());
    for (auto& p : b2Points)
    {
        p.x *= -1.f;
        p.x -= ((TableSize.x / 2.f) - PlayAreaCentre) * 2.f;
    }
    createPoly(b2Points, { 0.f, 0.f }, 0.01f);



    //chute onto flippers
    createBox(FlipperChutePosLarge, FlipperChuteSizeLarge / 2.f, FlipperChuteLargeRotation);
    auto flippedPos = FlipperChutePosLarge;
    flippedPos.x *= -1.f;
    flippedPos.x -= ((TableSize.x / 2.f) - PlayAreaCentre) * 2.f;
    createBox(flippedPos, FlipperChuteSizeLarge / 2.f, -FlipperChuteLargeRotation);

    createBox(FlipperChutePosSmall, FlipperChuteSizeSmall / 2.f);
    flippedPos = FlipperChutePosSmall;
    flippedPos.x *= -1.f;
    flippedPos.x -= ((TableSize.x / 2.f) - PlayAreaCentre) * 2.f;
    createBox(flippedPos, FlipperChuteSizeSmall / 2.f);



    //funnel
    std::vector<b2Vec2> funnelPoints =
    {
        b2Vector(0.f, 0.f),
        b2Vector(FunnelSize.x, 0.f),
        b2Vector(0.f, FunnelSize.y),
    };
    createPoly(funnelPoints, FunnelLeftPos);

    funnelPoints =
    {
        b2Vector(0.f, 0.f),
        b2Vector(FunnelSize.x , 0.f),
        b2Vector(FunnelSize.x , FunnelSize.y),
    };
    createPoly(funnelPoints, FunnelRightPos);
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


    b2Body_ApplyLinearImpulse(pinball.bodyId, { 0.f, 0.015f }, { 0.f, 0.f }, true);
}

void PinballSystem::onEntityRemoved(cro::Entity e)
{
    auto& pinball = e.getComponent<Pinball>();
    b2DestroyShape(pinball.shapeId, false);
    b2DestroyBody(pinball.bodyId);
}