//Auto-generated source file for Scratchpad Stub 12/06/2026, 10:01:17

#include "SurrealState.hpp"

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

#include <crogine/util/Constants.hpp>

namespace
{
#include "Shaders.inl"

    struct ShaderID final
    {
        enum
        {
            Water,

            Count
        };
    };

    /*constexpr*/ float XRotation = -0.14f;
}

SurrealState::SurrealState(cro::StateStack& stack, cro::State::Context context)
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
bool SurrealState::handleEvent(const cro::Event& evt)
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

void SurrealState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool SurrealState::simulate(float dt)
{
    static float accum = 0.f;
    accum += dt;
    glUseProgram(m_waveShader.ID);
    glUniform1f(m_waveShader.timeUniform, accum);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void SurrealState::render()
{
    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.setActivePass(cro::Camera::Pass::Reflection);
    cam.reflectionBuffer.clear();
    m_gameScene.render();
    cam.reflectionBuffer.display();

    cam.setActivePass(cro::Camera::Pass::Final);
    m_gameScene.render();
    m_uiScene.render();
}

//private
void SurrealState::addSystems()
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

void SurrealState::loadAssets()
{
}

void SurrealState::createScene()
{
    m_gameScene.setCubemap("assets/skybox/blue/cubemap.ccm");
    //m_gameScene.setStarsAmount(1.f);

    //makes the skybox rotate
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [this](cro::Entity e, float dt)
        {
            static float YRotation = 0.f;
            YRotation += dt * 0.01f;

            glm::quat q = cro::Transform::QUAT_IDENTITY;
            q = glm::rotate(q, XRotation, cro::Transform::X_AXIS);
            q = glm::rotate(q, YRotation, cro::Transform::Y_AXIS);

            m_gameScene.setSkyboxOrientation(q);
        };


    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/water/plane.cmt"))
    {
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setScale(glm::vec3(4.f));
        md.createModel(entity);

        const std::string normalsPath = "assets/water/normals/";
        const auto files = cro::FileSystem::listFiles(normalsPath);

        if (!files.empty())
        {
            cro::Image img;
            img.loadFromFile(normalsPath + files[0]);
            m_arrayTexture.create(img.getSize().x, img.getSize().y);

            glBindTexture(GL_TEXTURE_2D_ARRAY, m_arrayTexture.getGLHandle());
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

            for (auto i = 0u; i < std::min(static_cast<std::uint32_t>(files.size()), m_arrayTexture.getLayerCount()); ++i)
            {
                if (i)
                {
                    img.loadFromFile(normalsPath + files[i]);
                }
                m_arrayTexture.insertLayer(img, i);
            }
        }

        const std::string FrameCount = "#define MAXFRAMES " + std::to_string(files.size()) + ".0\n";

        m_resources.shaders.loadFromString(ShaderID::Water,
            cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::VertexLit), 
            WaterFrag, "#define TEXTURED\n#define BUMP\n#define RX_SHADOWS\n#define REFLECTION_PLANE\n" + FrameCount);
        auto& shader = m_resources.shaders.get(ShaderID::Water);
        const auto matID = m_resources.materials.add(shader);
        m_waveShader.ID = shader.getGLHandle();
        m_waveShader.timeUniform = shader.getUniformID("u_time");
        //m_waveShader.skyUniform = shader.getUniformID("u_skyColour");

        auto mat = m_resources.materials.get(matID);
        cro::TextureID tid(m_arrayTexture.getGLHandle(), true);
        mat.setProperty("u_normalMap", tid);
        mat.setProperty("u_skybox", m_gameScene.getCubemap());

        entity.getComponent<cro::Model>().setMaterial(0, mat);
    }

    if (md.loadFromFile("assets/water/cone.cmt"))
    {
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -6.f, -0.9f, -4.f });
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, dt * 0.1f);

                e.getComponent<cro::Transform>().move({ 0.f, 1.f * dt, 0.f });
                if (e.getComponent<cro::Transform>().getPosition().y > 6.f)
                {
                    e.getComponent<cro::Transform>().move({ 0.f, -7.f, 0.f });
                }
            };
        md.createModel(entity);
    }
    if (md.loadFromFile("assets/water/cube.cmt"))
    {
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -0.f, 2.9f, -3.f });
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, dt * -0.1f);

                e.getComponent<cro::Transform>().move({ 0.f, 0.7f * dt, 0.f });
                if (e.getComponent<cro::Transform>().getPosition().y > 6.f)
                {
                    e.getComponent<cro::Transform>().move({ 0.f, -7.f, 0.f });
                }
            };
        md.createModel(entity);
    }
    if (md.loadFromFile("assets/water/torus.cmt"))
    {
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 5.f, 2.9f, -3.f });
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, dt * 0.3f);

                e.getComponent<cro::Transform>().move({ 0.f, 0.8f * dt, 0.f });
                if (e.getComponent<cro::Transform>().getPosition().y > 6.f)
                {
                    e.getComponent<cro::Transform>().move({ 0.f, -7.f, 0.f });
                }
            };
        md.createModel(entity);
    }



    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(80.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 50.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    cam.shadowMapBuffer.create(2048, 2048);
    cam.setBlurPassCount(1);
    cam.setMaxShadowDistance(10.f);
    cam.setShadowExpansion(1.f);

    cam.reflectionBuffer.create(1024, 1024);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 1.44f, 2.f });
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, XRotation);

    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.8f);
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.8f);

    registerWindow([this, &cam]() 
        {
            if (ImGui::Begin("Cam"))
            {
                static float c[3] = { 1.f, 1.f, 1.f };
                if (ImGui::ColorPicker3("Sky", c))
                {
                    m_gameScene.getSunlight().getComponent<cro::Sunlight>().setColour({ c[0], c[1], c[2] });
                }
                float hsv[3] = {};
                ImGui::ColorConvertRGBtoHSV(c[0], c[1], c[2], hsv[0], hsv[1], hsv[2]);
                ImGui::Text("%3.2f, %3.2f, %3.2f", hsv[0], hsv[1], hsv[2]);
                m_gameScene.setStarsAmount(1.f - hsv[2]);

                ImGui::Image(m_gameScene.getActiveCamera().getComponent<cro::Camera>().shadowMapBuffer.getTexture(0), { 128.f, 128.f }, { 0.f, 1.f }, { 1.f, 0.f });
                ImGui::SameLine();
                ImGui::Image(m_gameScene.getActiveCamera().getComponent<cro::Camera>().reflectionBuffer.getTexture(), { 128.f, 128.f }, { 0.f, 1.f }, { 1.f, 0.f });
            }
            ImGui::End();        
        });
}

void SurrealState::createUI()
{
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