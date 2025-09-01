/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2023
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

#pragma once

#include "StateIDs.hpp"
#include "Aer.hpp"
#include <crogine/graphics/VideoPlayer.hpp>
#include <crogine/audio/sound_system/MusicPlayer.hpp>

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/gui/Gui.hpp>

class MyApp;
namespace sp
{
    class MenuState final : public cro::State, public cro::GuiClient
    {
    public:
        MenuState(cro::StateStack&, cro::State::Context, MyApp&);
        ~MenuState() = default;

        cro::StateID getStateID() const override { return States::ScratchPad::MainMenu; }

        bool handleEvent(const cro::Event&) override;
        void handleMessage(const cro::Message&) override;
        bool simulate(float) override;
        void render() override;

    private:
        MyApp& m_gameInstance;

        cro::Scene m_scene;
        cro::Font m_font;

        cro::VideoPlayer m_video;
        cro::MusicPlayer m_music;
        std::string m_musicName;

        ImGui::FileBrowser m_fileBrowser;

        Aer m_aer;

        void addSystems();
        void loadAssets();
        void createScene();
        void createUI();

        bool createStub(const std::string&) const;
        void fileToByteArray(const std::string&, const std::string&) const;
        void CSVToMap();

        cro::Texture m_quantizeInput;
        cro::RenderTexture m_quantizeOutput;
        cro::SimpleQuad m_quantizeQuad;
        cro::Shader m_quantizeShader;
        void imageQuantizer();
    };
}