/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "GameState.hpp"
#include "SeaSystem.hpp"
#include "GameConsts.hpp"
#include "fastnoise/FastNoiseSIMD.h"

#include <crogine/gui/Gui.hpp>
#include <crogine/core/Keyboard.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/Sunlight.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>

#include <crogine/graphics/CircleMeshBuilder.hpp>
#include <crogine/graphics/GridMeshBuilder.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/postprocess/PostChromeAB.hpp>

#include <crogine/util/Constants.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    //debug gui stuffs
    glm::vec3 LightColour = glm::vec3(1.0);
    glm::vec3 LightRotation = glm::vec3(-cro::Util::Const::PI * 0.9f, 0.f, 0.f);

    float ShadowmapProjection = 80.f;
    float ShadowmapClipPlane = 100.f;

    std::array moveEnts =
    {
        cro::Entity(),
        cro::Entity(),
        cro::Entity(),
        cro::Entity()
    };

    //render flags for reflection passes
    const std::uint64_t NoPlanes = 0xFFFFFFFFFFFFFFF0;
    const std::array<std::uint64_t, 4u> PlayerPlanes =
    {
        0x1, 0x2,0x4,0x8
    };

    const std::array Colours =
    {
        cro::Colour::Red(), cro::Colour::Magenta(),
        cro::Colour::Green(), cro::Colour::Yellow()
    };

    const float SpawnOffset = 24.f;
    const std::array PlayerSpawns =
    {
        glm::vec3(-SpawnOffset, 0.f, -SpawnOffset),
        glm::vec3(SpawnOffset, 0.f, -SpawnOffset),
        glm::vec3(-SpawnOffset, 0.f, SpawnOffset),
        glm::vec3(SpawnOffset, 0.f, SpawnOffset)
    };
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context, std::size_t localPlayerCount)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_cameras       (localPlayerCount),
    m_heightmap     (IslandTileCount * IslandTileCount, 0.f),
    m_foamEffect    (m_resources)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
        });

#ifdef CRO_DEBUG_

    registerWindow([&]()
        {
            if (ImGui::Begin("Buns"))
            {
                ImGui::Image(m_foamEffect.getTexture(),
                    { 256.f, 256.f }, { 0.f, 2.f }, { 2.f, 0.f });

                if (ImGui::SliderFloat("Shadow Map Projection", &ShadowmapProjection, 10.f, 200.f))
                {
                    auto half = ShadowmapProjection / 2.f;
                    m_gameScene.getSunlight().getComponent<cro::Sunlight>().setProjectionMatrix(glm::ortho(-half, half, -half, half, 0.1f, ShadowmapClipPlane));
                }

                if (ImGui::SliderFloat("Shadow Map Far Plane", &ShadowmapClipPlane, 1.f, 500.f))
                {
                    auto half = ShadowmapProjection / 2.f;
                    m_gameScene.getSunlight().getComponent<cro::Sunlight>().setProjectionMatrix(glm::ortho(-half, half, -half, half, 0.1f, ShadowmapClipPlane));
                }

                if (ImGui::ColorEdit3("Light Colour", &LightColour[0]))
                {
                    m_gameScene.getSunlight().getComponent<cro::Sunlight>().setColour(cro::Colour(LightColour.r, LightColour.g, LightColour.b, 1.f));
                }

                if (ImGui::SliderFloat3("Light Rotation", &LightRotation[0], -cro::Util::Const::PI, cro::Util::Const::PI))
                {
                    m_gameScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, LightRotation.x);
                    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, LightRotation.y);
                    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, LightRotation.z);
                }

                if (ImGui::CollapsingHeader("Height Map"))
                {
                    ImGui::Image(m_islandTexture,
                        { IslandTileCount * 3.f, IslandTileCount * 3.f }, { 0.f, 1.f }, { 1.f, 0.f });
                }

                if (ImGui::CollapsingHeader("ShadowMap"))
                {
                    ImGui::Image(m_gameScene.getSystem<cro::ShadowMapRenderer>().getDepthMapTexture(),
                        { 320.f, 320.f }, { 0.f, 0.f }, { 1.f, 1.f });
                }

                if (ImGui::CollapsingHeader("Reflection Map"/*, ImGuiTreeNodeFlags_DefaultOpen*/))
                {
                    for (auto ent : m_cameras)
                    {
                        const auto& cam = ent.getComponent<cro::Camera>();

                        ImGui::Image(cam.reflectionBuffer.getTexture(),
                            { 240.f, 240.f }, { 0.f, 1.f }, { 1.f, 0.f });

                        ImGui::SameLine();

                        ImGui::Image(cam.refractionBuffer.getTexture(),
                            { 240.f, 240.f }, { 0.f, 1.f }, { 1.f, 0.f });
                    }
                }
            }
            ImGui::End();
        });

#endif
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool GameState::simulate(float dt)
{
    glm::vec3 movement(0.f);
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_D))
    {
        movement.x += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_A))
    {
        movement.x -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_W))
    {
        movement.z -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_S))
    {
        movement.z += 1.f;
    }
    /*if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_Q))
    {
        movement.y -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_E))
    {
        movement.y += 1.f;
    }*/
    if (glm::length2(movement) > 0)
    {
        movement = glm::normalize(movement);
    }
    const float Speed = 10.f;
    moveEnts[0].getComponent<cro::Transform>().move(movement * Speed * dt);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void GameState::render()
{
    //m_gameScene.setPostEnabled(false);
    m_foamEffect.update();

    for(auto i = 0u; i < m_cameras.size(); ++i)
    {
        auto ent = m_cameras[i];
        m_gameScene.setActiveCamera(ent);

        auto& cam = ent.getComponent<cro::Camera>();
        cam.renderFlags = NoPlanes;
        auto oldVP = cam.viewport;

        cam.viewport = { 0.f,0.f,1.f,1.f };

        cam.setActivePass(cro::Camera::Pass::Reflection);
        cam.reflectionBuffer.clear(cro::Colour::Red());
        m_gameScene.render(cam.reflectionBuffer);
        cam.reflectionBuffer.display();

        cam.setActivePass(cro::Camera::Pass::Refraction);
        cam.refractionBuffer.clear(cro::Colour::Blue());
        m_gameScene.render(cam.refractionBuffer);
        cam.refractionBuffer.display();

        cam.renderFlags = NoPlanes | PlayerPlanes[i];
        cam.setActivePass(cro::Camera::Pass::Final);
        cam.viewport = oldVP;
    }

    //final render
    //m_gameScene.setPostEnabled(true);

    auto& rt = cro::App::getWindow();
    m_gameScene.render(rt, m_cameras);
    m_uiScene.render(rt);
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<SeaSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb, glm::uvec2(4096));
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::ParticleSystem>(mb);
}

void GameState::loadAssets()
{
    m_islandTexture.create(IslandTileCount, IslandTileCount);

    m_materialIDs[MaterialID::Sea] = m_gameScene.getSystem<SeaSystem>().loadResources(m_resources);
    m_resources.materials.get(m_materialIDs[MaterialID::Sea]).setProperty("u_depthMap", m_islandTexture);
    m_resources.materials.get(m_materialIDs[MaterialID::Sea]).setProperty("u_foamMap", m_foamEffect.getTexture());

    m_environmentMap.loadFromFile("assets/images/cubemap/beach02.hdr");

    //m_gameScene.setCubemap(m_environmentMap);
    m_gameScene.setCubemap("assets/images/cubemap/sky.ccm");
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI * 0.9f);
    //m_resources.materials.get(m_materialIDs[MaterialID::Sea]).setProperty("u_skybox", m_gameScene.getCubemap());
}

void GameState::createScene()
{
    //m_gameScene.addPostProcess<cro::PostChromeAB>();

    //sea plane
    auto gridID = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(SeaRadius, 30));

    auto addSeaplane = [&, gridID]()
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(gridID), m_resources.materials.get(m_materialIDs[MaterialID::Sea]));
        entity.addComponent<SeaComponent>();

        return entity;
    };

    //placeholder for player scale
    cro::ModelDefinition md;
    md.loadFromFile("assets/models/player_box.cmt", m_resources, &m_environmentMap);

    //as many cameras as there are local players
    for (auto i = 0u; i < m_cameras.size(); ++i)
    {
        m_cameras[i] = m_gameScene.createEntity();
        m_cameras[i].addComponent<cro::Transform>().setPosition({ 0.f, CameraHeight, CameraDistance });

        auto rotation = glm::lookAt(glm::vec3(0.f), glm::vec3(0.f, 0.f, -SeaRadius), cro::Transform::Y_AXIS);
        m_cameras[i].getComponent<cro::Transform>().rotate(glm::inverse(rotation));

        auto& cam = m_cameras[i].addComponent<cro::Camera>();
        cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
        cam.reflectionBuffer.setSmooth(true);
        cam.refractionBuffer.create(ReflectionMapSize, ReflectionMapSize);
        cam.refractionBuffer.setSmooth(true);

        auto waterEnt = addSeaplane();
        waterEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, -(SeaRadius - CameraDistance) });
        waterEnt.getComponent<cro::Model>().setRenderFlags(PlayerPlanes[i]);

        //placeholder for player model
        auto playerEnt = m_gameScene.createEntity();
        playerEnt.addComponent<cro::Transform>().setOrigin({ 0.f, -0.8f, 0.f });
        //playerEnt.getComponent<cro::Transform>().setPosition({ 0.f, CameraHeight / 2.f, 0.f });
        md.createModel(playerEnt, m_resources);
        playerEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", Colours[i]);

        playerEnt.addComponent<cro::Callback>().active = true;
        playerEnt.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            auto position = e.getComponent<cro::Transform>().getWorldPosition();
            position.x += (IslandSize / 2.f); //puts the position relative to the grid - this should be the origin coords
            position.z += (IslandSize / 2.f);

            auto height = getPlayerHeight(position);
            e.getComponent<cro::Transform>().setPosition({ 0.f, height + IslandWorldHeight, 0.f });
        };

        
        //this is the node actually controlled by the player.
        auto root = m_gameScene.createEntity();
        root.addComponent<cro::Transform>().setPosition(PlayerSpawns[i]);
        root.getComponent<cro::Transform>().addChild(m_cameras[i].getComponent<cro::Transform>());
        root.getComponent<cro::Transform>().addChild(waterEnt.getComponent<cro::Transform>());
        root.getComponent<cro::Transform>().addChild(playerEnt.getComponent<cro::Transform>());

        moveEnts[i] = root;
    }
    m_gameScene.setActiveCamera(m_cameras[0]);

    //main camera - updateView() updates all cameras so we only need one callback
    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&GameState::updateView, this, std::placeholders::_1);

    //add the light source to the camera so shadow map follows
    //TODO this should be independent and cover the whole map so each camera can see correct shadowing?
    //otherwise each camera requires its own shadow map...
    camEnt.getComponent<cro::Transform>().addChild(m_gameScene.getSunlight().getComponent<cro::Transform>());
    m_gameScene.getSunlight().getComponent<cro::Transform>().setPosition({ 0.f, 10.f, -16.f });
    m_gameScene.getSunlight().getComponent<cro::Sunlight>().setProjectionMatrix(glm::ortho(-40.f, 40.f, -40.f, 40.f, 0.1f, 100.f));

    //TODO place listener on cam if single player, else place in centre of map.

    //light direction dealy
    md.loadFromFile("assets/models/arrow.cmt", m_resources);
    md.createModel(m_gameScene.getSunlight(), m_resources);

    //box to display shadow frustum
    md.loadFromFile("assets/models/frustum.cmt", m_resources);
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ 0.f, 0.f, 0.5f });
    md.createModel(entity, m_resources);
    entity.getComponent<cro::Model>().setRenderFlags(~NoPlanes);
    m_gameScene.getSunlight().getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
    [](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setScale({ ShadowmapProjection, ShadowmapProjection, ShadowmapClipPlane });
    };

    createIsland();
}

void GameState::createUI()
{

}

void GameState::updateView(cro::Camera&)
{
    CRO_ASSERT(!m_cameras.empty(), "Need at least one camera!");

    const float fov = 42.f * cro::Util::Const::degToRad;
    const float nearPlane = 0.1f;
    const float farPlane = 140.f;
    float aspect = 16.f / 9.f;

    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;
    cro::FloatRect viewport(0.f, (1.f - size.y) / 2.f, size.x, size.y);

    //update the UI camera to match the new screen size
    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = viewport;

    //set up the viewports
    switch (m_cameras.size())
    {
    case 1:
        m_cameras[0].getComponent<cro::Camera>().viewport = viewport;
        break;
    case 2:
        aspect = 8.f / 9.f;
        viewport.width /= 2.f;
        m_cameras[0].getComponent<cro::Camera>().viewport = viewport;
        viewport.left = viewport.width;
        m_cameras[1].getComponent<cro::Camera>().viewport = viewport;
        break;

    case 3:
    case 4:
        viewport.width /= 2.f;
        viewport.height /= 2.f;
        viewport.bottom += viewport.height;
        m_cameras[0].getComponent<cro::Camera>().viewport = viewport;

        viewport.left = viewport.width;
        m_cameras[1].getComponent<cro::Camera>().viewport = viewport;

        viewport.left = 0.f;
        viewport.bottom -= viewport.height;
        m_cameras[2].getComponent<cro::Camera>().viewport = viewport;

        if (m_cameras.size() == 4)
        {
            viewport.left = viewport.width;
            m_cameras[3].getComponent<cro::Camera>().viewport = viewport;
        }
        break;

    default:
        break;
    }

    //set up projection
    for (auto cam : m_cameras)
    {
        cam.getComponent<cro::Camera>().projectionMatrix = glm::perspective(fov, aspect, nearPlane, farPlane);
    }
}

void GameState::createIsland()
{
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().rotate(-cro::Transform::X_AXIS, cro::Util::Const::PI / 2.f);
    entity.getComponent<cro::Transform>().move({ 0.f, IslandWorldHeight, 0.f }); //small extra amount to stop z-fighting
    entity.getComponent<cro::Transform>().setOrigin({ IslandSize / 2.f, IslandSize / 2.f, 0.f });

    auto meshID = m_resources.meshes.loadMesh(cro::GridMeshBuilder({ IslandSize, IslandSize }, IslandTileCount - 1)); //this accounts for extra row of vertices
    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::PBR, cro::ShaderResource::DiffuseMap |
                                                                                cro::ShaderResource::NormalMap | 
                                                                                cro::ShaderResource::MaskMap |
                                                                                cro::ShaderResource::RxShadows);

    m_materialIDs[MaterialID::Island] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    //m_resources.materials.get(m_materialIDs[MaterialID::Island]).setProperty("u_diffuseMap", m_islandTexture);
    //m_resources.materials.get(m_materialIDs[MaterialID::Island]).setProperty("u_colour", cro::Colour::Green());

    m_resources.textures.load(TextureID::SandAlbedo, "assets/materials/sand01/albedo.png");
    m_resources.textures.load(TextureID::SandNormal, "assets/materials/sand01/normal.png");
    m_resources.textures.load(TextureID::SandMask, "assets/materials/sand01/mask.png");

    auto& albedo = m_resources.textures.get(TextureID::SandAlbedo);
    albedo.setSmooth(true);
    albedo.setRepeated(true);
    auto& normal = m_resources.textures.get(TextureID::SandNormal);
    normal.setSmooth(true);
    normal.setRepeated(true);
    auto& mask = m_resources.textures.get(TextureID::SandMask);
    mask.setSmooth(true);
    mask.setRepeated(true);

    auto& material = m_resources.materials.get(m_materialIDs[MaterialID::Island]);
    material.setProperty("u_diffuseMap", albedo);
    material.setProperty("u_normalMap", normal);
    material.setProperty("u_maskMap", mask);
    material.setProperty("u_colour", cro::Colour::White());

    material.setProperty("u_irradianceMap", m_environmentMap.getIrradianceMap());
    material.setProperty("u_prefilterMap", m_environmentMap.getPrefilterMap());
    material.setProperty("u_brdfMap", m_environmentMap.getBRDFMap());

    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);

    //entity.getComponent<cro::Model>().setRenderFlags(0);

    createHeightmap();

    //create new vertex data from heightmap and upload to our island VBO
    auto& meshData = entity.getComponent<cro::Model>().getMeshData();
    updateIslandVerts(meshData);
}

void GameState::createHeightmap()
{
    //height mask
    const float MaxLength = TileSize * IslandFadeSize;
    const std::uint32_t Offset = IslandBorder + IslandFadeSize;

    for (auto y = IslandBorder; y < IslandTileCount - IslandBorder; ++y)
    {
        for (auto x = IslandBorder; x < IslandTileCount - IslandBorder; ++x)
        {
            float val = 0.f;
            float pos = 0.f;

            if (x < IslandTileCount / 2)
            {
                pos = (x - IslandBorder) * TileSize;

            }
            else
            {
                pos = (IslandTileCount - IslandBorder - x) * TileSize;
            }

            val = pos / MaxLength;
            val = std::min(1.f, std::max(0.f, val));

            if (y > Offset && y < IslandTileCount - Offset)
            {
                m_heightmap[y * IslandTileCount + x] = val;
            }

            if (y < IslandTileCount / 2)
            {
                pos = (y - IslandBorder) * TileSize;

            }
            else
            {
                pos = (IslandTileCount - IslandBorder - y) * TileSize;
            }

            val = pos / MaxLength;
            val = std::min(1.f, std::max(0.f, val));

            if (x > Offset && x < IslandTileCount - Offset)
            {
                m_heightmap[y * IslandTileCount + x] = val;
            }
        }
    }

    //mask corners
    auto corner = [&, MaxLength](glm::uvec2 start, glm::uvec2 end, glm::vec2 centre)
    {
        for (auto y = start.t; y < end.t + 1; ++y)
        {
            for (auto x = start.s; x < end.s + 1; ++x)
            {
                glm::vec2 pos = glm::vec2(x, y) * TileSize;
                float val = 1.f - (glm::length(pos - centre) / MaxLength);
                val = std::max(0.f, std::min(1.f, val));
                m_heightmap[y * IslandTileCount + x] = val;
            }
        }
    };
    glm::vec2 centre = glm::vec2(Offset, Offset) * TileSize;
    corner({ IslandBorder, IslandBorder }, { Offset, Offset }, centre);

    centre.x = (IslandTileCount - Offset) * TileSize;
    corner({ IslandTileCount - Offset - 1, IslandBorder }, { IslandTileCount - IslandBorder, Offset }, centre);

    centre.y = (IslandTileCount - Offset) * TileSize;
    corner({ IslandTileCount - Offset - 1, IslandTileCount - Offset - 1 }, { IslandTileCount - IslandBorder, IslandTileCount - IslandBorder }, centre);

    centre.x = Offset * TileSize;
    corner({ IslandBorder, IslandTileCount - Offset - 1 }, { Offset, IslandTileCount - IslandBorder }, centre);


    //fastnoise
    FastNoiseSIMD* myNoise = FastNoiseSIMD::NewFastNoiseSIMD();
    myNoise->SetFrequency(0.05f);
    float* noiseSet = myNoise->GetSimplexFractalSet(0, 0, 0, IslandTileCount * 4, 1, 1);


    //using a 1D noise push the edges of the mask in/out by +/-1
    std::uint32_t i = 0;
    for (auto x = Offset; x < IslandTileCount - Offset; ++x)
    {
        auto val = std::round(noiseSet[i++] * EdgeVariation);
        if (val < 0) //move gradient tiles up one
        {
            for (auto j = 0; j < std::abs(val); ++j)
            {
                //top row
                for (auto y = IslandBorder; y < Offset + 1; ++y)
                {
                    m_heightmap[(y - 1) * IslandTileCount + x] = m_heightmap[y * IslandTileCount + x];
                }

                //bottom row
                for (auto y = IslandTileCount - Offset - 1; y < IslandTileCount - IslandBorder; ++y)
                {
                    m_heightmap[(y - 1) * IslandTileCount + x] = m_heightmap[y * IslandTileCount + x];
                }
            }
        }
        else if (val > 0) //move gradient tiles down one
        {
            for (auto j = 0; j < val; ++j)
            {
                //top row
                for (auto y = Offset; y > IslandBorder; --y)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[(y - 1) * IslandTileCount + x];
                }

                //bottom row
                for (auto y = IslandTileCount - IslandBorder; y > IslandTileCount - Offset - 1; --y)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[(y - 1) * IslandTileCount + x];
                }
            }
        }
    }

    for (auto y = Offset; y < IslandTileCount - Offset; ++y)
    {
        auto val = std::round(noiseSet[i++] * EdgeVariation);
        if (val < 0) //move gradient tiles left one
        {
            for (auto j = 0; j < std::abs(val); ++j)
            {
                //left col
                for (auto x = IslandBorder; x < Offset + 1; ++x)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[y * IslandTileCount + (x + 1)];
                }

                //right col
                for (auto x = IslandTileCount - Offset - 1; x < IslandTileCount - IslandBorder; ++x)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[y * IslandTileCount + (x + 1)];
                }
            }
        }
        else if (val > 0) //move gradient tiles right one
        {
            for (auto j = 0; j < val; ++j)
            {
                //left col
                for (auto x = Offset; x > IslandBorder; --x)
                {
                    m_heightmap[y * IslandTileCount + (x + 1)] = m_heightmap[y * IslandTileCount + x];
                }

                //right col
                for (auto x = IslandTileCount - IslandBorder; x > IslandTileCount - Offset - 1; --x)
                {
                    m_heightmap[y * IslandTileCount + (x + 1)] = m_heightmap[y * IslandTileCount + x];
                }
            }
        }
    }
    FastNoiseSIMD::FreeNoiseSet(noiseSet);

    myNoise->SetFrequency(0.01f);
    noiseSet = myNoise->GetSimplexFractalSet(0, 0, 0, IslandTileCount, IslandTileCount, 1);

    for (auto y = 0u; y < IslandTileCount; ++y)
    {
        for (auto x = 0u; x < IslandTileCount; ++x)
        {
            float val = noiseSet[y * IslandTileCount + x];
            val += 1.f;
            val /= 2.f;

            const float minAmount = IslandLevels / 2.f;
            float multiplier = IslandLevels - minAmount;

            val *= multiplier;
            val = std::floor(val);
            val += minAmount;
            val /= IslandLevels;

            m_heightmap[y * IslandTileCount + x] *= val;
        }
    }
    FastNoiseSIMD::FreeNoiseSet(noiseSet);

    //preview texture / height map
    cro::Image img;
    img.create(IslandTileCount, IslandTileCount, cro::Colour::Black());
    for (auto i = 0u; i < m_heightmap.size(); ++i)
    {
        auto level = m_heightmap[i] * 255.f;
        auto channel = static_cast<std::uint8_t>(level);
        cro::Colour c(channel, channel, channel);

        auto x = i % IslandTileCount;
        auto y = i / IslandTileCount;
        img.setPixel(x, y, c);
    }

    m_islandTexture.update(img.getPixelData());
    m_islandTexture.setSmooth(true);
}

void GameState::updateIslandVerts(cro::Mesh::Data& meshData)
{
    struct Vertex final
    {
        glm::vec3 pos = glm::vec3(0.f);
        glm::vec3 normal = glm::vec3(0.f);
        glm::vec3 tan = glm::vec3(0.f);
        glm::vec3 bitan = glm::vec3(0.f);
        glm::vec2 uv = glm::vec2(0.f);
    };
    std::vector<Vertex> verts(IslandTileCount * IslandTileCount);

    auto heightAt = [&](std::int32_t x, std::int32_t y)
    {
        x = std::min(static_cast<std::int32_t>(IslandTileCount), std::max(0, x));
        y = std::min(static_cast<std::int32_t>(IslandTileCount), std::max(0, y));

        return m_heightmap[y * IslandTileCount + x];
    };

    for (auto i = 0u; i < m_heightmap.size(); ++i)
    {
        std::int32_t x = i % IslandTileCount;
        std::int32_t y = i / IslandTileCount;

        verts[i].pos = { x * TileSize, y * TileSize, m_heightmap[i] * IslandHeight };
        verts[i].uv = { verts[i].pos.x / IslandSize, verts[i].pos.y / IslandSize };

        verts[i].uv *= 8.f;

        //normal calc
        auto l = heightAt(x - 1, y);
        auto r = heightAt(x + 1, y);
        auto u = heightAt(x, y + 1);
        auto d = heightAt(x, y - 1);
        
        verts[i].normal = { l - r, d - u, 2.f };
        verts[i].normal = glm::normalize(verts[i].normal);

        //tan/bitan
        //if we assume the UV go parallel with the grid we can just use
        //a 90 deg rotation and the cross product of the result
        verts[i].tan = { verts[i].normal.z, verts[i].normal.y, -verts[i].normal.x };
        verts[i].bitan = glm::cross(verts[i].tan, verts[i].normal);
    }


    CRO_ASSERT(verts.size() == meshData.vertexCount, "Incorrect vertex count");
    CRO_ASSERT(sizeof(Vertex) == meshData.vertexSize, "Incorrect vertex size");

    glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

float GameState::getPlayerHeight(glm::vec3 position) 
{
    auto lerp = [](float a, float b, float t) constexpr
    {
        return a + t * (b - a);
    };

    const auto getHeightAt = [&](std::int32_t x, std::int32_t y)
    {
        //heightmap is flipped relative to the world innit
        return m_heightmap[(IslandTileCount - y) * IslandTileCount + x];
    };

    float posX = position.x / TileSize;
    float posY = position.z / TileSize;

    float intpart = 0.f;
    auto modX = std::modf(posX, &intpart) / TileSize;
    auto modY = std::modf(posY, &intpart) / TileSize; //normalise this for lerpitude

    std::int32_t x = static_cast<std::int32_t>(posX);
    std::int32_t y = static_cast<std::int32_t>(posY);

    float topLine = lerp(getHeightAt(x, y),  getHeightAt(x + 1, y), modX);
    float botLine = lerp(getHeightAt(x, y + 1), getHeightAt(x + 1, y + 1), modX);
    return lerp(topLine, botLine, modY) * IslandHeight;
};