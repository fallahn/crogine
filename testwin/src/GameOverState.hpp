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

#ifndef TL_GAMEOVER_STATE_HPP_
#define TL_GAMEOVER_STATE_HPP_

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ResourceAutomation.hpp>
#include <crogine/graphics/Texture.hpp>

#include "StateIDs.hpp"
#include "ResourceIDs.hpp"

namespace cro
{
    class UISystem;
}

struct SharedResources;
using ResourcePtr = std::unique_ptr<SharedResources>;

class GameOverState final : public cro::State
{
public:
    GameOverState(cro::StateStack&, cro::State::Context, ResourcePtr&);
    ~GameOverState() = default;

    cro::StateID getStateID() const override { return States::GameOver; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(cro::Time) override;
    void render() override;

private:

    cro::Scene m_uiScene;
    SharedResources& m_sharedResources;
    cro::ResourceCollection m_resources;
    cro::UISystem* m_uiSystem;
    cro::Texture m_backgroundTexture;

    void load();
    void updateView();
};

#endif //TL_GAMEOVER_STATE_HPP_