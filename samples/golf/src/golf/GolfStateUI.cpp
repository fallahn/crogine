/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "GolfState.hpp"
#include "GameConsts.hpp"
#include "CommandIDs.hpp"
#include "SharedStateData.hpp"
#include "Clubs.hpp"
#include "MenuConsts.hpp"
#include "CommonConsts.hpp"
#include "TextAnimCallback.hpp"
#include "ScoreStrings.hpp"
#include "MessageIDs.hpp"
#include "NotificationSystem.hpp"
#include "TrophyDisplaySystem.hpp"
#include "FloatingTextSystem.hpp"
#include "PacketIDs.hpp"
#include "MiniBallSystem.hpp"
#include "BallSystem.hpp"
#include "CallbackData.hpp"
#include "InterpolationSystem.hpp"
#include "XPAwardStrings.hpp"
#include "../ErrorCheck.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Social.hpp>

#include <crogine/audio/AudioScape.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/SpriteSystem3D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>

#include <crogine/detail/glm/gtx/rotate_vector.hpp>

namespace
{
#include "PostProcess.inl"

    //hack to map course names to achievement IDs
    const std::unordered_map < std::string, std::int32_t> ParAch =
    {
        std::make_pair("course_01", AchievementID::Master01),
        std::make_pair("course_02", AchievementID::Master02),
        std::make_pair("course_03", AchievementID::Master03),
        std::make_pair("course_04", AchievementID::Master04),
        std::make_pair("course_05", AchievementID::Master05),
        std::make_pair("course_06", AchievementID::Master06),
        std::make_pair("course_07", AchievementID::Master07),
        std::make_pair("course_08", AchievementID::Master08),
        std::make_pair("course_09", AchievementID::Master09),
        std::make_pair("course_10", AchievementID::Master10),
    };

    static constexpr float ColumnWidth = 20.f;
    static constexpr float ColumnHeight = 276.f;
    static constexpr float ColumnMargin = 6.f;
    static constexpr std::array ColumnPositions =
    {
        glm::vec2(8.f, ColumnHeight),
        glm::vec2((ColumnWidth * 6.f) + ColumnMargin, ColumnHeight),
        glm::vec2((ColumnWidth * 7.f) + ColumnMargin, ColumnHeight),
        glm::vec2((ColumnWidth * 8.f) + ColumnMargin, ColumnHeight),
        glm::vec2((ColumnWidth * 9.f) + ColumnMargin, ColumnHeight),
        glm::vec2((ColumnWidth * 10.f) + ColumnMargin, ColumnHeight),
        glm::vec2((ColumnWidth * 11.f) + ColumnMargin, ColumnHeight),
        glm::vec2((ColumnWidth * 12.f) + ColumnMargin, ColumnHeight),
        glm::vec2((ColumnWidth * 13.f) + ColumnMargin, ColumnHeight),
        glm::vec2((ColumnWidth * 14.f) + ColumnMargin, ColumnHeight),
        glm::vec2((ColumnWidth * 15.f) + ColumnMargin, ColumnHeight),
    };

    static constexpr float MaxExpansion = 100.f;
    float scoreboardExpansion = 0.f; //TODO move to member
    float stretchToScreen(cro::Entity e, cro::Sprite sprite, glm::vec2 baseSize)
    {
        constexpr float EdgeOffset = 40.f; //this much from outside before splitting

        e.getComponent<cro::Drawable2D>().setTexture(sprite.getTexture());
        auto bounds = sprite.getTextureBounds();
        auto rect = sprite.getTextureRectNormalised();

        //how much bigger to get either side in wider views
        float expansion = std::min(MaxExpansion, std::floor((baseSize.x - bounds.width) / 2.f));
        //only needs > 0 really but this gives a little leeway
        expansion = (baseSize.x - bounds.width > 10) ? expansion : 0.f;
        float edgeOffsetNorm = (EdgeOffset / sprite.getTexture()->getSize().x);

        bounds.width += expansion * 2.f;

        e.getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f, bounds.height), glm::vec2(rect.left, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(0.f), glm::vec2(rect.left, rect.bottom)),

                cro::Vertex2D(glm::vec2(EdgeOffset, bounds.height), glm::vec2(rect.left + edgeOffsetNorm, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(EdgeOffset, 0.f), glm::vec2(rect.left + edgeOffsetNorm, rect.bottom)),
                cro::Vertex2D(glm::vec2(EdgeOffset + expansion, bounds.height), glm::vec2(rect.left + edgeOffsetNorm, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(EdgeOffset + expansion, 0.f), glm::vec2(rect.left + edgeOffsetNorm, rect.bottom)),


                cro::Vertex2D(glm::vec2(bounds.width - EdgeOffset - expansion, bounds.height), glm::vec2((rect.left + rect.width) - edgeOffsetNorm, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(bounds.width - EdgeOffset - expansion, 0.f), glm::vec2((rect.left + rect.width) - edgeOffsetNorm, rect.bottom)),
                cro::Vertex2D(glm::vec2(bounds.width - EdgeOffset, bounds.height), glm::vec2((rect.left + rect.width) - edgeOffsetNorm, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(bounds.width - EdgeOffset, 0.f), glm::vec2((rect.left + rect.width) - edgeOffsetNorm, rect.bottom)),


                cro::Vertex2D(glm::vec2(bounds.width, bounds.height), glm::vec2(rect.left + rect.width, rect.bottom + rect.height)),
                cro::Vertex2D(glm::vec2(bounds.width, 0.f), glm::vec2(rect.left + rect.width, rect.bottom))
            });

        return expansion;
    }
}

void GolfState::buildUI()
{
    if (m_holeData.empty())
    {
        return;
    }

    auto resizeCallback = [&](cro::Entity e, float)
    {
        //this is activated once to make sure the
        //sprite is up to date with any texture buffer resize
        glm::vec2 texSize = e.getComponent<cro::Sprite>().getTexture()->getSize();
        e.getComponent<cro::Sprite>().setTextureRect({ glm::vec2(0.f), texSize });
        e.getComponent<cro::Transform>().setOrigin(texSize / 2.f);
        e.getComponent<cro::Callback>().active = false;
    };

    //draws the background using the render texture
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_gameSceneTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec3(bounds.width / 2.f, bounds.height / 2.f, 0.5f));
    entity.addComponent<cro::Callback>().function = resizeCallback;

    auto courseEnt = entity;
    m_courseEnt = courseEnt;

    //displays the trophies on round end - has to be displayed over top of scoreboard
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_trophySceneTexture.getTexture());
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec3(bounds.width / 2.f, bounds.height / 2.f, 0.f));
    entity.addComponent<cro::Callback>().function = resizeCallback;

    auto trophyEnt = entity;
    for (auto trophy : m_trophies)
    {
        trophyEnt.getComponent<cro::Transform>().addChild(trophy.label.getComponent<cro::Transform>());
        trophy.label.getComponent<cro::Callback>().setUserData<cro::Entity>(trophyEnt);
    }

    //info panel background - vertices are set in resize callback
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    auto infoEnt = entity;
    createSwingMeter(infoEnt);

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //player's name
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerName | CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.2f, 0.f };
    entity.getComponent<UIElement>().depth = 0.05f;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<cro::Callback>().setUserData<TextCallbackData>();
    entity.getComponent<cro::Callback>().function = TextAnimCallback();
    auto nameEnt = entity;
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //think bulb displayed when CPU players are thinking
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Thinking];
    bounds = m_sprites[SpriteID::Thinking].getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::ThinkBubble;
    entity.addComponent<cro::Callback>().setUserData<std::pair<std::int32_t, float>>(1, 0.f);
    entity.getComponent<cro::Callback>().function =
        [nameEnt](cro::Entity e, float dt)
    {
        auto end = nameEnt.getComponent<cro::Drawable2D>().getLocalBounds().width;
        e.getComponent<cro::Transform>().setPosition({ end + 6.f, 4.f });

        float scale = 0.f;
        auto& [direction, currTime] = e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>();
        if (direction == 0)
        {
            //grow
            currTime = std::min(1.f, currTime + (dt * 3.f));
            scale = cro::Util::Easing::easeOutQuint(currTime);
        }
        else
        {
            //shrink
            currTime = std::max(0.f, currTime - (dt * 3.f));
            if (currTime == 0)
            {
                e.getComponent<cro::Callback>().active = false;
            }

            scale = cro::Util::Easing::easeInQuint(currTime);
        }

        e.getComponent<cro::Transform>().setScale({ scale, scale });
    };
    nameEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //hole distance
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PinDistance | CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().depth = 0.05f;
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
    entity.getComponent<UIElement>().depth = 0.05f;
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //current stroke
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.61f, 0.f };
    entity.getComponent<UIElement>().absolutePosition = { 12.f, 0.f };
    entity.getComponent<UIElement>().depth = 0.05f;
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        auto stroke = std::to_string(m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole]);
        auto par = std::to_string(m_holeData[m_currentHole].par);
        e.getComponent<cro::Text>().setString("Score: " + stroke + " - Par: " + par);
    };
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //current terrain
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.76f, 1.f };
    entity.getComponent<UIElement>().depth = 0.05f;
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(TerrainStrings[m_currentPlayer.terrain]);
    };
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
    entity.getComponent<cro::Callback>().function =
        [&, spinEnt, fgOffset](cro::Entity e, float dt) mutable
    {
        auto& scale = e.getComponent<cro::Callback>().getUserData<float>();
        const float speed = dt * 10.f;
        if (getClub() != ClubID::Putter &&
            m_inputParser.isSpinputActive())
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
        auto s = /*cro::Util::Easing::easeOutElastic*/(scale);
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(38.f, 24.f, 0.03f));
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
    
    //circular background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(38.f, 56.f, 0.01f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::WindIndicator];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.getComponent<cro::Transform>().move(glm::vec2(0.f, -bounds.height));
    windEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto windDial = entity;
    //text background
    auto vertColour = cro::Colour(0.f, 0.f, 0.f, 0.25f);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -4.f, -14.f, -0.01f });
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, 6.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::WindSpeedBg];
    windEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //rotating part of dial
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::WindSpeed];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::WindSpeed;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
    {
        auto speed = e.getComponent<cro::Callback>().getUserData<float>();
        e.getComponent<cro::Transform>().rotate(speed / 6.f);
    };
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec3(bounds.width / 2.f, bounds.height / 2.f, 0.01f));
    entity.getComponent<cro::Transform>().setPosition(windDial.getComponent<cro::Transform>().getOrigin());
    windDial.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    
    //sets the initial cam rotation for the wind indicator compensation
    auto camDir = m_holeData[0].target - m_currentPlayer.position;
    m_camRotation = std::atan2(-camDir.z, camDir.y);

    //root used to show/hide input UI
    auto rootNode = m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    rootNode.addComponent<cro::CommandTarget>().ID = CommandID::UI::Root;
    rootNode.addComponent<cro::Callback>().setUserData<std::pair<std::int32_t, float>>(0, 0.f);
    rootNode.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& [dir, currTime] = e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>();

        if (dir == 0)
        {
            //grow
            currTime = std::min(1.f, currTime + dt);
            const float scale = cro::Util::Easing::easeOutElastic(currTime);

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
            const float scale = cro::Util::Easing::easeOutBack(currTime);

            e.getComponent<cro::Transform>().setScale({ scale, 1.f });

            if (currTime == 0)
            {
                dir = 0;
                e.getComponent<cro::Callback>().active = false;
            }
        }
        
    };
    infoEnt.getComponent<cro::Transform>().addChild(rootNode.getComponent<cro::Transform>());

    //power bar frame
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PowerBar];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec3(bounds.width / 2.f, bounds.height / 2.f, -0.05f));
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //power bar
    auto barEnt = entity;
    const auto BarCentre = bounds.width / 2.f;
    const auto BarWidth = bounds.width - 8.f;
    const auto BarHeight = bounds.height;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(5.f, 0.f, 0.1f)); //TODO expell the magic number!!
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(BarCentre, 8.f, 0.25f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::HookBar];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f)));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, BarCentre](cro::Entity e, float)
    {
        glm::vec3 pos(std::round(BarCentre + (BarCentre * m_inputParser.getHook())), 8.f, 0.25f);
        e.getComponent<cro::Transform>().setPosition(pos);
    };
    barEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //flag power/distance when putting
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(2.f, BarHeight, -0.01f));
    entity.getComponent<cro::Transform>().setOrigin({ -6.f, 1.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::MiniFlag];
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, BarWidth](cro::Entity e, float dt)
    {       
        float vScaleTarget = m_currentPlayer.terrain == TerrainID::Green ? 1.f : 0.f;

        auto scale = e.getComponent<cro::Transform>().getScale();
        if (vScaleTarget > 0)
        {
            //grow if not the first stroke (CPU players still need power prediction though)
            if (m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole] == 0
                || !m_sharedData.showPuttingPower)
            {
                scale.y = 0.f;
            }
            else
            {
                scale.y = std::min(1.f, scale.y + dt);
            }

            //move to position
            float hTarget = estimatePuttPower();
            if (m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].isCPU)
            {
                m_cpuGolfer.setPuttingPower(hTarget);
            }
            hTarget *= BarWidth;

            auto pos = e.getComponent<cro::Transform>().getPosition();
            pos.x = std::min(pos.x + ((hTarget - pos.x) * dt), BarWidth - 4.f);
            e.getComponent<cro::Transform>().setPosition(pos);
        }
        else
        {
            //shrink
            scale.y = std::max(0.f, scale.y - dt);
        }
        e.getComponent<cro::Transform>().setScale(scale);
    };
    barEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //hole number
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(38.f, -10.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::HoleNumber;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<cro::Callback>().setUserData<TextCallbackData>();
    entity.getComponent<cro::Callback>().function = TextAnimCallback();
    windEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //minimap view
    struct MinimapData final
    {
        std::int32_t state = 0;
        float scale = 0.001f;
        float rotation = -1.f;

        float textureRatio = 1.f; //pixels per metre in the minimap texture * 2
    };
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 82.f });
    entity.getComponent<cro::Transform>().setRotation(-90.f * cro::Util::Const::degToRad);
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(ShaderID::MinimapView));
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MiniMap;
    entity.addComponent<cro::Callback>().setUserData<MinimapData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [state, scale, rotation, ratio] = e.getComponent<cro::Callback>().getUserData<MinimapData>();
        float speed = dt * 4.f;
        float newScale = 0.f;
        
        if (state == 0)
        {
            //shrinking
            scale = std::max(0.f, scale - speed);
            newScale = cro::Util::Easing::easeOutSine(scale);

            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::MiniBall;
            cmd.action = [&](cro::Entity e, float)
            {
                e.getComponent<cro::Transform>().setScale({ 0.f, 0.f });
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            if (scale == 0)
            {
                m_mapCam.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);

                //update render
                glUseProgram(m_gridShaders[1].shaderID);
                glUniform1f(m_gridShaders[1].transparency, 0.f); //hides any putting grid

                auto oldCam = m_gameScene.setActiveCamera(m_mapCam);
                m_gameScene.getSystem<cro::CameraSystem>()->process(0.f);

                cro::Colour c = cro::Colour::Transparent;
                //cro::Colour c(std::uint8_t(39), 56, 153);
                m_mapTexture.clear(c);
                m_gameScene.render();
                m_mapTexture.display();
                m_gameScene.setActiveCamera(oldCam);
                m_mapTexture.setBorderColour(c);

                //and set to grow
                state = 1;

                //disable the cam again
                m_mapCam.getComponent<cro::Camera>().active = false;

                retargetMinimap(true);
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

                //show all balls
                cro::Command cmd;
                cmd.targetFlags = CommandID::UI::MiniBall;
                cmd.action = [&](cro::Entity e, float)
                {
                    e.getComponent<cro::Transform>().setPosition(m_minimapZoom.toMapCoords(m_holeData[m_currentHole].tee));
                    e.getComponent<cro::Transform>().setScale({ 1.f, 1.f });
                    e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                //and mini flag
                cmd.targetFlags = CommandID::UI::MiniFlag;
                cmd.action = [](cro::Entity e, float)
                {
                    e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }
        }
        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f / ratio, newScale / ratio));
    };
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto mapEnt = entity;
    m_minimapEnt = entity;


    //mini flag icon
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setRotation(cro::Util::Const::degToRad * 90.f);
    entity.getComponent<cro::Transform>().setOrigin({ 0.5f, -0.5f });
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MiniFlag;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::MapFlag];
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        e.getComponent<cro::Transform>().setPosition(glm::vec3(m_minimapZoom.toMapCoords(m_holeData[m_currentHole].pin), 0.02f));
        e.getComponent<cro::Transform>().setScale((m_minimapZoom.mapScale * 2.f * (1.f + ((m_minimapZoom.zoom - 1.f) * 0.125f))) * 0.75f);
    };
    mapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //stroke indicator
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>().getVertexData() = getStrokeIndicatorVerts();
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, mapEnt](cro::Entity e, float dt)
    {
        e.getComponent<cro::Transform>().setPosition(glm::vec3(m_minimapZoom.toMapCoords(m_currentPlayer.position), 0.01f));
        e.getComponent<cro::Transform>().setRotation(m_inputParser.getYaw() + m_minimapZoom.tilt);

        float& scale = e.getComponent<cro::Callback>().getUserData<float>();
        if (!m_inputParser.getActive()
            || m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].isCPU)
        {
            scale = std::max(0.f, scale - ((scale * dt) * 8.f));
        }
        else
        {
            auto club = getClub();
            if (club == ClubID::Putter
                /*&& !m_sharedData.showPuttingPower*/)
            {
                scale = std::max(0.f, scale - ((scale * dt) * 8.f));
            }
            else
            {
                const float targetScale = m_inputParser.getEstimatedDistance() * m_minimapZoom.zoom;
                if (scale < targetScale)
                {
                    scale = std::min(scale + (dt * ((targetScale - scale) * 10.f)), targetScale);
                }
                else
                {
                    scale = std::max(targetScale, scale - ((scale * dt) * 2.f));
                }
            }
        }

        //4 is the relative size of the sprite to the texture... need to update this if we make sprite scale dynamic
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale, 1.f) * (1.f / mapEnt.getComponent<cro::Transform>().getScale().x)); 
    };
    mapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //green close up view
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 0.f, 0.f }); //position is set in UI cam callback, below
    entity.addComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(ShaderID::Minimap));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MiniGreen;
    entity.addComponent<cro::Sprite>(); //updated by the camera callback with correct texture
    entity.addComponent<cro::Callback>().setUserData<GreenCallbackData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt) mutable
    {
        static constexpr float Speed = 2.f;
        auto& [currTime, state, targetScale] = e.getComponent<cro::Callback>().getUserData<GreenCallbackData>();
        if (state == 0)
        {
            //expand
            currTime = std::min(1.f, currTime + (dt * Speed));
            float scale = cro::Util::Easing::easeOutQuint(currTime) * targetScale;
            e.getComponent<cro::Transform>().setScale({ scale,  targetScale });

            if (currTime == 1)
            {
                state = 1;
                e.getComponent<cro::Callback>().active = false;

                //start the cam view updater
                m_greenCam.getComponent<cro::Callback>().active = true;
            }
        }
        else
        {
            //contract
            currTime = std::max(0.f, currTime - (dt * Speed));
            float scale = cro::Util::Easing::easeOutQuint(currTime) * targetScale;
            e.getComponent<cro::Transform>().setScale({ targetScale, scale });

            if (currTime == 0)
            {
                state = 0;
                e.getComponent<cro::Callback>().active = false;

                m_greenCam.getComponent<cro::Callback>().active = false;
                m_greenCam.getComponent<cro::Camera>().active = false;
            }
        }
    };

    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto greenEnt = entity;
    m_miniGreenEnt = entity;

    //stroke indicator for minigreen
    const auto startCol = TextGoldColour;
    auto endCol = startCol;
    endCol.setAlpha(0.05f);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 0.25f), startCol),
            cro::Vertex2D(glm::vec2(0.f, -0.25f), startCol),
            cro::Vertex2D(glm::vec2(15.f, 0.25f), endCol),
            cro::Vertex2D(glm::vec2(15.f, -0.25f), endCol),
        });
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(1.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        const float Speed = dt * 4.f;
        auto& currSize = e.getComponent<cro::Callback>().getUserData<float>();
        if (m_inputParser.getActive()
            && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU
            && m_sharedData.showPuttingPower)
        {
            currSize = std::min(1.f, currSize + Speed);
            const float rotation = m_inputParser.getYaw();
            e.getComponent<cro::Transform>().setRotation(rotation);
        }
        else
        {
            currSize = std::max(0.f, currSize - Speed);
        }
        float scale = cro::Util::Easing::easeOutBack(currSize);
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale) * (m_viewScale.x / m_miniGreenEnt.getComponent<cro::Transform>().getScale().x));
    };
    m_miniGreenIndicatorEnt = entity;
    greenEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    createScoreboard();


    //set up the overhead cam for the mini map
    auto updateMiniView = [&, mapEnt](cro::Camera& miniCam) mutable
    {
        glm::uvec2 previewSize = MapSize * 8u;

        m_mapTexture.create(previewSize.x, previewSize.y);

        mapEnt.getComponent<cro::Sprite>().setTexture(m_mapTexture.getTexture());
        mapEnt.getComponent<cro::Transform>().setOrigin({ previewSize.x / 2.f, previewSize.y / 2.f });
        mapEnt.getComponent<cro::Callback>().getUserData<MinimapData>().textureRatio = 16.f; //TODO this is always double the map size multiplier
        m_minimapZoom.mapScale = previewSize / MapSize;
        m_minimapZoom.pan = previewSize / 2u;
        m_minimapZoom.textureSize = previewSize;
        m_minimapZoom.updateShader();

        glm::vec2 viewSize(MapSize);
        miniCam.setOrthographic(-viewSize.x / 2.f, viewSize.x / 2.f, -viewSize.y / 2.f, viewSize.y / 2.f, -0.1f, 50.f);
        miniCam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    m_mapCam = m_gameScene.createEntity();
    m_mapCam.addComponent<cro::Transform>().setPosition({ static_cast<float>(MapSize.x) / 2.f, 38.f, -static_cast<float>(MapSize.y) / 2.f});
    m_mapCam.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    auto& miniCam = m_mapCam.addComponent<cro::Camera>();
    miniCam.renderFlags = RenderFlags::MiniMap;
    miniCam.active = false;
    //this is a hack to stop the entire terrain being drawn in shadow
    miniCam.shadowMapBuffer.create(2, 2);
    miniCam.shadowMapBuffer.clear();
    miniCam.shadowMapBuffer.display();
    //miniCam.resizeCallback = updateMiniView; //don't do this on resize as recreating the buffer clears it..
    updateMiniView(miniCam);



    //and the mini view of the green
    auto updateGreenView = [&, greenEnt](cro::Camera& greenCam) mutable
    {
        auto texSize = MapSize.y / 2u;

        auto windowScale = getViewScale();
        float scale = m_sharedData.pixelScale ? windowScale : 1.f;
        scale = (windowScale + 1.f) - scale;
        texSize *= static_cast<std::uint32_t>(scale);

        std::uint32_t samples = m_sharedData.pixelScale ? 0 :
            m_sharedData.antialias ? m_sharedData.multisamples : 0;

        m_greenBuffer.create(texSize, texSize, true, false, samples); //yes, it's square
        greenEnt.getComponent<cro::Sprite>().setTexture(m_greenBuffer.getTexture());

        auto targetScale = glm::vec2(1.f / scale);
        if (m_currentPlayer.terrain == TerrainID::Green)
        {
            greenEnt.getComponent<cro::Transform>().setScale(targetScale);
        }
        greenEnt.getComponent<cro::Transform>().setOrigin({ (texSize / 2), (texSize / 2) }); //must divide to a whole pixel!
        greenEnt.getComponent<cro::Callback>().getUserData<GreenCallbackData>().targetScale = targetScale.x;
    };

    m_greenCam = m_gameScene.createEntity();
    m_greenCam.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    auto& greenCam = m_greenCam.addComponent<cro::Camera>();
    greenCam.renderFlags = RenderFlags::MiniGreen;
    greenCam.resizeCallback = updateGreenView;
    greenCam.active = false;
    greenCam.shadowMapBuffer.create(2, 2);
    greenCam.shadowMapBuffer.clear();
    greenCam.shadowMapBuffer.display();
    updateGreenView(greenCam);

    m_greenCam.addComponent<cro::Callback>().active = true;
    m_greenCam.getComponent<cro::Callback>().setUserData<MiniCamData>();
    m_greenCam.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        //zooms the view as the current player approaches hole
        auto& data = e.getComponent<cro::Callback>().getUserData<MiniCamData>();
        auto diff = data.targetSize - data.currentSize;
        data.currentSize += diff * (dt * 4.f);

        auto& cam = e.getComponent<cro::Camera>();
        cam.setOrthographic(-data.currentSize, data.currentSize, -data.currentSize, data.currentSize, -0.15f, 1.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };


    //callback for the UI camera when window is resized
    auto updateView = [&, trophyEnt, courseEnt, infoEnt, spinEnt, windEnt, mapEnt, greenEnt, rootNode](cro::Camera& cam) mutable
    {
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -3.5f, 20.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        m_inputParser.setMouseScale(m_viewScale.x);

        glm::vec2 courseScale(m_sharedData.pixelScale ? m_viewScale.x : 1.f);

        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -0.1f));
        courseEnt.getComponent<cro::Transform>().setScale(courseScale);
        courseEnt.getComponent<cro::Callback>().active = true; //makes sure to delay so updating the texture size is complete first

        trophyEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, 2.1f));
        trophyEnt.getComponent<cro::Transform>().setScale(courseScale);
        trophyEnt.getComponent<cro::Callback>().active = true;

        //update minimap
        const auto uiSize = size / m_viewScale;
        auto mapSize = glm::vec2(MapSize / 2u);
        mapSize /= 2.f;
        mapEnt.getComponent<cro::Transform>().setPosition({ uiSize.x - mapSize.y - 2.f, uiSize.y - mapSize.x - (UIBarHeight + 2.f), -0.05f }); //map sprite is rotated 90


        greenEnt.getComponent<cro::Transform>().setPosition({ 2.f, uiSize.y - std::floor(MapSize.y * 0.6f) - UIBarHeight - 2.f, -0.1f });
        greenEnt.getComponent<cro::Transform>().move(glm::vec2(static_cast<float>(MapSize.y) / 4.f));

        windEnt.getComponent<cro::Transform>().setPosition(glm::vec2(/*uiSize.x + */WindIndicatorPosition.x, WindIndicatorPosition.y));
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
        refreshUI();

        //relocate the power bar
        auto uiPos = glm::vec2(uiSize.x / 2.f, UIBarHeight / 2.f);
        rootNode.getComponent<cro::Transform>().setPosition(uiPos);

        //this calls the update for the scoreboard render texture
        updateScoreboard();
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.renderFlags = ~RenderFlags::Reflection;
    cam.resizeCallback = updateView;
    updateView(cam);
    m_uiScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 5.f });

    m_emoteWheel.build(infoEnt, m_uiScene, m_resources.textures);
}

void GolfState::createSwingMeter(cro::Entity root)
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

void GolfState::showCountdown(std::uint8_t seconds)
{
    m_roundEnded = true;

    //hide any input
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>().first = 1;
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //show the scores
    updateScoreboard();
    showScoreboard(true);

    //check if we're the winner
    if (m_statBoardScores.size() > 1) //not the only player
    {
        if (m_statBoardScores[0].client == m_sharedData.clientConnection.connectionID)
        {
            if (!m_sharedData.localConnectionData.playerData[m_statBoardScores[0].player].isCPU)
            {
                //remember this is auto-disabled if the player is not the only one on the client
                Achievements::awardAchievement(AchievementStrings[AchievementID::LeaderOfThePack]);

                switch (m_sharedData.scoreType)
                {
                default:
                case ScoreType::Stroke:
                    Achievements::awardAchievement(AchievementStrings[AchievementID::StrokeOfGenius]);
                    break;
                case ScoreType::Match:
                    Achievements::awardAchievement(AchievementStrings[AchievementID::NoMatch]);
                    break;
                case ScoreType::Skins:
                    Achievements::awardAchievement(AchievementStrings[AchievementID::SkinOfYourTeeth]);
                    break;
                }
            }

            //message for audio director
            auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::RoundEnd;
            msg->score = 0;
        }
        else if (m_statBoardScores.back().client == m_sharedData.clientConnection.connectionID)
        {
            auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::RoundEnd;
            msg->score = 1; //lose
        }
    }

    if (m_holeData.size() > 8) //only consider it a round if there are at least 9 holes
    {
        Achievements::incrementStat(StatStrings[StatID::TotalRounds]);

        if (m_holeData.size() == 18) //set to ALL - which ought to be 18
        {
            Achievements::incrementStat(m_sharedData.mapDirectory);
            Achievements::setAvgStat(m_sharedData.mapDirectory, m_playTime.asSeconds(), 1.f);

            //if we're stroke play see if we get the achievement
            //for coming in under par
            //if (m_sharedData.scoreType == ScoreType::Stroke)
            {
                for (const auto& p : m_sharedData.connectionData[m_sharedData.localConnectionData.connectionID].playerData)
                {
                    if (!p.isCPU)
                    {
                        if (m_sharedData.scoreType == ScoreType::Stroke
                            && p.parScore <= 0)
                        {
                            //course specific
                            if (ParAch.count(m_sharedData.mapDirectory) != 0)
                            {
                                auto id = ParAch.at(m_sharedData.mapDirectory);
                                Achievements::awardAchievement(AchievementStrings[id]);
                            }

                            //any course
                            if (p.parScore < -17)
                            {
                                Achievements::awardAchievement(AchievementStrings[AchievementID::RoadToSuccess]);
                            }
                        }

                        if (m_achievementTracker.alwaysOnTheCourse)
                        {
                            Achievements::awardAchievement(AchievementStrings[AchievementID::ConsistencyIsKey]);
                        }

                        if (m_achievementTracker.noHolesOverPar)
                        {
                            Achievements::awardAchievement(AchievementStrings[AchievementID::NoMistake]);
                        }

                        if (m_achievementTracker.noGimmeUsed)
                        {
                            Achievements::awardAchievement(AchievementStrings[AchievementID::NeverGiveUp]);
                        }

                        if (m_achievementTracker.underTwoPutts)
                        {
                            Achievements::awardAchievement(AchievementStrings[AchievementID::ThreesACrowd]);
                        }

                        if (m_achievementTracker.twoShotsSpare)
                        {
                            Achievements::awardAchievement(AchievementStrings[AchievementID::GreensInRegulation]);
                        }
                    }
                }
            }
        }
    }

    auto trophyCount = std::min(std::size_t(3), m_statBoardScores.size());

    for (auto i = 0u; i < trophyCount; ++i)
    {
        if (m_statBoardScores[i].client == m_sharedData.clientConnection.connectionID
            && !m_sharedData.localConnectionData.playerData[m_statBoardScores[i].player].isCPU)
        {
            if (m_statBoardScores.size() > 1
                && m_holeData.size() > 8)
            {
                Achievements::incrementStat(StatStrings[StatID::GoldWins + i]);

                //only award rank XP if there are players to rank against
                //and reduce XP if < 4 players. Probably ought to consider
                //the opponent's XP too, ie award more if a player wins
                //against someone with a significantly higher level.
                float multiplier = std::min(1.f, static_cast<float>(m_statBoardScores.size()) / 4.f);
                float xp = 0.f;
                std::int32_t xpReason = -1;
                switch (i)
                {
                default: break;
                case 0:
                    xp = static_cast<float>(XPValues[XPID::First]) * multiplier;
                    xpReason = XPStringID::FirstPlace;
                    break;
                case 1:
                    xp = static_cast<float>(XPValues[XPID::Second]) * multiplier;
                    xpReason = XPStringID::SecondPlace;
                    break;
                case 2:
                    xp = static_cast<float>(XPValues[XPID::Third]) * multiplier;
                    xpReason = XPStringID::ThirdPlace;
                    break;
                }
                Social::awardXP(static_cast<std::int32_t>(xp), xpReason);
            }
        }

        m_trophies[i].trophy.getComponent<TrophyDisplay>().state = TrophyDisplay::In;
        //m_trophyLabels[i].getComponent<cro::Callback>().active = true; //this is done by TrophyDisplay (above) to properly delay it
        m_trophies[i].badge.getComponent<cro::SpriteAnimation>().play(std::min(5, m_sharedData.connectionData[m_statBoardScores[i].client].level / 10));
        m_trophies[i].badge.getComponent<cro::Model>().setDoubleSided(0, true);

        m_trophies[i].label.getComponent<cro::Sprite>().setTexture(m_sharedData.nameTextures[m_statBoardScores[i].client].getTexture(), false);
        auto bounds = m_trophies[i].label.getComponent<cro::Sprite>().getTextureBounds();
        bounds.bottom = bounds.height * m_statBoardScores[i].player;
        m_trophies[i].label.getComponent<cro::Sprite>().setTextureRect(bounds);

        //choose the relevant player from the sheet
        bounds = { 0.f, LabelTextureSize.y - (LabelIconSize.x * 4.f), LabelIconSize.x, LabelIconSize.y };
        bounds.left = LabelIconSize.x * (m_statBoardScores[i].player % 2);
        bounds.bottom += LabelIconSize.y * (m_statBoardScores[i].player / 2);
        m_trophies[i].avatar.getComponent<cro::Sprite>().setTextureRect(bounds);
    }


    bool personalBest = false;

#ifndef CRO_DEBUG_
    //enter score into leaderboard
    if (m_sharedData.scoreType == ScoreType::Stroke)
    {
        const auto& connectionData = m_sharedData.connectionData[m_sharedData.clientConnection.connectionID];
        for (auto k = 0u; k < connectionData.playerCount; ++k)
        {
            if (!connectionData.playerData[k].isCPU)
            {
                std::int32_t score = connectionData.playerData[k].score;
                auto best = Social::getPersonalBest(m_sharedData.mapDirectory, m_sharedData.holeCount);

                if (score < best
                    || best == 0)
                {
                    personalBest = true;
                }

                Social::insertScore(m_sharedData.mapDirectory, m_sharedData.holeCount, score);
                cro::Logger::log("LEADERBOARD attempting to insert score: " + std::to_string(connectionData.playerData[k].score), cro::Logger::Type::Info, cro::Logger::Output::File);
                break;
            }
        }
    }
    else
    {
        cro::Logger::log("LEADERBOARD failed to insert score: Score Type is not Stroke.", cro::Logger::Type::Error, cro::Logger::Output::File);
    }
#endif

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 200.f + scoreboardExpansion, 10.f, 0.23f }); //attaches to scoreboard
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 200.f, 10.f - UITextPosV };
    entity.getComponent<UIElement>().depth = 0.23f;
    entity.getComponent<UIElement>().resizeCallback = [](cro::Entity e)
    {
        e.getComponent<cro::Transform>().move({ scoreboardExpansion, 0.f }); 
    };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::uint8_t>>(1.f, seconds);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [current, sec] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::uint8_t>>();
        current -= dt;
        if (current < 0)
        {
            current += 1.f;
            sec--;
        }

        if (m_sharedData.tutorial)
        {
            e.getComponent<cro::Text>().setString("Returning to menu in: " + std::to_string(sec));
        }
        else
        {
            e.getComponent<cro::Text>().setString("Returning to lobby in: " + std::to_string(sec));
        }

        auto bounds = cro::Text::getLocalBounds(e);
        bounds.width = std::floor(bounds.width / 2.f);
        e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
    };

    //attach to the scoreboard
    cmd.targetFlags = CommandID::UI::Scoreboard;
    cmd.action =
        [entity](cro::Entity e, float) mutable
    {
        e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    if (personalBest)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 200.f + scoreboardExpansion, 10.f, 0.8f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<UIElement>().absolutePosition = { 200.f, 150.f };
        entity.getComponent<UIElement>().depth = 2.23f;
        entity.getComponent<UIElement>().resizeCallback = [](cro::Entity e)
        {
            e.getComponent<cro::Transform>().move({ scoreboardExpansion, 0.f });
        };
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
        entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize * 2);
        entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
        //entity.getComponent<cro::Text>().setOutlineColour(LeaderboardTextDark);
        //entity.getComponent<cro::Text>().setOutlineThickness(1.f);
        entity.getComponent<cro::Text>().setString("PERSONAL BEST!");
        centreText(entity);

        cmd.targetFlags = CommandID::UI::Scoreboard;
        cmd.action =
            [entity](cro::Entity e, float) mutable
        {
            e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    }



    //create status icons for each connected client
    //to show vote to skip
    auto unreadyRect = m_sprites[SpriteID::QuitNotReady].getTextureRect();
    auto readyRect = m_sprites[SpriteID::QuitReady].getTextureRect();
    const glm::vec2 texSize(m_sprites[SpriteID::QuitNotReady].getTexture()->getSize());
    if (texSize.x != 0 && texSize.y != 0)
    {
        float posOffset = unreadyRect.width;

        unreadyRect.left /= texSize.x;
        unreadyRect.width /= texSize.x;
        unreadyRect.bottom /= texSize.y;
        unreadyRect.height /= texSize.y;

        readyRect.left /= texSize.x;
        readyRect.width /= texSize.x;
        readyRect.bottom /= texSize.y;
        readyRect.height /= texSize.y;

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Drawable2D>().setTexture(m_sprites[SpriteID::QuitNotReady].getTexture());
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, readyRect, unreadyRect, posOffset](cro::Entity e, float)
        {
            auto& tx = e.getComponent<cro::Transform>();
            tx.setPosition({ 13.f, UIBarHeight + 10.f, 2.f });
            tx.setScale(m_viewScale);

            float basePos = 0.f;
            std::vector<cro::Vertex2D> vertices;
            for (auto i = 0u; i < 4u; ++i)
            {
                if (m_sharedData.connectionData[i].playerCount)
                {
                    //check status flags to choose rectangle
                    auto rect = (m_readyQuitFlags & (1 << i)) ? readyRect : unreadyRect;

                    
                    vertices.emplace_back(glm::vec2(basePos, posOffset), glm::vec2(rect.left, rect.bottom + rect.height));
                    vertices.emplace_back(glm::vec2(basePos, 0.f), glm::vec2(rect.left, rect.bottom));
                    vertices.emplace_back(glm::vec2(basePos + posOffset, posOffset), glm::vec2(rect.left + rect.width, rect.bottom + rect.height));
                    vertices.emplace_back(glm::vec2(basePos + posOffset, 0.f), glm::vec2(rect.left + rect.width, rect.bottom));                    

                    vertices.emplace_back(glm::vec2(basePos + posOffset, posOffset), glm::vec2(rect.left + rect.width, rect.bottom + rect.height), cro::Colour::Transparent);
                    vertices.emplace_back(glm::vec2(basePos + posOffset, 0.f), glm::vec2(rect.left + rect.width, rect.bottom), cro::Colour::Transparent);

                    basePos += posOffset + 2.f;

                    vertices.emplace_back(glm::vec2(basePos, posOffset), glm::vec2(rect.left, rect.bottom + rect.height), cro::Colour::Transparent);
                    vertices.emplace_back(glm::vec2(basePos, 0.f), glm::vec2(rect.left, rect.bottom), cro::Colour::Transparent);
                }
            }
            e.getComponent<cro::Drawable2D>().setVertexData(vertices);
        };
    }


    if (m_drone.isValid() &&
        cro::Util::Random::value(0, 2) == 0)
    {
        float radius = glm::length(m_holeData[m_currentHole].pin - m_cameras[CameraID::Player].getComponent<cro::Transform>().getWorldPosition()) * 0.85f;
        auto targetEnt = m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>().target;

        //set a callback that makes the camera orbit the flag - and hence the drone follows
        targetEnt.getComponent<cro::Callback>().function =
            [&, radius](cro::Entity e, float dt)
        {
            static float elapsed = 0.f;
            elapsed += dt;

            auto basePos = m_holeData[m_currentHole].pin;
            basePos.x += std::sin(elapsed) * radius;
            basePos.z += std::cos(elapsed) * radius;
            basePos.y += 0.7f;
            e.getComponent<cro::Transform>().setPosition(basePos);
        };
    }


    refreshUI();
}

void GolfState::createScoreboard()
{
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/scoreboard.spt", m_resources.textures);

    m_sprites[SpriteID::QuitReady] = spriteSheet.getSprite("quit_ready");
    m_sprites[SpriteID::QuitNotReady] = spriteSheet.getSprite("quit_not_ready");

    auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    size.x /= 2.f;
    size.y -= size.y / 2.f;

    auto rootEnt = m_uiScene.createEntity();
    rootEnt.addComponent<cro::Transform>().setPosition({ size.x, -size.y });
    rootEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::ScoreboardController;
    //use the callback to keep the board centred/scaled
    rootEnt.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        static constexpr float Speed = 10.f;

        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        auto target = glm::vec3(size / 2.f, 0.22f);
        target.y -= e.getComponent<cro::Callback>().getUserData<std::int32_t>() * size.y;

        auto pos = e.getComponent<cro::Transform>().getPosition();
        pos += (target - pos) * dt * Speed;

        e.getComponent<cro::Transform>().setPosition(pos);
        e.getComponent<cro::Transform>().setScale(m_viewScale);
    };

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::Scoreboard | CommandID::UI::UIElement;
    auto bgSprite = spriteSheet.getSprite("border");
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -12.f };
    entity.getComponent<UIElement>().resizeCallback =
        [&,bgSprite](cro::Entity e)
    {
        auto baseSize = glm::vec2(cro::App::getWindow().getSize()) / m_viewScale;
        scoreboardExpansion = stretchToScreen(e, bgSprite, baseSize);
        auto bounds = e.getComponent<cro::Drawable2D>().getLocalBounds();
        e.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    };
    entity.getComponent<UIElement>().resizeCallback(entity);
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    auto bounds = entity.getComponent<cro::Drawable2D>().getLocalBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;
    m_scoreboardEnt = entity;

    auto resizeCentre =
        [](cro::Entity e)
    {
        e.getComponent<cro::Transform>().move({ scoreboardExpansion, 0.f });
    };

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().absolutePosition = { 200.f, 281.f };
    entity.getComponent<UIElement>().depth = 0.5f;
    entity.getComponent<UIElement>().resizeCallback = resizeCentre;
    entity.addComponent<cro::Text>(font).setString("LEADERS");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    centreText(entity);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    if (!m_courseTitle.empty())
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
        entity.addComponent<UIElement>().absolutePosition = { 200.f, 11.f };
        entity.getComponent<UIElement>().depth = 0.5f;
        entity.getComponent<UIElement>().resizeCallback = resizeCentre;
        entity.addComponent<cro::Text>(font).setString(m_courseTitle);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
        centreText(entity);
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    auto connectionCount = 0;
    for (auto i = 1u; i < m_sharedData.connectionData.size(); ++i)
    {
        connectionCount += m_sharedData.connectionData[i].playerCount;
    }
    if (connectionCount != 0)
    {
        auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 200.f + scoreboardExpansion, 10.f, 0.5f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::WaitMessage | CommandID::UI::UIElement;
        entity.addComponent<UIElement>().absolutePosition = { 200.f, 10.f - UITextPosV };
        entity.getComponent<UIElement>().relativePosition = { 0.f, 0.f };
        entity.getComponent<UIElement>().depth = 0.5f;
        entity.getComponent<UIElement>().resizeCallback = [](cro::Entity e)
        {
            e.getComponent<cro::Transform>().move({ scoreboardExpansion, 0.f });
        };
        entity.addComponent<cro::Text>(smallFont).setString("Waiting For Other Players");
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        bounds = cro::Text::getLocalBounds(entity);
        bounds.width = std::floor(bounds.width / 2.f);
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        entity.addComponent<cro::Callback>().setUserData<float>(0.f);
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::min(1.f, currTime + (dt * 2.f));

            auto scale = e.getComponent<cro::Transform>().getScale();
            scale.x = 0.8f + (0.2f * cro::Util::Easing::easeOutElastic(currTime));
            scale.y = 1.f;
            e.getComponent<cro::Transform>().setScale(scale);

            if (currTime == 1)
            {
                currTime = 0.f;
                e.getComponent<cro::Callback>().active = false;
            }
        };
    }

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bgSprite = spriteSheet.getSprite("background");
    entity.addComponent<UIElement>().absolutePosition = { 6.f, -265.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().resizeCallback =
        [&, bgSprite](cro::Entity e)
    {
        auto baseSize = glm::vec2(cro::App::getWindow().getSize()) / m_viewScale;
        stretchToScreen(e, bgSprite, baseSize);
        
        //refreshes the cropping area
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::ScoreScroll;
        cmd.action = [](cro::Entity f, float)
        {
            f.getComponent<cro::Callback>().getUserData<std::int32_t>() = 0;
            f.getComponent<cro::Callback>().active = true;
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };
    entity.getComponent<UIElement>().resizeCallback(entity);

    bgSprite = spriteSheet.getSprite("board");
    m_leaderboardTexture.init(bgSprite, font);

    cro::FloatRect bgCrop = bgSprite.getTextureBounds();
    bgCrop.bottom += bgCrop.height;

    auto scrollEnt = m_uiScene.createEntity();
    scrollEnt.addComponent<cro::Transform>();
    scrollEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::ScoreScroll;
    scrollEnt.addComponent<cro::Callback>().setUserData<std::int32_t>(0); //set to the number of steps to scroll
    scrollEnt.getComponent<cro::Callback>().function =
        [bgEnt, entity, bgCrop](cro::Entity e, float) mutable
    {
        auto& steps = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
        static constexpr float StepSize = 14.f;
        static constexpr float MaxMove = StepSize * 19.f;

        auto move = steps * StepSize;
        auto pos = e.getComponent<cro::Transform>().getPosition();
        pos.y = std::min(MaxMove, std::max(0.f, pos.y + move));

        e.getComponent<cro::Transform>().setPosition(pos);
        e.getComponent<cro::Callback>().active = false;
        steps = 0; //this is a reference, don't delete it...

        //update the cropping
        const auto& ents = bgEnt.getComponent<cro::Callback>().getUserData<std::vector<cro::Entity>>();
        for (auto ent : ents)
        {
            auto crop = cro::Text::getLocalBounds(ent);
            crop.width = std::min(crop.width, MinLobbyCropWidth + 16.f + scoreboardExpansion);
            crop.height = bgCrop.height;
            crop.height += 1.f;
            crop.bottom = -(bgCrop.height - 1.f) - pos.y;
            ent.getComponent<cro::Drawable2D>().setCroppingArea(crop);
        }
        //TODO these values need to be rounded to
        //the nearest scaled pixel ie nearest 2,3 or whatever viewScale is
        auto crop = bgCrop;
        crop.width += (scoreboardExpansion * 2.f);
        crop.bottom -= pos.y;
        entity.getComponent<cro::Drawable2D>().setCroppingArea(crop);
    };
    scrollEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    bgEnt.getComponent<cro::Transform>().addChild(scrollEnt.getComponent<cro::Transform>());

    //these have the text components on them, the callback updates scroll cropping
    bgEnt.addComponent<cro::Callback>().setUserData<std::vector<cro::Entity>>();

    //m_scoreColumnCount = 11;
    m_scoreColumnCount = std::min(m_holeData.size() + m_scoreColumnCount, std::size_t(11));

    auto& ents = bgEnt.getComponent<cro::Callback>().getUserData<std::vector<cro::Entity>>();
    ents.resize(m_scoreColumnCount); //title and total
    std::int32_t i = 0;
    for (auto& e : ents)
    {
        e = m_uiScene.createEntity();
        e.addComponent<cro::Transform>().setPosition(glm::vec3(ColumnPositions[i], 1.3f));
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
        e.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
        e.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);

        if (i > 0)
        {
            //UITextPosV gets added by the camera resize callback...
            e.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
            e.addComponent<UIElement>().absolutePosition = ColumnPositions[i] - glm::vec2(0.f, UITextPosV);
            e.getComponent<UIElement>().depth = 1.3f;
            e.getComponent<UIElement>().resizeCallback =
                [&](cro::Entity ent)
            {
                //gotta admit - I don't know why this works.
                if (scoreboardExpansion > 0)
                {
                    float offset = scoreboardExpansion == MaxExpansion ? std::floor(ColumnMargin) : 0.f;
                    ent.getComponent<cro::Transform>().move({ std::floor(scoreboardExpansion - offset), 0.f});
                }
            };
        }

        scrollEnt.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
        i++;
    }
    ents.back().getComponent<cro::Transform>().setPosition(glm::vec3(ColumnPositions.back(), 0.5f));
    ents.back().getComponent<UIElement>().absolutePosition = ColumnPositions.back() - glm::vec2(0.f, UITextPosV);


    //net strength icons
    glm::vec3 iconPos(8.f, 235.f, 2.2f);
    static constexpr float IconSpacing = 14.f;
    for (const auto& c : m_sharedData.connectionData)
    {
        for (auto j = 0u; j < c.playerCount; ++j)
        {
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(iconPos);
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("strength_meter");
            entity.addComponent<cro::SpriteAnimation>();
            entity.addComponent<cro::Callback>().setUserData<std::pair<std::uint8_t, std::uint8_t>>(c.connectionID, j);
            entity.getComponent<cro::Callback>().function =
                [&, bgEnt](cro::Entity e, float)
            {
                auto [client, player] = e.getComponent<cro::Callback>().getUserData<std::pair<std::uint8_t, std::uint8_t>>();

                if (m_sharedData.connectionData[client].playerData[player].isCPU)
                {
                    e.getComponent<cro::SpriteAnimation>().play(5);
                }
                else
                {
                    auto idx = m_sharedData.connectionData[client].pingTime / 30;
                    e.getComponent<cro::SpriteAnimation>().play(std::min(4u, idx));
                }
            };
            bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_netStrengthIcons.push_back(entity);

            iconPos.y -= IconSpacing;
        }
    }

    updateScoreboard();
}

void GolfState::updateScoreboard()
{
    struct ScoreEntry final
    {
        cro::String name;
        std::vector<std::uint8_t> holes;
        std::int32_t frontNine = 0;
        std::int32_t backNine = 0;
        std::int32_t total = 0;
        std::int32_t parDiff = 0;
        std::uint8_t client = 0;
        std::uint8_t player = 0;
    };

    std::vector<ScoreEntry> scores;
    m_statBoardScores.clear();

    std::uint32_t playerCount = 0;
    auto holeCount = m_holeData.size();
    std::uint8_t clientID = 0;
    for (auto& client : m_sharedData.connectionData)
    {
        playerCount += client.playerCount;

        for (auto i = 0u; i < client.playerCount; ++i)
        {
            auto& entry = scores.emplace_back();
            entry.name = client.playerData[i].name;
            entry.client = clientID;
            entry.player = i;

            bool overPar = false;

            for (auto j = 0u; j < client.playerData[i].holeScores.size(); ++j)
            {
                auto s = client.playerData[i].holeScores[j];
                entry.holes.push_back(s);

                if (s)
                {
                    auto diff = static_cast<std::int32_t>(s) - m_holeData[j].par;
                    entry.parDiff += diff;

                    overPar = (diff > 0);
                }

                if (j < 9)
                {
                    if (m_sharedData.scoreType == ScoreType::Stroke)
                    {
                        entry.frontNine += client.playerData[i].holeScores[j];
                    }
                    else if (m_sharedData.scoreType == ScoreType::Match)
                    {
                        entry.frontNine = client.playerData[i].matchScore;
                    }
                    else
                    {
                        entry.frontNine = client.playerData[i].skinScore;
                    }
                }
                else
                {
                    if (m_sharedData.scoreType == ScoreType::Stroke)
                    {
                        entry.backNine += client.playerData[i].holeScores[j];
                    }
                    else if (m_sharedData.scoreType == ScoreType::Match)
                    {
                        entry.backNine = client.playerData[i].matchScore;
                    }
                    else
                    {
                        entry.backNine = client.playerData[i].skinScore;
                    }
                }
            }
            client.playerData[i].parScore = entry.parDiff;

            
            switch (m_sharedData.scoreType)
            {
            default:
            case ScoreType::Stroke:
                entry.total = entry.frontNine + entry.backNine;

                //track achievement make no mistake
                if (client.connectionID == m_sharedData.localConnectionData.connectionID
                    && !client.playerData[i].isCPU
                    && overPar)
                {
                    m_achievementTracker.noHolesOverPar = false;
                }

                break;
            case ScoreType::Skins:
                entry.total = client.playerData[i].skinScore;
                break;
            case ScoreType::Match:
                //entry.total = entry.frontNine;
                entry.total = client.playerData[i].matchScore;
                break;
            }

            //for stat/achievment tracking
            auto& leaderboardEntry = m_statBoardScores.emplace_back();
            leaderboardEntry.client = clientID;
            leaderboardEntry.player = i;
            leaderboardEntry.score = entry.total;
        }
        clientID++;
    }

    //tracks stats and decides on trophy layout on round end (see showCountdown())
    std::sort(m_statBoardScores.begin(), m_statBoardScores.end(),
        [&](const StatBoardEntry& a, const StatBoardEntry& b)
        {
            if (m_sharedData.scoreType == ScoreType::Stroke)
            {
                return a.score < b.score;
            }
            return b.score < a.score;
        });
    //LOG("Table Update", cro::Logger::Type::Info);

    auto& ents = m_scoreboardEnt.getComponent<cro::Callback>().getUserData<std::vector<cro::Entity>>();
    std::sort(scores.begin(), scores.end(),
        [&](const ScoreEntry& a, const ScoreEntry& b)
        {
            switch (m_sharedData.scoreType)
            {
            default:
            case ScoreType::Stroke:
                return a.total < b.total;
            case ScoreType::Skins:
            case ScoreType::Match:
                return b.total < a.total;
            }
        });


    std::size_t page2 = 0;
    static constexpr std::size_t MaxCols = 9;
    if (holeCount > m_scoreColumnCount)
    {
        page2 = std::min(MaxCols, holeCount - m_scoreColumnCount);
    }

    //store the strings to update the leaderboard texture
    std::vector<LeaderboardEntry> leaderboardEntries;

    //name column
    cro::String nameString = "HOLE\nPAR";
    for (auto i = 0u; i < playerCount; ++i)
    {
        nameString += "\n  " + scores[i].name.substr(0, ConstVal::MaxStringChars);
        m_netStrengthIcons[i].getComponent<cro::Callback>().getUserData<std::pair<std::uint8_t, std::uint8_t>>()
            = std::make_pair(scores[i].client, scores[i].player);
    }
    if (page2)
    {
        //pad out for page 2
        for (auto i = 0u; i < 16u - playerCount; ++i)
        {
            nameString += "\n";
        }

        nameString += "\n\nHOLE\nPAR";
        for (auto i = 0u; i < playerCount; ++i)
        {
            nameString += "\n  " + scores[i].name.substr(0, ConstVal::MaxStringChars);
        }
    }
    ents[0].getComponent<cro::Text>().setString(nameString);
    leaderboardEntries.emplace_back(ents[0].getComponent<cro::Transform>().getPosition() + glm::vec3(2.f, 0.f, 0.f), nameString);

    //score columns
    for (auto i = 1u; i < ents.size() - 1; ++i)
    {
        auto holeNumber = i;
        if (m_sharedData.holeCount == 2)
        {
            holeNumber += 9;
        }
        std::string scoreString = std::to_string(holeNumber) + "\n" + std::to_string(m_holeData[i - 1].par);

        for (auto j = 0u; j < playerCount; ++j)
        {
            scoreString += "\n";

            auto s = scores[j].holes[i - 1];
            if (s)
            {
                scoreString += std::to_string(s);
            }
        }

        if (page2)
        {
            for (auto j = 0u; j < 16 - playerCount; ++j)
            {
                scoreString += "\n";
            }

            auto holeIndex = (i + MaxCols) - 1;
            if (holeIndex < m_holeData.size())
            {
                scoreString += "\n\n" + std::to_string(i + MaxCols) + "\n" + std::to_string(m_holeData[holeIndex].par);
                for (auto j = 0u; j < playerCount; ++j)
                {
                    scoreString += "\n";
                    auto s = scores[j].holes[holeIndex];
                    if (s)
                    {
                        scoreString += std::to_string(s);
                    }
                }
            }
        }

        ents[i].getComponent<cro::Text>().setString(scoreString);
        leaderboardEntries.emplace_back(glm::vec3(ents[i].getComponent<UIElement>().absolutePosition - glm::vec2(ColumnMargin, -UITextPosV), 0.f), scoreString);
    }

    //total column
    std::int32_t par = 0;
    for (auto i = 0u; i < MaxCols && i < m_holeData.size(); ++i)
    {
        par += m_holeData[i].par;
    }

    std::string totalString = "TOTAL\n" + std::to_string(par);

    for (auto i = 0u; i < playerCount; ++i)
    {
        totalString += "\n" + std::to_string(scores[i].frontNine);

        switch (m_sharedData.scoreType)
        {
        default:
        case ScoreType::Stroke:
            if (scores[i].parDiff > 0)
            {
                totalString += " (+" + std::to_string(scores[i].parDiff) + ")";
            }
            else if (scores[i].parDiff < 0)
            {
                totalString += " (" + std::to_string(scores[i].parDiff) + ")";
            }
            else
            {
                totalString += " (0)";
            }
            break;
        case ScoreType::Match:
            totalString += " POINTS";
            break;
        case ScoreType::Skins:
            totalString += " SKINS";
            break;
        }
    }

    //pad out for page 2
    for (auto i = 0u; i < 16u - playerCount; ++i)
    {
        totalString += "\n";
    }

    if (page2)
    {
        const auto getSeparator =
            [](std::int32_t first)
        {
            std::string str;
            if (first < 10)
            {
                str += " ";
            }
            str += " - ";

            return str;
        };

        auto frontPar = par;
        par = 0;
        for (auto i = MaxCols; i < m_holeData.size(); ++i)
        {
            par += m_holeData[i].par;
        }
        auto separator = getSeparator(par);

        totalString += "\n\nTOTAL\n" + std::to_string(par) + separator + std::to_string(par + frontPar);
        for (auto i = 0u; i < playerCount; ++i)
        {
            separator = getSeparator(scores[i].backNine);
            totalString += "\n" + std::to_string(scores[i].backNine);

            switch (m_sharedData.scoreType)
            {
            default:
            case ScoreType::Stroke:
                totalString += separator + std::to_string(scores[i].total);
                if (scores[i].parDiff > 0)
                {
                    totalString += " (+" + std::to_string(scores[i].parDiff) + ")";
                }
                else if (scores[i].parDiff < 0)
                {
                    totalString += " (" + std::to_string(scores[i].parDiff) + ")";
                }
                else
                {
                    totalString += " (0)";
                }
                break;
            case ScoreType::Match:
                totalString += " POINTS";
                break;
            case ScoreType::Skins:
                totalString += " SKINS";
                break;
            }
        }
    }

    ents.back().getComponent<cro::Text>().setString(totalString);
    ents.back().getComponent<cro::Transform>().setPosition(ColumnPositions.back());
    //gotta admit - I don't know why this works.
    if (scoreboardExpansion > 0)
    {
        float offset = scoreboardExpansion == MaxExpansion ? std::floor(ColumnMargin) : 0.f;
        ents.back().getComponent<cro::Transform>().move({ std::floor(scoreboardExpansion - offset), 0.f });
    }
    leaderboardEntries.emplace_back(glm::vec3(ents.back().getComponent<UIElement>().absolutePosition - glm::vec2(ColumnMargin, -UITextPosV), 0.f), totalString);

    //for some reason we have to hack this to display and I'm too lazy to debug it
    auto pos = ents.back().getComponent<cro::Transform>().getPosition();
    pos.z = 1.5f;
    ents.back().getComponent<cro::Transform>().setPosition(pos);

    m_leaderboardTexture.update(leaderboardEntries);
}

void GolfState::showScoreboard(bool visible)
{
    for (auto e : m_netStrengthIcons)
    {
        e.getComponent<cro::Callback>().active = visible;
    }

    if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
    {
        if (m_inputParser.inProgress())
        {
            return;
        }

        //disable the input while the score is shown
        m_inputParser.setSuspended(visible);
    }

    //don't hide if the round finished
    if (m_roundEnded
        || m_newHole)
    {
        visible = true;
    }

    auto target = visible ? 0 : 1; //when 1 board is moved 1x screen size from centre

    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::ScoreboardController;
    cmd.action = [target](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().active = true;
        e.getComponent<cro::Callback>().setUserData<std::int32_t>(target);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    std::int32_t step = -19;
    if (m_currentHole > 8)
    {
        //scroll to lower part of the board
        step = 19;
    }

    cmd.targetFlags = CommandID::UI::ScoreScroll;
    cmd.action = [step](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().getUserData<std::int32_t>() = step;
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    if (!visible)
    {
        //hide message
        cmd.targetFlags = CommandID::UI::WaitMessage;
        cmd.action =
            [&](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setScale({ 1.f, 0.f });
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    }
}

void GolfState::updateWindDisplay(glm::vec3 direction)
{
    float rotation = std::atan2(-direction.z, direction.x);
    m_windUpdate.windVector = direction;

    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::WindSock;
    cmd.action = [&, rotation](cro::Entity e, float dt)
    {
        auto camRotation = m_camRotation;
        if (m_currentCamera != CameraID::Player)
        {
            //set the rotation relative to the active cam
            auto vec = m_cameras[m_currentCamera].getComponent<cro::Transform>().getForwardVector();
            camRotation = std::atan2(-vec.z, vec.x);
        }

        auto r = rotation - camRotation;

        float& currRotation = e.getComponent<float>();
        currRotation += cro::Util::Maths::shortestRotation(currRotation, r) * (dt * 4.f);
        e.getComponent<cro::Transform>().setRotation(currRotation);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::WindString;
    cmd.action = [&,direction](cro::Entity e, float dt)
    {
        float knots = direction.y * KnotsPerMetre; //use this anyway to set the colour :3
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
}

float GolfState::estimatePuttPower()
{
    auto maxDist = Clubs[ClubID::Putter].getTarget(m_distanceToHole);
    float guestimation = (m_distanceToHole / maxDist);

    //kludge stops the flag recommending too much power            
    if (maxDist == Clubs[ClubID::Putter].getBaseTarget())
    {
        //guestimation = cro::Util::Easing::easeInSine(guestimation);
        guestimation *= 0.83f;
    }
    else
    {
        guestimation = std::min(1.f, guestimation + 0.12f);
    }

    //add a bit more power if putting uphill
    float slope = 0.f;
    if (m_distanceToHole > 0.005f)
    {
        slope = glm::dot(cro::Transform::Y_AXIS, m_holeData[m_currentHole].pin - m_currentPlayer.position) / m_distanceToHole;
    }
    return std::clamp(guestimation + (0.25f * slope), 0.f, 1.f);
}

void GolfState::showMessageBoard(MessageBoardID messageType, bool special)
{
    auto bounds = m_sprites[SpriteID::MessageBoard].getTextureBounds();
    auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    auto position = glm::vec3(size.x / 2.f, (size.y / 2.f) + UIBarHeight * m_viewScale.y, 0.05f);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::MessageBoard];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MessageBoard;


    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 56.f, 0.02f });
    textEnt.addComponent<cro::Drawable2D>();
    textEnt.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    textEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);

    auto textEnt2 = m_uiScene.createEntity();
    textEnt2.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 26.f, 0.02f });
    textEnt2.addComponent<cro::Drawable2D>();
    textEnt2.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    textEnt2.getComponent<cro::Text>().setFillColour(TextNormalColour);

    auto textEnt3 = m_uiScene.createEntity();
    textEnt3.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 41.f, 0.02f });
    textEnt3.addComponent<cro::Drawable2D>();
    textEnt3.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    textEnt3.getComponent<cro::Text>().setFillColour(TextNormalColour);

    //add mini graphic depending on message type
    auto imgEnt = m_uiScene.createEntity();
    imgEnt.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, 0.01f });
    imgEnt.getComponent<cro::Transform>().move(glm::vec2(0.f, -6.f));
    imgEnt.addComponent<cro::Drawable2D>();

    switch (messageType)
    {
    default: break;
    case MessageBoardID::HoleScore:
    case MessageBoardID::Gimme:
    {
        std::int32_t score = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole];
        auto overPar = score - m_holeData[m_currentHole].par;
      
        //if this is a remote player the score won't
        //have arrived yet, so kludge this here so the
        //display type is correct.
        if (m_currentPlayer.client != m_sharedData.clientConnection.connectionID)
        {
            score++;
        }

        if (score > 1)
        {
            score -= m_holeData[m_currentHole].par;
            score += ScoreID::ScoreOffset;
        }
        else
        {
            //hio is also technically an eagle or birdie
            //etc, so we need to differentiate
            score = ScoreID::HIO;
            auto xp = XPValues[XPID::HIO];
            if (m_holeData[m_currentHole].puttFromTee)
            {
                xp /= 5;
            }

            if (m_sharedData.showPuttingPower)
            {
                xp /= 2;
            }

            Social::awardXP(xp, XPStringID::HIO);
        }


        //if this is a local player update achievements
        if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
        {
            if (m_achievementTracker.hadFoul && overPar < 1)
            {
                Achievements::awardAchievement(AchievementStrings[AchievementID::Boomerang]);
            }

            switch (score)
            {
            default: break;
            case ScoreID::Birdie:
                Achievements::incrementStat(StatStrings[StatID::Birdies]);
                break;
            case ScoreID::Eagle:
                Achievements::incrementStat(StatStrings[StatID::Eagles]);
                Achievements::awardAchievement(AchievementStrings[AchievementID::Soaring]);
                break;
            case ScoreID::HIO:
                Achievements::incrementStat(StatStrings[StatID::HIOs]);
                Achievements::awardAchievement(AchievementStrings[AchievementID::HoleInOne]);
                break;
            }
        }

        if (messageType == MessageBoardID::HoleScore)
        {
            //this triggers the VO which we only want if it went in the hole.
            auto* msg = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::Scored;
            msg->score = static_cast<std::uint8_t>(score);
            msg->travelDistance = glm::length2(m_holeData[m_currentHole].pin - m_currentPlayer.position);
            msg->club = getClub();

            if (score <= ScoreID::Par
                && m_sharedData.showBeacon)
            {
                //display ring animation
                cro::Command cmd;
                cmd.targetFlags = CommandID::HoleRing;
                cmd.action = [](cro::Entity e, float)
                {
                    e.getComponent<cro::Callback>().active = true;
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }
        }


        if (score < ScoreID::Count)
        {
            textEnt.getComponent<cro::Text>().setString(ScoreStrings[score]);
            textEnt.getComponent<cro::Transform>().move({ 0.f, -10.f, 0.f });

            std::int32_t divisor = (m_sharedData.showPuttingPower && getClub() == ClubID::Putter) ? 2 : 1;

            switch (score)
            {
            default: break;
            case ScoreID::Albatross:
                Social::awardXP(XPValues[XPID::Albatross] / divisor, XPStringID::Albatross + (divisor - 1));
                break;
            case ScoreID::Eagle:
                Social::awardXP(XPValues[XPID::Eagle] / divisor, XPStringID::Eagle + (divisor - 1));
                break;
            case ScoreID::Birdie:
                Social::awardXP(XPValues[XPID::Birdie] / divisor, XPStringID::Birdie + (divisor - 1));
                break;
            case ScoreID::Par:
                Social::awardXP(XPValues[XPID::Par] / divisor, XPStringID::Par + (divisor - 1));
                break;
            }

            if (special)
            {
                textEnt3.getComponent<cro::Text>().setString("Nice Putt!");
                textEnt3.getComponent<cro::Text>().setFillColour(TextGoldColour);
                textEnt3.getComponent<cro::Transform>().move({ 0.f, -8.f });

                textEnt.getComponent<cro::Transform>().move({ 0.f, 2.f, 0.f });
            }
        }
        else
        {
            textEnt.getComponent<cro::Text>().setString("Bad Luck!");
            textEnt2.getComponent<cro::Text>().setString(std::to_string(overPar) + " Over Par");
        }
    }
    break;
    case MessageBoardID::Bunker:
        textEnt.getComponent<cro::Text>().setString("Bunker!");
        imgEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::Bunker];
        bounds = m_sprites[SpriteID::Bunker].getTextureBounds();
        break;
    case MessageBoardID::PlayerName:
        textEnt.getComponent<cro::Text>().setString(m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].name.substr(0, 19));
        textEnt.getComponent<cro::Text>().setFillColour(TextGoldColour);
        textEnt2.getComponent<cro::Text>().setString("Stroke: " + std::to_string(m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole] + 1));
        textEnt3.getComponent<cro::Text>().setString(ScoreTypes[m_sharedData.scoreType]);
        break;
    case MessageBoardID::Scrub:
    case MessageBoardID::Water:
        textEnt.getComponent<cro::Text>().setString("Foul!");
        textEnt.getComponent<cro::Text>().setFillColour(TextGoldColour);
        textEnt2.getComponent<cro::Text>().setString("1 Stroke Penalty");
        imgEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::Foul];
        bounds = m_sprites[SpriteID::Foul].getTextureBounds();
        break;
    }
    imgEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, 0.f });

    centreText(textEnt);
    centreText(textEnt2);
    centreText(textEnt3);
    
    entity.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(textEnt2.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(textEnt3.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(imgEnt.getComponent<cro::Transform>());

    //callback for anim/self destruction
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<MessageAnim>();
    entity.getComponent<cro::Callback>().function =
        [&, textEnt, textEnt2, textEnt3, imgEnt, messageType](cro::Entity e, float dt)
    {
        static constexpr float HoldTime = 2.f;
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
            }
            break;
        case MessageAnim::Hold:
            //hold
            currTime = std::min(HoldTime, currTime + dt);
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

                if (messageType == MessageBoardID::PlayerName)
                {
                    //this assumes it was raised by an event
                    //from requestNextPlayer
                    setCurrentPlayer(m_currentPlayer);
                }
            }
            break;
        }
    };


    //send a message to immediately close any current open messages
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::MessageBoard;
    cmd.action = [entity](cro::Entity e, float)
    {
        if (e != entity)
        {
            auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<MessageAnim>();
            if (state != MessageAnim::Close)
            {
                currTime = 1.f;
                state = MessageAnim::Close;
            }
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void GolfState::floatingMessage(const std::string& msg)
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

    entity.addComponent<FloatingText>().basePos = position;
}

void GolfState::createTransition()
{
    glm::vec2 screenSize(cro::App::getWindow().getSize());
    auto& shader = m_resources.shaders.get(ShaderID::Transition);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 2.f });
    entity.addComponent<cro::Drawable2D>().setShader(&shader);
    entity.getComponent<cro::Drawable2D>().setVertexData(
    {
        cro::Vertex2D(glm::vec2(0.f, screenSize.y), glm::vec2(0.f, 1.f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f), cro::Colour::Black),
        cro::Vertex2D(screenSize, glm::vec2(1.f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(screenSize.x, 0.f), glm::vec2(1.f, 0.f), cro::Colour::Black)
    });

    auto timeID = shader.getUniformID("u_time");
    auto shaderID = shader.getGLHandle();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, shaderID, timeID](cro::Entity e, float dt)
    {
        static constexpr float MaxTime = 2.f - (1.f/60.f);
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(MaxTime, currTime + dt);

        glCheck(glUseProgram(shaderID));
        glCheck(glUniform1f(timeID, currTime));

        if (currTime == MaxTime)
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }
    };

    glCheck(glUseProgram(shader.getGLHandle()));
    glCheck(glUniform2f(shader.getUniformID("u_scale"), m_viewScale.x, m_viewScale.y));
    glCheck(glUniform2f(shader.getUniformID("u_resolution"), screenSize.x, screenSize.y));
}

void GolfState::notifyAchievement(const std::array<std::uint8_t, 2u>& data)
{
    //only notify if someone else
    if (m_sharedData.localConnectionData.connectionID != data[0])
    {
        //this came off the network so better validate it a bit...
        if (data[0] < ConstVal::MaxClients
            && m_sharedData.connectionData[data[0]].playerCount != 0
            && data[1] < AchievementID::Count - 1)
        {
            auto name = m_sharedData.connectionData[data[0]].playerData[0].name;
            auto achievement = AchievementLabels[data[1]];

            showNotification(name + " achieved " + achievement);

            auto* msg = postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
            msg->type = Social::SocialEvent::PlayerAchievement;
            msg->level = 0; //cheer
        }
    }
}

void GolfState::showNotification(const cro::String& msg)
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f * m_viewScale.x, UIBarHeight * m_viewScale.y * 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::UI));
    entity.getComponent<cro::Text>().setCharacterSize(8u * static_cast<std::uint32_t>(m_viewScale.y));
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<Notification>().message = msg;
}

void GolfState::showLevelUp(std::uint64_t levelData)
{
    std::int32_t level =   (levelData & 0x00000000FFFFFFFF);
    std::int32_t player = ((levelData & 0x000000FF00000000) >> 32);
    std::int32_t client = ((levelData & 0x0000FF0000000000) >> 40);

    m_sharedData.connectionData[client].level = std::uint8_t(level);

    cro::String msg = m_sharedData.connectionData[client].playerData[player].name;
    msg += " has reached level " + std::to_string(level);
    showNotification(msg);
}

void GolfState::toggleQuitReady()
{
    if (m_roundEnded
        || m_newHole)
    {
        m_sharedData.clientConnection.netClient.sendPacket<std::uint8_t>(PacketID::ReadyQuit, m_sharedData.clientConnection.connectionID, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

        if (m_newHole)
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::WaitMessage;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().active = true;
                e.getComponent<cro::Callback>().getUserData<float>() = 0.f;
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
    }
}

void GolfState::updateSkipMessage(float dt)
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
                        if (cro::GameController::hasPSLayout(activeControllerID(m_currentPlayer.player)))
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
                || cro::GameController::isButtonPressed(activeControllerID(m_currentPlayer.player), m_sharedData.inputBinding.buttons[InputBinding::Action]))
            {
                m_skipState.currentTime = std::min(SkipState::SkipTime, m_skipState.currentTime + dt);
                if (m_skipState.currentTime == SkipState::SkipTime)
                {
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::SkipTurn, m_sharedData.localConnectionData.connectionID, net::NetFlag::Reliable);
                    m_skipState.wasSkipped = true;

                    //hide message
                    cro::Command cmd;
                    cmd.targetFlags = CommandID::UI::FastForward;
                    cmd.action = [&](cro::Entity e, float)
                    {
                        e.getComponent<cro::Callback>().getUserData<SkipCallbackData>().direction = 0;
                        e.getComponent<cro::Callback>().active = true;
                    };
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                    //switch to sky cam after slight delay
                    /*auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Callback>().active = true;
                    entity.getComponent<cro::Callback>().setUserData<float>(1.f);
                    entity.getComponent<cro::Callback>().function =
                        [&](cro::Entity ent, float dt)
                    {
                        auto& currTime = ent.getComponent<cro::Callback>().getUserData<float>();
                        currTime -= dt;

                        if (currTime < 0)
                        {
                            if (m_currentCamera == CameraID::Player
                                || m_currentCamera == CameraID::Bystander)
                            {
                                setActiveCamera(CameraID::Sky);
                            }
                            ent.getComponent<cro::Callback>().active = false;
                            m_gameScene.destroyEntity(ent);
                        }
                    };*/
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

void GolfState::refreshUI()
{
    auto uiSize = glm::vec2(GolfGame::getActiveTarget()->getSize()) / m_viewScale;

    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::UIElement;
    cmd.action = [&, uiSize](cro::Entity e, float)
    {
        auto pos = e.getComponent<UIElement>().relativePosition;
        pos.x *= uiSize.x;
        pos.x = std::floor(pos.x);
        pos.y *= (uiSize.y - UIBarHeight);
        pos.y = std::round(pos.y);
        pos.y += UITextPosV;

        pos += e.getComponent<UIElement>().absolutePosition;

        e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, e.getComponent<UIElement>().depth));

        if (e.getComponent<UIElement>().resizeCallback)
        {
            e.getComponent<UIElement>().resizeCallback(e);
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void GolfState::buildTrophyScene()
{
    auto updateCam = [&](cro::Camera& cam)
    {
        auto winSize = glm::vec2(480.f, 360.f);
        float maxScale = getViewScale();
        if (!m_sharedData.pixelScale)
        {
            winSize *= maxScale;
        }

        std::uint32_t samples = m_sharedData.pixelScale ? 0 :
            m_sharedData.antialias ? m_sharedData.multisamples : 0;

        m_trophySceneTexture.create(static_cast<std::uint32_t>(winSize.x), static_cast<std::uint32_t>(winSize.y), true, false, samples);
        
        float ratio = winSize.x / winSize.y;
        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, ratio, 0.1f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto& cam = m_trophyScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = updateCam;
    cam.isStatic = true;
    updateCam(cam);

    auto sunEnt = m_trophyScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -15.f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -10.f * cro::Util::Const::degToRad);

    const std::array<std::pair<std::string, glm::vec3>, 3u> Paths =
    {
        std::make_pair("assets/golf/models/trophies/trophy01.cmt", glm::vec3(0.f, -0.3f, -1.f)),
        std::make_pair("assets/golf/models/trophies/trophy02.cmt", glm::vec3(-0.55f, -0.315f, -1.f)),
        std::make_pair("assets/golf/models/trophies/trophy03.cmt", glm::vec3(0.55f, -0.33f, -1.f))        
    };

    cro::EmitterSettings emitterSettings;
    emitterSettings.loadFromFile("assets/golf/particles/firework.cps", m_resources.textures);

    if (emitterSettings.releaseCount == 0)
    {
        emitterSettings.releaseCount = 10;
    }

    cro::AudioScape as;
    as.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/player_menu.spt", m_resources.textures);

    std::int32_t i = 0;
    cro::ModelDefinition md(m_resources);
    for (const auto& [path, position] : Paths)
    {
        if (md.loadFromFile(path))
        {
            auto entity = m_trophyScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(position);
            entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
            md.createModel(entity);

            //TODO there's no gaurantee that the materials are in this order...
            auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]); //doesn't pixel fade like Cel does.
            applyMaterialData(md, material);
            entity.getComponent<cro::Model>().setMaterial(0, material);

            material = m_resources.materials.get(m_materialIDs[MaterialID::Trophy]);
            applyMaterialData(md, material);
            entity.getComponent<cro::Model>().setMaterial(1, material);

            entity.addComponent<TrophyDisplay>().delay = static_cast<float>(i) / 2.f;
            entity.addComponent<cro::ParticleEmitter>().settings = emitterSettings;

            entity.addComponent<cro::AudioEmitter>() = as.getEmitter("firework");
            entity.getComponent<cro::AudioEmitter>().setPitch(static_cast<float>(cro::Util::Random::value(8, 11)) / 10.f);
            entity.getComponent<cro::AudioEmitter>().setLooped(false);

            m_trophies[i].trophy = entity;
            auto trophyEnt = entity;

            //badge
            entity = m_trophyScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.08f, 0.06f });
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("rank_icon");
            auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
            bounds.width /= m_trophyScene.getSystem<cro::SpriteSystem3D>()->getPixelsPerUnit();
            entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });
            entity.addComponent<cro::Model>();
            entity.addComponent<cro::SpriteAnimation>();
            trophyEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_trophies[i].badge = entity;

            //name label
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>(m_sharedData.nameTextures[0].getTexture());
            bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
            bounds.height -= (LabelIconSize.y * 4.f);
            bounds.height /= ConstVal::MaxPlayers;
            bounds.bottom = bounds.height * i;
            entity.getComponent<cro::Sprite>().setTextureRect(bounds);
            entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -0.2f });
            auto p = position;
            entity.addComponent<cro::Callback>().function =
                [&,p,trophyEnt](cro::Entity e, float)
            {
                //the trophy scene sprite is set as user data in createUI()
                auto b = e.getComponent<cro::Callback>().getUserData<cro::Entity>().getComponent<cro::Sprite>().getTextureBounds();
                glm::vec2 pos(std::floor(((b.width / 1.5f) * p.x) + (b.width / 2.f)), std::floor(((b.height / 2.f) * p.y) + (b.height / 3.f)));

                float parentScale = e.getComponent<cro::Callback>().getUserData<cro::Entity>().getComponent<cro::Transform>().getScale().x;

                e.getComponent<cro::Transform>().setPosition(pos);
                e.getComponent<cro::Transform>().setScale(trophyEnt.getComponent<cro::Transform>().getScale() * (m_viewScale.x / parentScale));
            };
            trophyEnt.getComponent<TrophyDisplay>().label = entity;

            m_trophies[i].label = entity;


            //icon if available 
            //if (Social::isAvailable()) //always do this in case the player profile has an avatar
            {
                entity = m_uiScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height, 0.1f });
                entity.addComponent<cro::Drawable2D>();
                entity.addComponent<cro::Sprite>(m_sharedData.nameTextures[0].getTexture());
                bounds = { 0.f, LabelTextureSize.y - (LabelIconSize.y * 4.f), LabelIconSize.x, LabelIconSize.y };
                entity.getComponent<cro::Sprite>().setTextureRect(bounds);
                entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, -14.f, -0.1f });
                m_trophies[i].label.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                m_trophies[i].avatar = entity;

                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(1.f);
                entity.getComponent<cro::Callback>().function =
                    [&,i](cro::Entity e, float dt)
                {
                    if (m_trophies[i].label.getComponent<cro::Callback>().active)
                    {
                        e.getComponent<cro::Sprite>().setTexture(*m_trophies[i].label.getComponent<cro::Sprite>().getTexture(), false);

                        static constexpr float BaseScale = 0.5f;
                        static constexpr float SpinCount = 6.f;
                        static constexpr float Duration = 3.f;

                        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                        currTime = std::max(0.f, currTime - (dt / Duration));

                        float progress = cro::Util::Easing::easeInQuart(currTime) * SpinCount;
                        float scale = std::cos(cro::Util::Const::TAU * progress);

                        scale += 1.f;
                        scale /= 2.f;
                        scale *= BaseScale;

                        e.getComponent<cro::Transform>().setScale({ scale, BaseScale });

                        if (currTime == 0)
                        {
                            e.getComponent<cro::Callback>().active = false;
                            e.getComponent<cro::Transform>().setScale({ BaseScale, BaseScale });
                        }
                    }
                };
            }
        }
        ++i;
    }

    //we have to make sure everything is processed at least once
    //as a hacky way of making sure the double sided property is
    //applied to the badge materials.
    m_trophyScene.simulate(0.f);
}

void GolfState::updateMiniMap()
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::MiniMap;
    cmd.action = [&](cro::Entity en, float)
    {
        //trigger animation - this does the actual render
        en.getComponent<cro::Callback>().active = true;
        m_mapCam.getComponent<cro::Camera>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void GolfState::retargetMinimap(bool reset)
{
    if (m_minimapZoom.activeAnimation.isValid())
    {
        //remove existing animation
        m_minimapZoom.activeAnimation.getComponent<cro::Callback>().active = false;
        m_uiScene.destroyEntity(m_minimapZoom.activeAnimation);
        m_minimapZoom.activeAnimation = {};
    }
    struct MapZoomData final
    {
        struct
        {
            glm::vec2 pan = glm::vec2(0.f);
            float tilt = 0.f;
            float zoom = 1.f;
        }start, end;

        float progress = 0.f;
    }target;

    target.start.pan = m_minimapZoom.pan;
    target.start.tilt = m_minimapZoom.tilt;
    target.start.zoom = m_minimapZoom.zoom;

    if (reset)
    {
        //create a default view around the bounds of the hole model
        target.end.tilt = 0.f; //TODO this could be wound several times past TAU and should be only fmod this value

        auto bb = m_holeData[m_currentHole].modelEntity.getComponent<cro::Model>().getAABB();
        target.end.pan = m_minimapZoom.textureSize / 2.f;

        auto xZoom = std::clamp(static_cast<float>(MapSize.x) / ((bb[1].x - bb[0].x) * 1.6f), 0.9f, 16.f);
        auto zZoom = std::clamp(static_cast<float>(MapSize.y) / ((bb[1].z - bb[0].z) * 1.6f), 0.9f, 16.f);
        target.end.zoom = xZoom > zZoom ? xZoom : zZoom;
    }
    else
    {
        //find vec between player and flag
        auto pin = m_holeData[m_currentHole].pin;
        auto player = m_currentPlayer.position;

        //rotate minimap so flag is at top
        glm::vec2 dir(pin.x - player.x, -pin.z + player.z);
        auto rotation = std::atan2(-dir.y, dir.x) + cro::Util::Const::PI;
        target.end.tilt = m_minimapZoom.tilt + cro::Util::Maths::shortestRotation(m_minimapZoom.tilt, rotation);


        target.end.pan = glm::vec2(player.x, -player.z);

        //if we have a tight dogleg, such as on the mini putt
        //check if the primary target is inbetween and shift
        //towards it to better centre the hole
        auto targ = m_holeData[m_currentHole].target;
        glm::vec2 targDir(targ.x - player.x, -targ.z + player.z);
        const auto d = glm::dot(glm::normalize(dir), glm::normalize(targDir));
        const auto l2 = glm::length2(targDir);
        if (d > 0 && d < 0.8f && l2 > 2.25f && l2 < glm::length2(dir))
        {
            //project the target onto the current dir
            //then move half way between projection and
            //primary target
            //auto p = dir * d * (1.f / glm::length2(dir));

            //actually just moving towards the target seems to work better
            auto p = dir / 2.f;
            p += (targDir - p) / 2.f;
            target.end.pan += p;
        }
        else
        {
            //centre view between player and flag
            target.end.pan += (dir / 2.f);
        }
        //(pan is in texture coords hum)
        target.end.pan *= m_minimapZoom.mapScale;

        //get distance between flag and player and expand by 1.7 (about 3m around a putting hole)
        float viewLength = glm::length(dir) * 1.7f; //remember this is world coords

        //scale zoom on long edge of map by box length and clamp to 16x
        target.end.zoom = std::clamp(static_cast<float>(MapSize.x) / viewLength, 0.8f, 16.f);
    }

    //create a temp ent to interp between start and end values
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<MapZoomData>(target);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<MapZoomData>();

        const auto speed = 0.4f + (0.7f * (1.f - std::clamp(glm::length2(data.start.pan - data.end.pan) / (100.f * 100.f), 0.f, 1.f)));
        data.progress = std::min(1.f, data.progress + (dt * speed));

        m_minimapZoom.pan = glm::mix(data.start.pan, data.end.pan, cro::Util::Easing::easeOutExpo(data.progress));
        m_minimapZoom.tilt = glm::mix(data.start.tilt, data.end.tilt, cro::Util::Easing::easeInOutBack(data.progress));
        m_minimapZoom.zoom = glm::mix(data.start.zoom, data.end.zoom, cro::Util::Easing::easeOutBack(data.progress));
        m_minimapZoom.updateShader();

        if (data.progress == 1)
        {
            m_minimapZoom.activeAnimation = {};
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }
    };
    m_minimapZoom.activeAnimation = entity;
}

void MinimapZoom::updateShader()
{
    CRO_ASSERT(glm::length2(textureSize) != 0, "");

    auto pos = pan / textureSize;

    static constexpr glm::vec3 centre(0.5f, 0.5f, 0.f);
    const float aspect = textureSize.x / textureSize.y;

    glm::mat4 matrix(1.f);
    matrix = glm::translate(matrix, glm::vec3(pos.x, pos.y, 0.f));
    matrix = glm::scale(matrix, glm::vec3(1.f / aspect, 1.f, 1.f));
    matrix = glm::rotate(matrix, -tilt, cro::Transform::Z_AXIS);
    matrix = glm::scale(matrix, glm::vec3(aspect, 1.f, 1.f));
    matrix = glm::scale(matrix, glm::vec3(1.f / zoom));

    matrix = glm::translate(matrix, -centre);
    invTx = glm::inverse(matrix);

    glUseProgram(shaderID);
    glUniformMatrix4fv(matrixUniformID, 1, GL_FALSE, &matrix[0][0]);
}

glm::vec2 MinimapZoom::toMapCoords(glm::vec3 worldCoord) const
{
    CRO_ASSERT(glm::length2(textureSize) != 0, "");

    glm::vec2 mapCoord = glm::vec2(worldCoord.x, -worldCoord.z) * mapScale;
    mapCoord /= textureSize;
    mapCoord = glm::vec2(invTx * glm::vec4(mapCoord, 0.0, 1.0));
    return (mapCoord * textureSize);
}

//------emote wheel-----//
void GolfState::EmoteWheel::build(cro::Entity root, cro::Scene& uiScene, cro::TextureResource& textures)
{
    if (sharedData.tutorial)
    {
        //don't need this.
        return;
    }

    auto entity = uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.5f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;

    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    rootNode = entity;


    cro::SpriteSheet spriteSheet;
    if (spriteSheet.loadFromFile("assets/golf/sprites/emotes.spt", textures))
    {
        const std::array SpriteNames =
        {
            std::string("happy_large"),
            std::string("grumpy_large"),
            std::string("laughing_large"),
            std::string("sad_large")
        };

        auto& font = sharedData.sharedResources->fonts.get(FontID::UI);

        struct AnimData final
        {
            enum
            {
                In, Out
            }state = In;
            float progress = 1.f;
        };

        for (auto i = 0u; i < EmotePositions.size(); ++i)
        {
            entity = uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(EmotePositions[i]);
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite(SpriteNames[i]);
            auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
            entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

            entity.addComponent<cro::Callback>().setUserData<AnimData>();
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
            {
                const float Speed = dt * 10.f;
                auto& data = e.getComponent<cro::Callback>().getUserData<AnimData>();
                if (data.state == AnimData::In)
                {
                    data.progress = std::max(0.f, data.progress - Speed);
                    if (data.progress == 0)
                    {
                        data.state = AnimData::Out;
                    }
                }
                else
                {
                    data.progress = std::min(1.f, data.progress + Speed);
                    if (data.progress == 1)
                    {
                        data.state = AnimData::In;
                        targetScale = 0.f;
                        e.getComponent<cro::Callback>().active = false;
                    }
                }

                cro::Colour c(1.f, data.progress, data.progress);
                e.getComponent<cro::Sprite>().setColour(c);

                float scale = cro::Util::Easing::easeOutCirc(data.progress);
                e.getComponent<cro::Transform>().setScale({ scale, 1.f });
            };

            rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            buttonNodes[i] = entity;

            auto labelEnt = uiScene.createEntity();
            labelEnt.addComponent<cro::Transform>().setPosition(entity.getComponent<cro::Transform>().getOrigin() + glm::vec3(0.f, 20.f, 0.f));
            labelEnt.addComponent<cro::Drawable2D>();
            labelEnt.addComponent<cro::Text>(font);
            labelEnt.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
            /*labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f,-1.f });*/
            labelEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
            entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
            labelNodes[i] = labelEnt;
        }
        refreshLabels();

        entity = uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, 0.13f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

        rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }
}

bool GolfState::EmoteWheel::handleEvent(const cro::Event& evt)
{
    if (sharedData.tutorial)
    {
        return false;
    }

    const auto sendEmote = [&](std::uint8_t emoteID, std::int32_t controllerID)
    {
        std::uint32_t data = 0;
        if (currentPlayer.client == sharedData.localConnectionData.connectionID)
        {
            data |= (sharedData.localConnectionData.connectionID << 16) | (currentPlayer.player << 8) | (emoteID);
        }
        else
        {
            data |= (sharedData.localConnectionData.connectionID << 16) | (std::uint8_t(controllerID) << 8) | (emoteID);
        }
        sharedData.clientConnection.netClient.sendPacket(PacketID::Emote, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

        cooldown = 6.f;
        buttonNodes[emoteID].getComponent<cro::Callback>().active = true; //play anim which also closes wheel
    };

    if (cooldown > 0)
    {
        return false;
    }

    if (evt.type == SDL_KEYDOWN
        && evt.key.repeat == 0)
    {
        if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::EmoteMenu])
        {
            targetScale = 1.f;
            return true;
        }

        //stop these getting forwarded to input parser
        if (cro::Keyboard::isKeyPressed(sharedData.inputBinding.keys[InputBinding::EmoteMenu]))
        {
            if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::Up])
            {
                return true;
            }
            else if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::Down])
            {
                return true;
            }
            else if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::Left])
            {
                return true;
            }
            else if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::Right])
            {
                return true;
            }
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::EmoteMenu])
        {
            targetScale = 0.f;
            return true;
        }

        if (currentScale == 1)
        {
            if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::Up])
            {
                sendEmote(Emote::Happy, 0);
                return true;
            }
            else if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::Down])
            {
                sendEmote(Emote::Laughing, 0);
                return true;
            }
            else if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::Left])
            {
                sendEmote(Emote::Sad, 0);
                return true;
            }
            else if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::Right])
            {
                sendEmote(Emote::Grumpy, 0);
                return true;
            }
        }
    }


    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        auto controllerID = activeControllerID(sharedData.inputBinding.playerID);
        if (cro::GameController::controllerID(evt.cbutton.which) == controllerID)
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonY:
                targetScale = 1.f;
                return true;
            }
        }

        //prevent these getting forwarded to input parser if wheel is open
        if (cro::GameController::isButtonPressed(controllerID, cro::GameController::ButtonY))
        {
            switch (evt.cbutton.button)
            {
            default: return false;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                return true;
            }
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        auto controllerID = activeControllerID(sharedData.inputBinding.playerID);

        if (cro::GameController::controllerID(evt.cbutton.which) == controllerID)
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonY:
                targetScale = 0.f;
                return true;
            }
        }

        if (currentScale == 1)
        {
            switch (evt.cbutton.button)
            {
            default: return false;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                sendEmote(Emote::Happy, controllerID);
                return true;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                sendEmote(Emote::Laughing, controllerID);
                return true;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                sendEmote(Emote::Sad, controllerID);
                return true;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                sendEmote(Emote::Grumpy, controllerID);
                return true;
            }
        }
    }

    return false;
}

void GolfState::EmoteWheel::update(float dt)
{
    if (sharedData.tutorial)
    {
        return;
    }

    const float speed = dt * 10.f;
    if (currentScale < targetScale)
    {
        currentScale = std::min(targetScale, currentScale + speed);
    }
    else if (currentScale > targetScale)
    {
        currentScale = std::max(targetScale, currentScale - speed);
    }

    float scale = cro::Util::Easing::easeOutCirc(currentScale);
    rootNode.getComponent<cro::Transform>().setScale(glm::vec2(scale));

    cooldown -= dt;
}

void GolfState::EmoteWheel::refreshLabels()
{
    if (sharedData.tutorial)
    {
        //these won't exist
        return;
    }

    const std::array InputMap =
    {
        InputBinding::Up,
        InputBinding::Right,
        InputBinding::Down,
        InputBinding::Left,
    };

    for (auto i = 0u; i < labelNodes.size(); ++i)
    {
        labelNodes[i].getComponent<cro::Text>().setString(SDL_GetKeyName(sharedData.inputBinding.keys[InputMap[i]]));
        centreText(labelNodes[i]);

        if (cro::GameController::getControllerCount() == 0)
        {
            labelNodes[i].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        }
        else
        {
            labelNodes[i].getComponent<cro::Transform>().setScale({ 1.f, 0.f });
        }
    }
}

void GolfState::showEmote(std::uint32_t data)
{
    std::uint8_t client = (data & 0x00ff0000) >> 16;
    std::uint8_t player = (data & 0x0000ff00) >> 8;
    std::uint8_t emote = (data & 0x000000ff);

    client = std::min(client, std::uint8_t(3u));
    player = std::min(player, std::uint8_t(3u));

    auto msg = m_sharedData.connectionData[client].playerData[player].name;
    msg += " is ";

    std::int32_t emoteID = SpriteID::EmoteHappy;
    switch (emote)
    {
    default:
        msg += "undecided";
        break;
    case Emote::Happy:
        msg += "happy";
        break;
    case Emote::Grumpy:
        msg += "grumpy";
        emoteID = SpriteID::EmoteGrumpy;
        break;
    case Emote::Laughing:
        msg += "laughing";
        emoteID = SpriteID::EmoteLaugh;
        break;
    case Emote::Sad:
        msg += "sad";
        emoteID = SpriteID::EmoteSad;
        break;
    }

    showNotification(msg);


    struct EmoteData final
    {
        float velocity = 50.f;
        float decayRate = cro::Util::Random::value(13.f, 15.5f);
        float rotation = cro::Util::Random::value(-1.f, 1.f);
    };

    glm::vec3 pos(32.f, -16.f, 0.2f);
    for (auto i = 0u; i < 5u; ++i)
    {
        auto ent = m_uiScene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(pos);
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Sprite>() = m_sprites[emoteID];
        ent.addComponent<cro::Callback>().active = true;
        ent.getComponent<cro::Callback>().setUserData<EmoteData>();
        ent.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<EmoteData>();
            data.velocity = std::max(0.f, data.velocity - (dt * data.decayRate/* * m_viewScale.y*/));
            data.rotation *= 0.999f;

            if (data.velocity == 0)
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
            e.getComponent<cro::Transform>().setScale(m_viewScale);
            e.getComponent<cro::Transform>().move({ 0.f, data.velocity * m_viewScale.y * dt, 0.f });
            e.getComponent<cro::Transform>().rotate(data.rotation * dt);
        };

        auto bounds = ent.getComponent<cro::Sprite>().getTextureBounds();
        ent.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

        pos.x += static_cast<float>(cro::Util::Random::value(24, 38)) * m_viewScale.x;
        pos.y = -static_cast<float>(cro::Util::Random::value(1, 3)) * 10.f;
    }
}