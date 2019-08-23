/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#ifndef TL_GAME_STATE_HPP_
#define TL_GAME_STATE_HPP_

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ResourceAutomation.hpp>

#include "StateIDs.hpp"
#include "ResourceIDs.hpp"

namespace cro
{
    class UISystem;
}

class GameState final : public cro::State
{
public:
    GameState(cro::StateStack&, cro::State::Context);
    ~GameState() = default;

    cro::StateID getStateID() const override { return States::GamePlaying; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(cro::Time) override;
    void render() override;

private:
    cro::ResourceCollection m_resources; //how many times??? ORDER IS IMPORTANT

    cro::Scene m_scene;
    cro::Scene m_uiScene;


    std::array<cro::ModelDefinition, GameModelID::Count> m_modelDefs;

    cro::UISystem* m_uiSystem;

    void addSystems();
    void loadAssets();
    void createScene();
    void createHUD();

    void loadTerrain();
    void loadModels();
    void loadParticles();
    void loadWeapons();

    void updateView();
};
#endif //TL_GAME_STATE_HPP_