//Auto-generated source file for Scratchpad Stub 30/05/2023, 10:21:52

#include "LogState.hpp"

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
#include <crogine/detail/glm/gtx/euler_angles.hpp>

LogState::LogState(cro::StateStack& stack, cro::State::Context context)
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
}

//public
bool LogState::handleEvent(const cro::Event& evt)
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

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void LogState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool LogState::simulate(float dt)
{
    glm::vec3 movement(0.f);
    if (cro::Keyboard::isKeyPressed(SDLK_a))
    {
        movement.x -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_d))
    {
        movement.x += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_s))
    {
        movement.z += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_w))
    {
        movement.z -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_e))
    {
        movement.y += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_q))
    {
        movement.y -= 1.f;
    }
    if (auto len2 = glm::length2(movement); len2 != 0)
    {
        movement /= std::sqrt(len2);
        m_gameScene.getActiveCamera().getComponent<cro::Transform>().move(movement * 10.f * dt);
    }


    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void LogState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void LogState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void LogState::loadAssets()
{
    m_envMap.loadFromFile("assets/images/hills.hdr");
    m_gameScene.setCubemap(m_envMap);
}

void LogState::createScene()
{
    cro::ModelDefinition md(m_resources, &m_envMap);
    md.loadFromFile("assets/models/terrain_chunk.cmt");

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);




    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 50.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 20.f, 5.5f, 15.4f });
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(glm::vec3(-0.255f, 0.f, 0.886f))));
}

void LogState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Camera"))
            {
                auto& tx = m_gameScene.getActiveCamera().getComponent<cro::Transform>();
                auto pos = tx.getPosition();
                ImGui::Text("Position: %3.3f, %3.3f, %3.3f", pos.x, pos.y, pos.z);

                static glm::vec3 rotation(0.f);
                if (ImGui::SliderFloat3("Rotation", &rotation[0], -cro::Util::Const::PI, cro::Util::Const::PI))
                {
                    auto q = glm::toQuat(glm::orientate3(rotation));
                    tx.setRotation(q);
                }
            }
            ImGui::End();        
        });



    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}