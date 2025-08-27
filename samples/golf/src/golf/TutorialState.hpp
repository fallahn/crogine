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

#include "../StateIDs.hpp"

#include <crogine/audio/AudioScape.hpp>
#include <crogine/core/State.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <array>

namespace cro
{
    struct Camera;
}

struct ButtonID final
{
    enum
    {
        A, B, X, Y, Start, Select, LT, RT,

        Count
    };
};

class TutorialState final : public cro::State, public cro::GuiClient
{
public:
    TutorialState(cro::StateStack&, cro::State::Context, struct SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Tutorial; }

private:

    SharedStateData& m_sharedData;
    cro::Scene m_scene;
    cro::Entity m_backgroundEnt;
    cro::Entity m_messageEnt;
    glm::vec2 m_viewScale;

    cro::AudioScape m_audioScape;

    bool m_mouseVisible;
    cro::Clock m_mouseClock;

    std::vector<std::function<void()>> m_actionCallbacks;
    std::size_t m_currentAction;
    bool m_actionActive;

    std::array<cro::Sprite, ButtonID::Count> m_buttonSprites = {};

    void buildScene();

    void tutorialOne(cro::Entity);
    void tutorialTwo(cro::Entity);
    void tutorialThree(cro::Entity);
    void tutorialPutt(cro::Entity);
    void tutorialSwing(cro::Entity);
    void tutorialSpin(cro::Entity);

    void tutorialLowerClubs(cro::Entity);
    void tutorialPuttAssist(cro::Entity);

    void showContinue();
    void doCurrentAction();

    void playSound(const std::string&);

    void quitState();
};