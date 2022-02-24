/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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
#include "../ErrorCheck.hpp"
#include "../Achievements.hpp"
#include "../AchievementStrings.hpp"

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

#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Random.hpp>

namespace
{
    static constexpr float ColumnWidth = 20.f;
    static constexpr float ColumnHeight = 276.f;
    static constexpr std::array ColumnPositions =
    {
        glm::vec2(10.f, ColumnHeight),
        glm::vec2(ColumnWidth * 6.f, ColumnHeight),
        glm::vec2(ColumnWidth * 7.f, ColumnHeight),
        glm::vec2(ColumnWidth * 8.f, ColumnHeight),
        glm::vec2(ColumnWidth * 9.f, ColumnHeight),
        glm::vec2(ColumnWidth * 10.f, ColumnHeight),
        glm::vec2(ColumnWidth * 11.f, ColumnHeight),
        glm::vec2(ColumnWidth * 12.f, ColumnHeight),
        glm::vec2(ColumnWidth * 13.f, ColumnHeight),
        glm::vec2(ColumnWidth * 14.f, ColumnHeight),
        glm::vec2(ColumnWidth * 15.f, ColumnHeight),
    };
}

void GolfState::buildUI()
{
    if (m_holeData.empty())
    {
        return;
    }

    auto resizeCallback = [](cro::Entity e, float)
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
    entity.getComponent<UIElement>().depth = 0.05f;
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
    entity.getComponent<UIElement>().depth = 0.05f;
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

    auto windDial = entity;
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

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PowerBar];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec3(bounds.width / 2.f, bounds.height / 2.f, -0.05f));
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //power bar
    auto barEnt = entity;
    auto barCentre = bounds.width / 2.f;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(5.f, 0.f, 0.05f)); //TODO expel the magic number!!
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


    //hole number
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(38.f, -12.f));
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
        float scale = 1.f;
        float rotation = -1.f;
    };
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 82.f });
    entity.getComponent<cro::Transform>().setRotation(-90.f * cro::Util::Const::degToRad);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MiniMap;
    entity.addComponent<cro::Callback>().setUserData<MinimapData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [state, scale, rotation] = e.getComponent<cro::Callback>().getUserData<MinimapData>();
        float speed = dt * 4.f;
        float newScale = 0.f;
        
        if (state == 0)
        {
            //shrinking
            scale = std::max(0.f, scale - speed);
            newScale = cro::Util::Easing::easeOutSine(scale);

            if (scale == 0)
            {
                //orientation - sets tee to bottom of map
                if (m_currentHole == 0
                    || m_holeData[m_currentHole].modelEntity != m_holeData[m_currentHole - 1].modelEntity)
                {
                    if (m_holeData[m_currentHole].tee.x > 160)
                    {
                        e.getComponent<cro::Transform>().setRotation(-90.f * cro::Util::Const::degToRad);
                        rotation = -1.f;
                    }
                    else
                    {
                        e.getComponent<cro::Transform>().setRotation(90.f * cro::Util::Const::degToRad);
                        rotation = 1.f;
                    }
                }
                glm::vec2 offset = glm::vec2(2.f * -rotation);
                m_flagQuad.setRotation(-rotation * 90.f);


                //update render
                auto oldCam = m_gameScene.setActiveCamera(m_mapCam);
                m_mapBuffer.clear(cro::Colour::Transparent);
                m_gameScene.render();
                m_mapBuffer.display();
                m_gameScene.setActiveCamera(oldCam);

                m_mapQuad.setPosition(offset);
                m_mapQuad.setColour(cro::Colour(0.396f,0.263f,0.184f));
                m_mapTexture.clear(cro::Colour::Transparent);
                m_mapQuad.draw();

                m_mapQuad.setPosition(glm::vec2(0.f));
                m_mapQuad.setColour(cro::Colour::White);
                m_mapQuad.draw();

                auto holePos = m_holeData[m_currentHole].pin / 2.f;
                m_flagQuad.setPosition({ holePos.x, -holePos.z });
                m_flagQuad.draw();
                m_mapTexture.display();

                //and set to grow
                state = 1;

                //disable the cam again
                m_mapCam.getComponent<cro::Camera>().active = false;
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
        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f, newScale));
    };
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto mapEnt = entity;

    //ball icon on mini map
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
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

    //stroke indicator
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    auto endColour = TextGoldColour;
    endColour.setAlpha(0.f);
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.5f, 0.5f), endColour),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), endColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setRotation(m_inputParser.getYaw());
        auto pos = m_currentPlayer.position;
        pos.y = -pos.z;
        pos.z = 1.f;
        e.getComponent<cro::Transform>().setPosition(pos / 2.f);

        if (!m_inputParser.getActive())
        {
            e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
        else
        {
            auto club = getClub();
            switch (club)
            {
            default: 
                e.getComponent<cro::Transform>().setScale(glm::vec2(Clubs[club].target, 1.f));
                break;
            case ClubID::Putter:
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                break;
            }
            
        }
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

    //arrow pointing to player position on the green
    //kinda made redundant now that the slope indicator is no longer visible
    //on the mini view
    /*entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(42.f, -1.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(42.f, 1.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(32.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, greenEnt](cro::Entity e, float dt)
    {
        if (m_currentPlayer.terrain == TerrainID::Green)
        {
            auto offset = greenEnt.getComponent<cro::Transform>().getOrigin();
            e.getComponent<cro::Transform>().setPosition({ offset.x, offset.y });

            auto& currentRotation = e.getComponent<cro::Callback>().getUserData<float>();
            auto dir = m_currentPlayer.position - m_holeData[m_currentHole].pin;
            auto targetRotation = std::atan2(-dir.z, dir.x);

            float rotation = cro::Util::Maths::shortestRotation(currentRotation, targetRotation) * dt;
            currentRotation += rotation;
            e.getComponent<cro::Transform>().setRotation(currentRotation);
        }
    };
    greenEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());*/

    createScoreboard();


    //set up the overhead cam for the mini map
    auto updateMiniView = [&, mapEnt](cro::Camera& miniCam) mutable
    {
        glm::uvec2 previewSize = MapSize / 2u;

        m_mapBuffer.create(previewSize.x, previewSize.y);
        m_mapQuad.setTexture(m_mapBuffer.getTexture());
        m_mapTexture.create(previewSize.x, previewSize.y);
        mapEnt.getComponent<cro::Sprite>().setTexture(m_mapTexture.getTexture());
        mapEnt.getComponent<cro::Transform>().setOrigin({ previewSize.x / 2.f, previewSize.y / 2.f });

        miniCam.setOrthographic(0.f, static_cast<float>(MapSize.x), 0.f, static_cast<float>(MapSize.y), -0.1f, 20.f);
        miniCam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    m_mapCam = m_gameScene.createEntity();
    m_mapCam.addComponent<cro::Transform>().setPosition({ 0.f, 10.f, 0.f });
    m_mapCam.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    auto& miniCam = m_mapCam.addComponent<cro::Camera>();
    miniCam.renderFlags = RenderFlags::MiniMap;
    miniCam.active = false;
    //miniCam.resizeCallback = updateMiniView; //don't do this on resize as recreating the buffer clears it..
    updateMiniView(miniCam);



    //and the mini view of the green
    auto updateGreenView = [&, greenEnt](cro::Camera& greenCam) mutable
    {
        auto texSize = MapSize.y / 2u;

        auto windowScale = std::floor(cro::App::getWindow().getSize().y / calcVPSize().y);
        float scale = m_sharedData.pixelScale ? windowScale : 1.f;
        scale = (windowScale + 1.f) - scale;
        texSize *= static_cast<std::uint32_t>(scale);

        m_greenBuffer.create(texSize, texSize); //yes, it's square
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
    updateGreenView(greenCam);

    m_greenCam.addComponent<cro::Callback>().active = true;
    m_greenCam.getComponent<cro::Callback>().setUserData<MiniCamData>();
    m_greenCam.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<MiniCamData>();
        auto diff = data.targetSize - data.currentSize;
        data.currentSize += diff * (dt * 4.f);

        auto& cam = e.getComponent<cro::Camera>();
        cam.setOrthographic(-data.currentSize, data.currentSize, -data.currentSize, data.currentSize, -0.15f, 1.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };


    //callback for the UI camera when window is resized
    auto updateView = [&, trophyEnt, courseEnt, infoEnt, windEnt, mapEnt, greenEnt, rootNode](cro::Camera& cam) mutable
    {
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.5f, 20.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();
        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
        auto texSize = glm::vec2(m_gameSceneTexture.getSize());

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
        mapEnt.getComponent<cro::Transform>().setPosition({ uiSize.x - mapSize.y, uiSize.y - (mapSize.x) - (UIBarHeight * 1.5f) }); //map sprite is rotated 90


        greenEnt.getComponent<cro::Transform>().setPosition({ 2.f, uiSize.y - (MapSize.y / 2) - UIBarHeight - 2.f });
        greenEnt.getComponent<cro::Transform>().move(glm::vec2(static_cast<float>(MapSize.y) / 4.f));

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

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.renderFlags = ~RenderFlags::Reflection;
    cam.resizeCallback = updateView;
    updateView(cam);
    m_uiScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 5.f });

    //camera for rendering player reflection
    //wow all this effort I went to now unused... >.<
    //imma just leave it here as a tribute.

    //auto reflectionResize = [&](cro::Camera& cam)
    //{
    //    auto s = glm::vec2(m_gameSceneTexture.getSize());
    //    auto s2 = glm::vec2(GolfGame::getActiveTarget()->getSize());
    //    auto diff = (s - s2) / 2.f;
    //    auto diffScaled = ((s - (s2 / m_viewScale)) / 2.f) * m_viewScale.y;

    //    cam.setOrthographic(-diff.x, s.x - diff.x, -diffScaled.y, s.y - diffScaled.y, -2.f, 10.f);

    //    float vpW = 1.f / m_viewScale.x;
    //    float vpH = 1.f / m_viewScale.y;
    //    cam.viewport = { (1.f - vpW) / 2.f, 0.f, vpW, vpH };
    //};
    ////we'll manually set the drawlist for this cam to prevent unnecessary
    ////sorting at run-time.
    //entity = m_uiScene.createEntity();
    //entity.addComponent<cro::Transform>();
    //auto& uiCam = entity.addComponent<cro::Camera>();
    //uiCam.resizeCallback = reflectionResize;
    //uiCam.renderFlags = RenderFlags::Reflection;
    //uiCam.active = false;
    //uiCam.getDrawList(cro::Camera::Pass::Final)[m_uiScene.getSystem<cro::RenderSystem2D>()->getType()] = std::make_any<std::vector<cro::Entity>>(reflectionList);
    //reflectionResize(uiCam);
    //m_uiReflectionCam = entity;
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
            //remember this is auto-disabled if the player is not the only one on the client
            Achievements::awardAchievement(AchievementStrings[AchievementID::LeaderOfThePack]);
        }
    }
    Achievements::incrementStat(StatStrings[StatID::TotalRounds]);

    auto trophyCount = std::min(std::size_t(3), m_statBoardScores.size());
    float trophyStat = 1.f - (1.f / m_statBoardScores.size()); //in other words, 0 if we're the only player

    for (auto i = 0u; i < trophyCount; ++i)
    {
        if (m_statBoardScores.size() > 1 &&
            m_statBoardScores[i].client == m_sharedData.clientConnection.connectionID)
        {
            Achievements::incrementStat(StatStrings[StatID::GoldAverage + i], trophyStat);
        }

        m_trophies[i].getComponent<TrophyDisplay>().state = TrophyDisplay::In;
        m_trophyLabels[i].getComponent<cro::Callback>().active = true;

        m_trophyLabels[i].getComponent<cro::Sprite>().setTexture(m_sharedData.nameTextures[m_statBoardScores[i].client].getTexture(), false);
        auto bounds = m_trophyLabels[i].getComponent<cro::Sprite>().getTextureBounds();
        bounds.bottom = bounds.height * m_statBoardScores[i].player;
        m_trophyLabels[i].getComponent<cro::Sprite>().setTextureRect(bounds);
    }

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 200.f, 10.f, 0.23f }); //attaches to scoreboard which is fixed size
    entity.addComponent<cro::Drawable2D>();
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
}

void GolfState::createScoreboard()
{
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/scoreboard.spt", m_resources.textures);

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
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::Scoreboard;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("border");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 200.f, 293.f, 0.5f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("LEADERS");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    bounds = cro::Text::getLocalBounds(entity);
    bounds.width = std::floor(bounds.width / 2.f);
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().setOrigin({ -6.f, 253.f, -0.2f});
    auto bgSprite = spriteSheet.getSprite("background");;
    entity.addComponent<cro::Sprite>() = bgSprite;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    cro::FloatRect bgCrop({ 0.f, bounds.height - 266.f, 389.f, 266.f });
    entity.addComponent<cro::Drawable2D>().setCroppingArea(bgCrop);

    bgSprite = spriteSheet.getSprite("board");;
    m_leaderboardTexture.init(bgSprite, font);

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
        steps = 0;

        //update the cropping
        cro::FloatRect crop(0.f, -pos.y, 400.f, -256.f);

        auto& ents = bgEnt.getComponent<cro::Callback>().getUserData<std::vector<cro::Entity>>();
        for (auto ent : ents)
        {
            ent.getComponent<cro::Drawable2D>().setCroppingArea(crop);
        }
        crop = bgCrop;
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

        scrollEnt.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
        i++;
    }
    ents.back().getComponent<cro::Transform>().setPosition(glm::vec3(ColumnPositions.back(), 0.5f));

    updateScoreboard();
}

void GolfState::updateScoreboard()
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::Scoreboard;
    cmd.action = [&](cro::Entity e, float)
    {
        struct ScoreEntry final
        {
            cro::String name;
            std::vector<std::uint8_t> holes;
            std::int32_t frontNine = 0;
            std::int32_t backNine = 0;
            std::int32_t total = 0;
        };

        std::vector<ScoreEntry> scores;
        m_statBoardScores.clear();

        std::uint32_t playerCount = 0;
        auto holeCount = m_holeData.size();
        std::int32_t clientID = 0;
        for (const auto& client : m_sharedData.connectionData)
        {
            playerCount += client.playerCount;

            for (auto i = 0u; i < client.playerCount; ++i)
            {
                auto& entry = scores.emplace_back();
                entry.name = client.playerData[i].name;
                for (auto j = 0u; j < client.playerData[i].holeScores.size(); ++j)
                {
                    entry.holes.push_back(client.playerData[i].holeScores[j]);
                    if (j < 9)
                    {
                        entry.frontNine += client.playerData[i].holeScores[j];
                    }
                    else
                    {
                        entry.backNine += client.playerData[i].holeScores[j];
                    }
                }
                entry.total = entry.frontNine + entry.backNine;

                //for stat/achievment tracking
                auto& leaderboardEntry = m_statBoardScores.emplace_back();
                leaderboardEntry.client = clientID;
                leaderboardEntry.player = i;
                leaderboardEntry.score = entry.total;
            }
            clientID++;
        }

        std::sort(m_statBoardScores.begin(), m_statBoardScores.end(),
            [](const StatBoardEntry& a, const StatBoardEntry& b)
            {
                return a.score < b.score;
            });


        //auto playerCount = 16u;
        //auto holeCount = 18u;

        ////dummy data for layout testing
        //for (auto i = 0u; i < playerCount; ++i)
        //{
        //    auto& entry = scores.emplace_back();
        //    entry.name = "Player " + std::to_string(i);
        //    for (auto j = 0u; j < holeCount; ++j)
        //    {
        //        entry.holes.push_back(j);
        //    }
        //}


        auto& ents = e.getComponent<cro::Callback>().getUserData<std::vector<cro::Entity>>();
        std::sort(scores.begin(), scores.end(),
            [](const ScoreEntry& a, const ScoreEntry& b)
            {
                return a.total < b.total;
            });


        std::size_t page2 = 0;
        static constexpr std::size_t MaxCols = 9;
        if (holeCount > m_scoreColumnCount)
        {
            page2 = std::min(MaxCols, holeCount - m_scoreColumnCount);
        }

        //store the strings to update the leaderboard texture with
        std::vector<LeaderboardEntry> leaderboardEntries;

        //name column
        cro::String nameString = "HOLE\nPAR";
        for (auto i = 0u; i < playerCount; ++i)
        {
            nameString += "\n" + scores[i].name;
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
                nameString += "\n" + scores[i].name;
            }
        }
        ents[0].getComponent<cro::Text>().setString(nameString);
        leaderboardEntries.emplace_back(ents[0].getComponent<cro::Transform>().getPosition(), nameString);

        //score columns
        for (auto i = 1u; i < ents.size() - 1; ++i)
        {
            std::string scoreString = std::to_string(i) + "\n" + std::to_string(m_holeData[i - 1].par);

            for (auto j = 0u; j < playerCount; ++j)
            {
                scoreString += "\n" + std::to_string(scores[j].holes[i - 1]);
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
                        scoreString += "\n" + std::to_string(scores[j].holes[holeIndex]);
                    }
                }
            }

            ents[i].getComponent<cro::Text>().setString(scoreString);
            leaderboardEntries.emplace_back(ents[i].getComponent<cro::Transform>().getPosition(), scoreString);
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
                totalString += "\n" + std::to_string(scores[i].backNine) + separator + std::to_string(scores[i].total);
            }
        }

        ents.back().getComponent<cro::Text>().setString(totalString);
        leaderboardEntries.emplace_back(ents.back().getComponent<cro::Transform>().getPosition(), totalString);

        //for some reason we have to hack this to display and I'm too lazy to debug it
        auto pos = ents.back().getComponent<cro::Transform>().getPosition();
        pos.z = 1.5f;
        ents.back().getComponent<cro::Transform>().setPosition(pos);


        m_leaderboardTexture.update(leaderboardEntries);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void GolfState::showScoreboard(bool visible)
{
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
    if (m_roundEnded)
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
}

void GolfState::updateWindDisplay(glm::vec3 direction)
{
    float rotation = std::atan2(-direction.z, direction.x);

    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::WindSock;
    cmd.action = [&, rotation](cro::Entity e, float dt)
    {
        //nice idea but there's a little too much lag.
        //might work better if we switch as quickly as we
        //switch cameras, so maybe revisit this. For now
        //let's leave it as a problem for future me.
        /*auto target = m_cameras[m_currentCamera].getComponent<TargetInfo>().targetLookAt;
        
        auto targetDir = target - m_cameras[m_currentCamera].getComponent<cro::Transform>().getPosition();
        auto camRotation = std::atan2(-targetDir.z, targetDir.x);*/

        auto r = rotation - m_camRotation;

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

    cmd.targetFlags = CommandID::UI::WindSpeed;
    cmd.action = [direction](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().setUserData<float>(-direction.y);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void GolfState::showMessageBoard(MessageBoardID messageType)
{
    auto bounds = m_sprites[SpriteID::MessageBoard].getTextureBounds();
    auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    auto position = glm::vec3(size.x / 2.f, size.y / 2.f /*- (((bounds.height / 2.f) + UIBarHeight + 2.f) * m_viewScale.y)*/, 0.05f);

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

    //add mini graphic depending on message type
    auto imgEnt = m_uiScene.createEntity();
    imgEnt.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, 0.01f });
    imgEnt.getComponent<cro::Transform>().move(glm::vec2(0.f, -6.f));
    imgEnt.addComponent<cro::Drawable2D>();

    switch (messageType)
    {
    default: break;
    case MessageBoardID::HoleScore:
    {
        std::int32_t score = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole];
        score -= m_holeData[m_currentHole].par;
        auto overPar = score;
        score += ScoreID::ScoreOffset;

        //if this is a remote player the score won't
        //have arrived yet, so kludge this here so the
        //display type is correct.
        if (m_currentPlayer.client != m_sharedData.clientConnection.connectionID)
        {
            score++;
        }
        else
        {
            //if this is a local player check to see if they got the boomerang achievement
            if (m_hadFoul && overPar < 1)
            {
                Achievements::awardAchievement(AchievementStrings[AchievementID::Boomerang]);
            }
        }

        auto* msg = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
        msg->type = GolfEvent::Scored;
        msg->score = static_cast<std::uint8_t>(score);

        if (score < ScoreID::Count)
        {
            textEnt.getComponent<cro::Text>().setString(ScoreStrings[score]);
            textEnt.getComponent<cro::Transform>().move({ 0.f, -10.f, 0.f });
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
        textEnt.getComponent<cro::Text>().setString(m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].name);
        textEnt2.getComponent<cro::Text>().setString("Stroke: " + std::to_string(m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole] + 1));
        break;
    case MessageBoardID::Scrub:
    case MessageBoardID::Water:
        textEnt.getComponent<cro::Text>().setString("Foul!");
        textEnt2.getComponent<cro::Text>().setString("1 Stroke Penalty");
        imgEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::Foul];
        bounds = m_sprites[SpriteID::Foul].getTextureBounds();
        break;
    }
    imgEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, 0.f });

    centreText(textEnt);
    centreText(textEnt2);
    
    entity.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(textEnt2.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(imgEnt.getComponent<cro::Transform>());

    //callback for anim/self destruction
    struct MessageAnim final
    {
        enum
        {
            Delay, Open, Hold, Close
        }state = Delay;
        float currentTime = 0.5f;
    };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<MessageAnim>();
    entity.getComponent<cro::Callback>().function =
        [&, textEnt, textEnt2, imgEnt, messageType](cro::Entity e, float dt)
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
        if (data[0] < 4
            && m_sharedData.connectionData[data[0]].playerCount != 0
            && data[1] < AchievementID::Count - 1)
        {
            auto name = m_sharedData.connectionData[data[0]].playerData[0].name;
            auto achievement = AchievementLabels[data[1]];

            showNotification(name + " achieved " + achievement);
        }
    }
}

void GolfState::showNotification(const std::string& msg)
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f, UIBarHeight * m_viewScale.y * 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::UI));
    entity.getComponent<cro::Text>().setCharacterSize(8u * static_cast<std::uint32_t>(m_viewScale.y));
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<Notification>().message = msg;
}

void GolfState::buildTrophyScene()
{
    auto updateCam = [&](cro::Camera& cam)
    {
        auto vpSize = calcVPSize();

        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        float maxScale = std::floor(winSize.y / vpSize.y);
        float scale = m_sharedData.pixelScale ? maxScale : 1.f;
        auto texSize = winSize / scale;
        m_trophySceneTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y));

        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, winSize.x / winSize.y, 0.1f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto& cam = m_trophyScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = updateCam;
    cam.isStatic = true;
    updateCam(cam);

    const std::array<std::pair<std::string, glm::vec3>, 3u> Paths =
    {
        std::make_pair("assets/golf/models/trophy01.cmt", glm::vec3(0.f, -0.3f, -1.f)),
        std::make_pair("assets/golf/models/trophy02.cmt", glm::vec3(-0.5f, -0.35f, -1.f)),
        std::make_pair("assets/golf/models/trophy03.cmt", glm::vec3(0.5f, -0.4f, -1.f))        
    };

    cro::EmitterSettings emitterSettings;
    emitterSettings.loadFromFile("assets/golf/particles/firework.cps", m_resources.textures);

    if (emitterSettings.releaseCount == 0)
    {
        emitterSettings.releaseCount = 10;
    }

    cro::AudioScape as;
    as.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);

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

            auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]); //doesn't pixel fade like Cel does.
            applyMaterialData(md, material);
            entity.getComponent<cro::Model>().setMaterial(0, material);

            entity.addComponent<TrophyDisplay>().delay = static_cast<float>(i) / 2.f;
            entity.addComponent<cro::ParticleEmitter>().settings = emitterSettings;

            entity.addComponent<cro::AudioEmitter>() = as.getEmitter("firework");
            entity.getComponent<cro::AudioEmitter>().setPitch(static_cast<float>(cro::Util::Random::value(8, 11)) / 10.f);
            entity.getComponent<cro::AudioEmitter>().setLooped(false);

            m_trophies[i] = entity;
            auto trophyEnt = entity;

            //name label
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>(m_sharedData.nameTextures[0].getTexture());
            auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
            bounds.height /= 4.f;
            bounds.bottom = bounds.height * i;
            entity.getComponent<cro::Sprite>().setTextureRect(bounds);
            entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -1.2f });

            entity.addComponent<cro::Callback>().function =
                [&,trophyEnt](cro::Entity e, float)
            {
                auto trophyPos = trophyEnt.getComponent<cro::Transform>().getPosition();
                trophyPos.y -= 0.06f;

                auto pos = m_trophyScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(trophyPos, m_trophySceneTexture.getSize());

                float scale = m_sharedData.pixelScale ? m_viewScale.x : 1.f;
                e.getComponent<cro::Transform>().setPosition(pos * scale);
                e.getComponent<cro::Transform>().setScale(trophyEnt.getComponent<cro::Transform>().getScale() * m_viewScale.x);
            };

            m_trophyLabels[i] = entity;
        }
        ++i;
    }
}

void GolfState::updateMiniMap()
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::MiniMap;
    cmd.action = [&](cro::Entity en, float)
    {
        //trigger animation
        en.getComponent<cro::Callback>().active = true;
        m_mapCam.getComponent<cro::Camera>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}