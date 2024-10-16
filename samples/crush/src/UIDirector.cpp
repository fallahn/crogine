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

#include "UIDirector.hpp"
#include "UIConsts.hpp"
#include "Messages.hpp"
#include "SharedStateData.hpp"
#include "ActorIDs.hpp"
#include "GameConsts.hpp"
#include "ResourceIDs.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/graphics/TextureResource.hpp>

#include <crogine/util/Easings.hpp>

namespace
{

}

UIDirector::UIDirector(SharedStateData& sd, cro::TextureResource& tr, std::array<PlayerUI, 4u>& pui)
    : m_sharedData  (sd),
    m_textures      (tr),
    m_playerUIs     (pui),
    m_playerCount   (0)
{

}

//public
void UIDirector::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case MessageID::AvatarMessage:
    {
        const auto& data = msg.getData<AvatarEvent>();
        switch (data.type)
        {
        default: break;
        case AvatarEvent::Died:
        {
            CRO_ASSERT(data.playerID > -1, "");

            //we want to check that the playerID actually belongs to this client
            if (data.playerID == m_sharedData.localPlayer.playerID
                || m_sharedData.localPlayerCount > 1)
            {
                if (!m_resetMessages[data.playerID].isValid())
                {
                    auto pos = getUICorner(data.playerID, m_sharedData.localPlayerCount);
                    auto ent = createTextMessage(pos + DiedMessageOffset, "Press Jump To Continue");
                    ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);

                    auto offset = m_sharedData.localPlayerCount > 1 ? cro::DefaultSceneSize.x / 4.f : cro::DefaultSceneSize.x / 2.f;
                    ent.getComponent<cro::Transform>().move(glm::vec3(offset, 0.f, UIDepth));
                    ent.getComponent<cro::Transform>().setScale(glm::vec2(0.f));

                    ent.addComponent<cro::Callback>().setUserData<std::tuple<float, float, float>>(0.f, 0.f, 1.f);
                    ent.getComponent<cro::Callback>().function =
                        [](cro::Entity e, float dt)
                    {
                        auto& [progress, target, delay] = e.getComponent<cro::Callback>().getUserData<std::tuple<float, float, float>>();

                        delay -= dt;
                        if (delay > 0)
                        {
                            return;
                        }

                        const auto rate = dt * 4.f;
                        if (target > progress)
                        {
                            progress = std::min(1.f, progress + rate);
                        }
                        else
                        {
                            progress = std::max(0.f, progress - rate);
                        }

                        float scale = cro::Util::Easing::easeInElastic(progress);
                        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

                        if (progress == target)
                        {
                            e.getComponent<cro::Callback>().active = false;
                            delay = 1.f - target;
                        }
                    };

                    m_resetMessages[data.playerID] = ent;
                }

                if (data.lives == 1)
                {
                    m_resetMessages[data.playerID].getComponent<cro::Text>().setString("Game Over");

                    auto pos = getUICorner(data.playerID, m_sharedData.localPlayerCount);
                    auto ent = createTextMessage(pos + DiedMessageOffset, "Press Jump To Switch Camera");
                    ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);

                    auto offset = m_sharedData.localPlayerCount > 1 ? cro::DefaultSceneSize.x / 4.f : cro::DefaultSceneSize.x / 2.f;
                    ent.getComponent<cro::Transform>().move(glm::vec3(offset, -38.f, UIDepth));
                }

                std::get<1>(m_resetMessages[data.playerID].getComponent<cro::Callback>().getUserData<std::tuple<float, float, float>>()) = 1.f;
                m_resetMessages[data.playerID].getComponent<cro::Callback>().active = true;
            }
        }
            break;
        case AvatarEvent::Reset:            
            if (m_resetMessages[data.playerID].isValid())
            {
                std::get<1>(m_resetMessages[data.playerID].getComponent<cro::Callback>().getUserData<std::tuple<float, float, float>>()) = 0.f;
                m_resetMessages[data.playerID].getComponent<cro::Callback>().active = true;
            }
            break;
        }
        updateLives(data);
    }
        break;
    case MessageID::GameMessage:
    {
        const auto& data = msg.getData<GameEvent>();
        switch (data.type)
        {
        default: break;
        case GameEvent::RoundWarn:
        {
            auto entity = createTextMessage(glm::vec2(cro::DefaultSceneSize) / 2.f, "30 Seconds Left!");
            entity.getComponent<cro::Transform>().move(glm::vec2(cro::DefaultSceneSize.x, 0.f));
            entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
            entity.getComponent<cro::Text>().setCharacterSize(120);
            entity.getComponent<cro::Text>().setFillColour(cro::Colour::Red);
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().move(glm::vec2(-800.f * dt, 0.f));
                if (e.getComponent<cro::Transform>().getPosition().x < -static_cast<float>(cro::DefaultSceneSize.x))
                {
                    e.getComponent<cro::Callback>().active = false;
                    getScene().destroyEntity(e);
                }
            };
        }
            break;
        case GameEvent::SuddenDeath:
        {
            auto entity = createTextMessage(glm::vec2(cro::DefaultSceneSize) / 2.f, "SUDDEN DEATH!");
            entity.getComponent<cro::Transform>().move(glm::vec2(cro::DefaultSceneSize.x, 0.f));
            entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
            entity.getComponent<cro::Text>().setCharacterSize(120);
            entity.getComponent<cro::Text>().setFillColour(cro::Colour::Red);
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().move(glm::vec2(-800.f * dt, 0.f));
                if (e.getComponent<cro::Transform>().getPosition().x < -static_cast<float>(cro::DefaultSceneSize.x))
                {
                    e.getComponent<cro::Callback>().active = false;
                    getScene().destroyEntity(e);
                }
            };

            //set all active players lives display instead of waiting for next update
            for (auto i = 0; i < 4; ++i)
            {
                if (m_playerUIs[i].lives.isValid())
                {
                    auto colour = PlayerColours[i];

                    auto& verts = m_playerUIs[i].lives.getComponent<cro::Drawable2D>().getVertexData();
                    verts.clear();

                    verts.emplace_back(glm::vec2(0.f), glm::vec2(0.f), colour);
                    verts.emplace_back(glm::vec2(LifeSize.x, 0.f), glm::vec2(1.f, 0.f), colour);
                    verts.emplace_back(glm::vec2(0.f, LifeSize.y), glm::vec2(0.f, 1.f), colour);
                    verts.emplace_back(glm::vec2(LifeSize.x, LifeSize.y), glm::vec2(1.f, 1.f), colour);

                    m_playerUIs[i].lives.getComponent<cro::Drawable2D>().updateLocalBounds();
                }
            }
        }
            break;
        case GameEvent::GameEnd:
        {
            showSummary();
        }
            break;
        }
    }
        break;
    }
}

//private
void UIDirector::updateLives(const AvatarEvent& evt)
{
    if (m_playerCount == 0)
    {
        return;
    }

    if (!m_playerUIs[evt.playerID].lives.isValid())
    {
        m_playerUIs[evt.playerID].lives = getScene().createEntity();
        m_playerUIs[evt.playerID].lives.addComponent<cro::Transform>();
        m_playerUIs[evt.playerID].lives.addComponent<cro::Drawable2D>().setTexture(&m_textures.get(TextureID::Life));
    }

    auto colour = PlayerColours[evt.playerID];

    auto& verts = m_playerUIs[evt.playerID].lives.getComponent<cro::Drawable2D>().getVertexData();
    verts.clear();

    for (auto j = 0; j < evt.lives; ++j)
    {
        verts.emplace_back(glm::vec2(0.f, j * LifeSize.y), glm::vec2(0.f), colour);
        verts.emplace_back(glm::vec2(LifeSize.x, j * LifeSize.y), glm::vec2(1.f, 0.f), colour);
        verts.emplace_back(glm::vec2(0.f, (j + 1) * LifeSize.y), glm::vec2(0.f, 1.f), colour);
        verts.emplace_back(glm::vec2(LifeSize.x, (j + 1) * LifeSize.y), glm::vec2(1.f, 1.f), colour);
    }

    m_playerUIs[evt.playerID].lives.getComponent<cro::Drawable2D>().updateLocalBounds();
    
    auto offset = LivesOffset;
    offset.x += (evt.playerID % 2) * (((cro::DefaultSceneSize.x / 2) - (LivesOffset.x * 2.f)) - LifeSize.x);
    m_playerUIs[evt.playerID].lives.getComponent<cro::Transform>().setPosition(glm::vec3(getUICorner(evt.playerID, m_playerCount) + offset, UIDepth));
}

cro::Entity UIDirector::createTextMessage(glm::vec2 position, const std::string& str)
{
    auto& font = m_sharedData.fonts.get(m_sharedData.defaultFontID);
    auto entity = getScene().createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(str);

    return entity;
}

void UIDirector::showSummary()
{
    auto entity = createTextMessage(glm::vec2(cro::DefaultSceneSize) / 2.f, "GAME OVER");
    entity.getComponent<cro::Transform>().move(glm::vec2(0.f, 480.f));
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setCharacterSize(120);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Red);

    entity = createTextMessage(glm::vec2(cro::DefaultSceneSize) / 2.f, "Return to Lobby in 15");
    entity.getComponent<cro::Transform>().move(glm::vec2(0.f, -440.f));
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setCharacterSize(100);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Red);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(0.f, 15);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& [currTime, secs] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
        currTime += dt;
        if (currTime > 1)
        {
            currTime -= 1;
            secs = std::max(0, secs - 1);

            auto str = "Return to Lobby in " + std::to_string(secs);
            e.getComponent<cro::Text>().setString(str);
        }
    };

    //dark background
    const cro::Colour bgColour = cro::Colour::Transparent;
    entity = getScene().createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, SummaryDepth));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, cro::DefaultSceneSize.y), bgColour),
        cro::Vertex2D(glm::vec2(0.f), bgColour),
        cro::Vertex2D(glm::vec2(cro::DefaultSceneSize), bgColour),
        cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x, 0.f), bgColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(currTime + (dt * 2.f), 1.f);

        std::uint8_t alpha = static_cast<std::uint8_t>(currTime * 190.f);
        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(alpha);
        }
        e.getComponent<cro::Drawable2D>().updateLocalBounds();

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };
}