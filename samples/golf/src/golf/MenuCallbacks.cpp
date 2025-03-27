/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include "MenuCallbacks.hpp"
#include "MenuState.hpp"
#include "CommandIDs.hpp"
#include "MessageIDs.hpp"

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/UISystem.hpp>

#include <crogine/util/Easings.hpp>

using namespace cl;

void MenuCallback::operator()(cro::Entity e, float dt)
{
    static constexpr float Speed = 2.f;
    auto& menuData = e.getComponent<cro::Callback>().getUserData<MenuData>();
    if (menuData.direction == MenuData::In)
    {
        //expand vertically
        menuData.currentTime = std::min(1.f, menuData.currentTime + (dt * Speed));
        e.getComponent<cro::Transform>().setScale({ 1.f, cro::Util::Easing::easeInQuint(menuData.currentTime) });

        if (menuData.currentTime == 1)
        {
            //stop here
            menuData.direction = MenuData::Out;
            e.getComponent<cro::Callback>().active = false;
            menuContext.uiScene->getSystem<cro::UISystem>()->setActiveGroup(menuData.targetMenu);
            *menuContext.currentMenu = menuData.targetMenu;

            //if we're hosting a lobby update the stride
            //if ((menuData.targetMenu == MenuState::MenuID::Lobby
            //    && menuContext.sharedData->hosting)
            //    /*|| menuData.targetMenu == MenuState::MenuID::Avatar*/)
            //{
            //    menuContext.uiScene->getSystem<cro::UISystem>()->setColumnCount(2);
            //}
            //else
            {
                menuContext.uiScene->getSystem<cro::UISystem>()->setColumnCount(1);
            }

            //start title animation
            cro::Command cmd;
            cmd.targetFlags = CommandID::Menu::TitleText;
            cmd.action = [](cro::Entity t, float)
            {
                t.getComponent<cro::Callback>().setUserData<float>(0.f);
                t.getComponent<cro::Callback>().active = true;
            };
            menuContext.uiScene->getSystem<cro::CommandSystem>()->sendCommand(cmd);

            auto* msg = cro::App::getInstance().getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
            msg->type = SystemEvent::MenuChanged;
            msg->data = menuData.targetMenu;
        }
    }
    else
    {
        //contract horizontally
        menuData.currentTime = std::max(0.f, menuData.currentTime - (dt * Speed));
        e.getComponent<cro::Transform>().setScale({ cro::Util::Easing::easeInQuint(menuData.currentTime), 1.f });

        menuContext.uiScene->getActiveCamera().getComponent<cro::Camera>().isStatic = false;
        menuContext.uiScene->getActiveCamera().getComponent<cro::Camera>().active = true;

        if (menuData.currentTime == 0)
        {
            //stop this callback
            menuData.direction = MenuData::In;
            e.getComponent<cro::Callback>().active = false;

            //set target position and init target animation
            auto* positions = menuContext.menuPositions;
            auto viewScale = *menuContext.viewScale;

            cro::Command cmd;
            cmd.targetFlags = CommandID::Menu::RootNode;
            cmd.action = [positions, menuData, viewScale](cro::Entity n, float)
            {
                n.getComponent<cro::Transform>().setPosition(positions[menuData.targetMenu] * viewScale);
            };
            menuContext.uiScene->getSystem<cro::CommandSystem>()->sendCommand(cmd);

            menuContext.menuEntities[menuData.targetMenu].getComponent<cro::Callback>().active = true;
            menuContext.menuEntities[menuData.targetMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = menuData.targetMenu;

            //hide incoming titles
            cmd.targetFlags = CommandID::Menu::TitleText;
            cmd.action = [](cro::Entity t, float)
            {
                t.getComponent<cro::Transform>().setScale({ 0.f, 0.f });
            };
            menuContext.uiScene->getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //remove any minigame actors that may have been active
            auto* scene = menuContext.uiScene;
            cmd.targetFlags = CommandID::Menu::Actor;
            cmd.action = [scene](cro::Entity e, float) { scene->destroyEntity(e); };
            menuContext.uiScene->getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
    }
}

void TitleTextCallback::operator()(cro::Entity e, float dt)
{
    auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
    currTime = std::min(1.f, currTime + dt);
    float scale = cro::Util::Easing::easeOutBounce(currTime);
    e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

    if (currTime == 1)
    {
        e.getComponent<cro::Callback>().active = false;
    }
}