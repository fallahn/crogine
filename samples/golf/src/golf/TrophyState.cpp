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

#include "TrophyState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "../GolfGame.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/core/SysTime.hpp>
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
#include <crogine/ecs/components/Model.hpp>
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
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/util/Easings.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
#include "shaders/CelShader.inl"
    constexpr glm::vec2 TrophyTextureSize(140.f, 184.f);

    //use static material IDs as we're reading materials from
    //shared resources and we want to recycle them each time
    //this state is created

    struct MaterialID final
    {
        enum
        {
            Shiny, Flat, Shelf,
            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> materialIDs = { 0,0,0 };

    struct MenuID final
    {
        enum
        {
            Dummy, Main
        };
    };
}

TrophyState::TrophyState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus()),
    m_trophyScene       (ctx.appInstance.getMessageBus()),
    m_sharedData        (sd),
    m_trophyIndex       (1),
    m_scaleBuffer       ("PixelScale"),
    m_resolutionBuffer  ("ScaledResolution"),
    m_viewScale         (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildTrophyScene();
    buildScene();

    //delays animation
    /*auto entity = m_scene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.5f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;

        if (currTime < 0)
        {
            m_rootNode.getComponent<cro::Callback>().active = true;

            e.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(e);
        }
    };*/

#ifdef CRO_DEBUG_
//    registerWindow([&]() 
//        {
//            if (ImGui::Begin("buns"))
//            {
//                static float overshoot = 0.f;
//                if (ImGui::SliderFloat("expansion", &overshoot, 0.f, 15.f))
//                {
//                    m_trophyScene.getActiveCamera().getComponent<cro::Camera>().setShadowExpansion(overshoot);
//                }
//
//                static float maxDistance = 10.f;
//                if (ImGui::SliderFloat("max dist", &maxDistance, 0.4f, 10.f))
//                {
//                    m_trophyScene.getActiveCamera().getComponent<cro::Camera>().setMaxShadowDistance(maxDistance);
//                }
//
//                ImGui::Image(m_trophyScene.getActiveCamera().getComponent<cro::Camera>().shadowMapBuffer.getTexture(), { 256.f, 256.f }, { 0.f, 1.f }, { 1.f, 0.f });
//            }
//            ImGui::End();
//        });
#endif
}

//public
bool TrophyState::handleEvent(const cro::Event& evt)
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
            || evt.key.keysym.sym == SDLK_ESCAPE)
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
        case cro::GameController::ButtonBack:
            quitState();
            return false;
        case cro::GameController::ButtonLeftShoulder:
            m_pageFuncs[0]();
            return false;
        case cro::GameController::ButtonRightShoulder:
            m_pageFuncs[1]();
            return false;
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
    m_trophyScene.forwardEvent(evt);
    return false;
}

void TrophyState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
    m_trophyScene.forwardMessage(msg);
}

bool TrophyState::simulate(float dt)
{
    m_scene.simulate(dt);
    m_trophyScene.simulate(dt);
    return true;
}

void TrophyState::render()
{
    m_trophyTexture.clear(cro::Colour::Transparent/*cro::Colour::Red*/);
    m_trophyScene.render();
    m_trophyTexture.display();

    m_scene.render();
}

//private
void TrophyState::buildScene()
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



    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/billiards_ui.spt", m_sharedData.sharedResources->textures);

    struct DoorData final
    {
        explicit DoorData(float d = 1.f)
            : direction(d) 
        {
            facing = direction > 0 ? cro::Drawable2D::Facing::Back : cro::Drawable2D::Facing::Front;
        }

        float progress = 0.f;
        cro::Drawable2D::Facing facing = cro::Drawable2D::Facing::Front;
        const float direction = 1.f;
    };

    auto doorRect = spriteSheet.getSprite("door").getTextureRectNormalised();
    auto doorSize = spriteSheet.getSprite("door").getSize();
    auto doorCallback = [&, doorSize](cro::Entity e, float dt)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<DoorData>();
        auto oldProgress = data.progress;
        data.progress = std::min(1.f, data.progress + (dt * 2.f));

        float xScale = ((cro::Util::Easing::easeOutCirc(data.progress) * 2.f) - 1.f) * -data.direction;
        e.getComponent<cro::Transform>().setScale({ xScale, 1.f, 0.f });

        static constexpr float OffsetSize = 20.f;
        auto sinProgress = std::sin(data.progress * cro::Util::Const::PI);
        auto offset = OffsetSize * sinProgress;

        auto colour = std::min(1.f, 0.5f + (0.5f * (1.f - sinProgress)));
        auto c = cro::Colour(colour, colour, colour);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        verts[0].colour = c;
        verts[1].colour = c;
        verts[2].position.y = doorSize.y + offset;
        verts[3].position.y = -offset;

        if (oldProgress <= 0.5f && data.progress > 0.5f)
        {
            e.getComponent<cro::Drawable2D>().setFacing(data.facing);
        }

        if (data.progress == 1)
        {
            verts[2].position.y = doorSize.y;
            verts[3].position.y = 0.f;
            
            e.getComponent<cro::Callback>().active = false;
            if (!m_trophyEnts.empty())
            {
                auto modelIndex = std::min(m_trophyEnts.size() - 1, AchievementTrophies[m_trophyIndex]);
                m_trophyEnts[AchievementTrophies[modelIndex]].getComponent<cro::Callback>().active = true;
            }
        }
    };

    std::vector<cro::Vertex2D> doorVerts =
    {
        cro::Vertex2D(glm::vec2(0.f, doorSize.y), glm::vec2(doorRect.left, doorRect.bottom + doorRect.height)),
        cro::Vertex2D(glm::vec2(0.f), glm::vec2(doorRect.left, doorRect.bottom)),
        cro::Vertex2D(doorSize, glm::vec2(doorRect.left + doorRect.width, doorRect.bottom + doorRect.height)),
        cro::Vertex2D(glm::vec2(doorSize.x, 0.f), glm::vec2(doorRect.left + doorRect.width, doorRect.bottom))
    };

    auto leftDoor = m_scene.createEntity();
    leftDoor.addComponent<cro::Transform>().setPosition({14.f, 38.f, 0.3f});
    leftDoor.addComponent<cro::Drawable2D>().setTexture(spriteSheet.getSprite("door").getTexture());
    leftDoor.getComponent<cro::Drawable2D>().setVertexData(doorVerts);
    leftDoor.addComponent<cro::Callback>().function = doorCallback;
    leftDoor.getComponent<cro::Callback>().setUserData<DoorData>();

    auto rightDoor = m_scene.createEntity();
    rightDoor.addComponent<cro::Transform>().setPosition({ 256.f, 38.f, 0.3f });
    rightDoor.getComponent<cro::Transform>().setScale({ -1.f, 1.f });
    rightDoor.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    rightDoor.getComponent<cro::Drawable2D>().setTexture(spriteSheet.getSprite("door").getTexture());
    rightDoor.getComponent<cro::Drawable2D>().setVertexData(doorVerts);
    rightDoor.addComponent<cro::Callback>().function = doorCallback;
    rightDoor.getComponent<cro::Callback>().setUserData<DoorData>(-1.f);

    struct RootCallbackData final
    {
        enum
        {
            FadeIn, FadeOut
        }state = FadeIn;
        float currTime = 0.f;
    };

    auto rootNode = m_scene.createEntity();
    rootNode.addComponent<cro::Transform>().setScale({0.f, 0.f});
    rootNode.addComponent<cro::Callback>().active = true;
    rootNode.getComponent<cro::Callback>().setUserData<RootCallbackData>();
    rootNode.getComponent<cro::Callback>().function =
        [&, leftDoor, rightDoor, doorVerts](cro::Entity e, float dt) mutable
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

                leftDoor.getComponent<cro::Callback>().active = true;
                rightDoor.getComponent<cro::Callback>().active = true;

                m_scene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();

                state = RootCallbackData::FadeIn;
                m_scene.setSystemActive<cro::AudioPlayerSystem>(true);

                /*leftDoor.getComponent<cro::Drawable2D>().setVertexData(doorVerts);
                leftDoor.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                leftDoor.getComponent<cro::Callback>().active = false;
                leftDoor.getComponent<cro::Callback>().setUserData<DoorData>(1.f);
                leftDoor.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                rightDoor.getComponent<cro::Drawable2D>().setVertexData(doorVerts);
                rightDoor.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                rightDoor.getComponent<cro::Callback>().active = false;
                rightDoor.getComponent<cro::Callback>().setUserData<DoorData>(-1.f);
                leftDoor.getComponent<cro::Transform>().setScale(glm::vec2(-1.f, 1.f));*/
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
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("trophy_background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto backgroundEnt = entity;
    backgroundEnt.getComponent<cro::Transform>().addChild(leftDoor.getComponent<cro::Transform>());
    backgroundEnt.getComponent<cro::Transform>().addChild(rightDoor.getComponent<cro::Transform>());

    auto& uiFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& infoFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    auto selectedID = uiSystem.addCallback(
        [](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White); 
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto unselectedID = uiSystem.addCallback(
        [](cro::Entity e) 
        { 
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    
    auto bgBounds = bounds;


    auto titleEnt = m_scene.createEntity();
    titleEnt.addComponent<cro::Transform>().setPosition({ bgBounds.width / 2.f, 293.f, 0.1f });
    titleEnt.addComponent<cro::Drawable2D>();
    titleEnt.addComponent<cro::Text>(uiFont).setCharacterSize(UITextSize);
    titleEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    backgroundEnt.getComponent<cro::Transform>().addChild(titleEnt.getComponent<cro::Transform>());

    auto descEnt = m_scene.createEntity();
    descEnt.addComponent<cro::Transform>().setPosition({ 74.f, 269.f, 0.1f });
    descEnt.addComponent<cro::Drawable2D>();
    descEnt.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
    descEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    backgroundEnt.getComponent<cro::Transform>().addChild(descEnt.getComponent<cro::Transform>());

    auto dateEnt = m_scene.createEntity();
    dateEnt.addComponent<cro::Transform>().setPosition({ bgBounds.width / 2.f, 56.f, 0.2f });
    dateEnt.addComponent<cro::Drawable2D>();
    dateEnt.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
    dateEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    backgroundEnt.getComponent<cro::Transform>().addChild(dateEnt.getComponent<cro::Transform>());

    auto iconEnt = m_scene.createEntity();
    iconEnt.addComponent<cro::Transform>().setPosition({ 32.f, 238.f, 0.1f });
    iconEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.5f));
    iconEnt.addComponent<cro::Drawable2D>();
    backgroundEnt.getComponent<cro::Transform>().addChild(iconEnt.getComponent<cro::Transform>());

    auto updateText = [&, titleEnt, descEnt, dateEnt, iconEnt]() mutable
    {
        auto [descString, hidden] = AchievementDesc[m_trophyIndex];
        if (!hidden || Achievements::getAchievement(AchievementStrings[m_trophyIndex])->achieved)
        {
            static constexpr std::size_t MaxLength = 32;

            if (descString.length() >= MaxLength)
            {
                /*auto end = std::min(descString.length() - 1, MaxLength);
                auto pos = descString.substr(0, end).find_last_of(' ');
                if (pos != std::string::npos)
                {
                    descString[pos] = '\n';
                }*/
                descString = cro::Util::String::wordWrap(descString, MaxLength, descString.length());
            }

            titleEnt.getComponent<cro::Text>().setString(AchievementLabels[m_trophyIndex]);
        }
        else
        {
            titleEnt.getComponent<cro::Text>().setString("HIDDEN");
            descString = "????????";
        }
        descEnt.getComponent<cro::Text>().setString(descString);
        centreText(titleEnt);

        auto timestamp = Achievements::getAchievement(AchievementStrings[m_trophyIndex])->timestamp;
        if (timestamp != 0)
        {
            dateEnt.getComponent<cro::Text>().setString("Achieved: " + cro::SysTime::dateString(timestamp));
        }
        else
        {
            dateEnt.getComponent<cro::Text>().setString(" ");
        }
        centreText(dateEnt);

        static constexpr float IconWidth = 64.f;
        auto icon = Achievements::getIcon(AchievementStrings[m_trophyIndex]);
        iconEnt.getComponent<cro::Drawable2D>().setTexture(icon.texture);
        std::vector<cro::Vertex2D> verts =
        {
            cro::Vertex2D(glm::vec2(0.f, IconWidth), glm::vec2(icon.textureRect.left, icon.textureRect.bottom + icon.textureRect.height)),
            cro::Vertex2D(glm::vec2(0.f), glm::vec2(icon.textureRect.left, icon.textureRect.bottom)),
            cro::Vertex2D(glm::vec2(IconWidth), glm::vec2(icon.textureRect.left + icon.textureRect.width, icon.textureRect.bottom + icon.textureRect.height)),
            cro::Vertex2D(glm::vec2(IconWidth, 0.f), glm::vec2(icon.textureRect.left + icon.textureRect.width, icon.textureRect.bottom))
        };
        iconEnt.getComponent<cro::Drawable2D>().setVertexData(verts);
    };
    updateText();

    m_pageFuncs[0] =
        [&, updateText]() mutable
    {
        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

        //hide model
        if (!m_trophyEnts.empty())
        {
            auto modelIndex = std::min(m_trophyEnts.size() - 1, AchievementTrophies[m_trophyIndex]);
            m_trophyEnts[modelIndex].getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 0;
            m_trophyEnts[modelIndex].getComponent<cro::Callback>().active = true;
        }


        m_trophyIndex = ((m_trophyIndex + (AchievementID::Count - 1)) % AchievementID::Count);
        if (m_trophyIndex == 0)
        {
            m_trophyIndex = AchievementID::Count - 1;
        }
        updateText();
    };

    m_pageFuncs[1] =
        [&, updateText]() mutable
    {
        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

        //hide model
        if (!m_trophyEnts.empty())
        {
            auto modelIndex = std::min(m_trophyEnts.size() - 1, AchievementTrophies[m_trophyIndex]);
            m_trophyEnts[modelIndex].getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 0;
            m_trophyEnts[modelIndex].getComponent<cro::Callback>().active = true;
        }


        m_trophyIndex = ((m_trophyIndex + 1) % AchievementID::Count);

        //there's probably a smart way to do this, but brain.
        if (m_trophyIndex == 0)
        {
            m_trophyIndex++;
        }

        updateText();
    };

    //arrow left
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 16.f, 19.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_left");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown ] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt) 
            {
                if (activated(evt))
                {
                    m_pageFuncs[0]();
                }
            });
    backgroundEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //quit button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({bgBounds.width / 2.f, 24.f, 0.1f});
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("close_button");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    auto highlightBounds = spriteSheet.getSprite("close_highlight").getTextureRect();
    auto normalBounds = spriteSheet.getSprite("close_button").getTextureRect();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem.addCallback([highlightBounds](cro::Entity e) 
            {
                e.getComponent<cro::Sprite>().setTextureRect(highlightBounds);
                e.getComponent<cro::AudioEmitter>().play();
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        uiSystem.addCallback([normalBounds](cro::Entity e) 
            {
                e.getComponent<cro::Sprite>().setTextureRect(normalBounds);
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    quitState();
                }
            });

    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    backgroundEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //arrow right
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 244.f, 19.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_right");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_pageFuncs[1]();
                }
            });
    backgroundEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);

    //displays trophy render texture
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgBounds.width / 2.f, 138.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    backgroundEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto trophyEnt = entity;

    auto updateView = [&, rootNode, trophyEnt](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());

        //don't do this unless the fade in callback is complete.
        if (rootNode.getComponent<cro::Callback>().getUserData<RootCallbackData>().state == RootCallbackData::FadeOut)
        {
            rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        }
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


        //resize the trophy texture and rescale sprite
        glm::uvec2 textureSize(TrophyTextureSize);
        float invScale = 1.f;
        if (m_sharedData.pixelScale)
        {
            trophyEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }
        else
        {
            invScale = m_viewScale.x;
            textureSize *= static_cast<std::uint32_t>(m_viewScale.x);
            trophyEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f) / m_viewScale);
        }

        std::uint32_t samples = m_sharedData.pixelScale ? 0 :
            m_sharedData.antialias ? m_sharedData.multisamples : 0;

        m_trophyTexture.create(textureSize.x, textureSize.y, true, false, samples);
        trophyEnt.getComponent<cro::Sprite>().setTexture(m_trophyTexture.getTexture());
        trophyEnt.getComponent<cro::Transform>().setOrigin({ textureSize.x / 2.f, textureSize.y / 2.f });

        m_scaleBuffer.setData(invScale);

        ResolutionData d;
        d.resolution = glm::vec2(textureSize) / invScale;
        m_resolutionBuffer.setData(d);
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void TrophyState::buildTrophyScene()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_trophyScene.addSystem<cro::CallbackSystem>(mb);
    m_trophyScene.addSystem<cro::CameraSystem>(mb);
    m_trophyScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_trophyScene.addSystem<cro::ModelRenderer>(mb);
   
    m_scaleBuffer.bind(0);
    m_resolutionBuffer.bind(1);
    m_reflectionMap.loadFromFile("assets/golf/images/skybox/billiards/trophy.ccm");


    std::string wobble;
    if (m_sharedData.vertexSnap)
    {
        wobble = "#define WOBBLE\n";
    }

    if (!m_sharedData.sharedResources->shaders.hasShader(ShaderID::Course))
    {
        m_sharedData.sharedResources->shaders.loadFromString(ShaderID::Course, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define RX_SHADOWS\n#define TEXTURED\n");
    }

    auto* shader = &m_sharedData.sharedResources->shaders.get(ShaderID::Course);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    if (materialIDs[MaterialID::Shelf] == 0)
    {
        materialIDs[MaterialID::Shelf] = m_sharedData.sharedResources->materials.add(*shader);
    }
    auto shelfMaterial = m_sharedData.sharedResources->materials.get(materialIDs[MaterialID::Shelf]);


    if (!m_sharedData.sharedResources->shaders.hasShader(ShaderID::Ball))
    {
        m_sharedData.sharedResources->shaders.loadFromString(ShaderID::Ball, CelVertexShader, CelFragmentShader, "#define TINT\n#define RX_SHADOWS\n#define VERTEX_COLOURED\n");
    }

    shader = &m_sharedData.sharedResources->shaders.get(ShaderID::Ball);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    if (materialIDs[MaterialID::Flat] == 0)
    {
        materialIDs[MaterialID::Flat] = m_sharedData.sharedResources->materials.add(*shader);
    }
    auto flatMaterial = m_sharedData.sharedResources->materials.get(materialIDs[MaterialID::Flat]);

    if (!m_sharedData.sharedResources->shaders.hasShader(ShaderID::Trophy))
    {
        m_sharedData.sharedResources->shaders.loadFromString(ShaderID::Trophy, CelVertexShader, CelFragmentShader, "#define TINT\n/*#define RX_SHADOWS\n*/#define VERTEX_COLOURED\n#define REFLECTIONS\n");
    }
    shader = &m_sharedData.sharedResources->shaders.get(ShaderID::Trophy);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    if (materialIDs[MaterialID::Shiny] == 0)
    {
        materialIDs[MaterialID::Shiny] = m_sharedData.sharedResources->materials.add(*shader);
    }
    auto shinyMaterial = m_sharedData.sharedResources->materials.get(materialIDs[MaterialID::Shiny]);
    shinyMaterial.setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap.getGLHandle()));

    cro::ModelDefinition md(*m_sharedData.sharedResources);
    if (md.loadFromFile("assets/golf/models/trophies/shelf.cmt"))
    {
        auto entity = m_trophyScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        applyMaterialData(md, shelfMaterial);
        entity.getComponent<cro::Model>().setMaterial(0, shelfMaterial);
    }

    auto rotateEnt = m_trophyScene.createEntity();
    rotateEnt.addComponent<cro::Transform>();
    rotateEnt.addComponent<cro::Callback>().active = true;
    rotateEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
    };


    //these ought to be loaded in the same order
    //as TrophyID in AchievementStrings.hpp
    const std::array<std::string, 15u> paths =
    {
        std::string("assets/golf/models/trophies/trophy01.cmt"),
        "assets/golf/models/trophies/trophy02.cmt",
        "assets/golf/models/trophies/trophy03.cmt",
        "assets/golf/models/trophies/trophy04.cmt",
        "assets/golf/models/trophies/trophy05.cmt",
        "assets/golf/models/trophies/trophy06.cmt",
        "assets/golf/models/trophies/trophy07.cmt",
        "assets/golf/models/trophies/trophy08.cmt",
        "assets/golf/models/trophies/level01.cmt",
        "assets/golf/models/trophies/level10.cmt",
        "assets/golf/models/trophies/level20.cmt",
        "assets/golf/models/trophies/level30.cmt",
        "assets/golf/models/trophies/level40.cmt",
        "assets/golf/models/trophies/level50.cmt",
        "assets/golf/models/trophies/trophy09.cmt",
    };

    auto trophyCallback =
        [&](cro::Entity e, float dt) mutable
    {
        auto& [currTime, direction] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
        const float updateTime = dt * 3.f;

        if (direction == 0)
        {
            //shrink
            currTime = std::max(0.f, currTime - updateTime);
            if (currTime == 0)
            {
                direction = 1;
                e.getComponent<cro::Callback>().active = false;
                e.getComponent<cro::Model>().setHidden(true);

                //start next trophy growing
                if (!m_trophyEnts.empty())
                {
                    auto idx = std::min(m_trophyEnts.size() - 1, AchievementTrophies[m_trophyIndex]);
                    m_trophyEnts[idx].getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 1;
                    m_trophyEnts[idx].getComponent<cro::Callback>().active = true;
                    m_trophyEnts[idx].getComponent<cro::Model>().setHidden(false);

                    if (Achievements::getAchievement(AchievementStrings[m_trophyIndex])->achieved)
                    {
                        m_trophyEnts[idx].getComponent<cro::Model>().setMaterialProperty(0, "u_tintColour", glm::vec4(1.f));
                        m_trophyEnts[idx].getComponent<cro::Model>().setMaterialProperty(1, "u_tintColour", glm::vec4(1.f));
                    }
                    else
                    {
                        m_trophyEnts[idx].getComponent<cro::Model>().setMaterialProperty(0, "u_tintColour", glm::vec4(glm::vec3(0.f), 1.f));
                        m_trophyEnts[idx].getComponent<cro::Model>().setMaterialProperty(1, "u_tintColour", glm::vec4(glm::vec3(0.f), 1.f));
                    }
                }
            }
        }
        else
        {
            //grow
            currTime = std::min(1.f, currTime + updateTime);

            if (currTime == 1)
            {
                direction = 0;
                e.getComponent<cro::Callback>().active = false;
            }
        }

        float scale = cro::Util::Easing::easeOutBack(currTime);
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
    };

    //TODO map these to achievements in some meaningful way
    for (const auto& path : paths)
    {
        if (md.loadFromFile(path))
        {
            auto entity = m_trophyScene.createEntity();
            entity.addComponent<cro::Transform>().setScale(glm::vec3(0.f));
            md.createModel(entity);
            rotateEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            entity.getComponent<cro::Model>().setMaterial(0, flatMaterial);
            entity.getComponent<cro::Model>().setMaterial(1, shinyMaterial);
            entity.getComponent<cro::Model>().setHidden(true);

            entity.addComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(0.f, 0);
            entity.getComponent<cro::Callback>().function = trophyCallback;

            m_trophyEnts.push_back(entity);
        }
    }


    auto resizeCallback = [](cro::Camera& cam)
    {
        auto ratio = TrophyTextureSize.x / TrophyTextureSize.y;
        cam.setPerspective(64.f * cro::Util::Const::degToRad, ratio, 0.3f, 4.5f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    resizeCallback(m_trophyScene.getActiveCamera().getComponent<cro::Camera>());
    m_trophyScene.getActiveCamera().getComponent<cro::Camera>().resizeCallback = resizeCallback;
    m_trophyScene.getActiveCamera().getComponent<cro::Camera>().shadowMapBuffer.create(1024, 1024);
    m_trophyScene.getActiveCamera().getComponent<cro::Camera>().setMaxShadowDistance(2.f);
    m_trophyScene.getActiveCamera().getComponent<cro::Camera>().setShadowExpansion(15.f);
    m_trophyScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.27f, 0.55f });

    auto sunEnt = m_trophyScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -35.f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -40.f * cro::Util::Const::degToRad);
}

void TrophyState::quitState()
{
    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);

    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}