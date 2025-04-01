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

#include "ShopState.hpp"
#include "SharedStateData.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/UIElement.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>

ShopState::ShopState(cro::StateStack& stack, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(stack, ctx),
    m_sharedData(sd),
    m_uiScene   (ctx.appInstance.getMessageBus())
{
    CRO_ASSERT(!isCached(), "");

    ctx.mainWindow.loadResources([&]()
        {
            loadAssets();
            addSystems();
            buildScene();
        });
}

//public
bool ShopState::handleEvent(const cro::Event& evt)
{
    m_uiScene.forwardEvent(evt);

    return false;
}

void ShopState::handleMessage(const cro::Message& msg)
{
    m_uiScene.forwardMessage(msg);
}

bool ShopState::simulate(float dt)
{
    m_uiScene.simulate(dt);

    return true;
}

void ShopState::render()
{
    m_uiScene.render();
}

//private
void ShopState::loadAssets()
{

}

void ShopState::addSystems()
{
    auto& mb = cro::App::getInstance().getMessageBus();

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::UIElementSystem>(mb);
    m_uiScene.addSystem<cro::UISystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);
}

void ShopState::buildScene()
{


    auto camCallback = [&](cro::Camera& cam)
        {
            const glm::vec2 size(cro::App::getWindow().getSize());

            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
            cam.setOrthographic(0.f, size.x, 0.f, size.y, -10.f, 10.f);
        };
    m_uiScene.getActiveCamera().getComponent<cro::Camera>().resizeCallback = camCallback;
    camCallback(m_uiScene.getActiveCamera().getComponent<cro::Camera>());
}
