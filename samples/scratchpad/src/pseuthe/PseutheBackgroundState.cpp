//Auto-generated source file for Scratchpad Stub 20/08/2024, 12:39:17

#include "PseutheBackgroundState.hpp"
#include "PseutheConsts.hpp"
#include "PoissonDisk.hpp"
#include "PseutheBallSystem.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/detail/OpenGL.hpp>

namespace
{
#include "PseutheShaders.inl"

    constexpr std::size_t BallCount = 19;
    constexpr std::int32_t MinBallSize = 40;
    constexpr std::int32_t MaxBallSize = 92;
    constexpr float BallSize = 128.f; //this is the sprite size

    constexpr std::uint8_t Alpha = 60;
    const std::vector<cro::Colour> BackgroundColours =
    {
        cro::Colour(32u, 37u, 102u, Alpha),
        cro::Colour(32u, 93u, 102u, Alpha),
        cro::Colour(32u, 102u, 74u, Alpha),
        cro::Colour(52u, 32u, 102u, Alpha)
    };

}

PseutheBackgroundState::PseutheBackgroundState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        
        //TODO push menu on load
        cacheState(States::ScratchPad::PseutheGame);
        cacheState(States::ScratchPad::PseutheMenu);
    });
}

//public
bool PseutheBackgroundState::handleEvent(const cro::Event& evt)
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
            cro::App::quit();
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    return true;
}

void PseutheBackgroundState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
}

bool PseutheBackgroundState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    return true;
}

void PseutheBackgroundState::render()
{
    m_gameScene.render();
}

//private
void PseutheBackgroundState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<PseutheBallSystem>(mb);
    m_gameScene.addSystem<cro::SpriteSystem2D>(mb);
    m_gameScene.addSystem<cro::SpriteAnimator>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::RenderSystem2D>(mb);
}

void PseutheBackgroundState::loadAssets()
{
}

void PseutheBackgroundState::createScene()
{
    //background
    std::vector<cro::Vertex2D> verts;
    verts.emplace_back(glm::vec2(0.f), cro::Colour::Black);

    constexpr float FanRadius = 1300.f;
    constexpr float increment = cro::Util::Const::TAU / 8.f;
    for (auto angle = 0.f; angle <= cro::Util::Const::TAU; angle += increment)
    {
        verts.emplace_back(glm::vec2(FanRadius * std::cos(angle), FanRadius * std::sin(angle)), BackgroundColours[0]);
    }

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({SceneSizeFloat.x / 2.f, SceneSizeFloat.y / 2.f, -10.f});
    entity.addComponent<cro::Drawable2D>().setVertexData(verts);
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLE_FAN);

    struct BackgroundData final
    {
        bool shrink = false;

        float currentTime = 0.f;
        std::uint32_t indexA = 0;
        std::uint32_t indexB = 1;
        const float CycleTime = 50.f;
    };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<BackgroundData>();
    entity.getComponent<cro::Callback>().function =
    [](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<BackgroundData>();
            data.currentTime += dt;

            //cycle colour
            if (data.currentTime > data.CycleTime)
            {
                data.currentTime -= data.CycleTime;
                data.indexA = (data.indexA + 1) % BackgroundColours.size();
                data.indexB = (data.indexB + 1) % BackgroundColours.size();
            }

            float ratio = data.currentTime / data.CycleTime;
            glm::vec4 colour = glm::mix(BackgroundColours[data.indexA].getVec4(), BackgroundColours[data.indexB].getVec4(), ratio);

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto i = 1u; i < verts.size(); ++i)
            {
                verts[i].colour = colour;
            }

            //modify size
            static constexpr float MaxScale = 1.2f;
            auto scale = e.getComponent<cro::Transform>().getScale();
            const float amount = dt * 0.01f;
            if (data.shrink)
            {
                scale.x -= amount;
                scale.y -= amount;
                if (scale.x < 1.f)
                {
                    data.shrink = false;
                }
            }
            else
            {
                scale.x += amount;
                scale.y += amount;
                if (scale.x > MaxScale)
                {
                    data.shrink = true;
                }
            }
            e.getComponent<cro::Transform>().setScale(scale);
        };


    createLightRays();


    //balls
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("pseuthe/assets/sprites/ball.spt", m_resources.textures);

    //for the sake of simplicity we'll use 1px/m - so max size is 128m
    std::vector<std::array<float, 2u>> p = pd::PoissonDiskSampling(256.f, std::array{ 0.f, 0.f }, std::array{ SceneSizeFloat.x, SceneSizeFloat.y });
    for (auto i = 0u; i < BallCount && i < p.size(); ++i)
    {
        const glm::vec2 pos(p[i][0], p[i][1]);
        const float dia = static_cast<float>(cro::Util::Random::value(MinBallSize, MaxBallSize));

        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(pos);
        entity.getComponent<cro::Transform>().setScale(glm::vec2(dia / BallSize));
        entity.getComponent<cro::Transform>().setOrigin(glm::vec2(BallSize) / 2.f);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("ball");
        entity.addComponent<cro::SpriteAnimation>().play(0);
        entity.addComponent<PseutheBall>(dia / 2.f);
    }


    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = std::bind(cameraCallback, std::placeholders::_1);
    cameraCallback(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 2.f });
}

void PseutheBackgroundState::createLightRays()
{
    struct LightPosData final
    {
        std::uint32_t shaderID = 0;
        std::int32_t uniformID = -1;
    };

    //root not which moves the rays
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ SceneSizeFloat.x / 2.f, SceneSizeFloat.y + 200.f, -10.f });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<LightPosData>();
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            static constexpr glm::vec2 Movement(10.f, 0.f);
            e.getComponent<cro::Transform>().move(Movement * dt);

            auto pos = e.getComponent<cro::Transform>().getPosition();
            if (pos.x > MaxLightPos)
            {
                pos.x -= MaxLightPos - MinLightPos;
                e.getComponent<cro::Transform>().setPosition(pos);
            }

            //on the shader the final brightness is 1.0 - pow(cos(brightness), 4.f)
            //this needs to be applied to the balls as 'intensity' - probably UBO/shader include
            const float brightness = (pos.x / SceneSizeFloat.x) * cro::Util::Const::PI;

            const auto& data = e.getComponent<cro::Callback>().getUserData<LightPosData>();
            glUseProgram(data.shaderID);
            glUniform1f(data.uniformID, brightness);

            const auto dist = (pos.x - (SceneSizeFloat.x / 2.f)) / (MaxLightPos / 2.f);
            const float rotation = dist * (cro::Util::Const::PI / 8.f);
            e.getComponent<cro::Transform>().setRotation(-rotation);
        };
    auto lightRoot = entity;

    //registerWindow([lightRoot]() mutable
    //    {
    //        ImGui::Begin("SEDF");
    //        static float P = 960.f;

    //        if (ImGui::SliderFloat("p", &P, MinLightPos, MaxLightPos))
    //        {
    //            auto pos = lightRoot.getComponent<cro::Transform>().getPosition();
    //            pos.x = P;
    //            lightRoot.getComponent<cro::Transform>().setPosition(pos);
    //        }

    //        ImGui::End();
    //    });

    static constexpr float FalloffSize = 100.f;
    static constexpr float RayLength = -3000.f;
    const cro::Colour RayColour(std::uint8_t(255u), 250u, 190u, 28u);

    struct RayData final
    {
        RayData()
        {
            const float StepCount = 60.f / static_cast<float>(cro::Util::Random::value(2, 3));
            const float Step = cro::Util::Const::TAU / StepCount;
            const float Amplitude = static_cast<float>(cro::Util::Random::value(3, 10));

            for (auto i = 0.f; i < StepCount; ++i)
            {
                wavetable.push_back(std::sin(Step * i) * Amplitude);
            }
            currIndex = cro::Util::Random::value(0, static_cast<std::int32_t>(StepCount) - 1);
        }

        void operator()(cro::Entity e, float dt)
        {
            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            auto v = wavetable[currIndex];
            verts[0].position.x = verts[1].position.x = verts[4].position.x = -FalloffSize + v;
            verts[12].position.x = verts[13].position.x = verts[16].position.x = FalloffSize - v;

            currIndex = (currIndex + 1) % wavetable.size();
        }

        std::int32_t currIndex = 0;
        std::vector<float> wavetable;
    };

    m_resources.shaders.loadFromString(ShaderID::LightRay, RayVertex, RayFragment);
    auto& shader = m_resources.shaders.get(ShaderID::LightRay);
    auto& lightPosData = lightRoot.getComponent<cro::Callback>().getUserData<LightPosData>();
    lightPosData.shaderID = shader.getGLHandle();
    lightPosData.uniformID = shader.getUniformID("u_alpha");

    //each ray
    static constexpr std::int32_t RayCount = 7;
    static constexpr std::int32_t MaxAngle = 100 / RayCount;
    float rotation = 42.f;

    for (auto i = 0; i < RayCount; ++i)
    {
        rotation -= static_cast<float>(cro::Util::Random::value(4, MaxAngle));

        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setRotation(rotation* cro::Util::Const::degToRad);
        entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
        entity.getComponent< cro::Drawable2D>().setBlendMode(cro::Material::BlendMode::Additive);
        entity.getComponent<cro::Drawable2D>().setShader(&shader);
        entity.getComponent<cro::Drawable2D>().setDoubleSided(true);

        std::vector<cro::Vertex2D> verts =
        {
            cro::Vertex2D(glm::vec2(-FalloffSize, 0.f), cro::Colour::Transparent),
            cro::Vertex2D(glm::vec2(-FalloffSize, RayLength), RayColour),
            cro::Vertex2D(glm::vec2(-1.f, 0.f), RayColour),

            cro::Vertex2D(glm::vec2(-1.f, 0.f), RayColour),
            cro::Vertex2D(glm::vec2(-FalloffSize, RayLength), RayColour),
            cro::Vertex2D(glm::vec2(-1.f, RayLength), RayColour),


            cro::Vertex2D(glm::vec2(-1.f, 0.f), RayColour),
            cro::Vertex2D(glm::vec2(-1.f, RayLength), RayColour),
            cro::Vertex2D(glm::vec2(1.f, 0.f), RayColour),

            cro::Vertex2D(glm::vec2(1.f, 0.f), RayColour),
            cro::Vertex2D(glm::vec2(-1.f, RayLength), RayColour),
            cro::Vertex2D(glm::vec2(1.f, RayLength), RayColour),


            cro::Vertex2D(glm::vec2(FalloffSize, 0.f), cro::Colour::Transparent),
            cro::Vertex2D(glm::vec2(FalloffSize, RayLength), RayColour),
            cro::Vertex2D(glm::vec2(1.f, 0.f), RayColour),

            cro::Vertex2D(glm::vec2(1.f, 0.f), RayColour),
            cro::Vertex2D(glm::vec2(FalloffSize, RayLength), RayColour),
            cro::Vertex2D(glm::vec2(1.f, RayLength), RayColour),
        };
        entity.getComponent<cro::Drawable2D>().setVertexData(verts);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = RayData();
        lightRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }
}