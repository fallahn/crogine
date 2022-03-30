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

#include "BilliardsState.hpp"
#include "BilliardsSystem.hpp"
#include "InterpolationSystem.hpp"

#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>

namespace
{
    bool showDebug = false;

    cro::Entity interpEnt;

    std::array interpTargets =
    {
        glm::vec3(0.5f, 0.5f, 0.f),
        glm::vec3(0.5f, 0.5f, -0.5f),
        glm::vec3(0.f, 0.5f, -0.5f),
        glm::vec3(-0.5f, 0.5f, -0.5f),
        glm::vec3(-0.5f, 0.5f, 0.f),
        glm::vec3(-0.5f, 0.5f, 0.5f),
        glm::vec3(0.f, 0.5f, 0.5f),
        glm::vec3(0.5f, 0.5f, 0.5f)
    };
    std::size_t interpIndex = 0;
}

BilliardsState::BilliardsState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State    (ss, ctx),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_ballDef       (m_resources)
{
    addSystems();
    buildScene();
}

//public
bool BilliardsState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_i:
            m_scene.getSystem<BilliardsSystem>()->applyImpulse();
            break;
        case SDLK_o:
            addBall();
            break;
        case SDLK_p:
            showDebug = !showDebug;
            m_debugDrawer.setDebugMode(showDebug ? /*std::numeric_limits<std::int32_t>::max()*/3 : 0);
            break;
        case SDLK_BACKSPACE:
            requestStackPop();
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        case SDLK_KP_0:
            m_scene.setActiveCamera(m_cameras[0]);
            break;
        case SDLK_KP_1:
            m_scene.setActiveCamera(m_cameras[1]);
            break;
        case SDLK_KP_2:
            m_scene.setActiveCamera(m_cameras[2]);
            break;
        case SDLK_KP_3:
            m_scene.setActiveCamera(m_cameras[3]);
            break;
        case SDLK_KP_4:
            m_scene.setActiveCamera(m_cameras[4]);
            break;
        case SDLK_SPACE:
            interpEnt.getComponent<cro::Callback>().active = !interpEnt.getComponent<cro::Callback>().active;
            break;
        }
    }

    m_scene.forwardEvent(evt);
    return false;
}

void BilliardsState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool BilliardsState::simulate(float dt)
{
    m_scene.simulate(dt);
    return false;
}

void BilliardsState::render()
{
    m_scene.render();

    auto viewProj = m_scene.getActiveCamera().getComponent<cro::Camera>().getActivePass().viewProjectionMatrix;
    m_debugDrawer.render(viewProj);
}

//private
void BilliardsState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<InterpolationSystem>(mb);
    m_scene.addSystem<BilliardsSystem>(mb, m_debugDrawer);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
}

void BilliardsState::buildScene()
{
    TableData tableData;

    cro::ConfigFile tableConfig;
    if (tableConfig.loadFromFile("assets/billiards/billiards.table"))
    {
        const auto& props = tableConfig.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "model")
            {
                tableData.viewModel = p.getValue<std::string>();
            }
            else if (name == "collision")
            {
                tableData.collisionModel = p.getValue<std::string>();
            }
        }

        const auto& objs = tableConfig.getObjects();
        for (const auto& obj : objs)
        {
            const auto& name = obj.getName();
            if (name == "pocket")
            {
                auto& pocket = tableData.pockets.emplace_back();

                const auto& pocketProps = obj.getProperties();
                for (const auto& prop : pocketProps)
                {
                    const auto& propName = prop.getName();
                    if (propName == "position")
                    {
                        pocket.position = prop.getValue<glm::vec2>();
                    }
                    else if (propName == "value")
                    {
                        pocket.value = prop.getValue<std::int32_t>();
                    }
                    else if (propName == "radius")
                    {
                        pocket.radius = prop.getValue<float>();
                    }
                }
            }
        }
    }


    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile(tableData.collisionModel))
    {
        //table
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        m_scene.getSystem<BilliardsSystem>()->initTable(entity.getComponent<cro::Model>().getMeshData(), tableData.pockets);

        //TODO extract the mesh data in a more sensible way such as reading it straight from the file...
        m_scene.destroyEntity(entity);
    }

    if (md.loadFromFile(tableData.viewModel))
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
    }

    //ball
    if (m_ballDef.loadFromFile("assets/billiards/ball.cmt"))
    {
        addBall();
    }


    //camera / sunlight
    auto callback = [](cro::Camera& cam)
    {
        glm::vec2 winSize(cro::App::getWindow().getSize());
        cam.setPerspective(60.f * cro::Util::Const::degToRad, winSize.x / winSize.y, 0.1f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    auto& camera = m_scene.getActiveCamera().getComponent<cro::Camera>();
    camera.resizeCallback = callback;
    //camera.shadowMapBuffer.create(2048, 2048);
    callback(camera);

    m_scene.getActiveCamera().getComponent<cro::Transform>().move({ 0.f, 2.f, 0.f });
    m_scene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    m_scene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, -90.f * cro::Util::Const::degToRad);
    m_cameras[0] = m_scene.getActiveCamera();


    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -1.2f, 0.8f, 1.4f });
    entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -45.f * cro::Util::Const::degToRad);
    entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -25.f * cro::Util::Const::degToRad);
    callback(entity.addComponent<cro::Camera>());
    entity.getComponent<cro::Camera>().resizeCallback = callback;
    m_cameras[1] = entity;

    auto orthoCallback = [](cro::Camera& cam)
    {
        glm::vec2 winSize(cro::App::getWindow().getSize());
        float ratio = winSize.x / winSize.y;

        winSize.y = 1.f;
        winSize.x = winSize.y * ratio;

        cam.setOrthographic(-winSize.x, winSize.x, -winSize.y, winSize.y, 0.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    for (auto i = 2u; i < m_cameras.size(); ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        orthoCallback(entity.addComponent < cro::Camera>());
        entity.getComponent<cro::Camera>().resizeCallback = orthoCallback;
        m_cameras[i] = entity;
    }

    m_cameras[2].getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 2.4f });

    m_cameras[3].getComponent<cro::Transform>().setPosition({ -1.4f, 0.f, 0.f });
    m_cameras[3].getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);

    m_cameras[4].getComponent<cro::Transform>().setPosition({ -0.f, 3.f, 0.f });
    m_cameras[4].getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
    m_cameras[4].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);

    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -25.f * cro::Util::Const::degToRad);
    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -25.f * cro::Util::Const::degToRad);





    //test interp component by sending updates with fixed space timestamps
    //but applying them with +- 0.2 sec jitter. 1 in 60 updates are also dropped.
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<InterpolationComponent>(InterpolationPoint(glm::vec3(0.f), glm::quat(1.f, 0.f ,0.f, 0.f), m_interpClock.elapsed().asMilliseconds()));
    interpEnt = entity;

    md.loadFromFile("assets/billiards/interp.cmt");
    md.createModel(entity);

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        static constexpr float TimeStep = 1.f;
        static float nextTimeStep = TimeStep;

        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime += dt;

        static std::int32_t ts = 1000;

        if (currTime > nextTimeStep)
        {
            currTime -= nextTimeStep;
            nextTimeStep = TimeStep + cro::Util::Random::value(-0.2f, 0.2f);

            if (cro::Util::Random::value(0, 60) != 0)
            {
                interpEnt.getComponent<InterpolationComponent>().addPoint({ interpTargets[interpIndex], glm::quat(1.f, 0.f, 0.f, 0.f), ts });
            }
            else
            {
                LogI << "skipped" << std::endl;
            }
            interpIndex = (interpIndex + 1) % interpTargets.size();

            ts += 1000;
        }
    };
}

void BilliardsState::addBall()
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cro::Util::Random::value(-0.1f, 0.1f), 0.5f, cro::Util::Random::value(-0.1f, 0.1f) });
    m_ballDef.createModel(entity);
    entity.addComponent<BilliardBall>().id = cro::Util::Random::value(0, 15);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (e.getComponent<cro::Transform>().getPosition().y < -1.f)
        {
            m_scene.destroyEntity(e);

            addBall();
        }
    };
}