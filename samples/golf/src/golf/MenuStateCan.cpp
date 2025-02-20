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
#include "server/ServerPacketData.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>

namespace
{
    static constexpr float ActorY = 22.f; //banner height
    static constexpr float ActorZ = 0.3f;
}

void MenuState::spawnActor(const ActorInfo& info)
{
    //TODO pre-load this
    cro::SpriteSheet sheet;
    sheet.loadFromFile("assets/golf/sprites/lobby_can.spt", m_resources.textures);

    const glm::vec3 pos = { info.position.x, ActorY, ActorZ };

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::Actor;
    entity.addComponent<InterpolationComponent<InterpolationType::Linear>>(
        InterpolationPoint(pos, glm::vec3(0.f), cro::Transform::QUAT_IDENTITY, info.timestamp)).id = info.serverID;
    entity.addComponent<cro::Drawable2D>();

    if (info.playerID == 0)
    {
        //can
        entity.addComponent<cro::Sprite>() = sheet.getSprite("can");
    }
    else
    {
        //coin
        entity.addComponent<cro::Sprite>() = sheet.getSprite("coin");
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
            
            if (count == 3)
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
                glm::vec3 pos = { info.position.x, ActorY, ActorZ };
                interp.addPoint({ pos, glm::vec3(0.f), cro::Transform::QUAT_IDENTITY, info.timestamp });
            }
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}