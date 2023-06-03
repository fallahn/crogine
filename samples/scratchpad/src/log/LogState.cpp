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
#include <crogine/util/Wavetable.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtx/euler_angles.hpp>

namespace
{
    const std::string LogVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec2 a_texCoord0;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform mat3 u_normalMatrix;

    uniform float u_radius = 60.0;

    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec2 v_texCoord;

    mat3 getRotation(float angle)
    {
        float c = cos(angle);
        float s = sin(angle);

        return mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, c, -s), vec3(0.0, s, c));
    }

    void main()
    {
        vec4 worldPos = u_worldMatrix * a_position;

        float angle = -(worldPos.z / u_radius);
        worldPos.z = 0.0;
        worldPos.y += u_radius;
        
        mat3 rotation = getRotation(angle);
        worldPos.xyz = rotation * worldPos.xyz;
        worldPos.y -= u_radius;

        gl_Position = u_viewProjectionMatrix * worldPos;
        v_normal = u_normalMatrix * rotation * a_normal;
        v_texCoord = a_texCoord0;
        v_colour = a_colour;
    })";

    const std::string LogFragment = R"(
    OUTPUT

    uniform sampler2D u_diffuseMap;
    uniform vec3 u_lightDirection;

    VARYING_IN vec4 v_colour;
    VARYING_IN vec3 v_normal;
    VARYING_IN vec2 v_texCoord;

    void main()
    {
        FRAG_OUT = v_colour * (0.5 + (0.5 * dot(normalize(v_normal), normalize(-u_lightDirection))));
    })";


    struct ShaderID final
    {
        enum
        {
            Log
        };
    };

    //glm::vec3 camRotation = glm::vec3(-0.255f, 0.f, 0.886f);
    glm::vec3 camRotation = glm::vec3(-0.58f, 0.f, 0.f);
    glm::vec3 camStartPos = glm::vec3(0.f, 6.f, 13.3f);
}

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
    else if (evt.type == SDL_MOUSEMOTION)
    {
        if (evt.motion.state & SDL_BUTTON_RMASK)
        {
            camRotation.z -= static_cast<float>(evt.motion.xrel) * 0.005f;
            camRotation.x -= static_cast<float>(evt.motion.yrel) * 0.005f;

            m_gameScene.getActiveCamera().getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(camRotation)));
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            cro::App::getWindow().setMouseCaptured(false);
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
    if (cro::Keyboard::isKeyPressed(SDLK_SPACE))
    {
        movement.y += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_LCTRL))
    {
        movement.y -= 1.f;
    }
    if (auto len2 = glm::length2(movement); len2 != 0)
    {
        movement /= std::sqrt(len2);
        movement = m_gameScene.getActiveCamera().getComponent<cro::Transform>().getRotation() * movement;

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

    m_resources.shaders.loadFromString(ShaderID::Log, LogVertex, LogFragment);
}

void LogState::createScene()
{
    auto matID = m_resources.materials.add(m_resources.shaders.get(ShaderID::Log));
    auto material = m_resources.materials.get(matID);

    cro::ModelDefinition md(m_resources, &m_envMap);
    md.loadFromFile("assets/models/terrain_chunk.cmt");

    cro::Entity planeEnt;
    for (auto i = 0; i < 2; ++i)
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -i * 25.f });
        md.createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
        {
            /*static std::vector<float> wave = cro::Util::Wavetable::sine(0.2f);
            static std::size_t tableIndex = 0;

            auto pos = e.getComponent<cro::Transform>().getPosition();
            pos.z = wave[tableIndex] * 10.f;
            e.getComponent<cro::Transform>().setPosition(pos);

            tableIndex = (tableIndex + 1) % wave.size();*/

            auto pos = e.getComponent<cro::Transform>().getPosition();
            pos.z += 40.f * dt;
            if (pos.z > 25.f)
            {
                pos.z -= 50.f;
            }
            e.getComponent<cro::Transform>().setPosition(pos);
        };
        planeEnt = entity;
    }

    //auto planeEnt = entity;
    md.loadFromFile("assets/ssao/cart/cart.cmt");
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI / 2.f);
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 8.f });
    md.createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, material);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        float movement = 0.f;
        if (cro::Keyboard::isKeyPressed(SDLK_LEFT))
        {
            movement -= 30.f;
        }
        if (cro::Keyboard::isKeyPressed(SDLK_RIGHT))
        {
            movement += 30.f;
        }
        e.getComponent<cro::Transform>().move({ movement * dt, 0.f, 0.f });
    };

    //planeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 50.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition(camStartPos/*{ 20.f, 5.5f, 15.4f }*/);
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(camRotation)));

    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -1.2f);
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
                ImGui::Text("Rotation: %3.3f, %3.3f, %3.3f", camRotation.x, camRotation.y, camRotation.z);

                /*static glm::vec3 rotation(0.f);
                if (ImGui::SliderFloat3("Rotation", &rotation[0], -cro::Util::Const::PI, cro::Util::Const::PI))
                {
                    auto q = glm::toQuat(glm::orientate3(rotation));
                    tx.setRotation(q);
                }*/
            }
            ImGui::End();

            if (ImGui::Begin("Shader"))
            {
                static float radius = 60.f;
                if (ImGui::SliderFloat("Radius", &radius, 20.f, 150.f))
                {
                    radius = std::clamp(radius, 20.f, 150.f);

                    auto& shader = m_resources.shaders.get(ShaderID::Log);
                    glUseProgram(shader.getGLHandle());
                    glUniform1f(shader.getUniformID("u_radius"), radius);
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