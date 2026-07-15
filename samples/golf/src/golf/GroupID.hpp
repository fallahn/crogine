/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2026
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

#include <Social.hpp>

#include <crogine/core/Clock.hpp>
#include <crogine/core/Log.hpp>
#include <crogine/gui/GuiClient.hpp>

class GroupID final : public cro::GuiClient
{
public:
    GroupID()
        : m_activeGroupID(0)
    {
        registerWindow([this]()
            {
                ImGui::Begin("Group ID");
                ImGui::Text("Rich Presence Group %llu", m_activeGroupID);
                ImGui::End();
            });
    }

    void update(std::uint64_t groupID, std::int32_t playerCount)
    {
        if (groupID
            && m_clock.elapsed() > m_expireTime)
        {
            m_clock.restart();
            Social::setGroup(groupID, playerCount);

            m_activeGroupID = groupID;
        }
    }

private:

    std::uint64_t m_activeGroupID;

    cro::Clock m_clock;
    static constexpr cro::Time m_expireTime = cro::seconds(30.f);
};