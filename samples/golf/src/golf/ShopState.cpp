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
#include "Inventory.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/UIElement.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>

namespace
{
    //from edge of window, scaled with getViewScale()
    constexpr float BorderPadding = 4.f;

    constexpr float PositionDepth = 0.f;
    constexpr float SpriteDepth = -2.f;
    constexpr float TextDepth = 2.f;

    constexpr glm::vec2 TopButtonSize = glm::vec2(68.f, 12.f);
    constexpr glm::vec2 TitleSize = glm::vec2(160.f, 100.f);
    constexpr glm::vec2 ItemButtonSize = glm::vec2(200.f, 32.f);

    constexpr float DividerOffset = 0.4f;

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
            Unselected, Highlighted, Selected
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
}

ShopState::ShopState(cro::StateStack& stack, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (stack, ctx),
    m_sharedData        (sd),
    m_uiScene           (ctx.appInstance.getMessageBus(), 384),
    m_selectedCategory  (Category::Driver)
{
    CRO_ASSERT(!isCached(), "");

    ctx.mainWindow.loadResources([&]()
        {
            loadAssets();
            addSystems();
            buildScene();
        });
}

//public
bool ShopState::handleEvent(const cro::Event& evt)
{
    m_uiScene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_uiScene.forwardEvent(evt);

    return false;
}

void ShopState::handleMessage(const cro::Message& msg)
{
    m_uiScene.forwardMessage(msg);
}

bool ShopState::simulate(float dt)
{
    m_uiScene.simulate(dt);

    return true;
}

void ShopState::render()
{
    m_uiScene.render();
}

//private
void ShopState::loadAssets()
{
    //load up the three-patch data for the button textures
    //TODO if we use this excessively then create a cfg format

    m_threePatchTexture.loadFromFile("assets/golf/images/shop_buttons.png");
    m_threePatchTexture.setRepeated(true);
    const auto texSize = glm::vec2(m_threePatchTexture.getSize());
    
    //if for some reason we failed to load don't try to div0
    if (texSize.x * texSize.y != 0.f)
    {
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
}

void ShopState::buildScene()
{
    //TODO black background
    auto entity = m_uiScene.createEntity();

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //title box in top right corner
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { 1.f, 1.f };
    entity.getComponent<cro::UIElement>().absolutePosition = -(TitleSize + glm::vec2(BorderPadding));
    auto titleRoot = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, TitleSize.y), TextNormalColour),
            cro::Vertex2D(glm::vec2(0.f), TextNormalColour),
            cro::Vertex2D(TitleSize, TextNormalColour),
            cro::Vertex2D(glm::vec2(TitleSize.x, 0.f), TextNormalColour)
        });
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
    titleRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Shop\nor\nLoadout");
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(CD32::Colours[CD32::Black]);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).absolutePosition = TitleSize / 2.f;
    entity.getComponent<cro::UIElement>().characterSize = UITextSize * 2;
    titleRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto& uiSystem = *m_uiScene.getSystem<cro::UISystem>();
    const auto selectedID = uiSystem.addCallback([&](cro::Entity e) 
        {
            applyButtonTexture(ButtonTexID::Highlighted, e, m_threePatches[ThreePatch::ButtonTop]);
        });

    //Top bar - position node
    // |
    // -s/t s/t s/t s/t s/t s/t
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { 0.f, 1.f };
    entity.getComponent<cro::UIElement>().absolutePosition = { BorderPadding, -BorderPadding };
    auto topBarRoot = entity;

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
            ent.getComponent<cro::Drawable2D>().setTexture(&m_threePatchTexture);
            ent.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
            ent.getComponent<cro::UIElement>().absolutePosition = position;
            ent.getComponent<cro::UIElement>().resizeCallback =
                [&, index, calcBackgroundSize, patch](cro::Entity e)
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
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
                uiSystem.addCallback([&,index](cro::Entity e)
                    {
                        applyButtonTexture(m_selectedCategory == index ? ButtonTexID::Selected : ButtonTexID::Unselected, e, m_threePatches[ThreePatch::ButtonTop]);
                    });
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
                uiSystem.addCallback([&, index](cro::Entity e, const cro::ButtonEvent& evt)
                    {
                        if (activated(evt))
                        {
                            m_scrollNodes[m_selectedCategory].scrollNode.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                            m_scrollNodes[m_selectedCategory].buttonText.getComponent<cro::Text>().setFillColour(ButtonTextColour);
                            applyButtonTexture(ButtonTexID::Unselected, m_scrollNodes[m_selectedCategory].buttonBackground, m_threePatches[ThreePatch::ButtonTop]);

                            m_selectedCategory = index;

                            m_scrollNodes[m_selectedCategory].scrollNode.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                            m_scrollNodes[m_selectedCategory].buttonText.getComponent<cro::Text>().setFillColour(ButtonTextSelectedColour);
                            applyButtonTexture(ButtonTexID::Selected, e, m_threePatches[ThreePatch::ButtonTop]);
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
            ent.getComponent<cro::Text>().setCharacterSize(UITextSize);
            ent.getComponent<cro::Text>().setFillColour(c);
            ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
            ent.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
            ent.getComponent<cro::UIElement>().absolutePosition = position + glm::vec2({ TopButtonSize.x / 2.f, 10.f });
            ent.getComponent<cro::UIElement>().characterSize = UITextSize;
            ent.getComponent<cro::UIElement>().resizeCallback =
                [&, index, calcBackgroundSize](cro::Entity e)
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
        createTopButton({ (TopButtonSize.x + BorderPadding) * i, -(TopButtonSize.y)}, ButtonText[i], i);
    }


    //screen divider - TODO add the scrollbar to this
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { DividerOffset, 0.f };
    entity.getComponent<cro::UIElement>().absolutePosition = { -1.f, BorderPadding };
    auto dividerRoot = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
    entity.getComponent<cro::UIElement>().resizeCallback = 
        [&](cro::Entity e)
        {
            auto size = cro::App::getWindow().getSize().y / cro::UIElementSystem::getViewScale();
            size = std::round(size);
            size -= ((BorderPadding * 4.f) + TopButtonSize.y);

            e.getComponent<cro::Drawable2D>().setVertexData(
                {
                    cro::Vertex2D(glm::vec2(0.f, size), ButtonBackgroundColour),
                    cro::Vertex2D(glm::vec2(0.f), ButtonBackgroundColour),
                    cro::Vertex2D(glm::vec2(2.f, size), ButtonBackgroundColour),
                    cro::Vertex2D(glm::vec2(2.f, 0.f), ButtonBackgroundColour)
                });
        };
    dividerRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    const auto& patch = m_threePatches[ThreePatch::ButtonItem];
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    const auto selectedIDItem = uiSystem.addCallback([&](cro::Entity e)
        {
            applyButtonTexture(ButtonTexID::Highlighted, e, m_threePatches[ThreePatch::ButtonItem]);
        });

    //category list - index is item index within category, NOT into inv::Items array
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
            ent.getComponent<cro::Drawable2D>().setTexture(&m_threePatchTexture);
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
            
            ent.addComponent<cro::UIInput>().area = { 0.f, 0.f, ItemButtonSize.x, ItemButtonSize.y };
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedIDItem;
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
                uiSystem.addCallback([&, index, category](cro::Entity e)
                    {
                        const auto texID = m_scrollNodes[category].selectedItem == index ?
                            ButtonTexID::Selected : ButtonTexID::Unselected;
                        applyButtonTexture(texID, e, m_threePatches[ThreePatch::ButtonItem]);
                    });
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
                uiSystem.addCallback([&, index, category](cro::Entity e, const cro::ButtonEvent& evt)
                    {
                        if (activated(evt))
                        {
                            auto& item = m_scrollNodes[category].items[m_scrollNodes[category].selectedItem];
                            item.buttonText.getComponent<cro::Text>().setFillColour(ButtonTextColour);
                            item.priceText.getComponent<cro::Text>().setFillColour(ButtonPriceColour);
                            applyButtonTexture(ButtonTexID::Unselected, item.buttonBackground, m_threePatches[ThreePatch::ButtonItem]);

                            m_scrollNodes[category].selectedItem = index;

                            auto& newItem = m_scrollNodes[category].items[index];
                            newItem.buttonText.getComponent<cro::Text>().setFillColour(ButtonTextSelectedColour);
                            newItem.priceText.getComponent<cro::Text>().setFillColour(ButtonTextSelectedColour);
                            applyButtonTexture(ButtonTexID::Selected, e, m_threePatches[ThreePatch::ButtonItem]);

                            //LogI << "Item index: " << index << ", Category: " << category << std::endl;
                            //LogI << "Global index: " << newItem.itemIndex << std::endl;
                        }
                    });

            applyButtonTexture(index == 0 ? ButtonTexID::Selected : ButtonTexID::Unselected, ent, patch);
            parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            itemEntry.buttonBackground = ent;

            //TODO we *can* parent the manufacturer icon to the background as it doesn't need a UIElement



            auto text = inv::Manufacturers[item.manufacturer] + "\n" + inv::ItemStrings[item.type];

            ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>();
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Text>(font).setString(text);
            ent.getComponent<cro::Text>().setCharacterSize(UITextSize);
            ent.getComponent<cro::Text>().setVerticalSpacing(3.f);
            ent.getComponent<cro::Text>().setFillColour(index == 0 ? ButtonTextSelectedColour : ButtonTextColour);
            ent.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
            ent.getComponent<cro::UIElement>().absolutePosition = position + glm::vec2({ BorderPadding + 32.f, 26.f });
            ent.getComponent<cro::UIElement>().characterSize = UITextSize;
            parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            itemEntry.buttonText = ent;

            //add the price here - TODO check if owned and set text accordingly, maybe red
            ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>();
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Text>(smallFont).setString(std::to_string(item.price) + " Cr");
            ent.getComponent<cro::Text>().setCharacterSize(UITextSize);
            ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
            ent.getComponent<cro::Text>().setFillColour(index == 0 ? ButtonTextSelectedColour : ButtonPriceColour);
            ent.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
            ent.getComponent<cro::UIElement>().characterSize = InfoTextSize;
            ent.getComponent<cro::UIElement>().resizeCallback =
                [position, calcBackgroundSize](cro::Entity e)
                {
                    const auto width = calcBackgroundSize();
                    const auto textHeight = InfoTextSize + BorderPadding;
                    e.getComponent<cro::UIElement>().absolutePosition = { width - BorderPadding, position.y + textHeight };
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
            auto size = cro::App::getWindow().getSize().y / cro::UIElementSystem::getViewScale();
            size = std::round(size);
            size -= ((BorderPadding * 4.f) + TopButtonSize.y);

            e.getComponent<cro::UIElement>().absolutePosition = { BorderPadding, size };
        };
    auto scrollRoot = entity;

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
                m_scrollNodes[catIndex++].scrollNode = ent;

                scrollNode = ent;
                prevCat = item.type;
                currentPos = glm::vec2(0.f, -(ItemButtonSize.y));
                itemIndex = 0;

                //TODO attach the scroll bar logic to the node
            }
                break;
            }
        }

        //add item to current scroll node
        createItem(scrollNode, currentPos, item, itemIndex, catIndex-1);
        m_scrollNodes[catIndex - 1].items[itemIndex].itemIndex = itemID++;
        
        itemIndex++;
        currentPos.y -= (ItemButtonSize.y + BorderPadding);
    }

    CRO_ASSERT(m_scrollNodes.size() == Category::Count, "");

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
