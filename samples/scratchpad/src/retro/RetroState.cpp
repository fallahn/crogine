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

#include "RetroState.hpp"

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

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    const std::string ShaderVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform mat3 u_normalMatrix;

    VARYING_OUT vec3 v_normal;
    VARYING_OUT float v_colourIndex;

    void main()
    {
        gl_Position = u_viewProjectionMatrix * u_worldMatrix * a_position;

        v_normal = u_normalMatrix * a_normal;
        v_colourIndex = a_colour.r;
    })";


    const std::string ShaderFragment = R"(
    OUTPUT

    uniform sampler2D u_palette;

    VARYING_IN vec3 v_normal;
    VARYING_IN float v_colourIndex;


    const vec3 LightDir = vec3(1.0, 1.0, 1.0);
    const float PixelSize = 1.0;

    void main()
    {
        float lightAmount = dot(normalize(v_normal), normalize(LightDir));
        lightAmount *= 2.0;
        lightAmount = round(lightAmount);
        lightAmount /= 2.0;

        float check = mod((floor(gl_FragCoord.x / PixelSize) + floor(gl_FragCoord.y / PixelSize)), 2.0);
        check *= 2.0;
        check -= 1.0;
        lightAmount += 0.01 * check; //TODO this magic number should be based on the palette texture size, but it'll do as long as it's enough to push the coord in the correct direction.

        vec2 coord = vec2(v_colourIndex, lightAmount);
        vec3 colour = TEXTURE(u_palette, coord).rgb;

        FRAG_OUT = vec4(colour, 1.0);
    })";

    struct RetroShader final
    {
        enum
        {
            Ball
        };
    };

    struct ShaderProperties final
    {
        std::uint32_t ID = 0;
        std::int32_t palette = -1;
    }shaderProperties;



    struct RetroMaterial final
    {
        enum
        {
            Ball,

            Count
        };
    };

    std::array<std::int32_t, RetroMaterial::Count> MaterialIDs = {};
}

RetroState::RetroState(cro::StateStack& stack, cro::State::Context context)
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
bool RetroState::handleEvent(const cro::Event& evt)
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
        case SDLK_KP_PLUS:
            //if (!debugEnt.isValid())
            {
                auto debugEnt = m_uiScene.createEntity();
                debugEnt.addComponent<cro::Transform>().setScale(glm::vec2(10.f));
                debugEnt.getComponent<cro::Transform>().setPosition({ 0.f, m_sprites.size() * 10.f });
                debugEnt.addComponent<cro::Drawable2D>();
                debugEnt.addComponent<cro::Sprite>(m_paletteTexture);
                m_sprites.push_back(debugEnt);
            }
            break;
        case SDLK_KP_MINUS:
            if (!m_sprites.empty())
            {
                m_uiScene.destroyEntity(m_sprites.back());
                m_sprites.pop_back();
            }
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void RetroState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            float scale = std::round(static_cast<float>(data.data0) / m_renderTexture.getSize().x);
            m_quad.setScale(glm::vec2(scale));
        }
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool RetroState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void RetroState::render()
{
    m_renderTexture.clear();
    m_gameScene.render();
    m_renderTexture.display();

    m_quad.draw();
    m_uiScene.render();
}

//private
void RetroState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void RetroState::loadAssets()
{
    m_paletteTexture.loadFromFile("assets/retro/palette01.png");
    m_paletteTexture.setRepeated(false);

    m_resources.shaders.loadFromString(RetroShader::Ball, ShaderVertex, ShaderFragment);
    auto* shader = &m_resources.shaders.get(RetroShader::Ball);

    shaderProperties.ID = shader->getGLHandle();
    shaderProperties.palette = shader->getUniformID("u_palette");

    MaterialIDs[RetroMaterial::Ball] = m_resources.materials.add(*shader);


    m_renderTexture.create(480, 270);
    m_quad.setTexture(m_renderTexture.getTexture());
    m_quad.setScale(glm::vec2(4.f));
}

void RetroState::createScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/retro/head.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -5.f });
        md.createModel(entity);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
            e.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, dt / 2.f);
        };

        auto material = m_resources.materials.get(MaterialIDs[RetroMaterial::Ball]);
        material.setProperty("u_palette", m_paletteTexture);
        entity.getComponent<cro::Model>().setMaterial(0, material);
    }



    //this is called when the window is resized to automatically update the camera's matrices/viewport
    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&RetroState::updateView, this, std::placeholders::_1);
}

void RetroState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Window of happiness"))
            {
                ImGui::Image(m_paletteTexture, { 256.f, 32.f }, { 0.f, 1.f }, { 1.f, 0.f });

                if (ImGui::Button("Swap Palette"))
                {
                    static std::size_t pathIndex = 0;
                    pathIndex = (pathIndex + 1) % 2;

                    std::array<std::string, 2u> paths =
                    {
                        "assets/retro/palette01.png",
                        "assets/retro/palette02.png"
                    };

                    m_paletteTexture.loadFromFile(paths[pathIndex]);
                }


                ImGui::Text("Sprites %u", m_sprites.size());
            }
            ImGui::End();        
        });
}

void RetroState::updateView(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    auto windowSize = size;
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    //90 deg in x (glm expects fov in y)
    cam3D.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 140.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    //update the UI camera to match the new screen size
    //TODO DON'T DO THIS. Each camera wants its own specific
    //resize callback else the default one will override it.
    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = {0.f, 0.f, 1.f, 1.f};

    cam2D.setOrthographic(0.f, windowSize.x, 0.f, windowSize.y, -0.1f, 10.f);
}