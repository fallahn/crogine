//Auto-generated source file for Scratchpad Stub 24/12/2024, 12:04:36

#include "GKGameState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/graphics/ImageArray.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>

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

#define glCheck(x) x

namespace
{
    const std::string Frag =
R"(

uniform HIGH vec3 u_lightDirection;
uniform HIGH vec3 u_cameraWorldPosition;

VARYING_IN vec4 v_colour;
VARYING_IN HIGH vec3 v_normalVector;

OUTPUT

void main()
{
    float diffuseAmount = max(dot(normalize(v_normalVector), normalize(-u_lightDirection)), 0.0);

    FRAG_OUT = v_colour * diffuseAmount;
})";
}

GKGameState::GKGameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([&]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
bool GKGameState::handleEvent(const cro::Event& evt)
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

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void GKGameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool GKGameState::simulate(float dt)
{
    //TODO this should be long the camera forward vector...

    glm::vec3 movement(0.f);
    if (cro::Keyboard::isKeyPressed(SDLK_w))
    {
        movement.z -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_s))
    {
        movement.z += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_q))
    {
        movement.y += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_e))
    {
        movement.y -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_a))
    {
        movement.x -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_d))
    {
        movement.x += 1.f;
    }

    if (glm::length2(movement) != 0)
    {
        m_gameScene.getActiveCamera().getComponent<cro::Transform>().move(glm::normalize(movement) * 50.f * dt);
    }


    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void GKGameState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void GKGameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void GKGameState::loadAssets()
{

}

void GKGameState::createScene()
{
    loadMap("assets/gk/maps/01");




    //TODO camera is fixed Z from target
    //follows X of target
    //is fixed Y above target position (not camera position)
    //uses LookAt each frame for correct orientation


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 256.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 128.f, 10.8f, 2.f });
    m_gameScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -(cro::Util::Const::PI / 4.f));
}

void GKGameState::createUI()
{
    registerWindow([&]()
        {
            ImGui::Begin("sadf");
            auto texSize = glm::vec2(m_inputTexture.getSize());
            ImGui::Image(m_inputTexture, { texSize.x, texSize.y }, { 0.f, 1.f }, { 1.f, 0.f });
            
            ImGui::SameLine();

            texSize = glm::vec2(m_loadedTexture.getSize());
            ImGui::Image(m_loadedTexture, { texSize.x, texSize.y }, { 0.f, 1.f }, { 1.f, 0.f });


            static float t = -cro::Util::Const::PI / 4.f;
            if (ImGui::SliderFloat("Rot", &t, -cro::Util::Const::PI, cro::Util::Const::PI))
            {
                t = std::clamp(t, -cro::Util::Const::PI, cro::Util::Const::PI);
                m_gameScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, t);
            }

            const auto pos = m_gameScene.getActiveCamera().getComponent<cro::Transform>().getWorldPosition();
            ImGui::Text("Cam Pos: %3.2f, %3.2f, %3.2f", pos.x, pos.y, pos.z);


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

bool GKGameState::loadMap(const std::string& path)
{
    const auto heightPath = path + "/heightmap.png";
    cro::ImageArray<std::uint8_t> heightmap;

    if (!heightmap.loadFromFile(heightPath, true))
    {
        LogE << "Failed opening heightmap " << path << std::endl;
        return false;
    }

    //heightmap should be 257x257 which includes extra row / column
    //to create 256 tiles
    static constexpr std::uint32_t HeightMapSize = 257;
    if (heightmap.getDimensions().x != HeightMapSize
        || heightmap.getDimensions().y != HeightMapSize)
    {
        LogE << "heightmap data incorrect dimensions - expected {" << HeightMapSize << ", " << HeightMapSize << "}, got: " << heightmap.getDimensions() << std::endl;
        return false;
    }


    std::vector<std::uint8_t> debugArray(HeightMapSize * HeightMapSize * heightmap.getChannels());
    std::fill(debugArray.begin(), debugArray.end(), 0xff);


    //map is split into 16x16 chunks
    static constexpr std::int32_t ChunkCount = 16;
    static constexpr std::int32_t ChunkSize = (HeightMapSize - 1) / ChunkCount;
    static constexpr std::int32_t ChunkRowSize = HeightMapSize * ChunkSize;
    const auto stride = heightmap.getChannels();

    static constexpr float TileWorldSize = 5.f; //TODO increase to 5
    static constexpr float TileWorldHeight = 5.f; //max height of terrain

    const auto heightAtIndex = [&](std::uint32_t i)
        {
            return (static_cast<float>(heightmap[i]) / 255.f) * TileWorldHeight;
        };


    //TODO do we want to leave all entities at 0,0,0 and set the verts in absolute space?
    //if we offset the entity by chunk position we get the added bonus of proper frustum culling
    //of any entities parented to the chunk entity

    std::vector<float> verts;
    std::vector<std::uint32_t> indices;
    std::uint32_t vertexCount = 0;

    const auto addVertex = [&](glm::vec3 pos, cro::Colour c, glm::vec3 norm)
        {
            verts.push_back(pos.x);
            verts.push_back(pos.y);
            verts.push_back(pos.z);

            verts.push_back(c.getRed());
            verts.push_back(c.getGreen());
            verts.push_back(c.getBlue());
            verts.push_back(1.f);

            verts.push_back(norm.x);
            verts.push_back(norm.y);
            verts.push_back(norm.z);
        };

    for (auto chunkY = 0; chunkY < ChunkCount; ++chunkY)
    {
        for (auto chunkX = 0; chunkX < ChunkCount; ++chunkX)
        {
            const std::int32_t chunkIdx = ((chunkX * ChunkSize) + (chunkY * ChunkRowSize)) * stride;

            for (auto tileY = 0; tileY < ChunkSize; ++tileY)
            {
                for (auto tileX = 0; tileX < ChunkSize; ++tileX)
                {
                    const auto tileIdx = ((tileX + (tileY * HeightMapSize)) * stride) + chunkIdx;

                    debugArray[tileIdx] = heightmap[tileIdx];


                    const float x0 = (static_cast<float>(chunkX * ChunkSize) + tileX) * TileWorldSize;
                    const float z0 = -static_cast<float>((chunkY * ChunkSize) + tileY) * TileWorldSize;

                    const auto y00 = heightAtIndex(tileIdx);
                    const auto y01 = heightAtIndex(tileIdx + (HeightMapSize * stride));
                    const auto y10 = heightAtIndex(tileIdx + stride);
                    const auto y11 = heightAtIndex(tileIdx + (HeightMapSize * stride) + stride);

                    const float x1 = x0 + TileWorldSize;
                    const float z1 = z0 - TileWorldSize;

                    //0-------2
                    //|
                    //|   /
                    //|
                    //1-------3

                    const glm::vec3 v0(x0, y01, z1);
                    const glm::vec3 v1(x0, y00, z0);
                    const glm::vec3 v2(x1, y11, z1);
                    const glm::vec3 v3(x1, y10, z0);

                    const glm::vec3 norm = glm::normalize(glm::cross(v3 - v1, v0 - v1));
                    const float avgHeight = v1.y + ((v2.y - v1.y) / 2.f); //TODO use this to calc colour and cull quad below water

                    const auto baseIndex = vertexCount;
                    vertexCount += 4;

                    addVertex(v0, cro::Colour::White, norm);
                    addVertex(v1, cro::Colour::White, norm);
                    addVertex(v2, cro::Colour::White, norm);
                    addVertex(v3, cro::Colour::White, norm);

                    indices.push_back(baseIndex);
                    indices.push_back(baseIndex + 1);
                    indices.push_back(baseIndex + 2);
                    indices.push_back(baseIndex + 2);
                    indices.push_back(baseIndex + 1);
                    indices.push_back(baseIndex + 3);
                }
            }
        }
    }

    //auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::BuiltIn::VertexLit, cro::ShaderResource::BuiltInFlags::VertexColour);

    m_resources.shaders.loadFromString(0, cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::VertexLit), Frag, "#define VERTEX_COLOUR\n");
    auto matID = m_resources.materials.add(m_resources.shaders.get(0));
    auto material = m_resources.materials.get(matID);
    

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::Normal, 1, GL_TRIANGLES));
    auto* meshData = &m_resources.meshes.getMesh(meshID);

    meshData->vertexCount = vertexCount;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    meshData->boundingBox = { {0.f, 0.f, 0.f}, {256.f * TileWorldSize, 10.f, -256.f * TileWorldSize} };
    meshData->boundingSphere = meshData->boundingBox;

    //temp - for now we'll stick all the verts on here to check rendering
    cro::Entity entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();// .setPosition(glm::vec3(0.f, 0.f, 0.f));
    entity.addComponent<cro::Model>(*meshData, material);




    m_inputTexture.create(HeightMapSize, HeightMapSize, heightmap.getFormat());
    m_inputTexture.update(heightmap.data());

    m_loadedTexture.create(HeightMapSize, HeightMapSize, heightmap.getFormat());
    m_loadedTexture.update(debugArray.data());



    return true;
}