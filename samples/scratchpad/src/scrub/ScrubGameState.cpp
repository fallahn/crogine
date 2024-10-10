//Auto-generated source file for Scratchpad Stub 10/10/2024, 12:10:42

#include "ScrubGameState.hpp"

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
#include <crogine/util/Easings.hpp>

namespace
{
    constexpr float BallRadius = 0.021f;
    constexpr float StrokeDistance = 0.16f - BallRadius;
    constexpr float BallOffsetPos = 0.2f;

    /*
    Handle default position is 0 on y
    and -StrokeDistance when fully inserted
    */

    //hacky placeholder for future input
    struct InputBinding
    {
        enum
        {
            NextClub, PrevClub, Left, Right,
            Count
        };
        std::array<std::int32_t, Count> keys = { SDLK_q, SDLK_e, SDLK_a, SDLK_d };
    };
    struct SharedData
    {
        InputBinding inputBinding;
    }m_sharedData;
}

ScrubGameState::ScrubGameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    //this is a pre-cached state
    //context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    //});
}

//public
bool ScrubGameState::handleEvent(const cro::Event& evt)
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
            //requestStackPush(StateID::ScrubPauseState);
            break;
        }

        if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Left])
        {
            m_handle.switchDirection(Handle::Down);
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Right])
        {
            m_handle.switchDirection(Handle::Up);
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void ScrubGameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool ScrubGameState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void ScrubGameState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void ScrubGameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void ScrubGameState::loadAssets()
{
}

void ScrubGameState::createScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/arcade/scrub/models/handle.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity entity, float dt)
            {
                m_handle.progress = std::clamp(m_handle.progress + (m_handle.speed * -m_handle.direction * dt), 0.f, 1.f);
                
                auto pos = entity.getComponent<cro::Transform>().getPosition();
                pos.y = cro::Util::Easing::easeOutSine(m_handle.progress) * -StrokeDistance;
                entity.getComponent<cro::Transform>().setPosition(pos);

                if (m_handle.progress == 0 || m_handle.progress == 1)
                {
                    if (m_handle.speed != 0)
                    {
                        m_handle.calcStroke();
                    }
                    m_handle.speed = 0.f;
                }
            };

        m_handle.entity = entity;
    }

    if (md.loadFromFile("assets/arcade/scrub/models/ball.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -BallOffsetPos, 0.f, 0.f });
        md.createModel(entity);

        m_ball.entity = entity;
    }

    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto camera = m_gameScene.getActiveCamera();
    auto& cam = camera.getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    camera.getComponent<cro::Transform>().setPosition({ 0.f, 0.05f, 0.25f });
    camera.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.1f);

    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.02f);
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.02f);
}

void ScrubGameState::createUI()
{
    registerWindow([&]() 
        {
            ImGui::Begin("Buns");
            ImGui::Text("Handle Speed: %3.2f", m_handle.speed);
            ImGui::Text("Handle Direction: %3.2f", m_handle.direction);

            //TODO make this steeper as we progress in the game (idk, water is getting dirtier or something
            ImGui::Text("Handle Stroke: %3.2f", cro::Util::Easing::easeInQuad(m_handle.stroke));
            ImGui::Text("Handle Progress: %3.2f", m_handle.progress);

            /*const auto pos = m_handle.entity.getComponent<cro::Transform>().getPosition().y;
            ImGui::Text("Handle Position: %3.3f", pos);*/

            ImGui::End();
        });


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);

        //TODO send messge to UI elements to reposition them
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}


//handle funcs
void ScrubGameState::Handle::switchDirection(float d)
{
    CRO_ASSERT(d == Down || d == Up, "");

    if (d != direction)
    {
        //do this first as it uses the current direction
        calcStroke();

        direction = d;
        speed = MaxSpeed;
    }
}

void ScrubGameState::Handle::calcStroke()
{
    const float currPos = entity.getComponent<cro::Transform>().getPosition().y;
    stroke = ((currPos - strokeStart) / StrokeDistance) * direction;
    strokeStart = currPos;
}