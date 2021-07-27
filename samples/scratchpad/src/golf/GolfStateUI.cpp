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

#include "GolfState.hpp"
#include "GameConsts.hpp"
#include "CommandIDs.hpp"
#include "SharedStateData.hpp"
#include "Clubs.hpp"
#include "MenuConsts.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/graphics/SpriteSheet.hpp>

namespace
{
    glm::vec2 viewScale(1.f); //tracks the current scale of the output
    std::size_t scoreColumnCount = 2;

    constexpr float ColumnWidth = 20.f;
    constexpr float ColumnHeight = 276.f;
    constexpr std::array ColumnPositions =
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
        glm::vec2(ColumnWidth * 17.f, ColumnHeight),
    };
}

void GolfState::buildUI()
{
    if (m_holeData.empty())
    {
        return;
    }

    //draws the background using the render texture
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_renderTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    auto courseEnt = entity;


    auto& camera = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    camera.updateMatrices(m_gameScene.getActiveCamera().getComponent<cro::Transform>());
    auto pos = camera.coordsToPixel(m_holeData[0].tee, m_renderTexture.getSize());

    //player sprite - TODO apply avatar customisation
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerSprite;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Player01];
    entity.addComponent<cro::SpriteAnimation>();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width * 0.78f, 0.f));
    courseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto playerEnt = entity;
    m_currentPlayer.position = m_holeData[0].tee;


    //info panel background
    auto windowSize = glm::vec2(cro::App::getWindow().getSize());
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(HoleInfoPosition * windowSize);
    entity.addComponent<UIElement>().position = HoleInfoPosition;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    auto colour = cro::Colour(0.f, 0.f, 0.f, 0.5f);
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 160.f), colour), //TODO remove all these magic numbers
        cro::Vertex2D(glm::vec2(0.f), colour),
        cro::Vertex2D(glm::vec2(134.f, 160.f), colour),
        cro::Vertex2D(glm::vec2(134.f, 0.f), colour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    auto infoEnt = entity;



    auto& font = m_resources.fonts.get(FontID::UI);

    //player's name
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(10.f, 150.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerName;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(8);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //hole number
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(10.f, 132.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::HoleNumber;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(8);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //hole distance
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(10.f, 120.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PinDistance;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(8);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //club info
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(10.f, 108.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(8);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(Clubs[getClub()].name);
    };
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //current stroke
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(10.f, 96.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(8);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        auto stroke = std::to_string(m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole]);
        e.getComponent<cro::Text>().setString("Stroke: " + stroke);
    };
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //current terrain
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(10.f, 84.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(8);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(TerrainStrings[m_currentPlayer.terrain]);
    };
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //wind indicator
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(67.f, 82.f)); //TODO de-magicfy this and make it half the size of the background
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::WindIndicator];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::WindSock;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.getComponent<cro::Transform>().move(glm::vec2(0.f, -bounds.height));
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto camDir = m_holeData[0].pin - m_currentPlayer.position;
    m_camRotation = std::atan2(-camDir.z, camDir.y);

    //wind strength
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(67.f, 30.f)); //TODO same as indicator
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::WindString;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(8);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());







    //root used to show/hide input UI
    auto rootNode = m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>().setPosition(UIHiddenPosition);
    rootNode.addComponent<cro::CommandTarget>().ID = CommandID::UI::Root;
    infoEnt.getComponent<cro::Transform>().addChild(rootNode.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(67.f, 10.f, 0.02f)); //TODO demagify - this is 50% panel width
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



    createScoreboard();





    //callback for the UI camera when window is resized
    auto updateView = [&, playerEnt, courseEnt, rootNode](cro::Camera& cam) mutable
    {
        auto size = glm::vec2(cro::App::getWindow().getSize());
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.5f, 1.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();

        viewScale = glm::vec2(std::floor(size.y / vpSize.y));
        courseEnt.getComponent<cro::Transform>().setScale(viewScale);
        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -0.1f));
        courseEnt.getComponent<cro::Transform>().setOrigin(vpSize / 2.f);
        courseEnt.getComponent<cro::Sprite>().setTextureRect({ 0.f, 0.f, vpSize.x, vpSize.y });

        //update avatar position
        const auto& camera = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
        auto pos = camera.coordsToPixel(m_currentPlayer.position, m_renderTexture.getSize());
        playerEnt.getComponent<cro::Transform>().setPosition(pos);


        //send command to UIElements and reposition / rescale
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::UIElement;
        cmd.action = [size](cro::Entity e, float)
        {
            auto pos = size * e.getComponent<UIElement>().position;
            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, e.getComponent<UIElement>().depth));
            e.getComponent<cro::Transform>().setScale(viewScale);
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);
}

void GolfState::showCountdown(std::uint8_t seconds)
{
    m_roundEnded = true;

    //hide any input
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setPosition(UIHiddenPosition);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //show the scores
    showScoreboard(true);

    auto& font = m_resources.fonts.get(FontID::UI);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 200.f, 10.f, 0.23f }); //attaches to scoreboard which is fixed size
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(8);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::uint8_t>>(1.f, seconds);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& [current, sec] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::uint8_t>>();
        current -= dt;
        if (current < 0)
        {
            current += 1.f;
            sec--;
        }

        e.getComponent<cro::Text>().setString("Return to lobby in: " + std::to_string(sec));
    };

    //attach to the scoreboard
    cmd.targetFlags = CommandID::UI::Scoreboard;
    cmd.action =
        [entity](cro::Entity e, float) mutable
    {
        e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}

void GolfState::createScoreboard()
{
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/scoreboard.spt", m_resources.textures);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(UIHiddenPosition);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::Scoreboard;
    //TODO some sort of background sprite
    entity.addComponent<cro::Drawable2D>();

    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("border");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    auto bgEnt = entity;
    auto& font = m_resources.fonts.get(FontID::UI);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 200.f, 290.f, 0.02f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("LEADERS");
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setCharacterSize(8);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().setOrigin({ -6.f, 253.f});
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    cro::FloatRect bgCrop({ 0.f, bounds.height - 266.f, 389.f, 266.f });
    entity.addComponent<cro::Drawable2D>().setCroppingArea(bgCrop);

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

    //scoreColumnCount = 11;
    scoreColumnCount = std::min(m_holeData.size() + scoreColumnCount, std::size_t(11));

    auto& ents = bgEnt.getComponent<cro::Callback>().getUserData<std::vector<cro::Entity>>();
    ents.resize(scoreColumnCount); //title and total
    std::int32_t i = 0;
    for (auto& e : ents)
    {
        e = m_uiScene.createEntity();
        e.addComponent<cro::Transform>().setPosition(glm::vec3(ColumnPositions[i], 0.02f));
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Text>(font).setCharacterSize(8);
        e.getComponent<cro::Text>().setVerticalSpacing(6.f);
        e.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);

        scrollEnt.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
        i++;
    }
    ents.back().getComponent<cro::Transform>().setPosition(glm::vec3(ColumnPositions.back(), 0.02f));

    //use the callback to keep the board centred/scaled
    bgEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
    {
        //always centre when visible
        auto pos = glm::vec2(cro::App::getWindow().getSize()) / 2.f;
        e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.22f));
        e.getComponent<cro::Transform>().setScale(viewScale);
    };

    updateScoreboard();
    showScoreboard(true);

    //when starting the game some players might not realise
    //they can just close the scores, so create a timeout ent here
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(5.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
        ct -= dt;

        if (ct < 0)
        {
            showScoreboard(false);
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }
    };
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
            std::uint32_t total = 0;
        };

        std::vector<ScoreEntry> scores;

        std::uint32_t playerCount = 0;
        auto holeCount = m_holeData.size();
        for (const auto& client : m_sharedData.connectionData)
        {
            playerCount += client.playerCount;

            for (auto i = 0u; i < client.playerCount; ++i)
            {
                auto& entry = scores.emplace_back();
                entry.name = client.playerData[i].name;
                for (auto s : client.playerData[i].holeScores)
                {
                    entry.holes.push_back(s);
                    entry.total += s;
                }
            }
        }



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
        if (holeCount > scoreColumnCount)
        {
            page2 = std::min(MaxCols, holeCount - scoreColumnCount);
        }

        //name column
        cro::String nameString = "HOLE\nPAR";
        for (auto i = 0u; i < playerCount; ++i)
        {
            nameString += "\n" + scores[i].name;
        }
        if (page2)
        {
            nameString += "\n\nHOLE\nPAR";
            for (auto i = 0u; i < playerCount; ++i)
            {
                nameString += "\n" + scores[i].name;
            }
        }
        ents[0].getComponent<cro::Text>().setString(nameString);

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
                scoreString += "\n\n" + std::to_string(i + MaxCols) + "\n" + std::to_string(m_holeData[i + MaxCols - 1].par);
                for (auto j = 0u; j < playerCount; ++j)
                {
                    scoreString += "\n" + std::to_string(scores[j].holes[i + MaxCols - 1]);
                }
            }

            ents[i].getComponent<cro::Text>().setString(scoreString);
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
            //TODO this should only total the front 9
            //if page2 > 0
            totalString += "\n" + std::to_string(scores[i].total);
        }

        if (page2)
        {
            par = 0;
            for (auto i = MaxCols; i < m_holeData.size(); ++i)
            {
                par += m_holeData[i].par;
            }

            totalString += "\n\nTOTAL\n" + std::to_string(par);
            for (auto i = 0u; i < playerCount; ++i)
            {
                totalString += "\n" + std::to_string(scores[i + MaxCols].total);;
            }
        }

        ents.back().getComponent<cro::Text>().setString(totalString);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}

void GolfState::showScoreboard(bool visible)
{
    //don't hide if the round finished
    if (m_roundEnded)
    {
        visible = true;
    }

    auto pos = visible ? glm::vec2(cro::App::getWindow().getSize()) / 2.f : UIHiddenPosition;
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::Scoreboard;
    cmd.action = [visible, pos](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.22f));
        e.getComponent<cro::Callback>().active = visible;
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);


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
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}