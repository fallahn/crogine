/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
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
#include "ResourceIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/gui/Gui.hpp>

namespace cro
{
    struct Camera;
}

struct SharedStateData;
class LayoutState final : public cro::State, public cro::GuiClient
{
public:
    LayoutState(cro::StateStack&, cro::State::Context, SharedStateData&);
    ~LayoutState() = default;

    cro::StateID getStateID() const override { return States::LayoutEditor; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_modelScene;
    cro::Scene m_uiScene;
    cro::Scene m_thumbScene;
    SharedStateData& m_sharedData;

    cro::ResourceCollection m_resources;
    cro::Texture m_backgroundTexture;

    void addSystems();
    void loadAssets();
    void createScene();

    //assigned to camera resize callback
    glm::vec2 m_layoutSize;
    cro::Entity m_backgroundEntity;
    void updateView2D(cro::Camera&);
    void updateView3D(cro::Camera&);

    ImVec4 m_messageColour;

    void initUI();
    void drawMenuBar();
    void drawInspector();
    void drawBrowser();
    void drawInfo();

    void updateLayout(std::int32_t, std::int32_t);

    struct TreeNode final
    {
        std::string name;
        cro::Entity entity;
        enum
        {
            Sprite, Text
        }type = Sprite;
        std::vector<TreeNode> children;
    };
    std::vector<TreeNode> m_treeNodes;


    struct SpriteThumb final
    {
        std::string spriteSheet;
        std::string spriteName;
        cro::Sprite sprite;
    };

    std::uint32_t m_selectedSprite;
    std::vector<SpriteThumb> m_spriteThumbs;
    void loadSpriteSheet(const std::string&);

    //TODO could we improve this by rendering all thumbs to a large
    //texture and only previewing part of it?
    std::uint32_t m_nextResourceID;

    struct FontThumb final
    {
        cro::RenderTexture texture;
        std::string relPath;
        std::string name;
    };
    std::uint32_t m_selectedFont;
    std::unordered_map<std::uint32_t, FontThumb> m_fontThumbs;
    void loadFont(const std::string&);
};