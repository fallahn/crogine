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

#include "GameState.hpp"
#include "GameConsts.hpp"
#include "UIConsts.hpp"
#include "PlayerSystem.hpp"
#include "PuntBarSystem.hpp"
#include "SharedStateData.hpp"

#include <crogine/detail/OpenGL.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>

namespace
{

}

void GameState::createUI()
{
    //draws the split screen lines. The geometry
    //is calculated by the camera view update callback
    cro::Entity entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);

    m_splitScreenNode = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>(); //the game scene camera callback will also update this
    m_uiScene.setActiveCamera(entity);
}

void GameState::updatePlayerUI()
{
    for (auto i = 0u; i < m_cameras.size(); ++i)
    {
        auto playerIndex = m_cameras[i].getComponent<std::uint8_t>();
        if (!m_playerUIs[i].puntBar.isValid())
        {
            //create the entities
            m_playerUIs[i].puntBar = m_uiScene.createEntity();
            m_playerUIs[i].puntBar.addComponent<cro::Transform>();
            m_playerUIs[i].puntBar.addComponent<cro::Drawable2D>();
            m_playerUIs[i].puntBar.addComponent<PuntBar>().player = &m_inputParsers.at(playerIndex).getEntity().getComponent<Player>();

            //lives and stat entities are created by UI Director
        }

        //update the geometry
        auto colour = PlayerColours[playerIndex];

        //punt bar 
        m_playerUIs[i].puntBar.getComponent<cro::Drawable2D>().getVertexData() =
        {
            cro::Vertex2D(glm::vec2(0.f, PuntBarSize.y), colour),
            cro::Vertex2D(glm::vec2(0.f), colour),
            cro::Vertex2D(PuntBarSize, colour),
            cro::Vertex2D(glm::vec2(PuntBarSize.x, 0.f), colour)
        };
        m_playerUIs[i].puntBar.getComponent<cro::Drawable2D>().updateLocalBounds();

        auto offset = PuntBarOffset;
        offset.x = (cro::DefaultSceneSize.x / (m_cameras.size() == 1 ? 2 : 4)) - (PuntBarSize.x / 2.f);
        m_playerUIs[i].puntBar.getComponent<cro::Transform>().setPosition(glm::vec3(getUICorner(i, m_cameras.size()) + offset, UIDepth));
    }
}

void GameState::updateRoundStats(const RoundStats& stats)
{
    //if this is not our stat, skip
    if (m_inputParsers.count(stats.playerID) == 0)
    {
        return;
    }


    if (!m_playerUIs[stats.playerID].stats.isValid())
    {
        auto& font = m_sharedData.fonts.get(m_sharedData.defaultFontID);
        glm::vec2 messageOffset = glm::vec2(cro::DefaultSceneSize) / 2.f;
        if (m_cameras.size() > 1)
        {
            messageOffset.x /= 2;

            if (m_cameras.size() > 2)
            {
                messageOffset.y /= 2;
            }
        }
        messageOffset.y += 140.f;

        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(getUICorner(stats.playerID, m_cameras.size()) + messageOffset);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font);
        m_playerUIs[stats.playerID].stats = entity;
    }

    std::int32_t total = 0;

    std::string str;
    str += "Crush count: " + std::to_string(stats.crushCount) + " - " + std::to_string(stats.crushCount * CrushScore) + "\n";
    str += "Snail count: " + std::to_string(stats.snailCount) + " - " + std::to_string(stats.snailCount * SnailScore) + "\n";
    str += "Death count: " + std::to_string(stats.deathCount) + " - " + std::to_string(stats.deathCount * DeathPenalty) + "\n";
    str += "Bonus count: " + std::to_string(stats.bonusCount) + " - " + std::to_string(stats.bonusCount * BonusScore) + "\n";
    if (stats.winner)
    {
        str += "Survivor bonus: " + std::to_string(RoundScore) + "\n";
        total += RoundScore;
    }

    total += (stats.crushCount * CrushScore)
        + (stats.snailCount * SnailScore)
        + (stats.deathCount * DeathPenalty)
        + (stats.bonusCount * BonusScore);
    str += "\nTotal:" + std::to_string(total);

    m_playerUIs[stats.playerID].stats.getComponent<cro::Text>().setString(str);
    auto bounds = cro::Text::getLocalBounds(m_playerUIs[stats.playerID].stats);
    m_playerUIs[stats.playerID].stats.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, 0.f });

    //zoom on winner
    if (stats.winner)
    {
        for (auto& ip : m_inputParsers)
        {
            ip.second.getEntity().getComponent<Player>().cameraTargetIndex = stats.playerID;
        }
    }
}