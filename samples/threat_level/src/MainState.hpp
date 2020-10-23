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

#ifndef TL_MAIN_STATE_HPP_
#define TL_MAIN_STATE_HPP_

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/gui/GuiClient.hpp>

#include "StateIDs.hpp"
#include "ResourceIDs.hpp"

namespace GroupID
{
    enum
    {
        Main,
        Options,
        MapSelect,
        Scores,
        QuitConfirm
    };
}

namespace cro
{
    class CommandSystem;
    class UISystem;
    class SpriteSheet;
}

struct SharedResources;
using ResourcePtr = std::unique_ptr<SharedResources>;

class MainState final : public cro::State, public cro::GuiClient
{
public:
    MainState(cro::StateStack&, cro::State::Context, ResourcePtr&);
    ~MainState() = default;

    cro::StateID getStateID() const override { return States::MainMenu; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    cro::ResourceCollection m_resources; //destruction order important!
    cro::Scene m_backgroundScene;
    cro::Scene m_menuScene;
    SharedResources& m_sharedResources;

    std::array<cro::ModelDefinition, MenuModelID::Count> m_modelDefs;

    cro::CommandSystem* m_commandSystem;
    cro::UISystem* m_uiSystem;

    void addSystems();
    void loadAssets();
    void createScene();
    void createMainMenu(cro::uint32, cro::uint32, const cro::SpriteSheet&, const cro::SpriteSheet&);
    void createMapSelect(cro::Entity, const cro::SpriteSheet&, const cro::SpriteSheet&);
    void createOptionsMenu(cro::uint32, cro::uint32, const cro::SpriteSheet&, const cro::SpriteSheet&);
    void createScoreMenu(cro::uint32, cro::uint32, const cro::SpriteSheet&, const cro::SpriteSheet&);
    void createMenus();
    void updateView();
};
#endif //TL_MAIN_STATE_HPP_