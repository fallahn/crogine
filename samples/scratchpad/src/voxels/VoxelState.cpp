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

#include "VoxelState.hpp"
#include "FpsCameraSystem.hpp"
#include "../StateIDs.hpp"

#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/Mouse.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/Image.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/gui/Gui.hpp>

#include <polyvox/MarchingCubesSurfaceExtractor.h>
#include <polyvox/VolumeResampler.h>

#include "../ErrorCheck.hpp"

namespace
{
    glm::vec3 cursorWorldPos = glm::vec3(0.f);
}

VoxelState::VoxelState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus(), 512),
    m_textureBuffer     (static_cast<std::size_t>(Voxel::MapSize.x * Voxel::MapSize.y)),
    m_activeLayer       (Layer::Terrain),
    m_voxelVolume       (pv::Region(pv::Vector3DInt32(0), pv::Vector3DInt32(Voxel::IslandSize.x, Voxel::IslandSize.y, Voxel::IslandSize.z))),
    m_terrainBuffer     (static_cast<std::size_t>(Voxel::MapSize.x * Voxel::MapSize.y)),
    m_showBrushWindow   (true),
    m_showLayerWindow   (false),
    m_drawTopView       (true)
{
    std::fill(m_showLayer.begin(), m_showLayer.end(), true);
    std::fill(m_textureBuffer.begin(), m_textureBuffer.end(), glm::vec4(0.f, 0.f, 0.f, 1.f));

    loadSettings();    
    buildScene();

    registerWindow([&]()
        {
            drawMenuBar();
            drawLayerWindow();
            drawBrushWindow();
            drawTopView();
#ifdef CRO_DEBUG_
            if (ImGui::Begin("Debug"))
            {
                auto camVec = m_scene.getActiveCamera().getComponent<cro::Transform>().getForwardVector();

                ImGui::Image(m_terrainTexture, { 320.f, 200.f }, { 0.f, 1.f }, { 1.f, 0.f });
                ImGui::Text("Cursor Pos: %3.3f, %3.3f, %3.3f", cursorWorldPos.x, cursorWorldPos.y, cursorWorldPos.z);
            }
            ImGui::End();
#endif
        });
}

//public
bool VoxelState::handleEvent(const cro::Event& evt)
{
    m_scene.getSystem<VoxelFpsCameraSystem>()->handleEvent(evt);

    /*if (cro::ui::wantsKeyboard() || cro::ui::wantsMouse())
    {
        return false;
    }*/

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_l:
        {
        }
        break;
        case SDLK_p:

            break;
        case SDLK_BACKSPACE:
            requestStackPop();
            requestStackPush(States::ScratchPad::MainMenu);
            saveSettings();
            break;
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        handleKeyboardShortcut(evt.key);
    }
    else if (evt.type == SDL_QUIT)
    {
        saveSettings();
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        updateCursorPosition();

        if (evt.motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK))
        {
            applyEdit();
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_MIDDLE)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
        else if (evt.button.button == SDL_BUTTON_LEFT)
        {
            applyEdit();
        }
        else if(evt.button.button == SDL_BUTTON_RIGHT)
        {
            m_brush.editMode *= -1;
            applyEdit();
        }
    }
    else if(evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_MIDDLE)
        {
            cro::App::getWindow().setMouseCaptured(false);
        }
        else if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            m_brush.editMode *= -1;
        }
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        if (cro::Keyboard::isKeyPressed(SDLK_LSHIFT))
        {
            float scale = m_cursor.getComponent<cro::Transform>().getScale().x;
            if (evt.wheel.y > 0)
            {
                scale = std::min(Voxel::MaxCursorScale, scale + (evt.wheel.y * Voxel::CursorScaleStep));
            }
            else if (evt.wheel.y < 0)
            {
                scale = std::max(Voxel::MinCursorScale, scale + (evt.wheel.y * Voxel::CursorScaleStep));
            }
            m_cursor.getComponent<cro::Transform>().setScale(glm::vec3(scale));
        }
    }

    m_scene.forwardEvent(evt);

    return false;
}

void VoxelState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool VoxelState::simulate(float dt)
{
    if (m_scene.getSystem<VoxelFpsCameraSystem>()->hasInput())
    {
        updateCursorPosition();
    }

    m_scene.simulate(dt);

    return false;
}

void VoxelState::render()
{
    m_scene.render();

    if (m_drawTopView)
    {
        auto oldCam = m_scene.setActiveCamera(m_overviewCam);
        m_overviewImage.clear(cro::Colour::CornflowerBlue);
        m_scene.render();
        m_overviewImage.display();
        m_scene.setActiveCamera(oldCam);
    }
}

//private
void VoxelState::buildScene()
{
 /*   auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CallbackSystem>(mb);
    //m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<VoxelFpsCameraSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);

    m_scene.setWaterLevel(Voxel::WaterLevel);

    m_environmentMap.loadFromFile("assets/images/hills.hdr");
    //TODO set skybox if we move this project to golf

    m_terrainTexture.create(static_cast<std::uint32_t>(Voxel::MapSize.x), static_cast<std::uint32_t>(Voxel::MapSize.y));
    //modify the texture to accept float data - be careful not to modify/recreate this texture anywhere!!
    auto size = m_terrainTexture.getSize();
    glCheck(glBindTexture(GL_TEXTURE_2D, m_terrainTexture.getGLHandle()));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_FLOAT, m_textureBuffer.data()));
    glCheck(glBindTexture(GL_TEXTURE_2D, 0));

    glm::uvec2 imageSize(Voxel::MapSize);
    m_overviewImage.create(imageSize.x, imageSize.y);


    //cameras
    auto updateView = [](cro::Camera& cam)
    {
        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        cam.setPerspective(70.f * cro::Util::Const::degToRad, winSize.x / winSize.y, 0.1f, Voxel::MapSize.x);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_scene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition({ Voxel::MapSize.x, 80.f, -Voxel::MapSize.y / 2.f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -30.f * cro::Util::Const::degToRad);

    camEnt.addComponent<cro::Camera>().resizeCallback = updateView;
    //camEnt.getComponent<cro::Camera>().reflectionBuffer.create(1024, 1024);
    //camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<VoxelFpsCamera>().controllerIndex = 0;
    updateView(camEnt.getComponent<cro::Camera>());
    m_scene.setActiveCamera(camEnt);



    auto updateTopView = [](cro::Camera& cam)
    {
        glm::vec2 viewSize(Voxel::MapSize / 2.f);
        cam.setOrthographic(-viewSize.y, viewSize.y, -viewSize.x, viewSize.x, 0.f, 20.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    camEnt = m_scene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition({ Voxel::MapSize.x / 2.f, 18.f, -Voxel::MapSize.y / 2.f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    camEnt.addComponent<cro::Camera>().resizeCallback = updateTopView;
    camEnt.getComponent<cro::Camera>().isStatic = true;
    updateTopView(camEnt.getComponent<cro::Camera>());
    m_overviewCam = camEnt;

    //sun direction
    auto sunEnt = m_scene.getSunlight();
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -65.f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -35.f * cro::Util::Const::degToRad);

    createLayers();*/
}

void VoxelState::createLayers()
{
    //water plane
/*    cro::ModelDefinition md(m_resources, &m_environmentMap);
    md.loadFromFile("assets/voxels/models/ground_plane.cmt");

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    entity.getComponent<cro::Transform>().setPosition({ Voxel::MapSize.x / 2.f, Voxel::WaterLevel, -Voxel::MapSize.y / 2.f });
    md.createModel(entity);
    entity.getComponent<cro::Model>().setHidden(!m_showLayer[Layer::Water]);
    m_layers[Layer::Water] = entity;


    //terrain mesh
    m_shaderIDs[Shader::Terrain] = m_resources.shaders.loadBuiltIn(cro::ShaderResource::VertexLit, cro::ShaderResource::VertexColour);
    m_materialIDs[Material::Terrain] = m_resources.materials.add(m_resources.shaders.get(m_shaderIDs[Shader::Terrain]));

    auto material = m_resources.materials.get(m_materialIDs[Material::Terrain]);
    material.setProperty("u_maskColour", cro::Colour::Red);

    auto flags = cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::Normal;
    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(flags, 1, GL_TRIANGLE_STRIP));

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, Voxel::TerrainLevel, 0.f });
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().setHidden(!m_showLayer[Layer::Terrain]);
    m_layers[Layer::Terrain] = entity;


    //terrain vertex data
    for (auto i = 0u; i < m_terrainBuffer.size(); ++i)
    {
        std::size_t x = i % static_cast<std::int32_t>(Voxel::MapSize.x);
        std::size_t y = i / static_cast<std::int32_t>(Voxel::MapSize.x);

        m_terrainBuffer[i].position = { static_cast<float>(x), 0.f, -static_cast<float>(y)};
        //terrainBuffer[i].colour = theme.grassColour.getVec4();
    }

    constexpr auto xCount = static_cast<std::uint32_t>(Voxel::MapSize.x);
    constexpr auto yCount = static_cast<std::uint32_t>(Voxel::MapSize.y);
    std::vector<std::uint32_t> indices;

    for (auto y = 0u; y < yCount - 1; ++y)
    {
        if (y > 0)
        {
            indices.push_back((y + 1) * xCount);
        }

        for (auto x = 0u; x < xCount; x++)
        {
            indices.push_back(((y + 1) * xCount) + x);
            indices.push_back((y * xCount) + x);
        }

        if (y < yCount - 2)
        {
            indices.push_back((y * xCount) + (xCount - 1));
        }
    }

    auto* meshData = &entity.getComponent<cro::Model>().getMeshData();
    meshData->vertexCount = static_cast<std::uint32_t>(m_terrainBuffer.size());
    meshData->boundingBox[0] = glm::vec3(0.f, -10.f, 0.f);
    meshData->boundingBox[1] = glm::vec3(Voxel::MapSize.x, 10.f, -Voxel::MapSize.y);
    meshData->boundingSphere = meshData->boundingBox;

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(TerrainVertex) * m_terrainBuffer.size(), m_terrainBuffer.data(), GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));



    //voxel mesh
    m_shaderIDs[Shader::VoxelPreview] = m_resources.shaders.loadBuiltIn(cro::ShaderResource::VertexLit, cro::ShaderResource::VertexColour);
    m_materialIDs[Material::VoxelPreview] = m_resources.materials.add(m_resources.shaders.get(m_shaderIDs[Shader::VoxelPreview]));

    material = m_resources.materials.get(m_materialIDs[Material::VoxelPreview]);
    material.setProperty("u_maskColour", cro::Colour::Red);

    //root node for layer
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 20.f, -0.5f, -190.f });
    m_layers[Layer::Voxel] = entity;

    flags = cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::Normal;

    //then for each chunk
    for (auto z = 0; z < Voxel::ChunkCount.z; ++z)
    {
        for (auto x = 0; x < Voxel::ChunkCount.x; ++x)
        {
            meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(flags, 1, GL_TRIANGLES));
            entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec3(x * Voxel::ChunkSize.x, 0, z * Voxel::ChunkSize.z));
            entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
            entity.getComponent<cro::Model>().setHidden(!m_showLayer[Layer::Voxel]);
            m_layers[Layer::Voxel].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_chunks.push_back(entity);
        }
    }

    resetVolume();

    //cursor circle
    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseColour);
    auto* shader = &m_resources.shaders.get(shaderID);

    auto materialID = m_resources.materials.add(*shader);
    material = m_resources.materials.get(materialID);
    material.setProperty("u_colour", Voxel::LayerColours[m_activeLayer]);

    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position, 1, GL_LINE_STRIP));

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().setHidden(!m_showLayer[Layer::Terrain]);
    m_cursor = entity;

    indices.clear();

    std::vector<glm::vec3> circleVerts;
    constexpr std::uint32_t PointCount = 36;
    constexpr float SegmentSize = cro::Util::Const::TAU / PointCount;
    for (auto i = 0u; i < PointCount; ++i)
    {
        const float angle = i * SegmentSize;
        float x = std::sin(angle) * Voxel::CursorRadius;
        float z = -std::cos(angle) * Voxel::CursorRadius;

        circleVerts.emplace_back(x, 0.f, z);

        indices.push_back(i);
    }
    indices.push_back(0);

    auto circleStart = static_cast<std::uint32_t>(circleVerts.size());
    for (auto i = 0u; i < PointCount; ++i)
    {
        const float angle = i * SegmentSize;
        float y = std::sin(angle) * Voxel::CursorRadius;
        float z = -std::cos(angle) * Voxel::CursorRadius;

        circleVerts.emplace_back(0.f, y, z);

        indices.push_back(i + circleStart);
    }
    indices.push_back(circleStart);

    meshData = &entity.getComponent<cro::Model>().getMeshData();
    meshData->vertexCount = PointCount;
    meshData->boundingBox[0] = glm::vec3(-1.f);
    meshData->boundingBox[1] = glm::vec3(1.f);
    meshData->boundingSphere = meshData->boundingBox;

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * circleVerts.size(), circleVerts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));


    glCheck(glLineWidth(1.6f));
    glCheck(glEnable(GL_LINE_SMOOTH));



    //node for export preview
    flags = cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::Normal | cro::VertexProperty::UV0;
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(flags, TerrainID::Unused, GL_TRIANGLES));
    material = m_resources.materials.get(m_materialIDs[Material::VoxelPreview]);
    material.setProperty("u_maskColour", cro::Colour::Red);

    m_exportPreview = m_scene.createEntity();
    m_exportPreview.addComponent<cro::Transform>();
    m_exportPreview.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);

    //materials for export preview
    std::array<std::string, TerrainID::Count - 1> TexturePaths =
    {
        "assets/voxels/images/rough.png",
        "assets/voxels/images/fairway.png",
        "assets/voxels/images/green.png",
        "assets/voxels/images/bunker.png",
        "assets/voxels/images/water.png",
        "assets/voxels/images/scrub.png",
        "assets/voxels/images/stone.png"
    };
    m_shaderIDs[Shader::VoxelTextured] = m_resources.shaders.loadBuiltIn(cro::ShaderResource::VertexLit, cro::ShaderResource::DiffuseMap);
    shader = &m_resources.shaders.get(m_shaderIDs[Shader::VoxelTextured]);
    m_materialIDs[Material::VoxelTextured] = m_resources.materials.add(*shader);

    for (auto i = 0u; i < TexturePaths.size(); ++i)
    {
        auto material = m_resources.materials.get(m_materialIDs[Material::VoxelTextured]);
        auto& texture = m_resources.textures.get(TexturePaths[i]);
        texture.setRepeated(true);
        material.setProperty("u_diffuseMap", texture);
        material.setProperty("u_maskColour", cro::Colour::Red);

        m_exportPreview.getComponent<cro::Model>().setMaterial(i, material);
    }*/
}

void VoxelState::updateCursorPosition()
{
 /*   const auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
    auto cursorPos = cro::Mouse::getPosition();

    //TODO do we really want to keep this? Used in debugging
    cursorWorldPos = cam.pixelToCoords(cursorPos);

    auto finalPos = cursorWorldPos + glm::vec3(0.f, 0.1f, 0.f);

    bool hidden = (m_activeLayer == Layer::Water)
        || (finalPos.x < 0 || finalPos.x > Voxel::MapSize.x)
        || (finalPos.z > 0 || finalPos.z < -Voxel::MapSize.y);

    m_cursor.getComponent<cro::Model>().setHidden(hidden);
    m_cursor.getComponent<cro::Transform>().setPosition(finalPos);*/
}

void VoxelState::loadSettings()
{
    const auto cfgPath = cro::App::getPreferencePath() + "voxels.cfg";

    cro::ConfigFile cfg;
    if (cfg.loadFromFile(cfgPath))
    {
        const auto& props = cfg.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "show_water")
            {
                m_showLayer[Layer::Water] = p.getValue<bool>();
            }
            else if (name == "show_terrain")
            {
                m_showLayer[Layer::Terrain] = p.getValue<bool>();
            }
            else if (name == "show_voxel")
            {
                m_showLayer[Layer::Voxel] = p.getValue<bool>();
            }
            else if (name == "show_layers")
            {
                m_showLayerWindow = p.getValue<bool>();
            }
            else if (name == "active_layer")
            {
                auto layer = p.getValue<std::int32_t>();
                layer = std::max(0, std::min(Layer::Count - 1, layer));
                m_activeLayer = layer;
            }
            
            else if (name == "show_brush")
            {
                m_showBrushWindow = p.getValue<bool>();
            }
            else if (name == "brush_strength")
            {
                m_brush.strength = std::max(Voxel::BrushMinStrength, std::min(Voxel::BrushMaxStrength, p.getValue<float>()));
            }
            else if (name == "brush_feather")
            {
                m_brush.feather = std::max(Voxel::BrushMinFeather, std::min(Voxel::BrushMaxFeather, p.getValue<float>()));
            }
            else if (name == "brush_mode")
            {
                m_brush.editMode = cro::Util::Maths::sgn(p.getValue<std::int32_t>());
            }
            else if (name == "brush_terrain")
            {
                m_brush.terrain = std::max(0, std::min(static_cast<std::int32_t>(TerrainID::Stone), p.getValue<std::int32_t>()));
            }
            else if (name == "brush_carve")
            {
                m_brush.paintMode = std::min(static_cast<std::int32_t>(Brush::PaintMode::Carve), std::max(static_cast<std::int32_t>(Brush::PaintMode::Paint), p.getValue<std::int32_t>()));
            }

            else if (name == "show_overview")
            {
                m_drawTopView = p.getValue<bool>();
            }
        }
    }
}

void VoxelState::saveSettings()
{
    cro::ConfigFile cfg;
    cfg.addProperty("show_water").setValue(m_showLayer[Layer::Water]);
    cfg.addProperty("show_terrain").setValue(m_showLayer[Layer::Terrain]);
    cfg.addProperty("show_voxel").setValue(m_showLayer[Layer::Voxel]);
    cfg.addProperty("show_layers").setValue(m_showLayerWindow);
    cfg.addProperty("active_layer").setValue(m_activeLayer);

    cfg.addProperty("show_brush").setValue(m_showBrushWindow);
    cfg.addProperty("brush_strength").setValue(m_brush.strength);
    cfg.addProperty("brush_feather").setValue(m_brush.feather);
    cfg.addProperty("brush_mode").setValue(m_brush.editMode);
    cfg.addProperty("brush_terrain").setValue(m_brush.terrain);
    cfg.addProperty("brush_carve").setValue(m_brush.paintMode);

    cfg.addProperty("show_overview").setValue(m_drawTopView);

    const auto cfgPath = cro::App::getPreferencePath() + "voxels.cfg";
    cfg.save(cfgPath);
}

void VoxelState::applyEdit()
{
    if (cro::ui::wantsKeyboard() || cro::ui::wantsMouse())
    {
        return;
    }

    switch (m_activeLayer)
    {
    default: break;
    case Layer::Terrain:
        editTerrain();
        break;
    case Layer::Voxel:
        editVoxel();
        break;
    }
}

void VoxelState::editTerrain()
{
/*    if (!m_cursor.getComponent<cro::Model>().isHidden()
        && m_showLayer[Layer::Terrain])
    {
        auto cursorPos = m_cursor.getComponent<cro::Transform>().getPosition();
        auto imagePos = glm::vec2(cursorPos.x, -cursorPos.z);

        const float cursorRadius = Voxel::CursorRadius * m_cursor.getComponent<cro::Transform>().getScale().x;
        const float cursorRad2 = cursorRadius * cursorRadius;

        const glm::ivec2 MapSize(Voxel::MapSize);

        cro::IntRect region;
        region.left = static_cast<std::int32_t>(imagePos.x - cursorRadius) - 1;
        region.bottom = static_cast<std::int32_t>(imagePos.y - cursorRadius) - 1;
        region.width = (static_cast<std::int32_t>(cursorRadius) * 2) + 2;
        region.height = (static_cast<std::int32_t>(cursorRadius) * 2) + 2;

        region.left = std::max(0, std::min(MapSize.x, region.left));
        region.bottom = std::max(0, std::min(MapSize.y, region.bottom));
        
        auto right = region.left + region.width;
        right = std::max(0, std::min(MapSize.x, right));
        region.width = right - region.left;

        auto top = region.bottom + region.height;
        top = std::max(0, std::min(MapSize.y, top));
        region.height = top - region.bottom;

        for (auto y = region.bottom; y < region.bottom + region.height; ++y)
        {
            for (auto x = region.left; x < region.left + region.width; ++x)
            {
                auto len2 = glm::length2(glm::vec2(x,y) - imagePos);
                if (len2 < cursorRad2)
                {
                    auto idx = y * MapSize.x + x;

                    float amount = m_brush.strength * 0.01f;
                    if (m_brush.feather > 0)
                    {
                        amount *= 1.f - std::sqrt(len2) / cursorRadius;
                        amount = std::pow(amount, m_brush.feather);
                    }

                    amount *= m_brush.editMode;

                    m_textureBuffer[idx].g = std::max(0.f, std::min(1.f, m_textureBuffer[idx].g + amount));

                    m_terrainBuffer[idx].position.y = m_textureBuffer[idx].g * Voxel::MaxTerrainHeight;
                }
            }
        }

        updateTerrainImage(region);
        updateTerrainMesh(region);
    }*/
}

void VoxelState::updateTerrainImage(cro::IntRect area)
{
    CRO_ASSERT(m_terrainTexture.getGLHandle() != 0, "texture not created");
    constexpr glm::uvec2 texSize(Voxel::MapSize);

    glCheck(glBindTexture(GL_TEXTURE_2D, m_terrainTexture.getGLHandle()));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize.x, texSize.y, 0, GL_RGBA, GL_FLOAT, m_textureBuffer.data()));

    //problem with this is the buffer must only contain sub image data
    //glCheck(glTexSubImage2D(GL_TEXTURE_2D, 0, area.left, area.bottom, area.width, area.height, GL_RGBA, GL_FLOAT, m_textureBuffer.data()));
    glCheck(glBindTexture(GL_TEXTURE_2D, 0));
}

void VoxelState::updateTerrainMesh(cro::IntRect region)
{
    //TODO what's faster - sending the whole buffer
    //or calling subBufferData for each row of the area?

    //recalc normals first
    auto heightAt = 
        [&](std::int32_t x, std::int32_t y)
    {
        x = std::max(0, std::min(static_cast<std::int32_t>(Voxel::MapSize.x) - 1, x));
        y = std::max(0, std::min(static_cast<std::int32_t>(Voxel::MapSize.y) - 1, y));

        return m_terrainBuffer[y * static_cast<std::int32_t>(Voxel::MapSize.x) + x].position.y;
    };

    //expand the region by 1 so surrounding normals are updated too
    region.left = std::max(0, region.left - 1);
    region.width = std::min(static_cast<std::int32_t>(Voxel::MapSize.x) - region.left, region.width + 1);
    region.bottom = std::max(0, region.bottom - 1);
    region.height = std::min(static_cast<std::int32_t>(Voxel::MapSize.y) - region.bottom, region.height + 1);

    for (auto y = region.bottom; y < region.bottom + region.height; ++y)
    {
        for (auto x = region.left; x < region.left + region.width; ++x)
        {
            auto l = heightAt(x - 1, y);
            auto r = heightAt(x + 1, y);
            auto u = heightAt(x, y + 1);
            auto d = heightAt(x, y - 1);

            glm::vec3 normal = { l - r, 2.f, -(d - u) };
            normal = glm::normalize(normal);

            m_terrainBuffer[y * static_cast<std::int32_t>(Voxel::MapSize.x) + x].normal = normal;
        }
    }

    auto* meshData = &m_layers[Layer::Terrain].getComponent<cro::Model>().getMeshData();
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vboAllocation.vboID));
    glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(TerrainVertex) * m_terrainBuffer.size(), m_terrainBuffer.data(), GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

void VoxelState::resetTerrain()
{
    for (auto i = 0u; i < m_terrainBuffer.size(); ++i)
    {
        m_terrainBuffer[i].position.y = 0.f;
        m_textureBuffer[i].g = 0.f;
    }

    cro::IntRect area(0, 0, static_cast<std::int32_t>(Voxel::MapSize.x), static_cast<std::int32_t>(Voxel::MapSize.y));
    updateTerrainImage(area);
    updateTerrainMesh(area);
}

void VoxelState::editVoxel()
{
    if (!m_cursor.getComponent<cro::Model>().isHidden()
        && m_showLayer[Layer::Voxel])
    {

        auto pos = m_cursor.getComponent<cro::Transform>().getPosition();
        pos -= m_layers[Layer::Voxel].getComponent<cro::Transform>().getPosition();

        const float radius = m_cursor.getComponent<cro::Transform>().getScale().x;

        pv::Vector3DInt32 minB(
            static_cast<std::int32_t>(std::floor(pos.x - radius)),
            static_cast<std::int32_t>(std::floor(pos.y - radius)),
            static_cast<std::int32_t>(std::floor(pos.z - radius)));
        pv::Vector3DInt32 maxB(
            static_cast<std::int32_t>(std::ceil(pos.x + radius)),
            static_cast<std::int32_t>(std::ceil(pos.y + radius)),
            static_cast<std::int32_t>(std::ceil(pos.z + radius)));

        minB.setX(std::max(0, std::min(Voxel::IslandSize.x - 2, minB.getX())));
        minB.setY(std::max(0, std::min(Voxel::IslandSize.y - 3, minB.getY())));
        minB.setZ(std::max(0, std::min(Voxel::IslandSize.z - 2, minB.getZ())));

        maxB.setX(std::max(minB.getX() + 1, std::min(Voxel::IslandSize.x - 1, maxB.getX())));
        maxB.setY(std::max(minB.getY() + 1, std::min(Voxel::IslandSize.y - 2, maxB.getY())));
        maxB.setZ(std::max(minB.getZ() + 1, std::min(Voxel::IslandSize.z - 1, maxB.getZ())));


        pv::Region editRegion(minB, maxB);
        const float Rad2 = radius * radius;

        for (auto z = minB.getZ(); z < maxB.getZ(); ++z)
        {
            for (auto y = minB.getY(); y < maxB.getY(); ++y)
            {
                for (auto x = minB.getX(); x < maxB.getX(); ++x)
                {
                    auto len2 = glm::length2(pos - glm::vec3(x, y, z));
                    if (len2 < Rad2)
                    {
                        auto vx = m_voxelVolume.getVoxel(x, y, z);

                        if (m_brush.paintMode == Brush::PaintMode::Carve)
                        {
                            float amount = m_brush.strength * 0.1f;
                            if (m_brush.feather > 0)
                            {
                                amount *= 1.f - std::sqrt(len2) / radius;
                                amount = std::pow(amount, m_brush.feather);
                            }
                            amount *= m_brush.editMode;
                            vx.density = y < 5 ? std::min(1.f, std::max(0.f, vx.density + amount)) : 0.f;
                        }

                        vx.terrain = m_brush.terrain;
                        m_voxelVolume.setVoxel(x, y, z, vx);
                    }
                }
            }
        }

        updateVoxelMesh(editRegion);
    }
}

void VoxelState::updateVoxelMesh(const pv::Region& region)
{
    auto chunkStartX = region.getLowerX() / Voxel::ChunkSize.x;
    auto chunkEndX = std::min((region.getUpperX() / Voxel::ChunkSize.x) +1, Voxel::ChunkCount.x);

    auto chunkStartZ = region.getLowerZ() / Voxel::ChunkSize.z;
    auto chunkEndZ = std::min((region.getUpperZ() / Voxel::ChunkSize.z) + 1, Voxel::ChunkCount.z);

    const pv::Vector3DInt32 chunkSize(Voxel::ChunkSize.x, Voxel::ChunkSize.y, Voxel::ChunkSize.z);

    auto z = chunkStartZ;
    do
    {
        auto x = chunkStartX;
        do
        {
            pv::Vector3DInt32 chunkCorner(x * Voxel::ChunkSize.x, 0, z * Voxel::ChunkSize.z);

            Voxel::PreviewMesh mesh;
            Voxel::ExtractionController controller;
            pv::extractMarchingCubesMeshCustom(&m_voxelVolume, pv::Region(chunkCorner, chunkCorner + chunkSize), &mesh, controller);

            auto idx = z * Voxel::ChunkCount.x + x;

            auto* meshData = &m_chunks[idx].getComponent<cro::Model>().getMeshData();
            meshData->vertexCount = mesh.getVertexData().size();
            meshData->boundingBox[0] = glm::vec3(0.f);
            meshData->boundingBox[1] = glm::vec3(Voxel::ChunkSize);
            meshData->boundingSphere = meshData->boundingBox;

            glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vboAllocation.vboID));
            glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(Voxel::PreviewVertex) * meshData->vertexCount, mesh.getVertexData().data(), GL_DYNAMIC_DRAW));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            auto* submesh = &meshData->indexData[0];
            submesh->indexCount = static_cast<std::uint32_t>(mesh.getIndexData().size());
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), mesh.getIndexData().data(), GL_DYNAMIC_DRAW));
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

            x++;
        } while (x < chunkEndX);
        z++;
    } while (z < chunkEndZ);
}

void VoxelState::resetVolume()
{
    m_voxelVolume.reset();

    /*const std::int32_t BorderSize = 50;
    const float SoftenRadius = static_cast<float>((Voxel::IslandSize.x / 2) - BorderSize);
    for (auto z = BorderSize; z < Voxel::IslandSize.z - BorderSize; ++z)
    {
        for (auto y = 0; y < Voxel::IslandSize.y / 2; ++y)
        {
            for (auto x = BorderSize + (y * 2); x < Voxel::IslandSize.x - (BorderSize + (y * 2)); ++x)
            {
                auto rad = glm::length(glm::vec3(Voxel::IslandSize / 2) - glm::vec3(x, y, z));

                m_voxelVolume.setVoxel(x, y, z, { 0.5f + (0.1f * (1.f - (rad / SoftenRadius))), TerrainID::Scrub });
            }
        }
    }*/

    auto centre = Voxel::IslandSize / 2;
    centre.y = -170;
    for (auto z = 0; z < Voxel::IslandSize.z; ++z)
    {
        for (auto y = 0; y < Voxel::IslandSize.y / 2; ++y)
        {
            for (auto x = 0; x < Voxel::IslandSize.x; ++x)
            {
                auto dist = glm::length2(glm::vec3(centre) - glm::vec3(x, y, z));
                //LogI << dist << std::endl;

                m_voxelVolume.setVoxel(x, y, z, { 1.f - std::min(1.f, (dist / (260.f * 260.f))), TerrainID::Scrub });
            }
        }
    }

    updateVoxelMesh(m_voxelVolume.getEnclosingRegion());
}

void VoxelState::createExportMesh()
{
    const std::int32_t ReScale = 2;

    //resample the volume to a lower density - the mesh will be rescaled to the given param
/*    pv::RawVolume<Voxel::Data> newVolume(pv::Region(pv::Vector3DInt32(0), pv::Vector3DInt32(Voxel::IslandSize.x / ReScale, Voxel::IslandSize.y / ReScale, Voxel::IslandSize.z / ReScale)));
    pv::VolumeResampler resampler(&m_voxelVolume, m_voxelVolume.getEnclosingRegion(), &newVolume, newVolume.getEnclosingRegion());
    resampler.execute();

    Voxel::ExtractionController<Voxel::UseSobel::True> controller;
    Voxel::ExportMesh mesh(m_layers[Layer::Voxel].getComponent<cro::Transform>().getPosition(), ReScale);
    pv::extractMarchingCubesMeshCustom(&newVolume, newVolume.getEnclosingRegion(), &mesh, controller);*/

    //custom mesh extractor - splits faces into submeshes by material
    //updates vertex colour, and discards downward facing triangles
    //Voxel::ExtractionController<Voxel::UseSobel::True, TerrainID::Rough> c0;
    //Voxel::ExtractionController<Voxel::UseSobel::True, TerrainID::Fairway> c1;
    //Voxel::ExtractionController<Voxel::UseSobel::True, TerrainID::Green> c2;
    //Voxel::ExtractionController<Voxel::UseSobel::True, TerrainID::Bunker> c3;
    //Voxel::ExtractionController<Voxel::UseSobel::True, TerrainID::Water> c4;
    //Voxel::ExtractionController<Voxel::UseSobel::True, TerrainID::Scrub> c5;
    //Voxel::ExtractionController<Voxel::UseSobel::True, TerrainID::Stone> c6;
    //Voxel::ExportMesh mesh(m_layers[Layer::Voxel].getComponent<cro::Transform>().getPosition()/*, ReScale*/);

    
    /*std::vector<Voxel::ExportVertex> vertices;
    std::array<std::vector<std::uint32_t>, TerrainID::Count - 1> indices;

    pv::extractMarchingCubesMeshCustom(&m_voxelVolume, m_voxelVolume.getEnclosingRegion(), &mesh, c0);
    indices[TerrainID::Rough] = mesh.getIndexData()[TerrainID::Rough];
    vertices.insert(vertices.end(), mesh.getVertexData().begin(), mesh.getVertexData().end());
    auto increment = vertices.size();

    pv::extractMarchingCubesMeshCustom(&m_voxelVolume, m_voxelVolume.getEnclosingRegion(), &mesh, c1);
    indices[TerrainID::Fairway] = mesh.getIndexData()[TerrainID::Fairway];
    for (auto& i : indices[TerrainID::Fairway])
    {
        i += static_cast<std::uint32_t>(increment);
    }
    vertices.insert(vertices.end(), mesh.getVertexData().begin(), mesh.getVertexData().end());
    increment = vertices.size();*/




    //auto* meshData = &m_exportPreview.getComponent<cro::Model>().getMeshData();
    ////meshData->vertexCount = vertices.size();// mesh.getVertexData().size();
    /*meshData->vertexCount = mesh.getVertexData().size();
    meshData->boundingBox[0] = glm::vec3(0.f);
    meshData->boundingBox[1] = glm::vec3(Voxel::IslandSize);
    meshData->boundingSphere = meshData->boundingBox;

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));*/
    ////glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(Voxel::ExportVertex) * meshData->vertexCount, vertices.data()/*mesh.getVertexData().data()*/, GL_STATIC_DRAW));
    /*glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(Voxel::ExportVertex) * meshData->vertexCount, mesh.getVertexData().data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    const auto& indices = mesh.getIndexData();

    for (auto i = 0u; i < indices.size(); ++i)
    {
        auto* submesh = &meshData->indexData[i];
        submesh->indexCount = static_cast<std::uint32_t>(indices[i].size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices[i].data(), GL_STATIC_DRAW));
    }
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));*/


    //TODO look at mesh decimation instead of resampling

    //TODO arrange submesh in such a way that we don't create
    //empty ones on the preview model

    //TODO cache the result somewhere in order that it can
    //be exported to gltf or similar for modification in blender
}