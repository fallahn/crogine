/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/TextureResource.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <array>
#include <tuple>

struct SharedStateData;
namespace cro
{
    class Sprite;
}

class SpriteState final : public cro::State, public cro::GuiClient
{
public:
    SpriteState(cro::StateStack&, cro::State::Context, SharedStateData&);

    cro::StateID getStateID() const override { return States::SpriteEditor; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    SharedStateData& m_sharedData;
    cro::Scene m_scene;

    cro::TextureResource m_textures;
    cro::SpriteSheet m_spriteSheet;
    std::string m_currentFilePath;

    struct EntityID final
    {
        enum
        {
            Root,
            Border,
            Bounds, //frames the sprite bounds on the sheet

            Count
        };
    };
    std::array<cro::Entity, EntityID::Count> m_entities;

    using SpritePair = std::pair<const std::string, cro::Sprite>;
    SpritePair* m_activeSprite;

    bool m_showPreferences;

    void initScene();

    void openSprite(const std::string&);
    void updateBoundsEntity();

    void createUI();
    void drawMenuBar();
    void drawInspector();
    void drawSpriteWindow();
    void drawPreferences();
};