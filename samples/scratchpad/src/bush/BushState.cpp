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

#include "BushState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>

#include <crogine/util/Constants.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    const std::string BushVertex = 
        R"(
        ATTRIBUTE vec4 a_position;
        ATTRIBUTE vec4 a_colour;
        ATTRIBUTE vec3 a_normal;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;
        uniform mat4 u_projectionMatrix;
        uniform mat3 u_normalMatrix;
        uniform vec3 u_cameraWorldPosition;

        uniform vec4 u_clipPlane;

        VARYING_OUT vec3 v_normal;
        VARYING_OUT vec4 v_colour;

        //TODO make a uniform
        const float LeafSize = 100.0;
        const float RandAmount = 0.2;

        float rand(vec2 position)
        {
            return fract(sin(dot(position, vec2(12.9898, 4.1414))) * 43758.5453);
        }


        void main()
        {
            float offset = rand(vec2(gl_VertexID)) * RandAmount;
            vec4 position = a_position;
            position.xyz += (a_normal * offset);

            v_normal = u_normalMatrix * a_normal;
            v_colour = a_colour;

            gl_ClipDistance[0] = dot(a_position, u_clipPlane);

            vec4 worldPos = u_worldMatrix * position;
            gl_Position = u_viewProjectionMatrix * worldPos;

            float pointSize = LeafSize + (LeafSize * offset);
            pointSize *= 0.6 + (0.4 * dot(v_normal, normalize(u_cameraWorldPosition - worldPos.xyz)));
            
            //TODO this needs to know viewport height if letterboxing (ie anything but 1.0)
            pointSize *= (u_projectionMatrix[1][1] / gl_Position.w); //shrink with perspective/distance

            gl_PointSize = pointSize;

            //TODO set up a varying based on how much offset is applied so less
            //offset leaves can be made darker. (or alter v_colour directly?)

        })";

    const std::string BushFragment = 
        R"(
        OUTPUT

        uniform sampler2D u_texture;
        uniform vec3 u_lightDirection;

        VARYING_IN vec3 v_normal;
        VARYING_IN vec4  v_colour;

        void main()
        {
            float amount = dot(normalize(v_normal), -u_lightDirection);
            amount *= 2.0;
            amount = round(amount);
            amount /= 2.0;
            amount = 0.8 + (amount * 0.2);

            vec4 colour = v_colour;
            colour.rgb *= amount;

            vec4 textureColour = TEXTURE(u_texture, gl_PointCoord.xy);
            if(textureColour.a < 0.3) discard;

            FRAG_OUT = colour * textureColour;
        })";

    struct ShaderID final
    {
        enum
        {
            Bush
        };
    };
}

BushState::BushState(cro::StateStack& stack, cro::State::Context context)
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
bool BushState::handleEvent(const cro::Event& evt)
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
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void BushState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool BushState::simulate(float dt)
{
    glm::vec3 movement(0.f);
    if (cro::Keyboard::isKeyPressed(SDLK_w))
    {
        movement.z -= dt;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_s))
    {
        movement.z += dt;
    }

    if (auto len2 = glm::length2(movement); len2 > 1)
    {
        movement /= std::sqrt(len2);
    }
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().move(movement);



    float rotation = 0.f;
    if (cro::Keyboard::isKeyPressed(SDLK_a))
    {
        rotation += dt;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_d))
    {
        rotation -= dt;
    }
    m_models[0].getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rotation);
    m_models[1].getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rotation);


    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void BushState::render()
{
    glEnable(GL_PROGRAM_POINT_SIZE);
    m_gameScene.render();
    m_uiScene.render();
}

//private
void BushState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void BushState::loadAssets()
{

}

void BushState::createScene()
{
    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/models/green_ball.cmt");

    //base model on left
    cro::Entity entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({-1.5f, 0.f, 0.f});
    md.createModel(entity);
    m_models[0] = entity;

    //same model with bush shader
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1.5f, 0.f, 0.f });
    md.createModel(entity);
    m_models[1] = entity;

    auto& meshData = entity.getComponent<cro::Model>().getMeshData();
    meshData.primitiveType = GL_POINTS;
    
    for (auto i = 0u; i < meshData.submeshCount; ++i)
    {
        meshData.indexData[i].primitiveType = GL_POINTS;
    }

    m_resources.shaders.loadFromString(ShaderID::Bush, BushVertex, BushFragment);
    auto* shader = &m_resources.shaders.get(ShaderID::Bush);
    auto materialID = m_resources.materials.add(*shader);

    auto& texture = m_resources.textures.get("assets/bush/leaf01.png");
    texture.setSmooth(false);

    auto material = m_resources.materials.get(materialID);
    material.setProperty("u_texture", texture);
    entity.getComponent<cro::Model>().setMaterial(0, material);


    //this is called when the window is resized to automatically update the camera's matrices/viewport
    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&BushState::updateView, this, std::placeholders::_1);
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 5.f });


    auto sun = m_gameScene.getSunlight();
    sun.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 35.f * cro::Util::Const::degToRad);
    sun.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -35.f * cro::Util::Const::degToRad);
}

void BushState::createUI()
{

}

void BushState::updateView(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    //90 deg in x (glm expects fov in y)
    cam3D.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 140.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    //update the UI camera to match the new screen size
    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;
}