/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "ModelState.hpp"
#include "OriginIconBuilder.hpp"
#include "NormalVisMeshBuilder.hpp"
#include "GLCheck.hpp"
#include "UIConsts.hpp"
#include "SharedStateData.hpp"
#include "Messages.hpp"
#include "FpsCameraSystem.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/Keyboard.hpp>

#include <crogine/gui/Gui.hpp>

#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/GBuffer.hpp>

#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/DeferredRenderSystem.hpp>

#include <crogine/util/String.hpp>

#include <string_view>

#define LIGHTMAPPER_IMPLEMENTATION
#include "lightmapper.h"

namespace
{
#include "LightmapShader.inl"

    const std::int32_t LightmapShaderID = 1;
}

ModelState::ModelState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State            (stack, context),
    m_useDeferred           (false),
    m_sharedData            (sd),
    m_scene                 (context.appInstance.getMessageBus()),
    m_previewScene          (context.appInstance.getMessageBus()),
    m_showMaskEditor        (false),
    m_showImageCombiner     (false),
    m_showTransformModifier (false),
    m_viewportRatio         (1.f),
    m_showPreferences       (false),
    m_showGroundPlane       (false),
    m_showSkybox            (false),
    m_showMaterialWindow    (false),
    m_showBakingWindow      (false),
    m_useFreecam            (false),
    m_exportAnimation       (true),
    m_skeletonMeshID        (0),
    m_transformUpdated      (false),
    m_browseGLTF            (false),
    m_showAABB              (false),
    m_showSphere            (false),
    m_selectedTexture       (std::numeric_limits<std::uint32_t>::max()),
    m_selectedMaterial      (std::numeric_limits<std::uint32_t>::max()),
    m_attachmentIndex       (0)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        buildUI();
    });

    context.appInstance.resetFrameTime();
    context.mainWindow.setTitle("Crogine Model Importer");

    /*registerWindow([&]()
        {
            if (ImGui::Begin("buns"))
            {
                auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
                float maxDepth = cam.getMaxShadowDistance();
                if (ImGui::SliderFloat("Depth", &maxDepth, 1.f, cam.getFarPlane()))
                {
                    cam.setMaxShadowDistance(maxDepth);
                }
            }
            ImGui::End();
        });*/
}

//public
bool ModelState::handleEvent(const cro::Event& evt)
{
    if(cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    switch (evt.type)
    {
    default: break;
    case SDL_QUIT:
        if (m_entities[EntityID::ActiveModel].isValid())
        {
            showSaveMessage();
        }
        break;
    case SDL_MOUSEMOTION:
        if (!m_useFreecam)
        {
            updateMouseInput(evt);
        }
        break;
    case SDL_MOUSEWHEEL:
        if(!m_useFreecam)
    {
        m_cameras[CameraID::Default].FOV = std::min(MaxFOV, std::max(MinFOV, m_cameras[CameraID::Default].FOV - (evt.wheel.y * 0.1f)));
        m_viewportRatio = updateView(m_scene.getActiveCamera(), DefaultFarPlane, m_cameras[CameraID::Default].FOV);
    }
        break;
    }

    if (m_useFreecam)
    {
        m_scene.getSystem<FpsCameraSystem>()->handleEvent(evt);
    }

    m_previewScene.forwardEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void ModelState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateLayout(data.data0, data.data1);
            m_viewportRatio = updateView(m_cameras[CameraID::Default].camera, m_cameras[CameraID::Default].FarPlane, m_cameras[CameraID::Default].FOV);
            updateView(m_cameras[CameraID::FreeLook].camera, m_cameras[CameraID::FreeLook].FarPlane, m_cameras[CameraID::FreeLook].FOV);
        }
    }
    else if (msg.id == cro::Message::ConsoleMessage)
    {
        const auto& data = msg.getData<cro::Message::ConsoleEvent>();
        if (data.type == cro::Message::ConsoleEvent::LinePrinted)
        {
            switch (data.level)
            {
            default:
            case cro::Message::ConsoleEvent::Info:
                m_messageColour = { 0.f,1.f,0.f,1.f };
                break;
            case cro::Message::ConsoleEvent::Warning:
                m_messageColour = { 1.f,0.5f,0.f,1.f };
                break;
            case cro::Message::ConsoleEvent::Error:
                m_messageColour = { 1.f,0.f,0.f,1.f };
                break;
            }
        }
    }

    m_previewScene.forwardMessage(msg);
    m_scene.forwardMessage(msg);
}

bool ModelState::simulate(float dt)
{
    const float colourIncrease = dt;// *0.5f;
    m_messageColour.w = std::min(1.f, m_messageColour.w + colourIncrease);
    m_messageColour.x = std::min(1.f, m_messageColour.x + colourIncrease);
    m_messageColour.y = std::min(1.f, m_messageColour.y + colourIncrease);
    m_messageColour.z = std::min(1.f, m_messageColour.z + colourIncrease);

    m_previewScene.simulate(dt);
    m_scene.simulate(dt);

    if (m_entities[EntityID::ActiveModel].isValid()
        && (m_showAABB || m_showSphere))
    {
        const auto& meshData = m_entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();
        std::optional<cro::Sphere> sphere;
        if (m_showSphere) sphere = meshData.boundingSphere;

        std::optional<cro::Box> box;
        if (m_showAABB) box = meshData.boundingBox;
        updateGridMesh(m_entities[EntityID::GridMesh].getComponent<cro::Model>().getMeshData(), sphere, box);
    }
    return false;
}

void ModelState::render()
{
    //render active material preview
    if (!m_materialDefs.empty())
    {
        m_materialDefs[m_selectedMaterial].previewTexture.clear(uiConst::PreviewClearColour);
        m_previewScene.render();
        m_materialDefs[m_selectedMaterial].previewTexture.display();
    }

    m_scene.render();
}

//private
void ModelState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);
    m_scene.addSystem<cro::BillboardSystem>(mb);
    m_scene.addSystem<FpsCameraSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    if (m_useDeferred)
    {
        m_scene.addSystem<cro::DeferredRenderSystem>(mb)->setEnvironmentMap(m_environmentMap);
    }
    else
    {
        m_scene.addSystem<cro::ModelRenderer>(mb);
    }

    m_previewScene.addSystem<cro::SkeletalAnimator>(mb);
    m_previewScene.addSystem<cro::CameraSystem>(mb);

    if (m_useDeferred)
    {
        m_previewScene.addSystem<cro::DeferredRenderSystem>(mb)->setEnvironmentMap(m_environmentMap);
    }
    else
    {
        m_previewScene.addSystem<cro::ModelRenderer>(mb);
    }
}

void ModelState::loadAssets()
{
    //black texture for empty texture slots
    cro::Image img;
    img.create(2, 2, cro::Colour::Black);

    m_blackTexture.loadFromImage(img);

    //checkered texture for default material
    std::array<std::uint8_t, 16> pixels =
    {
        0,0,0,255, 255,0,255,255,
        255,0,255,255, 0,0,0,255
    };
    m_magentaTexture.create(2, 2);
    m_magentaTexture.update(pixels.data());

    if (m_sharedData.skymapTexture.empty()
        || !m_environmentMap.loadFromFile(m_sharedData.skymapTexture))
    {
        m_environmentMap.loadFromFile("assets/images/brooklyn_bridge.hdr");
    }

    auto pbrType = m_useDeferred ? cro::ShaderResource::PBRDeferred : cro::ShaderResource::PBR;


    //create a default material to display models on import
    std::uint32_t flags = cro::ShaderResource::DiffuseColour;// | cro::ShaderResource::MaskMap;
    auto shaderID = m_resources.shaders.loadBuiltIn(pbrType, flags);
    m_materialIDs[MaterialID::Default] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_colour", cro::Colour(1.f, 1.f, 1.f));
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_maskColour", cro::Colour(0.f, 0.5f, 1.f));
    if (!m_useDeferred)
    {
        m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_irradianceMap", m_environmentMap.getIrradianceMap());
        m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_prefilterMap", m_environmentMap.getPrefilterMap());
        m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_brdfMap", m_environmentMap.getBRDFMap());
    }
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).deferred = m_useDeferred;

    flags = cro::ShaderResource::DiffuseColour | cro::ShaderResource::Skinning;
    shaderID = m_resources.shaders.loadBuiltIn(pbrType, flags);
    m_materialIDs[MaterialID::DefaultSkinned] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(m_materialIDs[MaterialID::DefaultSkinned]).setProperty("u_colour", cro::Colour(1.f, 1.f, 1.f));
    m_resources.materials.get(m_materialIDs[MaterialID::DefaultSkinned]).setProperty("u_maskColour", cro::Colour(0.f, 0.5f, 1.f));
    if (!m_useDeferred)
    {
        m_resources.materials.get(m_materialIDs[MaterialID::DefaultSkinned]).setProperty("u_irradianceMap", m_environmentMap.getIrradianceMap());
        m_resources.materials.get(m_materialIDs[MaterialID::DefaultSkinned]).setProperty("u_prefilterMap", m_environmentMap.getPrefilterMap());
        m_resources.materials.get(m_materialIDs[MaterialID::DefaultSkinned]).setProperty("u_brdfMap", m_environmentMap.getBRDFMap());
    }
    m_resources.materials.get(m_materialIDs[MaterialID::DefaultSkinned]).deferred = m_useDeferred;

    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::ShadowMap, cro::ShaderResource::DepthMap);
    m_materialIDs[MaterialID::DefaultShadow] = m_resources.materials.add(m_resources.shaders.get(shaderID));

    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::ShadowMap, cro::ShaderResource::Skinning | cro::ShaderResource::DepthMap);
    m_materialIDs[MaterialID::DefaultShadowSkinned] = m_resources.materials.add(m_resources.shaders.get(shaderID));

    //used for drawing debug lines
    auto unlitType = m_useDeferred ? cro::ShaderResource::UnlitDeferred : cro::ShaderResource::Unlit;
    shaderID = m_resources.shaders.loadBuiltIn(unlitType, cro::ShaderResource::VertexColour);
    m_materialIDs[MaterialID::DebugDraw] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(m_materialIDs[MaterialID::DebugDraw]).blendMode = cro::Material::BlendMode::Alpha;

    m_skeletonMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position 
                                                                        | cro::VertexProperty::Colour, 2, GL_LINES));

    //for receiving shadows on the ground plane
    std::uint32_t texID = 10000;
    m_resources.textures.load(texID, "assets/images/grid.png");
    m_resources.textures.get(texID).setSmooth(true);

    shaderID = m_resources.shaders.loadBuiltIn(unlitType, cro::ShaderResource::DiffuseMap | cro::ShaderResource::RxShadows);
    m_materialIDs[MaterialID::GroundPlane] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(m_materialIDs[MaterialID::GroundPlane]).setProperty("u_diffuseMap", m_resources.textures.get(texID));
    m_resources.materials.get(m_materialIDs[MaterialID::GroundPlane]).setProperty("u_maskColour", cro::Colour(0.f, 0.f, 0.f));

    m_resources.shaders.loadFromString(LightmapShaderID, Lightmap::Vertex, Lightmap::Fragment);
}

void ModelState::createScene()
{
    //create grid // bounding box mesh
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    auto material = m_resources.materials.get(m_materialIDs[MaterialID::DebugDraw]);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    auto& meshData = entity.getComponent<cro::Model>().getMeshData();
    updateGridMesh(meshData, std::nullopt, std::nullopt);
    m_entities[EntityID::GridMesh] = entity;

    //create the camera - using a custom camera prevents the scene updating the projection on window resize
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(DefaultCameraPosition);
    entity.addComponent<cro::Camera>().shadowMapBuffer.create(4096, 4096);
    entity.getComponent<cro::Camera>().setMaxShadowDistance(50.f);
    entity.getComponent<cro::Camera>().setShadowExpansion(30.f);
    m_viewportRatio = updateView(entity, DefaultFarPlane, DefaultFOV);
    m_scene.setActiveCamera(entity);
    m_cameras[CameraID::Default].camera = entity;

    m_entities[EntityID::RootNode] = m_scene.createEntity();
    m_entities[EntityID::RootNode].addComponent<cro::Transform>();
    //entities[EntityID::RootNode].addComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_entities[EntityID::RootNode].getComponent<cro::Transform>().addChild(m_entities[EntityID::GridMesh].getComponent<cro::Transform>());

    //axis icon
    meshID = m_resources.meshes.loadMesh(OriginIconBuilder());
    material = m_resources.materials.get(m_materialIDs[MaterialID::DebugDraw]);
    material.enableDepthTest = false;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_cameras[CameraID::Default].FOV / DefaultFOV;
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
    };
    m_entities[EntityID::RootNode].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //ground plane for receiving shadows
    std::vector<float> verts =
    {
        -1.5f, 0.f, -1.5f, 0.f, 1.f, 0.f,  0.f, 1.f,
        -1.5f, 0.f, 1.5f,  0.f, 1.f, 0.f,  0.f, 0.f,
        1.5f, 0.f, -1.5f,  0.f, 1.f, 0.f,  1.f, 1.f,
        1.5f, 0.f, 1.5f,   0.f, 1.f, 0.f,  1.f, 0.f,
    };
    std::vector<std::uint32_t> indices =
    {
        0, 1, 2, 3
    };
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Normal | cro::VertexProperty::UV0, 1, GL_TRIANGLE_STRIP));

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(m_materialIDs[MaterialID::GroundPlane]));
    auto* mesh = &entity.getComponent<cro::Model>().getMeshData();

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indexData[0].ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    mesh->boundingBox[0] = { -1.5f, 0.25f, -1.5f };
    mesh->boundingBox[1] = { 1.5f, -0.25f, 1.5f };
    mesh->boundingSphere.radius = 1.5f;
    mesh->vertexCount = 4;
    mesh->indexData[0].indexCount = 4;

    m_entities[EntityID::GroundPlane] = entity;
    m_entities[EntityID::RootNode].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //set the default sunlight properties
    m_scene.getSunlight().getComponent<cro::Transform>().setPosition({ -1.5f, 1.5f, 1.5f });
    m_scene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -0.79f);
    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.79f);

    m_entities[EntityID::RootNode].getComponent<cro::Transform>().addChild(m_scene.getSunlight().getComponent<cro::Transform>());

    cro::ModelDefinition def(m_resources);
    def.loadFromFile("assets/models/arrow.cmt", m_useDeferred);
    //def.createModel(m_scene.getSunlight());
    //m_scene.getSunlight().getComponent<cro::Model>().setMaterialProperty(0, "u_maskColour", cro::Colour(1.f, 1.f, 0.f, 1.f));

    m_entities[EntityID::ArcBall] = m_scene.createEntity();
    m_entities[EntityID::ArcBall].addComponent<cro::Transform>().setPosition(DefaultArcballPosition);
    m_entities[EntityID::ArcBall].getComponent<cro::Transform>().addChild(m_scene.getActiveCamera().getComponent<cro::Transform>());


    //highlights the selected joint when editing animation events
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);

    mesh = &entity.getComponent<cro::Model>().getMeshData();
    verts = { 0.f,0.f,0.f, 0.f,1.f,1.f,1.f };
    indices = { 0 };

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indexData[0].ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    mesh->boundingBox[0] = { -0.5f, 0.25f, -0.5f };
    mesh->boundingBox[1] = { 0.5f, -0.25f, 0.5f };
    mesh->boundingSphere.radius = 0.5f;
    mesh->vertexCount = 1;
    mesh->indexData[0].indexCount = 1;

    entity.getComponent<cro::Model>().setHidden(true);
    m_entities[EntityID::JointNode] = entity;
    m_entities[EntityID::RootNode].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    if (m_useDeferred)
    {
        m_scene.getActiveCamera().addComponent<cro::GBuffer>();
    }

    //create the material preview scene
    cro::ModelDefinition modelDef(m_resources);
    modelDef.loadFromFile("assets/models/preview.cmt", m_useDeferred);
    entity = m_previewScene.createEntity();
    entity.addComponent<cro::Transform>();
    modelDef.createModel(entity);

    m_previewEntity = entity;

    //camera
    entity = m_previewScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 2.f });
    entity.addComponent<cro::GBuffer>();
    entity.addComponent<cro::Camera>();
    auto& cam3D = entity.getComponent<cro::Camera>();
    cam3D.setPerspective(DefaultFOV, 1.f, 0.1f, 10.f, 3);
    m_previewScene.setActiveCamera(entity);

    //not rendering shadows on here, but we still want a light direction
    m_previewScene.getSunlight().getComponent<cro::Sunlight>().setDirection({ 0.5f, -0.5f, -0.5f });

    m_scene.setCubemap(m_environmentMap);
    m_previewScene.setCubemap(m_environmentMap);


    //set up free cam
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(DefaultCameraPosition);
    entity.addComponent<FpsCamera>();
    entity.addComponent<cro::Camera>().shadowMapBuffer.create(4096, 4096);
    entity.getComponent<cro::Camera>().setShadowExpansion(50.f);
    entity.getComponent<cro::Camera>().setMaxShadowDistance(80.f);
    updateView(entity, DefaultFarPlane * 3.f, DefaultFOV);

    m_cameras[CameraID::FreeLook].FarPlane = DefaultFarPlane * 3.f;
    m_cameras[CameraID::FreeLook].camera = entity;


    //default model for previewing attachment points
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    if (modelDef.loadFromFile("assets/models/gizmoo.cmt"))
    {
        modelDef.createModel(entity);
        m_attachmentModels.push_back(entity);
        entity.getComponent<cro::Model>().setHidden(true);
        entity.setLabel("Default");
    }
    else
    {
        m_scene.destroyEntity(entity);
    }
}

void ModelState::toggleFreecam()
{
    m_useFreecam = !m_useFreecam;
    m_scene.setActiveCamera(m_useFreecam ? m_cameras[CameraID::FreeLook].camera : m_cameras[CameraID::Default].camera);
}

void ModelState::loadPrefs()
{
    cro::ConfigFile prefs;
    if (prefs.loadFromFile(cro::App::getPreferencePath() + "model_viewer.cfg"))
    {
        const auto& props = prefs.getProperties();
        for (const auto& prop : props)
        {
            auto name = cro::Util::String::toLower(prop.getName());
            if (name == "show_groundplane")
            {
                m_showGroundPlane = prop.getValue<bool>();
                if (m_showGroundPlane)
                {
                    m_entities[EntityID::GroundPlane].getComponent<cro::Transform>().setScale(glm::vec3(1.f));
                }
                else
                {
                    m_entities[EntityID::GroundPlane].getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                }
            }
            else if (name == "show_skybox")
            {
                m_showSkybox = prop.getValue<bool>();
                if (m_showSkybox)
                {
                    m_scene.enableSkybox();
                }
            }
            else if (name == "show_material")
            {
                m_showMaterialWindow = prop.getValue<bool>();
            }
            else if (name == "import_dir")
            {
                m_preferences.lastImportDirectory = prop.getValue<std::string>();
            }
            else if (name == "export_dir")
            {
                m_preferences.lastExportDirectory = prop.getValue<std::string>();
            }
            else if (name == "model_dir")
            {
                m_preferences.lastModelDirectory = prop.getValue<std::string>();
            }
            else if (name == "sky_colour")
            {
                getContext().appInstance.setClearColour(prop.getValue<cro::Colour>());
            }
        }
    }
}

void ModelState::savePrefs()
{
    auto toString = [](cro::Colour c)->std::string
    {
        return std::to_string(c.getRed()) + ", " + std::to_string(c.getGreen()) + ", " + std::to_string(c.getBlue()) + ", " + std::to_string(c.getAlpha());
    };

    cro::ConfigFile prefsOut;
    prefsOut.addProperty("show_groundplane").setValue(m_showGroundPlane);
    prefsOut.addProperty("show_skybox").setValue(m_showSkybox);
    prefsOut.addProperty("show_material").setValue(m_showMaterialWindow);
    prefsOut.addProperty("sky_colour").setValue(getContext().appInstance.getClearColour());

    prefsOut.addProperty("import_dir", m_preferences.lastImportDirectory);
    prefsOut.addProperty("export_dir", m_preferences.lastExportDirectory);
    prefsOut.addProperty("model_dir", m_preferences.lastModelDirectory);

    if (prefsOut.save(cro::App::getPreferencePath() + "model_viewer.cfg"))
    {
        //notify so the global prefs are also written
        auto* msg = getContext().appInstance.getMessageBus().post<UIEvent>(MessageID::UIMessage);
        msg->type = UIEvent::WrotePreferences;
    }
}

void ModelState::updateNormalVis()
{
    //TODO this technically works but only on imported previews where m_importedVBO is populated.
    /*
    This is actually not what we want - rather it should only be displayed on the loaded model. If we
    track the loaded model file name (which ought to be in m_currentModelConfig) we can load the VBO
    data from there when needed (and check not already loaded etc to facilitate show/hiding already 
    loaded data). This also means that the entity used only needs a single 'DynamicMesh' where we update
    the vertex data each time instead of re-creating a new mesh entirely. Also means we can ditch the
    normal mesh vis class.
    */


    if (m_entities[EntityID::ActiveModel].isValid())
    {
        //if (entities[EntityID::NormalVis].isValid())
        //{
        //    m_scene.destroyEntity(entities[EntityID::NormalVis]);
        //}

        //const auto& meshData = entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();

        ////pass to mesh builder - TODO we would be better recycling the VBO with new vertex data, rather than
        ////destroying and creating a new one (unique instances will build up in the resource manager)
        //auto meshID = m_resources.meshes.loadMesh(NormalVisMeshBuilder(meshData, m_modelProperties.vertexData));

        //entities[EntityID::NormalVis] = m_scene.createEntity();
        //entities[EntityID::NormalVis].addComponent<cro::Transform>();
        //entities[EntityID::NormalVis].addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(materialIDs[MaterialID::DebugDraw]));
        //entities[EntityID::RootNode].getComponent<cro::Transform>().addChild(entities[EntityID::NormalVis].getComponent<cro::Transform>());
    }
}

void ModelState::updateMouseInput(const cro::Event& evt)
{
    const float moveScale = 0.004f;
    if (evt.motion.state & SDL_BUTTON_LMASK)
    {
        float pitchMove = static_cast<float>(evt.motion.yrel)* moveScale * m_viewportRatio;
        float yawMove = static_cast<float>(evt.motion.xrel)* moveScale;

        auto& tx = m_entities[EntityID::ArcBall].getComponent<cro::Transform>();
        tx.rotate(cro::Transform::X_AXIS, -pitchMove);
        tx.rotate(cro::Transform::Y_AXIS, -yawMove);
    }
    else if (evt.motion.state & SDL_BUTTON_RMASK)
    {
        //do roll
        float rollMove = static_cast<float>(-evt.motion.xrel)* moveScale;

        auto& tx = m_entities[EntityID::ArcBall].getComponent<cro::Transform>();
        tx.rotate(cro::Transform::Z_AXIS, -rollMove);
    }
    else if (evt.motion.state & SDL_BUTTON_MMASK)
    {
        auto& tx = m_entities[EntityID::ArcBall].getComponent<cro::Transform>();
        tx.move(tx.getRightVector() * -static_cast<float>(evt.motion.xrel) / 160.f);
        tx.move(tx.getUpVector() * static_cast<float>(evt.motion.yrel) / 160.f);
    }
}

void ModelState::updateGridMesh(cro::Mesh::Data& meshData, std::optional<cro::Sphere> sphere, std::optional<cro::Box> boundingBox)
{
    std::vector<float> verts;
    auto addVert = [&verts](glm::vec3 pos, glm::vec4 colour)
    {
        //pos
        verts.push_back(pos.x);
        verts.push_back(pos.y);
        verts.push_back(pos.z);

        //colour
        verts.push_back(colour.r);
        verts.push_back(colour.g);
        verts.push_back(colour.b);
        verts.push_back(colour.a);
    };    
    
    //yeah we don't really need the middle 4 verts
    //but it's just easier to build in a loop
    const float GridWidth = 3.f;
    const glm::vec4 grey(0.5f, 0.5f, 0.5f, 1.f);
    for (auto y = 0; y < 4; ++y)
    {
        for (auto x = 0; x < 4; ++x)
        {
            addVert({ static_cast<float>(x) - (GridWidth / 2.f) , 0.f, static_cast<float>(y) - (GridWidth / 2.f) }, grey);
        }
    }
    std::vector<std::uint32_t> indices =
    {
        0,3,15,12,0,
        1,13,14,2,3,
        7,4,8,11
    };

    auto vertStride = (meshData.vertexSize / sizeof(float));
    std::vector<float> lastVert(verts.end() - vertStride, verts.end());
    lastVert.back() = 0.f; //set alpha to zero. problem with vert colours means we dupe verts, but mehhhhh

    auto currentIndex = static_cast<std::uint32_t>(verts.size() / vertStride);

    const glm::vec4 transparent(0.f);

    if (sphere)
    {
        const glm::vec4 green(0.1f, 0.9f, 0.4f, 1.f);
        constexpr std::int32_t SegmentCount = 16;
        constexpr float step = cro::Util::Const::TAU / SegmentCount;
        cro::Sphere rad = sphere.value();

        std::vector<glm::vec2> points;
        for (auto i = 0; i < SegmentCount; ++i)
        {
            points.emplace_back(std::sin(i * step), std::cos(i * step));
            points.back() *= rad.radius;
        }

        //draw the circle in all 3 planes
        //starting with transparent segment
        verts.insert(verts.end(), lastVert.begin(), lastVert.end());
        indices.push_back(currentIndex++);

        auto vertsStart = verts.size();
        addVert({ points.front().x, points.front().y, 0.f }, transparent);
        indices.push_back(currentIndex++);

        for (auto p : points)
        {
            addVert({ p.x, p.y, 0.f }, green);
            indices.push_back(currentIndex++);
        }
        //close the circle
        indices.push_back(currentIndex - static_cast<std::uint32_t>(points.size()));

        //transparent segment
        addVert({ points.back().x, points.back().y, 0.f }, transparent);
        indices.push_back(currentIndex++);


        addVert({ points.front().x, 0.f, points.front().y }, transparent);
        indices.push_back(currentIndex++);

        for (auto p : points)
        {
            addVert({ p.x, 0.f, p.y }, green);
            indices.push_back(currentIndex++);
        }
        //close the circle
        indices.push_back(currentIndex - static_cast<std::uint32_t>(points.size()));

        //transparent segment
        addVert({ points.back().x, 0.f, points.back().y }, transparent);
        indices.push_back(currentIndex++);


        addVert({ 0.f, points.front().x, points.front().y }, transparent);
        indices.push_back(currentIndex++);

        for (auto p : points)
        {
            addVert({ 0.f, p.x, p.y }, green);
            indices.push_back(currentIndex++);
        }
        //close the circle
        indices.push_back(currentIndex - static_cast<std::uint32_t>(points.size()));

        //update lastVert in case we're also building an AABB
        lastVert = { verts.end() - vertStride, verts.end() };
        lastVert.back() = 0.f;

        //update all the verts with centre offset
        for (auto i = vertsStart; i < verts.size(); i += vertStride)
        {
            verts[i] += rad.centre.x;
            verts[i+1] += rad.centre.y;
            verts[i+2] += rad.centre.z;
        }
    }

    if (boundingBox)
    {
        const glm::vec4 red(0.8f, 0.3f, 0.2f, 1.f);
        cro::Box box = boundingBox.value();

        //transparent seg
        verts.insert(verts.end(), lastVert.begin(), lastVert.end());
        indices.push_back(currentIndex++);

        addVert(box[0], transparent);
        indices.push_back(currentIndex++);

        //create the 8 verts
        addVert(box[0], red);
        addVert({ box[1].x, box[0].y, box[0].z }, red);
        addVert({ box[0].x, box[0].y, box[1].z }, red);
        addVert({ box[1].x, box[0].y, box[1].z }, red);

        addVert({ box[0].x, box[1].y, box[0].z }, red);
        addVert({ box[1].x, box[1].y, box[0].z }, red);
        addVert({ box[0].x, box[1].y, box[1].z }, red);
        addVert(box[1], red);


        //then add the indices
        std::vector<std::uint32_t> boxIndices =
        {
            0,1,3,2,0,  4,5,7,6,4,6,  2,3,7,5,1
        };
        for (auto i : boxIndices)
        {
            indices.push_back(i + currentIndex);
        }
    }


    meshData.vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData.vertexSize * meshData.vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto& submesh = meshData.indexData[0];
    submesh.indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh.indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

void ModelState::bakeLightmap()
{
    const auto drawScene = [&](glm::mat4 viewMat, glm::mat4 projMat, const cro::Mesh::Data& meshData)
    {
        CRO_ASSERT(m_entities[EntityID::ActiveModel].isValid(), "");

        glCheck(glEnable(GL_DEPTH_TEST));

        const auto& shader = m_resources.shaders.get(LightmapShaderID);
        glCheck(glUseProgram(shader.getGLHandle()));
        glCheck(glUniformMatrix4fv(shader.getUniformID("u_projectionMatrix"), 1, GL_FALSE, &projMat[0][0]));
        glCheck(glUniformMatrix4fv(shader.getUniformID("u_viewMatrix"), 1, GL_FALSE, &viewMat[0][0]));

        for (auto i = 0u; i < meshData.submeshCount; ++i)
        {
            const auto& indexData = meshData.indexData[i];
            glCheck(glBindVertexArray(indexData.vao[cro::Mesh::IndexData::Final]));
            glCheck(glDrawElements(static_cast<GLenum>(indexData.primitiveType), indexData.indexCount, static_cast<GLenum>(indexData.format), 0));
        }

        glCheck(glUseProgram(0));
        glCheck(glDisable(GL_DEPTH_TEST));
    };

    lm_context* ctx = lmCreate(
        64,               // hemicube rendering resolution/quality
        0.001f, 100.0f,   // zNear, zFar
        1.0f, 1.0f, 1.0f, // sky/clear colour
        2, 0.01f,         // hierarchical selective interpolation for speedup (passes, threshold)
        0.0f);            // modifier for camera-to-surface distance for hemisphere rendering.
                          // tweak this to trade-off between interpolated vertex normal quality and other artifacts (see declaration).
    if (!ctx)
    {
        LogE << "Failed creating lightmapper" << std::endl;
        return;
    }


    const auto& meshData = m_entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();

    //make sure we have enough buffers / output textures
    if (m_lightmapBuffers.size() < meshData.submeshCount)
    {
        for (auto i = m_lightmapBuffers.size(); i < meshData.submeshCount; ++i)
        {
            m_lightmapBuffers.emplace_back(LightmapSize * LightmapSize * 3 * sizeof(float));
            m_lightmapTextures.emplace_back(std::make_unique<cro::Texture>())->create(LightmapSize, LightmapSize, cro::ImageFormat::RGB);
            m_lightmapTextures.back()->setSmooth(true);
        }
    }

    glm::mat4 modelMatrix = glm::mat4(1.f);
    glm::mat4 view = glm::mat4(1.f);
    glm::mat4 proj = glm::mat4(1.f);
    
    std::size_t normalOffset = 0;
    std::size_t uvOffset = 0;

    for (auto i = 0u; i < meshData.attributes.size(); ++i)
    {
        if (i < cro::Mesh::Normal)
        {
            normalOffset += meshData.attributes[i];
        }

        if (i < cro::Mesh::UV0)
        {
            uvOffset += meshData.attributes[i];
        }
    }
    normalOffset *= sizeof(float);
    uvOffset *= sizeof(float);

    cro::Clock timer; //push progress update

    //std::int32_t bounces = 2;
    //for (auto b = 0; b < bounces; b++)
    {
        //render all geometry to their lightmaps
        for (auto i = 0u; i < meshData.submeshCount; i++)
        {
            //fill black
            std::fill(m_lightmapBuffers[i].begin(), m_lightmapBuffers[i].end(), 0.f);

            lmSetTargetLightmap(ctx, m_lightmapBuffers[i].data(), LightmapSize, LightmapSize, 3);

            lmSetGeometry(ctx, &modelMatrix[0][0],
                LM_FLOAT, (uint8_t*)m_modelProperties.vertexData.data(), meshData.vertexSize, //position
                LM_FLOAT, (uint8_t*)m_modelProperties.vertexData.data() + normalOffset, meshData.vertexSize, //normal
                LM_FLOAT, (uint8_t*)m_modelProperties.vertexData.data() + uvOffset, meshData.vertexSize, //UV - TODO select which set to use
                meshData.indexData[i].indexCount, GL_UNSIGNED_INT, m_modelProperties.indexData[i].data());

            GLint vp[4];            
            while (lmBegin(ctx, vp, &view[0][0], &proj[0][0]))
            {
                //don't glClear on your own here!
                glCheck(glViewport(vp[0], vp[1], vp[2], vp[3]));
                drawScene(view, proj, meshData);
                if (timer.elapsed().asSeconds() > 1.f)
                {
                    printf("\r%6.2f%%", lmProgress(ctx) * 100.0f);
                    timer.restart();
                }
                lmEnd(ctx);
            }
        }

        //// postprocess and upload all lightmaps to the GPU now to use them for indirect lighting during the next bounce.
        //for (int i = 0; i < meshes; i++)
        //{
        //    // you can also call other lmImage* here to postprocess the lightmap.
        //    lmImageDilate(mesh[i].lightmap, temp, mesh[i].lightmapWidth, mesh[i].lightmapHeight, 3);
        //    lmImageDilate(temp, mesh[i].lightmap, mesh[i].lightmapWidth, mesh[i].lightmapHeight, 3);

        //    glBindTexture(GL_TEXTURE_2D, mesh[i].lightmapHandle);
        //    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mesh[i].lightmapWidth, mesh[i].lightmapHeight, 0, GL_RGB, GL_FLOAT, data);
        //}
    }

    lmDestroy(ctx);

    //gamma correct and upload lightmaps to texture
    std::vector<float> temp;
    for (auto i = 0u; i < meshData.submeshCount; ++i)
    {
        temp.resize(m_lightmapBuffers[i].size());
        lmImageSmooth(m_lightmapBuffers[i].data(), temp.data(), LightmapSize, LightmapSize, 3);
        lmImageDilate(temp.data(), m_lightmapBuffers[i].data(), LightmapSize, LightmapSize, 3);
        lmImagePower(m_lightmapBuffers[i].data(), LightmapSize, LightmapSize, 3, 1.0f / 2.2f);

        glCheck(glBindTexture(GL_TEXTURE_2D, m_lightmapTextures[i]->getGLHandle()));
        glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, LightmapSize, LightmapSize, 0, GL_RGB, GL_FLOAT, m_lightmapBuffers[i].data()));
        //lmImageSaveTGAf(mesh[i].lightmapFilename, mesh[i].lightmap, mesh[i].lightmapWidth, mesh[i].lightmapHeight, 3);
    }
    glCheck(glBindTexture(GL_TEXTURE_2D, 0));

    //reset the background
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
}