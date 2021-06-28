/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "LayoutState.hpp"
#include "SharedStateData.hpp"
#include "Messages.hpp"
#include "LayoutConsts.hpp"
#include "UIConsts.hpp"

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{

}

LayoutState::LayoutState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State    (stack, context),
    m_modelScene    (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_thumbScene    (context.appInstance.getMessageBus()),
    m_sharedData    (sd),
    m_layoutSize    (le::DefaultLayoutSize),
    m_nextResourceID(1),
    m_selectedSprite(0),
    m_selectedFont  (0)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        initUI();
    });

#ifdef CRO_DEBUG_
    loadFont("assets/fonts/ProggyClean.ttf");
#endif
}

//public
bool LayoutState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    m_modelScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void LayoutState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateLayout(data.data0, data.data1);
            //m_viewportRatio = updateView(m_scene.getActiveCamera(), DefaultFarPlane, m_fov);
        }
    }
    else if (msg.id == cro::Message::ConsoleMessage)
    {
        const auto& data = msg.getData<cro::Message::ConsoleEvent>();
        if (data.type == cro::Message::ConsoleEvent::LinePrinted)
        {
            switch (data.level)
            {
            default:
            case cro::Message::ConsoleEvent::Info:
                m_messageColour = { 0.f,1.f,0.f,1.f };
                break;
            case cro::Message::ConsoleEvent::Warning:
                m_messageColour = { 1.f,0.5f,0.f,1.f };
                break;
            case cro::Message::ConsoleEvent::Error:
                m_messageColour = { 1.f,0.f,0.f,1.f };
                break;
            }
        }
    }

    m_modelScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool LayoutState::simulate(float dt)
{
    //updates message colour
    const float colourIncrease = dt;// *0.5f;
    m_messageColour.w = std::min(1.f, m_messageColour.w + colourIncrease);
    m_messageColour.x = std::min(1.f, m_messageColour.x + colourIncrease);
    m_messageColour.y = std::min(1.f, m_messageColour.y + colourIncrease);
    m_messageColour.z = std::min(1.f, m_messageColour.z + colourIncrease);

    m_modelScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void LayoutState::render()
{
    auto& rt = cro::App::getWindow();
    m_modelScene.render(rt);
    m_uiScene.render(rt);
}

//private
void LayoutState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();


    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);


    m_thumbScene.addSystem<cro::SpriteSystem2D>(mb);
    m_thumbScene.addSystem<cro::TextSystem>(mb);
    m_thumbScene.addSystem<cro::CameraSystem>(mb);
    m_thumbScene.addSystem<cro::RenderSystem2D>(mb);
}

void LayoutState::loadAssets()
{
    cro::Image img;
    img.create(2, 2, cro::Colour::Magenta);
    m_backgroundTexture.loadFromImage(img);
}

void LayoutState::createScene()
{
    //TODO decide if we want a 3D scene and ditch it if we don't.

    //this is called when the window is resized to automatically update the camera's matrices/viewport
    auto camEnt = m_modelScene.getActiveCamera();
    updateView3D(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&LayoutState::updateView3D, this, std::placeholders::_1);



    //load a background texture. User can load a screen shot of their game for instance
    //to aid with lining up the the layout with their scene.
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -10.f });
    entity.getComponent<cro::Transform>().setScale(m_layoutSize / 2.f);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_backgroundTexture);
    m_backgroundEntity = entity;

    camEnt = m_uiScene.getActiveCamera();
    updateView2D(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&LayoutState::updateView2D, this, std::placeholders::_1);

    

    //this is used to render the thumbnail previews in the browser
    //so the camera can be set once and left as it's not affected by resizing the window
    camEnt = m_thumbScene.getActiveCamera();
    camEnt.getComponent<cro::Camera>().setOrthographic(0.f, static_cast<float>(uiConst::ThumbTextureSize), 0.f, static_cast<float>(uiConst::ThumbTextureSize), -0.1f, 1.f);
}

void LayoutState::updateView2D(cro::Camera& cam2D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    float infoHeight = (uiConst::InfoBarHeight / size.y);
    float titleHeight = (uiConst::TitleHeight / size.y);

    cam2D.viewport.left = uiConst::InspectorWidth;
    cam2D.viewport.width = 1.f - uiConst::InspectorWidth;
    cam2D.viewport.bottom = uiConst::BrowserHeight + infoHeight;
    cam2D.viewport.height = 1.f - cam2D.viewport.bottom - titleHeight;


    size.x -= (size.x * uiConst::InspectorWidth);
    size.y -= (size.y * (uiConst::BrowserHeight + infoHeight + titleHeight));
    auto windowRatio = size.x / size.y;
    auto viewRatio = m_layoutSize.x / m_layoutSize.y;

    if (windowRatio > viewRatio)
    {
        //side bars
        float size = cam2D.viewport.width * (viewRatio / windowRatio);
        float offset = (cam2D.viewport.width - size) / 2.f;

        cam2D.viewport.width = size;
        cam2D.viewport.left += offset;
    }
    else
    {
        //top/bottom bars
        float size = cam2D.viewport.height * (windowRatio / viewRatio);
        float offset = (cam2D.viewport.height - size) / 2.f;

        cam2D.viewport.height = size;
        cam2D.viewport.bottom += offset;
    }

    cam2D.setOrthographic(0.f, m_layoutSize.x, 0.f, m_layoutSize.y, -0.1f, 11.f);

    m_backgroundEntity.getComponent<cro::Transform>().setScale(
        m_layoutSize / m_backgroundEntity.getComponent<cro::Sprite>().getSize());
}

void LayoutState::updateView3D(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    //90 deg in x (glm expects fov in y)
    cam3D.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 140.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;
}