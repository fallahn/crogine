/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "../golf/SharedStateData.hpp"
#include "../scrub/ScrubSharedData.hpp"
#include "SBallBackgroundState.hpp"
#include "SBallConsts.hpp"

#include <Social.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>

SBallBackgroundState::SBallBackgroundState(cro::StateStack& stack, cro::State::Context ctx, const SharedStateData& ssd, SharedMinigameData& sd)
    : cro::State        (stack, ctx),
    m_sharedData        (ssd),
    m_sharedGameData    (sd),
    m_scene             (cro::App::getInstance().getMessageBus())
{
    ctx.mainWindow.loadResources([&]()
        {
            sd.initFonts();

            buildScene();

            cacheState(StateID::SBallGame);
            cacheState(StateID::SBallAttract);
            cacheState(StateID::ScrubPause);
        });
#ifdef CRO_DEBUG_
    //requestStackPush(StateID::SBallGame);
    requestStackPush(StateID::SBallAttract);
#else
    requestStackPush(StateID::SBallAttract);
#endif

    cro::String str("Playing Sports Ball ");
    str += cro::String(std::uint32_t(0x26BD));

    Social::refreshSBallScore();
    Social::setStatus(Social::InfoID::Menu, { reinterpret_cast<const char*>(str.toUtf8().c_str()) });
}

SBallBackgroundState::~SBallBackgroundState()
{
    m_sharedGameData.backgroundTexture = nullptr;

    //reset any controllers here in case we're just quitting the game
    for (auto i = 0; i < 4; ++i)
    {
        cro::GameController::applyDSTriggerEffect(i, cro::GameController::DSTriggerBoth, {});
    }
}

//public
bool SBallBackgroundState::handleEvent(const cro::Event& evt)
{
    m_scene.forwardEvent(evt);
    return false;
}

void SBallBackgroundState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped
            && data.id == StateID::SBallGame)
        {
            requestStackPush(StateID::SBallAttract);
        }
    }

    m_scene.forwardMessage(msg);
}

bool SBallBackgroundState::simulate(float dt)
{
    m_scene.simulate(dt);
    return false;
}

void SBallBackgroundState::render()
{
    m_scene.render();
}

//private
void SBallBackgroundState::buildScene()
{
    auto& mb = cro::App::getInstance().getMessageBus();

    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);


    m_environmentMap.loadFromFile("assets/images/indoor.hdr");
    cro::ModelDefinition md(m_resources, &m_environmentMap);

    md.loadFromFile("assets/arcade/sportsball/models/background.cmt");

    cro::Entity entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);



    auto resize = [&](cro::Camera& cam)
        {
            //update shadow quality from settings
            std::uint32_t res = 512;
            switch (m_sharedData.shadowQuality)
            {
            case 0:
                res = 512;
                break;
            case 1:
                res = 1024;
                break;
            default:
                res = 2048;
                break;
            }
            cam.shadowMapBuffer.create(res, res);
            cam.setBlurPassCount(m_sharedData.shadowQuality == 0 ? 0 : 1);

            const glm::vec2 size(cro::App::getWindow().getSize());
            const float ratio = size.x / size.y;
            const float y = WorldHeight;
            const float x = y * ratio;

            cam.setOrthographic(-x / 2.f, x / 2.f, 0.f, y, 0.1f, 4.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };

    auto camEnt = m_scene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, -0.05f, 1.4f });

    auto& cam = camEnt.getComponent<cro::Camera>();
    resize(cam);
    cam.resizeCallback = resize;
    cam.setMaxShadowDistance(10.f);
    cam.setShadowExpansion(15.f);


    auto lightEnt = m_scene.getSunlight();
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -45.f * cro::Util::Const::degToRad);
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -1.f * cro::Util::Const::degToRad);
}