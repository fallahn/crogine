/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#pragma once

#include "StateIDs.hpp"

#include <crogine/audio/AudioBuffer.hpp>
#include <crogine/audio/AudioResource.hpp>

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/VideoPlayer.hpp>
#include <crogine/graphics/SimpleQuad.hpp>

namespace cro
{
    struct Camera;
    class AudioResource;
}

struct SharedStateData;
class SplashState final : public cro::State
{
public:
    SplashState(cro::StateStack&, cro::State::Context, SharedStateData&);

    cro::StateID getStateID() const override { return StateID::SplashScreen; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    SharedStateData& m_sharedData;
    cro::AudioResource m_audioResource;

    cro::Scene m_uiScene;
    float m_timer;

    float m_windowRatio;
    std::int32_t m_timeUniform;
    std::int32_t m_scanlineUniform;

    cro::Texture m_texture;
    cro::Shader m_noiseShader;
    cro::Shader m_scanlineShader;

    cro::VideoPlayer m_video;

    void addSystems();
    void loadAssets();
    void createUI();
    void gotoMenu();

    //assigned to camera resize callback
    void updateView(cro::Camera&);
};