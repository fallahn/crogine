/*-----------------------------------------------------------------------

Matt Marchant - 2022
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

#include "BilliardsState.hpp"
#include "GameConsts.hpp"
#include "MenuConsts.hpp"
#include "CommandIDs.hpp"
#include "PacketIDs.hpp"
#include "../GolfGame.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Camera.hpp>

void BilliardsState::createUI()
{
    //displays the background
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.5f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_gameSceneTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function =
        [](cro::Entity e, float)
    {
        //this is activated once to make sure the
        //sprite is up to date with any texture buffer resize
        glm::vec2 texSize = e.getComponent<cro::Sprite>().getTexture()->getSize();
        e.getComponent<cro::Sprite>().setTextureRect({ glm::vec2(0.f), texSize });
        e.getComponent<cro::Transform>().setOrigin(texSize / 2.f);
        e.getComponent<cro::Callback>().active = false;
    };
    auto courseEnt = entity;



    //ui viewport is set 1:1 with window, then the scene
    //is scaled to best-fit to maintain pixel accuracy of text.
    auto updateView = [&, /*rootNode,*/ courseEnt](cro::Camera& cam) mutable
    {
        auto windowSize = GolfGame::getActiveTarget()->getSize();
        glm::vec2 size(windowSize);

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();

        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
        //rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        //rootNode.getComponent<cro::Transform>().setPosition(m_menuPositions[m_currentMenu] * m_viewScale);

        glm::vec2 courseScale(m_sharedData.pixelScale ? m_viewScale.x : 1.f);
        courseEnt.getComponent<cro::Transform>().setScale(courseScale);
        courseEnt.getComponent<cro::Callback>().active = true; //makes sure to delay so updating the texture size is complete first
        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -1.f));

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::UIElement;
        cmd.action =
            [&, size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size / m_viewScale;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void BilliardsState::showReadyNotify(const BilliardsPlayer& player)
{
    m_wantsNotify = (player.client == m_sharedData.localConnectionData.connectionID);

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 400.f, 300.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(m_sharedData.connectionData[player.client].playerData[player.player].name + "'s Turn");
    entity.addComponent<UIElement>().relativePosition= glm::vec2(0.5f);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MessageBoard | CommandID::UI::UIElement;

    if (m_wantsNotify)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 400.f, 200.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString("Press Button"); //TODO read controller/keyboard status for more meaningful string
        entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f, 0.33f);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MessageBoard | CommandID::UI::UIElement;
    }
}

void BilliardsState::showGameEnd(const BilliardsPlayer& player)
{
    //this may be from a player quitting mid-summary
    if (!m_gameEnded)
    {
        //show summary screen

        auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 400.f, 300.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString("Game Ended");


        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 200.f, 10.f, 0.23f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::uint8_t>>(1.f, ConstVal::SummaryTimeout);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            auto& [current, sec] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::uint8_t>>();
            current -= dt;
            if (current < 0)
            {
                current += 1.f;
                sec--;
            }

            e.getComponent<cro::Text>().setString("Returning to lobby in: " + std::to_string(sec));

            auto bounds = cro::Text::getLocalBounds(e);
            bounds.width = std::floor(bounds.width / 2.f);
            e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
        };


        m_gameEnded = true;
    }
}

void BilliardsState::toggleQuitReady()
{
    if (m_gameEnded)
    {
        m_sharedData.clientConnection.netClient.sendPacket<std::uint8_t>(PacketID::ReadyQuit, m_sharedData.clientConnection.connectionID, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
}
