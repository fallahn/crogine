/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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
    float cohesion = 10.f;
    float maxVelocity = 200.f;
    float targetRadius = 50.f;

    const std::string CubeFrag = R"(
    OUTPUT

    uniform sampler2D u_diffuseMap;

    VARYING_IN vec2 v_texCoord0;

    void main()
    {
        FRAG_OUT = TEXTURE(u_diffuseMap, v_texCoord0);
    })";

    struct ShaderID final
    {
        enum
        {
            Water = 1,


        };
    };

    struct MaterialID final
    {
        enum
        {
            Water

        };
    };
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
    else if (evt.type == SDL_MOUSEMOTION)
    {
        if (evt.motion.state & SDL_BUTTON_LMASK)
        {
            auto pos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(glm::vec2(evt.motion.x, evt.motion.y));
            m_target.setPosition(pos);
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            auto pos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(glm::vec2(evt.button.x, evt.button.y));
            m_target.setPosition(pos);
        }
    }

    //m_inputParser.handleEvent(evt);

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
    m_inputParser.process(dt);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void SwingState::render()
{
    m_gameScene.render();
    m_uiScene.render();

    m_target.draw();
    m_follower.draw();
}

//private
void SwingState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void SwingState::loadAssets()
{
    m_cubemap.loadFromFile("assets/images/1/cmap.ccm");
    m_resources.shaders.loadFromString(ShaderID::Water,
        cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::Unlit), CubeFrag, "#define TEXTURED\n#define SUBRECTS\n");
    m_resources.materials.add(MaterialID::Water, m_resources.shaders.get(ShaderID::Water));

    m_target.setVertexData(
        {
            cro::Vertex2D(glm::vec2(-10.f, 10.f), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(-10.f, -10.f), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(10.f, 10.f), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(10.f, -10.f), cro::Colour::Blue)
        }
    );

    m_follower.setVertexData(
        {
            cro::Vertex2D(glm::vec2(-10.f, 10.f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(-10.f, -10.f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f, 10.f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f, -10.f), cro::Colour::Red)
        }
    );
}

void SwingState::createScene()
{
    //this is called when the window is resized to automatically update the camera's matrices/viewport
    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&SwingState::updateView, this, std::placeholders::_1);

    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 1.2f, 2.5f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.5f);

    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/models/plane.cmt");

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt * 0.5f);
    };

    auto material = m_resources.materials.get(MaterialID::Water);
    material.doubleSided = true;
    auto* m = md.getMaterial(0);
    if (m->properties.count("u_diffuseMap"))
    {
        material.setProperty("u_diffuseMap", cro::TextureID(m->properties.at("u_diffuseMap").second.textureID));
    }
    if (m->properties.count("u_subrect"))
    {
        const float* v = m->properties.at("u_subrect").second.vecValue;
        glm::vec4 subrect(v[0], v[1], v[2], v[3]);
        material.setProperty("u_subrect", subrect);
    }
    material.animation = m->animation;
    entity.getComponent<cro::Model>().setMaterial(0, material);


    //entity uses callback system to process logic of follower
    struct FollowerData final
    {
        glm::vec2 velocity = glm::vec2(0.f);
    };

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<FollowerData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt) mutable
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<FollowerData>();

        auto diff = (m_target.getPosition() - m_follower.getPosition());

        //coherance
        data.velocity += diff * cohesion;

        //velocity matching is redundant in cases where target is stopped - because we'll just stop following


        //clamp velocity
        if (auto len2 = glm::length2(data.velocity); len2 > (maxVelocity * maxVelocity))
        {
            data.velocity = (data.velocity / glm::sqrt(len2)) * maxVelocity;
        }

        //reduce speed as we get within radius of target
        float multiplier = std::clamp(glm::length2(diff) / (targetRadius * targetRadius), 0.f, 1.f);

        //make sure not to mutate the velocity by the multiplier
        m_follower.move(data.velocity * multiplier * dt);
    };


    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.32f);
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.2f);
}

void SwingState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Follower"))
            {
                auto pos = m_target.getPosition();
                ImGui::Text("Target: %3.1f, %3.1f", pos.x, pos.y);

                pos = m_follower.getPosition();
                ImGui::Text("Follower: %3.1f, %3.1f", pos.x, pos.y);

                ImGui::Separator();
                ImGui::SliderFloat("Cohesion", &cohesion, 10.f, 40.f);
                ImGui::SliderFloat("Max Velocity", &maxVelocity, 200.f, 500.f);
                ImGui::SliderFloat("Target Radius", &targetRadius, 50.f, 100.f);
            }
            ImGui::End();   
        });


    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 320.f, 240.f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    entity.getComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-10.f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(50.f), cro::Colour::Blue) ,
            cro::Vertex2D(glm::vec2(50.f), cro::Colour::Green) ,
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        auto start = m_inputParser.getBackPoint();
        auto active = m_inputParser.getActivePoint();
        auto end = m_inputParser.getFrontPoint();

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        verts[0].position = start;
        verts[1].position = active;
        verts[2].position = end;
    };


    constexpr float Size = 100.f;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 600.f, 240.f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-10.f, -Size), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f,  -Size), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(-10.f,  -0.5f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f,  -0.5f), cro::Colour::Red),

            cro::Vertex2D(glm::vec2(-10.f,  -0.5f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(10.f,  -0.5f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(-10.f,  0.5f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(10.f,  0.5f), cro::Colour::White),

            cro::Vertex2D(glm::vec2(-10.f,  0.5f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f,  0.5f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(-10.f, Size), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f,  Size), cro::Colour::Red),


            cro::Vertex2D(glm::vec2(-10.f, -Size), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(10.f,  -Size), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(-10.f, 0.f), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(10.f,  0.f), cro::Colour::Blue),

        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        float height = verts[14].position.y;
        float targetAlpha = 0.f;

        if (m_inputParser.active())
        {
            height = m_inputParser.getActivePoint().y;
            targetAlpha = 1.f;
        }

        auto& currentAlpha = e.getComponent<cro::Callback>().getUserData<float>();
        const float InSpeed = dt * 6.f;
        const float OutSpeed = m_inputParser.getPower() == 0 ? InSpeed : dt * 0.5f;
        if (currentAlpha < targetAlpha)
        {
            currentAlpha = std::min(1.f, currentAlpha + InSpeed);
        }
        else
        {
            currentAlpha = std::max(0.f, currentAlpha - OutSpeed);
        }

        for (auto& v : verts)
        {
            v.colour.setAlpha(currentAlpha);
        }
        verts[14].position.y = height;
        verts[15].position.y = height;
    };

    auto updateUI = [](cro::Camera& cam)
    {
        //glm::vec2 size(cro::App::getWindow().getSize());
        //cam.setOrthographic(0.f, size.x, 0.f, size.y, -1.f, 2.f);
        cam.setOrthographic(0.f, 720.f, 0.f, 480.f, -1.f, 2.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    m_uiScene.getActiveCamera().getComponent<cro::Camera>().resizeCallback = updateUI;
    updateUI(m_uiScene.getActiveCamera().getComponent<cro::Camera>());
}

void SwingState::loadSettings()
{

}

void SwingState::saveSettings()
{

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