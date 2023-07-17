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

#include "StatsState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"
#include "../GolfGame.hpp"

#include <Social.hpp>

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct MenuID final
    {
        enum
        {
            Main
        };
    };
}

StatsState::StatsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus()),
    m_sharedData        (sd),
    m_viewScale         (2.f),
    m_currentTab        (0)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool StatsState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_BACKSPACE
            || evt.key.keysym.sym == SDLK_ESCAPE
            || evt.key.keysym.sym == SDLK_p)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_LEFT:
        case SDLK_RIGHT:
            cro::App::getWindow().setMouseCaptured(true);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        cro::App::getWindow().setMouseCaptured(true);

        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonB:
        case cro::GameController::ButtonStart:
            quitState();
            return false;
        case cro::GameController::ButtonRightShoulder:
            activateTab((m_currentTab + 1) % TabID::Max);
            break;
        case cro::GameController::ButtonLeftShoulder:
            activateTab((m_currentTab + (TabID::Max - 1)) % TabID::Max);
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (evt.caxis.value > LeftThumbDeadZone)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void StatsState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool StatsState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void StatsState::render()
{
    m_scene.render();
}

//private
void StatsState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::SpriteAnimator>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);

    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);

    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_scene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_scene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");


    struct RootCallbackData final
    {
        enum
        {
            FadeIn, FadeOut
        }state = FadeIn;
        float currTime = 0.f;
    };

    auto rootNode = m_scene.createEntity();
    rootNode.addComponent<cro::Transform>();
    rootNode.addComponent<cro::Callback>().active = true;
    rootNode.getComponent<cro::Callback>().setUserData<RootCallbackData>();
    rootNode.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<RootCallbackData>();

        switch (state)
        {
        default: break;
        case RootCallbackData::FadeIn:
            currTime = std::min(1.f, currTime + (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));

            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;

                m_scene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                Social::setStatus(Social::InfoID::Menu, { "Browsing Stats" });
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();

                state = RootCallbackData::FadeIn;

                Social::setStatus(Social::InfoID::Menu, { "Clubhouse" });
            }
            break;
        }

    };

    m_rootNode = rootNode;


    //quad to darken the screen
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.4f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, rootNode](cro::Entity e, float)
    {
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        e.getComponent<cro::Transform>().setScale(size);
        e.getComponent<cro::Transform>().setPosition(size / 2.f);

        auto scale = rootNode.getComponent<cro::Transform>().getScale().x;
        scale = std::min(1.f, scale / m_viewScale.x);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(BackgroundAlpha * scale);
        }
    };

   
    //background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/stats_browser.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto bgNode = entity;

    //tab buttons
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("club_stats_button");
    bgNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    m_tabEntity = entity;

    auto selectedID = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            e.getComponent<cro::Callback>().active = true;
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        });
    auto unselectedID = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    const float stride = 102.f;
    auto sprite = spriteSheet.getSprite("button_highlight");
    bounds = sprite.getTextureBounds();
    const float offset = 51.f;

    for (auto i = 0; i < TabID::Max; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ (stride * i) + offset, 13.f, 0.5f });
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = sprite;
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().enabled = i != m_currentTab;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = 
            m_scene.getSystem<cro::UISystem>()->addCallback(
                [&, i](cro::Entity e, const cro::ButtonEvent& evt) 
                {
                    if (activated(evt))
                    {
                        activateTab(i);
                        e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
                    }                
                });

        entity.addComponent<cro::Callback>().function = MenuTextCallback();

        bgNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

        m_tabButtons[i] = entity;
    }


    createClubStatsTab(bgNode, spriteSheet);
    createPerformanceTab(bgNode, spriteSheet);
    createHistoryTab(bgNode);
    createAwardsTab(bgNode);

    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::UIElement;
        cmd.action =
            [&, size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size / m_viewScale;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_scene.getActiveCamera();
    entity.getComponent<cro::Camera>().resizeCallback = updateView;
    updateView(entity.getComponent<cro::Camera>());

    m_scene.simulate(0.f);
}

void StatsState::createClubStatsTab(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    m_tabNodes[TabID::ClubStats] = m_scene.createEntity();
    m_tabNodes[TabID::ClubStats].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::ClubStats].getComponent<cro::Transform>());

    const auto centre = parent.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 36.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("club_stats");
    entity.getComponent<cro::Transform>().move(glm::vec2(-entity.getComponent<cro::Sprite>().getTextureBounds().width / 2.f, 0.f));
    m_tabNodes[TabID::ClubStats].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);

    constexpr glm::vec2 BasePos(76.f, 302.f);
    constexpr glm::vec2 StatSpacing(160.f, 64.f);
    constexpr std::int32_t RowCount = 4;
    constexpr float MaxBarWidth = StatSpacing.x - 56.f;
    constexpr float BarHeight = 10.f;
    constexpr std::array BarColours =
    {
        cro::Colour(0xb83530ff), //red
        cro::Colour(0x467e3eff), //green
        cro::Colour(0x6eb39dff), //blue
        cro::Colour(0xf2cf5cff) //yellow
    };
    struct ColourID final
    {
        enum
        {
            Novice, Expert, Pro, Spin
        };
    };

    auto playerLevel = Social::getLevel();
    auto clubFlags = Social::getUnlockStatus(Social::UnlockType::Club);

    const auto createStat = [&](std::int32_t clubID)
    {
        glm::vec2 statPos = BasePos;
        statPos.x += StatSpacing.x * (clubID / RowCount);
        statPos.y -= StatSpacing.y * (clubID % RowCount);

        //TODO check if this club is unlocked for current user
        //else mark at which level is unlocked
        //max range level 0,1,2 and spin influence
        auto statNode = m_scene.createEntity();
        statNode.addComponent<cro::Transform>().setPosition(statPos);
        m_tabNodes[TabID::ClubStats].getComponent<cro::Transform>().addChild(statNode.getComponent<cro::Transform>());

        //TODO we need to be able to refresh these labels if the user switch
        //to imperial measurements
        cro::String label = Clubs[clubID].getLabel() + "\n";
        label += Clubs[clubID].getDistanceLabel(m_sharedData.imperialMeasurements, 0) + "\n";
        label += Clubs[clubID].getDistanceLabel(m_sharedData.imperialMeasurements, 1) + "\n";
        label += Clubs[clubID].getDistanceLabel(m_sharedData.imperialMeasurements, 2) + "\n";
        
        std::stringstream ss;
        ss.precision(2);
        ss << Clubs[clubID].getTopSpinMultiplier() << "/" << Clubs[clubID].getSideSpinMultiplier();
        label += ss.str();

        auto labelEnt = m_scene.createEntity();
        labelEnt.addComponent<cro::Transform>();
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Text>(font).setString(label);
        labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        labelEnt.getComponent<cro::Text>().setVerticalSpacing(-1.f);
        labelEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
        statNode.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());

        auto unlockLevel = ClubID::getUnlockLevel(clubID);

        label = ((clubFlags & ClubID::Flags[clubID]) == 0) ? "Level " + std::to_string(unlockLevel) : "|";
        label += "\n";

        if ((playerLevel < 15) || (clubFlags & ClubID::Flags[clubID]) == 0)
        {
            auto l = std::max(15, unlockLevel);
            label += "Level " + std::to_string(l) + "\n";
        }
        else
        {
            label += "|\n";
        }
        if ((playerLevel < 30) || (clubFlags & ClubID::Flags[clubID]) == 0)
        {
            label += "Level 30\n";
        }
        else
        {
            label += "|\n";
        }
        label += "Top/Side";

        labelEnt = m_scene.createEntity();
        labelEnt.addComponent<cro::Transform>().setPosition({ 4.f, -11.f, 0.3f });
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Text>(font).setString(label);
        labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        labelEnt.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
        labelEnt.getComponent<cro::Text>().setVerticalSpacing(-1.f);
        statNode.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());


        std::vector<cro::Vertex2D> verts;

        float barPos = -10.f;
        const auto createBar = [&](float width, std::int32_t colourIndex)
        {
            //fudgenstein.
            auto c = BarColours[colourIndex];
            if (colourIndex == 2 && playerLevel < 30
                || colourIndex == 1 && playerLevel < 15)
            {
                c.setAlpha(0.15f);
            }

            if ((clubFlags & ClubID::Flags[clubID]) == 0)
            {
                c.setAlpha(0.15f);
            }

            verts.emplace_back(glm::vec2(0.f, barPos), c);
            verts.emplace_back(glm::vec2(0.f, barPos - BarHeight), c);
            verts.emplace_back(glm::vec2(width, barPos), c);
            verts.emplace_back(glm::vec2(width, barPos), c);
            verts.emplace_back(glm::vec2(0.f, barPos - BarHeight), c);
            verts.emplace_back(glm::vec2(width, barPos - BarHeight), c);

            barPos -= (BarHeight + 1.f);
        };

        //each level
        for (auto i = 0; i < 3; ++i)
        {
            const float barWidth = MaxBarWidth * (Clubs[clubID].getTargetAtLevel(i) / Clubs[ClubID::Driver].getTargetAtLevel(2));
            createBar(std::round(barWidth), i);
        }

        //and spin influence
        createBar(std::round(MaxBarWidth * Clubs[clubID].getSpinInfluence()), ColourID::Spin);


        //draw all the bars with one entity
        auto barEnt = m_scene.createEntity();
        barEnt.addComponent<cro::Transform>().setPosition({ 3.f, 0.f, 0.f });
        barEnt.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
        barEnt.getComponent<cro::Drawable2D>().setVertexData(verts);
        statNode.getComponent<cro::Transform>().addChild(barEnt.getComponent<cro::Transform>());
    };

    for (auto i = 0; i < ClubID::Putter; ++i)
    {
        createStat(i);
    }
}

void StatsState::createPerformanceTab(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    m_tabNodes[TabID::Performance] = m_scene.createEntity();
    m_tabNodes[TabID::Performance].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    m_tabNodes[TabID::Performance].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::Performance].getComponent<cro::Transform>());

    const auto centre = parent.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 56.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("performance");
    entity.getComponent<cro::Transform>().move(glm::vec2(-entity.getComponent<cro::Sprite>().getTextureBounds().width / 2.f, 0.f));
    m_tabNodes[TabID::Performance].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //TODO ideally we want to use a menu group to disable
    //these - but then this breaks our tab buttons
    //(oh if only we used bitflags to set multiple groups
    //on any given button....)

    //TODO we could also pad this out with disabled inputs
    //so that the column/row count properly matches.

    //cpu checkbox

    //previous course

    //next course

    //previous profile

    //next profile

}

void StatsState::createHistoryTab(cro::Entity parent)
{
    m_tabNodes[TabID::History] = m_scene.createEntity();
    m_tabNodes[TabID::History].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    m_tabNodes[TabID::History].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::History].getComponent<cro::Transform>());

    //TODO pie charts - left side number of times personally played a course
    //right side aggregated stats from steam of course plays
}

void StatsState::createAwardsTab(cro::Entity parent)
{
    m_tabNodes[TabID::Awards] = m_scene.createEntity();
    m_tabNodes[TabID::Awards].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    m_tabNodes[TabID::Awards].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::Awards].getComponent<cro::Transform>());

    const auto centre = parent.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 240.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("No Awards... sad :(");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    m_tabNodes[TabID::Awards].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void StatsState::activateTab(std::int32_t tabID)
{
    //update old
    m_tabButtons[m_currentTab].getComponent<cro::UIInput>().enabled = true;
    m_tabNodes[m_currentTab].getComponent<cro::Transform>().setScale(glm::vec2(0.f));

    //update index
    m_currentTab = tabID;

    //update new
    m_tabButtons[m_currentTab].getComponent<cro::UIInput>().enabled = false;
    m_tabNodes[m_currentTab].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

    //update the button selection graphic
    auto bounds = m_tabEntity.getComponent<cro::Sprite>().getTextureRect();
    bounds.bottom = bounds.height * m_currentTab;
    m_tabEntity.getComponent<cro::Sprite>().setTextureRect(bounds);

    //TODO set column count to make performance menu more intuitive when
    //navigating with a controller / steam deck

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
}

void StatsState::quitState()
{
    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}