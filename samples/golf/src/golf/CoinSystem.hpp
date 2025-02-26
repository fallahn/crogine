/*-----------------------------------------------------------------------

Matt Marchant 2025
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#pragma once

#include <crogine/ecs/System.hpp>

#include <unordered_map>

static constexpr float PixelsPerMetre = 200.f;
struct Coin final
{
    static constexpr glm::vec2 InitialVelocity = glm::vec2(1.f);
    static constexpr glm::vec2 Gravity = glm::vec2(0.f, -9.5f * PixelsPerMetre);
    
    glm::vec2 velocity = InitialVelocity;
    std::uint64_t owner = 0;
    bool collisionStart = false;
    bool hadCollisionLastFrame = false;
};

namespace sv
{
    struct SharedData;
}

class CoinSystem final : public cro::System
{
public:
    CoinSystem(cro::MessageBus& mb, sv::SharedData&);

    void handleMessage(const cro::Message&) override;

    void process(float) override;

    void setBucketEnt(cro::Entity e) { m_bucketEnt = e; }

    cro::Entity getBucketEnt() const { return m_bucketEnt; }

    bool canSpawn(std::uint64_t);

private:
    sv::SharedData& m_sharedData;
    cro::Entity m_bucketEnt;

    std::unordered_map<std::uint64_t, std::int32_t> m_coinCount;
    void onEntityAdded(cro::Entity) override;
    void onEntityRemoved(cro::Entity) override;
};