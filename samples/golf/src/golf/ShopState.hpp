/*-----------------------------------------------------------------------

Matt Marchant 2025
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
#include "Inventory.hpp"

#include <crogine/core/State.hpp>
#include <crogine/core/ConsoleClient.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/RenderTexture.hpp>

#include <array>
#include <functional>

struct ThreePatch final
{
    cro::FloatRect left, centre, right;
    cro::FloatRect leftNorm, centreNorm, rightNorm;

    enum ID
    {
        ButtonTop, ButtonItem, BuyButton, ExitButton,

        Count
    };
};

struct SharedStateData;
class ShopState final : public cro::State
#ifdef CRO_DEBUG_
    , public cro::ConsoleClient
#endif
{
public:
    ShopState(cro::StateStack&, cro::State::Context, SharedStateData&);

    cro::StateID getStateID() const override { return StateID::Shop; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;
    void render() override;

private:

    SharedStateData& m_sharedData;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;
    float m_viewScale;

    cro::Entity m_rootNode;// transition animation

    std::array<cro::Sprite, inv::Manufacturers.size() - 1> m_smallLogos = {};
    std::array<cro::Sprite, inv::Manufacturers.size() - 1> m_largeLogos = {};

    std::array<ThreePatch, ThreePatch::Count> m_threePatches = {};
    cro::Texture* m_threePatchTexture;

    struct ItemEntry final
    {
        cro::Entity buttonBackground;
        cro::Entity buttonText;
        cro::Entity priceText;
        cro::Entity badge;

        std::int32_t itemIndex = 0; //index to inv::Items
        bool visible = false; //set to false if cropped or partially cropped
        
        enum
        {
            Top, Bottom, None
        };
        std::int32_t cropping = Bottom; //only valid if not visible / partially cropped, dictates if we're off the top or bottom
    };

    struct CategoryItem final
    {
        cro::Entity scrollNode;
        cro::Entity buttonBackground;
        cro::Entity buttonText;

        std::int32_t selectedItem = 0; //selected item
        std::vector<ItemEntry> items;

        std::function<void()> cropItems;
    };
    std::vector<CategoryItem> m_scrollNodes;
    std::int32_t m_selectedCategory;
    cro::FloatRect m_catergoryCroppingArea;

    struct ButtonID final
    {
        enum
        {
            ScrollUp, ScrollDown, Buy, Exit,
            Count
        };
    };
    std::array<cro::Entity, ButtonID::Count> m_buttonEnts = {};

    void loadAssets();
    void addSystems();
    void buildScene();

    struct StatItems final
    {
        cro::Entity manufacturerIcon;
        cro::Entity manufacturerName;
        cro::Entity itemName;
        cro::Entity balanceText;

        std::vector<std::pair<cro::Entity, cro::Entity>> statBars = {};
    }m_statItems;

    cro::RenderTexture m_itemPreviewTexture;

    void createStatDisplay();
    void updateStatDisplay(std::int32_t uid);

    void setCategory(std::int32_t);
    void updateCatIndices();

    void scroll(bool up);
    void scrollTo(std::int32_t);

    const cro::String m_buyString;
    const cro::String m_sellString;

    struct BuyCounter final
    {
        cro::Entity str0;
        cro::Entity str1;

        static constexpr float MaxTime = 1.f;
        float currentTime = 0.f;
        bool active = false;

        bool [[nodiscard]] update(float dt)
        {
            const float progress = currentTime / MaxTime;
            auto bounds = cro::Text::getLocalBounds(str0);
            bounds.left += (progress * bounds.width);
            str0.getComponent<cro::Drawable2D>().setCroppingArea(bounds);

            bounds = cro::Text::getLocalBounds(str1);
            bounds.width *= progress;
            str1.getComponent<cro::Drawable2D>().setCroppingArea(bounds);

            if (active)
            {
                currentTime = std::min(MaxTime, currentTime + dt);
                if (currentTime == MaxTime)
                {
                    active = false;
                    return true;
                }
            }
            else
            {
                currentTime = 0.f;
            }
            return false;
        }
    }m_buyCounter;

    void purchaseItem();
    void sellItem();

    void quitState();

    void onCachedPush() override;
};