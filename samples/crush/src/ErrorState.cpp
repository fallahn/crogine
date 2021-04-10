/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "ErrorState.hpp"
#include "SharedStateData.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/graphics/Image.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

ErrorState::ErrorState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene     (ctx.appInstance.getMessageBus())
{
    ctx.mainWindow.setMouseCaptured(false);

    auto msg = sd.errorMessage;
    registerWindow([&, msg, ctx]() 
        {
            auto windowSize = ctx.mainWindow.getSize();
            ImGui::SetNextWindowSize({ 200.f, 120.f });
            ImGui::SetNextWindowPos({ (windowSize.x - 200.f) / 2.f, (windowSize.y - 120.f) / 2.f });
            if (ImGui::Begin("Error"))
            {
                ImGui::Text("%s", msg.c_str());
                if (ImGui::Button("OK"))
                {
                    sd.serverInstance.stop();
                    sd.clientConnection.connected = false;
                    sd.clientConnection.netClient.disconnect();
                    sd.clientConnection.connectionID = 4;
                    sd.clientConnection.ready = false;

                    sd.hostState = SharedStateData::HostState::None;

                    requestStackClear();
                    requestStackPush(States::MainMenu);
                }
            }
            ImGui::End();
        
        });

    buildScene();
}

//public
bool ErrorState::handleEvent(const cro::Event& evt)
{
    m_scene.forwardEvent(evt);
    return false;
}

void ErrorState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool ErrorState::simulate(float dt)
{
    m_scene.simulate(dt);
    return false;
}

void ErrorState::render()
{
    m_scene.render(cro::App::getWindow());
}

//private
void ErrorState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);

    cro::Image img;
    img.create(2, 2, cro::Colour(std::uint8_t(0),0u,0u,120u));

    m_backgroundTexture.create(2, 2);
    m_backgroundTexture.update(img.getPixelData());

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ cro::DefaultSceneSize.x / 2.f, cro::DefaultSceneSize.y / 2.f, 1.f });
    entity.getComponent<cro::Transform>().setPosition({ -(cro::DefaultSceneSize.x / 2.f), -(cro::DefaultSceneSize.y / 2.f), 0.f });
    entity.addComponent<cro::Sprite>(m_backgroundTexture);
    entity.addComponent<cro::Drawable2D>();

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = std::bind(&ErrorState::updateView, this, std::placeholders::_1);

    m_scene.setActiveCamera(entity);
}

void ErrorState::updateView(cro::Camera& cam)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    cam.setOrthographic(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -2.f, 100.f);
    cam.viewport.bottom = (1.f - size.y) / 2.f;
    cam.viewport.height = size.y;
}