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
#include <crogine/util/Wavetable.hpp>
#include <crogine/detail/OpenGL.hpp>

namespace
{
#include "PseutheShaders.inl"
#include "PseuthePostFX.inl"

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
        
        cacheState(States::ScratchPad::PseutheGame);
        cacheState(States::ScratchPad::PseutheMenu);
    });

    //TODO push menu on load
    requestStackPush(States::ScratchPad::PseutheGame);
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
    //m_backgroundBuffer.clear();
    //m_gameScene.render();
    //m_backgroundBuffer.display();

    //m_blurBufferH.clear();
    //m_backgroundQuad.draw();
    //m_blurBufferH.display();

    //m_blurBufferV.clear();
    //m_blurQuadH.draw();
    //m_blurBufferV.display();

    //m_blurQuadV.draw();

    m_gameScene.render();
}

//private
void PseutheBackgroundState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<PseutheBallSystem>(mb);
    m_gameScene.addSystem<cro::SpriteAnimator>(mb);
    m_gameScene.addSystem<cro::SpriteSystem2D>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::RenderSystem2D>(mb);
}

void PseutheBackgroundState::loadAssets()
{
    m_resources.shaders.loadFromString(ShaderID::BlurH, cro::SimpleDrawable::getDefaultVertexShader(), GaussianFrag, "#define HORIZONTAL\n");
    m_resources.shaders.loadFromString(ShaderID::BlurV, cro::SimpleDrawable::getDefaultVertexShader(), GaussianFrag, "#define VERTICAL\n");

    m_backgroundBuffer.create(SceneSize.x, SceneSize.y, false);
    m_backgroundQuad.setTexture(m_backgroundBuffer.getTexture());
    m_backgroundQuad.setShader(m_resources.shaders.get(ShaderID::BlurH));

    m_blurBufferH.create(SceneSize.x, SceneSize.y, false);
    m_blurQuadH.setTexture(m_blurBufferH.getTexture());
    m_blurQuadH.setShader(m_resources.shaders.get(ShaderID::BlurV));
    
    m_blurBufferV.create(SceneSize.x, SceneSize.y, false);
    m_blurQuadV.setTexture(m_blurBufferV.getTexture());
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
    entity.addComponent<cro::Transform>().setPosition({SceneSizeFloat.x / 2.f, SceneSizeFloat.y / 2.f, BackgroundDepth});
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
    createParticles();
    createBalls();

    //overlay for fade in
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, ScreenFadeDepth });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, SceneSizeFloat.y), cro::Colour::Black),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::Black),
            cro::Vertex2D(SceneSizeFloat, cro::Colour::Black),
            cro::Vertex2D(glm::vec2(SceneSizeFloat.x, 0.f), cro::Colour::Black),
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(1.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::max(0.f, currTime - dt);
            for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
            {
                v.colour.setAlpha(currTime);
            }
            if (currTime == 0)
            {
                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
        };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = std::bind(&cameraCallback, std::placeholders::_1);
    cameraCallback(cam);

    //cam.resizeCallback = [&](cro::Camera& c)
    //    {
    //        c.setOrthographic(0.f, SceneSizeFloat.x, 0.f, SceneSizeFloat.y, NearPlane, FarPlane);;
    //        c.viewport = { 0.f, 0.f, 1.f, 1.f };

    //        auto winSize = glm::vec2(cro::App::getWindow().getSize());
    //        auto viewScale = winSize.x / SceneSizeFloat.x;
    //        m_blurQuadV.setScale(glm::vec2(viewScale));

    //        float offset = (winSize.y - (SceneSize.y * viewScale)) / 2.f;
    //        m_blurQuadV.setPosition(glm::vec2(0.f, offset));
    //    };
    //cam.resizeCallback(cam);


    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 2.f });
}

void PseutheBackgroundState::createLightRays()
{
    //root not which moves the rays
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ SceneSizeFloat.x / 2.f, SceneSizeFloat.y + 200.f, LightRayDepth });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            static constexpr glm::vec2 Movement(10.f, 0.f);
            e.getComponent<cro::Transform>().move(Movement * dt);

            auto pos = e.getComponent<cro::Transform>().getPosition();
            if (pos.x > MaxLightPos)
            {
                pos.x -= MaxLightPos - MinLightPos;
                e.getComponent<cro::Transform>().setPosition(pos);
            }

            //on the shader the final brightness is smoothstep(0.2, 0.4, u_alpha) * (1.0 - smoothstep(0.6, 0.8, u_alpha));
            const float brightness = (pos.x / SceneSizeFloat.x);

            glUseProgram(m_shaderBlocks.lightRay.shaderID);
            glUniform1f(m_shaderBlocks.lightRay.intensityID, brightness);

            glUseProgram(m_shaderBlocks.ball.shaderID);
            glUniform1f(m_shaderBlocks.ball.intensityID, brightness);
            glUniform3f(m_shaderBlocks.ball.lightPosID, pos.x, pos.y, 10.f);

            const auto dist = (pos.x - (SceneSizeFloat.x / 2.f)) / (MaxLightPos / 2.f);
            const float rotation = dist * (cro::Util::Const::PI / 8.f);
            e.getComponent<cro::Transform>().setRotation(-rotation);
        };
    auto lightRoot = entity;
    //registerWindow([lightRoot]() mutable
    //    {
    //        ImGui::Begin("sdf");
    //        static float pos = 0.f;

    //        if (ImGui::SliderFloat("S", &pos, 0.f, SceneSizeFloat.x))
    //        {
    //            lightRoot.getComponent<cro::Transform>().setPosition({ pos, SceneSizeFloat.y + 200.f });
    //        }
    //    
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
    m_shaderBlocks.lightRay.shaderID = shader.getGLHandle();
    m_shaderBlocks.lightRay.intensityID = shader.getUniformID("u_alpha");

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

void PseutheBackgroundState::createParticles()
{
    struct Particle final
    {
        glm::vec3 velocity = glm::vec3(0.f);
        glm::vec3 position = glm::vec3(0.f);
        float angularVelocity = 0.f;
        float rotation = 0.f;

        glm::vec3 scale = glm::vec3(1.f);
        std::array<cro::Vertex2D, 6u> vertices = {};

        void update(float dt)
        {
            position += velocity * dt;
            rotation += angularVelocity * dt;

            if (position.x < MinLightPos)
            {
                position.x += SceneSizeFloat.x;
            }
            else if (position.x > MaxLightPos)
            {
                position.x -= SceneSizeFloat.x;
            }

            if (position.y < MinLightPos)
            {
                position.y += SceneSizeFloat.y;
            }
            else if (position.y > (SceneSizeFloat.y + MinLightPos))
            {
                position.y -= SceneSizeFloat.y;
            }
        }

        std::vector<cro::Vertex2D> getTransformedVertices() const
        {
            glm::mat4 tx = glm::mat4(1.f);
            tx = glm::translate(tx, position);
            tx = glm::rotate(tx, rotation, cro::Transform::Z_AXIS);
            tx = glm::scale(tx, scale);

            std::vector<cro::Vertex2D> retVal;
            for (auto v : vertices)
            {
                v.position = glm::vec2(tx * glm::vec4(v.position, 0.f, 1.f));
                retVal.push_back(v);
            }

            return retVal;
        }
    };

    static constexpr std::uint32_t MaxParticles = 100;
    static constexpr float MaxVelLength = 40.f;
    static constexpr float BaseColour = 190.f / 255.f;
    static constexpr float BaseAlpha = 150.f / 255.f;
    static constexpr float ParticleSize = 16.f;
    static constexpr float ParticleHalfSize = ParticleSize / 2.f;
    static constexpr float CoordWidth = 1.f / 4.f; //4x4 texture

    struct ParticleData final
    {
        std::vector<Particle> particles;

        ParticleData()
        {
            const glm::vec3 DefaultVelocity =
            {
            static_cast<float>(cro::Util::Random::value(-MaxVelLength, MaxVelLength)),
            static_cast<float>(cro::Util::Random::value(-MaxVelLength, MaxVelLength)),
            0.f
            };

            std::vector<std::array<float, 2u>> positions = pd::PoissonDiskSampling(110.f, std::array{ 0.f, 0.f }, std::array{ SceneSizeFloat.x, SceneSizeFloat.y });
            for (auto i = 0u; i < positions.size() && i < MaxParticles; ++i)
            {
                glm::vec3 pos = glm::vec3(positions[i][0], positions[i][1], 0.f);

                const float scale = static_cast<float>(cro::Util::Random::value(2, 10)) / 10.f;

                Particle p;
                p.angularVelocity = static_cast<float>(cro::Util::Random::value(-120, 120)) * cro::Util::Const::degToRad;
                p.velocity = scale * scale * DefaultVelocity;
                p.scale = glm::vec3(scale);
                p.position = pos;
                p.rotation = static_cast<float>(cro::Util::Random::value(0, 360)) * cro::Util::Const::degToRad;
                
                const cro::FloatRect TextureRect =
                {
                    cro::Util::Random::value(0,3) * CoordWidth,
                    cro::Util::Random::value(0,3) * CoordWidth,
                    CoordWidth, CoordWidth
                };

                const auto c = (scale * BaseColour);
                cro::Colour pColour(c, c, c, BaseAlpha * scale);
                p.vertices[0] = cro::Vertex2D(glm::vec2(-ParticleHalfSize, ParticleHalfSize), 
                    glm::vec2(TextureRect.left, TextureRect.bottom + TextureRect.height), pColour);
                p.vertices[1] = cro::Vertex2D(glm::vec2(-ParticleHalfSize), 
                    glm::vec2(TextureRect.left, TextureRect.bottom), pColour);
                p.vertices[2] = cro::Vertex2D(glm::vec2(ParticleHalfSize), 
                    glm::vec2(TextureRect.left + TextureRect.width, TextureRect.bottom + TextureRect.height), pColour);

                p.vertices[3] = cro::Vertex2D(glm::vec2(ParticleHalfSize), 
                    glm::vec2(TextureRect.left + TextureRect.width, TextureRect.bottom + TextureRect.height), pColour);
                p.vertices[4] = cro::Vertex2D(glm::vec2(-ParticleHalfSize), 
                    glm::vec2(TextureRect.left, TextureRect.bottom), pColour);
                p.vertices[5] = cro::Vertex2D(glm::vec2(ParticleHalfSize, -ParticleHalfSize), 
                    glm::vec2(TextureRect.left + TextureRect.width, TextureRect.bottom), pColour);

                particles.push_back(p);
            }
        }

        void operator()(cro::Entity e, float dt)
        {
            //for some reason we can't recycle the existing vector
            //on nvidia cards, it causes a crash in the driver trying
            //to dereference a nullptr
            std::vector<cro::Vertex2D> verts;
            verts.reserve(600);

            //TODO we could parallel execute this but perfs not a problem
            //also apple suck.
            for (auto& p : particles)
            {
                p.update(dt);
                auto pVerts = p.getTransformedVertices();
                verts.insert(verts.end(), pVerts.begin(), pVerts.end());
            }
            e.getComponent<cro::Drawable2D>().setVertexData(verts);
        }
    };

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, ParticleDepth });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Drawable2D>().updateLocalBounds(cro::FloatRect(glm::vec2(0.f), SceneSizeFloat));
    entity.getComponent<cro::Drawable2D>().setTexture(&m_resources.textures.get("pseuthe/assets/images/particles/field.png"));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = ParticleData();
}

void PseutheBackgroundState::createBalls()
{
    //balls
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("pseuthe/assets/sprites/ball.spt", m_resources.textures);

    auto& normalMap = m_resources.textures.get("pseuthe/assets/images/particles/ball_normal_animated.png");
    m_resources.shaders.loadFromString(ShaderID::Ball, OrbVertex, OrbFragment);
    auto* shader = &m_resources.shaders.get(ShaderID::Ball);
    m_shaderBlocks.ball.shaderID = shader->getGLHandle();
    m_shaderBlocks.ball.intensityID = shader->getUniformID("u_lightIntensity");
    m_shaderBlocks.ball.lightPosID = shader->getUniformID("u_lightPosition");

    static constexpr float BaseAlpha = 240.f / 255.f;
    static constexpr float BaseColour = 185.f / 255.f;

    //for the sake of simplicity we'll use 1px/m - so max size is 128m
    std::vector<std::array<float, 2u>> p = pd::PoissonDiskSampling(256.f, std::array{ 0.f, 0.f }, std::array{ SceneSizeFloat.x, SceneSizeFloat.y });
    for (auto i = 0u; i < BallCount && i < p.size(); ++i)
    {
        const glm::vec2 pos(p[i][0], p[i][1]);
        const float dia = static_cast<float>(cro::Util::Random::value(MinBallSize, MaxBallSize));

        const float c = (dia / MaxBallSize) * BaseColour;
        cro::Colour colour(c, c, c, (dia / MaxBallSize) * BaseAlpha);

        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, BallDepth));
        entity.getComponent<cro::Transform>().setScale(glm::vec2(dia / BallSize));
        entity.getComponent<cro::Transform>().setOrigin(glm::vec2(BallSize) / 2.f);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("ball");
        entity.getComponent<cro::Sprite>().setColour(colour);
        entity.addComponent<cro::SpriteAnimation>().play(0);
        entity.addComponent<PseutheBall>(dia / 2.f);

        entity.getComponent<cro::Drawable2D>().setShader(shader);
        entity.getComponent<cro::Drawable2D>().bindUniform("u_normalMap", cro::TextureID(normalMap));
    }
}