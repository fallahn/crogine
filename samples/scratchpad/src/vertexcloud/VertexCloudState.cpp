//Auto-generated source file for Scratchpad Stub 04/11/2026, 10:20:39

#include "VertexCloudState.hpp"
#include "../PoissonDisk.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Wavetable.hpp>

namespace pd = thinks;

namespace
{
    static const inline std::string WeatherVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec2 a_texCoord0; //holds vertex offset for rain in X, and alpha in Y

#include CAMERA_UBO

    uniform mat4 u_worldMatrix;
    uniform mat4 u_worldViewMatrix;

    uniform float u_time;

    VARYING_OUT vec4 v_colour;


#if !defined VOLUME_HEIGHT
#define VOLUME_HEIGHT 4.0
#endif

#if defined (EASING)
    float easeOutCubic(float v)
    {
        return 1.0 - pow(1.0 - v, 3.0);
    }

    const float PI = 3.1412;
    float easeOutSine(float v)
    {
        return sin((v * PI) / 2.0);
    }
#endif

    void main()
    {
        mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
        vec4 position = a_position;

#if defined (DYNAMIC)
        position.y = mod(position.y - u_time, VOLUME_HEIGHT);

#if defined (EASING)
        float posNorm = position.y / VOLUME_HEIGHT;
        position.y = easeOutSine(posNorm) * VOLUME_HEIGHT;
#endif
#endif

        gl_Position = wvp * position;

        //vec4 worldPos = u_worldMatrix * position;
        v_colour = a_colour;

#if defined (POINT_PERSPECTIVE)
        gl_PointSize = u_projectionMatrix[1][1] / gl_Position.w * 16.0;
#else
        gl_PointSize = 5.0;
#endif
    }
)";


}

VertexCloudState::VertexCloudState(cro::StateStack& stack, cro::State::Context context)
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

    cro::App::getInstance().setClearColour(cro::Colour::CornflowerBlue);
}

//public
bool VertexCloudState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_EVENT_KEY_DOWN)
    {
        //switch (evt.key.key)
        //{
        //default: break;
        //case SDLK_BACKSPACE:
        //case SDLK_ESCAPE:
        //    requestStackClear();
        //    requestStackPush(0);
        //    break;
        //}

        const auto showCloud = 
            [this](std::int32_t idx)
            {
                for (auto e : m_cloudEnts)
                {
                    e.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                }
                m_cloudEnts[idx].getComponent<cro::Transform>().setScale(glm::vec3(1.f));

                for (auto e : m_textEnts)
                {
                    e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }
                m_textEnts[idx].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            };

        switch (evt.key.scancode)
        {
        default: break;
        case SDL_SCANCODE_1:
            showCloud(ShaderID::Static);
            break;
        case SDL_SCANCODE_2:
            showCloud(ShaderID::Dynamic);
            break;
        case SDL_SCANCODE_3:
            showCloud(ShaderID::DynamicEased);
            break;
        case SDL_SCANCODE_4:
            showCloud(ShaderID::DynamicPerspective);
            break;
        case SDL_SCANCODE_5:
            showCloud(ShaderID::TopDownGrid);
            break;
        case SDL_SCANCODE_6:
            showCloud(ShaderID::TopDownWrap);
            break;
        case SDL_SCANCODE_7:
            showCloud(ShaderID::LineWrap);
            break;
        case SDL_SCANCODE_W:
            cro::App::getWindow().setSize({ 640, 360 });
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void VertexCloudState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool VertexCloudState::simulate(float dt)
{
    static float accum = 0.f;
    accum += dt;

    for (auto [s, u] : m_timeUniforms)
    {
        if (s)
        {
            glUseProgram(s);
            glUniform1f(u, accum);
        }
    }

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void VertexCloudState::render()
{
    glEnable(GL_PROGRAM_POINT_SIZE);
    glLineWidth(3.f);
    glEnable(GL_LINE_SMOOTH);
    m_gameScene.render();
    m_uiScene.render();
}

//private
void VertexCloudState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void VertexCloudState::loadAssets()
{
}

void VertexCloudState::createScene()
{
    const std::array<float, 3> AreaStart = { -3.25f, -0.f, -4.f };
    const std::array<float, 3> AreaEnd = { 3.25f, 4.f, 2.f };
    const auto points = pd::PoissonDiskSampling(0.6f, AreaStart, AreaEnd, 30u, static_cast<std::uint32_t>(std::time(nullptr)));

    std::size_t meshID = 0;
    const auto vertexFlags = cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::UV0;
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(vertexFlags, 1, GL_POINTS, GL_UNSIGNED_SHORT));

    auto* meshData = &m_resources.meshes.getMesh(meshID);
    meshData->attributes[cro::Mesh::Attribute::Colour].glType = GL_UNSIGNED_BYTE;
    meshData->attributes[cro::Mesh::Attribute::Colour].glNormalised = GL_TRUE;
    meshData->attributes[cro::Mesh::Attribute::UV0].glType = GL_HALF_FLOAT;
    meshData->vertexSize = cro::MeshBuilder::getVertexSize(meshData->attributes);

    struct LargeVertex final
    {
        glm::vec3 position = glm::vec3(0.f);
        cro::Detail::ColourLowP colour = cro::Colour::White;
        std::uint32_t vertexOffset = 0;

        LargeVertex() : vertexOffset(glm::packHalf2x16({ 0.f, 1.f })) {};
        LargeVertex(glm::vec3 pos, cro::Detail::ColourLowP c) : position(pos), colour(c), vertexOffset(glm::packHalf2x16({ 0.f, 1.f })) {}
        LargeVertex(float x, float y, float z) : position(x, y, z), vertexOffset(glm::packHalf2x16({ 0.f, 1.f })) {}
    };

    std::vector<LargeVertex> verts;
    std::vector<std::uint16_t> indices;
    for (auto i = 0u; i < points.size(); i++)
    {
        indices.push_back(static_cast<std::uint16_t>(verts.size()));
        verts.emplace_back(points[i][0], points[i][1], points[i][2]);
    }

    meshData->vertexCount = indices.size();
    cro::DynamicMeshBuilder::setVertexData(*meshData, cro::DataArray(verts.data(), verts.size()));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    cro::DynamicMeshBuilder::setIndexData(*meshData, { {indices.data(), indices.size()} });

    meshData->boundingBox[0] = { AreaStart[0], AreaStart[1], AreaStart[2] };
    meshData->boundingBox[1] = { AreaEnd[0], AreaEnd[1], AreaEnd[2] };
    meshData->boundingSphere.centre = meshData->boundingBox[0] + ((meshData->boundingBox[1] - meshData->boundingBox[0]) / 2.f);
    meshData->boundingSphere.radius = glm::length((meshData->boundingBox[1] - meshData->boundingBox[0]) / 2.f);

    const auto& Fragment = cro::ModelRenderer::getDefaultFragmentShader(cro::ModelRenderer::FragmentShaderID::Unlit);
    m_resources.shaders.loadFromString(ShaderID::Static, 
        WeatherVertex, Fragment);

    auto* shader = &m_resources.shaders.get(ShaderID::Static);
    auto materialID = m_resources.materials.add(*shader);
    auto material = m_resources.materials.get(materialID);

    //static mesh
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();// .setPosition({ -2.5f, 0.f, 0.f, });
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    m_cloudEnts[ShaderID::Static] = entity;

    //falling 
    m_resources.shaders.loadFromString(ShaderID::Dynamic,
        WeatherVertex, Fragment, "#define DYNAMIC\n");
    
    shader = &m_resources.shaders.get(ShaderID::Dynamic);
    m_timeUniforms[ShaderID::Dynamic].shader = shader->getGLHandle();
    m_timeUniforms[ShaderID::Dynamic].uniform = shader->getUniformID("u_time");
    materialID = m_resources.materials.add(*shader);
    material = m_resources.materials.get(materialID);

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    m_cloudEnts[ShaderID::Dynamic] = entity;


    //falling with easing
    m_resources.shaders.loadFromString(ShaderID::DynamicEased,
        WeatherVertex, Fragment, "#define DYNAMIC\n#define EASING\n");

    shader = &m_resources.shaders.get(ShaderID::DynamicEased);
    m_timeUniforms[ShaderID::DynamicEased].shader = shader->getGLHandle();
    m_timeUniforms[ShaderID::DynamicEased].uniform = shader->getUniformID("u_time");
    materialID = m_resources.materials.add(*shader);
    material = m_resources.materials.get(materialID);

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    m_cloudEnts[ShaderID::DynamicEased] = entity;


    //falling with perspective
    m_resources.shaders.loadFromString(ShaderID::DynamicPerspective,
        WeatherVertex, Fragment, "#define DYNAMIC\n#define EASING\n#define POINT_PERSPECTIVE\n");

    shader = &m_resources.shaders.get(ShaderID::DynamicPerspective);
    m_timeUniforms[ShaderID::DynamicPerspective].shader = shader->getGLHandle();
    m_timeUniforms[ShaderID::DynamicPerspective].uniform = shader->getUniformID("u_time");
    materialID = m_resources.materials.add(*shader);
    material = m_resources.materials.get(materialID);

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    m_cloudEnts[ShaderID::DynamicPerspective] = entity;





    //creates a wireframe box around the volume
    verts =
    {
        LargeVertex(glm::vec3(AreaStart[0], AreaEnd[1], AreaStart[2]), cro::Colour::Red),
        LargeVertex(glm::vec3(AreaStart[0], AreaStart[1], AreaStart[2]), cro::Colour::Red),
        LargeVertex(glm::vec3(AreaEnd[0], AreaStart[1], AreaStart[2]), cro::Colour::Red),
        LargeVertex(glm::vec3(AreaEnd[0], AreaEnd[1], AreaStart[2]), cro::Colour::Red),

        LargeVertex(glm::vec3(AreaStart[0], AreaEnd[1], AreaEnd[2]), cro::Colour::Red),
        LargeVertex(glm::vec3(AreaStart[0], AreaStart[1], AreaEnd[2]), cro::Colour::Red),
        LargeVertex(glm::vec3(AreaEnd[0], AreaStart[1], AreaEnd[2]), cro::Colour::Red),
        LargeVertex(glm::vec3(AreaEnd[0], AreaEnd[1], AreaEnd[2]), cro::Colour::Red),
    };

    indices =
    {
        0,1, 1,2, 2,3, 3,0,

        4,5, 5,6, 6,7, 7,4,

        0,4 ,1,5, 2,6, 3,7
    };

    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(vertexFlags, 1, GL_LINES, GL_UNSIGNED_SHORT));

    meshData = &m_resources.meshes.getMesh(meshID);
    meshData->attributes[cro::Mesh::Attribute::Colour].glType = GL_UNSIGNED_BYTE;
    meshData->attributes[cro::Mesh::Attribute::Colour].glNormalised = GL_TRUE;
    meshData->attributes[cro::Mesh::Attribute::UV0].glType = GL_HALF_FLOAT;
    meshData->vertexSize = cro::MeshBuilder::getVertexSize(meshData->attributes);

    meshData->vertexCount = indices.size();
    cro::DynamicMeshBuilder::setVertexData(*meshData, cro::DataArray(verts.data(), verts.size()));

    submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    cro::DynamicMeshBuilder::setIndexData(*meshData, { {indices.data(), indices.size()} });

    meshData->boundingBox[0] = { AreaStart[0], AreaStart[1], AreaStart[2] };
    meshData->boundingBox[1] = { AreaEnd[0], AreaEnd[1], AreaEnd[2] };
    meshData->boundingSphere.centre = meshData->boundingBox[0] + ((meshData->boundingBox[1] - meshData->boundingBox[0]) / 2.f);
    meshData->boundingSphere.radius = glm::length((meshData->boundingBox[1] - meshData->boundingBox[0]) / 2.f);

    m_resources.shaders.loadFromString(ShaderID::Count,
        cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::Unlit), Fragment, "#define VERTEX_COLOUR\n");


    shader = &m_resources.shaders.get(ShaderID::Count);
    materialID = m_resources.materials.add(*shader);
    material = m_resources.materials.get(materialID);

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [this](cro::Entity e, float)
        {
            //QaD hack to hide the wirefram in 2D view
            const float scale = 1.f - 
                (m_cloudEnts[ShaderID::TopDownGrid].getComponent<cro::Transform>().getScale().x +
                    m_cloudEnts[ShaderID::TopDownWrap].getComponent<cro::Transform>().getScale().x +
                    m_cloudEnts[ShaderID::LineWrap].getComponent<cro::Transform>().getScale().x);
            e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
        };


    //camera callback
    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 1.8f, 4.f });
}

void VertexCloudState::createUI()
{
    const auto fontID = 10;
    m_resources.fonts.load(fontID, "assets/fonts/Michroma-Regular_Artist.ttf");

    const auto& font = m_resources.fonts.get(fontID);


    const auto createText = 
        [this, &font](const cro::String& text) 
        {
            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 12.f, 34.f });
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(font).setString(text);
            entity.getComponent<cro::Text>().setFillColour(cro::Colour::Black);
            entity.getComponent<cro::Text>().setCharacterSize(24);
            return entity;
        };
    m_textEnts[ShaderID::Static] = createText("Volume filled with points");
    m_textEnts[ShaderID::Static].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

    m_textEnts[ShaderID::Dynamic] = createText("Movement with time");
    m_textEnts[ShaderID::DynamicEased] = createText("Movement with time and easing");
    m_textEnts[ShaderID::DynamicPerspective] = createText("Added perspective with point size");



    //displays the grid / wrapping methods
    m_textEnts[ShaderID::TopDownGrid] = createText("Top-down view 3x3 chunks");
    m_textEnts[ShaderID::TopDownWrap] = createText("Top-down view Single chunk");
    m_textEnts[ShaderID::LineWrap] = createText(" ");



    const auto rotationCallback = 
        [](cro::Entity e, float)
        {
            static const auto Table = cro::Util::Wavetable::sine(0.25f);

            auto& idx = e.getComponent<cro::Callback>().getUserData<std::size_t>();
            const auto x = Table[idx];
            const auto y = Table[(idx + (Table.size() / 4)) % Table.size()];
            idx = (idx + 1) % Table.size();

            const auto basePos = glm::vec2(cro::App::getWindow().getSize()) / 2.f;
            e.getComponent<cro::Transform>().setPosition((glm::vec2(x, y) * 100.f) + basePos);
        };

    //grid method
    m_cloudEnts[ShaderID::TopDownGrid] = m_uiScene.createEntity();
    m_cloudEnts[ShaderID::TopDownGrid].addComponent<cro::Transform>().setScale(glm::vec2(0.f));

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 320.f, 240.f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINES);
    entity.getComponent<cro::Drawable2D>().setVertexData({
        cro::Vertex2D(glm::vec2(0.f, 15.f), cro::Colour::Yellow),
        cro::Vertex2D(glm::vec2(0.f, -15.f), cro::Colour::Yellow),
        cro::Vertex2D(glm::vec2(-15.f, 0.f), cro::Colour::Yellow),
        cro::Vertex2D(glm::vec2(15.f, 0.f), cro::Colour::Yellow),
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::size_t>(0);
    entity.getComponent<cro::Callback>().function = rotationCallback;
    
    auto camEnt = entity;
    m_cloudEnts[ShaderID::TopDownGrid].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -12.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Camera");
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Black);
    entity.getComponent<cro::Text>().setCharacterSize(12);
    camEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    const glm::vec2 basePos = camEnt.getComponent<cro::Transform>().getPosition();

    static constexpr float ChunkSize = 60.f;
    static constexpr float ChunkHalfSize = ChunkSize / 2.f;
    static constexpr float Stride = ChunkSize * 2.f;
    for (auto y = 0; y < 3; ++y)
    {
        for (auto x = 0; x < 3; ++x)
        {
            auto pos = basePos + glm::vec2((x * ChunkSize), (y * ChunkSize)) - glm::vec2(ChunkHalfSize);

            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(pos);
            entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
            entity.getComponent<cro::Drawable2D>().setVertexData({
                cro::Vertex2D(glm::vec2(-ChunkHalfSize), cro::Colour::Red),
                cro::Vertex2D(glm::vec2(ChunkHalfSize, -ChunkHalfSize), cro::Colour::Red),
                cro::Vertex2D(glm::vec2(ChunkHalfSize), cro::Colour::Red),
                cro::Vertex2D(glm::vec2(-ChunkHalfSize, ChunkHalfSize), cro::Colour::Red),
                cro::Vertex2D(glm::vec2(-ChunkHalfSize), cro::Colour::Red)
                });
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [camEnt, x, y](cro::Entity e, float)
                {
                    const auto camPos = camEnt.getComponent<cro::Transform>().getPosition();
                    const float posX = (std::floor(camPos.x / ChunkSize) * ChunkSize);
                    const float posY = (std::floor(camPos.y / ChunkSize) * ChunkSize);

                    //hah this would have actually been better to do the first time around
                    //but ah well... we're trying to emulate what happened for the sake of narration.
                    const glm::vec2 pos = { posX + (x * ChunkSize), posY + (y * ChunkSize) };
                    e.getComponent<cro::Transform>().setPosition(pos - glm::vec2(ChunkHalfSize));
                };

            m_cloudEnts[ShaderID::TopDownGrid].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }




    //wrapping method
    m_cloudEnts[ShaderID::TopDownWrap] = m_uiScene.createEntity();
    m_cloudEnts[ShaderID::TopDownWrap].addComponent<cro::Transform>().setScale(glm::vec2(0.f));


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 320.f, 240.f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINES);
    entity.getComponent<cro::Drawable2D>().setVertexData({
        cro::Vertex2D(glm::vec2(0.f, 15.f), cro::Colour::Yellow),
        cro::Vertex2D(glm::vec2(0.f, -15.f), cro::Colour::Yellow),
        cro::Vertex2D(glm::vec2(-15.f, 0.f), cro::Colour::Yellow),
        cro::Vertex2D(glm::vec2(15.f, 0.f), cro::Colour::Yellow),

        cro::Vertex2D(glm::vec2(-100.f), cro::Colour::Red),
        cro::Vertex2D(glm::vec2(-100.f, 100.f), cro::Colour::Red),

        cro::Vertex2D(glm::vec2(-100.f), cro::Colour::Red),
        cro::Vertex2D(glm::vec2(100.f, -100.f), cro::Colour::Red),

        cro::Vertex2D(glm::vec2(100.f), cro::Colour::Red),
        cro::Vertex2D(glm::vec2(100.f, -100.f), cro::Colour::Red),

        cro::Vertex2D(glm::vec2(100.f), cro::Colour::Red),
        cro::Vertex2D(glm::vec2(-100.f, 100.f), cro::Colour::Red),
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::size_t>(0);
    entity.getComponent<cro::Callback>().function = rotationCallback;

    auto vertEnt = entity;
    m_cloudEnts[ShaderID::TopDownWrap].getComponent<cro::Transform>().addChild(vertEnt.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -12.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Camera");
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Black);
    entity.getComponent<cro::Text>().setCharacterSize(12);
    vertEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    std::string vertShader =
        R"(
uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;

ATTRIBUTE vec2 a_position;
ATTRIBUTE MED vec2 a_texCoord0;
ATTRIBUTE LOW vec4 a_colour;

VARYING_OUT LOW vec4 v_colour;

const vec2 MinBounds = vec2(-100.0);
const vec2 MaxBounds = vec2(100.0);

void main()
{
    vec3 txPos = u_worldMatrix[3].xyz;

    vec2 position = a_position;
    position -= txPos.xy;

    vec2 bounds = MaxBounds - MinBounds;
    position += floor(-(position + MinBounds) / bounds) * bounds;

    gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(position, 0.0, 1.0);
    v_colour = a_colour;
    gl_PointSize = 5.0;
}
)";

    m_resources.shaders.loadFromString(ShaderID::TopDownWrap, vertShader, cro::RenderSystem2D::getDefaultFragmentShader());
    auto* shader = &m_resources.shaders.get(ShaderID::TopDownWrap);


    static constexpr std::array<float, 2U> MaxBounds = { 100.f, 100.f };
    static constexpr std::array<float, 2U> MinBounds = { -100.f, -100.f };
    const auto points = pd::PoissonDiskSampling(20.f, MinBounds, MaxBounds);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_POINTS);
    entity.getComponent<cro::Drawable2D>().setShader(shader);

    std::vector<cro::Vertex2D> verts;
    for (auto i = 0u; i < points.size(); ++i)
    {
        const auto pos = glm::vec2(points[i][0], points[i][1]);
        verts.emplace_back(pos, cro::Colour::Cyan);
    }
    entity.getComponent<cro::Drawable2D>().setVertexData(verts);
    vertEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




    //line wrapping
    vertShader =
        R"(
uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;

uniform float u_time;

ATTRIBUTE vec2 a_position;
ATTRIBUTE MED vec2 a_texCoord0;
ATTRIBUTE LOW vec4 a_colour;

VARYING_OUT LOW vec4 v_colour;

const float WrapDist = 200.0;

void main()
{
    vec3 txPos = u_worldMatrix[3].xyz;

    vec2 position = a_position;
    position.y = mod(position.y - (u_time * 140.0), WrapDist);

    gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(position, 0.0, 1.0);
    v_colour = a_colour;
}
)";
    m_resources.shaders.loadFromString(ShaderID::LineWrap, vertShader, cro::RenderSystem2D::getDefaultFragmentShader());
    shader = &m_resources.shaders.get(ShaderID::LineWrap);
    m_timeUniforms[ShaderID::LineWrap].shader = shader->getGLHandle();
    m_timeUniforms[ShaderID::LineWrap].uniform = shader->getUniformID("u_time");

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 320.f, 240.f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINES);
    entity.getComponent<cro::Drawable2D>().setShader(shader);
    entity.getComponent<cro::Drawable2D>().setVertexData({
        cro::Vertex2D(glm::vec2(5.f, 55.f), cro::Colour::Yellow),
        cro::Vertex2D(glm::vec2(15.f, -35.f), cro::Colour::Red),

        cro::Vertex2D(glm::vec2(-15.f, 15.f), cro::Colour::Yellow),
        cro::Vertex2D(glm::vec2(-5.f, -75.f), cro::Colour::Red),
        });
    m_cloudEnts[ShaderID::LineWrap] = entity;

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