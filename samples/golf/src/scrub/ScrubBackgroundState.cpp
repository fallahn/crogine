/*-----------------------------------------------------------------------

Matt Marchant 2024
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "ScrubBackgroundState.hpp"
#include "ScrubConsts.hpp"
#include "ScrubSharedData.hpp"
#include "../golf/PoissonDisk.hpp"
#include "../golf/GameConsts.hpp"
#include "../golf/SpectatorSystem.hpp"
#include "../golf/CloudSystem.hpp"

#include <crogine/ecs/components/BillboardCollection.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/detail/OpenGL.hpp>

namespace
{
#include "../golf/shaders/Blur.inl"

    static constexpr std::array Path01 = { glm::vec3(-30.f, 0.f, 6.4f),glm::vec3(-10.f, 0.f, 6.4f),  glm::vec3(10.f, 0.f, 6.4f),  glm::vec3(30.f, 0.f, 6.4f) };
    static constexpr std::array Path02 = { glm::vec3(-30.f, 0.f, 8.4f),glm::vec3(-10.f, 0.f, 8.4f),  glm::vec3(10.f, 0.f, 8.4f),  glm::vec3(30.f, 0.f, 8.4f) };
    static constexpr std::array Path03 = { glm::vec3(-7.f, 0.f, 30.f),glm::vec3(-7.f, 0.f, 21.f), glm::vec3(-7.f, 0.f, 11.5f), glm::vec3(-7.f, 0.f, 6.5f) };
    //static constexpr std::array Path03 = { glm::vec3(76.f, 0.f, 9.5f), glm::vec3(8.f, 0.f, 9.5f), glm::vec3(-30.f, 0.f, 9.5f),glm::vec3(-70.f, 0.f, 9.5f) };

    namespace Debug
    {
        float rotate = 0.f;
    }
}

ScrubBackgroundState::ScrubBackgroundState(cro::StateStack& ss, cro::State::Context ctx, SharedScrubData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus()),
    m_sharedScrubData   (sd)
{
    ctx.mainWindow.loadResources([&]() 
        {
#ifndef HIDE_BACKGROUND
            addSystems();
            loadAssets();
            createScene();
#endif
            sd.fonts = std::make_unique<cro::FontResource>();
            sd.fonts->load(sc::FontID::Title, "assets/arcade/scrub/fonts/BowlbyOne-Regular.ttf");
            sd.fonts->load(sc::FontID::Body, "assets/arcade/scrub/fonts/Candal-Regular.ttf");

            cacheState(StateID::ScrubGame);
            cacheState(StateID::ScrubAttract);
            cacheState(StateID::ScrubPause);
        });
#ifdef CRO_DEBUG_
    requestStackPush(StateID::ScrubGame);
#else
    requestStackPush(StateID::ScrubAttract);
#endif
}

ScrubBackgroundState::~ScrubBackgroundState()
{
    m_sharedScrubData.fonts.reset();
}

//public
bool ScrubBackgroundState::handleEvent(const cro::Event& evt)
{
    m_scene.forwardEvent(evt);
    return false;
}

void ScrubBackgroundState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool ScrubBackgroundState::simulate(float dt)
{
    //glm::vec3 move(0.f);
    //if (cro::Keyboard::isKeyPressed(SDLK_e))
    //{
    //    move.z += dt;
    //}
    //if (cro::Keyboard::isKeyPressed(SDLK_q))
    //{
    //    move.z -= dt;
    //}
    //m_scene.getActiveCamera().getComponent<cro::Transform>().move(move);

    //if (cro::Keyboard::isKeyPressed(SDLK_w))
    //{
    //    Debug::rotate += dt;
    //}
    //if (cro::Keyboard::isKeyPressed(SDLK_s))
    //{
    //    Debug::rotate -= dt;
    //}
    //m_scene.getActiveCamera().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, Debug::rotate);


    m_scene.simulate(dt);
    return false;
}

void ScrubBackgroundState::render()
{
#ifdef HIDE_BACKGROUND
    return;
#endif
    m_renderTexture.clear(cro::Colour::Magenta);
    m_scene.render();
    m_renderTexture.display();

    glUseProgram(m_blurShader.id);
    glUniform2f(m_blurShader.uniform, 1.f / m_renderTexture.getSize().x, 0.f);

    m_blurTexture.clear();
    m_renderQuad.draw(); //first pass blur
    m_blurTexture.display();

    glUseProgram(m_blurShader.id);
    glUniform2f(m_blurShader.uniform, 0.f, 1.f / m_blurTexture.getSize().y);
    m_blurQuad.draw(); //second pass blur
}

//private
void ScrubBackgroundState::addSystems()
{
    auto& mb = cro::App::getInstance().getMessageBus();

    m_scene.addSystem<CloudSystem>(mb);
    m_scene.addSystem<cro::BillboardSystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);
    m_scene.addSystem<SpectatorSystem>(mb, m_collisionMesh);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
}

void ScrubBackgroundState::loadAssets()
{
    if (m_blurShader.shader.loadFromString(cro::SimpleDrawable::getDefaultVertexShader(), GaussianFrag, "#define TEXTURED\n"))
    {
        m_blurShader.id = m_blurShader.shader.getGLHandle();
        m_blurShader.uniform = m_blurShader.shader.getUniformID("u_offset");
    }

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/shrubbery.spt", m_resources.textures);

    m_billboardTemplates[BillboardID::Grass01] = spriteToBillboard(spriteSheet.getSprite("grass01"));
    m_billboardTemplates[BillboardID::Grass02] = spriteToBillboard(spriteSheet.getSprite("grass02"));
    m_billboardTemplates[BillboardID::Flowers01] = spriteToBillboard(spriteSheet.getSprite("flowers01"));
    m_billboardTemplates[BillboardID::Flowers02] = spriteToBillboard(spriteSheet.getSprite("flowers02"));
    m_billboardTemplates[BillboardID::Flowers03] = spriteToBillboard(spriteSheet.getSprite("flowers03"));
    m_billboardTemplates[BillboardID::Tree01] = spriteToBillboard(spriteSheet.getSprite("tree01"));
    m_billboardTemplates[BillboardID::Tree02] = spriteToBillboard(spriteSheet.getSprite("tree02"));
    m_billboardTemplates[BillboardID::Tree03] = spriteToBillboard(spriteSheet.getSprite("tree03"));
    m_billboardTemplates[BillboardID::Tree04] = spriteToBillboard(spriteSheet.getSprite("tree04"));

    loadSpectators();

    m_sharedScrubData.backgroundTexture = &m_blurTexture;
}

void ScrubBackgroundState::createScene()
{
    m_scene.enableSkybox();
    m_scene.setSkyboxColours(cro::Colour(0.858f, 0.686f, 0.467f, 1.f), TextNormalColour, cro::Colour(0.663f, 0.729f, 0.753f, 1.f));

    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/golf/models/menu_ground.cmt"))
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        m_collisionMesh.updateCollisionMesh(entity.getComponent<cro::Model>().getMeshData());
    }

    if (md.loadFromFile("assets/golf/models/menu_pavilion.cmt"))
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
    }

    if (md.loadFromFile("assets/golf/models/phone_box.cmt"))
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 8.2f, 0.f, 13.2f });
        entity.getComponent<cro::Transform>().setScale(glm::vec3(0.7f));
        md.createModel(entity);
    }

    if (md.loadFromFile("assets/golf/models/garden_bench.cmt"))
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 10.2f, 0.f, 13.6f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
        md.createModel(entity);
    }

    if (md.loadFromFile("assets/golf/models/spectators/sitting/02.cmt"))
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 10.2f, 0.f, 13.6f });
        md.createModel(entity);

        entity.getComponent<cro::Skeleton>().play(1);
    }

    if (md.loadFromFile("assets/golf/models/sign_post.cmt"))
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -10.f, 0.f, 12.f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -150.f * cro::Util::Const::degToRad);
        md.createModel(entity);
    }

    if (md.loadFromFile("assets/golf/models/bollard_day.cmt"))
    {
        constexpr std::array positions =
        {
            glm::vec3(7.2f, 0.f, 12.f),
            glm::vec3(7.2f, 0.f, 3.5f),
            //glm::vec3(-10.5f, 0.f, 12.5f),
            glm::vec3(-8.2f, 0.f, 3.5f)
        };

        for (auto pos : positions)
        {
            auto entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(pos);
            md.createModel(entity);
        }
    }

    //trees
    if (md.loadFromFile("assets/golf/models/shrubbery.cmt"))
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        if (entity.hasComponent<cro::BillboardCollection>())
        {
            std::vector<std::array<float, 2u>> bounds =
            {
                { 30.f, 0.f },
                { 80.f, 10.f },

                { 12.f, -23.f },
                { 42.f, -13.f },

                { -60.f, 0.f },
                { -30.f, 40.f },
            };

            auto& collection = entity.getComponent<cro::BillboardCollection>();

            for (auto i = 0u; i < bounds.size(); i += 2)
            {
                const auto& minBounds = bounds[i];
                const auto& maxBounds = bounds[i + 1];
                auto trees = pd::PoissonDiskSampling(2.8f, minBounds, maxBounds);
                for (auto [x, y] : trees)
                {
                    float scale = static_cast<float>(cro::Util::Random::value(12, 22)) / 10.f;

                    auto bb = m_billboardTemplates[cro::Util::Random::value(BillboardID::Tree01, BillboardID::Tree04)];
                    bb.position = { x, 0.f, -y };
                    bb.size *= scale;
                    collection.addBillboard(bb);
                }
            }
        }
    }

    loadClouds();

    auto resize = [&](cro::Camera& cam)
        {
            auto winSize = cro::App::getWindow().getSize();
            m_renderTexture.create(winSize.x, winSize.y);
            m_renderQuad.setTexture(m_renderTexture.getTexture());
            m_renderQuad.setShader(m_blurShader.shader);

            m_blurTexture.create(winSize.x, winSize.y);
            m_blurQuad.setTexture(m_blurTexture.getTexture());
            m_blurQuad.setShader(m_blurShader.shader);

            auto size = glm::vec2(winSize);
            cam.setPerspective(40.f * cro::Util::Const::degToRad, size.x / size.y, 0.01f, 100.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
    resize(cam);
    cam.resizeCallback = resize;

    cam.shadowMapBuffer.create(2048, 2048);

    auto& camTx = m_scene.getActiveCamera().getComponent<cro::Transform>();
    camTx.setPosition({ 0.f, 7.88f, 39.f });
    camTx.setRotation(cro::Transform::X_AXIS, -0.13f);


    auto sunEnt = m_scene.getSunlight();
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -40.56f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -39.f * cro::Util::Const::degToRad);



    //registerWindow([&]()
    //    {
    //        auto pos = m_scene.getActiveCamera().getComponent<cro::Transform>().getPosition();

    //        ImGui::Begin("Window");
    //        ImGui::Text("Pos: %3.2f, %3.2f, %3.2f", pos.x, pos.y, pos.z);
    //        ImGui::Text("Rotation: %3.2f", Debug::rotate);
    //        ImGui::End();        
    //    });
}

void ScrubBackgroundState::loadSpectators()
{
    auto& p1 = m_paths.emplace_back();
    for (auto p : Path01)
    {
        p1.addPoint(p);
    }
    auto& p2 = m_paths.emplace_back();
    for (auto p : Path02)
    {
        p2.addPoint(p);
    }
    auto& p3 = m_paths.emplace_back();
    for (auto p : Path03)
    {
        p3.addPoint(p);
    }

    std::size_t pathIndex = 0;

    cro::ModelDefinition md(m_resources);
    std::array modelPaths =
    {
        "assets/golf/models/spectators/01.cmt",
        "assets/golf/models/spectators/02.cmt",
        "assets/golf/models/spectators/03.cmt",
        "assets/golf/models/spectators/04.cmt"
    };

    for (auto i = 0; i < 2; ++i)
    {
        for (const auto& path : modelPaths)
        {
            if (md.loadFromFile(path))
            {
                for (auto j = 0; j < 3; ++j)
                {
                    auto entity = m_scene.createEntity();
                    entity.addComponent<cro::Transform>();
                    md.createModel(entity);
                    //entity.getComponent<cro::Model>().setHidden(true);
                    //entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));

                    if (md.hasSkeleton())
                    {
                        /*auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedSkinned]);
                        applyMaterialData(md, material);

                        glm::vec4 rect((1.f / 3.f) * j, 0.f, (1.f / 3.f), 1.f);
                        material.setProperty("u_subrect", rect);
                        entity.getComponent<cro::Model>().setMaterial(0, material);*/

                        glm::vec4 rect((1.f / 3.f) * j, 0.f, (1.f / 3.f), 1.f);
                        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_subrect", rect);

                        auto& skel = entity.getComponent<cro::Skeleton>();
                        if (!skel.getAnimations().empty())
                        {
                            auto& spectator = entity.addComponent<Spectator>();
                            for (auto k = 0u; k < skel.getAnimations().size(); ++k)
                            {
                                if (skel.getAnimations()[k].name == "Walk")
                                {
                                    spectator.anims[Spectator::AnimID::Walk] = k;
                                }
                                else if (skel.getAnimations()[k].name == "Idle")
                                {
                                    spectator.anims[Spectator::AnimID::Idle] = k;
                                }
                            }
                            skel.setMaxInterpolationDistance(30.f);
                            skel.play(spectator.anims[Spectator::AnimID::Idle]);
                        }
                    }

                    m_spectatorModels.push_back(entity);
                }
            }
        }
    }

    std::shuffle(m_spectatorModels.begin(), m_spectatorModels.end(), cro::Util::Random::rndEngine);
    
    std::size_t p = 0;
    for (auto entity : m_spectatorModels)
    {
        auto& spectator = entity.getComponent<Spectator>();
        spectator.path = &m_paths[pathIndex];
        spectator.target = p++ % m_paths[pathIndex].getPoints().size();
        spectator.stateTime = 5.f;
        spectator.state = Spectator::State::Pause;
        spectator.direction = spectator.target < (m_paths[pathIndex].getPoints().size() / 2) ? -1 : 1;
        
        const auto prevTarget = (spectator.target + (m_paths[pathIndex].getPoints().size() - 1)) % m_paths[pathIndex].getPoints().size();
        const auto prevTargetPoint = m_paths[pathIndex].getPoint(prevTarget);
        const auto targetPoint = m_paths[pathIndex].getPoint(spectator.target);
        const auto diff = (targetPoint - prevTargetPoint) / (static_cast<float>(p % m_paths[pathIndex].getPoints().size()) * cro::Util::Random::value(0.2f, 0.8f));


        entity.getComponent<cro::Transform>().setPosition(prevTargetPoint + diff);

        pathIndex = (pathIndex + 1) % m_paths.size();
    }

    m_scene.simulate(0.f);
    m_scene.getSystem<SpectatorSystem>()->updateSpectatorGroups();
}

void ScrubBackgroundState::loadClouds()
{
    const std::array Paths =
    {
        std::string("assets/golf/models/skybox/clouds/cloud01.cmt"),
        std::string("assets/golf/models/skybox/clouds/cloud02.cmt"),
        std::string("assets/golf/models/skybox/clouds/cloud03.cmt"),
        std::string("assets/golf/models/skybox/clouds/cloud04.cmt")
    };

    cro::ModelDefinition md(m_resources);
    std::vector<cro::ModelDefinition> definitions;

    for (const auto& path : Paths)
    {
        if (md.loadFromFile(path))
        {
            definitions.push_back(md);
        }
    }

    if (!definitions.empty())
    {
        auto seed = static_cast<std::uint32_t>(std::time(nullptr));
        static constexpr std::array MinBounds = { 0.f, 0.f };
        static constexpr std::array MaxBounds = { 280.f, 280.f };
        auto positions = pd::PoissonDiskSampling(40.f, MinBounds, MaxBounds, 30u, seed);

        auto Offset = 140.f;
        std::size_t modelIndex = 0;

        for (const auto& position : positions)
        {
            float height = static_cast<float>(cro::Util::Random::value(20, 40));
            glm::vec3 cloudPos(position[0] - Offset, height, -position[1] + Offset);


            auto entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(cloudPos);
            entity.addComponent<Cloud>().speedMultiplier = static_cast<float>(cro::Util::Random::value(10, 22)) / 100.f;
            definitions[modelIndex].createModel(entity);
            //entity.getComponent<cro::Model>().setMaterial(0, material);

            float scale = static_cast<float>(cro::Util::Random::value(5, 10));
            entity.getComponent<cro::Transform>().setScale(glm::vec3(scale));

            modelIndex = (modelIndex + 1) % definitions.size();
        }
    }
}