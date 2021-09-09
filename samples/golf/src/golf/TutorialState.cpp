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
#include "MenuConsts.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
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
    //add systems
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);


    //load background
    cro::Colour c(0.f, 0.f, 0.f, BackgroundAlpha);
    glm::vec2 size = cro::App::getWindow().getSize();

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ size.x / 2.f, size.y / 2.f, -0.1f });
    entity.getComponent<cro::Transform>().setOrigin(size / 2.f);
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, size.y), c),
        cro::Vertex2D(glm::vec2(0.f), c),
        cro::Vertex2D(size, c),
        cro::Vertex2D(glm::vec2(size.x, 0.f), c)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + dt);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(BackgroundAlpha * currTime);
        }

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };

    switch (m_sharedData.tutorialIndex)
    {
    default: break;
    case 0:
        tutorialOne(entity);
        break;
    }
}

void TutorialState::tutorialOne(cro::Entity backgroundEnt)
{
    //TODO set up layout


    //TODO set up camera / resize callback
}

void TutorialState::quitState()
{
    //TODO trigger background callback to pop state
}