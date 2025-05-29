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

#include "MainState.hpp"
#include "Slider.hpp"
#include "MyApp.hpp"

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/DebugInfo.hpp>

#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/UIDraggable.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Constants.hpp>
    
#include <crogine/android/Android.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

#include <string>

namespace
{
#include "MenuConsts.inl"

    const float scrollSpeed = 360.f;
}

using Score = std::pair<std::string, std::string>;

void MainState::createScoreMenu(std::uint32_t mouseEnterCallback, std::uint32_t mouseExitCallback,
    const cro::SpriteSheet& spriteSheetButtons, const cro::SpriteSheet& spriteSheetIcons)
{
    auto& menuFont = m_sharedResources.fonts.get(FontID::MenuFont);
    auto& scoreboardFont = m_sharedResources.fonts.get(FontID::ScoreboardFont);

    const auto buttonNormalArea = spriteSheetButtons.getSprite("button_inactive").getTextureRect();

    //create an entity to move the menu
    auto controlEntity = m_menuScene.createEntity();
    auto& controlTx = controlEntity.addComponent<cro::Transform>();
    controlTx.setPosition({ 2880.f, 624.f, 0.f });
    controlEntity.addComponent<cro::CommandTarget>().ID = CommandID::MenuController;
    controlEntity.addComponent<Slider>();

    auto backgroundEnt = m_menuScene.createEntity();
    backgroundEnt.addComponent<cro::Drawable2D>();
    backgroundEnt.addComponent<cro::Sprite>() = spriteSheetButtons.getSprite("menu");
    auto size = backgroundEnt.getComponent<cro::Sprite>().getSize();
    backgroundEnt.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y, 0.f });
    backgroundEnt.getComponent<cro::Transform>().setPosition({ 0.f, 140.f, -10.f });
    controlEntity.getComponent<cro::Transform>().addChild(backgroundEnt.getComponent<cro::Transform>());

    auto textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    auto& titleText = textEnt.addComponent<cro::Text>(menuFont);
    titleText.setString("Scores");
    titleText.setFillColour(textColourSelected);
    titleText.setCharacterSize(TextMedium);
    auto& titleTextTx = textEnt.addComponent<cro::Transform>();
    titleTextTx.setPosition({ -84.f, 110.f, 0.f });
    controlEntity.getComponent<cro::Transform>().addChild(titleTextTx);


    auto entity = m_menuScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetButtons.getSprite("button_inactive");
    auto& backTx = entity.addComponent<cro::Transform>();
    backTx.setPosition({ 0.f, -480.f, 0.f });
    controlEntity.getComponent<cro::Transform>().addChild(backTx);
    backTx.setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });

    textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    auto& backText = textEnt.addComponent<cro::Text>(menuFont);
    backText.setString("Back");
    backText.setFillColour(textColourNormal);
    backText.setCharacterSize(TextLarge);
    auto& backTexTx = textEnt.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().addChild(backTexTx);
    backTexTx.move({ 20.f, 80.f, 1.f });


    auto iconEnt = m_menuScene.createEntity();
    entity.getComponent<cro::Transform>().addChild(iconEnt.addComponent<cro::Transform>());
    iconEnt.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.f });
    iconEnt.addComponent<cro::Sprite>() = spriteSheetIcons.getSprite("back");
    iconEnt.addComponent<cro::Drawable2D>();

    auto backCallback = m_uiSystem->addCallback([this](cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, float)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(static_cast<float>(cro::DefaultSceneSize.x), 0.f, 0.f);
            };
            m_commandSystem->sendCommand(cmd);

            m_menuScene.getSystem<cro::UISystem>()->setActiveGroup(GroupID::Main);
        }
    });
    auto& backControl = entity.addComponent<cro::UIInput>();
    backControl.callbacks[cro::UIInput::ButtonUp] = backCallback;
    backControl.callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    backControl.callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    backControl.area.width = buttonNormalArea.width;
    backControl.area.height = buttonNormalArea.height;
    backControl.setGroup(GroupID::Scores);


    //load scores if we can, fill with fake if not yet created
    //yes these are plain text but you're daft to rig your own high scores
    cro::ConfigFile scores;
    if (!scores.loadFromFile(cro::App::getPreferencePath() + highscoreFile))
    {
        std::array<std::string, 10u> names =
        {
            "Jo", "Rick", "Sam", "Dmitri", "Lela",
            "Jeff", "Karen", "Marcus", "Chloe", "Cole"
        };
        for (auto i = 0u; i < 10u; ++i)
        {
            scores.addProperty(std::to_string(cro::Util::Random::value(10000, 500000))).setValue(names[i]);
        }
        if (!scores.save(cro::App::getPreferencePath() + highscoreFile))
        {
            cro::Logger::log("Failed saving default score file", cro::Logger::Type::Warning);
        }
    }

    std::vector<Score> scoreList;
    const auto& scoreValues = scores.getProperties();
    for (const auto& s : scoreValues)
    {
        scoreList.push_back(std::make_pair(s.getValue<std::string>(), s.getName()));
    }
    std::sort(std::begin(scoreList), std::end(scoreList), [](const Score& scoreA, const Score& scoreB)
    {
        try
        {
            //int conversion may fail :(
            return(std::stoi(scoreA.second) > std::stoi(scoreB.second));
        }
        catch (...)
        {
            return false;
        }
    });

    std::string scoreString;
    for (auto i = 0u; i < scoreList.size(); ++i)
    {
        scoreString += std::to_string(i + 1) + " " + scoreList[i].first + " " + scoreList[i].second + "\n";
    }

    entity = m_menuScene.createEntity();
    controlEntity.getComponent<cro::Transform>().addChild(entity.addComponent<cro::Transform>());
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(scoreboardFont).setString(scoreString);
    entity.getComponent<cro::Text>().setCharacterSize(TextLarge);
    entity.getComponent<cro::Text>().setFillColour(textColourSelected);

    auto bounds = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, -20.f, 0.f });

    size = backgroundEnt.getComponent<cro::Sprite>().getSize();
    cro::FloatRect croppingArea(0.f, -(size.y - backgroundEnt.getComponent<cro::Transform>().getPosition().y - 36.f), size.x * 0.8f, 0.f); 
    croppingArea.height = -croppingArea.bottom;//remember text origin is at top
                                                                                                                                           
    entity.getComponent<cro::Drawable2D>().setCroppingArea(croppingArea);

    //flip this back and use it for the scroll area
    croppingArea.bottom = 0.f;
    croppingArea.height = -croppingArea.height;

    //add click /drag
    const auto& scroll = [](cro::Entity entity, float delta)->float
    {
        auto& text = entity.getComponent<cro::Drawable2D>();
        auto crop = text.getCroppingArea();

        //clamp movement
        float movement = 0.f;
        if (delta > 0)
        {
            movement = cro::Util::Maths::clamp((text.getLocalBounds().height + crop.height) - entity.getComponent<cro::Transform>().getPosition().y, 0.f, delta);
        }
        else
        {
            movement = std::max(-entity.getComponent<cro::Transform>().getPosition().y, delta);
        }
        entity.getComponent<cro::Transform>().move({ 0.f, movement, 0.f });

        //update the cropping area
        crop.bottom -= movement;
        text.setCroppingArea(crop);

        //update the input area
        entity.getComponent<cro::UIInput>().area.bottom -= movement;

        return movement;
    };


    entity.addComponent<cro::UIDraggable>();
    entity.addComponent<cro::UIInput>().callbacks[cro::UIInput::Motion] = m_uiSystem->addCallback(
        [scroll](cro::Entity entity, glm::vec2 delta, const cro::MotionEvent&)
    {
        if (entity.getComponent<cro::UIDraggable>().flags & (1 << SDL_BUTTON_LEFT))
        {
            //add some momentum
            entity.getComponent<cro::UIDraggable>().velocity.y += scroll(entity, delta.y);
            entity.getComponent<cro::Callback>().active = true;
        }
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiSystem->addCallback(
        [](cro::Entity entity, const cro::ButtonEvent& evt)
    {
            if (evt.type == SDL_MOUSEBUTTONDOWN
                && evt.button.button == SDL_BUTTON_LEFT)
            {
                entity.getComponent<cro::UIDraggable>().flags |= (1 << SDL_BUTTON_LEFT);
            }
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback(
        [](cro::Entity entity, const cro::ButtonEvent& evt)
    {
            if (evt.type == SDL_MOUSEBUTTONUP
                && evt.button.button == SDL_BUTTON_LEFT)
            {
                entity.getComponent<cro::UIDraggable>().flags &= ~(1 << SDL_BUTTON_LEFT);
            }
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_uiSystem->addCallback(
        [](cro::Entity entity)
    {
        entity.getComponent<cro::UIDraggable>().flags = 0;
    });
    entity.getComponent<cro::UIInput>().area = croppingArea;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Scores);

    //callback gives scrolling some momentum
    entity.addComponent<cro::Callback>().function = 
        [scroll](cro::Entity entity, float dt)
    {
        auto& drag = entity.getComponent<cro::UIDraggable>();

        scroll(entity, drag.velocity.y * dt);
        drag.velocity *= 0.993f;

        if (glm::length2(drag.velocity) < 0.1f)
        {
            entity.getComponent<cro::Callback>().active = false;
        }
    };
    auto scoreEnt = entity;

    //scroll arrows
    auto activeArrow = spriteSheetButtons.getSprite("arrow_active").getTextureRect();
    auto inactiveArrow = spriteSheetButtons.getSprite("arrow_inactive").getTextureRect();

    auto arrowEnter = m_uiSystem->addCallback(
        [activeArrow](cro::Entity e)
    {
        e.getComponent<cro::Sprite>().setTextureRect(activeArrow);
    });
    auto arrowExit = m_uiSystem->addCallback(
        [inactiveArrow](cro::Entity e)
    {
        e.getComponent<cro::Sprite>().setTextureRect(inactiveArrow);
        e.getComponent<cro::Callback>().active = false;
    });

    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetButtons.getSprite("arrow_inactive");
    size = entity.getComponent<cro::Sprite>().getSize();
    entity.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    controlEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().setPosition({ (croppingArea.width / 2.f) + (size.x / 2.f) /** 0.89f*/, 60.f, 0.f });

    entity.addComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowExit;
    entity.getComponent<cro::UIInput>().area.width = activeArrow.width;
    entity.getComponent<cro::UIInput>().area.height = activeArrow.height;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Scores);

    entity.addComponent<cro::Callback>().function = [scroll, scoreEnt](cro::Entity entity, float dt)
    {
        scroll(scoreEnt, -scrollSpeed * dt);
    };
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiSystem->addCallback(
        [](cro::Entity entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            entity.getComponent<cro::Callback>().active = true;
        }
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback(
        [scoreEnt](cro::Entity entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            entity.getComponent<cro::Callback>().active = false;
            scoreEnt.getComponent<cro::UIDraggable>().velocity.y = -scrollSpeed / 2.f;
            scoreEnt.getComponent<cro::Callback>().active = true;
        }
    });
    auto upArrow = entity;



    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = upArrow.getComponent<cro::Sprite>();
    controlEntity.getComponent<cro::Transform>().addChild(entity.addComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().setOrigin(upArrow.getComponent<cro::Transform>().getOrigin());
    entity.getComponent<cro::Transform>().setPosition(upArrow.getComponent<cro::Transform>().getPosition());
    entity.getComponent<cro::Transform>().rotate({ 0.f, 0.f, 1.f }, cro::Util::Const::PI);
    entity.getComponent<cro::Transform>().move({ 0.f, croppingArea.height, 0.f });

    entity.addComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowExit;
    entity.getComponent<cro::UIInput>().area.width = activeArrow.width;
    entity.getComponent<cro::UIInput>().area.height = activeArrow.height;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Scores);

    entity.addComponent<cro::Callback>().function = [scroll, scoreEnt](cro::Entity entity, float dt)
    {
        scroll(scoreEnt, scrollSpeed * dt);
    };
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiSystem->addCallback(
        [](cro::Entity entity, const cro::ButtonEvent& evt)
    {
        if(activated(evt))
        {
            entity.getComponent<cro::Callback>().active = true;
        }
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback(
        [scoreEnt](cro::Entity entity, const cro::ButtonEvent& evt) mutable
    {
        if(activated(evt))
        {
            entity.getComponent<cro::Callback>().active = false;
            scoreEnt.getComponent<cro::UIDraggable>().velocity.y = scrollSpeed / 2.f;
            scoreEnt.getComponent<cro::Callback>().active = true;
        }
    });

    //TODO if shared resources contains a player name/score, scroll to it
}