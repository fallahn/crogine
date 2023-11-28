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

#include "FrustumState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    const std::string IndexVertex =
        R"(
        ATTRIBUTE vec4 a_position;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_camMatrix;
        uniform mat4 u_viewProjectionMatrix;
        
        VARYING_OUT vec4 v_viewPosition;

        void main()
        {
            gl_Position = u_viewProjectionMatrix * u_worldMatrix * a_position;

            v_viewPosition = u_camMatrix * u_worldMatrix * a_position;

        })";

    //https://github.com/walbourn/directx-sdk-samples/blob/main/CascadedShadowMaps11/RenderCascadeScene.hlsl#L342
    const std::string IndexFragment =
        R"(
        uniform float u_viewSplits[4];

        VARYING_IN vec4 v_viewPosition;

        const vec3 Colours[4] = 
        vec3[4](
            vec3(0.0), vec3(0.33), vec3(0.66), vec3(1.0)
        );

        const int u_cascadeCount = 3;

        OUTPUT

        void main()
        {
            //this is from the MSDN article, but I don't see how it could ever
            //work if the depth value is only used to determine if it is in the
            //first partition or not. The result is always either 0 or u_cascadeCount -1
            /*vec4 depthVector = vec4(float(v_viewPosition.z > u_viewSplits[0]));
            vec4 indexVector = vec4(float(u_cascadeCount > 0.0),
                                    float(u_cascadeCount > 1.0),
                                    float(u_cascadeCount > 2.0),
                                    float(u_cascadeCount > 3.0));

            int index = int(min(u_cascadeCount, dot(depthVector, indexVector)));*/

            int index = 0;
            for(;index < u_cascadeCount;++index)
            {
                if (v_viewPosition.z > u_viewSplits[index])
                {
                    break;
                }
            }

            int nextIndex = min(index + 1, u_cascadeCount - 1);
            float fade = smoothstep(u_viewSplits[index] + 0.5, u_viewSplits[index],  v_viewPosition.z);

            vec3 colour = mix(Colours[index], Colours[nextIndex], fade);

            FRAG_OUT = vec4(colour, 1.0);
        })";


    const std::string SliceVertex =
        R"(
        ATTRIBUTE vec4 a_position;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;

        uniform mat4 u_lightViewProjections[CASCADES];

        VARYING_OUT vec4 v_lightWorldPositions[CASCADES];

        void main()
        {
            gl_Position = u_viewProjectionMatrix * u_worldMatrix * a_position;
            
            for(int i = 0; i < CASCADES; ++i)
            {
                v_lightWorldPositions[i] = u_lightViewProjections[i] * u_worldMatrix * a_position;
            }

        })";

    const std::string SliceFragment =
        R"(
        OUTPUT

        VARYING_IN vec4 v_lightWorldPositions[CASCADES];

        const vec3 Colours[3] = vec3[]
        (
            vec3(0.8,0.0,0.0),
            vec3(0.0,0.8,0.0),
            vec3(0.0,0.0,0.8)
        );

        void main()
        {
            FRAG_OUT = vec4(vec3(0.2), 1.0);

            for(int i = 0; i < CASCADES; ++i)
            {
                if(v_lightWorldPositions[i].w > 0.0)
                {
                    vec2 coords = v_lightWorldPositions[i].xy / v_lightWorldPositions[i].w / 2.0 + 0.5;
                    if(coords.x > 0 && coords.x < 1
                        && coords.y > 0 && coords.y < 1)
                    {
                        FRAG_OUT.rgb += Colours[i];
                    }
                }
            }
        })";

    std::int32_t CascadeCount = 3;

    struct PerspectiveDebug final
    {
        float nearPlane = 0.1f;
        float farPlane = 50.f;
        float fov = 40.f;
        float aspectRatio = 1.3f;

        float maxDistance = 100.f;
        float overshoot = 0.f;

        void update(cro::Camera& cam)
        {
            cam.setPerspective(fov * cro::Util::Const::degToRad, aspectRatio, nearPlane, farPlane, CascadeCount);
            //cam.setOrthographic(-10.f ,10.f, -10.f, 10.f, nearPlane, farPlane, 3);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        }
    }perspectiveDebug;
    

    struct KeyID final
    {
        enum
        {
            Up, Down, Left, Right,
            CW, CCW,

            Count
        };
    };
    struct KeysetID final
    {
        enum
        {
            Cube, Light
        };
    };
    constexpr std::array keysets =
    {
        std::array<std::int32_t, KeyID::Count>({SDLK_w, SDLK_s, SDLK_a, SDLK_d, SDLK_q, SDLK_e}),
        std::array<std::int32_t, KeyID::Count>({SDLK_HOME, SDLK_END, SDLK_DELETE, SDLK_PAGEDOWN, SDLK_INSERT, SDLK_PAGEUP})
    };

    struct RenderFlags final
    {
        enum
        {
            Vis = 0x1,
            Scene = 0x2
        };
    };

    struct ShaderID final
    {
        enum
        {
            Slice = 1,
            Interval
        };
    };
    std::int32_t LightUniformID = -1;
    std::uint32_t SplitProgramID = 0;

    std::int32_t IntervalUniformID = -1;
    std::int32_t IntervalCamID = -1;
    std::uint32_t IntervalProgramID = 0;
}

FrustumState::FrustumState(cro::StateStack& stack, cro::State::Context context)
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
bool FrustumState::handleEvent(const cro::Event& evt)
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
            requestStackClear();
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void FrustumState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            
        }
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool FrustumState::simulate(float dt)
{
    if (rotateEntity(m_entities[EntityID::Cube], KeysetID::Cube, dt))
    {
        calcCameraFrusta();
    }

    if (rotateEntity(m_entities[EntityID::Light], KeysetID::Light, dt))
    {
        m_gameScene.getSunlight().getComponent<cro::Transform>().setRotation(m_entities[EntityID::Light].getComponent<cro::Transform>().getRotation());
    }

    //updates light projection display
    calcLightFrusta();

    glProgramUniform1fv(IntervalProgramID, IntervalUniformID, 3, m_entities[EntityID::Camera].getComponent<cro::Camera>().getSplitDistances().data());
    glProgramUniformMatrix4fv(IntervalProgramID, IntervalCamID, 1, GL_FALSE, &m_entities[EntityID::Camera].getComponent<cro::Camera>().getPass(cro::Camera::Pass::Final).viewMatrix[0][0]);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void FrustumState::render()
{
    auto oldCam = m_gameScene.setActiveCamera(m_entities[EntityID::Camera]);
    m_cameraPreview.clear();
    m_gameScene.render();
    m_cameraPreview.display();

    m_gameScene.setActiveCamera(oldCam);
    m_gameScene.render();
    m_uiScene.render();
}

//private
void FrustumState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void FrustumState::loadAssets()
{
    m_cameraPreview.create(320, 240);
}

void FrustumState::createScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/models/cube.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, -3.f, -15.f });
        md.createModel(entity);
        entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Vis);
        m_entities[EntityID::Cube] = entity;
    }

    if (md.loadFromFile("assets/batcat/models/arrow.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 12.f, 7.f, -20.f });
        md.createModel(entity);
        entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Vis);
        m_entities[EntityID::Light] = entity;
    }

    if (md.loadFromFile("assets/batcat/models/arrow_yellow.cmt"))
    {
        for (auto i = 0; i < CascadeCount; i++)
        {
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setScale(glm::vec3(0.5f));
            md.createModel(entity);
            entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Vis);
            m_entities[EntityID::LightDummy01 + i] = entity;
        }
    }

    m_resources.shaders.loadFromString(ShaderID::Slice, SliceVertex, SliceFragment, "#define CASCADES " + std::to_string(CascadeCount) + "\n");
    auto& sliceShader = m_resources.shaders.get(ShaderID::Slice);
    LightUniformID = sliceShader.getUniformID("u_lightViewProjections[0]");
    SplitProgramID = sliceShader.getGLHandle();

    auto sliceMaterialID = m_resources.materials.add(sliceShader);
    auto sliceMaterial = m_resources.materials.get(sliceMaterialID);


    if (md.loadFromFile("assets/bush/ground_plane.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, -4.f, -15.f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
        entity.getComponent<cro::Transform>().setScale(glm::vec3(4.f));
        md.createModel(entity);
        entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Scene);



        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, -4.f, -15.f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
        entity.getComponent<cro::Transform>().setScale(glm::vec3(4.f));
        md.createModel(entity);
        entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Vis);
        entity.getComponent<cro::Model>().setMaterial(0, sliceMaterial);
    }


    m_resources.shaders.loadFromString(ShaderID::Interval, IndexVertex, IndexFragment);
    auto& intervalShader = m_resources.shaders.get(ShaderID::Interval);
    IntervalUniformID = intervalShader.getUniformID("u_viewSplits[0]");
    IntervalCamID = intervalShader.getUniformID("u_camMatrix");
    IntervalProgramID = intervalShader.getGLHandle();

    auto intervalMaterialID = m_resources.materials.add(intervalShader);
    auto intervalMaterial = m_resources.materials.get(intervalMaterialID);


    const std::array BoxPositions =
    {
        glm::vec3(5.f, -4.f, -15.f),
        glm::vec3(-5.f, -4.f, -15.f),
        glm::vec3(0.4f, -4.f, -18.f),
        glm::vec3(-1.f, -4.f, -13.f)
    };
    if (md.loadFromFile("assets/bush/player_box.cmt"))
    {
        for (auto pos : BoxPositions)
        {
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(pos);
            md.createModel(entity);
            entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Scene);

            float offset = entity.getComponent<cro::Model>().getMeshData().boundingBox[0].y;
            entity.getComponent<cro::Transform>().setOrigin({ 0.f, offset, 0.f });


            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(pos);
            md.createModel(entity);
            entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Vis);
            entity.getComponent<cro::Model>().setMaterial(0, intervalMaterial);

            offset = entity.getComponent<cro::Model>().getMeshData().boundingBox[0].y;
            entity.getComponent<cro::Transform>().setOrigin({ 0.f, offset, 0.f });
        }
    }

    auto updateView = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        auto windowSize = size;
        size.y = ((size.x / 16.f) * 9.f) / size.y;
        size.x = 1.f;

        //90 deg in x (glm expects fov in y)
        cam.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 140.f);
        cam.viewport.bottom = (1.f - size.y) / 2.f;
        cam.viewport.height = size.y;
    };

    //scene camera
    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 10.f });
    camEnt.getComponent<cro::Camera>().resizeCallback = updateView;
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlags::Vis);
    updateView(camEnt.getComponent<cro::Camera>());

    //this is the camera on the cube which we're visualising
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback = std::bind(&PerspectiveDebug::update, &perspectiveDebug, std::placeholders::_1);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(512, 512);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlags::Scene);
    perspectiveDebug.update(camEnt.getComponent<cro::Camera>());
    m_entities[EntityID::Camera] = camEnt;

    if (m_entities[EntityID::Cube].isValid())
    {
        m_entities[EntityID::Cube].getComponent<cro::Transform>().addChild(camEnt.getComponent<cro::Transform>());
    }

    //this entity draws the camera frustum. The points are updated in world
    //space so the entity has an identity transform
    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    auto materialID = m_resources.materials.add(m_resources.shaders.get(shaderID));


    for (auto i = 0; i < CascadeCount; ++i)
    {
        auto material = m_resources.materials.get(materialID);
        auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINES));
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
        entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Vis);
        m_entities[EntityID::CameraViz01 + i] = entity;        
        
        
        //create a new mesh instance...
        meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_TRIANGLES));
        material = m_resources.materials.get(materialID);
        material.blendMode = cro::Material::BlendMode::Alpha;
        material.doubleSided = true; //because I'm too lazy to wind the vertices in the correct order.
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
        entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Vis);
        m_entities[EntityID::LightViz01 + i] = entity;
    }

    calcCameraFrusta();


    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -60.f * cro::Util::Const::degToRad);

    if (m_entities[EntityID::Light].isValid())
    {
        m_entities[EntityID::Light].getComponent<cro::Transform>().setRotation(m_gameScene.getSunlight().getComponent<cro::Transform>().getRotation());
    }

    m_gameScene.setCubemap("assets/billiards/skybox/sky.ccm");
}

void FrustumState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Window of happiness"))
            {
                static bool hideLights = false;
                if (ImGui::Button("Hide Lights"))
                {
                    hideLights = !hideLights;
                    m_entities[EntityID::LightViz01].getComponent<cro::Model>().setHidden(hideLights);
                    m_entities[EntityID::LightViz02].getComponent<cro::Model>().setHidden(hideLights);
                    m_entities[EntityID::LightViz03].getComponent<cro::Model>().setHidden(hideLights);
                }

                ImGui::Image(m_cameraPreview.getTexture(), { 320.f, 240.f }, { 0.f, 1.f }, { 1.f, 0.f });

                bool doUpdate = false;
                if (ImGui::SliderFloat("Near Plane", &perspectiveDebug.nearPlane, 0.1f, perspectiveDebug.farPlane -  0.1f))
                {
                    doUpdate = true;
                }
                if (ImGui::SliderFloat("Far Plane", &perspectiveDebug.farPlane, perspectiveDebug.nearPlane + 0.1f, 100.f))
                {
                    doUpdate = true;
                }
                /*if (ImGui::SliderFloat("FOV", &perspectiveDebug.fov, 10.f, 100.f))
                {
                    doUpdate = true;
                }
                if (ImGui::SliderFloat("Aspect", &perspectiveDebug.aspectRatio, 0.1f, 4.f))
                {
                    doUpdate = true;
                }*/
                if (ImGui::SliderFloat("Max Dist", &perspectiveDebug.maxDistance, 1.f, 100.f))
                {
                    doUpdate = true;
                    m_entities[EntityID::Camera].getComponent<cro::Camera>().setMaxShadowDistance(perspectiveDebug.maxDistance);
                }
                if (ImGui::SliderFloat("Overshoot", &perspectiveDebug.overshoot, 0.f, 50.f))
                {
                    m_entities[EntityID::Camera].getComponent<cro::Camera>().setShadowExpansion(perspectiveDebug.overshoot);
                }
                const auto& splits = m_entities[EntityID::Camera].getComponent<cro::Camera>().getSplitDistances();
                ImGui::Text("Splits: %3.3f, %3.3f, %3.3f", splits[0], splits[1], splits[2]);
                
                if (ImGui::InputInt("Cascades", &CascadeCount))
                {
                    CascadeCount = std::min(3, std::max(1, CascadeCount));

                    for (auto i = 0; i < 3; ++i)
                    {
                        m_entities[EntityID::LightViz01 + i].getComponent<cro::Model>().setHidden(i >= CascadeCount);
                        m_entities[EntityID::LightDummy01 + i].getComponent<cro::Model>().setHidden(i >= CascadeCount);
                        m_entities[EntityID::CameraViz01 + i].getComponent<cro::Model>().setHidden(i >= CascadeCount);
                    }

                    doUpdate = true;
                }

                if (doUpdate)
                {
                    perspectiveDebug.update(m_entities[EntityID::Camera].getComponent<cro::Camera>());

                    calcCameraFrusta();
                }

                ImGui::Text("WASDQE:\nRotate Camera");
                ImGui::NewLine();
                ImGui::Text("Home, Del, End,\nPgDn, Ins, PgUp:\nRotate Light");
            }
            ImGui::End();     

            if (ImGui::Begin("Depth Layers"))
            {
                const auto& depthMap = m_entities[EntityID::Camera].getComponent<cro::Camera>().shadowMapBuffer;
                for (auto i = 0u; i < depthMap.getLayerCount(); ++i)
                {
                    ImGui::Image(depthMap.getTexture(i), { 128.f, 128.f }, { 0.f, 1.f }, { 1.f, 0.f });
                }
            }
            ImGui::End();
        });

    auto updateView = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -1.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_uiScene.getActiveCamera();
    camEnt.getComponent<cro::Camera>().resizeCallback = updateView;
    updateView(camEnt.getComponent<cro::Camera>());
}

void FrustumState::calcCameraFrusta()
{
    auto worldMat = m_entities[EntityID::Camera].getComponent<cro::Transform>().getWorldTransform();
    const auto& corners = m_entities[EntityID::Camera].getComponent<cro::Camera>().getFrustumSplits();

    for (auto i = 0; i < CascadeCount; ++i)
    {
        updateFrustumVis(worldMat, corners[i], m_entities[EntityID::CameraViz01 + i].getComponent<cro::Model>().getMeshData(), glm::vec4(static_cast<float>(i) / CascadeCount, 1.f - (static_cast<float>(i) / CascadeCount), 0.5f, 1.f));
    }
}

void FrustumState::calcLightFrusta()
{
    static const std::array CascadeColours =
    {
        glm::vec4(1.f, 0.f, 0.f, 0.4f),
        glm::vec4(0.f, 1.f, 0.f, 0.4f),
        glm::vec4(0.f, 0.f, 1.f, 0.4f)
    };

    //position light source
    auto light = m_gameScene.getSunlight();
    auto lightDir = -light.getComponent<cro::Sunlight>().getDirection();

#ifdef CRO_DEBUG_
    //this visualises the output of the shadow map system
    //the camera members are only included in debug builds, however
    const auto& cam = m_entities[EntityID::Camera].getComponent<cro::Camera>();
    for (auto i = 0u; i < std::min(cam.lightCorners.size(), std::size_t(CascadeCount)); ++i)
    {
        m_entities[EntityID::LightDummy01 + i].getComponent<cro::Transform>().setPosition(cam.lightPositions[i]);
        m_entities[EntityID::LightDummy01 + i].getComponent<cro::Transform>().setRotation(light.getComponent<cro::Transform>().getRotation());
        updateFrustumVis(glm::inverse(cam.getShadowViewMatrix(i)), cam.lightCorners[i], m_entities[EntityID::LightViz01 + i].getComponent<cro::Model>().getMeshData(), CascadeColours[i]);
 
        glProgramUniformMatrix4fv(SplitProgramID, LightUniformID, CascadeCount, GL_FALSE, cam.getShadowViewProjections());
    }
    
#else
    //this is actually now inplemented in the ShadowMapRenderer system.

    //frustum corners in world coords
    auto worldMat = m_entities[EntityID::Camera].getComponent<cro::Transform>().getWorldTransform();
    auto corners = m_entities[EntityID::Camera].getComponent<cro::Camera>().getFrustumSplits();

    for (auto i = 0; i < CascadeCount; ++i)
    {
        glm::vec3 centre = glm::vec3(0.f);

        for (auto& c : corners[i])
        {
            c = worldMat * c;
            centre += glm::vec3(c);
        }
        centre /= corners[i].size();

        auto lightPos = centre + lightDir;

        //just set this so we can visualise what's happening
        m_entities[EntityID::LightDummy01 + i].getComponent<cro::Transform>().setPosition(lightPos);
        m_entities[EntityID::LightDummy01 + i].getComponent<cro::Transform>().setRotation(light.getComponent<cro::Transform>().getRotation());

        const auto lightView = glm::lookAt(lightPos, centre, cro::Transform::Y_AXIS);


        //world coords to light space
        glm::vec3 minPos(std::numeric_limits<float>::max());
        glm::vec3 maxPos(std::numeric_limits<float>::lowest());

        for (const auto& c : corners[i])
        {
            const auto p = lightView * c;
            minPos.x = std::min(minPos.x, p.x);
            minPos.y = std::min(minPos.y, p.y);
            minPos.z = std::min(minPos.z, p.z);

            maxPos.x = std::max(maxPos.x, p.x);
            maxPos.y = std::max(maxPos.y, p.y);
            maxPos.z = std::max(maxPos.z, p.z);
        }
        /*minPos -= 1.f;
        maxPos += 1.f;*/ //this needs to be a variable based on the scene
        const auto lightProj = glm::ortho(minPos.x, maxPos.x, minPos.y, maxPos.y, minPos.z, maxPos.z);

        std::array lightCorners =
        {
            //near
            glm::vec4(maxPos.x, maxPos.y, minPos.z, 1.f),
            glm::vec4(minPos.x, maxPos.y, minPos.z, 1.f),
            glm::vec4(minPos.x, minPos.y, minPos.z, 1.f),
            glm::vec4(maxPos.x, minPos.y, minPos.z, 1.f),
            //far
            glm::vec4(maxPos.x, maxPos.y, maxPos.z, 1.f),
            glm::vec4(minPos.x, maxPos.y, maxPos.z, 1.f),
            glm::vec4(minPos.x, minPos.y, maxPos.z, 1.f),
            glm::vec4(maxPos.x, minPos.y, maxPos.z, 1.f)
        };

        //this is a bit more wasteful of texture space but apparently
        //is better for texel snapping
        //float radius = (glm::length(corners[i][0] - corners[i][6]) / 2.f);// *1.1f;
        //const auto lightProj = glm::ortho(-radius, radius, -radius, radius, -radius, radius);
        //std::array lightCorners =
        //{
        //    glm::vec4(radius, radius, -radius, 1.f),
        //    glm::vec4(-radius, radius, -radius, 1.f),
        //    glm::vec4(-radius, -radius, -radius, 1.f),
        //    glm::vec4(radius, -radius, -radius, 1.f),

        //    glm::vec4(radius, radius, radius, 1.f),
        //    glm::vec4(-radius, radius, radius, 1.f),
        //    glm::vec4(-radius, -radius, radius, 1.f),
        //    glm::vec4(radius, -radius, radius, 1.f),
        //};


        updateFrustumVis(glm::inverse(lightView), lightCorners, m_entities[EntityID::LightViz01 + i].getComponent<cro::Model>().getMeshData(), CascadeColours[i]);
    }
#endif
}

void FrustumState::updateFrustumVis(glm::mat4 worldMat, const std::array<glm::vec4, 8u>& corners, cro::Mesh::Data& meshData, glm::vec4 c)
{
    struct Vert final
    {
        Vert(glm::vec3 p, glm::vec4 c = glm::vec4(1.f))
            : position(p), colour(c) {}
        glm::vec3 position = glm::vec3(0.f);
        glm::vec4 colour = glm::vec4(1.f);
    };

    std::vector<Vert> vertices =
    {
        //near
        Vert(glm::vec3(worldMat * corners[0]), c),
        Vert(glm::vec3(worldMat * corners[1]), c),
        Vert(glm::vec3(worldMat * corners[2]), c),
        Vert(glm::vec3(worldMat * corners[3]), c),

        //far
        Vert(glm::vec3(worldMat * corners[4]), c),
        Vert(glm::vec3(worldMat * corners[5]), c),
        Vert(glm::vec3(worldMat * corners[6]), c),
        Vert(glm::vec3(worldMat * corners[7]), c)
    };

    static const std::vector<std::uint32_t> lineIndices =
    {
        0,1,  1,2,  2,3,  3,0,
        4,5,  5,6,  6,7,  7,4,
        0,4,  1,5,  2,6,  3,7
    };

    static const std::vector<std::uint32_t> triangleIndices =
    {
        0,1,2,  2,3,0,
        4,5,6,  4,6,7,

        1,5,6,  1,6,2,
        0,3,7,  0,7,4,

        4,5,1,  4,1,0,
        7,6,2,  7,2,3
    };

    auto* indices = meshData.primitiveType == GL_LINES ? &lineIndices : &triangleIndices;

    meshData.vertexCount = vertices.size();
    glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo);
    glBufferData(GL_ARRAY_BUFFER, meshData.vertexSize * meshData.vertexCount, vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    auto& submesh = meshData.indexData[0];
    submesh.indexCount = static_cast<std::uint32_t>(indices->size());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh.indexCount * sizeof(std::uint32_t), indices->data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    //update the bounds so depth sorting works
    meshData.boundingBox[0] = glm::vec3(std::numeric_limits<float>::max());
    meshData.boundingBox[1] = glm::vec3(std::numeric_limits<float>::lowest());
    for (const auto& vert : vertices)
    {
        //min point
        if (meshData.boundingBox[0].x > vert.position.x)
        {
            meshData.boundingBox[0].x = vert.position.x;
        }
        if (meshData.boundingBox[0].y > vert.position.y)
        {
            meshData.boundingBox[0].y = vert.position.y;
        }
        if (meshData.boundingBox[0].z > vert.position.z)
        {
            meshData.boundingBox[0].z = vert.position.z;
        }

        //maxpoint
        if (meshData.boundingBox[1].x < vert.position.x)
        {
            meshData.boundingBox[1].x = vert.position.x;
        }
        if (meshData.boundingBox[1].y < vert.position.y)
        {
            meshData.boundingBox[1].y = vert.position.y;
        }
        if (meshData.boundingBox[1].z < vert.position.z)
        {
            meshData.boundingBox[1].z = vert.position.z;
        }
    }
    meshData.boundingSphere = meshData.boundingBox;
}

bool FrustumState::rotateEntity(cro::Entity entity, std::int32_t keysetID, float dt)
{
    const float rotationSpeed = dt;
    glm::vec3 rotation(0.f);
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Down]))
    {
        rotation.x -= rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Up]))
    {
        rotation.x += rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Right]))
    {
        rotation.y -= rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Left]))
    {
        rotation.y += rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::CW]))
    {
        rotation.z -= rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::CCW]))
    {
        rotation.z += rotationSpeed;
    }

    if (glm::length2(rotation) != 0
        && entity.isValid())
    {
        auto worldMat = glm::inverse(glm::mat3(entity.getComponent<cro::Transform>().getLocalTransform()));
        entity.getComponent<cro::Transform>().rotate(/*worldMat **/ cro::Transform::X_AXIS, rotation.x);
        entity.getComponent<cro::Transform>().rotate(worldMat * cro::Transform::Y_AXIS, rotation.y);
        entity.getComponent<cro::Transform>().rotate(/*worldMat **/ cro::Transform::Z_AXIS, rotation.z);

        return true;
    }
    return false;
}