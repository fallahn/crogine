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

        uniform float u_leafSize = 150.0;
        uniform float u_randAmount = 0.2;

        layout (std140) uniform WindValues
        {
            vec4 u_windData; //dirX, strength, dirZ, elapsedTime
        };

        VARYING_OUT vec3 v_normal;
        VARYING_OUT vec4 v_colour;
        VARYING_OUT mat2 v_rotation;


        float rand(vec2 position)
        {
            return fract(sin(dot(position, vec2(12.9898, 4.1414))) * 43758.5453);
        }

        const float MaxWindOffset = 0.1f;

        void main()
        {
            float randVal = rand(vec2(gl_VertexID));
            float offset = randVal * u_randAmount;
            vec4 position = a_position;
            position.xyz += (a_normal * offset);

//rotates tex coords - TODO apply the wind direction?
            float rotation = randVal * 2.0;
            rotation -= 1.0;
            rotation *= 0.15;

            /*vec2 rot = vec2(sin(rotation), cos(rotation));
            v_rotation[0] = vec2(rot.y, -rot.x);
            v_rotation[1]= rot;*/

            v_normal = u_normalMatrix * a_normal;
            v_colour = a_colour * (1.0 - (u_randAmount - offset)); //darken less offset leaves

            gl_ClipDistance[0] = dot(a_position, u_clipPlane);

            vec4 worldPos = u_worldMatrix * position;

//wind
            float time = (u_windData.w * 15.0) + gl_VertexID;
            float x = sin(time * 2.0) / 8.0;
            float y = cos(time) / 2.0;
            vec3 windOffset = vec3(x, y, x);

            vec3 windDir = normalize(vec3(u_windData.x, 0.f, u_windData.z));
            float dirStrength = dot(v_normal, windDir);

            vec2 rot = vec2(sin(x * u_windData.y), cos(x * u_windData.y));
            v_rotation[0] = vec2(rot.y, -rot.x);
            v_rotation[1]= rot;


            dirStrength += 1.0;
            dirStrength /= 2.0;

            windOffset += windDir * u_windData.y * dirStrength * 4.0;
            worldPos.xyz += windOffset * MaxWindOffset * u_windData.y;
            gl_Position = u_viewProjectionMatrix * worldPos;


//size calc
            float variation = rand(-vec2(gl_VertexID));
            variation = 0.5 + (0.5 * variation);

            float pointSize = u_leafSize + ((u_leafSize * 2.0) * offset);
            pointSize *= variation;
            pointSize *= 0.6 + (0.4 * dot(v_normal, normalize(u_cameraWorldPosition - worldPos.xyz)));
            
            //shrink with perspective/distance
            //TODO this needs to know viewport height if letterboxing (ie anything but 1.0)
            pointSize *= (u_projectionMatrix[1][1] / gl_Position.w);

            gl_PointSize = pointSize;
        })";

    const std::string BushFragment = 
        R"(
        OUTPUT

        uniform sampler2D u_texture;
        uniform vec3 u_lightDirection;

        VARYING_IN vec3 v_normal;
        VARYING_IN vec4 v_colour;
        VARYING_IN mat2 v_rotation;

        vec3 rgb2hsv(vec3 c)
        {
            vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
            vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
            vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

            float d = q.x - min(q.w, q.y);
            float e = 1.0e-10;
            return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
        }

        vec3 hsv2rgb(vec3 c)
        {
            vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
            vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
            return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
        }

        vec3 complementaryColour(vec3 c)
        {
            vec3 a = rgb2hsv(c);
            a.x += 0.25;
            a.z *= 0.5;
            c = hsv2rgb(a);
            return c;
        }



        void main()
        {
            float amount = dot(normalize(v_normal), -u_lightDirection);
            /*amount *= 2.0;
            amount = round(amount);
            amount /= 2.0;*/
            amount = 0.4 + (amount * 0.6);

            //vec4 colour = v_colour;
            //colour.rgb *= amount;
            vec3 colour = mix(complementaryColour(v_colour.rgb), v_colour.rgb, amount);


            vec2 coord = gl_PointCoord.xy;
            coord = v_rotation * (coord - vec2(0.5));
            coord += vec2(0.5);

            vec4 textureColour = TEXTURE(u_texture, coord);
            if(textureColour.a < 0.3) discard;

            FRAG_OUT = vec4(colour, 1.0) * textureColour;
        })";

    struct ShaderID final
    {
        enum
        {
            Bush
        };
    };

    std::int32_t materialID = -1;
    float leafSize = 150.f;
    float randomAmount = 0.2f;

    struct ShaderUniforms final
    {
        std::uint32_t shaderID = 0;
        std::int32_t size = -1;
        std::int32_t randomness = -1;
    }shaderUniform;

    float rotation = 0.f;
    cro::Texture* leafTexture = nullptr;

    WindData windData;
    float windRotation = 0.f;
}

BushState::BushState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_windBuffer    ("u_windData")
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });

    registerWindow([&]() 
        {
            if (ImGui::Begin("Window of bunnage"))
            {
                if (ImGui::Button("Open Model"))
                {
                    auto path = cro::FileSystem::openFileDialogue("", "cmt");
                    if (!path.empty())
                    {
                        loadModel(path);
                    }
                }

                if (ImGui::SliderFloat("Leaf Size", &leafSize, 1.f, 500.f))
                {
                    glUseProgram(shaderUniform.shaderID);
                    glUniform1f(shaderUniform.size, leafSize);
                }

                if (ImGui::SliderFloat("Randomness", &randomAmount, 0.f, 1.f))
                {
                    glUseProgram(shaderUniform.shaderID);
                    glUniform1f(shaderUniform.randomness, randomAmount);
                }

                ImGui::SliderFloat("Wind Strength", &windData.direction[1], 0.1f, 1.f);
                if (ImGui::SliderFloat("Wind Direction", &windRotation, 0.f, cro::Util::Const::TAU))
                {
                    auto dir = glm::rotate(glm::quat(0.f, 0.f, 0.f, 1.f), windRotation, cro::Transform::Y_AXIS) * glm::vec3(-1.f, 0.f, 0.f);
                    windData.direction[0] = dir.x;
                    windData.direction[2] = dir.z;
                }

                if (ImGui::Button("Load Texture"))
                {
                    auto path = cro::FileSystem::openFileDialogue("", "png");
                    if (!path.empty())
                    {
                        leafTexture->loadFromFile(path);
                    }
                }
                ImGui::SameLine();
                ImGui::Image(*leafTexture, { 32.f, 32.f }, { 0.f, 1.f }, { 1.f, 0.f });

                ImGui::Separator();
                ImGui::Text("If model appears black there's no vertex colour.");



                ImGui::NewLine();
                if (m_models[0].isValid())
                {
                    if (ImGui::SliderFloat("Rotation", &rotation, 0.f, cro::Util::Const::TAU))
                    {
                        m_models[0].getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
                        m_models[1].getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
                    }
                }

            }        
            ImGui::End();
        });
    context.appInstance.setClearColour(cro::Colour::CornflowerBlue);
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
    static constexpr float Speed = 4.f;
    glm::vec3 movement(0.f);
    if (cro::Keyboard::isKeyPressed(SDLK_w))
    {
        movement.z -= dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_s))
    {
        movement.z += dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_q))
    {
        movement.y += dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_e))
    {
        movement.y -= dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_a))
    {
        movement.x -= dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_d))
    {
        movement.x += dt * Speed;
    }

    if (auto len2 = glm::length2(movement); len2 > 1)
    {
        movement /= std::sqrt(len2);
    }
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().move(movement);


    windData.elapsedTime += dt;
    m_windBuffer.setData(windData);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void BushState::render()
{
    m_windBuffer.bind(0);

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
    m_resources.shaders.loadFromString(ShaderID::Bush, BushVertex, BushFragment);
    auto* shader = &m_resources.shaders.get(ShaderID::Bush);
    materialID = m_resources.materials.add(*shader);

    auto& texture = m_resources.textures.get("assets/bush/leaf03.png");
    texture.setSmooth(false);
    leafTexture = &texture;

    auto& material = m_resources.materials.get(materialID);
    material.setProperty("u_texture", texture);

    shaderUniform.shaderID = shader->getGLHandle();
    shaderUniform.randomness = shader->getUniformID("u_randAmount");
    shaderUniform.size = shader->getUniformID("u_leafSize");
}

void BushState::createScene()
{
    //this is called when the window is resized to automatically update the camera's matrices/viewport
    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&BushState::updateView, this, std::placeholders::_1);
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.5f, 6.f });


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

void BushState::loadModel(const std::string& path)
{
    if (m_models[0].isValid())
    {
        m_gameScene.destroyEntity(m_models[0]);
        m_gameScene.destroyEntity(m_models[1]);
        rotation = 0.f;
    }


    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile(path))
    {
        //TODO read model bounding box and place apart as appropriate


        //base model on left
        cro::Entity entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -2.5f, 0.f, 0.f });
        md.createModel(entity);
        m_models[0] = entity;

        //same model with bush shader
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 2.5f, 0.f, 0.f });
        md.createModel(entity);
        m_models[1] = entity;

        auto& meshData = entity.getComponent<cro::Model>().getMeshData();
        meshData.primitiveType = GL_POINTS;

        /*for (auto i = 0u; i < meshData.submeshCount; ++i)
        {
            meshData.indexData[i].primitiveType = GL_POINTS;
        }*/
        meshData.indexData[1].primitiveType = GL_POINTS;
        entity.getComponent<cro::Model>().setMaterial(1, m_resources.materials.get(materialID));
        //entity.getComponent<cro::Model>().setMaterial(1, m_resources.materials.get(materialID));
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Failed opening model");
    }
}