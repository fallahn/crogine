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
#include <crogine/util/Wavetable.hpp>

namespace
{
#include "Shaders.inl"

    struct ShaderID final
    {
        enum
        {
            Water,
            Shape,

            Count
        };
    };

    struct RenderFlags final
    {
        enum
        {
            Final = (1 << 0)
        };
    };

    constexpr float XRotation = -0.14f;
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

    //in my infinite wisdom I made the render flags for refraction
    //the same as final and now we have to hack out water rendering...
    cam.setRenderFlags(cro::Camera::Pass::Final, ~RenderFlags::Final);
    cam.setActivePass(cro::Camera::Pass::Refraction);
    cam.refractionBuffer.clear();
    m_gameScene.render();
    cam.refractionBuffer.display();


    cam.setRenderFlags(cro::Camera::Pass::Final, /*cro::RenderFlags::All*/RenderFlags::Final);
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


    cro::Entity waterEntity;
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/water/plane.cmt"))
    {
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setScale(glm::vec3(4.f));
        md.createModel(entity);

        entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Final);

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
        const auto waterMatID = m_resources.materials.add(shader);
        m_waveShader.ID = shader.getGLHandle();
        m_waveShader.timeUniform = shader.getUniformID("u_time");
        /*m_waveShader.fogDensity = shader.getUniformID("u_density");
        m_waveShader.fogEnd = shader.getUniformID("u_fogEnd");
        m_waveShader.fogStart = shader.getUniformID("u_fogStart");*/

        auto& mat = m_resources.materials.get(waterMatID);
        cro::TextureID tid(m_arrayTexture.getGLHandle(), true);
        mat.setProperty("u_normalMap", tid);
        mat.setProperty("u_skybox", m_gameScene.getCubemap());

        entity.getComponent<cro::Model>().setMaterial(0, mat);
        waterEntity = entity;
    }

    m_resources.shaders.loadFromString(ShaderID::Shape,
        cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::VertexLit),
        ShapeFrag);

    auto& shader = m_resources.shaders.get(ShaderID::Shape);
    const auto matID = m_resources.materials.add(shader);
    auto material = m_resources.materials.get(matID);

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
                    e.getComponent<cro::Transform>().move({ 0.f, -12.f, 0.f });
                }
            };
        md.createModel(entity);

        //TODO use a lazy loader save creating unused shaders
        entity.getComponent<cro::Model>().setMaterial(0, material);
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_maskColour", glm::vec4(0.5f, 0.5f, 1.f, 0.5f));
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", glm::vec4(1.f, 1.f, 0.2f, 1.f));
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
                    e.getComponent<cro::Transform>().move({ 0.f, -12.f, 0.f });
                }
            };
        md.createModel(entity);

        //TODO use a lazy loader save creating unused shaders
        entity.getComponent<cro::Model>().setMaterial(0, material);
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_maskColour", glm::vec4(0.5f, 0.5f, 1.f, 0.5f));
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", glm::vec4(0.f, 0.3f, 0.94f, 1.f));
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
                    e.getComponent<cro::Transform>().move({ 0.f, -12.f, 0.f });
                }
            };
        md.createModel(entity);

        //TODO use a lazy loader save creating unused shaders
        entity.getComponent<cro::Model>().setMaterial(0, material);
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_maskColour", glm::vec4(0.5f, 0.5f, 1.f, 0.5f));
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", glm::vec4(0.f, 1.f, 0.2f, 1.f));
    }



    if (md.loadFromFile("assets/water/head.cmt"))
    {
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 10.f, -20.f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, cro::Util::Const::PI);
        md.createModel(entity);

        struct HeadData final
        {
            std::vector<float> wavetable1 = cro::Util::Wavetable::sine(0.1f);
            std::vector<float> wavetable2 = cro::Util::Wavetable::sine(0.02f, 0.35f);
            std::uint32_t index1 = 0;
            std::uint32_t index2 = 0;
        };

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<HeadData>();
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float)
            {
                auto& [table1, table2, idx1, idx2] = e.getComponent<cro::Callback>().getUserData<HeadData>();
                e.getComponent<cro::Transform>().setPosition({-16.f, 10.f + (table1[idx1] * 2.f), -30.f });

                glm::quat q = glm::rotate(cro::Transform::QUAT_IDENTITY, 0.5f, cro::Transform::X_AXIS);
                q = glm::rotate(q, table2[idx2] + cro::Util::Const::PI, cro::Transform::Y_AXIS);
                e.getComponent<cro::Transform>().setRotation(q);

                idx1 = (idx1 + 1) % table1.size();
                idx2 = (idx2 + 1) % table2.size();
            };
    }




    auto resize = [](cro::Camera& cam)
    {
        //NOTE if we change the far plane update the fog calc in the water shader!
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

    cro::RenderTarget::Context ctx;
    ctx.depthTexture = true;
    ctx.width = 512;
    ctx.height = 512;
    cam.refractionBuffer.create(ctx);
    cam.refractionBuffer.setSmooth(true);
    //if (waterEntity.isValid())
    //{
    //    waterEntity.getComponent<cro::Model>().setMaterialProperty(0, "u_depthMap", cam.refractionBuffer.getDepthTexture());
    //}

    cam.setRenderFlags(cro::Camera::Pass::Reflection, ~RenderFlags::Final);
    //cam.setRenderFlags(cro::Camera::Pass::Refraction, ~RenderFlags::Final);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 1.44f, 2.f });
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, XRotation);

    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.8f);
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.2f);

    registerWindow([this, &cam]() 
        {
            if (ImGui::Begin("Cam"))
            {
                /*static float density = 0.5f;
                static float fogStart = 0.01f;
                static float fogEnd = 5.5f;

                if (ImGui::SliderFloat("Density", &density, 0.f, 1.f))
                {
                    glUseProgram(m_waveShader.ID);
                    glUniform1f(m_waveShader.fogDensity, density);
                }
                if (ImGui::SliderFloat("Start", &fogStart, 0.f, fogEnd))
                {
                    glUseProgram(m_waveShader.ID);
                    glUniform1f(m_waveShader.fogStart, fogStart);
                }
                if (ImGui::SliderFloat("End", &fogEnd, fogStart, 50.f))
                {
                    glUseProgram(m_waveShader.ID);
                    glUniform1f(m_waveShader.fogEnd, fogEnd);
                }*/

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
                ImGui::SameLine();
                ImGui::Image(m_gameScene.getActiveCamera().getComponent<cro::Camera>().refractionBuffer.getDepthTexture(), { 128.f, 128.f }, { 0.f, 1.f }, { 1.f, 0.f });
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