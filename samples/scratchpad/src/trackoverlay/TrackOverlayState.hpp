//Auto-generated header file for Scratchpad Stub 01/07/2024, 11:14:05

#pragma once

#include "../StateIDs.hpp"

#include <crogine/audio/sound_system/Playlist.hpp>
#include <crogine/audio/DynamicAudioStream.hpp>

#include <crogine/core/State.hpp>
#include <crogine/core/String.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/ArrayTexture.hpp>

#include <vector>

class TrackOverlayState final : public cro::State, public cro::GuiClient
{
public:
    TrackOverlayState(cro::StateStack&, cro::State::Context);
    ~TrackOverlayState() { writeSettings(); }

    cro::StateID getStateID() const override { return States::ScratchPad::TrackOverlay; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Playlist m_playlist;
    cro::DynamicAudioStream m_audioStream;
    cro::Entity m_audioEnt;

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    static constexpr std::size_t MaxTracks = 30;
    cro::ArrayTexture<std::uint8_t, MaxTracks> m_textures;
    cro::Shader m_thumbShader;
    cro::Image m_fallbackImage;

    struct DisplayEnts final
    {
        cro::Entity thumb;
        cro::Entity titleText;
        cro::Entity artistText;
    }m_displayEnts;


    struct ShaderHandle final
    {
        std::uint32_t id = 0;
        std::int32_t indexUniform = -1;
        std::int32_t normalUniform = -1;
    }m_shaderHandle;
    bool m_showSettings;

    std::size_t m_currentIndex;
    std::vector<std::pair<cro::String, cro::String>> m_textStrings;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    struct Settings final
    {
        std::string artistFont;
        std::int32_t artistFontSize = 60;
        std::string titleFont;
        std::int32_t titleFontSize = 80;
        std::string albumDirectory;
    }m_settings;

    void loadAlbumDirectory();
    void readSettings();
    void writeSettings() const;

    void nextTrack();
};