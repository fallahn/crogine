//Auto-generated header file for Scratchpad Stub 03/08/2023, 10:55:22

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/audio/sound_system/MusicPlayer.hpp>

class GCState final : public cro::State, public cro::GuiClient
{
public:
    GCState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::GC; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    cro::EnvironmentMap m_environmentMap;
    cro::Entity m_fadeEnt;
    cro::MusicPlayer m_music;

    bool m_processReturn;

    struct CameraID final
    {
        enum
        {
            Rotate, Pass, Static,

            Count
        };
    };
    std::array<cro::Entity, CameraID::Count> m_cameras = {};
    std::size_t m_cameraIndex;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    void setNextCam();
    void quitState();
};