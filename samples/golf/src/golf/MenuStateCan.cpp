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

#include "MenuState.hpp"
#include "InterpolationSystem.hpp"
#include "Networking.hpp"
#include "CommandIDs.hpp"
#include "CoinSystem.hpp"
#include "server/ServerPacketData.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/detail/OpenGL.hpp>

namespace
{
    static constexpr float ActorY = 22.f; //banner height
    static constexpr float ActorZ = 3.f;
}

void MenuState::spawnActor(const ActorInfo& info)
{
    const glm::vec3 pos = { info.position.x, ActorY, ActorZ };

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::Actor;
    entity.addComponent<InterpolationComponent<InterpolationType::Linear>>(
        InterpolationPoint(pos, glm::vec3(0.f), cro::Transform::QUAT_IDENTITY, info.timestamp)).id = info.serverID;
    entity.addComponent<cro::Drawable2D>().setCullingEnabled(false);

    if (info.playerID == 0
        && !m_canActive)
    {
        //can
        entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Can];

        entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        entity.getComponent<cro::Transform>().setOrigin(glm::vec3(0.f, 0.f, -0.2f));
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(0.f);
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
            {
                auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
                ct = std::min(1.f, ct + (dt * 2.f));

                const float scale = cro::Util::Easing::easeOutElastic(ct);
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f, scale));

                if (ct == 1)
                {
                    e.getComponent<cro::Callback>().active = false;
                }
            };

        //control icon
        auto buttonEnt = m_uiScene.createEntity();
        buttonEnt.addComponent<cro::Transform>();
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::CanButton];
        buttonEnt.addComponent<cro::SpriteAnimation>().play(cro::GameController::getControllerCount() ? 2 : hasPSLayout(0) ? 1 : 0);
        buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CanButton;;
        buttonEnt.addComponent<cro::Callback>().active = true;
        buttonEnt.getComponent<cro::Callback>().function =
            [&, entity](cro::Entity e, float)
            {
                if (entity.destroyed())
                {
                    //not the best place to put it, but it'll do
                    m_canActive = false;

                    e.getComponent<cro::Callback>().active = false;
                    m_uiScene.destroyEntity(e);
                }
            };
        const auto canBounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        const auto buttonBounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
        buttonEnt.getComponent<cro::Transform>().setPosition({ canBounds.width / 2.f, canBounds.height / 2.f, 0.6f });
        buttonEnt.getComponent<cro::Transform>().setOrigin(glm::vec2(buttonBounds.width / 2.f, buttonBounds.height / 2.f));

        entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

        createCanControl(entity);
        m_canActive = true;
    }
    else if(info.playerID == 1)
    {
        //coin
        entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Coin];
        entity.addComponent<cro::SpriteAnimation>().play(0);
        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(1.f);
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
            {
                static constexpr float MaxVel = 240000.f;
                
                const float vel = glm::length2(e.getComponent<InterpolationComponent<InterpolationType::Linear>>().getVelocity());
                const float animSpeed = std::min(1.f, vel / MaxVel);

                e.getComponent<cro::SpriteAnimation>().playbackRate = animSpeed;

                //initial vel is 0 so we want to wait before checking this
                auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
                ct -= dt;
                if (ct < 0 &&
                    animSpeed < 0.1f)
                {
                    e.getComponent<cro::SpriteAnimation>().stop();
                }
            };
    }

    m_bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    
    //beh we need to delay this a frame or 2
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::int32_t>(0);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            auto& count = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
            count++;

            m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true;
            
            if (count == 5)
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
        };
}

void MenuState::updateActor(const ActorInfo& info)
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::Actor;
    cmd.action = [info](cro::Entity e, float)
        {
            auto& interp = e.getComponent<InterpolationComponent<InterpolationType::Linear>>();
            if (interp.id == info.serverID)
            {
                glm::vec3 pos = { info.position.x, info.position.y + ActorY, ActorZ };
                interp.addPoint({ pos, glm::vec3(0.f), cro::Transform::QUAT_IDENTITY, info.timestamp });
            }
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void MenuState::createCanControl(cro::Entity can)
{
    auto entity = m_uiScene.createEntity();

    entity.addComponent<cro::Transform>().setPosition({ 0.f, ActorY, ActorZ + 0.4f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CanControl;

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<CanControl>();
    entity.getComponent<cro::Callback>().function =
        [&, can](cro::Entity e, float dt)
        {
            auto& control = e.getComponent<cro::Callback>().getUserData<CanControl>();
            if (control.active)
            {
                control.idx = (control.idx + 1) % control.waveTable.size();
                const float power = ((control.Max - control.Min) * control.waveTable[control.idx]) + control.Min;

                //update power and build verts
                glm::vec2 vel = glm::normalize(Coin::InitialVelocity) * power;
                glm::vec2 last = glm::vec2(0.f);
                std::vector<cro::Vertex2D> verts;

                do
                {
                    verts.emplace_back(last, TextNormalColour);
                    
                    last += vel * dt;
                    vel += Coin::Gravity * dt;

                } while (last.y > 0.f);
                verts.emplace_back(last, TextNormalColour);
                e.getComponent<cro::Drawable2D>().setVertexData(verts);

                //display arc
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            }
            else
            {
                //hide arc
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }

            if (can.destroyed())
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
        };
    m_bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}