/*-----------------------------------------------------------------------

Matt Marchant - 2022
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
#include "SharedStateData.hpp"
#include "PacketIDs.hpp"
#include "GameConsts.hpp"
#include "BilliardsSystem.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>

#include <crogine/util/Constants.hpp>

namespace
{
    const cro::Time ReadyPingFreq = cro::seconds(1.f);
}

BilliardsState::BilliardsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_sharedData        (sd),
    m_gameScene         (ctx.appInstance.getMessageBus()),
    m_scaleBuffer       ("PixelScale", sizeof(float)),
    m_resolutionBuffer  ("ScaledResolution", sizeof(glm::vec2)),
    m_wantsGameState    (true)
{
    ctx.mainWindow.loadResources([&]() 
        {
            loadAssets();
            addSystems();
            buildScene();
        });
}

//public
bool BilliardsState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse)
    {
        return false;
    }



    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_ESCAPE:
        case SDLK_p:
        case SDLK_PAUSE:
            requestStackPush(StateID::Pause);
            break;
        case SDLK_SPACE:
            //toggleQuitReady();
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN
        && evt.cbutton.which == cro::GameController::deviceID(m_sharedData.inputBinding.controllerID))
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonA:
            //toggleQuitReady();
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP
        && evt.cbutton.which == cro::GameController::deviceID(m_sharedData.inputBinding.controllerID))
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonStart:
            requestStackPush(StateID::Pause);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        //check if any players are using the controller
        //and reassign any still connected devices
        for (auto i = 0; i < 4; ++i)
        {
            if (evt.cdevice.which == cro::GameController::deviceID(i))
            {
                for (auto& idx : m_sharedData.controllerIDs)
                {
                    idx = std::max(0, i - 1);
                }
                //update the input parser in case this player is active
                //m_sharedData.inputBinding.controllerID = m_sharedData.controllerIDs[m_currentPlayer.player];
                break;
            }
        }
    }

    m_gameScene.forwardEvent(evt);
    return false;
}

void BilliardsState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
}

bool BilliardsState::simulate(float dt)
{
    if (m_sharedData.clientConnection.connected)
    {
        cro::NetEvent evt;
        while (m_sharedData.clientConnection.netClient.pollEvent(evt))
        {
            //handle events
            handleNetEvent(evt);
        }

        if (m_wantsGameState)
        {
            if (m_readyClock.elapsed() > ReadyPingFreq)
            {
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientReady, m_sharedData.clientConnection.connectionID, cro::NetFlag::Reliable);
                m_readyClock.restart();
            }
        }
    }
    else
    {
        //we've been disconnected somewhere - push error state
        m_sharedData.errorMessage = "Lost connection to host.";
        requestStackPush(StateID::Error);
    }

    m_gameScene.simulate(dt);
    return false;
}

void BilliardsState::render()
{
    m_gameScene.render();
}

//private
void BilliardsState::loadAssets()
{

}

void BilliardsState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb);
}

void BilliardsState::buildScene()
{
    //TODO validate the table data (or should this be done by the menu?)

    std::string path = "assets/golf/tables/" + m_sharedData.mapDirectory + ".table";
    TableData tableData;
    if (tableData.loadFromFile(path))
    {
        cro::ModelDefinition md(m_resources);
        if (md.loadFromFile(tableData.viewModel))
        {
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>();
            md.createModel(entity);
        }
    }
    else
    {
        //TODO push error for missing data.
    }

    //update the 3D view
    auto updateView = [&](cro::Camera& cam)
    {
        auto vpSize = calcVPSize();

        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        float maxScale = std::floor(winSize.y / vpSize.y);
        float scale = m_sharedData.pixelScale ? maxScale : 1.f;
        auto texSize = winSize / scale;
        m_gameSceneTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y));

        auto invScale = (maxScale + 1.f) - scale;
        /*glCheck(glPointSize(invScale * BallPointSize));
        glCheck(glLineWidth(invScale));*/

        m_scaleBuffer.setData(&invScale);

        glm::vec2 scaledRes = texSize / invScale;
        m_resolutionBuffer.setData(&scaledRes);

        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, texSize.x / texSize.y, 0.1f, 5.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({-2.f, 1.2f, 0.f});
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -30.f * cro::Util::Const::degToRad);
    //m_cameras[CameraID::Player] = camEnt;
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);

    static constexpr std::uint32_t ShadowMapSize = 2048u;
    cam.shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);

    //TODO overhead cam etc


    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
}

void BilliardsState::handleNetEvent(const cro::NetEvent& evt)
{
    if (evt.type == cro::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::SetPlayer:
            m_wantsGameState = false;

            //TODO this wants to be done after any animation
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::TransitionComplete,
                m_sharedData.clientConnection.connectionID, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

            break;
        case PacketID::ClientDisconnected:
            //TODO we should only have 2 clients max
            //so if only one player locally then game is forfeited.
            break;
        case PacketID::AchievementGet:
            LogI << "TODO: Notify Achievement" << std::endl;
            break;
        case PacketID::ServerError:
            switch (evt.packet.as<std::uint8_t>())
            {
            default:
                m_sharedData.errorMessage = "Server Error (Unknown)";
                break;
            case MessageType::MapNotFound:
                m_sharedData.errorMessage = "Server Failed To Load Table Data";
                break;
            }
            requestStackPush(StateID::Error);
            break;
        }
    }
    else if (evt.type == cro::NetEvent::ClientDisconnect)
    {
        m_sharedData.errorMessage = "Disconnected From Server (Host Quit)";
        requestStackPush(StateID::Error);
    }
}