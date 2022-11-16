/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "SwingState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct MouseData final
    {
        float velocity = 0.f; //+/-1
        float accuracy = 0.f; //+/-1

        std::int32_t rawX = 0;
        std::int32_t rawY = 0;
    }mouseData;

    struct Settings final
    {
        float maxVelocity = 15.f;
    }settings;
}

SwingState::SwingState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });

    loadSettings();
}

SwingState::~SwingState()
{
    saveSettings();
}

//public
bool SwingState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;

        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            requestStackClear();
            requestStackPush(0);
            break;
        }
    }

    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            cro::App::getWindow().setMouseCaptured(true);
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            cro::App::getWindow().setMouseCaptured(false);
            break;
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        if (evt.motion.state & SDL_BUTTON_LMASK)
        {
            mouseData.rawX = evt.motion.xrel;
            mouseData.rawY = evt.motion.yrel;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void SwingState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool SwingState::simulate(float dt)
{
    static constexpr glm::vec2 ForwardVec(0.f, -1.f);
    static constexpr glm::vec2 RightVec(1.f, -0.f);
    glm::vec2 mouseVec = glm::vec2(static_cast<float>(mouseData.rawX), static_cast<float>(mouseData.rawY));

    if (glm::length2(mouseVec))
    {
        glm::vec2 mouseVecNorm = glm::normalize(mouseVec);

        //TODO this vector should be from the start point to end point of swing, not relMotion
        mouseData.accuracy = glm::dot(mouseVecNorm, RightVec); 
        //TODO this should time how long it takes to move from start to end of the stroke
        mouseData.velocity = (glm::length(mouseVec) / settings.maxVelocity) * cro::Util::Maths::sgn(glm::dot(mouseVecNorm, ForwardVec));
    }

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void SwingState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void SwingState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void SwingState::loadAssets()
{
    //button press resets rest point
    //if button pressed
        //track motion
            //if motion was backward or still
                //restart timer
                //record start point
            //if motion stopped, turned reverse or button released
                //record end point
                //if end point greater than rest point
                    //record time
                    //velocity is start->end dist over time
                    //accuracy is start->end dot forward vector
                    //power is velocity / max velocity
}

void SwingState::createScene()
{


    //this is called when the window is resized to automatically update the camera's matrices/viewport
    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&SwingState::updateView, this, std::placeholders::_1);


    auto updateUI = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.setOrthographic(0.f, 0.f, size.x, size.y, -10.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    m_uiScene.getActiveCamera().getComponent<cro::Camera>().resizeCallback = updateUI;
    updateUI(m_uiScene.getActiveCamera().getComponent<cro::Camera>());
}

void SwingState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Window of happiness"))
            {
                ImGui::Text("Velocity %3.3f", mouseData.velocity);
                ImGui::Text("Accuracy %3.3f", mouseData.accuracy);
                ImGui::Text("Raw X %d", mouseData.rawX);
                ImGui::Text("Raw Y %d", mouseData.rawY);

                ImGui::Separator();
                ImGui::SliderFloat("Max Velocity", &settings.maxVelocity, 1.f, 30.f);
            }
            ImGui::End();        
        });
}

void SwingState::loadSettings()
{
    cro::ConfigFile cfg;
    if (cfg.loadFromFile("settings.set"))
    {
        for (const auto& prop : cfg.getProperties())
        {
            const auto& name = prop.getName();
            if (name == "max_vel")
            {
                settings.maxVelocity = prop.getValue<float>();
            }
        }
    }
}

void SwingState::saveSettings()
{
    cro::ConfigFile cfg;
    cfg.addProperty("max_vel").setValue(settings.maxVelocity);


    cfg.save("settings.set");
}

void SwingState::updateView(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    auto windowSize = size;
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    //90 deg in x (glm expects fov in y)
    cam3D.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 140.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;
}