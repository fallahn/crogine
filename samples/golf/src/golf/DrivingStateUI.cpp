/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "DrivingState.hpp"
#include "GameConsts.hpp"
#include "MenuConsts.hpp"
#include "CommandIDs.hpp"
#include "TextAnimCallback.hpp"
#include "DrivingRangeDirector.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Maths.hpp>

namespace
{
    constexpr float SummaryOffset = 70.f;
    constexpr float SummaryHeight = 246.f;

    struct MenuID final
    {
        enum
        {
            Dummy,
            Options,
            Summary,

            Count
        };
    };

    //callback data for anim/self destruction
    //of messages / options window
    struct MessageAnim final
    {
        enum
        {
            Delay, Open, Hold, Close
        }state = Delay;
        float currentTime = 0.5f;
    };

    struct MenuCallback final
    {
        const glm::vec2& viewScale;
        cro::UISystem* uiSystem = nullptr;
        std::int32_t menuID = MenuID::Dummy;

        MenuCallback(const glm::vec2& v, cro::UISystem* ui, std::int32_t id)
            : viewScale(v), uiSystem(ui), menuID(id) {}

        void operator ()(cro::Entity e, float dt)
        {
            auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
            auto position = glm::vec3(size.x / 2.f, size.y / 2.f, 1.5f);

            auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<MessageAnim>();
            switch (state)
            {
            default: break;
            case MessageAnim::Delay:
                currTime = std::max(0.f, currTime - dt);
                if (currTime == 0)
                {
                    state = MessageAnim::Open;
                }
                break;
            case MessageAnim::Open:
                //grow
                currTime = std::min(1.f, currTime + (dt * 2.f));
                e.getComponent<cro::Transform>().setPosition(position);
                e.getComponent<cro::Transform>().setScale(glm::vec2(viewScale.x, viewScale.y * cro::Util::Easing::easeOutQuint(currTime)));
                if (currTime == 1)
                {
                    currTime = 0;
                    state = MessageAnim::Hold;

                    //set UI active
                    uiSystem->setActiveGroup(menuID);
                }
                break;
            case MessageAnim::Hold:
            {
                //hold - make sure we stay centred
                e.getComponent<cro::Transform>().setPosition(position);
                e.getComponent<cro::Transform>().setScale(viewScale);
            }
            break;
            case MessageAnim::Close:
                //shrink
                currTime = std::max(0.f, currTime - (dt * 3.f));
                e.getComponent<cro::Transform>().setScale(glm::vec2(viewScale.x * cro::Util::Easing::easeInCubic(currTime), viewScale.y));
                if (currTime == 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    e.getComponent<cro::Transform>().setPosition({ -10000.f, -10000.f });

                    state = MessageAnim::Delay;
                    currTime = 0.75f;
                }
                break;
            }
        }
    };
}

void DrivingState::createUI()
{
    //displays the game scene
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_backgroundTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function =
        [](cro::Entity e, float)
    {
        //this is activated once to make sure the
        //sprite is up to date with any texture buffer resize
        glm::vec2 texSize = e.getComponent<cro::Sprite>().getTexture()->getSize();
        e.getComponent<cro::Sprite>().setTextureRect({ glm::vec2(0.f), texSize });
        e.getComponent<cro::Transform>().setOrigin(texSize / 2.f);
        e.getComponent<cro::Callback>().active = false;
    };
    auto courseEnt = entity;
    createPlayer(courseEnt);
    createBall(); //hmmm should probably be in createScene()?

    //info panel background - vertices are set in resize callback
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    auto infoEnt = entity;

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //player's name
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerName | CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.2f, 0.f };
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<cro::Callback>().setUserData<TextCallbackData>();
    entity.getComponent<cro::Callback>().function = TextAnimCallback();
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //hole distance
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PinDistance | CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //club info
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::ClubName | CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = ClubTextPosition;
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //current turn
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::HoleNumber;
    entity.addComponent<UIElement>().relativePosition = { 0.76f, 1.f };
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //wind strength - this is positioned by the camera's resize callback, below
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::WindString;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto windEnt = entity;

    //wind indicator
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(38.f, 20.f, 0.03f));
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-1.f, 12.f), LeaderboardTextLight),
            cro::Vertex2D(glm::vec2(-1.f, 0.f), LeaderboardTextLight),
            cro::Vertex2D(glm::vec2(1.f, 12.f), LeaderboardTextLight),
            cro::Vertex2D(glm::vec2(1.f, 0.f), LeaderboardTextLight)
        });
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::WindSock;
    entity.addComponent<float>() = 0.f; //current wind direction/rotation
    windEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(38.f, 52.f, 0.01f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::WindIndicator];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.getComponent<cro::Transform>().move(glm::vec2(0.f, -bounds.height));
    windEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //ui is attached to this for relative scaling
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(UIHiddenPosition);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::Root;
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto rootNode = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PowerBar];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //power bar
    auto barEnt = entity;
    auto barCentre = bounds.width / 2.f;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(5.f, 0.f)); //TODO expel the magic number!!
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PowerBarInner];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float)
    {
        auto crop = bounds;
        crop.width *= m_inputParser.getPower();
        e.getComponent<cro::Drawable2D>().setCroppingArea(crop);
    };
    barEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //hook/slice indicator
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(barCentre, 8.f, 0.1f)); //TODO expel the magic number!!
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::HookBar];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, barCentre](cro::Entity e, float)
    {
        glm::vec3 pos(barCentre + (barCentre * m_inputParser.getHook()), 8.f, 0.1f);
        e.getComponent<cro::Transform>().setPosition(pos);
    };
    barEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //camera for mini map - probably doesn't need to be a
    //callback as it's not actually resized.
    auto updateMiniView = [&](cro::Camera& miniCam) mutable
    {
        glm::uvec2 previewSize(RangeSize / 2.f);
        m_mapTexture.create(previewSize.x, previewSize.y);

        miniCam.setOrthographic((-RangeSize.x / 2.f) + 1.f, (RangeSize.x / 2.f) - 1.f, -RangeSize.y / 2.f, RangeSize.y / 2.f, -0.1f, 20.f);
        float xPixel = 1.f / (RangeSize.x / 2.f);
        float yPixel = 1.f / (RangeSize.y / 2.f);
        miniCam.viewport = { xPixel, yPixel, 1.f - (xPixel * 2.f), 1.f - (yPixel * 2.f) };
    };

    m_mapCam = m_gameScene.createEntity();
    m_mapCam.addComponent<cro::Transform>().setPosition({ 0.f, 10.f, 0.f });
    m_mapCam.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    auto& miniCam = m_mapCam.addComponent<cro::Camera>();
    miniCam.renderFlags = RenderFlags::MiniMap;
    miniCam.resizeCallback = updateMiniView;
    updateMiniView(miniCam);

    //minimap view
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 82.f });
    entity.getComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_mapTexture.getTexture());
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MiniMap;
    entity.addComponent<cro::Callback>().setUserData<std::pair<std::int32_t, float>>(0, 1.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [state, scale] = e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>();
        float speed = dt * 4.f;
        float newScale = 0.f;

        if (state == 0)
        {
            //shrinking
            scale = std::max(0.f, scale - speed);
            newScale = cro::Util::Easing::easeOutSine(scale);

            if (scale == 0)
            {
                glm::vec2 offset = glm::vec2(0.f);

                //update render
                updateMinimap();
                e.getComponent<cro::Sprite>().setTexture(m_mapTexture.getTexture());
                auto bounds = e.getComponent<cro::Sprite>().getTextureBounds();
                e.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

                //and set to grow
                state = 1;
            }
        }
        else
        {
            //growing
            scale = std::min(1.f, scale + speed);
            newScale = cro::Util::Easing::easeInSine(scale);

            if (scale == 1)
            {
                //stop callback
                state = 0;
                e.getComponent<cro::Callback>().active = false;
            }
        }
        e.getComponent<cro::Transform>().setScale(glm::vec2(newScale, 1.f));
    };
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto mapEnt = entity;

    //ball icon on mini map
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(PlayerPosition); //actually hides ball off map until ready to be drawn
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), TextNormalColour),
        cro::Vertex2D(glm::vec2(-0.5f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), TextNormalColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MiniBall;
    entity.addComponent<cro::Callback>().setUserData<float>(1.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::max(0.f, currTime - (dt * 3.f));

        static constexpr float MaxScale = 6.f - 1.f;
        float scale = 1.f + (MaxScale * currTime);
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

        float alpha = 1.f - currTime;
        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(alpha);
        }

        if (currTime == 0)
        {
            currTime = 1.f;
            e.getComponent<cro::Callback>().active = false;
        }
    };
    mapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ PlayerPosition.x / 2.f, -PlayerPosition.z / 2.f, 0.01f });
    entity.getComponent<cro::Transform>().move(RangeSize / 4.f);
    auto endColour = TextGoldColour;
    endColour.setAlpha(0.f);
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 18.f), endColour),
        cro::Vertex2D(glm::vec2(-0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.5f, 18.f), endColour),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setRotation(m_inputParser.getYaw());
    };
    mapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //ui viewport is set 1:1 with window, then the scene
    //is scaled to best-fit to maintain pixel accuracy of text.
    auto updateView = [&, rootNode, courseEnt, infoEnt, windEnt, mapEnt](cro::Camera& cam) mutable
    {
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.5f, 2.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();
        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
        auto texSize = glm::vec2(m_backgroundTexture.getSize());

        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -0.1f));
        courseEnt.getComponent<cro::Transform>().setScale(m_viewScale);
        courseEnt.getComponent<cro::Callback>().active = true; //makes sure to delay so updating the texture size is complete first

        //ui layout
        const auto uiSize = size / m_viewScale;

        auto mapSize = RangeSize / 4.f;
        mapEnt.getComponent<cro::Transform>().setPosition({ uiSize.x - mapSize.x - UIBarHeight, uiSize.y - (mapSize.y) - (UIBarHeight * 1.5f) });

        windEnt.getComponent<cro::Transform>().setPosition(glm::vec2(uiSize.x + WindIndicatorPosition.x, WindIndicatorPosition.y));

        //update the overlay
        auto colour = cro::Colour(0.f, 0.f, 0.f, 0.25f);
        infoEnt.getComponent<cro::Drawable2D>().getVertexData() =
        {
            //bottom bar
            cro::Vertex2D(glm::vec2(0.f, UIBarHeight), colour),
            cro::Vertex2D(glm::vec2(0.f), colour),
            cro::Vertex2D(glm::vec2(uiSize.x, UIBarHeight), colour),
            cro::Vertex2D(glm::vec2(uiSize.x, 0.f), colour),
            //degen
            cro::Vertex2D(glm::vec2(uiSize.x, 0.f), cro::Colour::Transparent),
            cro::Vertex2D(glm::vec2(0.f, uiSize.y), cro::Colour::Transparent),
            //top bar
            cro::Vertex2D(glm::vec2(0.f, uiSize.y), colour),
            cro::Vertex2D(glm::vec2(0.f, uiSize.y - UIBarHeight), colour),
            cro::Vertex2D(uiSize, colour),
            cro::Vertex2D(glm::vec2(uiSize.x, uiSize.y - UIBarHeight), colour),
        };
        infoEnt.getComponent<cro::Drawable2D>().updateLocalBounds();
        infoEnt.getComponent<cro::Transform>().setScale(m_viewScale);


        //send command to UIElements and reposition
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::UIElement;
        cmd.action = [&, uiSize](cro::Entity e, float)
        {
            auto pos = e.getComponent<UIElement>().relativePosition;
            pos.x *= uiSize.x;
            pos.x = std::round(pos.x);
            pos.y *= (uiSize.y - UIBarHeight);
            pos.y = std::round(pos.y);
            pos.y += UITextPosV;

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, e.getComponent<UIElement>().depth));
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        //relocate the power bar
        auto uiPos = glm::vec2(uiSize.x / 2.f, UIBarHeight / 2.f);
        rootNode.getComponent<cro::Transform>().setPosition(uiPos);
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());

    createGameOptions();
    createSummary();
}

void DrivingState::createGameOptions()
{
    const auto centreSprite = [](cro::Entity e)
    {
        auto bounds = e.getComponent<cro::Sprite>().getTextureBounds();
        e.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    };

    auto* uiSystem = m_uiScene.getSystem<cro::UISystem>();
    auto buttonSelect = uiSystem->addCallback([](cro::Entity e) { e.getComponent<cro::Sprite>().setColour(cro::Colour::White); });
    auto buttonUnselect = uiSystem->addCallback([](cro::Entity e) { e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); });


    //consumes events when menu not active
    auto dummyEnt = m_uiScene.createEntity();
    dummyEnt.addComponent<cro::Transform>();
    dummyEnt.addComponent<cro::UIInput>();

    //background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/scoreboard.spt", m_resources.textures);
    m_sprites[SpriteID::Stars] = spriteSheet.getSprite("orbs");
    auto bgSprite = spriteSheet.getSprite("border");

    auto bounds = bgSprite.getTextureBounds();
    auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    auto position = glm::vec3(size.x / 2.f, size.y / 2.f, 1.5f);

    auto bgEntity = m_uiScene.createEntity();
    bgEntity.addComponent<cro::Transform>().setPosition(position);
    bgEntity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    bgEntity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    bgEntity.addComponent<cro::Drawable2D>();
    bgEntity.addComponent<cro::Sprite>() = bgSprite;
    bgEntity.addComponent<cro::CommandTarget>().ID = CommandID::UI::DrivingBoard;
    bgEntity.addComponent<cro::Callback>().setUserData<MessageAnim>();
    bgEntity.getComponent<cro::Callback>().function = MenuCallback(m_viewScale, uiSystem, MenuID::Options);
    
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //title
    auto titleText = m_uiScene.createEntity();
    titleText.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 293.f, 0.02f });
    titleText.addComponent<cro::Drawable2D>();
    titleText.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    titleText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    titleText.getComponent<cro::Text>().setString("The Range");
    centreText(titleText);
    bgEntity.getComponent<cro::Transform>().addChild(titleText.getComponent<cro::Transform>());

    //header
    auto headerText = m_uiScene.createEntity();
    headerText.addComponent<cro::Transform>().setPosition({ 14.f, 233.f, 0.02f });
    headerText.addComponent<cro::Drawable2D>();
    headerText.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    headerText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    headerText.getComponent<cro::Text>().setString("How To Play");
    bgEntity.getComponent<cro::Transform>().addChild(headerText.getComponent<cro::Transform>());

    //help text
    auto infoText = m_uiScene.createEntity();
    infoText.addComponent<cro::Transform>().setPosition({ 14.f, 220.f, 0.02f });
    infoText.addComponent<cro::Drawable2D>();
    infoText.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    infoText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    const std::string helpString =
        R"(
Pick the number of shots you wish to take. For each shot you will be given
a new target. Hit the ball as close as possible to the target by selecting
the appropriate club. When all your shots are taken you will be given a
score based on your overall accuracy. Good Luck!
    )";

    infoText.getComponent<cro::Text>().setString(helpString);
    bgEntity.getComponent<cro::Transform>().addChild(infoText.getComponent<cro::Transform>());


    const auto createButton = [&](const std::string& sprite, glm::vec2 position)
    {
        auto buttonEnt = m_uiScene.createEntity();
        buttonEnt.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.4f));
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite(sprite);
        buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        buttonEnt.addComponent<cro::UIInput>().area = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
        buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = buttonSelect;
        buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = buttonUnselect;
        buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Options);

        return buttonEnt;
    };


    //hole count
    auto countEnt = m_uiScene.createEntity();
    countEnt.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 90.f, 0.1f });
    countEnt.addComponent<cro::Drawable2D>();
    countEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("stroke_select");
    auto strokeBounds = spriteSheet.getSprite("stroke_select").getTextureBounds();
    countEnt.getComponent<cro::Transform>().setOrigin({ strokeBounds.width / 2.f, 0.f });
    bgEntity.getComponent<cro::Transform>().addChild(countEnt.getComponent<cro::Transform>());

    auto strokeTextEnt = m_uiScene.createEntity();
    strokeTextEnt.addComponent<cro::Transform>().setPosition({ strokeBounds.width / 2.f, strokeBounds.height + 11.f });
    strokeTextEnt.addComponent<cro::Drawable2D>();
    strokeTextEnt.addComponent<cro::Text>(largeFont).setString("Strokes To Play");
    strokeTextEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    strokeTextEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    centreText(strokeTextEnt);
    countEnt.getComponent<cro::Transform>().addChild(strokeTextEnt.getComponent<cro::Transform>());

    auto numberEnt = m_uiScene.createEntity();
    numberEnt.addComponent<cro::Transform>().setPosition({ strokeBounds.width / 2.f, std::floor(strokeBounds.height / 2.f) + 4.f });
    numberEnt.addComponent<cro::Drawable2D>();
    numberEnt.addComponent<cro::Text>(largeFont);
    numberEnt.getComponent<cro::Text>().setString("5");
    numberEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    numberEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    centreText(numberEnt);
    countEnt.getComponent<cro::Transform>().addChild(numberEnt.getComponent<cro::Transform>());

    auto buttonEnt = createButton("arrow_left", glm::vec2(-3.f, 3.f));
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, numberEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_strokeCountIndex = (m_strokeCountIndex + (m_strokeCounts.size() - 1)) % m_strokeCounts.size();
                    numberEnt.getComponent<cro::Text>().setString(std::to_string(m_strokeCounts[m_strokeCountIndex]));
                    centreText(numberEnt);
                }
            });
    countEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

    buttonEnt = createButton("arrow_right", glm::vec2(35.f, 3.f));
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, numberEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_strokeCountIndex = (m_strokeCountIndex + 1) % m_strokeCounts.size();
                    numberEnt.getComponent<cro::Text>().setString(std::to_string(m_strokeCounts[m_strokeCountIndex]));
                    centreText(numberEnt);
                }
            });
    countEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    //TODO replace this with stat-slot selection
    /*auto playerEnt = m_uiScene.createEntity();
    playerEnt.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 48.f, 0.1f });
    playerEnt.addComponent<cro::Drawable2D>();
    playerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("player_select");
    playerEnt.getComponent<cro::Transform>().setOrigin({ spriteSheet.getSprite("player_select").getTextureBounds().width / 2.f, 0.f });
    bgEntity.getComponent<cro::Transform>().addChild(playerEnt.getComponent<cro::Transform>());

    auto avatarEnt = m_uiScene.createEntity();
    avatarEnt.addComponent<cro::Transform>();
    avatarEnt.addComponent<cro::Drawable2D>();
    avatarEnt.addComponent<cro::Sprite>(m_sharedData.avatarTextures[0][0]);
    avatarEnt.getComponent<cro::Sprite>().setTextureRect(m_sharedData.avatarInfo[0].buns);


    buttonEnt = createButton("arrow_left", glm::vec2(-2.f, 42.f));
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    playerEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

    buttonEnt = createButton("arrow_right", glm::vec2(80.f, 42.f));
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {

                }
            });
    playerEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());*/



    //start button
    auto selectedBounds = spriteSheet.getSprite("start_highlight").getTextureRect();
    auto unselectedBounds = spriteSheet.getSprite("start_button").getTextureRect();
    auto startButton = m_uiScene.createEntity();
    startButton.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 48.f, 0.2f });
    startButton.addComponent<cro::Drawable2D>();
    startButton.addComponent<cro::Sprite>() = spriteSheet.getSprite("start_button");
    startButton.addComponent<cro::UIInput>().setGroup(MenuID::Options);
    startButton.getComponent<cro::UIInput>().area = startButton.getComponent<cro::Sprite>().getTextureBounds();
    startButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem->addCallback(
            [selectedBounds](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setTextureRect(selectedBounds);
                e.getComponent<cro::Transform>().setOrigin({ selectedBounds.width / 2.f, selectedBounds.height / 2.f });
            });
    startButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        uiSystem->addCallback(
            [unselectedBounds](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setTextureRect(unselectedBounds);
                e.getComponent<cro::Transform>().setOrigin({ unselectedBounds.width / 2.f, unselectedBounds.height / 2.f });
            });
    startButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, uiSystem, bgEntity](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                auto& [state, timeout] = bgEntity.getComponent<cro::Callback>().getUserData<MessageAnim>();
                if (state == MessageAnim::Hold
                    && activated(evt))
                {
                    state = MessageAnim::Close;
                    timeout = 1.f;
                    uiSystem->setActiveGroup(MenuID::Dummy);
                    m_gameScene.getDirector<DrivingRangeDirector>()->setHoleCount(m_strokeCounts[m_strokeCountIndex]);

                    setHole(m_gameScene.getDirector<DrivingRangeDirector>()->getCurrentHole());
                }
            });
    centreSprite(startButton);
    bgEntity.getComponent<cro::Transform>().addChild(startButton.getComponent<cro::Transform>());


    //wang this in here so we can debug easier
    /*cro::Command cmd;
    cmd.targetFlags = CommandID::UI::DrivingBoard;
    cmd.action = [](cro::Entity e, float) {e.getComponent<cro::Callback>().active = true; };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);*/
}

void DrivingState::createSummary()
{
    auto* uiSystem = m_uiScene.getSystem<cro::UISystem>();

    //background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/scoreboard.spt", m_resources.textures);
    auto bgSprite = spriteSheet.getSprite("border");

    auto bounds = bgSprite.getTextureBounds();
    auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    auto position = glm::vec3(size.x / 2.f, size.y / 2.f, 1.5f);

    auto bgEntity = m_uiScene.createEntity();
    bgEntity.addComponent<cro::Transform>().setPosition(position);
    bgEntity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    bgEntity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    bgEntity.addComponent<cro::Drawable2D>();
    bgEntity.addComponent<cro::Sprite>() = bgSprite;
    bgEntity.addComponent<cro::Callback>().setUserData<MessageAnim>();
    bgEntity.getComponent<cro::Callback>().function = MenuCallback(m_viewScale, uiSystem, MenuID::Summary);
    

    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //title
    auto titleText = m_uiScene.createEntity();
    titleText.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 293.f, 0.02f });
    titleText.addComponent<cro::Drawable2D>();
    titleText.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    titleText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    titleText.getComponent<cro::Text>().setString("Summary");
    centreText(titleText);
    bgEntity.getComponent<cro::Transform>().addChild(titleText.getComponent<cro::Transform>());


    //info text
    auto infoEnt = m_uiScene.createEntity();
    infoEnt.addComponent<cro::Transform>().setPosition({ SummaryOffset, SummaryHeight, 0.02f });
    infoEnt.addComponent<cro::Drawable2D>();
    infoEnt.addComponent<cro::Text>(smallFont).setString("Sample Text\n1\n1\n1\n1\n1\n1\n1\n1");
    infoEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    infoEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bgEntity.getComponent<cro::Transform>().addChild(infoEnt.getComponent<cro::Transform>());
    m_summaryScreen.text01 = infoEnt;

    infoEnt = m_uiScene.createEntity();
    infoEnt.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + SummaryOffset, SummaryHeight, 0.02f });
    infoEnt.addComponent<cro::Drawable2D>();
    infoEnt.addComponent<cro::Text>(smallFont).setString("Sample Text\n1\n1\n1\n1\n1\n1\n1\n1");
    infoEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    infoEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bgEntity.getComponent<cro::Transform>().addChild(infoEnt.getComponent<cro::Transform>());
    m_summaryScreen.text02 = infoEnt;

    auto summaryEnt = m_uiScene.createEntity();
    summaryEnt.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 120.f, 0.02f });
    summaryEnt.addComponent<cro::Drawable2D>();
    summaryEnt.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    summaryEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bgEntity.getComponent<cro::Transform>().addChild(summaryEnt.getComponent<cro::Transform>());
    m_summaryScreen.summary = summaryEnt;

    //replay text
    auto questionEnt = m_uiScene.createEntity();
    questionEnt.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 72.f, 0.02f });
    questionEnt.addComponent<cro::Drawable2D>();
    questionEnt.addComponent<cro::Text>(largeFont).setString("Play Again?");
    questionEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    questionEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(questionEnt);
    bgEntity.getComponent<cro::Transform>().addChild(questionEnt.getComponent<cro::Transform>());

    const auto centreSprite = [](cro::Entity e)
    {
        auto bounds = e.getComponent<cro::Sprite>().getTextureBounds();
        e.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    };

    auto selectedBounds = spriteSheet.getSprite("yes_highlight").getTextureRect();
    auto unselectedBounds = spriteSheet.getSprite("yes_button").getTextureRect();

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) - 22.f, 48.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("yes_button");
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Summary);
    entity.getComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem->addCallback(
            [selectedBounds](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setTextureRect(selectedBounds);
                e.getComponent<cro::Transform>().setOrigin({ selectedBounds.width / 2.f, selectedBounds.height / 2.f });
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        uiSystem->addCallback(
            [unselectedBounds](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setTextureRect(unselectedBounds);
                e.getComponent<cro::Transform>().setOrigin({ unselectedBounds.width / 2.f, unselectedBounds.height / 2.f });
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, bgEntity](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                auto& [state, timeout] = bgEntity.getComponent<cro::Callback>().getUserData<MessageAnim>();
                if (state == MessageAnim::Hold
                    && activated(evt))
                {
                    state = MessageAnim::Close;
                    timeout = 1.f;

                    cro::Command cmd;
                    cmd.targetFlags = CommandID::UI::DrivingBoard;
                    cmd.action = [](cro::Entity e, float) {e.getComponent<cro::Callback>().active = true; };
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
                }
            });
    centreSprite(entity);
    bgEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    selectedBounds = spriteSheet.getSprite("no_highlight").getTextureRect();
    unselectedBounds = spriteSheet.getSprite("no_button").getTextureRect();

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 22.f, 48.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("no_button");
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Summary);
    entity.getComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem->addCallback(
            [selectedBounds](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setTextureRect(selectedBounds);
                e.getComponent<cro::Transform>().setOrigin({ selectedBounds.width / 2.f, selectedBounds.height / 2.f });
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        uiSystem->addCallback(
            [unselectedBounds](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setTextureRect(unselectedBounds);
                e.getComponent<cro::Transform>().setOrigin({ unselectedBounds.width / 2.f, unselectedBounds.height / 2.f });
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, bgEntity](cro::Entity e, const cro::ButtonEvent& evt)
            {
                auto& [state, timeout] = bgEntity.getComponent<cro::Callback>().getUserData<MessageAnim>();
                if (state == MessageAnim::Hold
                    && activated(evt))
                {
                    requestStackClear();
                    requestStackPush(StateID::Menu);
                }
            });
    centreSprite(entity);
    bgEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_summaryScreen.root = bgEntity;
}

void DrivingState::updateMinimap()
{
    auto oldCam = m_gameScene.setActiveCamera(m_mapCam);

    m_mapTexture.clear(TextNormalColour);
    m_gameScene.render(m_mapTexture);

    auto holePos = m_holeData[m_gameScene.getDirector<DrivingRangeDirector>()->getCurrentHole()].pin / 2.f;
    m_flagQuad.setPosition({ holePos.x, -holePos.z });
    m_flagQuad.move(RangeSize / 4.f);
    m_flagQuad.draw();

    m_mapTexture.display();

    m_gameScene.setActiveCamera(oldCam);
}

void DrivingState::updateWindDisplay(glm::vec3 direction)
{
    float rotation = std::atan2(-direction.z, direction.x);
    static constexpr float CamRotation = cro::Util::Const::PI / 2.f;

    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::WindSock;
    cmd.action = [&, rotation](cro::Entity e, float dt)
    {
        auto r = rotation - CamRotation;

        float& currRotation = e.getComponent<float>();
        currRotation += cro::Util::Maths::shortestRotation(currRotation, r) * dt;
        e.getComponent<cro::Transform>().setRotation(currRotation);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::WindString;
    cmd.action = [direction](cro::Entity e, float dt)
    {
        float knots = direction.y * KnotsPerMetre;
        std::stringstream ss;
        ss.precision(2);
        ss << std::fixed << knots << " knots";
        e.getComponent<cro::Text>().setString(ss.str());

        auto bounds = cro::Text::getLocalBounds(e);
        bounds.width = std::floor(bounds.width / 2.f);
        e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });

        if (knots < 1.5f)
        {
            if (knots < 1)
            {
                e.getComponent<cro::Text>().setFillColour(TextNormalColour);
            }
            else
            {
                e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            }
        }
        else
        {
            e.getComponent<cro::Text>().setFillColour(TextOrangeColour);
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::Flag;
    cmd.action = [rotation](cro::Entity e, float dt)
    {
        float& currRotation = e.getComponent<float>();
        currRotation += cro::Util::Maths::shortestRotation(currRotation, rotation) * dt;
        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, currRotation);
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void DrivingState::showMessage(float range)
{
    const auto* director = m_gameScene.getDirector<DrivingRangeDirector>();
    float score = director->getScore(director->getCurrentStroke() - 1); //this was incremented internally when score was updated

    auto bounds = m_sprites[SpriteID::MessageBoard].getTextureBounds();
    auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    auto position = glm::vec3(size.x / 2.f, size.y / 2.f, 0.05f);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::MessageBoard];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MessageBoard;

    std::int32_t starCount = 0;
    static constexpr float BadScore = 50.f;
    static constexpr float GoodScore = 75.f;
    static constexpr float ExcellentScore = 95.f;

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 56.f, 0.02f });
    textEnt.addComponent<cro::Drawable2D>();
    textEnt.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    textEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    if (score < BadScore)
    {
        textEnt.getComponent<cro::Text>().setString("Bad Luck!");
    }
    else if (score < GoodScore)
    {
        textEnt.getComponent<cro::Text>().setString("Good Effort!");
        starCount = 1;
    }
    else if (score < ExcellentScore)
    {
        textEnt.getComponent<cro::Text>().setString("Not Bad!");
        starCount = 2;
    }
    else
    {
        textEnt.getComponent<cro::Text>().setString("Excellent!");
        starCount = 3;
    }
    centreText(textEnt);
    entity.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());


    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    std::stringstream s1;
    s1.precision(3);
    s1 << range << "m";

    auto textEnt2 = m_uiScene.createEntity();
    textEnt2.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 36.f, 0.02f });
    textEnt2.addComponent<cro::Drawable2D>();
    textEnt2.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    textEnt2.getComponent<cro::Text>().setFillColour(TextNormalColour);
    textEnt2.getComponent<cro::Text>().setString("Range: " + s1.str());
    centreText(textEnt2);
    entity.getComponent<cro::Transform>().addChild(textEnt2.getComponent<cro::Transform>());

    std::stringstream s2;
    s2.precision(3);
    s2 << "Accuracy: " << score << "%";

    auto textEnt3 = m_uiScene.createEntity();
    textEnt3.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 24.f, 0.02f });
    textEnt3.addComponent<cro::Drawable2D>();
    textEnt3.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    textEnt3.getComponent<cro::Text>().setFillColour(TextNormalColour);
    textEnt3.getComponent<cro::Text>().setString(s2.str());
    centreText(textEnt3);
    entity.getComponent<cro::Transform>().addChild(textEnt3.getComponent<cro::Transform>());

    //add mini graphic showing rank
    auto imgEnt = m_uiScene.createEntity();
    imgEnt.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, 0.02f });
    imgEnt.getComponent<cro::Transform>().move(glm::vec2(0.f, 2.f));
    imgEnt.addComponent<cro::Drawable2D>();
    imgEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::Stars];
    imgEnt.addComponent<cro::SpriteAnimation>();
    bounds = imgEnt.getComponent<cro::Sprite>().getTextureBounds();
    imgEnt.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f), 0.f });
    entity.getComponent<cro::Transform>().addChild(imgEnt.getComponent<cro::Transform>());


    //callback for anim/self destruction
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<MessageAnim>();
    entity.getComponent<cro::Callback>().function =
        [&, textEnt, textEnt2, textEnt3, imgEnt, starCount](cro::Entity e, float dt) mutable
    {
        static constexpr float HoldTime = 4.f;
        auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<MessageAnim>();
        switch (state)
        {
        default: break;
        case MessageAnim::Delay:
            currTime = std::max(0.f, currTime - dt);
            if (currTime == 0)
            {
                state = MessageAnim::Open;
            }
            break;
        case MessageAnim::Open:
            //grow
            currTime = std::min(1.f, currTime + (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(glm::vec2(m_viewScale.x, m_viewScale.y * cro::Util::Easing::easeOutQuint(currTime)));
            if (currTime == 1)
            {
                currTime = 0;
                state = MessageAnim::Hold;
                imgEnt.getComponent<cro::SpriteAnimation>().play(starCount);
            }
            break;
        case MessageAnim::Hold:
            //hold
            currTime = std::min(HoldTime, currTime + dt);

            if (currTime > (HoldTime / 2.f))
            {
                //this should be safe to call repeatedly
                setActiveCamera(CameraID::Player);
            }

            if (currTime == HoldTime)
            {
                currTime = 1.f;
                state = MessageAnim::Close;
            }
            break;
        case MessageAnim::Close:
            //shrink
            currTime = std::max(0.f, currTime - (dt * 3.f));
            e.getComponent<cro::Transform>().setScale(glm::vec2(m_viewScale.x * cro::Util::Easing::easeInCubic(currTime), m_viewScale.y));
            if (currTime == 0)
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(textEnt);
                m_uiScene.destroyEntity(textEnt2);
                m_uiScene.destroyEntity(textEnt3);
                m_uiScene.destroyEntity(imgEnt);
                m_uiScene.destroyEntity(e);

                if (m_gameScene.getDirector<DrivingRangeDirector>()->roundEnded())
                {
                    //show summary screen
                    const auto* director = m_gameScene.getDirector<DrivingRangeDirector>();
                    auto scoreCount = director->getTotalStrokes();
                    float totalScore = 0.f;

                    std::string summary;
                    for (auto i = 0; i < std::min(9, scoreCount); ++i)
                    {
                        float score = director->getScore(i);

                        std::stringstream s;
                        s.precision(3);
                        s << "Turn " << i + 1 << ":      " << score << "%\n";
                        summary += s.str();

                        totalScore += score;
                    }
                    m_summaryScreen.text01.getComponent<cro::Text>().setString(summary);
                    summary.clear();

                    //second column
                    if (scoreCount > 9)
                    {
                        for (auto i = 9; i < scoreCount; ++i)
                        {
                            float score = director->getScore(i);

                            std::stringstream s;
                            s.precision(3);
                            s << "Turn " << i + 1 << ":      " << score << "%\n";
                            summary += s.str();

                            totalScore += score;
                        }
                        m_summaryScreen.text02.getComponent<cro::Text>().setString(summary);

                        auto& tx = m_summaryScreen.text01.getComponent<cro::Transform>();
                        auto pos = tx.getPosition();
                        pos.x = SummaryOffset;
                        tx.setPosition(pos);
                        tx.setOrigin({ 0.f, 0.f });
                    }
                    else
                    {
                        auto& tx = m_summaryScreen.text01.getComponent<cro::Transform>();
                        auto pos = tx.getPosition();
                        pos.x = 200.f; //TODO this should be half background width
                        tx.setPosition(pos);
                        centreText(m_summaryScreen.text01);

                        m_summaryScreen.text02.getComponent<cro::Text>().setString(" ");
                    }

                    totalScore /= scoreCount;
                    std::stringstream s;
                    s.precision(3);
                    s << "\nTotal: " << totalScore << "% - ";

                    if (totalScore < BadScore)
                    {
                        s << "Maybe more practice..?";
                    }
                    else if (totalScore < GoodScore)
                    {
                        s << "Could Do Better...";
                    }
                    else if (totalScore < ExcellentScore)
                    {
                        s << "Great Job!";
                    }
                    else
                    {
                        s << "Excellent!";
                    }

                    m_summaryScreen.summary.getComponent<cro::Text>().setString(s.str());
                    centreText(m_summaryScreen.summary);
                    
                    m_summaryScreen.root.getComponent<cro::Callback>().active = true;
                }
                else
                {
                    setHole(m_gameScene.getDirector<DrivingRangeDirector>()->getCurrentHole());
                    //setActiveCamera(CameraID::Green);
                }
            }
            break;
        }
    };
}

void DrivingState::floatingMessage(const std::string& msg)
{
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);

    glm::vec2 size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    glm::vec3 position((size.x / 2.f), (UIBarHeight + 14.f) * m_viewScale.y, 0.2f);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.getComponent<cro::Transform>().setScale(m_viewScale);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(msg);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    centreText(entity);

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, position](cro::Entity e, float dt)
    {
        static constexpr float MaxMove = 40.f;
        static constexpr float MaxTime = 3.f;

        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(MaxTime, currTime + dt);

        float progress = currTime / MaxTime;
        float move = std::floor(cro::Util::Easing::easeOutQuint(progress) * MaxMove);

        auto pos = position;
        pos.y += move;
        e.getComponent<cro::Transform>().setPosition(pos);

        if (currTime == MaxTime)
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }

        float alpha = cro::Util::Easing::easeInCubic(progress);
        auto c = TextNormalColour;
        c.setAlpha(1.f - alpha);
        e.getComponent<cro::Text>().setFillColour(c);
    };
}