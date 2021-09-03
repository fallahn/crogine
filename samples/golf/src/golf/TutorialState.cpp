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

#include "TutorialState.hpp"
#include "SharedStateData.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>

TutorialState::TutorialState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_sharedData    (sd)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool TutorialState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsKeyboard() || cro::ui::wantsMouse())
    {
        return false;
    }


    //m_scene.getSystem<cro::UISystem>().handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void TutorialState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool TutorialState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void TutorialState::render()
{
    m_scene.render(cro::App::getWindow());
}

//private
void TutorialState::buildScene()
{

}