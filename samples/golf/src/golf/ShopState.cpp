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

#include "../Colordome-32.hpp"

#include "ShopState.hpp"
#include "SharedStateData.hpp"
#include "MenuConsts.hpp"
#include "Clubs.hpp"
#include "Social.hpp"
#include "Timeline.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/UIElement.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/detail/OpenGL.hpp>

namespace
{
#include "ShopEnum.inl"

    //from edge of window, scaled with getViewScale()
    constexpr float BorderPadding = 4.f;

    constexpr float PositionDepth = 0.f;
    constexpr float SpriteDepth = -2.f;
    constexpr float TextDepth = 2.f;

    constexpr glm::vec2 TopButtonSize = glm::vec2(68.f, 12.f);
    constexpr glm::vec2 TitleSize = glm::vec2(160.f, 50.f);
    constexpr glm::vec2 ItemButtonSize = glm::vec2(200.f, 32.f);
    constexpr glm::vec2 BuyButtonSize = glm::vec2(200.f, TitleSize.y - (TopButtonSize.y + (BorderPadding * 2.f)));

    constexpr float DividerOffset = 0.4f;
    constexpr float StatBarHeight = 24.f;

    constexpr float LargeIconSize = 32.f;
    constexpr float SmallIconSize = 24.f;

    struct Category final
    {
        enum
        {
            Driver, Wood, Iron, Wedge, Ball,
            Count
        };
    };

    constexpr cro::Colour ButtonBackgroundColour = CD32::Colours[CD32::Brown];
    constexpr cro::Colour ButtonSelectedColour = CD32::Colours[CD32::Yellow];
    constexpr cro::Colour ButtonHighlightColour = CD32::Colours[CD32::BlueDark];
    constexpr cro::Colour ButtonTextColour = TextNormalColour;
    constexpr cro::Colour ButtonTextSelectedColour = CD32::Colours[CD32::Black];
    constexpr cro::Colour ButtonPriceColour = CD32::Colours[CD32::Yellow];

    constexpr cro::Colour StatBarColourFront = CD32::Colours[CD32::GreenMid];
    constexpr cro::Colour StatBarColourBack = CD32::Colours[CD32::TanDark];
    constexpr cro::Colour StatTextColour = CD32::Colours[CD32::BeigeLight];

    std::int32_t discountPrice(std::int32_t price)
    {
        return static_cast<std::int32_t>(static_cast<float>(price) * 0.55f);
    }

    const std::string BuyStr = u8"Buy ↓";
    const std::string SellStr = u8"Sell ↓";

    struct TextFlashData final
    {
        std::int32_t count = 8;
        float currTime = 0.f;

        cro::Colour c1 = TextNormalColour;
        cro::Colour c2 = TextHighlightColour;

        void operator()(cro::Entity e, float dt)
        {
            constexpr float FlashTime = 0.125f;

            currTime += dt;

            if (currTime > FlashTime)
            {
                currTime -= FlashTime;
                count--;

                if (count == 0)
                {
                    count = 8;
                    currTime = 0.f;
                    e.getComponent<cro::Callback>().active = false;

                    e.getComponent<cro::Text>().setFillColour(c1);
                }
                else
                {
                    const auto c = count % 2 == 0 ? c1 : c2;
                    e.getComponent<cro::Text>().setFillColour(c);
                }
            }
        }
    };

    struct StatBarData final
    {
        float currentSize = 40.f;
        float value = 0.f; //+/- 10
    };

    void applyButtonBackgroundColour(const cro::Colour c, cro::Entity e)
    {
        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour = c;
        }
    }

    struct ButtonTexID final
    {
        enum
        {
            Unselected, Highlighted, Selected, Owned
        };
    };

    void applyButtonTexture(std::int32_t index, cro::Entity e, const ThreePatch& patch)
    {
        const float offset = patch.leftNorm.height * index;

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        verts[0].UV.x = patch.leftNorm.left;
        verts[0].UV.y = patch.leftNorm.bottom + patch.leftNorm.height + offset;

        verts[1].UV.x = patch.leftNorm.left;
        verts[1].UV.y = patch.leftNorm.bottom + offset;

        verts[2].UV.x = patch.centreNorm.left;
        verts[2].UV.y = patch.centreNorm.bottom + patch.centreNorm.height + offset;

        verts[3].UV.x = patch.centreNorm.left;
        verts[3].UV.y = patch.centreNorm.bottom + offset;

        verts[4].UV.x = patch.rightNorm.left;
        verts[4].UV.y = patch.rightNorm.bottom + patch.rightNorm.height + offset;

        verts[5].UV.x = patch.rightNorm.left;
        verts[5].UV.y = patch.rightNorm.bottom + offset;

        verts[6].UV.x = patch.rightNorm.left + patch.rightNorm.width;
        verts[6].UV.y = patch.rightNorm.bottom + patch.rightNorm.height + offset;

        verts[7].UV.x = patch.rightNorm.left + patch.rightNorm.width;
        verts[7].UV.y = patch.rightNorm.bottom + offset;
    }

    struct ScrollData final
    {
        static constexpr float Stride = ItemButtonSize.y + BorderPadding;

        std::int32_t targetIndex = 0;
        std::int32_t itemCount = 0;
    };

    //hmm this is technically the same as Category - but maybe they'll diverge at some point?
    struct MenuID final
    {
        enum
        {
            Driver, Wood, Iron, Wedge, Balls,

            Dummy,
            Count
        };
    };
}

ShopState::ShopState(cro::StateStack& stack, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (stack, ctx),
    m_sharedData        (sd),
    m_uiScene           (ctx.appInstance.getMessageBus(), 512),
    m_viewScale         (1.f),
    m_previewScene      (ctx.appInstance.getMessageBus()),
    m_threePatchTexture (nullptr),
    m_selectedCategory  (Category::Driver),
    m_buyString         (cro::String::fromUtf8(BuyStr.begin(), BuyStr.end())),
    m_sellString        (cro::String::fromUtf8(SellStr.begin(), SellStr.end()))
{

#ifdef CRO_DEBUG_
    registerCommand("add_balance", 
        [&](const std::string val)
        {
            try
            {
                const auto v = std::stoi(val);
                m_sharedData.inventory.balance = std::clamp(m_sharedData.inventory.balance + v, 0, 10000);
            }
            catch (...)
            {
                cro::Console::print("Usage: add_balance <value>");
            }
        });
#endif

        loadAssets();
        addSystems();
        buildScene();
        buildPreviewScene();

        m_viewScale = cro::UIElementSystem::getViewScale();
}

//public
bool ShopState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureMouse)
    {
        return false;
    }

    const auto nextCat = 
        [&]()
        {
            const auto idx = (m_selectedCategory + 1) % Category::Count;
            setCategory(idx);
        };
    const auto prevCat = 
        [&]()
        {
            const auto idx = (m_selectedCategory + (Category::Count - 1)) % Category::Count;
            setCategory(idx);
        };

    switch (evt.type)
    {
    default: break;
    case SDL_MOUSEBUTTONUP:
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
            return false;
        }
        break;
    case SDL_MOUSEWHEEL:
        if (evt.wheel.y > 0)
        {
            scroll(true);
        }
        else if (evt.wheel.y < 0)
        {
            scroll(false);
        }
        break;
    case SDL_KEYUP:
        if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
        {
            prevCat();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
        {
            nextCat();
        }
        else if (evt.key.keysym.sym == SDLK_ESCAPE)
        {
            quitState();
            return false;
        }
        break;
    case SDL_CONTROLLERBUTTONUP:
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonLeftShoulder:
            prevCat();
            break;
        case cro::GameController::ButtonRightShoulder:
            nextCat();
            break;
        case cro::GameController::ButtonB:
            quitState();
            return false;
        }
        break;
    }

    m_uiScene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_uiScene.forwardEvent(evt);

    return false;
}

void ShopState::handleMessage(const cro::Message& msg)
{
    m_uiScene.forwardMessage(msg);
    m_previewScene.forwardMessage(msg); //do this second so we know the render target is already resized

    //we need to update cropping, doing so here is the only
    //way to ensure it happens after all UI elements were updated
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            auto node = m_scrollNodes[m_selectedCategory].scrollNode;
            node.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex = 0;
            node.getComponent<cro::Callback>().active = true;

            m_viewScale = cro::UIElementSystem::getViewScale();
        }
    }
}

bool ShopState::simulate(float dt)
{
    if (m_buyCounter.update(dt))
    {
        m_audioEnts[AudioID::Purchase].getComponent<cro::AudioEmitter>().play();

        const auto& item = m_scrollNodes[m_selectedCategory].items[m_scrollNodes[m_selectedCategory].selectedItem];
        //if (m_sharedData.inventory.inventory[item.itemIndex] != -1)
        if (item.owned)
        {
            //sell item
            sellItem();
        }
        else
        {
            //current item was purchased
            purchaseItem();
        }
    }

    m_previewScene.simulate(dt);
    m_uiScene.simulate(dt);

    return true;
}

void ShopState::render()
{
    m_itemPreviewTexture.clear(CD32::Colours[CD32::TanDarkest]);
    m_previewScene.render();
    m_itemPreviewTexture.display();

    m_uiScene.render();
}

//private
void ShopState::loadAssets()
{
    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);
    m_audioEnts[AudioID::Accept] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");
    m_audioEnts[AudioID::Select] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Select].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    m_audioEnts[AudioID::Nope] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Nope].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("nope");
    m_audioEnts[AudioID::Purchase] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Purchase].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("purchase");


    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/shop_badges.spt", m_resources.textures);

    m_smallLogos[0] = spriteSheet.getSprite("small_01");
    m_smallLogos[1] = spriteSheet.getSprite("small_02");
    m_smallLogos[2] = spriteSheet.getSprite("small_03");
    m_smallLogos[3] = spriteSheet.getSprite("small_04");
    m_smallLogos[4] = spriteSheet.getSprite("small_05");
    m_smallLogos[5] = spriteSheet.getSprite("small_06");
    m_smallLogos[6] = spriteSheet.getSprite("small_07");
    m_smallLogos[7] = spriteSheet.getSprite("small_08");
    m_smallLogos[8] = spriteSheet.getSprite("small_09");
    m_smallLogos[9] = spriteSheet.getSprite("small_10");
    m_smallLogos[10] = spriteSheet.getSprite("small_11");
    m_smallLogos[11] = spriteSheet.getSprite("small_12");

    m_largeLogos[0] = spriteSheet.getSprite("large_01");
    m_largeLogos[1] = spriteSheet.getSprite("large_02");
    m_largeLogos[2] = spriteSheet.getSprite("large_03");
    m_largeLogos[3] = spriteSheet.getSprite("large_04");
    m_largeLogos[4] = spriteSheet.getSprite("large_05");
    m_largeLogos[5] = spriteSheet.getSprite("large_06");
    m_largeLogos[6] = spriteSheet.getSprite("large_07");
    m_largeLogos[7] = spriteSheet.getSprite("large_08");
    m_largeLogos[8] = spriteSheet.getSprite("large_09");
    m_largeLogos[9] = spriteSheet.getSprite("large_10");
    m_largeLogos[10] = spriteSheet.getSprite("large_11");
    m_largeLogos[11] = spriteSheet.getSprite("large_12");


    m_envMap.loadFromFile("assets/images/hills.hdr");


    //load up the three-patch data for the button textures
    //TODO if we use this excessively then create a cfg format

    m_threePatchTexture = &m_resources.textures.get("assets/golf/images/shop_buttons.png");
    m_threePatchTexture->setRepeated(true);
    const auto texSize = glm::vec2(m_threePatchTexture->getSize());
    
    //if for some reason we failed to load don't try to div0
    if (texSize.x * texSize.y != 0.f)
    {
        //if we were smort we could make all of this constexpr
        const auto normaliseRect =
            [&](cro::FloatRect r)
            {
                cro::FloatRect s;
                s.left = r.left / texSize.x;
                s.bottom = r.bottom / texSize.y;
                s.width = r.width / texSize.x;
                s.height = r.height / texSize.y;

                return s;
            };

        auto& topPatch = m_threePatches[ThreePatch::ButtonTop];
        topPatch.left = { 2.f, 0.f, 6.f, 12.f };
        topPatch.centre = { 8.f, 0.f, 4.f, 12.f };
        topPatch.right = { 12.f, 0.f, 7.f, 12.f }; //hmm suspicious that this has to be set 1px wider to not crop...

        topPatch.leftNorm = normaliseRect(topPatch.left);
        topPatch.centreNorm = normaliseRect(topPatch.centre);
        topPatch.rightNorm = normaliseRect(topPatch.right);


        auto& itemPatch = m_threePatches[ThreePatch::ButtonItem];
        itemPatch.left = { 28.f, 0.f, 4.f, 32.f };
        itemPatch.centre = { 32.f, 0.f, 28.f, 32.f };
        itemPatch.right = { 60.f, 0.f, 4.f, 32.f };

        itemPatch.leftNorm = normaliseRect(itemPatch.left);
        itemPatch.centreNorm = normaliseRect(itemPatch.centre);
        itemPatch.rightNorm = normaliseRect(itemPatch.right);

        auto& buyPatch = m_threePatches[ThreePatch::BuyButton];
        buyPatch.left = { 66.f, 0.f, 14.f, 30.f };
        buyPatch.centre = { 80., 0.f, 16.f, 30.f };
        buyPatch.right = { 96.f, 0.f, 14.f, 30.f };

        buyPatch.leftNorm = normaliseRect(buyPatch.left);
        buyPatch.centreNorm = normaliseRect(buyPatch.centre);
        buyPatch.rightNorm = normaliseRect(buyPatch.right);

        auto& exitPatch = m_threePatches[ThreePatch::ExitButton];
        exitPatch.left = { 114.f, 0.f, 14.f, 30.f };
        exitPatch.centre = { 128., 0.f, 16.f, 30.f };
        exitPatch.right = { 144.f, 0.f, 14.f, 30.f };

        exitPatch.leftNorm = normaliseRect(exitPatch.left);
        exitPatch.centreNorm = normaliseRect(exitPatch.centre);
        exitPatch.rightNorm = normaliseRect(exitPatch.right);

        auto& statPatch = m_threePatches[ThreePatch::StatBar];
        statPatch.left = { 72.f, 100.f, 12.f, 24.f };
        statPatch.centre = { 84.f, 100.f, 24.f, 24.f };
        statPatch.right = { 106.f, 100.f, 13.f, 24.f }; //again, weird we need this extra pixel

        statPatch.leftNorm = normaliseRect(statPatch.left);
        statPatch.centreNorm = normaliseRect(statPatch.centre);
        statPatch.rightNorm = normaliseRect(statPatch.right);

        //scroll bar and divider are vertical, 'left' at the top
        auto& scrollPatch = m_threePatches[ThreePatch::ScrollBar];
        scrollPatch.left = {128.f, 101.f, 15.f, 3.f};
        scrollPatch.centre = {128.f, 99.f, 15.f, 2.f};
        scrollPatch.right = {128.f, 96.f, 15.f, 3.f};

        scrollPatch.leftNorm = normaliseRect(scrollPatch.left);
        scrollPatch.centreNorm = normaliseRect(scrollPatch.centre);
        scrollPatch.rightNorm = normaliseRect(scrollPatch.right);

        auto& divPatch = m_threePatches[ThreePatch::Divider];
        divPatch.left = {145.f, 101.f, 2.f, 3.f};
        divPatch.centre = {145.f, 99.f, 2.f, 2.f};
        divPatch.right = {145.f, 96.f, 2.f, 3.f};

        divPatch.leftNorm = normaliseRect(divPatch.left);
        divPatch.centreNorm = normaliseRect(divPatch.centre);
        divPatch.rightNorm = normaliseRect(divPatch.right);
    }
}

void ShopState::addSystems()
{
    auto& mb = cro::App::getInstance().getMessageBus();

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::UIElementSystem>(mb);
    m_uiScene.addSystem<cro::UISystem>(mb)->setMouseScrollNavigationEnabled(false);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);

    m_previewScene.addSystem<cro::CallbackSystem>(mb);
    m_previewScene.addSystem<cro::CameraSystem>(mb);
    m_previewScene.addSystem<cro::ModelRenderer>(mb);
}

void ShopState::buildScene()
{
    m_rootNode = m_uiScene.createEntity();
    m_rootNode.addComponent<cro::Transform>();
    m_rootNode.addComponent<cro::Callback>(); //this is updated on quit / cachedPush

    //black background
    constexpr auto BgColour = cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 1.f), BgColour),
            cro::Vertex2D(glm::vec2(0.f), BgColour),
            cro::Vertex2D(glm::vec2(1.f), BgColour),
            cro::Vertex2D(glm::vec2(1.f, 0.f), BgColour),
        });
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, false);
    entity.getComponent<cro::UIElement>().depth = SpriteDepth - 2.f;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [](cro::Entity e)
        {
            glm::vec2 size(cro::App::getWindow().getSize());
            e.getComponent<cro::Transform>().setScale(size);
        };
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/shop_buttons.spt", m_resources.textures);

    //title box in top right corner
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { 1.f, 1.f };
    entity.getComponent<cro::UIElement>().absolutePosition = -(TitleSize + glm::vec2(BorderPadding));
    auto titleRoot = entity;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
    titleRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto& uiSystem = *m_uiScene.getSystem<cro::UISystem>();

    //dummy input to handle events when playing exit transition
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);

    //Top bar - position node
    // |
    // -s/t s/t s/t s/t s/t s/t
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { 0.f, 1.f };
    entity.getComponent<cro::UIElement>().absolutePosition = { BorderPadding, -BorderPadding };
    auto topBarRoot = entity;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    const auto selectedID = uiSystem.addCallback([&](cro::Entity e)
        {
            applyButtonTexture(ButtonTexID::Highlighted, e, m_threePatches[ThreePatch::ButtonTop]);
            e.getComponent<cro::AudioEmitter>().play();
        });

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //and top bar buttons
    const auto createTopButton = [&](glm::vec2 position, const std::string& text, std::int32_t index)
        {
            auto& catItem = m_scrollNodes.emplace_back();
            const auto& patch = m_threePatches[ThreePatch::ButtonTop];

            const auto calcBackgroundSize =
                []()
                {
                    const auto scale = cro::UIElementSystem::getViewScale();
                    const auto screenWidth = std::round(cro::App::getWindow().getSize().x / scale);

                    const auto buttonSpacing = std::round((screenWidth - TitleSize.x - (BorderPadding * 2)) / Category::Count);
                    const auto buttonWidth = buttonSpacing - BorderPadding;

                    return std::make_pair(buttonSpacing, buttonWidth);
                };

            //note relative pos has no effect on sprite/text types
            //the abs position is scaled with the window and set
            //relative to the parent
            auto ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>().setPosition(position);
            ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            ent.addComponent<cro::Drawable2D>().setVertexData(
                {
                    cro::Vertex2D(glm::vec2(0.f, TopButtonSize.y)),
                    cro::Vertex2D(glm::vec2(0.f)),

                    cro::Vertex2D(glm::vec2(patch.left.width, TopButtonSize.y)),
                    cro::Vertex2D(glm::vec2(patch.left.width, 0.f)),

                    cro::Vertex2D(glm::vec2(patch.left.width + patch.centre.width, TopButtonSize.y)),
                    cro::Vertex2D(glm::vec2(patch.left.width + patch.centre.width, 0.f)),

                    cro::Vertex2D(TopButtonSize),
                    cro::Vertex2D(glm::vec2(TopButtonSize.x, 0.f)),
                });
            ent.getComponent<cro::Drawable2D>().setTexture(m_threePatchTexture);
            ent.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
            ent.getComponent<cro::UIElement>().absolutePosition = position;
            ent.getComponent<cro::UIElement>().resizeCallback =
                [index, calcBackgroundSize, patch](cro::Entity e)
                {
                    const auto [buttonSpacing, buttonWidth] = calcBackgroundSize();

                    e.getComponent<cro::UIElement>().absolutePosition.x = buttonSpacing * index;
                    auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
                    verts[4].position.x = buttonWidth - patch.right.width;
                    verts[5].position.x = buttonWidth - patch.right.width;

                    verts[6].position.x = buttonWidth;
                    verts[7].position.x = buttonWidth;

                    e.getComponent<cro::UIInput>().area.width = buttonWidth;
                };

            ent.addComponent<cro::UIInput>().area = { glm::vec2(0.f), TopButtonSize };
            ent.getComponent<cro::UIInput>().setSelectionIndex(CatButtonDriver + index);

            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
                uiSystem.addCallback([&, index](cro::Entity e)
                    {
                        applyButtonTexture(m_selectedCategory == index ? ButtonTexID::Selected : ButtonTexID::Unselected, e, m_threePatches[ThreePatch::ButtonTop]);
                    });
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
                uiSystem.addCallback([&, index](cro::Entity e, const cro::ButtonEvent& evt)
                    {
                        if (activated(evt))
                        {
                            setCategory(index);
                        }
                    });
            applyButtonTexture(m_selectedCategory == index ? ButtonTexID::Selected : ButtonTexID::Unselected, ent, m_threePatches[ThreePatch::ButtonTop]);
            catItem.buttonBackground = ent;
            topBarRoot.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            //can't parent this to the background as it will take on the bg scale
            //which will work, but only if we set this element scaling to false to
            //disable the character size re-scaling (complicated, huh?)
            auto c = m_selectedCategory == index ? ButtonTextSelectedColour : ButtonTextColour;

            ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>();
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Text>(font).setString(text);
            ent.getComponent<cro::Text>().setFillColour(c);
            ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
            ent.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
            ent.getComponent<cro::UIElement>().absolutePosition = position + glm::vec2({ TopButtonSize.x / 2.f, 10.f });
            ent.getComponent<cro::UIElement>().characterSize = UITextSize;
            ent.getComponent<cro::UIElement>().resizeCallback =
                [index, calcBackgroundSize](cro::Entity e)
                {
                    const auto [buttonSpacing, buttonWidth] = calcBackgroundSize();
                    e.getComponent<cro::UIElement>().absolutePosition.x = (buttonSpacing * index) + std::round(buttonWidth / 2.f);
                };
            catItem.buttonText = ent;
            topBarRoot.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
        };

    const std::array<std::string, Category::Count> ButtonText =
    {
        "Drivers",
        "Woods",
        "Irons",
        "Wedges",
        "Balls",
    };


    for (auto i = 0u; i < ButtonText.size(); ++i)
    {
        createTopButton({ (TopButtonSize.x + BorderPadding) * i, -(TopButtonSize.y) }, ButtonText[i], i);
    }

    //update the selection indices - note that these aren't added to their *own* groups as this is superfluous
    m_scrollNodes[Category::Driver].buttonBackground.getComponent<cro::UIInput>().setGroup(MenuID::Wood);
    m_scrollNodes[Category::Driver].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Iron);
    m_scrollNodes[Category::Driver].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Wedge);
    m_scrollNodes[Category::Driver].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Balls);

    m_scrollNodes[Category::Wood].buttonBackground.getComponent<cro::UIInput>().setGroup(MenuID::Driver);
    m_scrollNodes[Category::Wood].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Iron);
    m_scrollNodes[Category::Wood].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Wedge);
    m_scrollNodes[Category::Wood].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Balls);

    m_scrollNodes[Category::Iron].buttonBackground.getComponent<cro::UIInput>().setGroup(MenuID::Driver);
    m_scrollNodes[Category::Iron].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Wood);
    m_scrollNodes[Category::Iron].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Wedge);
    m_scrollNodes[Category::Iron].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Balls);

    m_scrollNodes[Category::Wedge].buttonBackground.getComponent<cro::UIInput>().setGroup(MenuID::Driver);
    m_scrollNodes[Category::Wedge].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Wood);
    m_scrollNodes[Category::Wedge].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Iron);
    m_scrollNodes[Category::Wedge].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Balls);

    m_scrollNodes[Category::Ball].buttonBackground.getComponent<cro::UIInput>().setGroup(MenuID::Driver);
    m_scrollNodes[Category::Ball].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Wood);
    m_scrollNodes[Category::Ball].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Iron);
    m_scrollNodes[Category::Ball].buttonBackground.getComponent<cro::UIInput>().addToGroup(MenuID::Wedge);



    //screen divider
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { DividerOffset, 0.f };
    entity.getComponent<cro::UIElement>().absolutePosition = { -1.f, BorderPadding };
    auto dividerRoot = entity;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    const auto& barPatch = m_threePatches[ThreePatch::Divider];
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setTexture(m_threePatchTexture);
    entity.getComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(barPatch.left.width, 10.f),                        glm::vec2(barPatch.leftNorm.left + barPatch.leftNorm.width, barPatch.leftNorm.bottom + barPatch.leftNorm.height)),
            cro::Vertex2D(glm::vec2(0.f, 10.f),                                        glm::vec2(barPatch.leftNorm.left, barPatch.leftNorm.bottom + barPatch.leftNorm.height)),
            cro::Vertex2D(glm::vec2(barPatch.left.width, 10.f - barPatch.left.height), glm::vec2(barPatch.leftNorm.left + barPatch.leftNorm.width, barPatch.leftNorm.bottom)),
            cro::Vertex2D(glm::vec2(0.f, 10.f - barPatch.left.height),                 glm::vec2(barPatch.leftNorm.left, barPatch.leftNorm.bottom)),
            cro::Vertex2D(glm::vec2(barPatch.left.width, barPatch.right.height),       glm::vec2(barPatch.centreNorm.left + barPatch.centreNorm.width, barPatch.centreNorm.bottom)),
            cro::Vertex2D(glm::vec2(0.f, barPatch.right.height),                       glm::vec2(barPatch.centreNorm.left, barPatch.centreNorm.bottom)),
            cro::Vertex2D(glm::vec2(barPatch.left.width, 0.f),                         glm::vec2(barPatch.rightNorm.left + barPatch.rightNorm.width, barPatch.rightNorm.bottom)),
            cro::Vertex2D(glm::vec2(0.f),                                              glm::vec2(barPatch.rightNorm.left, barPatch.rightNorm.bottom))
        });
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
    entity.getComponent<cro::UIElement>().resizeCallback = 
        [&, barPatch](cro::Entity e)
        {
            auto size = cro::App::getWindow().getSize().y / cro::UIElementSystem::getViewScale();
            size = std::round(size);
            size -= ((BorderPadding * 4.f) + TopButtonSize.y);

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            verts[0].position.y = size;
            verts[1].position.y = size;
            verts[2].position.y = size - barPatch.left.height;
            verts[3].position.y = size - barPatch.left.height;
        };
    dividerRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    
    auto arrowSelect = uiSystem.addCallback([](cro::Entity e) 
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto arrowUnselect = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_up");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
    entity.getComponent<cro::UIElement>().absolutePosition = { -20.f, 50.f };
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e)
        {
            auto size = cro::App::getWindow().getSize().y / cro::UIElementSystem::getViewScale();
            size = std::round(size);
            size -= ((BorderPadding * 4.f) + TopButtonSize.y);
            size -= e.getComponent<cro::Sprite>().getTextureBounds().height;

            e.getComponent<cro::UIElement>().absolutePosition.y = size;
        };
    dividerRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>().setPosition({ -1.f, -1.f, 1.f });
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("highlight_up");
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    buttonEnt.addComponent<cro::UIInput>().area = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(CatScrollUp);
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(CatScrollDown, CatScrollDown);
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(CatScrollDown, CatButtonIron);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowSelect;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowUnselect;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    scroll(true);
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Driver);
    buttonEnt.getComponent<cro::UIInput>().addToGroup(MenuID::Wood);
    buttonEnt.getComponent<cro::UIInput>().addToGroup(MenuID::Iron);
    buttonEnt.getComponent<cro::UIInput>().addToGroup(MenuID::Wedge);
    buttonEnt.getComponent<cro::UIInput>().addToGroup(MenuID::Balls);
    entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
    m_buttonEnts[ButtonID::ScrollUp] = buttonEnt;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_down");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
    entity.getComponent<cro::UIElement>().absolutePosition = { -20.f, BorderPadding };
    dividerRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>().setPosition({ -1.f, -1.f, 1.f });
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("highlight_down");
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    buttonEnt.addComponent<cro::UIInput>().area = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(CatScrollDown);
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonBuy, CatButtonIron);
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(ButtonExit, CatScrollUp);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowSelect;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowUnselect;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    scroll(false);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Driver);
    buttonEnt.getComponent<cro::UIInput>().addToGroup(MenuID::Wood);
    buttonEnt.getComponent<cro::UIInput>().addToGroup(MenuID::Iron);
    buttonEnt.getComponent<cro::UIInput>().addToGroup(MenuID::Wedge);
    buttonEnt.getComponent<cro::UIInput>().addToGroup(MenuID::Balls);
    entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
    m_buttonEnts[ButtonID::ScrollDown] = buttonEnt;

    const auto& patch = m_threePatches[ThreePatch::ButtonItem];
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    //checks if this particular club is unlocked
    const auto itemAvailable = [](const inv::Item& i)
        {
            switch (i.type)
            {
            default: return true;
            case inv::FiveW:
                return Social::getLevel() >= ClubID::getUnlockLevel(ClubID::FiveWood);
            case inv::FourI:
                return Social::getLevel() >= ClubID::getUnlockLevel(ClubID::FourIron);
            case inv::SixI:
                return Social::getLevel() >= ClubID::getUnlockLevel(ClubID::SixIron);
            case inv::SevenI:
                return Social::getLevel() >= ClubID::getUnlockLevel(ClubID::SevenIron);
            case inv::NineI:
                return Social::getLevel() >= ClubID::getUnlockLevel(ClubID::NineIron);
            }
            return true;
        };

    //category list - index is item index within category, NOT into inv::Items array
    //TODO we don't really need to pass the parent ent when we can just capture it
    const auto createItem = 
        [&](cro::Entity parent, glm::vec2 position, const inv::Item item, std::int32_t index, std::int32_t category)
        {
            const auto calcBackgroundSize =
                []()
                {
                    const auto scale = cro::UIElementSystem::getViewScale();
                    const auto screenWidth = std::round(cro::App::getWindow().getSize().x / scale);

                    return std::round((screenWidth * DividerOffset) - BorderPadding * 8.f);
                };


            auto& itemEntry = m_scrollNodes[category].items.emplace_back();

            auto ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>().setPosition(position);
            ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            ent.addComponent<cro::Drawable2D>().setVertexData(
                {
                    cro::Vertex2D(glm::vec2(0.f, ItemButtonSize.y)),
                    cro::Vertex2D(glm::vec2(0.f)),

                    cro::Vertex2D(glm::vec2(patch.left.width, ItemButtonSize.y)),
                    cro::Vertex2D(glm::vec2(patch.left.width, 0.f)),

                    cro::Vertex2D(glm::vec2(patch.left.width + patch.centre.width, ItemButtonSize.y)),
                    cro::Vertex2D(glm::vec2(patch.left.width + patch.centre.width, 0.f)),

                    cro::Vertex2D(glm::vec2(patch.left.width + patch.centre.width + patch.right.width, ItemButtonSize.y)),
                    cro::Vertex2D(glm::vec2(patch.left.width + patch.centre.width + patch.right.width, 0.f)),
                });
            ent.getComponent<cro::Drawable2D>().setTexture(m_threePatchTexture);
            ent.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
            ent.getComponent<cro::UIElement>().absolutePosition = position;
            ent.getComponent<cro::UIElement>().resizeCallback =
                [&, patch, calcBackgroundSize](cro::Entity e)
                {
                    const auto width = calcBackgroundSize();
                    auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();

                    verts[4].position.x = width - patch.right.width;
                    verts[5].position.x = verts[4].position.x;

                    verts[6].position.x = width;
                    verts[7].position.x = width;

                    e.getComponent<cro::UIInput>().area.width = width;
                };
            
            const auto selectionIndex = ((category + 1) * 100) + index;

            ent.addComponent<cro::UIInput>().area = { 0.f, 0.f, ItemButtonSize.x, ItemButtonSize.y };
            ent.getComponent<cro::UIInput>().setGroup(category);
            ent.getComponent<cro::UIInput>().setSelectionIndex(selectionIndex);
            ent.getComponent<cro::UIInput>().setNextIndex(CatScrollUp, selectionIndex + 1);
            ent.getComponent<cro::UIInput>().setPrevIndex(ButtonExit, selectionIndex - 1);
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
                uiSystem.addCallback([&, index, category](cro::Entity e)
                    {
                        applyButtonTexture(ButtonTexID::Highlighted, e, m_threePatches[ThreePatch::ButtonItem]);
                        e.getComponent<cro::AudioEmitter>().play();

                        const auto& item = m_scrollNodes[category].items[index];
                        if (!item.visible
                            && !e.getComponent<cro::UIInput>().wasMouseEvent())
                        {
                            auto target = index;

                            switch (item.cropping)
                            {
                            default:
                            case ItemEntry::None:
                                //do nothing
                                break;
                            case ItemEntry::Bottom:
                            {
                                const auto itemCount = static_cast<std::int32_t>(std::floor((m_catergoryCroppingArea.height / m_viewScale) / ScrollData::Stride)) - 1;
                                target -= itemCount;
                            }
                                [[fallthrough]];
                            case ItemEntry::Top:
                                scrollTo(target);
                                break;
                            }
                        }
                    });
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
                uiSystem.addCallback([&, index, category](cro::Entity e)
                    {
                        const auto texID = m_scrollNodes[category].selectedItem == index ?
                            ButtonTexID::Selected :
                            m_scrollNodes[category].items[index].owned ?
                            ButtonTexID::Owned : ButtonTexID::Unselected;
                        applyButtonTexture(texID, e, m_threePatches[ThreePatch::ButtonItem]);
                    });
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
                uiSystem.addCallback([&, index, category](cro::Entity e, const cro::ButtonEvent& evt)
                    {
                        if (activated(evt))
                        {
                            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                            auto& item = m_scrollNodes[category].items[m_scrollNodes[category].selectedItem];
                            item.buttonText.getComponent<cro::Text>().setFillColour(ButtonTextColour);
                            item.priceText.getComponent<cro::Text>().setFillColour(ButtonPriceColour);
                            applyButtonTexture(item.owned ? ButtonTexID::Owned : ButtonTexID::Unselected, item.buttonBackground, m_threePatches[ThreePatch::ButtonItem]);

                            m_scrollNodes[category].selectedItem = index;

                            auto& newItem = m_scrollNodes[category].items[index];
                            newItem.buttonText.getComponent<cro::Text>().setFillColour(ButtonTextSelectedColour);
                            newItem.priceText.getComponent<cro::Text>().setFillColour(ButtonTextSelectedColour);
                            applyButtonTexture(ButtonTexID::Selected, e, m_threePatches[ThreePatch::ButtonItem]);

                            updateStatDisplay(newItem);

                            if (!newItem.visible)
                            {
                                auto target = index;

                                switch (newItem.cropping)
                                {
                                default:
                                case ItemEntry::None:
                                    //do nothing
                                    break;
                                case ItemEntry::Bottom:
                                {
                                    const auto itemCount = static_cast<std::int32_t>(std::floor((m_catergoryCroppingArea.height / m_viewScale) / ScrollData::Stride)) - 1;
                                    target -= itemCount;
                                }
                                [[fallthrough]];
                                case ItemEntry::Top:
                                    scrollTo(target);
                                    break;
                                }
                            }

                            //update the buy button
                            if (!newItem.owned)
                            {
                                m_buyCounter.str0.getComponent<cro::Text>().setString(m_buyString);
                                m_buyCounter.str1.getComponent<cro::Text>().setString(m_buyString);
                            }
                            else
                            {
                                m_buyCounter.str0.getComponent<cro::Text>().setString(m_sellString);
                                m_buyCounter.str1.getComponent<cro::Text>().setString(m_sellString);
                            }
                        }
                    });

            applyButtonTexture(index == 0 ? ButtonTexID::Selected : ButtonTexID::Unselected, ent, patch);
            parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            itemEntry.buttonBackground = ent;

            //we *can* parent the manufacturer icon to the background as it doesn't need a UIElement
            //TODO replace this with a sprite
            ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>().setPosition(glm::vec3(4.f, 4.f, 0.1f));
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Sprite>() = m_smallLogos[item.manufacturer];
            itemEntry.buttonBackground.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
            itemEntry.badge = ent;

            auto text = inv::Manufacturers[item.manufacturer] + "\n" + inv::ItemStrings[item.type];

            ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>();
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Text>(font).setString(text);
            ent.getComponent<cro::Text>().setVerticalSpacing(3.f);
            ent.getComponent<cro::Text>().setFillColour(index == 0 ? ButtonTextSelectedColour : ButtonTextColour);
            ent.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
            ent.getComponent<cro::UIElement>().absolutePosition = position + glm::vec2({ BorderPadding + 32.f, 26.f });
            ent.getComponent<cro::UIElement>().characterSize = UITextSize;
            parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            itemEntry.buttonText = ent;

            //add the price here
            ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>();
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Text>(smallFont).setString(std::to_string(item.price) + " Cr");
            ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
            ent.getComponent<cro::Text>().setFillColour(index == 0 ? ButtonTextSelectedColour : ButtonPriceColour);
            ent.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
            ent.getComponent<cro::UIElement>().characterSize = InfoTextSize;
            ent.getComponent<cro::UIElement>().resizeCallback =
                [position, calcBackgroundSize](cro::Entity e)
                {
                    const auto width = calcBackgroundSize();
                    const auto textHeight = (InfoTextSize * 2) + std::round(BorderPadding * 1.5f);
                    e.getComponent<cro::UIElement>().absolutePosition = { width - BorderPadding - 2.f, position.y + textHeight };
                };

            parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            itemEntry.priceText = ent;
        };

    //root node for position
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { 0.f, 0.f };
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e)
        {
            const auto scale = cro::UIElementSystem::getViewScale();

            auto size = glm::vec2(cro::App::getWindow().getSize()) / scale;
            size.x = std::round(size.x);
            size.y = std::round(size.y);
            size.y -= ((BorderPadding * 4.f) + TopButtonSize.y);

            e.getComponent<cro::UIElement>().absolutePosition = { BorderPadding, size.y };

            //this is the area we want to crop scroll nodes to - although really it's
            //the same for all nodes so we should only do this once, not every node
            m_catergoryCroppingArea = { 0.f, BorderPadding, size.x, size.y };
            m_catergoryCroppingArea *= scale;
        };

    auto scrollRoot = entity;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //for each category
    std::int32_t prevCat = -1;
    std::int32_t catIndex = 0;
    std::int32_t itemIndex = 0; //index into current category
    std::int32_t itemID = 0; //index into inv::Items

    cro::Entity scrollNode;
    glm::vec2 currentPos(0.f, -(ItemButtonSize.y));
    for (const auto& item : inv::Items)
    {
        if (item.type != prevCat)
        {
            switch (item.type)
            {
            default: break;
            case inv::ItemType::Driver:
            case inv::ItemType::ThreeW:
            case inv::ItemType::NineI:
            case inv::ItemType::PitchWedge:
            case inv::ItemType::Ball:
                //new scroll node (no UIElement)
            {
                auto ent = m_uiScene.createEntity();
                scrollRoot.getComponent<cro::Transform>().addChild(ent.addComponent<cro::Transform>());
                
                if (m_selectedCategory != catIndex)
                {
                    ent.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }

                scrollNode = ent;
                prevCat = item.type;
                currentPos = glm::vec2(0.f, -(ItemButtonSize.y));
                itemIndex = 0;

                //attach the scroll bar logic to the node
                ent.addComponent<cro::Callback>().setUserData<ScrollData>();
                ent.getComponent<cro::Callback>().function =
                    [&, catIndex](cro::Entity e, float dt)
                    {
                        const auto target = e.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex * ScrollData::Stride * m_viewScale;

                        static constexpr float Speed = 15.f;
                        auto& tx = e.getComponent<cro::Transform>();
                        const float move = (target - tx.getPosition().y) * dt * Speed;

                        if (std::abs(move) > 0.1f)
                        {
                            tx.move(glm::vec2(0.f, move));

                            m_scrollNodes[catIndex].cropItems();
                        }
                        else
                        {
                            auto pos = tx.getPosition();
                            pos.y = target;
                            tx.setPosition(pos);
                            e.getComponent<cro::Callback>().active = false;
                        }
                    };

                m_scrollNodes[catIndex].cropItems =
                    [&, catIndex]()
                    {
                        //only iterate over as many as we can assume are at least visible
                        //const auto start = std::max(0, m_scrollNodes[catIndex].scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex - 1);
                        //const auto end = std::min(static_cast<std::int32_t>(m_scrollNodes[catIndex].items.size()/* - 1*/), 
                        //    start + static_cast<std::int32_t>(std::ceil((m_catergoryCroppingArea.height / m_viewScale) / ScrollData::Stride)) + 1);

                        //for(auto i = start; i < end; ++i)

                        //HMMM this would be a nice optimisation, but we need to update
                        //all items to each end of the list as it's possible to navigated to
                        //them using controller / cursor keys :(

                        for(auto i = 0u; i < m_scrollNodes[catIndex].items.size(); ++i)
                        {
                            auto& item = m_scrollNodes[catIndex].items[i];
                            auto bounds = item.buttonBackground.getComponent<cro::Drawable2D>().getLocalBounds();
                            bounds = bounds.transform(item.buttonBackground.getComponent<cro::Transform>().getWorldTransform());

                            if (!m_catergoryCroppingArea.contains(bounds))
                            {
                                item.buttonBackground.getComponent<cro::Drawable2D>().setCroppingArea(m_catergoryCroppingArea, true);
                                item.buttonText.getComponent<cro::Drawable2D>().setCroppingArea(m_catergoryCroppingArea, true);
                                item.priceText.getComponent<cro::Drawable2D>().setCroppingArea(m_catergoryCroppingArea, true);
                                item.badge.getComponent<cro::Drawable2D>().setCroppingArea(m_catergoryCroppingArea, true);

                                item.visible = false;
                                item.cropping = (bounds.bottom < m_catergoryCroppingArea.bottom) ? ItemEntry::Bottom : ItemEntry::Top;
                            }
                            else
                            {
                                item.visible = true;
                                item.cropping = ItemEntry::None;
                            }
                        }
                    };
                m_scrollNodes[catIndex++].scrollNode = ent;
            }
                break;
            }
        }

        //add item to current scroll node
        if (itemAvailable(item))
        {
            createItem(scrollNode, currentPos, item, itemIndex, catIndex - 1);
            scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().itemCount++;
            m_scrollNodes[catIndex - 1].items[itemIndex].itemIndex = itemID;

            //as we don't know the inventory index until we're here, we update the string now
            if (m_sharedData.inventory.inventory[itemID] != -1)
            {
                auto& ownedItem = m_scrollNodes[catIndex - 1].items[itemIndex];

                auto str = std::to_string(discountPrice(inv::Items[itemID].price)) + " Cr\nOwned";
                ownedItem.priceText.getComponent<cro::Text>().setString(str);
                ownedItem.owned = true;
                applyButtonTexture(ButtonTexID::Owned, ownedItem.buttonBackground, m_threePatches[ThreePatch::ButtonItem]);
            }

            itemIndex++;
            currentPos.y -= ScrollData::Stride;
        }
        itemID++; //make sure to always increment this!!
    }

    CRO_ASSERT(m_scrollNodes.size() == Category::Count, "");


    //correct the up/down indices for the first and last item in each cat
    m_scrollNodes[Category::Driver].items.front().buttonBackground.getComponent<cro::UIInput>().setPrevIndex(ButtonExit, CatButtonWood);
    m_scrollNodes[Category::Driver].items.back().buttonBackground.getComponent<cro::UIInput>().setNextIndex(CatScrollDown, CatButtonWood);

    m_scrollNodes[Category::Wood].items.front().buttonBackground.getComponent<cro::UIInput>().setPrevIndex(ButtonExit, CatButtonDriver);
    m_scrollNodes[Category::Wood].items.back().buttonBackground.getComponent<cro::UIInput>().setNextIndex(CatScrollDown, CatButtonDriver);

    m_scrollNodes[Category::Iron].items.front().buttonBackground.getComponent<cro::UIInput>().setPrevIndex(ButtonExit, CatButtonDriver);
    m_scrollNodes[Category::Iron].items.back().buttonBackground.getComponent<cro::UIInput>().setNextIndex(CatScrollDown, CatButtonDriver);

    m_scrollNodes[Category::Wedge].items.front().buttonBackground.getComponent<cro::UIInput>().setPrevIndex(ButtonExit, CatButtonDriver);
    m_scrollNodes[Category::Wedge].items.back().buttonBackground.getComponent<cro::UIInput>().setNextIndex(CatScrollDown, CatButtonDriver);

    m_scrollNodes[Category::Ball].items.front().buttonBackground.getComponent<cro::UIInput>().setPrevIndex(ButtonExit, CatButtonDriver);
    m_scrollNodes[Category::Ball].items.back().buttonBackground.getComponent<cro::UIInput>().setNextIndex(CatScrollDown, CatButtonDriver);



    //buy button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { DividerOffset, 0.f };
    entity.getComponent<cro::UIElement>().absolutePosition = { BorderPadding * 4.f, BorderPadding };
    auto buyNode = entity;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    const auto calcBuyWidth = [&]()
        {
            const auto scale = cro::UIElementSystem::getViewScale();
            const auto screenWidth = std::round(cro::App::getWindow().getSize().x / scale);

            return std::round(((screenWidth - (screenWidth * DividerOffset)) - BorderPadding * 8.f) / 2.f) - BorderPadding;
        };

    const auto& buyPatch = m_threePatches[ThreePatch::BuyButton];
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, BuyButtonSize.y)),
            cro::Vertex2D(glm::vec2(0.f)),

            cro::Vertex2D(glm::vec2(buyPatch.left.width, BuyButtonSize.y)),
            cro::Vertex2D(glm::vec2(buyPatch.left.width, 0.f)),

            cro::Vertex2D(glm::vec2(buyPatch.left.width + buyPatch.centre.width, BuyButtonSize.y)),
            cro::Vertex2D(glm::vec2(buyPatch.left.width + buyPatch.centre.width, 0.f)),

            cro::Vertex2D(BuyButtonSize),
            cro::Vertex2D(glm::vec2(BuyButtonSize.x, 0.f)),
        });
    entity.getComponent<cro::Drawable2D>().setTexture(m_threePatchTexture);
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [buyPatch, calcBuyWidth](cro::Entity e)
        {
            const auto buttonWidth = calcBuyWidth();

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            verts[4].position.x = buttonWidth - buyPatch.right.width;
            verts[5].position.x = buttonWidth - buyPatch.right.width;

            verts[6].position.x = buttonWidth;
            verts[7].position.x = buttonWidth;

            e.getComponent<cro::UIInput>().area.width = buttonWidth;
        };

    entity.addComponent<cro::UIInput>().area = { glm::vec2(0.f), BuyButtonSize };
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonBuy);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonExit, CatButtonWedge);
    entity.getComponent<cro::UIInput>().setPrevIndex(CatScrollDown, CatButtonWedge);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem.addCallback([&](cro::Entity e)
            {
                applyButtonTexture(ButtonTexID::Highlighted, e, m_threePatches[ThreePatch::BuyButton]);
                e.getComponent<cro::AudioEmitter>().play();
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        uiSystem.addCallback([&](cro::Entity e)
            {
                applyButtonTexture(ButtonTexID::Unselected, e, m_threePatches[ThreePatch::BuyButton]);
                m_buyCounter.active = false;
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    auto& item = m_scrollNodes[m_selectedCategory].items[m_scrollNodes[m_selectedCategory].selectedItem];
                    const auto& invItem = inv::Items[item.itemIndex];

                    //can't afford
                    if (m_sharedData.inventory.inventory[item.itemIndex] == -1
                        && invItem.price > m_sharedData.inventory.balance)
                    {
                        m_audioEnts[AudioID::Nope].getComponent<cro::AudioEmitter>().play();
                        
                        //play flash anim
                        m_statItems.balanceText.getComponent<cro::Callback>().active = true;
                    }                     
                    else
                    {
                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                        
                        //we're selling or buying
                        applyButtonTexture(ButtonTexID::Selected, e, m_threePatches[ThreePatch::BuyButton]);
                        m_buyCounter.active = true;
                    }
                }
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                //m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                applyButtonTexture(ButtonTexID::Highlighted, e, m_threePatches[ThreePatch::BuyButton]);
                m_buyCounter.active = false;
            });
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Driver);
    entity.getComponent<cro::UIInput>().addToGroup(MenuID::Wood);
    entity.getComponent<cro::UIInput>().addToGroup(MenuID::Iron);
    entity.getComponent<cro::UIInput>().addToGroup(MenuID::Wedge);
    entity.getComponent<cro::UIInput>().addToGroup(MenuID::Balls);
    applyButtonTexture(ButtonTexID::Unselected, entity, m_threePatches[ThreePatch::BuyButton]);
    buyNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_buttonEnts[ButtonID::Buy] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(m_buyString);
    entity.getComponent<cro::Text>().setFillColour(ButtonTextColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
    entity.getComponent<cro::UIElement>().absolutePosition = glm::vec2({ BuyButtonSize.x / 2.f, 22.f });
    entity.getComponent<cro::UIElement>().characterSize = UITextSize * 2;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [calcBuyWidth](cro::Entity e)
        {
            const auto buttonWidth = calcBuyWidth();
            e.getComponent<cro::UIElement>().absolutePosition.x = std::round(buttonWidth / 2.f);
        };
    m_buyCounter.str0 = entity;
    buyNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //red text for progress effect
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(m_buyString);
    entity.getComponent<cro::Text>().setFillColour(CD32::Colours[CD32::Red]);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
    entity.getComponent<cro::UIElement>().absolutePosition = glm::vec2({ BuyButtonSize.x / 2.f, 22.f });
    entity.getComponent<cro::UIElement>().characterSize = UITextSize * 2;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [calcBuyWidth](cro::Entity e)
        {
            const auto buttonWidth = calcBuyWidth();
            e.getComponent<cro::UIElement>().absolutePosition.x = std::round(buttonWidth / 2.f);
        };
    m_buyCounter.str1 = entity;
    buyNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //exit button
    auto& exitPatch = m_threePatches[ThreePatch::ExitButton];

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, BuyButtonSize.y)),
            cro::Vertex2D(glm::vec2(0.f)),

            cro::Vertex2D(glm::vec2(buyPatch.left.width, BuyButtonSize.y)),
            cro::Vertex2D(glm::vec2(buyPatch.left.width, 0.f)),

            cro::Vertex2D(glm::vec2(buyPatch.left.width + buyPatch.centre.width, BuyButtonSize.y)),
            cro::Vertex2D(glm::vec2(buyPatch.left.width + buyPatch.centre.width, 0.f)),

            cro::Vertex2D(BuyButtonSize),
            cro::Vertex2D(glm::vec2(BuyButtonSize.x, 0.f)),
        });
    entity.getComponent<cro::Drawable2D>().setTexture(m_threePatchTexture);
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [exitPatch, calcBuyWidth](cro::Entity e)
        {
            const auto buttonWidth = calcBuyWidth();

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            verts[4].position.x = buttonWidth - exitPatch.right.width;
            verts[5].position.x = buttonWidth - exitPatch.right.width;

            verts[6].position.x = buttonWidth;
            verts[7].position.x = buttonWidth;

            e.getComponent<cro::UIElement>().absolutePosition.x = buttonWidth + (BorderPadding * 2.f);
            e.getComponent<cro::UIInput>().area.width = buttonWidth;
        };

    entity.addComponent<cro::UIInput>().area = { glm::vec2(0.f), BuyButtonSize };
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonExit);
    entity.getComponent<cro::UIInput>().setNextIndex(CatScrollDown, CatButtonBall);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonBuy, CatButtonBall);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem.addCallback([&](cro::Entity e)
            {
                applyButtonTexture(ButtonTexID::Highlighted, e, m_threePatches[ThreePatch::ExitButton]);
                e.getComponent<cro::AudioEmitter>().play();
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        uiSystem.addCallback([&](cro::Entity e)
            {
                applyButtonTexture(ButtonTexID::Unselected, e, m_threePatches[ThreePatch::ExitButton]);
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    applyButtonTexture(ButtonTexID::Selected, e, m_threePatches[ThreePatch::ExitButton]);
                    quitState();
                }
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                applyButtonTexture(ButtonTexID::Highlighted, e, m_threePatches[ThreePatch::ExitButton]);
            });
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Driver);
    entity.getComponent<cro::UIInput>().addToGroup(MenuID::Wood);
    entity.getComponent<cro::UIInput>().addToGroup(MenuID::Iron);
    entity.getComponent<cro::UIInput>().addToGroup(MenuID::Wedge);
    entity.getComponent<cro::UIInput>().addToGroup(MenuID::Balls);

    applyButtonTexture(ButtonTexID::Unselected, entity, m_threePatches[ThreePatch::ExitButton]);
    buyNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_buttonEnts[ButtonID::Exit] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Exit");
    entity.getComponent<cro::Text>().setFillColour(ButtonTextColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
    entity.getComponent<cro::UIElement>().absolutePosition = glm::vec2({ BuyButtonSize.x / 2.f, 22.f });
    entity.getComponent<cro::UIElement>().characterSize = UITextSize * 2;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [calcBuyWidth](cro::Entity e)
        {
            const auto buttonWidth = calcBuyWidth();
            e.getComponent<cro::UIElement>().absolutePosition.x = (buttonWidth + (BorderPadding * 2.f)) + std::round(buttonWidth / 2.f);
        };
    buyNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //player balance
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { DividerOffset, 1.f };
    entity.getComponent<cro::UIElement>().absolutePosition = { BorderPadding * 4.f, -(TitleSize.y + BorderPadding) };
    auto balanceRoot = entity;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Balance: " + std::to_string(m_sharedData.inventory.balance) + " Cr");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
    entity.getComponent<cro::UIElement>().characterSize = InfoTextSize * 2;
    entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, 22.f };
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e)
        {
            const auto scale = cro::UIElementSystem::getViewScale() * 2.f;
            e.getComponent<cro::Text>().setShadowOffset({ scale, -scale });
        };
    
    entity.addComponent<cro::Callback>().function = TextFlashData();

    balanceRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_statItems.balanceText = entity;
    
    createStatDisplay();

    updateCatIndices();

    //camera resize callback
    auto camCallback = [&](cro::Camera& cam)
        {
            const glm::vec2 size(cro::App::getWindow().getSize());

            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
            cam.setOrthographic(0.f, size.x, 0.f, size.y, -10.f, 10.f);
        };
    m_uiScene.getActiveCamera().getComponent<cro::Camera>().resizeCallback = camCallback;
    camCallback(m_uiScene.getActiveCamera().getComponent<cro::Camera>());
}

void ShopState::buildPreviewScene()
{
    cro::ModelDefinition md(m_resources, &m_envMap);
    md.loadFromFile("assets/golf/models/shop/ball_hardings.cmt");

    cro::Entity entity = m_previewScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
        };


    const auto resizeCallback = 
        [&](cro::Camera& cam)
        {
            const auto size = glm::vec2(m_itemPreviewTexture.getSize());

            cam.setPerspective(cam.getFOV(), size.x / size.y, 0.01f, 1.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };

    auto camEnt = m_previewScene.getDefaultCamera();
    camEnt.getComponent<cro::Transform>().move({ 0.f, 0.f, 0.24f });

    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = resizeCallback;

    //resizeCallback(cam);
}

void ShopState::createStatDisplay()
{
    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    //stat bar root
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { DividerOffset, 1.f };
    entity.getComponent<cro::UIElement>().absolutePosition = { BorderPadding, -(TitleSize.y + (BorderPadding * 2.f)) };
    auto root = entity;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //manufacturer icon
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_largeLogos[0];
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).absolutePosition = { BorderPadding * 3.f, -(LargeIconSize + (BorderPadding * 2.f))};
    entity.getComponent<cro::UIElement>().depth = SpriteDepth;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_statItems.manufacturerIcon = entity;


    //manufacturer name
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Stockley & Brilton");
    entity.getComponent<cro::Text>().setFillColour(StatTextColour);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).absolutePosition = { (BorderPadding * 5.f) + LargeIconSize, -(BorderPadding * 2.f) };
    entity.getComponent<cro::UIElement>().characterSize = UITextSize * 2;
    entity.getComponent<cro::UIElement>().depth = TextDepth;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_statItems.manufacturerName = entity;


    //item name
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Pack of 25 Balls - 2500Cr");
    entity.getComponent<cro::Text>().setFillColour(StatTextColour);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).absolutePosition = { (BorderPadding * 5.f) + LargeIconSize, -((UITextSize * 2) + (BorderPadding * 3.f)) };
    entity.getComponent<cro::UIElement>().characterSize = InfoTextSize * 2;
    entity.getComponent<cro::UIElement>().depth = TextDepth;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_statItems.itemName = entity;

    
    const auto calcBackgroundWidth =
        []()
        {
            const auto scale = cro::UIElementSystem::getViewScale();
            const auto screenWidth = std::round(cro::App::getWindow().getSize().x / scale);

            return std::round((screenWidth - (screenWidth * DividerOffset)) - BorderPadding * 8.f);
        };

    const auto createDisplay = [&](glm::vec2 pos)
        {
            auto& [background, text] = m_statItems.statBars.emplace_back();
            const auto& patch = m_threePatches[ThreePatch::StatBar];
            const float StatWidth = patch.left.width + patch.centre.width + patch.right.width;
            const float StatHeight = patch.left.height;

            //stat bar
            auto ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>();
            ent.addComponent<cro::Drawable2D>().setVertexData(
                {
                    cro::Vertex2D(glm::vec2(0.f, StatHeight), glm::vec2(patch.leftNorm.left, patch.leftNorm.bottom + patch.leftNorm.height)),
                    cro::Vertex2D(glm::vec2(0.f)            , glm::vec2(patch.leftNorm.left, patch.leftNorm.bottom)),

                    cro::Vertex2D(glm::vec2(patch.left.width, StatHeight), glm::vec2(patch.leftNorm.left + patch.leftNorm.width, patch.leftNorm.bottom + patch.leftNorm.height)),
                    cro::Vertex2D(glm::vec2(patch.left.width, 0.f),        glm::vec2(patch.leftNorm.left + patch.leftNorm.width, patch.leftNorm.bottom)),

                    cro::Vertex2D(glm::vec2(patch.centre.left + (patch.centre.width / 2.f), StatHeight), glm::vec2(patch.centreNorm.left + (patch.centreNorm.width / 2.f), patch.centreNorm.bottom + patch.centreNorm.height)),
                    cro::Vertex2D(glm::vec2(patch.centre.left + (patch.centre.width / 2.f), 0.f),        glm::vec2(patch.centreNorm.left + (patch.centreNorm.width / 2.f), patch.centreNorm.bottom)),

                    cro::Vertex2D(glm::vec2(patch.left.width + patch.centre.width, StatHeight), glm::vec2(patch.rightNorm.left, patch.rightNorm.bottom + patch.rightNorm.height)),
                    cro::Vertex2D(glm::vec2(patch.left.width + patch.centre.width, 0.f),        glm::vec2(patch.rightNorm.left, patch.rightNorm.bottom)),

                    cro::Vertex2D(glm::vec2(StatWidth, StatHeight), glm::vec2(patch.rightNorm.left + patch.rightNorm.width, patch.rightNorm.bottom + patch.rightNorm.height)),
                    cro::Vertex2D(glm::vec2(StatWidth, 0.f),        glm::vec2(patch.rightNorm.left + patch.rightNorm.width, patch.rightNorm.bottom))
                });
            ent.getComponent<cro::Drawable2D>().setTexture(m_threePatchTexture);
            ent.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).absolutePosition = pos;
            ent.getComponent<cro::UIElement>().depth = SpriteDepth;
            ent.getComponent<cro::UIElement>().resizeCallback =
                [patch,calcBackgroundWidth](cro::Entity e)
                {
                    const float newSize = calcBackgroundWidth();

                    auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
                    verts[4].position.x = std::round(newSize / 2.f);
                    verts[5].position.x = std::round(newSize / 2.f);
                    
                    verts[6].position.x = newSize - patch.right.width;
                    verts[7].position.x = newSize - patch.right.width;

                    verts[8].position.x = newSize;
                    verts[9].position.x = newSize;

                    e.getComponent<cro::Callback>().getUserData<StatBarData>().currentSize = newSize;
                };

            ent.addComponent<cro::Callback>().active = true;
            ent.getComponent<cro::Callback>().setUserData<StatBarData>();
            ent.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
                {
                    static constexpr float SegmentCount = 20.f;
                    const auto& [size, value] = e.getComponent<cro::Callback>().getUserData<StatBarData>();
                    
                    const float segSize = size / SegmentCount;
                    const float targetPos = (segSize * (SegmentCount / 2.f)) + (value * segSize);

                    auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
                    const auto movement = (targetPos - verts[4].position.x) * dt * 6.f;

                    verts[4].position.x = std::round(verts[4].position.x + movement);
                    verts[5].position.x = std::roundf(verts[5].position.x + movement);
                };
            background = ent;
            root.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            //stat text
            ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>();
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Text>(smallFont).setString("Hook/Slice Reduction");
            ent.getComponent<cro::Text>().setFillColour(StatTextColour);
            ent.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
            ent.getComponent<cro::UIElement>().characterSize = InfoTextSize;
            ent.getComponent<cro::UIElement>().absolutePosition = pos + glm::vec2(6.f, InfoTextSize + 5.f);
            text = ent;
            root.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
    };
    
    glm::vec2 pos = { BorderPadding * 3.f, -((UITextSize * 8) + (BorderPadding * 2.f)) };
    createDisplay(pos);

    pos = { BorderPadding * 3.f, -((UITextSize * 8) + StatBarHeight + (BorderPadding * 4.f)) };
    createDisplay(pos);



    //item thumbnail
    pos.y -= BorderPadding * 2.f;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_itemPreviewTexture.getTexture());
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, false).depth = SpriteDepth;
    entity.getComponent<cro::UIElement>().absolutePosition = pos;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&, pos, calcBackgroundWidth](cro::Entity e)
        {
            const auto viewScale = cro::UIElementSystem::getViewScale();

            const auto width = static_cast<std::uint32_t>(calcBackgroundWidth() * viewScale);
            
            const auto basePos = -(TitleSize.y + (BorderPadding * 2.f)) + pos.y;
            const auto windowHeight = std::round(cro::App::getWindow().getSize().y / viewScale);
            const auto height = ((windowHeight + basePos) - (BuyButtonSize.y + (BorderPadding * 5.f)));

            m_itemPreviewTexture.create(width, static_cast<std::uint32_t>(height * viewScale), true, false, 2);
            e.getComponent<cro::Sprite>().setTexture(m_itemPreviewTexture.getTexture());
            //e.getComponent<cro::Sprite>().setTextureRect({ glm::vec2(0.f), glm::vec2(static_cast<float>(width), height) });
            e.getComponent<cro::Transform>().setScale(glm::vec2(1.f / viewScale));

            auto p = pos * viewScale;
            p.y -= std::round(height * viewScale);
            e.getComponent<cro::UIElement>().absolutePosition = p;

            //hmm this will get called again on the actual resize for no reason
            //but this is the only way to set the initial cam view once the texture
            //is created the very first time
            auto& cam = m_previewScene.getActiveCamera().getComponent<cro::Camera>();
            cam.resizeCallback(cam);
        };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //stat background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
            cro::Vertex2D(glm::vec2(0.f, -400.f), CD32::Colours[CD32::Brown]),
            cro::Vertex2D(glm::vec2(200.f, 0.f), CD32::Colours[CD32::Brown]),
            cro::Vertex2D(glm::vec2(200.f, -400.f), CD32::Colours[CD32::Brown])
        });
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth - 1.f;
    entity.getComponent<cro::UIElement>().absolutePosition = { BorderPadding, 0.f };
    entity.getComponent<cro::UIElement>().resizeCallback = 
        [calcBackgroundWidth](cro::Entity e)
        {
            const auto width = calcBackgroundWidth() + (BorderPadding * 4.f);
            const auto basePos = -(TitleSize.y + (BorderPadding * 2.f));
            const auto windowHeight = std::round(cro::App::getWindow().getSize().y / cro::UIElementSystem::getViewScale());
            const auto height = (windowHeight + basePos) - (BuyButtonSize.y + (BorderPadding * 3.f));

            constexpr auto Light = CD32::Colours[CD32::Olive];
            constexpr auto Mid = CD32::Colours[CD32::Brown];
            constexpr auto Dark = CD32::Colours[CD32::TanDarkest];

            /*auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            verts[1].position.y = -height;
            verts[2].position.x = width;
            verts[3].position.x = width;
            verts[3].position.y = -height;*/

            e.getComponent<cro::Drawable2D>().setVertexData(
                {
                    //Top
                    cro::Vertex2D(glm::vec2(1.f, 0.f), Light),
                    cro::Vertex2D(glm::vec2(1.f, -1.f), Light),
                    cro::Vertex2D(glm::vec2(width - 1.f, 0.f), Light),
                    
                    cro::Vertex2D(glm::vec2(width - 1.f, 0.f), Light),
                    cro::Vertex2D(glm::vec2(1.f, -1.f), Light),
                    cro::Vertex2D(glm::vec2(width - 1.f, -1.f), Light),

                    //Body
                    cro::Vertex2D(glm::vec2(1.f, -1.f), Mid),
                    cro::Vertex2D(glm::vec2(1.f, -(height - 1.f)), Mid),
                    cro::Vertex2D(glm::vec2(width - 1.f, -1.f), Mid),
                    
                    cro::Vertex2D(glm::vec2(width - 1.f, -1.f), Mid),
                    cro::Vertex2D(glm::vec2(1.f, -(height - 1.f)), Mid),
                    cro::Vertex2D(glm::vec2(width - 1.f, -(height - 1.f)), Mid),

                    //Bottom
                    cro::Vertex2D(glm::vec2(1.f, -(height - 1.f)), Dark),
                    cro::Vertex2D(glm::vec2(1.f, -height), Dark),
                    cro::Vertex2D(glm::vec2(width - 1.f, -(height - 1.f)), Dark),
                    
                    cro::Vertex2D(glm::vec2(width - 1.f, -(height - 1.f)), Dark),
                    cro::Vertex2D(glm::vec2(1.f, -height), Dark),
                    cro::Vertex2D(glm::vec2(width - 1.f, -height), Dark),

                    //Left
                    cro::Vertex2D(glm::vec2(0.f, -1.f), Light),
                    cro::Vertex2D(glm::vec2(0.f, -(height-1.f)), Light),
                    cro::Vertex2D(glm::vec2(1.f, -1.f), Light),
                    
                    cro::Vertex2D(glm::vec2(1.f, -1.f), Light),
                    cro::Vertex2D(glm::vec2(0.f, -(height - 1.f)), Light),
                    cro::Vertex2D(glm::vec2(1.f, -(height - 1.f)), Light),

                    //Right
                    cro::Vertex2D(glm::vec2(width - 1.f, -1.f), Dark),
                    cro::Vertex2D(glm::vec2(width - 1.f, -(height - 1.f)), Dark),
                    cro::Vertex2D(glm::vec2(width, -1.f), Dark),
                    
                    cro::Vertex2D(glm::vec2(width, -1.f), Dark),
                    cro::Vertex2D(glm::vec2(width - 1.f, -(height - 1.f)), Dark),
                    cro::Vertex2D(glm::vec2(width, -(height - 1.f)), Dark),
                });
        };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    updateStatDisplay(m_scrollNodes[0].items[0]);
}

void ShopState::updateStatDisplay(const ItemEntry& itemEntry)
{
    const auto itemIndex = itemEntry.itemIndex;

    CRO_ASSERT(itemIndex < inv::Items.size(), "");
    
    const auto& item = inv::Items[itemIndex];
    auto typeStr = inv::ItemStrings[item.type];
    if (itemEntry.owned)
    {
        typeStr += " (Owned)";
    }
    m_statItems.manufacturerName.getComponent<cro::Text>().setString(inv::Manufacturers[item.manufacturer]);
    m_statItems.itemName.getComponent<cro::Text>().setString(typeStr);

    //ugh nice consistency in the index naming here...
    auto& [bg1, text1] = m_statItems.statBars[0];
    bg1.getComponent<cro::Callback>().getUserData<StatBarData>().value = static_cast<float>(item.stat01);
    
    std::string valStr;
    if (item.stat01 > -1)
    {
        valStr += "+";
    }
    valStr += std::to_string(item.stat01);
    text1.getComponent<cro::Text>().setString(inv::StatLabels[m_selectedCategory].stat0 + valStr);

    auto& [bg2, text2] = m_statItems.statBars[1];
    bg2.getComponent<cro::Callback>().getUserData<StatBarData>().value = static_cast<float>(item.stat02);


    //second stat might be empty, eg balls
    valStr.clear();

    if (!inv::StatLabels[m_selectedCategory].stat1.empty())
    {
        if (item.stat02 > -1)
        {
            valStr += "+";
        }
        valStr += std::to_string(item.stat02);
        bg2.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
    }
    else
    {
        bg2.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    }
    text2.getComponent<cro::Text>().setString(inv::StatLabels[m_selectedCategory].stat1 + valStr);

    //update manufacturer icon
    m_statItems.manufacturerIcon.getComponent<cro::Sprite>().setTextureRect(m_largeLogos[item.manufacturer].getTextureRect());

}

void ShopState::setCategory(std::int32_t index)
{
    m_scrollNodes[m_selectedCategory].scrollNode.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_scrollNodes[m_selectedCategory].buttonText.getComponent<cro::Text>().setFillColour(ButtonTextColour);
    applyButtonTexture(ButtonTexID::Unselected, m_scrollNodes[m_selectedCategory].buttonBackground, m_threePatches[ThreePatch::ButtonTop]);

    m_selectedCategory = index;

    m_scrollNodes[m_selectedCategory].scrollNode.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    m_scrollNodes[m_selectedCategory].buttonText.getComponent<cro::Text>().setFillColour(ButtonTextSelectedColour);
    applyButtonTexture(ButtonTexID::Selected, m_scrollNodes[m_selectedCategory].buttonBackground, m_threePatches[ThreePatch::ButtonTop]);

    const auto& activeItem = m_scrollNodes[index].items[m_scrollNodes[index].selectedItem];
    updateStatDisplay(activeItem);

    if (!activeItem.owned)
    {
        m_buyCounter.str0.getComponent<cro::Text>().setString(m_buyString);
        m_buyCounter.str1.getComponent<cro::Text>().setString(m_buyString);
    }
    else
    {
        m_buyCounter.str0.getComponent<cro::Text>().setString(m_sellString);
        m_buyCounter.str1.getComponent<cro::Text>().setString(m_sellString);
    }

    m_scrollNodes[index].cropItems();

    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(index);
    updateCatIndices(); //updates the navigation indices for UI

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
}

void ShopState::updateCatIndices()
{
    //called when switching categories to set the correct
    //prev/next indices for the top row buttons
    switch (m_selectedCategory)
    {
    default: break;
    case Category::Driver:
        //no need to set the active cat as the button is disabled
    {
        auto& ip1 = m_scrollNodes[Category::Wood].buttonBackground.getComponent<cro::UIInput>();
        ip1.setNextIndex(CatButtonIron, CatItemDriver);
        ip1.setPrevIndex(CatButtonBall, CatItemDriver + (m_scrollNodes[m_selectedCategory].items.size() - 1));

        auto& ip2 = m_scrollNodes[Category::Iron].buttonBackground.getComponent<cro::UIInput>();
        ip2.setNextIndex(CatButtonWedge, CatScrollUp); //down scroll, up other scroll
        ip2.setPrevIndex(CatButtonWood, CatScrollDown);

        auto& ip3 = m_scrollNodes[Category::Wedge].buttonBackground.getComponent<cro::UIInput>();
        ip3.setNextIndex(CatButtonBall, ButtonBuy); //up/down buy button
        ip3.setPrevIndex(CatButtonWood, ButtonBuy);

        auto& ip4 = m_scrollNodes[Category::Ball].buttonBackground.getComponent<cro::UIInput>();
        ip4.setNextIndex(CatButtonWood, ButtonExit); //up/down exit button
        ip4.setPrevIndex(CatButtonWedge, ButtonExit);


        auto& ip5 = m_buttonEnts[ButtonID::ScrollUp].getComponent<cro::UIInput>();
        ip5.setNextIndex(CatItemDriver, CatScrollDown);
        ip5.setPrevIndex(CatItemDriver, CatButtonIron);

        auto& ip6 = m_buttonEnts[ButtonID::ScrollDown].getComponent<cro::UIInput>();
        ip6.setNextIndex(ButtonBuy, CatButtonIron);
        ip6.setPrevIndex(CatItemDriver + (m_scrollNodes[m_selectedCategory].items.size() - 1), CatScrollUp);

        auto& ip7 = m_buttonEnts[ButtonID::Buy].getComponent<cro::UIInput>();
        ip7.setNextIndex(ButtonExit, CatButtonWedge);
        ip7.setPrevIndex(CatScrollDown, CatButtonWedge);

        auto& ip8 = m_buttonEnts[ButtonID::Exit].getComponent<cro::UIInput>();
        ip8.setNextIndex(CatItemDriver + (m_scrollNodes[m_selectedCategory].items.size() - 1));
        ip8.setPrevIndex(ButtonBuy);
    }

        break;
    case Category::Wood:
    {
        auto& ip1 = m_scrollNodes[Category::Driver].buttonBackground.getComponent<cro::UIInput>();
        ip1.setNextIndex(CatButtonIron, CatItemWood);
        ip1.setPrevIndex(CatButtonBall, CatItemWood + (m_scrollNodes[m_selectedCategory].items.size() - 1));

        auto& ip2 = m_scrollNodes[Category::Iron].buttonBackground.getComponent<cro::UIInput>();
        ip2.setNextIndex(CatButtonWedge, CatScrollUp);
        ip2.setPrevIndex(CatButtonDriver, CatScrollDown);

        auto& ip3 = m_scrollNodes[Category::Wedge].buttonBackground.getComponent<cro::UIInput>();
        ip3.setNextIndex(CatButtonBall, ButtonBuy);
        ip3.setPrevIndex(CatButtonIron, ButtonBuy);

        auto& ip4 = m_scrollNodes[Category::Ball].buttonBackground.getComponent<cro::UIInput>();
        ip4.setNextIndex(CatButtonDriver, ButtonExit);
        ip4.setPrevIndex(CatButtonWedge, ButtonExit);



        auto& ip5 = m_buttonEnts[ButtonID::ScrollUp].getComponent<cro::UIInput>();
        ip5.setNextIndex(CatItemWood, CatScrollDown);
        ip5.setPrevIndex(CatItemWood, CatButtonIron);

        auto& ip6 = m_buttonEnts[ButtonID::ScrollDown].getComponent<cro::UIInput>();
        ip6.setNextIndex(ButtonBuy, CatButtonIron);
        ip6.setPrevIndex(CatItemWood + (m_scrollNodes[m_selectedCategory].items.size() - 1), CatScrollUp);

        auto& ip7 = m_buttonEnts[ButtonID::Buy].getComponent<cro::UIInput>();
        ip7.setNextIndex(ButtonExit, CatButtonWedge);
        ip7.setPrevIndex(CatScrollDown, CatButtonWedge);

        auto& ip8 = m_buttonEnts[ButtonID::Exit].getComponent<cro::UIInput>();
        ip8.setNextIndex(CatItemWood + (m_scrollNodes[m_selectedCategory].items.size() - 1), CatButtonBall);
        ip8.setPrevIndex(ButtonBuy, CatButtonBall);
    }
        break;
    case Category::Iron:
    {
        auto& ip1 = m_scrollNodes[Category::Driver].buttonBackground.getComponent<cro::UIInput>();
        ip1.setNextIndex(CatButtonWood, CatItemIron);
        ip1.setPrevIndex(CatButtonBall, CatItemIron + (m_scrollNodes[m_selectedCategory].items.size() - 1));

        auto& ip2 = m_scrollNodes[Category::Wood].buttonBackground.getComponent<cro::UIInput>();
        ip2.setNextIndex(CatButtonWedge, CatScrollUp);
        ip2.setPrevIndex(CatButtonDriver, CatScrollDown);

        auto& ip3 = m_scrollNodes[Category::Wedge].buttonBackground.getComponent<cro::UIInput>();
        ip3.setNextIndex(CatButtonBall,ButtonBuy);
        ip3.setPrevIndex(CatButtonWood, ButtonBuy);

        auto& ip4 = m_scrollNodes[Category::Ball].buttonBackground.getComponent<cro::UIInput>();
        ip4.setNextIndex(CatButtonDriver, ButtonExit);
        ip4.setPrevIndex(CatButtonWedge, ButtonExit);



        auto& ip5 = m_buttonEnts[ButtonID::ScrollUp].getComponent<cro::UIInput>();
        ip5.setNextIndex(CatItemIron, CatScrollDown);
        ip5.setPrevIndex(CatItemIron, CatButtonWedge);

        auto& ip6 = m_buttonEnts[ButtonID::ScrollDown].getComponent<cro::UIInput>();
        ip6.setNextIndex(ButtonBuy, CatButtonWedge);
        ip6.setPrevIndex(CatItemIron + (m_scrollNodes[m_selectedCategory].items.size() - 1), CatScrollUp);

        auto& ip7 = m_buttonEnts[ButtonID::Buy].getComponent<cro::UIInput>();
        ip7.setNextIndex(ButtonExit, CatButtonWedge);
        ip7.setPrevIndex(CatScrollDown, CatButtonWedge);

        auto& ip8 = m_buttonEnts[ButtonID::Exit].getComponent<cro::UIInput>();
        ip8.setNextIndex(CatItemIron + (m_scrollNodes[m_selectedCategory].items.size() - 1), CatButtonBall);
        ip8.setPrevIndex(ButtonBuy, CatButtonBall);
    }
        break;
    case Category::Wedge:
    {
        auto& ip1 = m_scrollNodes[Category::Driver].buttonBackground.getComponent<cro::UIInput>();
        ip1.setNextIndex(CatButtonWood, CatItemWedge);
        ip1.setPrevIndex(CatButtonBall, CatItemWedge + (m_scrollNodes[m_selectedCategory].items.size() - 1));

        auto& ip2 = m_scrollNodes[Category::Wood].buttonBackground.getComponent<cro::UIInput>();
        ip2.setNextIndex(CatButtonIron, CatItemWedge);
        ip2.setPrevIndex(CatButtonDriver, CatItemWedge + (m_scrollNodes[m_selectedCategory].items.size() - 1));

        auto& ip3 = m_scrollNodes[Category::Iron].buttonBackground.getComponent<cro::UIInput>();
        ip3.setNextIndex(CatButtonBall, CatScrollUp);
        ip3.setPrevIndex(CatButtonWood, CatScrollDown);

        auto& ip4 = m_scrollNodes[Category::Ball].buttonBackground.getComponent<cro::UIInput>();
        ip4.setNextIndex(CatButtonDriver, ButtonExit);
        ip4.setPrevIndex(CatButtonIron, ButtonExit);



        auto& ip5 = m_buttonEnts[ButtonID::ScrollUp].getComponent<cro::UIInput>();
        ip5.setNextIndex(CatItemWedge, CatScrollDown);
        ip5.setPrevIndex(CatItemWedge, CatButtonIron);

        auto& ip6 = m_buttonEnts[ButtonID::ScrollDown].getComponent<cro::UIInput>();
        ip6.setNextIndex(ButtonBuy, CatButtonIron);
        ip6.setPrevIndex(CatItemWedge + (m_scrollNodes[m_selectedCategory].items.size() - 1), CatScrollUp);

        auto& ip7 = m_buttonEnts[ButtonID::Buy].getComponent<cro::UIInput>();
        ip7.setNextIndex(ButtonExit, CatButtonBall);
        ip7.setPrevIndex(CatScrollDown, CatButtonBall);

        auto& ip8 = m_buttonEnts[ButtonID::Exit].getComponent<cro::UIInput>();
        ip8.setNextIndex(CatItemWedge - (m_scrollNodes[m_selectedCategory].items.size() - 1), CatButtonBall);
        ip8.setPrevIndex(ButtonBuy, CatButtonBall);
    }
        break;
    case Category::Ball:
    {
        auto& ip1 = m_scrollNodes[Category::Driver].buttonBackground.getComponent<cro::UIInput>();
        ip1.setNextIndex(CatButtonWood, CatItemBall);
        ip1.setPrevIndex(CatButtonWedge, CatItemBall + (m_scrollNodes[m_selectedCategory].items.size() - 1));

        auto& ip2 = m_scrollNodes[Category::Wood].buttonBackground.getComponent<cro::UIInput>();
        ip2.setNextIndex(CatButtonIron, CatItemBall);
        ip2.setPrevIndex(CatButtonDriver, CatItemBall + (m_scrollNodes[m_selectedCategory].items.size() - 1));

        auto& ip3 = m_scrollNodes[Category::Iron].buttonBackground.getComponent<cro::UIInput>();
        ip3.setNextIndex(CatButtonWedge, CatScrollUp);
        ip3.setPrevIndex(CatButtonWood, CatScrollDown);

        auto& ip4 = m_scrollNodes[Category::Wedge].buttonBackground.getComponent<cro::UIInput>();
        ip4.setNextIndex(CatButtonDriver, ButtonBuy);
        ip4.setPrevIndex(CatButtonIron, ButtonBuy);



        auto& ip5 = m_buttonEnts[ButtonID::ScrollUp].getComponent<cro::UIInput>();
        ip5.setNextIndex(CatItemBall, CatScrollDown);
        ip5.setPrevIndex(CatItemBall, CatButtonIron);

        auto& ip6 = m_buttonEnts[ButtonID::ScrollDown].getComponent<cro::UIInput>();
        ip6.setNextIndex(ButtonBuy, CatButtonIron);
        ip6.setPrevIndex(CatItemBall + (m_scrollNodes[m_selectedCategory].items.size() - 1), CatScrollUp);

        auto& ip7 = m_buttonEnts[ButtonID::Buy].getComponent<cro::UIInput>();
        ip7.setNextIndex(ButtonExit, CatButtonWedge);
        ip7.setPrevIndex(CatScrollDown, CatButtonWedge);

        auto& ip8 = m_buttonEnts[ButtonID::Exit].getComponent<cro::UIInput>();
        ip8.setNextIndex(CatItemBall + (m_scrollNodes[m_selectedCategory].items.size() - 1), CatButtonWedge);
        ip8.setPrevIndex(ButtonBuy, CatButtonWedge);
    }
        break;
    }
}

void ShopState::scroll(bool up)
{
    auto node = m_scrollNodes[m_selectedCategory].scrollNode;
    auto& currentTarget = node.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex;
    const auto itemCount = node.getComponent<cro::Callback>().getUserData<ScrollData>().itemCount;

    if (up)
    {
        currentTarget = std::max(0, currentTarget - 1);
    }
    else
    {
        currentTarget = std::min(itemCount - 1, currentTarget + 1);
    }

    node.getComponent<cro::Callback>().active = true;
}

void ShopState::scrollTo(std::int32_t targetIndex)
{
    auto node = m_scrollNodes[m_selectedCategory].scrollNode;
    auto& currentTarget = node.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex;
    const auto itemCount = node.getComponent<cro::Callback>().getUserData<ScrollData>().itemCount;

    currentTarget = std::clamp(targetIndex, 0, itemCount - 1);
    node.getComponent<cro::Callback>().active = true;
}

void ShopState::purchaseItem()
{
    auto& item = m_scrollNodes[m_selectedCategory].items[m_scrollNodes[m_selectedCategory].selectedItem];
    const auto& invItem = inv::Items[item.itemIndex];

    m_sharedData.inventory.inventory[item.itemIndex] = 1; //hmmmm I was going to store the actual indices here, but surely a bool will do?
    m_sharedData.inventory.balance -= invItem.price;
    inv::write(m_sharedData.inventory);

    auto str = std::to_string(discountPrice(invItem.price)) + " Cr\nOwned";
    item.priceText.getComponent<cro::Text>().setString(str);
    //applyButtonTexture(ButtonTexID::Owned, item.buttonBackground, m_threePatches[ThreePatch::ButtonItem]);
    item.owned = true;

    m_statItems.balanceText.getComponent<cro::Text>().setString("Balance: " + std::to_string(m_sharedData.inventory.balance) + " Cr");

    m_buyCounter.str0.getComponent<cro::Text>().setString(m_sellString);
    m_buyCounter.str1.getComponent<cro::Text>().setString(m_sellString);
}

void ShopState::sellItem()
{
    auto& item = m_scrollNodes[m_selectedCategory].items[m_scrollNodes[m_selectedCategory].selectedItem];
    const auto& invItem = inv::Items[item.itemIndex];

    m_sharedData.inventory.inventory[item.itemIndex] = -1;
    m_sharedData.inventory.balance += discountPrice(invItem.price);
    inv::write(m_sharedData.inventory);

    auto str = std::to_string(invItem.price) + " Cr";
    item.priceText.getComponent<cro::Text>().setString(str);
    //applyButtonTexture(ButtonTexID::Selected, item.buttonBackground, m_threePatches[ThreePatch::ButtonItem]);
    item.owned = false;

    m_statItems.balanceText.getComponent<cro::Text>().setString("Balance: " + std::to_string(m_sharedData.inventory.balance) + " Cr");

    m_buyCounter.str0.getComponent<cro::Text>().setString(m_buyString);
    m_buyCounter.str1.getComponent<cro::Text>().setString(m_buyString);
}

void ShopState::quitState()
{
    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_rootNode.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            const float width = static_cast<float>(cro::App::getWindow().getSize().x);
            e.getComponent<cro::Transform>().move({ -width * 6.f * dt, 0.f });
            const auto pos = e.getComponent<cro::Transform>().getPosition().x;

            if (pos < -width)
            {
                e.getComponent<cro::Callback>().active = false;
                requestStackPop();
            }
        };
}

void ShopState::onCachedPush()
{
    m_viewScale = cro::UIElementSystem::getViewScale();
    
    Timeline::setTimelineDesc("In the Shop");
    Social::setStatus(Social::InfoID::Menu, { "At the equipment counter" });

    const float width = static_cast<float>(cro::App::getWindow().getSize().x);
    m_rootNode.getComponent<cro::Transform>().setPosition(glm::vec2(width, 0.f));

    m_rootNode.getComponent<cro::Callback>().active = true;
    m_rootNode.getComponent<cro::Callback>().function =
        [&, width](cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().move({ -width * 6.f * dt, 0.f });
            const auto pos = e.getComponent<cro::Transform>().getPosition().x;

            if (pos < 0.f)
            {
                e.getComponent<cro::Callback>().active = false;
                m_rootNode.getComponent<cro::Transform>().setPosition(glm::vec2(0.f));
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(m_selectedCategory);
            }
        };
}