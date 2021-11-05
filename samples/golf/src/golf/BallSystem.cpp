/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#include "BallSystem.hpp"
#include "Terrain.hpp"
#include "HoleData.hpp"
#include "GameConsts.hpp"
#include "../ErrorCheck.hpp"
#include "server/ServerMessages.hpp"

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/MeshBuilder.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>

#include <crogine/detail/ModelBinary.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    constexpr glm::vec3 Gravity(0.f, -9.8f, 0.f);

    static constexpr float MinBallDistance = HoleRadius * HoleRadius;
    static constexpr float BallTurnDelay = 2.5f; //how long to delay before stating turn ended
}

BallSystem::BallSystem(cro::MessageBus& mb)
    : cro::System           (mb, typeid(BallSystem)),
    m_windDirTime           (cro::seconds(0.f)),
    m_windStrengthTime      (cro::seconds(1.f)),
    m_windDirection         (-1.f, 0.f, 0.f),
    m_windDirSrc            (m_windDirection),
    m_windDirTarget         (1.f, 0.f, 0.f),
    m_windStrength          (0.f),
    m_windStrengthSrc       (m_windStrength),
    m_windStrengthTarget    (0.1f),
    m_windInterpTime        (1.f),
    m_currentWindInterpTime (0.f),
    m_holeData              (nullptr)
{
    requireComponent<cro::Transform>();
    requireComponent<Ball>();

    m_windDirTarget.x = static_cast<float>(cro::Util::Random::value(-10, 10)) / 10.f;
    m_windDirTarget.z = static_cast<float>(cro::Util::Random::value(-10, 10)) / 10.f;

    m_windDirTarget = glm::normalize(m_windDirTarget);

    m_windStrengthTarget = static_cast<float>(cro::Util::Random::value(1, 10)) / 10.f;

    updateWind();

    initCollisionWorld();
}

BallSystem::~BallSystem()
{
    clearCollisionObjects();
}

//public
void BallSystem::process(float dt)
{
    //interpolate current strength/direction
    m_currentWindInterpTime = std::min(m_windInterpTime, m_currentWindInterpTime + dt);
    float interp = m_currentWindInterpTime / m_windInterpTime;
    m_windDirection = interpolate(m_windDirSrc, m_windDirTarget, interp);
    m_windStrength = interpolate(m_windStrengthSrc, m_windStrengthTarget, interp);

    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& ball = entity.getComponent<Ball>();
        switch (ball.state)
        {
        default: break;
        case Ball::State::Idle:
            ball.hadAir = false;
            break;
        case Ball::State::Flight:
        {
            ball.hadAir = false;
            ball.delay -= dt;
            if (ball.delay < 0)
            {
                //add gravity
                ball.velocity += Gravity * dt;

                //add wind
                ball.velocity += m_windDirection * m_windStrength * dt;

                //TODO add air friction?

                //move by velocity
                auto& tx = entity.getComponent<cro::Transform>();
                tx.move(ball.velocity * dt);

                //test collision
                doCollision(entity);
            }
        }
        break;
        case Ball::State::Putt:
            
            ball.delay -= dt;
            if (ball.delay < 0)
            {
                auto& tx = entity.getComponent<cro::Transform>();
                auto position = tx.getPosition();

                auto [terrain, normal, penetration] = getTerrain(position);

                //test distance to pin
                auto len2 = glm::length2(glm::vec2(position.x, position.z) - glm::vec2(m_holeData->pin.x, m_holeData->pin.z));
                if (len2 < MinBallDistance)
                {
                    //over hole or in the air
                    static constexpr float MinFallVelocity = 2.1f;
                    float gravityAmount = 1.f - std::min(1.f, glm::length2(ball.velocity) / MinFallVelocity);

                    //this is some fudgy non-physics.
                    //if the ball falls low enough when
                    //over the hole we'll put it in.
                    ball.velocity += (gravityAmount * Gravity) * dt;

                    ball.hadAir = true;
                }
                else //we're on the green so roll
                {
                    //if the ball has registered some air but is not over
                    //the hole reset the depth and slow it down as if it
                    //bumped the far edge
                    if (ball.hadAir)
                    {
                        //these are all just a wild stab
                        //destined for some tweaking - basically puts the ball back along its vector
                        //towards the hole while maintaining gravity. As if it bounced off the inside of the hole
                        if (penetration > (Ball::Radius * 0.5f))
                        {
                            ball.velocity *= -1.f;
                            ball.velocity.y *= -1.f;
                        }
                        else
                        {
                            //lets the ball continue travelling, ie overshoot
                            ball.velocity *= 0.7f;
                            ball.velocity.y = glm::length2(ball.velocity) * 0.4f;

                            position.y += penetration;
                            tx.setPosition(position);
                        }
                    }
                    else
                    {
                        if(penetration < 0) //above ground
                        {
                            //we had a bump so add gravity
                            ball.velocity += Gravity * dt;
                        }
                        else if (penetration > 0)
                        {
                            //we've sunk into the ground so correct
                            ball.velocity.y = 0.f;

                            position.y += penetration;
                            tx.setPosition(position);
                        }
                    }
                    ball.hadAir = false;

                    //move by slope from surface normal
                    auto velLength = glm::length(ball.velocity);
                    glm::vec3 slope = glm::vec3(normal.x, 0.f, normal.z) * 1.5f * smoothstep(0.5f, 4.5f, velLength);
                    ball.velocity += slope;

                    //add wind - adding less wind the more the ball travels in the
                    //wind direction means we don't get blown forever
                    float windAmount = 1.f - glm::dot(m_windDirection, ball.velocity / velLength);
                    ball.velocity += m_windDirection * m_windStrength * 0.06f * windAmount * dt;

                    //add friction
                    ball.velocity *= 0.985f;
                }


                //move by velocity
                tx.move(ball.velocity * dt);
                ball.terrain = terrain; //TODO this will be wrong if the above movement changed the terrain

                //if we've slowed down or fallen more than the
                //ball's diameter (radius??) stop the ball
                if (glm::length2(ball.velocity) < 0.01f
                    || (penetration > (Ball::Radius * 2.5f)))
                {
                    ball.velocity = glm::vec3(0.f);
                    
                    if (ball.terrain == TerrainID::Water
                        || ball.terrain == TerrainID::Scrub)
                    {
                        ball.state = Ball::State::Reset;
                    }
                    else
                    {
                        ball.state = Ball::State::Paused;
                    }

                    ball.delay = BallTurnDelay;

                    auto* msg = postMessage<BallEvent>(sv::MessageID::BallMessage);
                    msg->type = BallEvent::Landed;
                    msg->terrain = penetration > Ball::Radius ? TerrainID::Hole : ball.terrain;
                    msg->position = position;
                }
            }
            break;
        case Ball::State::Reset:
        {
            ball.delay -= dt;
            if (ball.delay < 0)
            {
                //move towards hole util we find non-water
                auto& tx = entity.getComponent<cro::Transform>();

                std::uint8_t terrain = TerrainID::Water;
                auto ballPos = tx.getPosition();
                ballPos.y = 0.f;

                auto dir = m_holeData->pin - ballPos;
                auto length = glm::length(dir);
                dir /= length;
                std::int32_t maxDist = static_cast<std::int32_t>(length - 10.f);

                for (auto i = 0; i < maxDist; ++i) //max 100m
                {
                    ballPos += dir;
                    auto res = getTerrain(ballPos);
                    terrain = res.terrain;

                    if (terrain != TerrainID::Water
                        && terrain != TerrainID::Scrub)
                    {
                        ballPos.y = -res.penetration;
                        tx.setPosition(ballPos);
                        break;
                    }
                }

                //if for some reason we never got out the water, put the ball back at the start
                if (terrain == TerrainID::Water
                    || terrain == TerrainID::Scrub)
                {
                    terrain = TerrainID::Fairway;
                    tx.setPosition(m_holeData->tee);
                }

                //raise message to say player should be penalised
                auto* msg = postMessage<BallEvent>(sv::MessageID::BallMessage);
                msg->type = BallEvent::Foul;
                msg->terrain = terrain;
                msg->position = tx.getPosition();

                //set ball to reset / correct terrain
                ball.delay = 0.5f;
                ball.terrain = terrain;
                ball.state = Ball::State::Paused;
            }
        }
            break;
        case Ball::State::Paused:
        {
            ball.delay -= dt;
            if (ball.delay < 0)
            {
                //send message to report status
                auto* msg = postMessage<BallEvent>(sv::MessageID::BallMessage);
                msg->terrain = ball.terrain;
                msg->position = entity.getComponent<cro::Transform>().getPosition();

                if (getTerrain(msg->position).penetration > Ball::Radius)
                {
                    //we're in the hole
                    msg->type = BallEvent::Holed;
                }
                else
                {
                    msg->type = BallEvent::TurnEnded;
                }


                ball.state = Ball::State::Idle;
                updateWind(); //is a bit less random but at least stops the wind
                //changing direction mid-stroke which is just annoying.
            }
        }
            break;
        }
    }
}

glm::vec3 BallSystem::getWindDirection() const
{
    //the Y value is unused so we pack the strength in here
    //(it's only for vis on the client anyhoo)
    return { m_windDirection.x, m_windStrength, m_windDirection.z };
}

bool BallSystem::setHoleData(const HoleData& holeData)
{
    m_holeData = &holeData;

    return updateCollisionMesh(holeData.modelPath);
}

//private
void BallSystem::doCollision(cro::Entity entity)
{
    //check height
    auto& tx = entity.getComponent<cro::Transform>();
    auto pos = tx.getPosition();

    //if (pos.y > 10.f)
    //{
    //    //skip test we're in the air (breaks if someone makes a tall mesh though...)
    //    return;
    //}

    const auto resetBall = [&](Ball& ball, Ball::State state, std::uint8_t terrain)
    {
        ball.velocity = glm::vec3(0.f);
        ball.state = state;
        ball.delay = BallTurnDelay;
        ball.terrain = terrain;

        auto* msg = postMessage<BallEvent>(sv::MessageID::BallMessage);
        msg->type = BallEvent::Landed;
        msg->terrain = ball.terrain;
        msg->position = tx.getPosition();
    };

    auto [terrain, normal, penetration] = getTerrain(pos);

    if (penetration > 0)
    {
        //pos.y = 0.f;
        pos.y += /*normal **/ penetration;
        //TODO reduce the velocity based on interpolation (see scratchpad)
        //TODO this should move back along ball velocity, not the normal.
        tx.setPosition(pos);

        auto& ball = entity.getComponent<Ball>();

        //apply dampening based on terrain (or splash)
        switch (terrain)
        {
        default: break;
        case TerrainID::Water:
        case TerrainID::Scrub:
            pos.y = -Ball::Radius * 2.f;
            tx.setPosition(pos);
            [[fallthrough]];
        case TerrainID::Bunker:
            ball.velocity = glm::vec3(0.f);
            break;
        case TerrainID::Fairway:
            ball.velocity *= 0.33f;
            ball.velocity = glm::reflect(ball.velocity, normal);
            break;
        case TerrainID::Green:
            ball.velocity *= 0.28f;

            //if low bounce start rolling
            if (ball.velocity.y > -0.05f)
            {
                float momentum = 1.f - glm::dot(-cro::Transform::Y_AXIS, glm::normalize(ball.velocity));
                static constexpr float MaxMomentum = 20.f;
                momentum *= MaxMomentum;


                auto len = glm::length(ball.velocity);
                ball.velocity.y = 0.f;
                ball.velocity = glm::normalize(ball.velocity) * len * momentum; //fake physics to simulate momentum
                ball.state = Ball::State::Putt;
                ball.delay = 0.f;

                return;
            }
            else //bounce
            {
                ball.velocity = glm::reflect(ball.velocity, normal);
            }
            break;
        case TerrainID::Rough:
            ball.velocity *= 0.23f;
            ball.velocity = glm::reflect(ball.velocity, normal);
            break;
        }

        //stop the ball if velocity low enough
        if (glm::length2(ball.velocity) < 0.01f)
        {
            if (terrain == TerrainID::Water
                || terrain == TerrainID::Scrub)
            {
                resetBall(ball, Ball::State::Reset, terrain);
            }
            else
            {
                resetBall(ball, Ball::State::Paused, terrain);
            }
        }
    }
    else if (pos.y < WaterLevel)
    {
        //must have missed all geometry and so are in scrub or water
        auto& ball = entity.getComponent<Ball>();
        resetBall(ball, Ball::State::Reset, TerrainID::Scrub);
    }
}

void BallSystem::updateWind()
{
    auto resetInterp =
        [&]()
    {
        m_windDirSrc = m_windDirection;
        m_windStrengthSrc = m_windStrength;

        m_currentWindInterpTime = 0.f;
        m_windInterpTime = static_cast<float>(cro::Util::Random::value(50, 75)) / 10.f;
    };

    //update wind direction
    if (m_windDirClock.elapsed() > m_windDirTime)
    {
        m_windDirClock.restart();
        m_windDirTime = cro::seconds(static_cast<float>(cro::Util::Random::value(100, 220)) / 10.f);

        //create new direction
        m_windDirTarget.x = static_cast<float>(cro::Util::Random::value(-10, 10)) / 10.f;
        m_windDirTarget.z = static_cast<float>(cro::Util::Random::value(-10, 10)) / 10.f;

        m_windDirTarget = glm::normalize(m_windDirTarget);

        resetInterp();
    }

    //update wind strength
    if (m_windStrengthClock.elapsed() > m_windStrengthTime)
    {
        m_windStrengthClock.restart();
        m_windStrengthTime = cro::seconds(static_cast<float>(cro::Util::Random::value(80, 180)) / 10.f);

        m_windStrengthTarget = static_cast<float>(cro::Util::Random::value(1, 10)) / 10.f;

        resetInterp();
    }
}

BallSystem::TerrainResult BallSystem::getTerrain(glm::vec3 pos) const
{
    TerrainResult retVal;

    //casts a vertical ray 5m above/below the ball
    static const btVector3 RayLength = { 0.f,  -20.f, 0.f };
    btVector3 rayStart = { pos.x, pos.y, pos.z };
    rayStart -= (RayLength / 2.f);
    auto rayEnd = rayStart + RayLength;

    //RayResultCallback res(rayStart, rayEnd);
    btCollisionWorld::ClosestRayResultCallback res(rayStart, rayEnd);
    m_collisionWorld->rayTest(rayStart, rayEnd, res);
    if (res.hasHit())
    {
        retVal.terrain = static_cast<std::uint8_t>(res.m_collisionObject->getUserIndex());
        retVal.normal = { res.m_hitNormalWorld.x(), res.m_hitNormalWorld.y(), res.m_hitNormalWorld.z() };
        retVal.penetration = res.m_hitPointWorld.y() - pos.y;
    }

    return retVal;
}

void BallSystem::initCollisionWorld()
{
    m_collisionCfg = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionCfg.get());

    m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
    m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionCfg.get());
}

void BallSystem::clearCollisionObjects()
{
    for (auto& obj : m_groundObjects)
    {
        m_collisionWorld->removeCollisionObject(obj.get());
    }

    m_groundObjects.clear();
    m_groundShapes.clear();
    m_groundVertices.clear();

    m_vertexData.clear();
    m_indexData.clear();
}

bool BallSystem::updateCollisionMesh(const std::string& modelPath)
{
    clearCollisionObjects();

    cro::Mesh::Data meshData;

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(modelPath.c_str(), "rb");
    if (file.file)
    {
        cro::Detail::ModelBinary::Header header;
        auto len = SDL_RWseek(file.file, 0, RW_SEEK_END);
        if (len < sizeof(header))
        {
            LogE << "Unable to open " << modelPath << ": invalid file size" << std::endl;
            return false;
        }

        SDL_RWseek(file.file, 0, RW_SEEK_SET);
        SDL_RWread(file.file, &header, sizeof(header), 1);

        if (header.magic != cro::Detail::ModelBinary::MAGIC)
        {
            LogE << "Invalid header found" << std::endl;
            return {};
        }

        if (header.meshOffset)
        {
            cro::Detail::ModelBinary::MeshHeader meshHeader;
            SDL_RWread(file.file, &meshHeader, sizeof(meshHeader), 1);

            if ((meshHeader.flags & cro::VertexProperty::Position) == 0)
            {
                LogE << "No position data in mesh" << std::endl;
                return false;
            }

            std::vector<float> tempVerts;
            std::vector<std::uint32_t> sizes(meshHeader.indexArrayCount);
            m_indexData.resize(meshHeader.indexArrayCount);

            SDL_RWread(file.file, sizes.data(), meshHeader.indexArrayCount * sizeof(std::uint32_t), 1);

            std::uint32_t vertStride = 0;
            for (auto i = 0u; i < cro::Mesh::Attribute::Total; ++i)
            {
                if (meshHeader.flags & (1 << i))
                {
                    switch (i)
                    {
                    default:
                    case cro::Mesh::Attribute::Bitangent:
                        break;
                    case cro::Mesh::Attribute::Position:
                        vertStride += 3;
                        meshData.attributes[i] = 3;
                        break;
                    case cro::Mesh::Attribute::Colour:
                        vertStride += 4;
                        meshData.attributes[i] = 1; //we'll only store the red channel for reading terrain
                        break;
                    case cro::Mesh::Attribute::Normal:
                        vertStride += 3;
                        break;
                    case cro::Mesh::Attribute::UV0:
                    case cro::Mesh::Attribute::UV1:
                        vertStride += 2;
                        break;
                    case cro::Mesh::Attribute::Tangent:
                    case cro::Mesh::Attribute::BlendIndices:
                    case cro::Mesh::Attribute::BlendWeights:
                        vertStride += 4;
                        break;
                    }
                }
            }

            auto pos = SDL_RWtell(file.file);
            auto vertSize = meshHeader.indexArrayOffset - pos;
            tempVerts.resize(vertSize / sizeof(float));
            SDL_RWread(file.file, tempVerts.data(), vertSize, 1);
            CRO_ASSERT(tempVerts.size() % vertStride == 0, "");

            for (auto i = 0u; i < meshHeader.indexArrayCount; ++i)
            {
                m_indexData[i].resize(sizes[i]);
                SDL_RWread(file.file, m_indexData[i].data(), sizes[i] * sizeof(std::uint32_t), 1);
            }

            //process vertex data
            for (auto i = 0u; i < tempVerts.size(); i += vertStride)
            {
                std::uint32_t offset = 0;
                for (auto j = 0u; j < cro::Mesh::Attribute::Total; ++j)
                {
                    if (meshHeader.flags & (1 << j))
                    {
                        switch (j)
                        {
                        default: break;
                        case cro::Mesh::Attribute::Position:
                            m_vertexData.push_back(tempVerts[i + offset]);
                            m_vertexData.push_back(tempVerts[i + offset + 1]);
                            m_vertexData.push_back(tempVerts[i + offset + 2]);

                            offset += 3;
                            meshData.attributeFlags |= cro::VertexProperty::Position;
                            break;
                        case cro::Mesh::Attribute::Colour:
                            m_vertexData.push_back(tempVerts[i + offset]);
                            /*m_vertexData.push_back(tempVerts[i + offset + 1]);
                            m_vertexData.push_back(tempVerts[i + offset + 2]);
                            m_vertexData.push_back(tempVerts[i + offset + 3]);*/
                            offset += 1;
                            meshData.attributeFlags |= cro::VertexProperty::Colour;
                            break;
                        case cro::Mesh::Attribute::Normal:
                        case cro::Mesh::Attribute::Tangent:
                        case cro::Mesh::Attribute::Bitangent:
                        case cro::Mesh::Attribute::UV0:
                        case cro::Mesh::Attribute::UV1:
                        case cro::Mesh::Attribute::BlendIndices:
                        case cro::Mesh::Attribute::BlendWeights:
                            break;
                        }
                    }
                }
            }

            meshData.primitiveType = GL_TRIANGLES;

            for (const auto& a : meshData.attributes)
            {
                meshData.vertexSize += a;
            }
            meshData.vertexSize *= sizeof(float);
            meshData.vertexCount = m_vertexData.size() / (meshData.vertexSize / sizeof(float));

            meshData.submeshCount = meshHeader.indexArrayCount;
            for (auto i = 0u; i < meshData.submeshCount; ++i)
            {
                meshData.indexData[i].format = GL_UNSIGNED_INT;
                meshData.indexData[i].primitiveType = meshData.primitiveType;
                meshData.indexData[i].indexCount = static_cast<std::uint32_t>(m_indexData[i].size());
            }
        }
    }
    else
    {
        LogE << modelPath << ": " << SDL_GetError() << std::endl;
        return false;
    }


    if ((meshData.attributeFlags & cro::VertexProperty::Colour) == 0)
    {
        LogE << "No colour property found in collision mesh" << std::endl;
        return false;
    }



    std::int32_t colourOffset = 0;
    for (auto i = 0; i < cro::Mesh::Attribute::Colour; ++i)
    {
        colourOffset += meshData.attributes[i];
    }

    //we have to create a specific object for each sub mesh
    //to be able to tag it with a different terrain...
    for (auto i = 0u; i < m_indexData.size(); ++i)
    {
        btIndexedMesh groundMesh;
        groundMesh.m_vertexBase = reinterpret_cast<std::uint8_t*>(m_vertexData.data());
        groundMesh.m_numVertices = meshData.vertexCount;
        groundMesh.m_vertexStride = meshData.vertexSize;

        groundMesh.m_numTriangles = meshData.indexData[i].indexCount / 3;
        groundMesh.m_triangleIndexBase = reinterpret_cast<std::uint8_t*>(m_indexData[i].data());
        groundMesh.m_triangleIndexStride = 3 * sizeof(std::uint32_t);


        float terrain = std::min(1.f, std::max(0.f, m_vertexData[(m_indexData[i][0] * (meshData.vertexSize / sizeof(float))) + colourOffset])) * 255.f;
        terrain = std::floor(terrain / 10.f);
        if (terrain > TerrainID::Hole)
        {
            terrain = TerrainID::Fairway;
        }

        m_groundVertices.emplace_back(std::make_unique<btTriangleIndexVertexArray>())->addIndexedMesh(groundMesh);
        m_groundShapes.emplace_back(std::make_unique<btBvhTriangleMeshShape>(m_groundVertices.back().get(), false));
        m_groundObjects.emplace_back(std::make_unique<btPairCachingGhostObject>())->setCollisionShape(m_groundShapes.back().get());
        m_groundObjects.back()->setUserIndex(static_cast<std::int32_t>(terrain)); // set the terrain type
        m_collisionWorld->addCollisionObject(m_groundObjects.back().get());
    }

    return true;
}

//custom callback to return proper face normal (I wish we could cache these...)
RayResultCallback::RayResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld)
    : ClosestRayResultCallback(rayFromWorld, rayToWorld)
{
}

btScalar RayResultCallback::addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
{
    //caller already does the filter on the m_closestHitFraction
    btAssert(rayResult.m_hitFraction <= m_closestHitFraction);

    m_closestHitFraction = rayResult.m_hitFraction;
    m_collisionObject = rayResult.m_collisionObject;
    m_hitNormalWorld = getFaceNormal(rayResult);
    m_hitPointWorld.setInterpolate3(m_rayFromWorld, m_rayToWorld, rayResult.m_hitFraction);
    return rayResult.m_hitFraction;
}

btVector3 RayResultCallback::getFaceNormal(const btCollisionWorld::LocalRayResult& rayResult) const
{
    /*
    Respect to https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=12826
    */

    const unsigned char* vertices = nullptr;
    int numVertices = 0;
    int vertexStride = 0;
    PHY_ScalarType verticesType;

    const unsigned char* indices = nullptr;
    int indicesStride = 0;
    int numFaces = 0;
    PHY_ScalarType indicesType;

    auto vertex = [&](int vertexIndex)
    {
        const auto* data = reinterpret_cast<const btScalar*>(vertices + vertexIndex * vertexStride);
        return btVector3(*data, *(data + 1), *(data + 2));
    };

    const auto* triangleShape = static_cast<const btBvhTriangleMeshShape*>(rayResult.m_collisionObject->getCollisionShape());
    const auto* triangleMesh = static_cast<const btTriangleIndexVertexArray*>(triangleShape->getMeshInterface());

    triangleMesh->getLockedReadOnlyVertexIndexBase(
        &vertices, numVertices, verticesType, vertexStride, &indices, indicesStride, numFaces, indicesType, rayResult.m_localShapeInfo->m_shapePart
    );

    const auto* index = reinterpret_cast<const int*>(indices + rayResult.m_localShapeInfo->m_triangleIndex * indicesStride);
    btVector3 va = vertex(*index);
    btVector3 vb = vertex(*(index + 1));
    btVector3 vc = vertex(*(index + 2));
    btVector3 normal = (vb - va).cross(vc - va).normalized();

    triangleMesh->unLockReadOnlyVertexBase(rayResult.m_localShapeInfo->m_shapePart);

    return normal;
}