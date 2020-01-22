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

#ifndef TL_PLAYER_WEAPONS_HPP_
#define TL_PLAYER_WEAPONS_HPP_

#include <crogine/ecs/System.hpp>
#include <crogine/core/Clock.hpp>

#include <crogine/detail/glm/vec3.hpp>

#include <vector>

struct PlayerWeapon final
{
    enum class Type
    {
        Pulse, Laser
    }type = Type::Pulse;
    float damage = 0.f;
    float activeTime = 0.f;
};

class PlayerWeaponSystem final : public cro::System
{
public:
    explicit PlayerWeaponSystem(cro::MessageBus&);

    void process(cro::Time) override;

    void handleMessage(const cro::Message&) override;

private:

    void onEntityAdded(cro::Entity) override;

    bool m_systemActive;
    bool m_allowFiring;
    enum FireMode
    {
        Single, Double, Triple, Laser/*, LaserWide*/
    }m_fireMode;

    float m_fireTime;
    float m_weaponDowngradeTime;

    glm::vec3 m_playerPosition = glm::vec3(0.f);
    cro::uint32 m_playerID;

    std::size_t m_aliveCount;
    std::vector<cro::int32> m_aliveList;
    std::size_t m_deadPulseCount;
    std::vector<cro::int32> m_deadPulses;
    std::size_t m_deadLaserCount;
    std::vector<cro::int32> m_deadLasers;
};

#endif //TL_PLAYER_WEAPONS_HPP_