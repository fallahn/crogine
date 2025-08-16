/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2025
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

struct MinimapZoom;
struct MiniBall final
{
    float currentTime = 1.f;
    std::int32_t playerID = -1; //(client * maxPlayer) + player - used when tidying up disconnecting client
    cro::Entity parent;
    
    cro::Entity minimap;
    cro::Entity minitrail;

    enum
    {
        Animating,
        Idle
    }state = Idle;

    std::uint8_t groupID = 0;
};

class MiniBallSystem final : public cro::System
{
public:
    MiniBallSystem(cro::MessageBus&, const MinimapZoom&, const std::uint8_t& serverGroup);

    void process(float) override;

    void setActivePlayer(std::uint8_t client, std::uint8_t player);


private:
    const MinimapZoom& m_minimapZoom;
    const std::uint8_t m_serverGroupID;
    std::int32_t m_activePlayer;

};