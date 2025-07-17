/*-----------------------------------------------------------------------

Matt Marchant - 2022 - 2025
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

#include "BilliardsState.hpp"
#include "GameConsts.hpp"
#include "MenuConsts.hpp"
#include "CommandIDs.hpp"
#include "PacketIDs.hpp"
#include "MessageIDs.hpp"
#include "NotificationSystem.hpp"
#include "BilliardsSystem.hpp"
#include "../GolfGame.hpp"
#include "../ErrorCheck.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <crogine/core/Mouse.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/util/Wavetable.hpp>

using namespace cl;

namespace
{
    constexpr glm::vec3 TopSpinPosition(50.f, 0.f, 0.f);
    constexpr glm::vec3 TargetBallPosition(100.f, 0.f, 0.f);
    constexpr glm::vec3 TrophyPosition(150.f, 0.f, 0.f);
    constexpr glm::vec3 CamOffset(0.f, 0.f, 0.06f);
}

void BilliardsState::createUI()
{
#ifdef CRO_DEBUG_
    registerWindow(
        [&]()
        {
            ImGui::Begin("Camera");
        
            static const std::array<std::string, CameraID::Count> CameraStrings =
            {
                "Spectate",
                "Overhead",
                "Player",
                "Transition",
            };
            ImGui::Text("Camera: %s", CameraStrings[m_activeCamera].c_str());

            auto& cam = m_cameras[m_activeCamera].getComponent<cro::Camera>();
            float dist = cam.getMaxShadowDistance();
            if (ImGui::SliderFloat("Dist", &dist, 1.f, cam.getFarPlane()))
            {
                cam.setMaxShadowDistance(dist);
            }

            float exp = cam.getShadowExpansion();
            if (ImGui::SliderFloat("Exp", &exp, 1.f, 50.f))
            {
                cam.setShadowExpansion(exp);
            }

            auto& prop = m_cameras[m_activeCamera].getComponent<CameraProperties>();
            float farP = prop.farPlane;
            if (ImGui::SliderFloat("Far", &farP, 1.f, 8.f))
            {
                prop.farPlane = farP;
                cam.resizeCallback(cam);
            }

            float nearP = cam.getNearPlane();
            if (ImGui::SliderFloat("Near", &nearP, 0.1f, farP - 0.1f))
            {
                cam.resizeCallback(cam);
            }

            for (auto i = 0u; i < cam.getCascadeCount(); ++i)
            {
                ImGui::Image(cam.shadowMapBuffer.getTexture(i), { 256.f, 256.f }, { 0.f, 1.f }, { 1.f, 0.f });
            }

            ImGui::End();
        });
#endif

    createMiniballScenes();
    updateTargetTexture(0, -1);
    updateTargetTexture(1, -1);

    auto bufferUpdateCallback = [](cro::Entity e, float)
    {
        //this is activated once to make sure the
        //sprite is up to date with any texture buffer resize
        glm::vec2 texSize = e.getComponent<cro::Sprite>().getTexture()->getSize();
        e.getComponent<cro::Sprite>().setTextureRect({ glm::vec2(0.f), texSize });
        e.getComponent<cro::Transform>().setOrigin(texSize / 2.f);
        e.getComponent<cro::Callback>().active = false;
    };

    //displays the background
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.5f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_gameSceneTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function = bufferUpdateCallback;

    auto backgroundEnt = entity;

    //displays the topspin ball
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.3f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_topspinTexture.getTexture());
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function = bufferUpdateCallback;
    auto topspinEnt = entity;


    //displays the player's current target ball
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.3f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_targetTextures[0].getTexture());
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function = bufferUpdateCallback;
    auto targetEnt0 = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.3f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_targetTextures[1].getTexture());
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function = bufferUpdateCallback;
    auto targetEnt1 = entity;


    //attach UI elements to this so they scale to their parent
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    auto rootNode = entity;


    //player details
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/billiards_ui.spt", m_resources.textures);


    //rim around the potted balls
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("pocket_rim");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -0.1f });
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -std::floor(UIBarHeight * 3.f) };
    entity.getComponent<UIElement>().depth = 0.1f;
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto& menuFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    if (m_sharedData.courseIndex == 2)
    {
        //snooker so add scores...
        auto createScore = [&, entity](glm::vec3 position, std::int32_t client, std::int32_t idx) mutable
        {
            auto ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>().setPosition(position + entity.getComponent<cro::Transform>().getOrigin());
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
            ent.getComponent<cro::Text>().setFillColour(TextNormalColour);
            ent.getComponent<cro::Text>().setString("0");
            ent.addComponent<cro::Callback>().active = true;
            ent.getComponent<cro::Callback>().function =
                [&, client, idx](cro::Entity e, float)
            {
                if (m_localPlayerInfo[client][idx].score > -1)
                {
                    e.getComponent<cro::Text>().setString(std::to_string(m_localPlayerInfo[client][idx].score));
                }
                centreText(e);
            };
            entity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
            return ent;
        };

        static constexpr float Padding = 14.f;
        createScore(glm::vec3((-bounds.width / 2.f) - Padding, std::floor(bounds.height / 3.f), 0.1f), 0, 0);

        if (m_sharedData.localConnectionData.playerCount == 2)
        {
            createScore(glm::vec3((bounds.width / 2.f) + Padding, std::floor(bounds.height / 3.f), 0.1f), 0, 1);
        }
        else
        {
            createScore(glm::vec3((bounds.width / 2.f) + Padding, std::floor(bounds.height / 3.f), 0.1f), 1, 0);
        }
    }


    auto createText = [&](const std::string& str, glm::vec2 relPos, glm::vec2 absPos)
    {
        auto ent = m_uiScene.createEntity();
        ent.addComponent<cro::Transform>();
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Text>(menuFont).setString(str);
        ent.getComponent<cro::Text>().setCharacterSize(UITextSize);
        ent.getComponent<cro::Text>().setFillColour(TextNormalColour);
        ent.addComponent<UIElement>().relativePosition = relPos;
        ent.getComponent<UIElement>().absolutePosition = absPos;
        ent.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::PlayerName;
        ent.addComponent<cro::Callback>().setUserData<std::int32_t>(-1);

        centreText(ent);
        rootNode.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

        return ent;
    };

    auto createPowerBar = [&](std::int32_t playerIndex)
    {
        static constexpr std::array Positions = { glm::vec2(0.13f, 1.f), glm::vec2(1.f - 0.13f, 1.f) };

        auto ent = m_uiScene.createEntity();
        ent.addComponent<cro::Transform>().setRotation((-90.f * cro::Util::Const::degToRad) + ((180.f * playerIndex) * cro::Util::Const::degToRad));
        ent.addComponent<UIElement>().relativePosition = Positions[playerIndex];
        ent.getComponent<UIElement>().absolutePosition = { 0.f, -22.f };
        ent.getComponent<UIElement>().depth = 0.1f;
        ent.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;

        //using negative scale on y to hide when inactive
        auto cueEnt = m_uiScene.createEntity();
        cueEnt.addComponent<cro::Transform>();
        cueEnt.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing(playerIndex));
        cueEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("cue");
        auto bounds = cueEnt.getComponent<cro::Sprite>().getTextureBounds();
        cueEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        cueEnt.getComponent<cro::Transform>().setScale({ 1.f - (2.f * playerIndex), -1.f });
        cueEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::StrengthMeter;
        cueEnt.addComponent<cro::Callback>().setUserData<const std::int32_t>(playerIndex);
        cueEnt.getComponent<cro::Callback>().function =
            [&, bounds](cro::Entity e, float dt)
        {
            auto power = 1.f - m_inputParser.getPower();
            auto yPos = (power * bounds.height) - bounds.height;

            auto pos = e.getComponent<cro::Transform>().getPosition();
            pos.y += (yPos - pos.y) * (dt * 5.f);

            e.getComponent<cro::Transform>().setPosition(pos);
        };
        ent.getComponent<cro::Transform>().addChild(cueEnt.getComponent<cro::Transform>());

        rootNode.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

        ent = m_uiScene.createEntity();
        ent.addComponent<cro::Transform>().setScale({1.f, -1.f});
        ent.addComponent<UIElement>().relativePosition = Positions[playerIndex];
        ent.getComponent<UIElement>().absolutePosition = { 0.f, -30.f };
        ent.getComponent<UIElement>().depth = 0.1f;
        ent.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::StrengthMeter;
        ent.addComponent<cro::Callback>().setUserData<const std::int32_t>(playerIndex);
        ent.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            auto red = m_inputParser.getPower();
            auto green = 1.f - red;
            cro::Colour c(red, green, 0.f);

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto& v : verts)
            {
                v.colour = c;
            }
        };

        std::vector<cro::Vertex2D> verts = 
        {
            cro::Vertex2D(glm::vec2(-bounds.height / 2.f, bounds.width / 4.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(-bounds.height / 2.f, -bounds.width / 4.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(bounds.height / 2.f, bounds.width / 4.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(bounds.height / 2.f, -bounds.width / 4.f), cro::Colour::White)
        };
        ent.addComponent<cro::Drawable2D>().setVertexData(verts);

        rootNode.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
    };

    static constexpr float NameVertPos = 0.95f;
    static constexpr float NameVertOffset = -4.f;
    static constexpr float NameHorPos = 0.2f;
    createText(m_sharedData.connectionData[0].playerData[0].name, glm::vec2(NameHorPos, 1.f), glm::vec2(0.f, NameVertOffset));

    struct NameAnimationCallback final
    {
        float currTime = 0.f;
        void operator() (cro::Entity e, float dt)
        {
            currTime = std::min(1.f, currTime + (dt * 2.f));
            float scale = 1.f + cro::Util::Easing::easeOutCirc(currTime);

            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

            auto colour = TextNormalColour;
            colour.setAlpha(1.f - currTime);

            e.getComponent<cro::Text>().setFillColour(colour);

            if (currTime == 1.f)
            {
                currTime = 0.f;
                e.getComponent<cro::Callback>().active = false;
            }
        }
    };

    auto highlightText = createText(m_sharedData.connectionData[0].playerData[0].name, glm::vec2(NameHorPos, 1.f), glm::vec2(0.f, NameVertOffset));
    highlightText.getComponent<cro::Callback>().setUserData<std::int32_t>(0);
    highlightText.getComponent<cro::Callback>().function = NameAnimationCallback();

    if (m_sharedData.clientConnection.connectionID == 0)
    {
        createPowerBar(0);
    }

    if (m_sharedData.connectionData[0].playerCount == 2)
    {
        createText(m_sharedData.connectionData[0].playerData[1].name, glm::vec2(1.f - NameHorPos, 1.f), glm::vec2(0.f, NameVertOffset));
        highlightText = createText(m_sharedData.connectionData[0].playerData[1].name, glm::vec2(1.f - NameHorPos, 1.f), glm::vec2(0.f, NameVertOffset));

        highlightText.getComponent<cro::Callback>().setUserData<std::int32_t>(1);
        highlightText.getComponent<cro::Callback>().function = NameAnimationCallback();


        createPowerBar(1);
    }
    else
    {
        createText(m_sharedData.connectionData[1].playerData[0].name, glm::vec2(1.f - NameHorPos, 1.f), glm::vec2(0.f, NameVertOffset));
        highlightText = createText(m_sharedData.connectionData[1].playerData[0].name, glm::vec2(1.f - NameHorPos, 1.f), glm::vec2(0.f, NameVertOffset));

        highlightText.getComponent<cro::Callback>().setUserData<std::int32_t>(1);
        highlightText.getComponent<cro::Callback>().function = NameAnimationCallback();

        if (m_sharedData.clientConnection.connectionID == 1)
        {
            createPowerBar(1);
        }
    }


    //free table sign
    auto ftEnt = createText("Free Table", { 0.5f, 0.f }, { 0.f, std::floor(UIBarHeight * 1.5f) + 2.f });
    ftEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    ftEnt.getComponent<cro::CommandTarget>().ID = CommandID::UI::WindSock | CommandID::UI::UIElement; //for the sake of recycling enum...
    ftEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
    ftEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        static constexpr float FlashTime = 0.5f;
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime += dt;
        if (currTime > FlashTime)
        {
            currTime -= FlashTime;

            float scale = e.getComponent<cro::Transform>().getScale().x;
            scale = (scale == 0) ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        }
    };

    //mouse cursor for rotating
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("rotate");
    entity.getComponent<cro::Transform>().setOrigin({ -4.f, -4.f });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (m_inputParser.canRotate() && 
            (cro::Mouse::isButtonPressed(cro::Mouse::Button::Right)
                || cro::GameController::isButtonPressed(/*m_sharedData.inputBinding.controllerID*/-1, m_sharedData.inputBinding.buttons[InputBinding::CamModifier])))
        {
            auto camPos = m_cameraController.getComponent<cro::Transform>().getPosition();
            auto size = m_gameSceneTexture.getSize();
            auto screenPos = m_gameScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(camPos, size);
            screenPos.y = size.y - screenPos.y; //expecting mouse coords which start at the top...
#ifndef __APPLE__
            //TODO pixelToCoords is broken on macOS :(
            auto pos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(screenPos);
            pos.x = std::round(pos.x);
            pos.y = std::round(pos.y);
            pos.z = 0.1f;
            e.getComponent<cro::Transform>().setPosition(pos);
            e.getComponent<cro::Transform>().setScale(m_viewScale);
#endif
        }
        else
        {
            e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
    };
    auto rotateEnt = entity;

    //shaded window bars - updated by callback below
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.5f });
    entity.addComponent<cro::Drawable2D>();
    auto infoEnt = entity;


    //game over window
    spriteSheet.loadFromFile("assets/golf/sprites/lobby_menu.spt", m_resources.textures);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({0.f, 0.01f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("versus");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().depth = 0.6f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::Scoreboard;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 2.f));

        auto scale = e.getComponent<cro::Transform>().getScale();
        if (scale.x < 1)
        {
            scale.x = cro::Util::Easing::easeOutBounce(currTime);

            if (currTime == 1)
            {
                currTime = scale.y;
            }
        }
        else
        {
            scale.y = cro::Util::Easing::easeOutBounce(currTime);

            if (currTime == 1)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        }
        e.getComponent<cro::Transform>().setScale(scale);
    };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto scoreEnt = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -80.f, 77.f, 0.1f });
    entity.getComponent<cro::Transform>().move(scoreEnt.getComponent<cro::Transform>().getOrigin());
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(menuFont).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(m_sharedData.connectionData[0].playerData[0].name);
    centreText(entity);
    scoreEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 77.f, 0.1f });
    entity.getComponent<cro::Transform>().move(scoreEnt.getComponent<cro::Transform>().getOrigin());
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(menuFont).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    if (m_sharedData.connectionData[0].playerCount == 2)
    {
        entity.getComponent<cro::Text>().setString(m_sharedData.connectionData[0].playerData[1].name);
    }
    else
    {
        entity.getComponent<cro::Text>().setString(m_sharedData.connectionData[1].playerData[0].name);
    }
    centreText(entity);
    scoreEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //dispays the trophy
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(scoreEnt.getComponent<cro::Transform>().getOrigin());
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>().setTexture(m_trophyTexture.getTexture());
    entity.addComponent<cro::Callback>().function = bufferUpdateCallback;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -0.1f });
    scoreEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto trophyEnt = entity;

    //stash these for showing the summary
    spriteSheet.loadFromFile("assets/golf/sprites/scoreboard.spt", m_resources.textures);

    m_sprites[SpriteID::QuitReady] = spriteSheet.getSprite("quit_ready");
    m_sprites[SpriteID::QuitNotReady] = spriteSheet.getSprite("quit_not_ready");


    //displays the balls which have been pocketed
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>().setTexture(m_pocketedTexture.getTexture());
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().function = bufferUpdateCallback;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -std::floor(UIBarHeight * 3.f) };
    entity.getComponent<UIElement>().depth = -0.1f;
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto pocketEnt = entity;



    //ui viewport is set 1:1 with window, then the scene
    //is scaled to best-fit to maintain pixel accuracy of text.
    auto updateView = [&, rootNode, backgroundEnt, infoEnt, topspinEnt, targetEnt0, targetEnt1, trophyEnt, pocketEnt, rotateEnt](cro::Camera& cam) mutable
    {
        auto windowSize = GolfGame::getActiveTarget()->getSize();
        glm::vec2 size(windowSize);

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);

        glm::vec2 courseScale(m_sharedData.pixelScale ? m_viewScale.x : 1.f);
        backgroundEnt.getComponent<cro::Transform>().setScale(courseScale);
        backgroundEnt.getComponent<cro::Callback>().active = true; //makes sure to delay so updating the texture size is complete first
        backgroundEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -5.f));

        topspinEnt.getComponent<cro::Transform>().setScale(courseScale);
        topspinEnt.getComponent<cro::Callback>().active = true;
        topspinEnt.getComponent<cro::Transform>().setPosition({ size.x / 2.f, size.y * NameVertPos, -0.3f });

        targetEnt0.getComponent<cro::Transform>().setScale(courseScale);
        targetEnt0.getComponent<cro::Callback>().active = true;
        targetEnt0.getComponent<cro::Transform>().setPosition({ size.x * 0.425f, size.y * NameVertPos, -0.3f });

        targetEnt1.getComponent<cro::Transform>().setScale(courseScale);
        targetEnt1.getComponent<cro::Callback>().active = true;
        targetEnt1.getComponent<cro::Transform>().setPosition({ size.x * 0.575f, size.y * NameVertPos, -0.3f });

        trophyEnt.getComponent<cro::Transform>().setScale(courseScale / m_viewScale.x);
        trophyEnt.getComponent<cro::Callback>().active = true;

        pocketEnt.getComponent<cro::Transform>().setScale(courseScale / m_viewScale.x);
        pocketEnt.getComponent<cro::Callback>().active = true;

        rotateEnt.getComponent<cro::Transform>().setScale(m_viewScale);

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::UIElement;
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
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


        //update the overlay (top/bottom bar)
        const auto uiSize = size / m_viewScale;
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
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void BilliardsState::showReadyNotify(const BilliardsPlayer& player)
{
    auto score = player.score - m_localPlayerInfo[player.client][player.player].score;
    if (player.score > 0
        && score > 0)
    {
        auto* m = getContext().appInstance.getMessageBus().post<BilliardBallEvent>(MessageID::BilliardsMessage);
        m->type = BilliardBallEvent::Score;
        m->data = score;
    }

    m_localPlayerInfo[player.client][player.player].score = player.score;

    m_wantsNotify = (player.client == m_sharedData.localConnectionData.connectionID);

    cro::String msg = m_sharedData.connectionData[player.client].playerData[player.player].name + "'s Turn";

    if (m_wantsNotify)
    {
        if (cro::GameController::getControllerCount() > 0)
        {
            msg += " (Press A Button)";
        }
        else
        {
            msg += " (Press " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) + ")";
        }
    }
    showNotification(msg);

    auto playerIndex = player.client | player.player;

    //show the active player's strength meter
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::StrengthMeter;
    cmd.action = [playerIndex](cro::Entity e, float)
    {
        if (e.getComponent<cro::Callback>().getUserData<const std::int32_t>() == playerIndex)
        {
            auto scale = e.getComponent<cro::Transform>().getScale();
            scale.y = 1.f;
            e.getComponent<cro::Transform>().setScale(scale);
            e.getComponent<cro::Callback>().active = true;
        }
        else
        {
            auto scale = e.getComponent<cro::Transform>().getScale();
            scale.y = -1.f;
            e.getComponent<cro::Transform>().setScale(scale);
            e.getComponent<cro::Callback>().active = false;
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    //and highlight effect
    cmd.targetFlags = CommandID::UI::PlayerName;
    cmd.action = [playerIndex](cro::Entity e, float)
    {
        if (e.getComponent<cro::Callback>().getUserData<const std::int32_t>() == playerIndex)
        {
            e.getComponent<cro::Callback>().active = true;
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    m_sharedData.inputBinding.playerID = player.player;
}

void BilliardsState::showNotification(const cro::String& msg)
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 8.f, 12.f * m_viewScale.y });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::UI));
    entity.getComponent<cro::Text>().setCharacterSize(8u * static_cast<std::uint32_t>(m_viewScale.y));
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<Notification>().message = msg;
    entity.getComponent<Notification>().speed = 2.f;
}

void BilliardsState::showGameEnd(const BilliardsPlayer& player)
{
    //this may be from a player quitting mid-summary
    if (!m_gameEnded)
    {
        //show summary screen
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::Scoreboard;
        cmd.action = [&, player](cro::Entity ent, float)
        {
            auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
            
            //countdown
            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ ent.getComponent<cro::Transform>().getOrigin().x, 31.f, 0.23f});
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
            entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::uint8_t>>(1.f, ConstVal::SummaryTimeout);
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

                e.getComponent<cro::Text>().setString("Returning to lobby in: " + std::to_string(sec));

                auto bounds = cro::Text::getLocalBounds(e);
                bounds.width = std::floor(bounds.width / 2.f);
                e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
            };

            ent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            //winner name
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ ent.getComponent<cro::Transform>().getOrigin().x, 45.f, 0.23f });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
            entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
            entity.getComponent<cro::Text>().setString(m_sharedData.connectionData[player.client].playerData[player.player].name + " Wins!");
            centreText(entity);

            ent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            ent.getComponent<cro::Callback>().active = true;
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


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

            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::Drawable2D>().setTexture(m_sprites[SpriteID::QuitNotReady].getTexture());
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, readyRect, unreadyRect, posOffset](cro::Entity e, float)
            {
                auto& tx = e.getComponent<cro::Transform>();
                tx.setPosition({ 13.f, (UIBarHeight * 2.f) + 10.f, 2.f });
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

        //update stats (only active if in a net game so we only need check if this is our client)
        if (player.client == m_sharedData.clientConnection.connectionID)
        {
            switch (m_gameMode)
            {
            default: break;
            case TableData::Eightball:
                Achievements::incrementStat(StatStrings[StatID::EightballWon]);
                Achievements::awardAchievement(AchievementStrings[AchievementID::Spots]);
                break;
            case TableData::Nineball:
                Achievements::incrementStat(StatStrings[StatID::NineballWon]);
                Achievements::awardAchievement(AchievementStrings[AchievementID::Stripes]);
                break;
            case TableData::Snooker:
                Achievements::incrementStat(StatStrings[StatID::SnookerWon]);
                Achievements::awardAchievement(AchievementStrings[AchievementID::EasyPink]);
                break;
            }
        }
        m_gameEnded = true;

        //plays an applause
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::AudioEmitter>() = m_audioScape.getEmitter("applause");
        entity.getComponent<cro::AudioEmitter>().play();

        //in a net game play win/lose audio
        if (m_sharedData.localConnectionData.playerCount == 1)
        {
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<float>(1.f);
            entity.getComponent<cro::Callback>().function =
                [&, player](cro::Entity e, float dt)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime -= dt;

                if (currTime < 0)
                {
                    auto* msg = getContext().appInstance.getMessageBus().post<BilliardBallEvent>(MessageID::BilliardsMessage);
                    msg->type = BilliardBallEvent::GameEnded;

                    if (player.client == m_sharedData.clientConnection.connectionID)
                    {
                        //we win
                        msg->data = 0;
                    }
                    else
                    {
                        //we lost :(
                        msg->data = 1;
                    }

                    e.getComponent<cro::Callback>().active = true;
                    m_uiScene.destroyEntity(e);
                }
            };
        }
    }

    m_inputParser.setActive(false, false);
}

void BilliardsState::toggleQuitReady()
{
    if (m_gameEnded)
    {
        m_sharedData.clientConnection.netClient.sendPacket<std::uint8_t>(PacketID::ReadyQuit, m_sharedData.clientConnection.connectionID, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
}

void BilliardsState::createMiniballScenes()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/golf/models/hole_19/topspin.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(TopSpinPosition);
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
        applyMaterialData(md, material);

        glm::vec4 rect;
        rect.x = 0.f;
        rect.y = 0.f;
        rect.z = 1.f;
        rect.w = 1.f;
        material.setProperty("u_subrect", rect);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setRotation(m_inputParser.getSpinOffset());
        };
    }

    auto camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(TopSpinPosition + CamOffset);

    auto& cam = camEnt.addComponent<cro::Camera>();
    auto resizeCallback = [&](cro::Camera& cam)
    {
        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, 1.f, 0.01f, 0.5f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    cam.resizeCallback = resizeCallback;
    cam.isStatic = true;
    resizeCallback(cam);

    m_topspinCamera = camEnt;


    //renders the player's current target ball
    if (md.loadFromFile("assets/golf/models/hole_19/billiard_ball.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(TargetBallPosition);
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
        applyMaterialData(md, material);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        if (m_ballTexture.textureID)
        {
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", m_ballTexture);
        }

        m_targetBall = entity;
    }

    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(TargetBallPosition + CamOffset);
    auto& cam2 = camEnt.addComponent<cro::Camera>();
    cam2.resizeCallback = resizeCallback;
    cam2.isStatic = true;
    resizeCallback(cam2);

    m_targetCamera = camEnt;


    if (md.loadFromFile("assets/golf/models/trophies/trophy07.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(TrophyPosition);

        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::TrophyBase]);
        applyMaterialData(md, material);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        material = m_resources.materials.get(m_materialIDs[MaterialID::Trophy]);
        applyMaterialData(md, material);
        entity.getComponent<cro::Model>().setMaterial(1, material);


        struct TrophyCallback final
        {
            const std::vector<float> wavetable = cro::Util::Wavetable::sine(0.25f);
            std::size_t index = 0;

            void operator()(cro::Entity e, float)
            {
                float rotation = (cro::Util::Const::PI / 4.f) * wavetable[index];
                index = (index + 1) % wavetable.size();

                e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
            };
        };
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = TrophyCallback();
    }

    //displays trophy on round end panel
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(TrophyPosition + glm::vec3(0.f, 0.125f, 0.32f));
    auto& cam3 = camEnt.addComponent<cro::Camera>();
    cam3.resizeCallback = resizeCallback;
    cam3.isStatic = true;
    resizeCallback(cam3);

    m_trophyCamera = camEnt;


    //displays balls pocketed at the top of the screen
    
    auto pocketCallback = [&](cro::Camera& cam)
    {
        auto size = glm::vec2(m_pocketedTexture.getSize());
        auto ratio = (BilliardBall::Radius * 2.f) / size.y;
        size *= ratio;
        size /= 2.f;

        cam.setOrthographic(-size.x, size.x, -size.y, size.y, 0.01f, 1.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(glm::vec3(-100.f, 0.f, 0.f) + CamOffset);
    auto& cam4 = camEnt.addComponent<cro::Camera>();
    cam4.resizeCallback = pocketCallback;
    //cam4.shadowMapBuffer.create(1024, 1024);
    pocketCallback(cam4);

    m_pocketedCamera = camEnt;
}

void BilliardsState::updateTargetTexture(std::int32_t playerIndex, std::int8_t ballID)
{
    m_targetTextures[playerIndex].clear(cro::Colour::Transparent);
    if (ballID > 0 && m_targetBall.isValid())
    {
        //set material UV
        auto rect = getSubrect(ballID);
        m_targetBall.getComponent<cro::Model>().setMaterialProperty(0, "u_subrect", rect);

        auto oldCam = m_gameScene.setActiveCamera(m_targetCamera);
        m_gameScene.render();
        m_gameScene.setActiveCamera(oldCam);
    }
    m_targetTextures[playerIndex].display();
}
