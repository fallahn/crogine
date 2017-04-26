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
#include <crogine/graphics/MeshResource.hpp>
#include <crogine/graphics/ShaderResource.hpp>
#include <crogine/graphics/MaterialResource.hpp>
#include <crogine/graphics/TextureResource.hpp>
#include <crogine/graphics/FontResource.hpp>

#include "StateIDs.hpp"

namespace cro
{
    class SceneRenderer;
    class SpriteRenderer;
    class TextRenderer;
    class CommandSystem;
}

class MainState final : public cro::State
{
public:
    MainState(cro::StateStack&, cro::State::Context);
    ~MainState() = default;

    cro::StateID getStateID() const override { return States::MainMenu; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(cro::Time) override;
    void render() const override;

private:

    cro::Scene m_backgroundScene;
    cro::Scene m_mainMenuScene;
    cro::Scene m_optionMenuScene; //TODO different scene depending on platform
    cro::Scene m_highScoreScene;
    cro::Scene* m_currentMenu;

    cro::MeshResource m_meshResource;
    cro::ShaderResource m_shaderResource;
    cro::MaterialResource m_materialResource;
    cro::TextureResource m_textureResource;
    cro::FontResource m_fontResource;

    cro::SceneRenderer* m_backgroundRenderer;
    cro::SpriteRenderer* m_spriteRenderer;
    cro::TextRenderer* m_textRenderer;
    cro::CommandSystem* m_commandSystem;

    void addSystems();
    void loadAssets();
    void createScene();
    void createMainMenu();
};

#endif //TL_MAIN_STATE_HPP_