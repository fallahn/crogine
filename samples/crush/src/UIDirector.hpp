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

#pragma once

#include <crogine/ecs/Director.hpp>

#include <array>

struct PlayerUI final
{
    cro::Entity puntBar;
    cro::Entity lives;
};

struct SharedStateData;
struct AvatarEvent;
class UIDirector final : public cro::Director 
{
public:
    UIDirector(SharedStateData&, std::array<PlayerUI, 4u>&);

    void handleMessage(const cro::Message&) override;


private:
    SharedStateData& m_sharedData;
    std::array<PlayerUI, 4u>& m_playerUIs;

    std::array<cro::Entity, 4> m_resetMessages = {};

    void updateLives(const AvatarEvent&);
    cro::Entity createTextMessage(glm::vec2, const std::string&);
};
