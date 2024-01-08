/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "DrivingState.hpp"
#include "GameConsts.hpp"
#include "MenuConsts.hpp"
#include "CommandIDs.hpp"
#include "MessageIDs.hpp"
#include "TextAnimCallback.hpp"
#include "DrivingRangeDirector.hpp"
#include "CloudSystem.hpp"
#include "BallSystem.hpp"
#include "FloatingTextSystem.hpp"
#include "Clubs.hpp"
#include "CallbackData.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>

#include <crogine/audio/AudioScape.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Random.hpp>

#include "../ErrorCheck.hpp"

using namespace cl;

namespace
{
    //used as indices when scrolling through leaderboards
    std::int32_t leaderboardTryCount = 0;
    std::int32_t leaderboardHoleIndex = 0;
    std::int32_t leaderboardFilter = 0;
    constexpr std::int32_t MaxLeaderboardFilter = 3;

    const std::array<std::string, 3u> ScoreStrings =
    {
        "Best Of 5",
        "Best Of 9",
        "Best Of 18",
    };
    const std::array<std::string, 3u> RankStrings =
    {
        "Global",
        "Nearest",
        "Friends"
    };
    const std::array<std::string, 14u> HoleStrings =
    {
        "Random",
        "Target 1",
        "Target 2",
        "Target 3",
        "Target 4",
        "Target 5",
        "Target 6",
        "Target 7",
        "Target 8",
        "Target 9",
        "Target 10",
        "Target 11",
        "Target 12",
        "Target 13",
    };

    static constexpr float SummaryOffset = 54.f;
    static constexpr float SummaryHeight = 254.f;

    static constexpr float BadScore = 50.f;
    static constexpr float GoodScore = 75.f;
    static constexpr float ExcellentScore = 95.f;

    struct MenuCallback final
    {
        const glm::vec2& viewScale;
        cro::UISystem* uiSystem = nullptr;
        std::int32_t menuID = DrivingState::MenuID::Dummy;

        MenuCallback(const glm::vec2& v, cro::UISystem* ui, std::int32_t id)
            : viewScale(v), uiSystem(ui), menuID(id) {}

        void operator ()(cro::Entity e, float dt)
        {
            auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
            auto position = glm::vec3(size.x / 2.f, size.y / 2.f, 1.5f);

            auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<PopupAnim>();
            switch (state)
            {
            default: break;
            case PopupAnim::Delay:
                currTime = std::max(0.f, currTime - dt);
                if (currTime == 0)
                {
                    state = PopupAnim::Open;
                }
                break;
            case PopupAnim::Open:
                //grow
                currTime = std::min(1.f, currTime + (dt * 2.f));
                e.getComponent<cro::Transform>().setPosition(position);
                e.getComponent<cro::Transform>().setScale(glm::vec2(viewScale.x, viewScale.y * cro::Util::Easing::easeOutQuint(currTime)));
                if (currTime == 1)
                {
                    currTime = 0;
                    state = PopupAnim::Hold;

                    //set UI active
                    uiSystem->setActiveGroup(menuID);
                }
                break;
            case PopupAnim::Hold:
            {
                //hold - make sure we stay centred
                e.getComponent<cro::Transform>().setPosition(position);
                e.getComponent<cro::Transform>().setScale(viewScale);
            }
            break;
            case PopupAnim::Close:
                //shrink
                currTime = std::max(0.f, currTime - (dt * 3.f));
                e.getComponent<cro::Transform>().setScale(glm::vec2(viewScale.x * cro::Util::Easing::easeInCubic(currTime), viewScale.y));
                if (currTime == 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    e.getComponent<cro::Transform>().setPosition({ -10000.f, -10000.f });

                    state = PopupAnim::Delay;
                    currTime = 0.75f;
                }
                break;
            }
        }
    };

    struct StarAnimCallback final
    {
        float delay = 0.f;
        float currentTime = 0.f;

        explicit StarAnimCallback(float d) : delay(d), currentTime(d) {}

        void operator() (cro::Entity e, float dt)
        {
            currentTime = std::max(0.f, currentTime - dt);

            if (currentTime == 0)
            {
                currentTime = delay;
                e.getComponent<cro::SpriteAnimation>().play(1);
                e.getComponent<cro::Callback>().active = false;
                e.getComponent<cro::ParticleEmitter>().start();
                e.getComponent<cro::AudioEmitter>().play();
            }
        }
    };

    struct ButtonID final
    {
        enum
        {
            Null = 100,
            CountPrev, CountNext,
            TargetPrev, TargetNext,

            Clubset,
            Leaderboard,
            Begin,

            LBFilterPrev, LBFilterNext,
            LBCountPrev, LBCountNext,
            LBTargetPrev, LBTargetNext,
            LBClose
        };
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
    m_courseEntity = entity;
    createPlayer(courseEnt);
    createBall(); //hmmm should probably be in createScene()?

    //info panel background - vertices are set in resize callback
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    auto infoEnt = entity;
    createSwingMeter(entity);

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


    //fast forward option
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::FastForward | CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -UIBarHeight };
    entity.getComponent<UIElement>().depth = 0.05f;
    entity.addComponent<cro::Callback>().setUserData<SkipCallbackData>();
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& [progress, direction, _] = e.getComponent<cro::Callback>().getUserData<SkipCallbackData>();
        const float Speed = dt * 3.f;
        if (direction == 1)
        {
            //bigger!
            progress = std::min(1.f, progress + Speed);
            if (progress == 1)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        }
        else
        {
            progress = std::max(0.f, progress - Speed);
            if (progress == 0)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        }

        float scale = cro::Util::Easing::easeOutBack(progress);
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
    centreText(entity);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto ffEnt = entity;

    cro::SpriteSheet buttonSprites;
    buttonSprites.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_resources.textures);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 36.f, -12.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = buttonSprites.getSprite("button_a");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&,ffEnt](cro::Entity e, float)
    {
        auto index = ffEnt.getComponent<cro::Callback>().getUserData<SkipCallbackData>().buttonIndex;

        if (cro::GameController::getControllerCount() != 0
            && index != std::numeric_limits<std::uint32_t>::max())
        {
            e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            e.getComponent<cro::SpriteAnimation>().play(index);
        }
        else
        {
            e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
    };
    ffEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    static constexpr glm::vec2 BarSize(30.f, 2.f); //actually half-size
    auto darkColour = LeaderboardTextDark;
    darkColour.setAlpha(0.25f);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-BarSize.x, BarSize.y), TextHighlightColour),
            cro::Vertex2D(-BarSize, TextHighlightColour),

            cro::Vertex2D(glm::vec2(0.f), TextHighlightColour),
            cro::Vertex2D(glm::vec2(0.f), TextHighlightColour),

            cro::Vertex2D(glm::vec2(0.f), darkColour),
            cro::Vertex2D(glm::vec2(0.f), darkColour),

            cro::Vertex2D(BarSize, darkColour),
            cro::Vertex2D(glm::vec2(BarSize.x, -BarSize.y), darkColour),
        });
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLE_STRIP);
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, ffEnt](cro::Entity e, float)
    {
        if (ffEnt.getComponent<cro::Transform>().getScale().x != 0)
        {
            const float xPos = ((BarSize.x * 2.f) * (m_skipState.currentTime / SkipState::SkipTime)) - BarSize.x;
            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            verts[2].position = { xPos, BarSize.y };
            verts[3].position = { xPos, -BarSize.y };
            verts[4].position = { xPos, BarSize.y };
            verts[5].position = { xPos, -BarSize.y };

            e.getComponent<cro::Transform>().setPosition({ ffEnt.getComponent<cro::Transform>().getOrigin().x, -14.f });
        }
    };
    ffEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




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
    entity.addComponent<UIElement>().relativePosition = { 0.75f, 1.f };
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //ball spin indicator - positioned by camera callback
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::SpinBg];
    bounds = m_sprites[SpriteID::SpinBg].getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto spinEnt = entity;

    const auto fgOffset = glm::vec2(spinEnt.getComponent<cro::Transform>().getOrigin());
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(fgOffset);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::SpinFg];
    bounds = m_sprites[SpriteID::SpinFg].getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -0.1f });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = //TODO we could recycle this callback with GolfState to save duplication...
        [&, spinEnt, fgOffset](cro::Entity e, float dt) mutable
    {
        auto& scale = e.getComponent<cro::Callback>().getUserData<float>();
        const float speed = dt * 10.f;
        if (m_inputParser.isSpinputActive())
        {
            scale = std::min(1.f, scale + speed);

            auto size = spinEnt.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

            auto spin = m_inputParser.getSpin();
            if (auto len2 = glm::length2(spin); len2 != 0)
            {
                auto q = glm::rotate(cro::Transform::QUAT_IDENTITY, -spin.y, cro::Transform::X_AXIS);
                q = glm::rotate(q, spin.x, cro::Transform::Y_AXIS);
                auto r = q * cro::Transform::Z_AXIS;

                spin.x = -r.x;
                spin.y = r.y;
            }
            spin *= size;

            e.getComponent<cro::Transform>().setPosition(fgOffset + spin);
        }
        else
        {
            scale = std::max(0.f, scale - speed);
        }
        auto s = scale;
        spinEnt.getComponent<cro::Transform>().setScale({ s,s });
    };

    spinEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(38.f, 30.f, 0.03f));
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(38.f, 62.f, 0.01f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::WindIndicator];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.getComponent<cro::Transform>().move(glm::vec2(0.f, -bounds.height));
    windEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto windDial = entity;

    //text background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -4.f, -10.f, -0.01f });
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, 6.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::WindTextBg];
    windEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //rotating part
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::WindSpeed];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::WindSpeed;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec3(bounds.width / 2.f, bounds.height / 2.f, 0.01f));
    entity.getComponent<cro::Transform>().setPosition(windDial.getComponent<cro::Transform>().getOrigin());
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
    {
        auto speed = e.getComponent<cro::Callback>().getUserData<float>();
        e.getComponent<cro::Transform>().rotate(speed / 6.f);
    };
    windDial.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //wind effect strength
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 64.f, 12.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLE_STRIP);
    entity.getComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 32.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(0.f, 0.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(2.f, 32.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(2.f, 0.f), cro::Colour::White)
        });
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::WindEffect;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<WindCallbackData>(0.f, 0.f);
    entity.getComponent<cro::Callback>().function = WindEffectCallback();
    windEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //ui is attached to this for relative scaling
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(UIHiddenPosition);
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::Root;
    entity.addComponent<cro::Callback>().setUserData<std::pair<std::int32_t, float>>(0, 0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& [dir, currTime] = e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>();

#ifdef USE_GNS
        const float ScaleMultiplier = Social::isSteamdeck() ? 2.f : 1.f;
#else
        const float ScaleMultiplier = 1.f;
#endif

        if (dir == 0)
        {
            //grow
            currTime = std::min(1.f, currTime + dt);
            const float scale = cro::Util::Easing::easeOutElastic(currTime) * ScaleMultiplier;

            e.getComponent<cro::Transform>().setScale({ scale, scale });

            if (currTime == 1)
            {
                dir = 1;
                e.getComponent<cro::Callback>().active = false;
            }
        }
        else
        {
            //shrink
            currTime = std::max(0.f, currTime - (dt * 2.f));
            const float scale = cro::Util::Easing::easeOutBack(currTime) * ScaleMultiplier;

            e.getComponent<cro::Transform>().setScale({ scale, ScaleMultiplier });

            if (currTime == 0)
            {
                dir = 0;
                e.getComponent<cro::Callback>().active = false;
            }
        }

    };

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
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(std::floor(bounds.width / 2.f), bounds.height / 2.f));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, barCentre](cro::Entity e, float)
    {
        glm::vec3 pos(barCentre + (barCentre * m_inputParser.getHook()), 8.f, 0.1f);
        e.getComponent<cro::Transform>().setPosition(pos);
    };
    barEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //camera for mini map
    auto updateMiniView = [&](cro::Camera& miniCam) mutable
    {
        glm::uvec2 previewSize(RangeSize / 2.f);
        m_mapTexture.create(previewSize.x, previewSize.y);

        miniCam.setOrthographic((-RangeSize.x / 2.f) + 1.f, (RangeSize.x / 2.f) - 1.f, -RangeSize.y / 2.f, RangeSize.y / 2.f, -0.1f, 7.f);
        float xPixel = 1.f / (RangeSize.x / 2.f);
        float yPixel = 1.f / (RangeSize.y / 2.f);
        miniCam.viewport = { xPixel, yPixel, 1.f - (xPixel * 2.f), 1.f - (yPixel * 2.f) };
    };

    m_mapCam = m_gameScene.createEntity();
    m_mapCam.addComponent<cro::Transform>().setPosition({ 0.f, 5.f, 0.f });
    m_mapCam.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    auto& miniCam = m_mapCam.addComponent<cro::Camera>();
    miniCam.setRenderFlags(cro::Camera::Pass::Final, RenderFlags::MiniMap);
    //miniCam.resizeCallback = updateMiniView;
    updateMiniView(miniCam);

    //minimap view
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/scoreboard.spt", m_resources.textures);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 82.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("minimap");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto mapEnt = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, std::ceil(bounds.height / 2.f) + 1.f, 0.2f });
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
    mapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto miniEnt = entity;

    mapEnt.addComponent<cro::Callback>().active = true;
    mapEnt.getComponent<cro::Callback>().function =
        [miniEnt](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setScale(miniEnt.getComponent<cro::Transform>().getScale());
    };

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
    miniEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //stroke indicator
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ PlayerPosition.x / 2.f, -PlayerPosition.z / 2.f, 0.01f });
    entity.getComponent<cro::Transform>().move(RangeSize / 4.f);
    auto endColour = TextGoldColour;
    endColour.setAlpha(0.f);
    entity.addComponent<cro::Drawable2D>().getVertexData() = getStrokeIndicatorVerts();
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        e.getComponent<cro::Transform>().setRotation(m_inputParser.getYaw());
        float scale = e.getComponent<cro::Transform>().getScale().x;

        //more magic numbers than Ken Dodd's tax return.
        if (m_inputParser.getActive())
        {
            const auto targetScale = m_inputParser.getEstimatedDistance();
            if (scale < targetScale)
            {
                scale = std::min(scale + (dt * ((targetScale - scale) * 10.f)), targetScale);
            }
            else
            {
                scale = std::max(targetScale, scale - ((scale * dt) * 2.f));
            }
        }
        else
        {
            scale = std::max(0.f, scale - ((scale * dt) * 8.f));
        }
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale, 1.f));
    };
    miniEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //ui viewport is set 1:1 with window, then the scene
    //is scaled to best-fit to maintain pixel accuracy of text.
    auto updateView = [&, rootNode, courseEnt, infoEnt, spinEnt, windEnt, mapEnt](cro::Camera& cam) mutable
    {
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.5f, 2.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        m_inputParser.setMouseScale(m_viewScale.x);

        glm::vec2 courseScale(m_sharedData.pixelScale ? m_viewScale.x : 1.f);

        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -0.1f));
        courseEnt.getComponent<cro::Transform>().setScale(courseScale);
        courseEnt.getComponent<cro::Callback>().active = true; //makes sure to delay so updating the texture size is complete first

        //ui layout
        const auto uiSize = size / m_viewScale;

        auto mapSize = RangeSize / 4.f;
        mapEnt.getComponent<cro::Transform>().setPosition({ uiSize.x - mapSize.x - UIBarHeight, uiSize.y - (mapSize.y) - (UIBarHeight * 1.5f) });

        windEnt.getComponent<cro::Transform>().setPosition(glm::vec2(/*uiSize.x +*/ WindIndicatorPosition.x, WindIndicatorPosition.y - UIBarHeight));
        spinEnt.getComponent<cro::Transform>().setPosition(glm::vec2(std::floor(uiSize.x / 2.f), 32.f));

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

            pos += e.getComponent<UIElement>().absolutePosition;

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, e.getComponent<UIElement>().depth));
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        //relocate the power bar
        auto uiPos = glm::vec2(uiSize.x / 2.f, UIBarHeight / 2.f);
#ifdef USE_GNS
        if (Social::isSteamdeck())
        {
            uiPos.y *= 2.f;
            spinEnt.getComponent<cro::Transform>().move({ 0.f, 32.f, 0.f });
        }
#endif
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

void DrivingState::createSwingMeter(cro::Entity root)
{
    static constexpr float Width = 4.f;
    static constexpr float Height = 40.f;
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-Width, -Height), SwingputDark),
            cro::Vertex2D(glm::vec2(Width,  -Height), SwingputDark),
            cro::Vertex2D(glm::vec2(-Width,  -0.5f), SwingputDark),
            cro::Vertex2D(glm::vec2(Width,  -0.5f), SwingputDark),

            cro::Vertex2D(glm::vec2(-Width,  -0.5f), TextNormalColour),
            cro::Vertex2D(glm::vec2(Width,  -0.5f), TextNormalColour),
            cro::Vertex2D(glm::vec2(-Width,  0.5f), TextNormalColour),
            cro::Vertex2D(glm::vec2(Width,  0.5f), TextNormalColour),

            cro::Vertex2D(glm::vec2(-Width,  0.5f), SwingputDark),
            cro::Vertex2D(glm::vec2(Width,  0.5f), SwingputDark),
            cro::Vertex2D(glm::vec2(-Width, Height), SwingputDark),
            cro::Vertex2D(glm::vec2(Width,  Height), SwingputDark),


            cro::Vertex2D(glm::vec2(-Width, -Height), SwingputLight),
            cro::Vertex2D(glm::vec2(Width,  -Height), SwingputLight),
            cro::Vertex2D(glm::vec2(-Width, 0.f), SwingputLight),
            cro::Vertex2D(glm::vec2(Width,  0.f), SwingputLight),
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        float height = verts[14].position.y;
        float targetAlpha = -0.01f;

        if (m_inputParser.isSwingputActive())
        {
            height = m_inputParser.getSwingputPosition() * ((Height * 2.f) / MaxSwingputDistance);
            targetAlpha = 1.f;
        }

        auto& currentAlpha = e.getComponent<cro::Callback>().getUserData<float>();
        const float InSpeed = dt * 6.f;
        const float OutSpeed = m_inputParser.getPower() < 0.5 ? InSpeed : dt * 0.5f;
        if (currentAlpha <= targetAlpha)
        {
            currentAlpha = std::min(1.f, currentAlpha + InSpeed);
        }
        else
        {
            currentAlpha = std::max(0.f, currentAlpha - OutSpeed);
        }

        for (auto& v : verts)
        {
            v.colour.setAlpha(currentAlpha);
        }
        verts[14].position.y = height;
        verts[15].position.y = height;
    };

    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    entity.getComponent<UIElement>().absolutePosition = { -10.f, 50.f };

    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void DrivingState::createGameOptions()
{
    const auto centreSprite = [](cro::Entity e)
    {
        auto bounds = e.getComponent<cro::Sprite>().getTextureBounds();
        e.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    };

    auto* uiSystem = m_uiScene.getSystem<cro::UISystem>();
    auto buttonSelect = uiSystem->addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto buttonUnselect = uiSystem->addCallback([](cro::Entity e) { e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); });


    //consumes events when menu not active
    auto dummyEnt = m_uiScene.createEntity();
    dummyEnt.addComponent<cro::Transform>();
    dummyEnt.addComponent<cro::UIInput>();

    cro::AudioScape as;
    as.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);


    //background
    auto& tex = m_resources.textures.get("assets/golf/images/driving_range_menu.png");

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/scoreboard.spt", m_resources.textures);
    m_sprites[SpriteID::Stars] = spriteSheet.getSprite("orbs");

    auto bounds = cro::FloatRect(glm::vec2(0.f), glm::vec2(tex.getSize()));
    auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    auto position = glm::vec3(size.x / 2.f, size.y / 2.f, 1.5f);

    auto bgEntity = m_uiScene.createEntity();
    bgEntity.addComponent<cro::Transform>().setPosition(position);
    bgEntity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    bgEntity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    bgEntity.addComponent<cro::Drawable2D>();
    bgEntity.addComponent<cro::Sprite>(tex);// = bgSprite;
    bgEntity.addComponent<cro::CommandTarget>().ID = CommandID::UI::DrivingBoard;
    bgEntity.addComponent<cro::Callback>().setUserData<PopupAnim>();
    bgEntity.getComponent<cro::Callback>().function = MenuCallback(m_viewScale, uiSystem, MenuID::Options);

    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& labelFont = m_sharedData.sharedResources->fonts.get(FontID::Label);

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
    headerText.addComponent<cro::Transform>().setPosition({ 25.f, 248.f, 0.02f });
    headerText.addComponent<cro::Drawable2D>();
    headerText.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    headerText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    headerText.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    headerText.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    headerText.getComponent<cro::Text>().setString("How To Play");
    bgEntity.getComponent<cro::Transform>().addChild(headerText.getComponent<cro::Transform>());

    //help text
    auto infoText = m_uiScene.createEntity();
    infoText.addComponent<cro::Transform>().setPosition({ 25.f, 237.f, 0.02f });
    infoText.addComponent<cro::Drawable2D>();
    infoText.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    infoText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    infoText.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    infoText.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    const std::string helpString =
        R"(
Pick the number of strokes you wish to take. Hit the ball as close as possible to the
target by selecting the appropriate club. When all of your strokes are taken you
will be given a score based on your overall accuracy. Good Luck!
    )";

    infoText.getComponent<cro::Text>().setString(helpString);
    bgEntity.getComponent<cro::Transform>().addChild(infoText.getComponent<cro::Transform>());


    const auto createButton = [&](const std::string& sprite, glm::vec2 position, std::int32_t selectionIndex)
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
        buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(selectionIndex);
        buttonEnt.addComponent<cro::AudioEmitter>() = as.getEmitter("switch");

        return buttonEnt;
    };


    //current record holder
    auto recordEnt = m_uiScene.createEntity();
    recordEnt.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 270.f, 0.1f });
    recordEnt.addComponent<cro::Drawable2D>();
    recordEnt.addComponent<cro::Text>(labelFont).setString(Social::getDrivingLeader(m_targetIndex, m_strokeCountIndex));
    recordEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    recordEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    recordEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    recordEnt.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    centreText(recordEnt);
    bgEntity.getComponent<cro::Transform>().addChild(recordEnt.getComponent<cro::Transform>());


    //hole count
    auto countEnt = m_uiScene.createEntity();
    countEnt.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f) - 42.f, 134.f, 0.1f });
    countEnt.addComponent<cro::Drawable2D>();
    countEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("stroke_select");
    auto strokeBounds = spriteSheet.getSprite("stroke_select").getTextureBounds();
    countEnt.getComponent<cro::Transform>().setOrigin({ strokeBounds.width / 2.f, 0.f });
    bgEntity.getComponent<cro::Transform>().addChild(countEnt.getComponent<cro::Transform>());

    auto strokeTextEnt = m_uiScene.createEntity();
    strokeTextEnt.addComponent<cro::Transform>().setPosition({ strokeBounds.width / 2.f, strokeBounds.height + 22.f });
    strokeTextEnt.addComponent<cro::Drawable2D>();
    strokeTextEnt.addComponent<cro::Text>(largeFont).setString("Strokes\nTo Play");
    strokeTextEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    strokeTextEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    strokeTextEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    //strokeTextEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    strokeTextEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    strokeTextEnt.getComponent<cro::Text>().setVerticalSpacing(2.f);
    centreText(strokeTextEnt);
    countEnt.getComponent<cro::Transform>().addChild(strokeTextEnt.getComponent<cro::Transform>());

    auto numberEnt = m_uiScene.createEntity();
    numberEnt.addComponent<cro::Transform>().setPosition({ strokeBounds.width / 2.f, std::floor(strokeBounds.height / 2.f) + 4.f, 0.02f });
    numberEnt.addComponent<cro::Drawable2D>();
    numberEnt.addComponent<cro::Text>(largeFont);
    numberEnt.getComponent<cro::Text>().setString("5");
    numberEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    numberEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    centreText(numberEnt);
    countEnt.getComponent<cro::Transform>().addChild(numberEnt.getComponent<cro::Transform>());


    //high score text
    auto textEnt4 = m_uiScene.createEntity();
    textEnt4.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), 123.f, 0.02f });
    textEnt4.addComponent<cro::Drawable2D>();
    textEnt4.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    textEnt4.getComponent<cro::Text>().setFillColour(TextNormalColour);
    textEnt4.addComponent<cro::Callback>().active = true;
    textEnt4.getComponent<cro::Callback>().function = //make sure to update with current high score
        [&, bgEntity](cro::Entity e, float)
    {
        const auto& data = bgEntity.getComponent<cro::Callback>().getUserData<PopupAnim>();
        if (data.state == PopupAnim::Open)
        {
            if (m_topScores[m_strokeCountIndex] > 0)
            {
                std::stringstream s;
                s.precision(3);
                s << "Personal Best: " << m_topScores[m_strokeCountIndex] << "%";

                e.getComponent<cro::Text>().setString(s.str());
            }
            else
            {
                e.getComponent<cro::Text>().setString("No Score");
            }
            centreText(e);
        }
    };
    bgEntity.getComponent<cro::Transform>().addChild(textEnt4.getComponent<cro::Transform>());

    cro::Entity tickerEnt;
#ifdef USE_GNS
    //Top 5 ticker
    for (auto i = 0u; i < m_tickerStrings.size(); ++i)
    {
        m_tickerStrings[i] = Social::getDrivingTopFive(i);
    }

    tickerEnt = m_uiScene.createEntity();
    tickerEnt.addComponent<cro::Transform>().setPosition({ 100.f, 0.f, 0.2f });
    tickerEnt.addComponent<cro::Drawable2D>();
    tickerEnt.addComponent<cro::Text>(labelFont).setString(m_tickerStrings[0]);
    tickerEnt.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    tickerEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    tickerEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    tickerEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    tickerEnt.addComponent<cro::Callback>().active = true;
    tickerEnt.getComponent<cro::Callback>().function =
        [&, bounds, bgEntity](cro::Entity e, float dt)
    {
        if (bgEntity.getComponent<cro::Transform>().getScale().x != 0)
        {
            auto scrollBounds = cro::Text::getLocalBounds(e);

            auto pos = e.getComponent<cro::Transform>().getPosition();

            pos.x -= 20.f * dt;
            pos.y = 27.f;
            pos.z = 0.3f;

            static constexpr float Offset = 6.f;
            const auto bgWidth = bounds.width;
            if (pos.x < -scrollBounds.width + Offset)
            {
                pos.x = bgWidth;
                pos.x -= Offset;
            }

            e.getComponent<cro::Transform>().setPosition(pos);

            cro::FloatRect cropping = { -pos.x + Offset, -16.f, (bgWidth)-(Offset * 2.f), 18.f };
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
        }
    };
    bgEntity.getComponent<cro::Transform>().addChild(tickerEnt.getComponent<cro::Transform>());

#endif

    //hole count buttons
    auto buttonEnt = createButton("arrow_left", glm::vec2(-3.f, 3.f), ButtonID::CountPrev);
    if (Social::getClubLevel())
    {
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::CountNext, ButtonID::Clubset);
    }
    else
    {
#ifdef USE_GNS
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::CountNext, ButtonID::Leaderboard);
#else
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::CountNext, ButtonID::Begin);
#endif
    }
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(ButtonID::TargetNext, ButtonID::Begin);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, numberEnt, textEnt4, tickerEnt, recordEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_strokeCountIndex = (m_strokeCountIndex + (m_strokeCounts.size() - 1)) % m_strokeCounts.size();
                    numberEnt.getComponent<cro::Text>().setString(std::to_string(m_strokeCounts[m_strokeCountIndex]));
                    centreText(numberEnt);

                    leaderboardTryCount = m_strokeCountIndex;

                    if (m_topScores[m_strokeCountIndex] > 0)
                    {
                        std::stringstream s;
                        s.precision(3);
                        s << "Personal Best: " << m_topScores[m_strokeCountIndex] << "%";

                        textEnt4.getComponent<cro::Text>().setString(s.str());
                    }
                    else
                    {
                        textEnt4.getComponent<cro::Text>().setString("No Score");
                    }
                    centreText(textEnt4);

#ifdef USE_GNS
                    tickerEnt.getComponent<cro::Text>().setString(m_tickerStrings[m_strokeCountIndex]);
                    auto pos = tickerEnt.getComponent<cro::Transform>().getPosition();
                    pos.x = 300.f;
                    tickerEnt.getComponent<cro::Transform>().setPosition(pos);
                    tickerEnt.getComponent<cro::Callback>().function(tickerEnt, 0.f); //updates the cropping

                    recordEnt.getComponent<cro::Text>().setString(Social::getDrivingLeader(m_targetIndex, m_strokeCountIndex));
                    centreText(recordEnt);
#endif

                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
                }
            });
    countEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

    buttonEnt = createButton("arrow_right", glm::vec2(35.f, 3.f), ButtonID::CountNext);
    if (Social::getClubLevel())
    {
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::TargetPrev, ButtonID::Clubset);
    }
    else
    {
#ifdef USE_GNS
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::TargetPrev, ButtonID::Leaderboard);
#else
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::TargetPrev, ButtonID::Begin);
#endif
    }
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(ButtonID::CountPrev, ButtonID::Begin);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, numberEnt, textEnt4, tickerEnt, recordEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_strokeCountIndex = (m_strokeCountIndex + 1) % m_strokeCounts.size();
                    numberEnt.getComponent<cro::Text>().setString(std::to_string(m_strokeCounts[m_strokeCountIndex]));
                    centreText(numberEnt);

                    leaderboardTryCount = m_strokeCountIndex;

                    if (m_topScores[m_strokeCountIndex] > 0)
                    {
                        std::stringstream s;
                        s.precision(3);
                        s << "Personal Best: " << m_topScores[m_strokeCountIndex] << "%";

                        textEnt4.getComponent<cro::Text>().setString(s.str());
                    }
                    else
                    {
                        textEnt4.getComponent<cro::Text>().setString("No Score");
                    }
                    centreText(textEnt4);

#ifdef USE_GNS
                    tickerEnt.getComponent<cro::Text>().setString(m_tickerStrings[m_strokeCountIndex]);
                    auto pos = tickerEnt.getComponent<cro::Transform>().getPosition();
                    pos.x = 300.f;
                    tickerEnt.getComponent<cro::Transform>().setPosition(pos);
                    tickerEnt.getComponent<cro::Callback>().function(tickerEnt, 0.f); //updates the cropping

                    recordEnt.getComponent<cro::Text>().setString(Social::getDrivingLeader(m_targetIndex, m_strokeCountIndex));
                    centreText(recordEnt);
#endif

                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
                }
            });
    countEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());



    //minimap for targets (delay before rendering)
    auto renderEnt = m_uiScene.createEntity();
    renderEnt.addComponent<cro::Callback>().active = true;
    renderEnt.getComponent<cro::Callback>().setUserData<float>(1.f);
    renderEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;

        if (currTime < 0)
        {
            auto oldCam = m_gameScene.setActiveCamera(m_mapCam);
            m_mapTexture.clear(TextNormalColour);
            m_gameScene.render();
            m_mapTexture.display();
            m_gameScene.setActiveCamera(oldCam);

            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }
    };

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 5.f, 120.f, 0.3f });
    entity.getComponent<cro::Transform>().setOrigin(RangeSize / 4.f);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_mapTexture.getTexture());
    bgEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto mapEnt = entity;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(mapEnt.getComponent<cro::Transform>().getOrigin());
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("minimap");
    auto border = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ border.width / 2.f, border.height / 2.f, 0.1f });
    mapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    cro::SpriteSheet flagSheet;
    flagSheet.loadFromFile("assets/golf/sprites/ui.spt", m_resources.textures);
    auto flagEnt = m_uiScene.createEntity();
    flagEnt.addComponent<cro::Transform>().setOrigin({ 0.f, 0.f, -0.1f });
    flagEnt.addComponent<cro::Drawable2D>();
    flagEnt.addComponent<cro::Sprite>() = flagSheet.getSprite("flag03");

    flagEnt.addComponent<cro::Callback>().active = !m_holeData.empty();
    flagEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
    flagEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        static constexpr glm::vec2 Offset(RangeSize / 4.f);
        glm::vec2 flagPos(0.f);

        if (m_targetIndex == 0)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime -= dt;

            if (currTime < 0.f)
            {
                currTime += 0.5f;

                static std::size_t idx = 0;
                idx = (idx + cro::Util::Random::value(1, 3)) % m_holeData.size();

                auto pos = m_holeData[idx].pin / 2.f;
                flagPos = { pos.x, -pos.z };

                flagPos += Offset;
                e.getComponent<cro::Transform>().setPosition(flagPos);
            }
        }
        else
        {
            auto pos = m_holeData[m_targetIndex - 1].pin / 2.f;
            flagPos = { pos.x, -pos.z };

            flagPos += Offset;
            e.getComponent<cro::Transform>().setPosition(flagPos);
        }
    };

    mapEnt.getComponent<cro::Transform>().addChild(flagEnt.getComponent<cro::Transform>());


    //target select
    countEnt = m_uiScene.createEntity();
    countEnt.addComponent<cro::Transform>().setPosition({ std::floor((bounds.width / 2.f) + 42.f), 134.f, 0.1f });
    countEnt.addComponent<cro::Drawable2D>();
    countEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("stroke_select");
    strokeBounds = spriteSheet.getSprite("stroke_select").getTextureBounds();
    countEnt.getComponent<cro::Transform>().setOrigin({ strokeBounds.width / 2.f, 0.f });
    bgEntity.getComponent<cro::Transform>().addChild(countEnt.getComponent<cro::Transform>());

    strokeTextEnt = m_uiScene.createEntity();
    strokeTextEnt.addComponent<cro::Transform>().setPosition({ strokeBounds.width / 2.f, strokeBounds.height + 22.f });
    strokeTextEnt.addComponent<cro::Drawable2D>();
    strokeTextEnt.addComponent<cro::Text>(largeFont).setString("Select\nTarget");
    strokeTextEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    strokeTextEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    strokeTextEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    //strokeTextEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    strokeTextEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    strokeTextEnt.getComponent<cro::Text>().setVerticalSpacing(2.f);
    centreText(strokeTextEnt);
    countEnt.getComponent<cro::Transform>().addChild(strokeTextEnt.getComponent<cro::Transform>());

    numberEnt = m_uiScene.createEntity();
    numberEnt.addComponent<cro::Transform>().setPosition({ strokeBounds.width / 2.f, std::floor(strokeBounds.height / 2.f) + 4.f, 0.02f });
    numberEnt.addComponent<cro::Drawable2D>();
    numberEnt.addComponent<cro::Text>(largeFont);
    numberEnt.getComponent<cro::Text>().setString("?");
    numberEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    numberEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    centreText(numberEnt);
    countEnt.getComponent<cro::Transform>().addChild(numberEnt.getComponent<cro::Transform>());


    //target select buttons
    buttonEnt = createButton("arrow_left", glm::vec2(-3.f, 3.f), ButtonID::TargetPrev);
    if (Social::getClubLevel())
    {
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::TargetNext, ButtonID::Clubset);
    }
    else
    {
#ifdef USE_GNS
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::TargetNext, ButtonID::Leaderboard);
#else
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::TargetNext, ButtonID::Begin);
#endif
    }
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(ButtonID::CountNext, ButtonID::Begin);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, numberEnt, recordEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_targetIndex = static_cast<std::int32_t>((m_targetIndex + m_holeData.size()) % (m_holeData.size() + 1));
                    std::string str = (m_targetIndex - 1) < 0 ? "?" : std::to_string(m_targetIndex);
                    numberEnt.getComponent<cro::Text>().setString(str);
                    centreText(numberEnt);

                    leaderboardHoleIndex = m_targetIndex;

                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
#ifdef USE_GNS
                    recordEnt.getComponent<cro::Text>().setString(Social::getDrivingLeader(m_targetIndex, m_strokeCountIndex));
                    centreText(recordEnt);
#endif
                }
            });
    countEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

    buttonEnt = createButton("arrow_right", glm::vec2(35.f, 3.f), ButtonID::TargetNext);
    if (Social::getClubLevel())
    {
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::CountPrev, ButtonID::Clubset);
    }
    else
    {
#ifdef USE_GNS
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::CountPrev, ButtonID::Leaderboard);
#else
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(ButtonID::CountPrev, ButtonID::Begin);
#endif
    }
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(ButtonID::TargetPrev, ButtonID::Begin);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, numberEnt, recordEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_targetIndex = (m_targetIndex + 1) % (m_holeData.size() + 1);
                    std::string str = (m_targetIndex - 1) < 0 ? "?" : std::to_string(m_targetIndex);
                    numberEnt.getComponent<cro::Text>().setString(str);
                    centreText(numberEnt);

                    leaderboardHoleIndex = m_targetIndex;

                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();

#ifdef USE_GNS
                    recordEnt.getComponent<cro::Text>().setString(Social::getDrivingLeader(m_targetIndex, m_strokeCountIndex));
                    centreText(recordEnt);
#endif
                }
            });
    countEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    //clubset select
    auto uBounds = spriteSheet.getSprite("leaderboard_button").getTextureRect();
    auto sBounds = spriteSheet.getSprite("leaderboard_highlight").getTextureRect();
    const auto lbuttonSelected = uiSystem->addCallback(
        [sBounds](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setTextureRect(sBounds);
            e.getComponent<cro::AudioEmitter>().play();
        });
    const auto lbuttonUnselected = uiSystem->addCallback(
        [uBounds](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setTextureRect(uBounds);
        });

    if (Social::getClubLevel())
    {
        auto labelEnt = m_uiScene.createEntity();
        labelEnt.addComponent<cro::Transform>().setPosition({ std::floor(sBounds.width / 2.f), 13.f, 0.1f });
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Text>(smallFont).setFillColour(TextNormalColour);
        labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        switch (m_sharedData.clubSet)
        {
        default:
        case 0:
            labelEnt.getComponent<cro::Text>().setString("Clubset: Novice");
            break;
        case 1:
            labelEnt.getComponent<cro::Text>().setString("Clubset: Expert");
            break;
        case 2:
            labelEnt.getComponent<cro::Text>().setString("Clubset: Pro");
            break;
        }
        centreText(labelEnt);

        textEnt4 = m_uiScene.createEntity();
        textEnt4.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 108.f, 0.02f });
        textEnt4.addComponent<cro::Drawable2D>();
        textEnt4.addComponent<cro::Sprite>() = spriteSheet.getSprite("leaderboard_button");

        auto buttonBounds = textEnt4.getComponent<cro::Sprite>().getTextureBounds();
        textEnt4.getComponent<cro::Transform>().setOrigin({ std::floor(buttonBounds.width / 2.f), buttonBounds.height });
        textEnt4.addComponent<cro::AudioEmitter>() = as.getEmitter("switch");
        textEnt4.addComponent<cro::UIInput>().setGroup(MenuID::Options);
        textEnt4.getComponent<cro::UIInput>().area = buttonBounds;
        textEnt4.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::Clubset);
#ifdef USE_GNS
        textEnt4.getComponent<cro::UIInput>().setNextIndex(ButtonID::Leaderboard, ButtonID::Leaderboard);
#else
        textEnt4.getComponent<cro::UIInput>().setNextIndex(ButtonID::Begin, ButtonID::Begin);
#endif
        textEnt4.getComponent<cro::UIInput>().setPrevIndex(ButtonID::CountPrev, ButtonID::CountPrev);
        textEnt4.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = lbuttonSelected;
        textEnt4.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = lbuttonUnselected;
        textEnt4.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem->addCallback(
                [&, bgEntity, labelEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
                {
                    const auto& [state, _] = bgEntity.getComponent<cro::Callback>().getUserData<PopupAnim>();
                    if (state == PopupAnim::Hold
                        && activated(evt))
                    {
                        m_sharedData.clubSet = (m_sharedData.clubSet + 1) % (Social::getClubLevel() + 1);
                        switch (m_sharedData.clubSet)
                        {
                        default:
                        case 0:
                            labelEnt.getComponent<cro::Text>().setString("Clubset: Novice");
                            break;
                        case 1:
                            labelEnt.getComponent<cro::Text>().setString("Clubset: Expert");
                            break;
                        case 2:
                            labelEnt.getComponent<cro::Text>().setString("Clubset: Pro");
                            break;
                        }
                    }
                    Club::setClubLevel(m_sharedData.clubSet);
                    centreText(labelEnt);
                });

        bgEntity.getComponent<cro::Transform>().addChild(textEnt4.getComponent<cro::Transform>());
        textEnt4.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
    }



#ifdef USE_GNS

    cro::SpriteSheet leaderSheet;
    leaderSheet.loadFromFile("assets/golf/sprites/driving_leaderboard.spt", m_resources.textures);

    struct LeaderboardData final
    {
        float progress = 0.f;
        int direction = 0;
    };

    auto bgBounds = bgEntity.getComponent<cro::Sprite>().getTextureBounds();

    //scoreboard window
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 13.f, 0.8f });
    entity.getComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = leaderSheet.getSprite("board");
    auto b = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.getComponent<cro::Transform>().setOrigin({ b.width / 2.f, b.height / 2.f });
    entity.getComponent<cro::Transform>().move({ std::floor(bgBounds.width / 2.f), b.height / 2.f });
    entity.addComponent<cro::Callback>().setUserData<LeaderboardData>();
    //callback is added below so we can captch upDisplay lambda
    bgEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto lbEntity = entity;
    m_leaderboardEntity = entity;

    auto textSelected = uiSystem->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextHighlightColour);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto textUnselected = uiSystem->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
        });


    //close leaderboards
    textEnt4 = m_uiScene.createEntity();
    textEnt4.addComponent<cro::Transform>().setPosition({ 442.f, 11.f, 0.12f });
    textEnt4.addComponent<cro::Drawable2D>();
    textEnt4.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    textEnt4.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    textEnt4.getComponent<cro::Text>().setString("Back");
    auto textBounds = cro::Text::getLocalBounds(textEnt4);
    textEnt4.addComponent<cro::AudioEmitter>() = as.getEmitter("switch");
    textEnt4.addComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    textEnt4.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::LBClose);
    textEnt4.getComponent<cro::UIInput>().setNextIndex(ButtonID::LBCountPrev, ButtonID::LBFilterNext);
    textEnt4.getComponent<cro::UIInput>().setPrevIndex(ButtonID::LBTargetNext, ButtonID::LBFilterNext);
    textEnt4.getComponent<cro::UIInput>().area = textBounds;
    textEnt4.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = textSelected;
    textEnt4.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = textUnselected;
    textEnt4.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, uiSystem, lbEntity](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    uiSystem->setActiveGroup(MenuID::Dummy);
                    lbEntity.getComponent<cro::Callback>().active = true;

                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
                }
            });
    lbEntity.getComponent<cro::Transform>().addChild(textEnt4.getComponent<cro::Transform>());

    //name column
    textEnt4 = m_uiScene.createEntity();
    textEnt4.addComponent<cro::Transform>().setPosition({ 94.f, 234.f, 0.12f });
    textEnt4.addComponent<cro::Drawable2D>();
    textEnt4.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    textEnt4.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    textEnt4.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
    lbEntity.getComponent<cro::Transform>().addChild(textEnt4.getComponent<cro::Transform>());
    auto nameColumn = textEnt4;

    //rank column
    textEnt4 = m_uiScene.createEntity();
    textEnt4.addComponent<cro::Transform>().setPosition({ 38.f, 234.f, 0.12f });
    textEnt4.addComponent<cro::Drawable2D>();
    textEnt4.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    textEnt4.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    textEnt4.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
    lbEntity.getComponent<cro::Transform>().addChild(textEnt4.getComponent<cro::Transform>());
    auto rankColumn = textEnt4;

    //score column
    textEnt4 = m_uiScene.createEntity();
    textEnt4.addComponent<cro::Transform>().setPosition({ 364.f, 234.f, 0.12f });
    textEnt4.addComponent<cro::Drawable2D>();
    textEnt4.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    textEnt4.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    textEnt4.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
    lbEntity.getComponent<cro::Transform>().addChild(textEnt4.getComponent<cro::Transform>());
    auto scoreColumn = textEnt4;

    //id display
    textEnt4 = m_uiScene.createEntity();
    textEnt4.addComponent<cro::Transform>().setPosition({ std::floor(b.width / 2.f) - 98.f, 11.f, 0.12f });
    textEnt4.addComponent<cro::Drawable2D>();
    textEnt4.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    textEnt4.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    textEnt4.getComponent<cro::Text>().setString("Best of 5");
    lbEntity.getComponent<cro::Transform>().addChild(textEnt4.getComponent<cro::Transform>());
    auto scoreType = textEnt4;

    //hole index
    textEnt4 = m_uiScene.createEntity();
    textEnt4.addComponent<cro::Transform>().setPosition({ std::floor(b.width / 2.f) + 98.f, 11.f, 0.12f });
    textEnt4.addComponent<cro::Drawable2D>();
    textEnt4.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    textEnt4.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    textEnt4.getComponent<cro::Text>().setString("Random Target");
    lbEntity.getComponent<cro::Transform>().addChild(textEnt4.getComponent<cro::Transform>());
    auto holeIndex = textEnt4;

    //rank display
    textEnt4 = m_uiScene.createEntity();
    textEnt4.addComponent<cro::Transform>().setPosition({ b.width / 2.f, 262.f, 0.12f });
    textEnt4.addComponent<cro::Drawable2D>();
    textEnt4.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    textEnt4.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    textEnt4.getComponent<cro::Text>().setString("Global");
    centreText(textEnt4);
    lbEntity.getComponent<cro::Transform>().addChild(textEnt4.getComponent<cro::Transform>());
    auto rankType = textEnt4;

    auto updateDisplay = [nameColumn, scoreColumn, rankColumn, scoreType, holeIndex, rankType]() mutable
    {
        scoreType.getComponent<cro::Text>().setString(ScoreStrings[leaderboardTryCount]);
        holeIndex.getComponent<cro::Text>().setString(HoleStrings[leaderboardHoleIndex]);
        rankType.getComponent<cro::Text>().setString(RankStrings[leaderboardFilter]);
        centreText(scoreType);
        centreText(holeIndex);
        centreText(rankType);

        auto scores = Social::getDrivingRangeResults(leaderboardHoleIndex, leaderboardTryCount, leaderboardFilter);
        rankColumn.getComponent<cro::Text>().setString(scores[0]);
        nameColumn.getComponent<cro::Text>().setString(scores[1]);
        scoreColumn.getComponent<cro::Text>().setString(scores[2]);
    };


    lbEntity.getComponent<cro::Callback>().function =
        [&, updateDisplay](cro::Entity e, float dt) mutable
    {
        const float Speed = dt * 4.f;
        auto& [progress, direction] = e.getComponent<cro::Callback>().getUserData<LeaderboardData>();
        if (direction == 0)
        {
            //grow
            progress = std::min(1.f, progress + Speed);
            if (progress == 1)
            {
                direction = 1;
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Leaderboard);

                updateDisplay();
            }
        }
        else
        {
            //shrink
            progress = std::max(0.f, progress - Speed);
            if (progress == 0)
            {
                direction = 0;
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Options);
            }
        }
        e.getComponent<cro::Transform>().setScale({ cro::Util::Easing::easeOutQuad(progress), 1.f });
    };



    //browse leaderboards
    textEnt4 = m_uiScene.createEntity();
    textEnt4.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 88.f, 0.02f });
    textEnt4.addComponent<cro::Drawable2D>();
    textEnt4.addComponent<cro::Sprite>() = spriteSheet.getSprite("leaderboard_button");

    auto buttonBounds = textEnt4.getComponent<cro::Sprite>().getTextureBounds();
    textEnt4.getComponent<cro::Transform>().setOrigin({ std::floor(buttonBounds.width / 2.f), buttonBounds.height });
    textEnt4.addComponent<cro::AudioEmitter>() = as.getEmitter("switch");
    textEnt4.addComponent<cro::UIInput>().setGroup(MenuID::Options);
    textEnt4.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::Leaderboard);
    textEnt4.getComponent<cro::UIInput>().setNextIndex(ButtonID::Begin, ButtonID::Begin);
    if (Social::getClubLevel())
    {
        textEnt4.getComponent<cro::UIInput>().setPrevIndex(ButtonID::Clubset, ButtonID::Clubset);
    }
    else
    {
        textEnt4.getComponent<cro::UIInput>().setPrevIndex(ButtonID::CountPrev, ButtonID::CountPrev);
    }
    textEnt4.getComponent<cro::UIInput>().area = buttonBounds;
    textEnt4.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = lbuttonSelected;
    textEnt4.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = lbuttonUnselected;
    textEnt4.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, uiSystem, bgEntity, lbEntity, updateDisplay](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                const auto& [state, _] = bgEntity.getComponent<cro::Callback>().getUserData<PopupAnim>();
                if (state == PopupAnim::Hold
                    && activated(evt))
                {
                    uiSystem->setActiveGroup(MenuID::Dummy);
                    lbEntity.getComponent<cro::Callback>().active = true;
                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();

                    updateDisplay();
                }
            });

    bgEntity.getComponent<cro::Transform>().addChild(textEnt4.getComponent<cro::Transform>());

    auto labelEnt = m_uiScene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ std::floor(sBounds.width / 2.f), 13.f, 0.1f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(smallFont).setString("View Leaderboards");
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    centreText(labelEnt);
    textEnt4.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());

    //arrow buttons
    auto buttonSelected = uiSystem->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto buttonUnselected = uiSystem->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto createArrow =
        [&](glm::vec2 position, const std::string& spriteName, std::int32_t selectionIndex)
    {
        auto e = m_uiScene.createEntity();
        e.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.1f));
        e.addComponent<cro::AudioEmitter>() = as.getEmitter("switch");
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Sprite>() = leaderSheet.getSprite(spriteName);
        e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        auto b = e.getComponent<cro::Sprite>().getTextureBounds();
        e.addComponent<cro::UIInput>().area = b;
        e.getComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
        e.getComponent<cro::UIInput>().setSelectionIndex(selectionIndex);
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = buttonSelected;
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = buttonUnselected;

        lbEntity.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());

        return e;
    };
    //filter left
    entity = createArrow(glm::vec2(193.f, 250.f), "arrow_left", ButtonID::LBFilterPrev);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::LBFilterNext, ButtonID::LBCountNext);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::LBFilterNext, ButtonID::LBCountNext);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, updateDisplay](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    leaderboardFilter = (leaderboardFilter + (MaxLeaderboardFilter - 1)) % MaxLeaderboardFilter;
                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
                    updateDisplay();
                }
            });

    //filter right
    entity = createArrow(glm::vec2(285.f, 250.f), "arrow_right", ButtonID::LBFilterNext);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::LBFilterPrev, ButtonID::LBTargetPrev);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::LBFilterPrev, ButtonID::LBTargetPrev);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, updateDisplay](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    leaderboardFilter = (leaderboardFilter + 1) % MaxLeaderboardFilter;
                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
                    updateDisplay();
                }
            });

    //score left
    entity = createArrow(glm::vec2(95.f, -2.f), "arrow_left", ButtonID::LBCountPrev);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::LBCountNext, ButtonID::LBFilterPrev);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::LBClose, ButtonID::LBFilterPrev);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, updateDisplay](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    leaderboardTryCount = (leaderboardTryCount + (MaxLeaderboardFilter - 1)) % MaxLeaderboardFilter;
                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
                    updateDisplay();
                }
            });

    //score right
    entity = createArrow(glm::vec2(187.f, -2.f), "arrow_right", ButtonID::LBCountNext);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::LBTargetPrev, ButtonID::LBFilterPrev);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::LBCountPrev, ButtonID::LBFilterPrev);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, updateDisplay](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    leaderboardTryCount = (leaderboardTryCount + 1) % MaxLeaderboardFilter;
                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
                    updateDisplay();
                }
            });

    //index left
    entity = createArrow(glm::vec2(291.f, -2.f), "arrow_left", ButtonID::LBTargetPrev);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::LBTargetNext, ButtonID::LBFilterNext);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::LBCountNext, ButtonID::LBFilterNext);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, updateDisplay](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    leaderboardHoleIndex = (leaderboardHoleIndex + 13) % 14;
                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
                    updateDisplay();
                }
            });

    //index right
    entity = createArrow(glm::vec2(383.f, -2.f), "arrow_right", ButtonID::LBTargetNext);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::LBClose, ButtonID::LBFilterNext);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::LBTargetPrev, ButtonID::LBFilterNext);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem->addCallback(
            [&, updateDisplay](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    leaderboardHoleIndex = (leaderboardHoleIndex + 1) % 14;
                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
                    updateDisplay();
                }
            });

#endif

    //start button
    auto selectedBounds = spriteSheet.getSprite("start_highlight").getTextureRect();
    auto unselectedBounds = spriteSheet.getSprite("start_button").getTextureRect();
    auto startButton = m_uiScene.createEntity();
    startButton.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), 58.f, 0.2f });
    startButton.addComponent<cro::Drawable2D>();
    startButton.addComponent<cro::Sprite>() = spriteSheet.getSprite("start_button");
    startButton.addComponent<cro::AudioEmitter>() = as.getEmitter("switch");
    startButton.addComponent<cro::UIInput>().setGroup(MenuID::Options);
    startButton.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::Begin);
    startButton.getComponent<cro::UIInput>().setNextIndex(ButtonID::CountPrev, ButtonID::CountPrev);
#ifdef USE_GNS
    startButton.getComponent<cro::UIInput>().setPrevIndex(ButtonID::Leaderboard, ButtonID::Leaderboard);
#else
    if (Social::getClubLevel())
    {
        startButton.getComponent<cro::UIInput>().setNextIndex(ButtonID::Clubset, ButtonID::Clubset);
    }
    else
    {
        startButton.getComponent<cro::UIInput>().setNextIndex(ButtonID::CountPrev, ButtonID::CountPrev);
    }
#endif
    startButton.getComponent<cro::UIInput>().area = startButton.getComponent<cro::Sprite>().getTextureBounds();
    startButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem->addCallback(
            [selectedBounds](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setTextureRect(selectedBounds);
                e.getComponent<cro::Transform>().setOrigin({ selectedBounds.width / 2.f, selectedBounds.height / 2.f });
                e.getComponent<cro::AudioEmitter>().play();
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
                auto& [state, timeout] = bgEntity.getComponent<cro::Callback>().getUserData<PopupAnim>();
                if (state == PopupAnim::Hold
                    && activated(evt))
                {
                    state = PopupAnim::Close;
                    timeout = 1.f;
                    uiSystem->setActiveGroup(MenuID::Dummy);
                    
                    m_gameScene.getSystem<BallSystem>()->forceWindChange();
                    m_gameScene.getDirector<DrivingRangeDirector>()->setHoleCount(m_strokeCounts[m_strokeCountIndex], m_targetIndex - 1);

                    setHole(m_gameScene.getDirector<DrivingRangeDirector>()->getCurrentHole());

                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();

                    //hide the black fade.
                    m_summaryScreen.fadeEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
                    m_summaryScreen.fadeEnt.getComponent<cro::Callback>().active = true;

                    //set the round title
                    m_summaryScreen.roundName.getComponent<cro::Text>().setString(ScoreStrings[m_strokeCountIndex] + ", " + HoleStrings[m_targetIndex]);
                    centreText(m_summaryScreen.roundName);

                    m_mapTexture.clear(cro::Colour::Transparent);
                    m_mapTexture.display();

                    //reset stat timer
                    m_statClock.restart();
                }
            });
    centreSprite(startButton);
    bgEntity.getComponent<cro::Transform>().addChild(startButton.getComponent<cro::Transform>());



    spriteSheet.loadFromFile("assets/golf/sprites/large_flag.spt", m_resources.textures);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 380.f, 48.f, 0.01f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flag");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    bgEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //wang this in here so we can debug easier
    /*cro::Command cmd;
    cmd.targetFlags = CommandID::UI::DrivingBoard;
    cmd.action = [](cro::Entity e, float) {e.getComponent<cro::Callback>().active = true; };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);*/
}

void DrivingState::createSummary()
{
    auto* uiSystem = m_uiScene.getSystem<cro::UISystem>();

    //black fade
    auto fadeEnt = m_uiScene.createEntity();
    fadeEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 1.f });
    fadeEnt.addComponent<cro::Drawable2D>().setVertexData(
        {
        cro::Vertex2D(glm::vec2(0.f, 1.f), cro::Colour::Transparent),
        cro::Vertex2D(glm::vec2(0.f), cro::Colour::Transparent),
        cro::Vertex2D(glm::vec2(1.f), cro::Colour::Transparent),
        cro::Vertex2D(glm::vec2(1.f, 0.f), cro::Colour::Transparent)
        }
    );
    fadeEnt.addComponent<cro::Callback>().setUserData<float>(BackgroundAlpha);
    fadeEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto size = glm::vec2(cro::App::getWindow().getSize());
        e.getComponent<cro::Transform>().setScale(size);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        auto a = verts[0].colour.getAlpha();

        auto target = e.getComponent<cro::Callback>().getUserData<float>();
        const float step = dt * 2.f;

        if (a < target)
        {
            a = std::min(target, a + step);
        }
        else
        {
            a = std::max(target, a - step);

            if (a == 0)
            {
                e.getComponent<cro::Callback>().setUserData<float>(BackgroundAlpha);
                e.getComponent<cro::Callback>().active = false;
                e.getComponent<cro::Transform>().setPosition({ -10000.f, -10000.f });
            }
        }

        for (auto& v : verts)
        {
            v.colour.setAlpha(a);
        }
    };
    m_summaryScreen.fadeEnt = fadeEnt;

    //background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/scoreboard.spt", m_resources.textures);
    auto bgSprite = cro::Sprite(m_resources.textures.get("assets/golf/images/driving_range_menu.png"));

    auto bounds = bgSprite.getTextureBounds();
    auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    auto position = glm::vec3(size.x / 2.f, size.y / 2.f, 1.5f);

    auto bgEntity = m_uiScene.createEntity();
    bgEntity.addComponent<cro::Transform>().setPosition(position);
    bgEntity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    bgEntity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    bgEntity.addComponent<cro::Drawable2D>();
    bgEntity.addComponent<cro::Sprite>() = bgSprite;
    bgEntity.addComponent<cro::Callback>().setUserData<PopupAnim>();
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

    auto rangeText = m_uiScene.createEntity();
    rangeText.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 26.f, 0.02f });
    rangeText.addComponent<cro::Drawable2D>();
    rangeText.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    rangeText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    rangeText.getComponent<cro::Text>().setString("Game Over Man");
    centreText(rangeText);
    bgEntity.getComponent<cro::Transform>().addChild(rangeText.getComponent<cro::Transform>());
    m_summaryScreen.roundName = rangeText;


    //info text
    auto infoEnt = m_uiScene.createEntity();
    infoEnt.addComponent<cro::Transform>().setPosition({ /*SummaryOffset*/std::floor(bounds.width / 3.f) - 25.f, SummaryHeight, 0.02f});
    infoEnt.addComponent<cro::Drawable2D>();
    infoEnt.addComponent<cro::Text>(smallFont).setString("Sample Text\n1\n1\n1\n1\n1\n1\n1\n1");
    infoEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    infoEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bgEntity.getComponent<cro::Transform>().addChild(infoEnt.getComponent<cro::Transform>());
    m_summaryScreen.text01 = infoEnt;

    infoEnt = m_uiScene.createEntity();
    infoEnt.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 18.f/* SummaryOffset*/, SummaryHeight, 0.02f });
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
    summaryEnt.getComponent<cro::Text>().setString("Placeholder Text");
    centreText(summaryEnt);
    bgEntity.getComponent<cro::Transform>().addChild(summaryEnt.getComponent<cro::Transform>());
    m_summaryScreen.summary = summaryEnt;



    cro::AudioScape as;
    as.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);
    m_summaryScreen.audioEnt = m_uiScene.createEntity();
    m_summaryScreen.audioEnt.addComponent<cro::Transform>();
    m_summaryScreen.audioEnt.addComponent<cro::AudioEmitter>() = as.getEmitter("accept");

    //star ratings
    const float starWidth = spriteSheet.getSprite("star").getTextureBounds().width;
    glm::vec3 pos(std::floor((bounds.width / 2.f) - (starWidth * 1.5f)), 76.f, 0.02f);
    for (auto i = 0; i < 3; ++i)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(pos);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("star");
        entity.addComponent<cro::SpriteAnimation>().play(0);
        entity.addComponent<cro::AudioEmitter>() = as.getEmitter("star");
        entity.addComponent<cro::ParticleEmitter>().settings.loadFromFile("assets/golf/particles/spark.xyp", m_resources.textures);
        entity.addComponent<cro::Callback>().function = StarAnimCallback(1.5f + (i * 0.5f));
        bgEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        m_summaryScreen.stars[i] = entity;
        pos.x += starWidth;
    }


    //high score text
    auto textEnt4 = m_uiScene.createEntity();
    textEnt4.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 134.f, 0.02f });
    textEnt4.addComponent<cro::Drawable2D>();
    textEnt4.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    textEnt4.getComponent<cro::Text>().setFillColour(TextNormalColour);
    textEnt4.getComponent<cro::Text>().setString("New Personal Best!");
    textEnt4.addComponent<cro::Callback>().setUserData<float>(0.f);
    textEnt4.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;
        if (currTime < 0)
        {
            currTime += 1.f;

            auto c = e.getComponent<cro::Text>().getFillColour();
            c.setAlpha(c.getAlpha() > 0 ? 0.f : 1.f);
            e.getComponent<cro::Text>().setFillColour(c);
        }
    };
    centreText(textEnt4);
    bgEntity.getComponent<cro::Transform>().addChild(textEnt4.getComponent<cro::Transform>());
    m_summaryScreen.bestMessage = textEnt4;

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
    entity.addComponent<cro::AudioEmitter>() = as.getEmitter("switch");
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Summary);
    entity.getComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem->addCallback(
            [selectedBounds](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setTextureRect(selectedBounds);
                e.getComponent<cro::Transform>().setOrigin({ selectedBounds.width / 2.f, selectedBounds.height / 2.f });
                e.getComponent<cro::AudioEmitter>().play();
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
                auto& [state, timeout] = bgEntity.getComponent<cro::Callback>().getUserData<PopupAnim>();
                if (state == PopupAnim::Hold
                    && activated(evt))
                {
                    auto c = TextNormalColour;
                    c.setAlpha(0.f);
                    m_summaryScreen.bestMessage.getComponent<cro::Text>().setFillColour(c);
                    m_summaryScreen.bestMessage.getComponent<cro::Callback>().active = false;

                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
                    state = PopupAnim::Close;
                    timeout = 1.f;

                    for (auto star : m_summaryScreen.stars)
                    {
                        star.getComponent<cro::SpriteAnimation>().play(0);
                    }

                    cro::Command cmd;
                    cmd.targetFlags = CommandID::UI::DrivingBoard;
                    cmd.action = [](cro::Entity e, float) {e.getComponent<cro::Callback>().active = true; };
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                    //fade background should already be visible from showing summary
                    /*m_summaryScreen.fadeEnt.getComponent<cro::Callback>().setUserData<float>(BackgroundAlpha);
                    m_summaryScreen.fadeEnt.getComponent<cro::Callback>().active = true;
                    m_summaryScreen.fadeEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, FadeDepth });*/
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
    entity.addComponent<cro::AudioEmitter>() = as.getEmitter("switch");
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Summary);
    entity.getComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem->addCallback(
            [selectedBounds](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setTextureRect(selectedBounds);
                e.getComponent<cro::Transform>().setOrigin({ selectedBounds.width / 2.f, selectedBounds.height / 2.f });
                e.getComponent<cro::AudioEmitter>().play();
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
                auto& [state, timeout] = bgEntity.getComponent<cro::Callback>().getUserData<PopupAnim>();
                if (state == PopupAnim::Hold
                    && activated(evt))
                {
                    m_summaryScreen.audioEnt.getComponent<cro::AudioEmitter>().play();
                    requestStackClear();
                    requestStackPush(StateID::Menu);
                }
            });
    centreSprite(entity);
    bgEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    spriteSheet.loadFromFile("assets/golf/sprites/large_flag.spt", m_resources.textures);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 380.f, 48.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flag");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    bgEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    m_summaryScreen.root = bgEntity;
}

void DrivingState::updateMinimap()
{
    auto oldCam = m_gameScene.setActiveCamera(m_mapCam);

    m_mapTexture.clear(TextNormalColour);
    m_gameScene.render();

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
    cmd.action = [&,direction](cro::Entity e, float dt)
    {
        float knots = direction.y * KnotsPerMetre;
        float speed = direction.y;

        std::stringstream ss;
        ss.precision(2);
        if (m_sharedData.imperialMeasurements)
        {
            ss << " " << std::fixed << speed * MPHPerMetre << " mph";
        }
        else
        {
            ss << " " << std::fixed << speed * KPHPerMetre << " kph";
        }
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

    cmd.targetFlags = CommandID::UI::WindSpeed;
    cmd.action = [direction](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().setUserData<float>(-direction.y);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    m_gameScene.getSystem<CloudSystem>()->setWindVector(direction);
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

    std::uint8_t starCount = 0;

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 56.f, 0.02f });
    textEnt.addComponent<cro::Drawable2D>();
    textEnt.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    textEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    if (score < BadScore)
    {
        textEnt.getComponent<cro::Text>().setString("Bad Luck!");

        if (m_avatar.animationIDs[AnimationID::Disappoint] != AnimationID::Invalid)
        {
            m_avatar.model.getComponent<cro::Skeleton>().play(m_avatar.animationIDs[AnimationID::Disappoint], 1.f, 0.8f);
        }
    }
    else if (score < GoodScore)
    {
        textEnt.getComponent<cro::Text>().setString("Good Effort!");
        starCount = 1;
        Social::awardXP(XPValues[XPID::Good]);
    }
    else if (score < ExcellentScore)
    {
        textEnt.getComponent<cro::Text>().setString("Not Bad!");
        starCount = 2;
        Social::awardXP(XPValues[XPID::NotBad]);
    }
    else
    {
        textEnt.getComponent<cro::Text>().setString("Excellent!");
        starCount = 3;
        Social::awardXP(XPValues[XPID::Excellent]);

        if (m_avatar.animationIDs[AnimationID::Celebrate] != AnimationID::Invalid)
        {
            m_avatar.model.getComponent<cro::Skeleton>().play(m_avatar.animationIDs[AnimationID::Celebrate], 1.f, 0.8f);
        }
    }
    centreText(textEnt);
    entity.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());


    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    std::stringstream s1;
    s1.precision(3);
    if (m_sharedData.imperialMeasurements)
    {
        s1 << range * 1.094f << "y";
    }
    else
    {
        s1 << range << "m";
    }

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
    entity.getComponent<cro::Callback>().setUserData<PopupAnim>();
    entity.getComponent<cro::Callback>().function =
        [&, textEnt, textEnt2, textEnt3, imgEnt, starCount](cro::Entity e, float dt) mutable
    {
        static constexpr float HoldTime = 4.f;
        auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<PopupAnim>();
        switch (state)
        {
        default: break;
        case PopupAnim::Delay:
            currTime = std::max(0.f, currTime - dt);
            if (currTime == 0)
            {
                state = PopupAnim::Open;
            }
            break;
        case PopupAnim::Open:
            //grow
            currTime = std::min(1.f, currTime + (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(glm::vec2(m_viewScale.x, m_viewScale.y * cro::Util::Easing::easeOutQuint(currTime)));
            if (currTime == 1)
            {
                currTime = 0;
                state = PopupAnim::Hold;
                imgEnt.getComponent<cro::SpriteAnimation>().play(starCount);
            }
            break;
        case PopupAnim::Hold:
            //hold
            currTime = std::min(HoldTime, currTime + dt);

            if (currTime > (HoldTime / 2.f))
            {
                //this should be safe to call repeatedly
                m_gameScene.getSystem<CameraFollowSystem>()->resetCamera();
            }

            if (currTime == HoldTime)
            {
                currTime = 1.f;
                state = PopupAnim::Close;
            }
            break;
        case PopupAnim::Close:
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
                    auto j = 0;
                    for (j = 0; j < std::min(9, scoreCount); ++j)
                    {
                        float score = director->getScore(j);

                        std::stringstream s;
                        s.precision(3);
                        s << "Turn " << j + 1 << ":      " << score << "%\n";
                        summary += s.str();

                        totalScore += score;
                    }
                    j++;
                    for (; j < 10; ++j)
                    {
                        summary += "Turn " + std::to_string(j) + ":       N/A\n";
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
                        /*m_summaryScreen.text02.getComponent<cro::Text>().setString(summary);

                        auto& tx = m_summaryScreen.text01.getComponent<cro::Transform>();
                        auto pos = tx.getPosition();
                        pos.x = SummaryOffset;
                        tx.setPosition(pos);
                        tx.setOrigin({ 0.f, 0.f });*/
                    }
                    else
                    {
                        for (auto i = 10; i < 19; ++i)
                        {
                            summary += "Turn " + std::to_string(i) + ":       N/A\n";
                        }

                        //auto& tx = m_summaryScreen.text01.getComponent<cro::Transform>();
                        //auto pos = tx.getPosition();
                        //pos.x = 200.f; //TODO this should be half background width
                        //tx.setPosition(pos);
                        //centreText(m_summaryScreen.text01);
                    }
                    m_summaryScreen.text02.getComponent<cro::Text>().setString(summary);

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
                        m_summaryScreen.stars[0].getComponent<cro::Callback>().active = true;
                        Achievements::awardAchievement(AchievementStrings[AchievementID::BronzeStar]);
                    }
                    else if (totalScore < ExcellentScore)
                    {
                        s << "Great Job!";
                        m_summaryScreen.stars[0].getComponent<cro::Callback>().active = true;
                        m_summaryScreen.stars[1].getComponent<cro::Callback>().active = true;
                        Achievements::awardAchievement(AchievementStrings[AchievementID::SilverStar]);
                    }
                    else
                    {
                        s << "Excellent!";
                        m_summaryScreen.stars[0].getComponent<cro::Callback>().active = true;
                        m_summaryScreen.stars[1].getComponent<cro::Callback>().active = true;
                        m_summaryScreen.stars[2].getComponent<cro::Callback>().active = true;
                        Achievements::awardAchievement(AchievementStrings[AchievementID::GoldStar]);
                    }

                    m_summaryScreen.summary.getComponent<cro::Text>().setString(s.str());
                    centreText(m_summaryScreen.summary);
                    
                    m_summaryScreen.fadeEnt.getComponent<cro::Transform>().setPosition({0.f, 0.f, FadeDepth});
                    m_summaryScreen.fadeEnt.getComponent<cro::Callback>().setUserData<float>(BackgroundAlpha);
                    m_summaryScreen.fadeEnt.getComponent<cro::Callback>().active = true;
                    m_summaryScreen.root.getComponent<cro::Callback>().active = true;

                    if (totalScore > m_topScores[m_strokeCountIndex])
                    {
                        m_topScores[m_strokeCountIndex] = totalScore;
                        saveScores();
                        m_summaryScreen.bestMessage.getComponent<cro::Callback>().active = true;
                    }
                    else
                    {
                        auto c = TextNormalColour;
                        c.setAlpha(0.f);
                        m_summaryScreen.bestMessage.getComponent<cro::Text>().setFillColour(c);
                    }

#ifdef USE_GNS
                    Social::insertDrivingScore(m_targetIndex, static_cast<std::int32_t>(m_strokeCountIndex), totalScore);
#endif

                    //reset the minimap
                    auto oldCam = m_gameScene.setActiveCamera(m_mapCam);
                    m_mapTexture.clear(TextNormalColour);
                    m_gameScene.render();
                    m_mapTexture.display();
                    m_gameScene.setActiveCamera(oldCam);

                    //update the stat
                    Achievements::incrementStat(StatStrings[StatID::TimeOnTheRange], m_statClock.elapsed().asSeconds());
                }
                else
                {
                    setHole(m_gameScene.getDirector<DrivingRangeDirector>()->getCurrentHole());
                }
            }
            break;
        case PopupAnim::Abort:
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(textEnt);
            m_uiScene.destroyEntity(textEnt2);
            m_uiScene.destroyEntity(textEnt3);
            m_uiScene.destroyEntity(imgEnt);
            m_uiScene.destroyEntity(e);
            break;
        }
    };


    //raise a message for sound effects etc
    auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::DriveComplete;
    msg->score = starCount;
}

void DrivingState::floatingMessage(const std::string& msg)
{
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);

    const float offsetScale = Social::isSteamdeck() ? 2.f : 1.f;

    glm::vec2 size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    glm::vec3 position((size.x / 2.f), (UIBarHeight + (14.f * offsetScale)) * m_viewScale.y, 0.2f);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.getComponent<cro::Transform>().setScale(m_viewScale);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(msg);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    centreText(entity);

    entity.addComponent<FloatingText>().basePos = position;
}

void DrivingState::updateSkipMessage(float dt)
{
    if (m_skipState.state == static_cast<std::int32_t>(Ball::State::Flight))
    {
        if (!m_skipState.wasSkipped)
        {
            if (m_skipState.previousState != m_skipState.state)
            {
                //state has changed, set visible
                cro::Command cmd;
                cmd.targetFlags = CommandID::UI::FastForward;
                cmd.action = [&](cro::Entity e, float)
                {
                    auto& data = e.getComponent<cro::Callback>().getUserData<SkipCallbackData>();
                    data.direction = 1;

                    e.getComponent<cro::Callback>().active = true;

                    if (cro::GameController::getControllerCount() != 0
                        && m_skipState.displayControllerMessage)
                    {
                        //set correct button icon
                        if (cro::GameController::hasPSLayout(activeControllerID(0)))
                        {
                            data.buttonIndex = 1; //used as animation ID
                        }
                        else
                        {
                            data.buttonIndex = 0;
                        }
                        e.getComponent<cro::Text>().setString("Hold   to Skip");
                    }
                    else
                    {
                        data.buttonIndex = std::numeric_limits<std::uint32_t>::max();
                        e.getComponent<cro::Text>().setString("Hold " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) + " to Skip");
                    }
                    centreText(e);
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }

            //read input
            if (cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Action])
                || cro::GameController::isButtonPressed(activeControllerID(0), m_sharedData.inputBinding.buttons[InputBinding::Action]))
            {
                m_skipState.currentTime = std::min(SkipState::SkipTime, m_skipState.currentTime + dt);
                if (m_skipState.currentTime == SkipState::SkipTime)
                {
                    //hide message
                    cro::Command cmd;
                    cmd.targetFlags = CommandID::UI::FastForward;
                    cmd.action = [&](cro::Entity e, float)
                    {
                        e.getComponent<cro::Callback>().getUserData<SkipCallbackData>().direction = 0;
                        e.getComponent<cro::Callback>().active = true;
                    };
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                    //skip ball forwards
                    cmd.targetFlags = CommandID::Ball;
                    cmd.action = [&](cro::Entity e, float)
                    {
                        m_gameScene.getSystem<BallSystem>()->fastForward(e);

                        //update minimap with final position
                        cro::Command cmd2;
                        cmd2.targetFlags = CommandID::UI::MiniBall;
                        cmd2.action =
                            [e](cro::Entity f, float)
                        {
                            auto pos = e.getComponent<cro::Transform>().getPosition();

                            auto position = glm::vec3(pos.x, -pos.z, 0.1f) / 2.f;
                            //need to tie into the fact the mini map is 1/2 scale
                            //and has the origin in the centre
                            f.getComponent<cro::Transform>().setPosition(position + glm::vec3(RangeSize / 4.f, 0.f));

                            //set scale based on height
                            static constexpr float MaxHeight = 40.f;
                            float scale = 1.f + ((pos.y / MaxHeight) * 2.f);
                            f.getComponent<cro::Transform>().setScale(glm::vec2(scale));
                        };
                        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd2);


                        //and any following cameras
                        cmd2.targetFlags = CommandID::SpectatorCam;
                        cmd2.action = [&, e](cro::Entity f, float)
                        {
                            f.getComponent<CameraFollower>().target = e;
                            f.getComponent<CameraFollower>().playerPosition = PlayerPosition;
                            f.getComponent<CameraFollower>().holePosition = m_holeData[m_gameScene.getDirector<DrivingRangeDirector>()->getCurrentHole()].pin;
                        };
                        m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd2);

                        setActiveCamera(CameraID::Green);
                    };
                    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
                    m_ballTrail.reset();

                    m_skipState.wasSkipped = true;
                }
            }
            else
            {
                m_skipState.currentTime = 0.f;
            }
        }
    }
    else
    {
        if (m_skipState.previousState != m_skipState.state)
        {
            //state has changed, hide
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::FastForward;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().getUserData<SkipCallbackData>().direction = 0;
                e.getComponent<cro::Callback>().active = true;
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
    }

    m_skipState.previousState = m_skipState.state;
}