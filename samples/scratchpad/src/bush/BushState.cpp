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
#include <crogine/core/ConfigFile.hpp>
#include <crogine/graphics/Palette.hpp>

#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
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

    #if defined(INSTANCING)
        ATTRIBUTE mat4 a_instanceWorldMatrix;
        ATTRIBUTE mat3 a_instanceNormalMatrix;
    #else
        uniform mat3 u_normalMatrix;
    #endif
        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewMatrix;
        uniform mat4 u_viewProjectionMatrix;
        uniform mat4 u_projectionMatrix;
        uniform vec3 u_cameraWorldPosition;

        uniform vec4 u_clipPlane;
        uniform float u_targetHeight; //height of the render target multiplied by its viewport, ie height of the renderable area

        uniform float u_leafSize = 0.25; //world units, in this case metres
        uniform float u_randAmount = 0.2;

        layout (std140) uniform WindValues
        {
            vec4 u_windData; //dirX, strength, dirZ, elapsedTime
        };

        layout (std140) uniform ScaledResolution
        {
            vec2 u_scaledResolution;
            float u_nearFadeDistance;
        };

        VARYING_OUT float v_ditherAmount;
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
            int UID = gl_InstanceID << 16 | (gl_VertexID & 0x0000ffff);

        #if defined (INSTANCING)
            mat4 worldMatrix = u_worldMatrix * a_instanceWorldMatrix;
            mat3 normalMatrix = mat3(u_worldMatrix) * a_instanceNormalMatrix;            
        #else
            mat4 worldMatrix = u_worldMatrix;
            mat3 normalMatrix = u_normalMatrix;
        #endif

            float randVal = rand(vec2(UID));
            float offset = randVal * u_randAmount;
            vec4 position = a_position;
            position.xyz += (a_normal * offset);

            v_normal = normalMatrix * a_normal;
            v_colour = a_colour * (1.0 - (u_randAmount - offset)); //darken less offset leaves

            gl_ClipDistance[0] = dot(a_position, u_clipPlane);

            vec4 worldPosition = worldMatrix * position;

//wind
            float time = (u_windData.w * 15.0) + UID;
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
            worldPosition.xyz += windOffset * MaxWindOffset * u_windData.y;
            gl_Position = u_viewProjectionMatrix * worldPosition;


//size calc
            float variation = rand(-vec2(UID));
            variation = 0.5 + (0.5 * variation);

            float pointSize = u_leafSize + ((u_leafSize * 2.0) * offset);
            pointSize *= variation;

            vec3 camForward = vec3(u_viewMatrix[0][2], u_viewMatrix[1][2], u_viewMatrix[2][2]);
            
            float facingAmount = dot(v_normal, camForward);
            pointSize *= 0.5 + (0.5 * facingAmount);
            
            //shrink 'backfacing' to zero
            pointSize *= step(0.0, facingAmount); 
            
            //we use the camera's forward vector to shrink any points out of view to zero
            vec3 eyeDir = normalize(u_cameraWorldPosition - worldPosition.xyz);
            pointSize *= clamp(dot(eyeDir, (camForward)), 0.0, 1.0);

            
            //shrink with perspective/distance and scale to world units
            pointSize *= u_targetHeight * (u_projectionMatrix[1][1] / gl_Position.w);

            //we scale point size by model matrix but it assumes all axis are
            //scaled equally ,as we only use the X axis
            pointSize *= length(worldMatrix[0].xyz);

            gl_PointSize = pointSize;

//proximity fade
            float fadeDistance = u_nearFadeDistance * 2.0;
            const float farFadeDistance = 360.f;
            float distance = length(worldPosition.xyz - u_cameraWorldPosition);

            v_ditherAmount = pow(clamp((distance - u_nearFadeDistance) / fadeDistance, 0.0, 1.0), 2.0);
            v_ditherAmount *= 1.0 - clamp((distance - farFadeDistance) / fadeDistance, 0.0, 1.0);

        })";

    const std::string BushFragment = 
        R"(
        OUTPUT

        uniform sampler2D u_texture;
        uniform vec3 u_lightDirection;
        uniform vec3 u_colour = vec3(1.0);

        VARYING_IN float v_ditherAmount;
        VARYING_IN vec3 v_normal;
        VARYING_IN vec4 v_colour;
        VARYING_IN mat2 v_rotation;

        //function based on example by martinsh.blogspot.com
        const int MatrixSize = 8;
        float findClosest(int x, int y, float c0)
        {
            /* 8x8 Bayer ordered dithering */
            /* pattern. Each input pixel */
            /* is scaled to the 0..63 range */
            /* before looking in this table */
            /* to determine the action. */

            const int dither[64] = int[64](
             0, 32, 8, 40, 2, 34, 10, 42, 
            48, 16, 56, 24, 50, 18, 58, 26, 
            12, 44, 4, 36, 14, 46, 6, 38, 
            60, 28, 52, 20, 62, 30, 54, 22, 
             3, 35, 11, 43, 1, 33, 9, 41, 
            51, 19, 59, 27, 49, 17, 57, 25,
            15, 47, 7, 39, 13, 45, 5, 37,
            63, 31, 55, 23, 61, 29, 53, 21 );

            float limit = 0.0;
            if (x < MatrixSize)
            {
                limit = (dither[y * MatrixSize + x] + 1) / 64.0;
            }

            if (c0 < limit)
            {
                return 0.0;
            }
            return 1.0;
        }


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
            amount *= 2.0;
            amount = round(amount);
            amount /= 2.0;
            amount = 0.4 + (amount * 0.6);
#if defined(VERTEX_COLOURED)
            vec3 colour = mix(complementaryColour(v_colour.rgb), v_colour.rgb, amount);
#else
            vec3 colour = mix(complementaryColour(u_colour.rgb), u_colour.rgb, amount);
            //multiply by v_colour.a to darken on leaf depth - looks nice but not used
#endif

            vec2 coord = gl_PointCoord.xy;
            coord = v_rotation * (coord - vec2(0.5));
            coord += vec2(0.5);

//use texture and dither amount to see if we discard
            vec4 textureColour = TEXTURE(u_texture, coord);

            vec2 xy = gl_FragCoord.xy; // / u_pixelScale;
            int x = int(mod(xy.x, MatrixSize));
            int y = int(mod(xy.y, MatrixSize));

            float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount));
            if (textureColour.a * alpha < 0.3) discard;


            FRAG_OUT = vec4(colour, 1.0) * textureColour;
        })";

    std::string BranchVertex = R"(
        ATTRIBUTE vec4 a_position;
        ATTRIBUTE vec4 a_colour;
        ATTRIBUTE vec3 a_normal;
        ATTRIBUTE vec2 a_texCoord0;

    #if defined(INSTANCING)
        ATTRIBUTE mat4 a_instanceWorldMatrix;
        ATTRIBUTE mat3 a_instanceNormalMatrix;
    #else
        uniform mat3 u_normalMatrix;
    #endif

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;
        uniform vec4 u_clipPlane;
        uniform vec3 u_cameraWorldPosition;

        layout (std140) uniform WindValues
        {
            vec4 u_windData; //dirX, strength, dirZ, elapsedTime
        };

        layout (std140) uniform ScaledResolution
        {
            vec2 u_scaledResolution;
            float u_nearFadeDistance;
        };

        VARYING_OUT float v_ditherAmount;
        VARYING_OUT vec2 v_texCoord;
        VARYING_OUT vec3 v_normal;

        const float MaxWindOffset = 0.2;

        void main()
        {
        #if defined (INSTANCING)
            mat4 worldMatrix = u_worldMatrix * a_instanceWorldMatrix;
            mat3 normalMatrix = mat3(u_worldMatrix) * a_instanceNormalMatrix;            
        #else
            mat4 worldMatrix = u_worldMatrix;
            mat3 normalMatrix = u_normalMatrix;
        #endif

            vec4 worldPosition = worldMatrix * a_position;

            float time = (u_windData.w * 15.0) + gl_InstanceID;
            float x = sin(time * 2.0) / 8.0;
            float y = cos(time) / 2.0;
            vec3 windOffset = vec3(x, y, x) * a_colour.b * 0.1;


            vec3 windDir = normalize(vec3(u_windData.x, 0.f, u_windData.z));
            float dirStrength = a_colour.b;

            windOffset += windDir * u_windData.y * dirStrength;// * 2.0;
            worldPosition.xyz += windOffset * MaxWindOffset * u_windData.y;


            gl_Position = u_viewProjectionMatrix * worldPosition;
            gl_ClipDistance[0] = dot(u_clipPlane, a_position);

            v_texCoord = a_texCoord0;
            v_normal = normalMatrix * a_normal;

//proximity fade
            float fadeDistance = u_nearFadeDistance * 2.0;
            const float farFadeDistance = 360.f;
            float distance = length(worldPosition.xyz - u_cameraWorldPosition);

            v_ditherAmount = pow(clamp((distance - u_nearFadeDistance) / fadeDistance, 0.0, 1.0), 2.0);
            v_ditherAmount *= 1.0 - clamp((distance - farFadeDistance) / fadeDistance, 0.0, 1.0);
        })";

    std::string BranchFragment = R"(
        OUTPUT

        uniform sampler2D u_texture;
        uniform vec3 u_lightDirection;

        VARYING_IN float v_ditherAmount;
        VARYING_IN vec2 v_texCoord;
        VARYING_IN vec3 v_normal;

        //function based on example by martinsh.blogspot.com
        const int MatrixSize = 8;
        float findClosest(int x, int y, float c0)
        {
            const int dither[64] = int[64](
             0, 32, 8, 40, 2, 34, 10, 42, 
            48, 16, 56, 24, 50, 18, 58, 26, 
            12, 44, 4, 36, 14, 46, 6, 38, 
            60, 28, 52, 20, 62, 30, 54, 22, 
             3, 35, 11, 43, 1, 33, 9, 41, 
            51, 19, 59, 27, 49, 17, 57, 25,
            15, 47, 7, 39, 13, 45, 5, 37,
            63, 31, 55, 23, 61, 29, 53, 21 );

            float limit = 0.0;
            if (x < MatrixSize)
            {
                limit = (dither[y * MatrixSize + x] + 1) / 64.0;
            }

            if (c0 < limit)
            {
                return 0.0;
            }
            return 1.0;
        }

        void main()
        {
            vec4 colour = TEXTURE(u_texture, v_texCoord);

            float amount = dot(normalize(v_normal), -u_lightDirection);
            amount *= 2.0;
            amount = round(amount);
            amount /= 2.0;
            amount = 0.6 + (amount * 0.4);

            colour.rgb *= amount;
            FRAG_OUT = colour;

            vec2 xy = gl_FragCoord.xy; // / u_pixelScale;
            int x = int(mod(xy.x, MatrixSize));
            int y = int(mod(xy.y, MatrixSize));

            float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount));
            if (alpha < 0.1) discard;
        })";

    struct ShaderID final
    {
        enum
        {
            Bush,
            Branch
        };
    };

    std::int32_t bushMaterial = -1;
    std::int32_t branchMaterial = -1;

    struct TreesetData final
    {
        float leafSize = 0.25f; //metres
        float randomAmount = 0.2f;
        glm::vec3 colour = glm::vec3(1.f);

        std::string modelPath;
        std::string texturePath;

    }treeset;
    std::string lastTreeset = "untitled.tst";

    struct ShaderUniforms final
    {
        std::uint32_t shaderID = 0;
        std::int32_t size = -1;
        std::int32_t randomness = -1;
        std::int32_t colour = -1;
        std::int32_t targetHeight = -1;
    }shaderUniform;

    float rotation = 0.f;
    cro::Texture* leafTexture = nullptr;

    WindData windData;
    float windRotation = 0.f;

    cro::Palette palette;
}

BushState::BushState(cro::StateStack& stack, cro::State::Context context)
    : cro::State        (stack, context),
    m_gameScene         (context.appInstance.getMessageBus()),
    m_uiScene           (context.appInstance.getMessageBus()),
    m_windBuffer        ("WindValues"),
    m_resolutionBuffer  ("ScaledResolution")
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
                ImGui::Text("NOTE THAT THIS IS A PROTOTYPE\nAND DEVELOPMENT HAS BEEN MOVED\nTO GOLF PROJECT");
                if (ImGui::Button("Open Model"))
                {
                    auto path = cro::FileSystem::openFileDialogue("", "cmt");
                    if (!path.empty())
                    {
                        loadModel(path);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Open Preset"))
                {
                    auto path = cro::FileSystem::openFileDialogue("", "tst");
                    if (!path.empty())
                    {
                        loadPreset(path);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Save Preset"))
                {
                    auto path = cro::FileSystem::saveFileDialogue(lastTreeset, "tst");
                    if (!path.empty())
                    {
                        savePreset(path);
                    }
                }

                if (ImGui::SliderFloat("Leaf Size", &treeset.leafSize, 0.01f, 1.f))
                {
                    glUseProgram(shaderUniform.shaderID);
                    glUniform1f(shaderUniform.size, treeset.leafSize);
                }

                if (ImGui::SliderFloat("Randomness", &treeset.randomAmount, 0.f, 1.f))
                {
                    glUseProgram(shaderUniform.shaderID);
                    glUniform1f(shaderUniform.randomness, treeset.randomAmount);
                }

                if (ImGui::ColorEdit3("Colour", &treeset.colour[0]))
                {
                    glUseProgram(shaderUniform.shaderID);
                    glUniform3f(shaderUniform.colour, treeset.colour.r, treeset.colour.g, treeset.colour.b);
                }

                if (ImGui::Button("Load Palette"))
                {
                    auto path = cro::FileSystem::openFileDialogue("", "ase");
                    if (!path.empty())
                    {
                        palette.loadFromFile(path);
                    }
                }
                auto i = 0;
                for (const auto& swatch : palette.getSwatches())
                {
                    for (const auto& colour : swatch.colours)
                    {
                        ImVec4 c(colour.colour.getVec4());
                        if (ImGui::ColorButton(std::to_string(i).c_str(), c))
                        {
                            treeset.colour = { c.x, c.y, c.z };
                            glUseProgram(shaderUniform.shaderID);
                            glUniform3f(shaderUniform.colour, c.x, c.y, c.z);
                        }

                        if ((i++ % 12) != 11)
                        {
                            ImGui::SameLine();
                        }
                    }
                }
                ImGui::NewLine();
                ImGui::Separator();
                ImGui::SliderFloat("Wind Strength", &windData.direction[1], 0.1f, 1.f);
                if (ImGui::SliderFloat("Wind Direction", &windRotation, 0.f, cro::Util::Const::PI))
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
                        treeset.texturePath = cro::FileSystem::getFileName(path);
                    }
                }
                ImGui::SameLine();
                ImGui::Image(*leafTexture, { 32.f, 32.f }, { 0.f, 0.f }, { 1.f, 1.f });


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

            //if (ImGui::Begin("Flaps"))
            //{
            //    /*ImGui::SliderFloat("Wind Strength", &windData.direction[1], 0.1f, 1.f);
            //    if (ImGui::SliderFloat("Wind Direction", &windRotation, cro::Util::Const::PI, cro::Util::Const::TAU))
            //    {
            //        auto dir = glm::rotate(glm::quat(0.f, 0.f, 0.f, 1.f), windRotation, cro::Transform::Y_AXIS) * glm::vec3(-1.f, 0.f, 0.f);
            //        windData.direction[0] = dir.x;
            //        windData.direction[2] = dir.z;
            //    }*/
            //}
            //ImGui::End();
        });
    context.appInstance.setClearColour(cro::Colour::CornflowerBlue);

    palette.loadFromFile("assets/bush/colordome-32.ase");
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
    if (cro::Keyboard::isKeyPressed(SDLK_SPACE))
    {
        movement.y += dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_LCTRL))
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
    m_windBuffer.bind();
    m_resolutionBuffer.bind();

    glEnable(GL_PROGRAM_POINT_SIZE);
    m_gameScene.render();
    m_uiScene.render();
}

//private
void BushState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void BushState::loadAssets()
{
    m_resources.shaders.loadFromString(ShaderID::Bush, BushVertex, BushFragment, "#define INSTANCING\n");
    auto* shader = &m_resources.shaders.get(ShaderID::Bush);
    bushMaterial = m_resources.materials.add(*shader);

    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    auto& texture = m_resources.textures.get("assets/bush/leaf05.png");
    texture.setSmooth(false);
    leafTexture = &texture;
    treeset.texturePath = "leaf05.png";

    auto& material = m_resources.materials.get(bushMaterial);
    material.setProperty("u_texture", texture);

    shaderUniform.shaderID = shader->getGLHandle();
    shaderUniform.randomness = shader->getUniformID("u_randAmount");
    shaderUniform.size = shader->getUniformID("u_leafSize");
    shaderUniform.colour = shader->getUniformID("u_colour");
    shaderUniform.targetHeight = shader->getUniformID("u_targetHeight");


    m_resources.shaders.loadFromString(ShaderID::Branch, BranchVertex, BranchFragment, "#define INSTANCING\n");
    shader = &m_resources.shaders.get(ShaderID::Branch);
    branchMaterial = m_resources.materials.add(*shader);

    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);


    struct Transform final
    {
        Transform(glm::vec3 t, glm::vec3 s, float r)
            : position(t), scale(s), rotation(r) {}
        glm::vec3 position = glm::vec3(0.f);
        glm::vec3 scale = glm::vec3(1.f);
        float rotation = 0.f;
    };

    const std::array transforms =
    {
        Transform(glm::vec3(2.f, 0.f, 10.f), glm::vec3(1.f), 1.f),
        Transform(glm::vec3(-2.f, 0.f, 10.f), glm::vec3(1.2f), 3.f),
        Transform(glm::vec3(0.f, 0.f, -3.f), glm::vec3(0.85f), 4.5f),
        Transform(glm::vec3(4.f, 0.f, -2.f), glm::vec3(0.5f), 5.5f),
        Transform(glm::vec3(-4.f, 0.f, -2.f), glm::vec3(1.85f), 2.5f),
        Transform(glm::vec3(0.f, 0.f, 24.f), glm::vec3(1.1f), 4.f),
    };

    for (const auto& t : transforms)
    {
        auto& mat = m_instanceTransforms.emplace_back(1.f);
        mat = glm::translate(mat, t.position);
        mat = glm::rotate(mat, t.rotation, cro::Transform::Y_AXIS);
        mat = glm::scale(mat, t.scale);
    }
}

void BushState::createScene()
{
    //placeholder to help visialise player size
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/bush/player_box.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        float offset = entity.getComponent<cro::Model>().getMeshData().boundingBox[0].y;
        entity.getComponent<cro::Transform>().setOrigin({ 0.f, offset, 0.f });
    }

    if (md.loadFromFile("assets/bush/ground_plane.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
        md.createModel(entity);
    }


    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&BushState::updateView, this, std::placeholders::_1);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(2048, 2048);
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
    cam3D.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 40.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    //set the correct renderable height in the shader
    auto targetHeight = static_cast<float>(cro::App::getWindow().getSize().y) * size.y;
    glUseProgram(shaderUniform.shaderID);
    glUniform1f(shaderUniform.targetHeight, targetHeight);

    ResolutionData rd;
    rd.resolution = cro::App::getWindow().getSize();
    m_resolutionBuffer.setData(rd);

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
        //base model on left
        cro::Entity entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        m_models[0] = entity;

        float radius = m_models[0].getComponent<cro::Model>().getMeshData().boundingSphere.radius;
        radius *= 1.1f;
        m_models[0].getComponent<cro::Transform>().setPosition({ -radius, 0.f, 0.f });


        //same model with bush shader (instanced)
        md.loadFromFile(path, true);
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        m_models[1] = entity;
        m_models[1].getComponent<cro::Transform>().setPosition({ radius, 0.f, 0.f });
        m_models[1].getComponent<cro::Model>().setInstanceTransforms(m_instanceTransforms);

        m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, radius, radius * 2.f });

        auto& meshData = entity.getComponent<cro::Model>().getMeshData();
        meshData.primitiveType = GL_POINTS;

        //fudgy but it'll do to prove it works.
        auto mat = m_resources.materials.get(branchMaterial);
        if (md.getMaterial(0)->properties.count("u_diffuseMap"))
        {
            cro::TextureID tid(md.getMaterial(0)->properties.at("u_diffuseMap").second.textureID);
            mat.setProperty("u_texture", tid);
        }
        entity.getComponent<cro::Model>().setMaterial(0, mat);


        if (meshData.submeshCount > 1)
        {
            meshData.indexData[1].primitiveType = GL_POINTS;
            entity.getComponent<cro::Model>().setMaterial(1, m_resources.materials.get(bushMaterial));
        }

        treeset.modelPath = cro::FileSystem::getFileName(path);
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Failed opening model");
        treeset = {};
    }
}

void BushState::loadPreset(const std::string& path)
{
    auto workingDir = cro::FileSystem::getFilePath(path);

    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path))
    {
        treeset = {};

        const auto& props = cfg.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "model")
            {
                treeset.modelPath = p.getValue<std::string>();
            }
            else if (name == "texture")
            {
                treeset.texturePath = p.getValue<std::string>();
            }
            else if (name == "colour")
            {
                treeset.colour = p.getValue<glm::vec3>();
                for (auto i = 0; i < 3; ++i)
                {
                    treeset.colour[i] = std::min(1.f, std::max(0.f, treeset.colour[i]));
                }
            }
            else if (name == "randomness")
            {
                treeset.randomAmount = std::max(treeset.randomAmount, std::min(1.f, p.getValue<float>()));
            }
            else if (name == "leaf_size")
            {
                treeset.leafSize = std::max(treeset.leafSize, std::min(1.f, p.getValue<float>()));
            }
        }

        loadModel(workingDir + treeset.modelPath);
        leafTexture->loadFromFile(workingDir + treeset.texturePath);

        glUseProgram(shaderUniform.shaderID);
        glUniform1f(shaderUniform.randomness, treeset.randomAmount);
        glUniform1f(shaderUniform.size, treeset.leafSize);
        glUniform3f(shaderUniform.colour, treeset.colour.r, treeset.colour.g, treeset.colour.b);

        lastTreeset = path;
    }
}

void BushState::savePreset(const std::string& path)
{
    cro::ConfigFile cfg("treeset");
    cfg.addProperty("model").setValue(treeset.modelPath);
    cfg.addProperty("texture").setValue(treeset.texturePath);
    cfg.addProperty("colour").setValue(treeset.colour);
    cfg.addProperty("randomness").setValue(treeset.randomAmount);
    cfg.addProperty("leaf_size").setValue(treeset.leafSize);

    //TODO these are just placeholders as we can't easily determine
    //programatically which material indices should have which shader
    cfg.addProperty("branch_index").setValue(0);
    cfg.addProperty("leaf_index").setValue(1);

    if (cfg.save(path))
    {
        lastTreeset = path;
    }
}