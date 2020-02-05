/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#ifndef DH_PLAYER_WEAPONS_HPP_
#define DH_PLAYER_WEAPONS_HPP_

#include <crogine/ecs/System.hpp>

#include <crogine/detail/glm/vec3.hpp>

struct PlayerWeapon final
{
    glm::vec3 velocity = glm::vec3(0.f);
    enum
    {
        Bullet,
        Laser,
        Grenade
    }type;
    float damage = 0.f;
};

class PlayerWeaponSystem final : public cro::System
{
public:
    PlayerWeaponSystem(cro::MessageBus&);

    void handleMessage(const cro::Message&) override;
    void process(float) override;

private:
    void onEntityAdded(cro::Entity) override;

    std::size_t m_aliveLaserCount;
    std::size_t m_deadLaserCount;
    std::vector<cro::int32> m_aliveLasers;
    std::vector<cro::int32> m_deadLasers;

    void spawnLaser(glm::vec3, glm::vec3);
    void processLasers(float);

    std::size_t m_aliveGrenadeCount;
    std::size_t m_deadGrenadeCount;
    std::vector<cro::int32> m_aliveGrenades;
    std::vector<cro::int32> m_deadGrenades;

    void spawnGrenade(glm::vec3, glm::vec3);
    void processGrenades(float);
};


#endif //DH_PLAYER_WEAPONS_HPP_