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

    struct Category final
    {
        enum
        {
            Driver, Wood, Iron, Wedge, Ball,
            Count
        };
    };
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

}

void ShopState::addSystems()
{
    auto& mb = cro::App::getInstance().getMessageBus();

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::UIElementSystem>(mb);
    m_uiScene.addSystem<cro::UISystem>(mb);
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
    const auto createTopButton = [&](glm::vec2 position, const std::string& text)
        {
            //note relative pos has no effect on sprite/text types
            //the abs position is scaled with the window and set
            //relative to the parent
            auto ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>().setPosition(position);
            ent.addComponent<cro::Drawable2D>().setVertexData(
                {
                    cro::Vertex2D(glm::vec2(0.f, TopButtonSize.y), TextNormalColour),
                    cro::Vertex2D(glm::vec2(0.f), TextNormalColour),
                    cro::Vertex2D(TopButtonSize, TextNormalColour),
                    cro::Vertex2D(glm::vec2(TopButtonSize.x, 0.f), TextNormalColour),
                });
            ent.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
            ent.getComponent<cro::UIElement>().absolutePosition = position;
            topBarRoot.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            //can't parent this to the background as it will take on the bg scale
            //which will work, but only if we set this element scaling to false to
            //disable the character size re-scaling (complicated, huh?)
            ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>();
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Text>(font).setString(text);
            ent.getComponent<cro::Text>().setCharacterSize(UITextSize);
            ent.getComponent<cro::Text>().setFillColour(CD32::Colours[CD32::Black]);
            ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
            ent.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
            ent.getComponent<cro::UIElement>().absolutePosition = position + glm::vec2({ TopButtonSize.x / 2.f, 10.f });
            ent.getComponent<cro::UIElement>().characterSize = UITextSize;
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
        createTopButton({ (TopButtonSize.x + BorderPadding) * i, -(TopButtonSize.y + BorderPadding)}, ButtonText[i]);
    }


    //screen divider - TODO add the scrollbar to this
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    entity.getComponent<cro::UIElement>().relativePosition = { 0.5f, 0.f };
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
                    cro::Vertex2D(glm::vec2(0.f, size), TextNormalColour),
                    cro::Vertex2D(glm::vec2(0.f), TextNormalColour),
                    cro::Vertex2D(glm::vec2(2.f, size), TextNormalColour),
                    cro::Vertex2D(glm::vec2(2.f, 0.f), TextNormalColour)
                });
        };
    dividerRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //category list
    const auto createItem = 
        [&](cro::Entity parent, glm::vec2 position, const inv::Item item)
        {
            auto ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>().setPosition(position);
            ent.addComponent<cro::Drawable2D>().setVertexData(
                {
                    cro::Vertex2D(glm::vec2(0.f, ItemButtonSize.y), TextNormalColour),
                    cro::Vertex2D(glm::vec2(0.f), TextNormalColour),
                    cro::Vertex2D(ItemButtonSize, TextNormalColour),
                    cro::Vertex2D(glm::vec2(ItemButtonSize.x, 0.f), TextNormalColour),
                });
            ent.addComponent<cro::UIElement>(cro::UIElement::Sprite, true).depth = SpriteDepth;
            ent.getComponent<cro::UIElement>().absolutePosition = position;
            //TODO add callback to resize width to (screenX/2) - (Border*2) - scrollbarWidth;
            parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            //TODO we *can* parent the manufacturer icon to the background as it doesn't need a UIElement



            auto text = inv::Manufacturers[item.manufacturer] + "\n" + inv::ItemStrings[item.type];

            ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>();
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Text>(font).setString(text);
            ent.getComponent<cro::Text>().setCharacterSize(UITextSize);
            ent.getComponent<cro::Text>().setVerticalSpacing(3.f);
            ent.getComponent<cro::Text>().setFillColour(CD32::Colours[CD32::Black]);
            ent.addComponent<cro::UIElement>(cro::UIElement::Text, true).depth = TextDepth;
            ent.getComponent<cro::UIElement>().absolutePosition = position + glm::vec2({ BorderPadding + 32.f, 26.f });
            ent.getComponent<cro::UIElement>().characterSize = UITextSize;
            parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

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
                std::int32_t catIndex = static_cast<std::int32_t>(m_scrollNodes.size());

                auto ent = m_uiScene.createEntity();
                scrollRoot.getComponent<cro::Transform>().addChild(ent.addComponent<cro::Transform>());
                
                if (m_selectedCategory != catIndex)
                {
                    ent.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }
                m_scrollNodes.push_back(ent);

                scrollNode = ent;
                prevCat = item.type;
                currentPos = glm::vec2(0.f, -(ItemButtonSize.y));


                //TODO attach the scroll bar logic to the node
            }
                break;
            }
        }

        //add item to current scroll node
        createItem(scrollNode, currentPos, item);
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
