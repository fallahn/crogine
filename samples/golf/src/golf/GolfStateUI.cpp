/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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
#include "ClientCollisionSystem.hpp"
#include "Clubs.hpp"
#include "Career.hpp"
#include "MenuConsts.hpp"
#include "CommonConsts.hpp"
#include "TextAnimCallback.hpp"
#include "ScoreStrings.hpp"
#include "MessageIDs.hpp"
#include "NotificationSystem.hpp"
#include "TrophyDisplaySystem.hpp"
#include "FloatingTextSystem.hpp"
#include "FpsCameraSystem.hpp"
#include "PacketIDs.hpp"
#include "MiniBallSystem.hpp"
#include "BallSystem.hpp"
#include "CallbackData.hpp"
#include "InterpolationSystem.hpp"
#include "WeatherAnimationSystem.hpp"
#include "XPAwardStrings.hpp"
#include "XPValues.hpp"
#include "../ErrorCheck.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Social.hpp>
#include <Content.hpp>
#include <Timeline.hpp>

#include <crogine/audio/AudioScape.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/detail/Types.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem3D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/LightVolumeSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>

#include <crogine/detail/glm/gtx/rotate_vector.hpp>

#ifdef USE_GNS
#include "DebugUtil.hpp"
#endif

using namespace cl;

namespace
{
#include "shaders/PostProcess.inl"
    //cro::FloatRect cropRect;
    //hack to map course names to achievement IDs
    const std::unordered_map<std::string, std::int32_t> ParAch =
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
        std::make_pair("course_11", AchievementID::Master11),
        std::make_pair("course_12", AchievementID::Master12),
    };

    const std::unordered_map<std::string, std::int32_t> CourseIDs =
    {
        std::make_pair("course_01", AchievementID::Complete01),
        std::make_pair("course_02", AchievementID::Complete02),
        std::make_pair("course_03", AchievementID::Complete03),
        std::make_pair("course_04", AchievementID::Complete04),
        std::make_pair("course_05", AchievementID::Complete05),
        std::make_pair("course_06", AchievementID::Complete06),
        std::make_pair("course_07", AchievementID::Complete07),
        std::make_pair("course_08", AchievementID::Complete08),
        std::make_pair("course_09", AchievementID::Complete09),
        std::make_pair("course_10", AchievementID::Complete10),
        std::make_pair("course_11", AchievementID::Complete11),
        std::make_pair("course_12", AchievementID::Complete12),
    };

    //used in netstat icon callback
    struct NetStatData final
    {
        std::uint8_t client = 0;
        std::uint8_t player = 0;
        bool leaguePlayer = false;
        NetStatData(std::uint8_t cl, std::uint8_t pl, bool lp)
            : client(cl), player(pl), leaguePlayer(lp) {}
    };

    static constexpr float ColumnWidth = 20.f;
    static constexpr float ColumnHeight = 276.f;
    static constexpr float ColumnMargin = 6.f;
    static constexpr float RightAdj = 14.f;
    static constexpr std::array ColumnPositions =
    {
        glm::vec2(8.f, ColumnHeight),
        glm::vec2((ColumnWidth * 6.f) + ColumnMargin + RightAdj, ColumnHeight),
        glm::vec2((ColumnWidth * 7.f) + ColumnMargin + RightAdj, ColumnHeight),
        glm::vec2((ColumnWidth * 8.f) + ColumnMargin + RightAdj, ColumnHeight),
        glm::vec2((ColumnWidth * 9.f) + ColumnMargin + RightAdj, ColumnHeight),
        glm::vec2((ColumnWidth * 10.f) + ColumnMargin + RightAdj, ColumnHeight),
        glm::vec2((ColumnWidth * 11.f) + ColumnMargin + RightAdj, ColumnHeight),
        glm::vec2((ColumnWidth * 12.f) + ColumnMargin + RightAdj, ColumnHeight),
        glm::vec2((ColumnWidth * 13.f) + ColumnMargin + RightAdj, ColumnHeight),
        glm::vec2((ColumnWidth * 14.f) + ColumnMargin + RightAdj, ColumnHeight),
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
    entity.addComponent<cro::Drawable2D>().setBlendMode(cro::Material::BlendMode::None);

    auto* shader = &m_resources.shaders.get(ShaderID::Composite);
    entity.getComponent<cro::Drawable2D>().setShader(shader);

    if (m_sharedData.nightTime)
    {
        entity.addComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            //update texture rect and recentre the origin
            e.getComponent<cro::Drawable2D>().setTexture(m_gameSceneMRTexture.getTexture(MRTIndex::Colour), m_gameSceneMRTexture.getSize());

            auto size = glm::vec2(m_gameSceneMRTexture.getSize());
            e.getComponent<cro::Drawable2D>().setVertexData(
                {
                    cro::Vertex2D(glm::vec2(0.f, size.y), glm::vec2(0.f, 1.f)),
                    cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f)),
                    cro::Vertex2D(size, glm::vec2(1.f)),
                    cro::Vertex2D(glm::vec2(size.x, 0.f), glm::vec2(1.f, 0.f))
                });
            e.getComponent<cro::Transform>().setOrigin(size / 2.f);
            e.getComponent<cro::Callback>().active = false;
        };

        cro::TextureID lightID(m_lightMaps[LightMapID::Scene].getTexture());
        cro::TextureID blurID(m_lightBlurTextures[LightMapID::Scene].getTexture());
        entity.getComponent<cro::Drawable2D>().bindUniform("u_depthTexture", m_gameSceneMRTexture.getDepthTexture());
        entity.getComponent<cro::Drawable2D>().bindUniform("u_lightTexture", lightID);
        entity.getComponent<cro::Drawable2D>().bindUniform("u_blurTexture", blurID);
        entity.getComponent<cro::Drawable2D>().bindUniform("u_maskTexture", m_gameSceneMRTexture.getTexture(MRTIndex::Normal));
        //entity.getComponent<cro::Drawable2D>().bindUniform("u_density", 0.6f);
        m_postProcesses[PostID::Composite].uniforms.emplace_back(std::make_pair("u_depthTexture", m_gameSceneMRTexture.getDepthTexture()));
        m_postProcesses[PostID::Composite].uniforms.emplace_back(std::make_pair("u_lightTexture", lightID));
        m_postProcesses[PostID::Composite].uniforms.emplace_back(std::make_pair("u_blurTexture", blurID));
        m_postProcesses[PostID::Composite].uniforms.emplace_back(std::make_pair("u_maskTexture", m_gameSceneMRTexture.getTexture(MRTIndex::Normal)));

        
        m_postProcesses[PostID::CompositeDOF].uniforms.emplace_back(std::make_pair("u_depthTexture", m_gameSceneMRTexture.getDepthTexture()));
        m_postProcesses[PostID::CompositeDOF].uniforms.emplace_back(std::make_pair("u_lightTexture", lightID));
        m_postProcesses[PostID::CompositeDOF].uniforms.emplace_back(std::make_pair("u_blurTexture", blurID));
        m_postProcesses[PostID::CompositeDOF].uniforms.emplace_back(std::make_pair("u_maskTexture", m_gameSceneMRTexture.getTexture(MRTIndex::Normal)));
        m_postProcesses[PostID::CompositeDOF].uniforms.emplace_back(std::make_pair("u_dofTexture", cro::TextureID(m_focusTexture.getTexture())));
    }
    else
    {
        entity.addComponent<cro::Sprite>(m_gameSceneTexture.getTexture());

        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin(glm::vec3(bounds.width / 2.f, bounds.height / 2.f, 0.5f));
        entity.addComponent<cro::Callback>().function = resizeCallback;

        entity.getComponent<cro::Drawable2D>().bindUniform("u_depthTexture", m_gameSceneTexture.getDepthTexture());
        m_postProcesses[PostID::Composite].uniforms.emplace_back(std::make_pair("u_depthTexture", m_gameSceneTexture.getDepthTexture()));

        m_postProcesses[PostID::CompositeDOF].uniforms.emplace_back(std::make_pair("u_depthTexture", m_gameSceneTexture.getDepthTexture()));
        m_postProcesses[PostID::CompositeDOF].uniforms.emplace_back(std::make_pair("u_dofTexture", cro::TextureID(m_focusTexture.getTexture())));
    }    


    for (auto cam : m_cameras)
    {
        cam.getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Composite];
    }

    auto courseEnt = entity;
    m_courseEnt = courseEnt;

    //if we have a lens flare material then create the lensflare entity
    if (m_materialIDs[MaterialID::LensFlare] != -1)
    {
        auto& t = m_resources.textures.get("assets/golf/images/lens_flare.png");
        t.setRepeated(false);

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.01f });
        entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
        entity.getComponent<cro::Drawable2D>().setBlendMode(cro::Material::BlendMode::Additive);
        entity.getComponent<cro::Drawable2D>().setTexture(&t);
        entity.getComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(ShaderID::LensFlare));
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            std::bind(&GolfState::updateLensFlare, this, std::placeholders::_1, std::placeholders::_2);

        if (!m_sharedData.nightTime)
        {
            entity.getComponent<cro::Drawable2D>().bindUniform("u_depthTexture", m_gameSceneTexture.getDepthTexture());

        }
        else
        {
            entity.getComponent<cro::Drawable2D>().bindUniform("u_depthTexture", m_gameSceneMRTexture.getDepthTexture());
        }
    }

    //lens flares for point lights
    if (m_sharedData.nightTime
        && m_materialIDs[MaterialID::PointFlare] != -1)
    {
        auto& t = m_resources.textures.get("assets/golf/images/flare02.png");
        t.setRepeated(false);

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.02f });
        entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
        entity.getComponent<cro::Drawable2D>().setBlendMode(cro::Material::BlendMode::Additive);
        entity.getComponent<cro::Drawable2D>().setTexture(&t);
        entity.getComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(ShaderID::PointFlare));
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            std::bind(&GolfState::updatePointFlares, this, std::placeholders::_1, std::placeholders::_2);

        entity.getComponent<cro::Drawable2D>().bindUniform("u_depthTexture", m_gameSceneMRTexture.getDepthTexture());
    }


    //displays the trophies on round end - has to be displayed over top of scoreboard
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_trophySceneTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec3(bounds.width / 2.f, (bounds.height / 2.f), 0.f));
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
    
    entity = m_uiScene.createEntity();
    createSwingputMeter(entity, m_inputParser);
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_textChat.setRootNode(infoEnt);

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //shown when drone cam is active
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().absolutePosition = { 10.f, 32.f };
    entity.getComponent<UIElement>().depth = 8.1f;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setString("DRONE CAM");
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            static constexpr float FlashTime = 0.5f;
            static float currTime = 0.f;

            currTime += dt;
            if (currTime > FlashTime)
            {
                currTime -= FlashTime;
                auto facing = e.getComponent<cro::Drawable2D>().getFacing();
                facing = facing == cro::Drawable2D::Facing::Front ? cro::Drawable2D::Facing::Back : cro::Drawable2D::Facing::Front;
                e.getComponent<cro::Drawable2D>().setFacing(facing);
            }
        };
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_droneTextEnt = entity;

    //info menu shown when in freecam mode
    cro::SpriteSheet freecamSheet;
    freecamSheet.loadFromFile("assets/golf/sprites/freecam.spt", m_resources.textures);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin(glm::vec2(0.f, 150.f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().absolutePosition = { 10.f, 10.f };
    entity.getComponent<UIElement>().depth = 8.1f;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::Sprite>() = freecamSheet.getSprite("freecam");
    const float offset = (entity.getComponent<UIElement>().absolutePosition.y * 2.f) +
        entity.getComponent<cro::Sprite>().getTextureBounds().height + 16.f;
    entity.addComponent<cro::Callback>().function = 
        [&, offset](cro::Entity e, float dt)
        {
            //this is the slide in/out anim
            const auto amount = 1.f - m_freeCam.getComponent<FpsCamera>().transition.progress;
            const glm::vec2 origin(0.f, amount * offset);
            e.getComponent<cro::Transform>().setOrigin(origin);

            const float alpha = smoothstep(0.5f, 0.9f, m_freeCam.getComponent<FpsCamera>().transition.progress);
            const cro::Colour c(1.f, 1.f, 1.f, alpha);
            e.getComponent<cro::Sprite>().setColour(c);

            //and then scale based on if we want to show the menu
            auto& [direction, progress] = e.getComponent<cro::Callback>().getUserData<FreecamHideData>();
            const float Speed = dt * 5.f;
            if (direction == 0) //in
            {
                progress = std::min(1.f, progress + Speed);
            }
            else
            {
                progress = std::max(0.f, progress - Speed);
            }
            const float scale = cro::Util::Easing::easeOutBack(progress);
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale, 1.f));
        };
    entity.getComponent<cro::Callback>().setUserData<FreecamHideData>();

    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_freecamMenuEnt = entity;

    //zoom amount text
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(m_freecamMenuEnt.getComponent<cro::Sprite>().getTextureBounds().width / 2.f, 132.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Zoom: 100%");
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(glm::vec2(1.f, -1.f));
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (m_freecamMenuEnt.getComponent<cro::Transform>().getScale().x != 0)
            {
                const auto Zoom = (FpsCamera::MinZoom - m_freeCam.getComponent<FpsCamera>().fov) / (FpsCamera::MinZoom - FpsCamera::MaxZoom);
                const std::int32_t Z = std::round(Zoom * 100.f);
                e.getComponent<cro::Text>().setString("Zoom: " + std::to_string(Z) + "%");
            }
        };
    m_freecamMenuEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //up/down button text
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(m_freecamMenuEnt.getComponent<cro::Sprite>().getTextureBounds().width / 2.f, 132.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    //entity.getComponent<cro::Text>().setShadowOffset(glm::vec2(1.f, -1.f));
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(5.f);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (m_freecamMenuEnt.getComponent<cro::Transform>().getScale().x != 0)
            {
                if (m_freecamMenuEnt.getComponent<cro::SpriteAnimation>().id == 0)
                {
                    const auto Out = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::PrevClub]);
                    const auto In = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::NextClub]);
                    const auto Down = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::EmoteMenu]);
                    const auto Up = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]);
                    e.getComponent<cro::Text>().setString(Out + "                 " + In + "\n" + Down + "                 " + Up);
                    e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                }
                else
                {
                    e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }
            }
        };
    m_freecamMenuEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //movement button text
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(49.f, 97.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setAlignment(cro::Text::Alignment::Right);
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    //entity.getComponent<cro::Text>().setShadowOffset(glm::vec2(1.f, -1.f));
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(1.f);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (m_freecamMenuEnt.getComponent<cro::Transform>().getScale().x != 0)
            {
                if (m_freecamMenuEnt.getComponent<cro::SpriteAnimation>().id == 0)
                {
                    const auto Left = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Left]);
                    const auto Right = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Right]);
                    const auto Up = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Up]);
                    const auto Down = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Down]);
                    auto str = Left + "," + Right + "," + Up + "," + Down;
                    if (str.size() > 10) //a fudge because the string might be too long to fit
                    {
                        str = "Aim Keys";
                    }
                    str += "\n\n" + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::SpinMenu]);
                    e.getComponent<cro::Text>().setString(str);
                    e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                }
                else
                {
                    e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }
            }
        };
    m_freecamMenuEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

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

    //player avatar
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -21.f, -11.f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_sharedData.nameTextures[0].getTexture());
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::uint32_t>(1);
    entity.getComponent<cro::Callback>().function =
        [&, nameEnt](cro::Entity e, float)
    {
        static constexpr auto BaseScale = glm::vec2(14.f) / LabelIconSize;
        if (nameEnt.getComponent<cro::Callback>().active)
        {
            //set scale based on progress
            const auto& data = nameEnt.getComponent<cro::Callback>().getUserData<TextCallbackData>();
            float scale = static_cast<float>(data.currentChar) / data.string.size();

            //TODO could probably use the timer to interpolate scale
            e.getComponent<cro::Transform>().setScale(BaseScale * glm::vec2(scale, 1.f));

            //if the scale is zero set the new texture properties
            auto& prevChar = e.getComponent<cro::Callback>().getUserData<std::uint32_t>();
            if (data.currentChar == 0
                && prevChar != 0)
            {
                e.getComponent<cro::Sprite>().setTexture(m_sharedData.nameTextures[data.client].getTexture());

                cro::FloatRect bounds = getAvatarBounds(data.player);
                e.getComponent<cro::Sprite>().setTextureRect(bounds);
            }
            prevChar = data.currentChar;
        }
        else
        {

        }
    };
    nameEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //light bulb displayed when players are thinking
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Thinking];
    bounds = m_sprites[SpriteID::Thinking].getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::ThinkBubble;
    entity.addComponent<cro::Callback>().setUserData<CogitationData>();
    entity.getComponent<cro::Callback>().function = CogitationCallback(nameEnt);
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

    //pin elevation
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -12.f, 0.f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PinHeight | CommandID::UI::UIElement;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -10.f };
    entity.getComponent<UIElement>().depth = 0.05f;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::min(1.f, currTime + (dt * 3.f));
            
            e.getComponent<cro::Transform>().setScale({ cro::Util::Easing::easeInOutSine(currTime), 1.f });

            if(currTime == 1)
            {
                currTime = 0.f;
                e.getComponent<cro::Callback>().active = false;
            }
        };
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    static constexpr float tWidth = 52.f;
    static constexpr float tHeight = 4.f;
    static constexpr float tSlope = tWidth - 3.f;

    auto tEnt = entity;
    const auto c = cro::Colour(0.f, 0.f, 0.f, OverlayAlpha);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -6.f, -0.05f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-tWidth, tHeight), c),
            cro::Vertex2D(glm::vec2(-tSlope, -tHeight), c),
            cro::Vertex2D(glm::vec2(tWidth, tHeight), c),
            cro::Vertex2D(glm::vec2(tSlope, -tHeight), c)
        });
    tEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    cro::AudioScape as;
    as.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);
    entity = m_gameScene.createEntity(); //needs to be in game scene to play audio
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = as.getEmitter("switch");
    auto audioEnt = entity;

    //afk warning
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::AFKWarn | CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -UIBarHeight };
    entity.getComponent<UIElement>().depth = 0.05f;
    entity.addComponent<cro::Callback>().setUserData<float>(10.f);
    entity.getComponent<cro::Callback>().function =
        [audioEnt](cro::Entity e, float dt) mutable
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            auto oldTime = currTime;

            currTime = std::max(0.f, currTime - dt);

            if (std::floor(currTime) < std::floor(oldTime))
            {
                auto t = static_cast<std::int32_t>(currTime);
                std::string str = "Skipping in " + std::to_string(t);
                e.getComponent<cro::Text>().setString(str);
                centreText(e);

                audioEnt.getComponent<cro::AudioEmitter>().play();

                if (t == 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }
            }
        };
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
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
    entity.addComponent<UIElement>().relativePosition = { 0.64f, 0.f };
    entity.getComponent<UIElement>().absolutePosition = { 32.f, 0.f };
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
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::TerrainType;
    entity.addComponent<UIElement>().relativePosition = { 0.76f, 1.f };
    entity.getComponent<UIElement>().depth = 0.05f;
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
   /* entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (m_currentPlayer.terrain == TerrainID::Bunker)
        {
            auto lie = m_avatars[m_currentPlayer.client][m_currentPlayer.player].ballModel.getComponent<ClientCollider>().lie;
            static const std::array<std::string, 2u> str = { "Bunker (B)", "Bunker (SU)" };
            e.getComponent<cro::Text>().setString(str[lie]);
        }
        else
        {
            e.getComponent<cro::Text>().setString(TerrainStrings[m_currentPlayer.terrain]);
        }
    };*/
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


    //punch indicator
    const auto originOffset = spinEnt.getComponent<cro::Transform>().getOrigin();
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -36.f, 0.f, 0.f });
    entity.getComponent<cro::Transform>().move(originOffset);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("PUNCH");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const auto allowPunch = glm::length2(m_currentPlayer.position - m_holeData[m_currentHole].tee) > 1.f;
            m_inputParser.setAllowPunch(allowPunch);

            const float scale =
                ((ClubShot[getClub()] & ShotType::Punch) 
                    && allowPunch) ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    spinEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto textEnt = entity;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 8.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = buttonSprites.getSprite("button_lb");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float) 
        {
            const cro::Colour c = Club::getModifierIndex() == 1 ? TextHighlightColour : cro::Colour::White;
            switch (m_sharedData.activeInput)
            {
            default:
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                break;
            case SharedStateData::ActiveInput::XBox:
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::SpriteAnimation>().play(0);
                e.getComponent<cro::Sprite>().setColour(c);
                break;
            case SharedStateData::ActiveInput::PS:
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::SpriteAnimation>().play(1);
                e.getComponent<cro::Sprite>().setColour(c);
                break;
            }
        };
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    textEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 10.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(glm::vec2(1.f, -1.f));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
            {
                const cro::Colour c = Club::getModifierIndex() == 1 ? TextHighlightColour : TextNormalColour;
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::Text>().setString(cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::PrevClub]));
                e.getComponent<cro::Text>().setFillColour(c);
            }
            else
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
        };
    textEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //flop indicator
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 36.f, 0.f, 0.f });
    entity.getComponent<cro::Transform>().move(originOffset);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("FLOP");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const float scale =
                (ClubShot[getClub()] & ShotType::Flop) ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    spinEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    textEnt = entity;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 8.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = buttonSprites.getSprite("button_rb");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const cro::Colour c = Club::getModifierIndex() == 2 ? TextHighlightColour : cro::Colour::White;
            switch (m_sharedData.activeInput)
            {
            default:
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                break;
            case SharedStateData::ActiveInput::XBox:
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::SpriteAnimation>().play(0);
                e.getComponent<cro::Sprite>().setColour(c);
                break;
            case SharedStateData::ActiveInput::PS:
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::SpriteAnimation>().play(1);
                e.getComponent<cro::Sprite>().setColour(c);
                break;
            }
        };
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    textEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 10.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(glm::vec2(1.f, -1.f));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
            {
                const cro::Colour c = Club::getModifierIndex() == 2 ? TextHighlightColour : TextNormalColour;
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::Text>().setString(cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::NextClub]));
                e.getComponent<cro::Text>().setFillColour(c);
            }
            else
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
        };
    textEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //wind strength - this is positioned by the camera's resize callback, below
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::WindString | CommandID::UI::WindHidden;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    //entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::Callback>().setUserData<WindHideData>();
    entity.getComponent<cro::Callback>().getUserData<WindHideData>().progress = 1.f;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float  dt)
    {
        const float Speed = dt * 3.f;
        auto& [direction, progress] = e.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>();
        if (direction == 0)
        {
            //shrink
            progress = std::max(0.f, progress - Speed);
            if (progress == 0)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        }
        else
        {
            //grow
            progress = std::min(1.f, progress + Speed);
            if (progress == 1)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        }

        float scale = cro::Util::Easing::easeOutQuint(progress);
        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f, scale));
    };
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
    
    //wind effect strength
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 64.f, 8.f, 0.1f });
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
    entity.getComponent<cro::Callback>().setUserData<WindEffectData>(0.f, 0.f);
    entity.getComponent<cro::Callback>().function = WindEffectCallback();
    windEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //sets the initial cam rotation for the wind indicator compensation
    auto camDir = m_holeData[0].target - m_currentPlayer.position;
    m_camRotation = std::atan2(-camDir.z, camDir.y);

    //root used to show/hide power bar
    auto rootNode = m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    rootNode.addComponent<cro::CommandTarget>().ID = CommandID::UI::Root;
    rootNode.addComponent<cro::Callback>().setUserData<std::pair<std::int32_t, float>>(0, 0.f);
    rootNode.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
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

    createPowerBars(rootNode);

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



    //shows hole number when wind indicator is hidden
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::WindHidden;
    entity.addComponent<cro::Callback>().setUserData<WindHideData>();
    entity.getComponent<cro::Callback>().function =
        [windEnt](cro::Entity e, float  dt)
    {
        auto origin = windEnt.getComponent<cro::Transform>().getOrigin();

        const float Speed = dt * 3.f;
        auto& [direction, progress] = e.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>();
        if (direction == 1)
        {
            //shrink
            progress = std::max(0.f, progress - Speed);
            if (progress == 0)
            {
                e.getComponent<cro::Callback>().active = false;
            }
            origin.y = (1.f - progress) * 120.f;
        }
        else
        {
            //wait until shrunk
            if (windEnt.getComponent<cro::Transform>().getScale().y > 0)
            {
                return;
            }

            //grow
            progress = std::min(1.f, progress + Speed);
            if (progress == 1)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        }

        float scale = cro::Util::Easing::easeOutQuint(progress);
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale, 1.f));
        e.getComponent<cro::Transform>().setOrigin(origin);
    };
    infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto windEnt2 = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(38.f, -5.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::HoleNumber;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<cro::Callback>().setUserData<TextCallbackData>();
    entity.getComponent<cro::Callback>().function = TextAnimCallback();
    windEnt2.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -4.f, -14.f, -0.01f });
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, 6.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::WindSpeedBg];
    windEnt2.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //displays if a mulligan is available in Career
    if (m_sharedData.leagueRoundID != LeagueRoundID::Club
        || m_sharedData.gameMode == GameMode::Tournament)
    {
        auto mRoot = m_uiScene.createEntity();
        mRoot.addComponent<cro::Transform>();
        mRoot.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
        mRoot.addComponent<UIElement>();// .relativePosition = { 0.01f, 0.01f };
        mRoot.getComponent<UIElement>().absolutePosition = { WindIndicatorPosition.x + 3.f, (WindIndicatorPosition.y / 2.f) - 6.f };
        mRoot.getComponent<UIElement>().depth = 1.f;
        infoEnt.getComponent<cro::Transform>().addChild(mRoot.getComponent<cro::Transform>());

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize * 2);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
        entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        entity.getComponent<cro::Text>().setString("M");
        entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        mRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        auto mEnt = entity;

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, -3.f });
        entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
        entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
        entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        entity.getComponent<cro::Text>().setString("Mulligan Available");
        entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        mRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        auto tEnt = entity;

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, -3.f });
        entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
        entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
        entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        entity.getComponent<cro::Text>().setString("Options Menu");
        entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        mRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        auto oEnt = entity;

        struct MulliganData final
        {
            enum
            {
                Hide, Shrink, Grow,
                Hold
            };
            std::int32_t state = 0;
            std::int32_t idx = 0;
            float progress = 0.f;
            float currTime = 5.f;
        };
        static constexpr float SpinTime = 6.f; //Time between spins

        mRoot.addComponent<cro::Callback>().active = true;
        mRoot.getComponent<cro::Callback>().setUserData<MulliganData>();
        mRoot.getComponent<cro::Callback>().function =
            [&, mEnt, tEnt, oEnt](cro::Entity e, float dt) mutable
            {
                auto& [state, idx, progress, currTime] = e.getComponent<cro::Callback>().getUserData<MulliganData>();
                const float Speed = dt * 4.f;

                switch (state)
                {
                default: break;
                case MulliganData::Hide:
                    if (m_sharedData.hasMulligan)
                    {
                        state = MulliganData::Grow;
                        progress = 0.f;
                    }
                    else
                    {
                        progress = std::max(0.f, progress - Speed);
                        const float scale = cro::Util::Easing::easeOutSine(progress);
                        e.getComponent<cro::Transform>().setScale({ scale, 1.f });
                    }
                    break;
                case MulliganData::Grow:
                {
                    progress = std::min(1.f, progress + Speed);
                    const float scale = cro::Util::Easing::easeOutSine(progress);
                    e.getComponent<cro::Transform>().setScale({ scale, 1.f });

                    if (progress == 1)
                    {
                        state = MulliganData::Hold;
                    }
                }
                    break;
                case MulliganData::Hold:
                    if (m_sharedData.hasMulligan)
                    {
                        //count time to spin
                        currTime -= dt;
                        if (currTime < 0.f)
                        {
                            currTime = (mEnt.getComponent<cro::Drawable2D>().getFacing() == cro::Drawable2D::Facing::Back) ? SpinTime : SpinTime / 3.f;
                            state = MulliganData::Shrink;
                        }
                    }
                    else
                    {
                        state = MulliganData::Hide;
                    }
                    break;
                case MulliganData::Shrink:
                {
                    progress = std::max(0.f, progress - Speed);
                    const float scale = cro::Util::Easing::easeOutSine(progress);
                    e.getComponent<cro::Transform>().setScale({ scale, 1.f });

                    if (progress == 0)
                    {
                        state = MulliganData::Grow;
                        if (mEnt.getComponent<cro::Drawable2D>().getFacing() == cro::Drawable2D::Facing::Front)
                        {
                            mEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                            
                            if (idx == 0)
                            {
                                tEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                                idx = 1;
                            }
                            else
                            {
                                oEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                                idx = 0;
                            }
                        }
                        else
                        {
                            mEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                            tEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                            oEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                        }
                    }
                }
                    break;
                }
            };
    }



    //minimap view
    struct MinimapData final
    {
        std::int32_t state = 0;
        float scale = 0.001f;
        float rotation = -1.f;

        float textureRatio = 1.f; //pixels per metre in the minimap texture * 2
    };
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();// .setPosition({ 0.f, 0.f, 0.2f });
    entity.getComponent<cro::Transform>().setRotation(-90.f * cro::Util::Const::degToRad);
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, 0.f, 0.05f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.05f, 0.f));
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
            cmd.action = [&](cro::Entity en, float)
            {
                en.getComponent<cro::Transform>().setScale({ 0.f, 0.f });
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            if (scale == 0)
            {
                //starts the multipass rendering (actually done in Scene::simulate())
                m_minimapTexturePass = 0;

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
        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f / ratio, newScale / ratio) * MapSizeRatio);
    };

    //these are const so calc them once here and capture them
    glm::vec2 mapOffset = glm::vec2(MapSize.y / MapSizeMultiplier, (MapSize.x/* - 32.f*/) / MapSizeMultiplier) * glm::vec2(MapSizeRatio.y, MapSizeRatio.x);
    const float l = glm::length(mapOffset);
    mapOffset = glm::normalize(mapOffset);

    m_mapRoot = m_uiScene.createEntity();
    m_mapRoot.addComponent<cro::Transform>().setPosition({0.f, 0.f, 0.02f});
    m_mapRoot.getComponent<cro::Transform>().setScale(glm::vec2(m_sharedData.showMinimap ? 1.f : 0.f));
    m_mapRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_mapRoot.addComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(0.f, 1);
    m_mapRoot.getComponent<cro::Callback>().function =
        [&, l, mapOffset](cro::Entity e, float dt)
        {
            if (m_sharedData.showMinimap)
            {
                auto& [progress, dir] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
                if (dir == 0)
                {
                    //bigger
                    progress = std::min(1.f, progress + (dt * 8.f));
                    if (progress == 1)
                    {
                        e.getComponent<cro::Callback>().active = false;
                    }
                }
                else
                {
                    progress = std::max(0.f, progress - (dt * 8.f));
                    if (progress == 0)
                    {
                        e.getComponent<cro::Callback>().active = false;
                    }
                }

                const auto p = cro::Util::Easing::easeOutExpo(progress);
                auto o = mapOffset * l * p;
                o.x = std::round(o.x);
                o.y = std::round(o.y);

                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f + p));
                e.getComponent<cro::Transform>().setOrigin(o);
            }
            else
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
        };

    infoEnt.getComponent<cro::Transform>().addChild(m_mapRoot.getComponent<cro::Transform>());
    auto mapEnt = entity;
    m_minimapEnt = entity;


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -35.f, -65.f, 0.2f });
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Text>(smallFont).setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({1.f, -1.f});
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<cro::String>(" ");
    entity.getComponent<cro::Callback>().function =
        [mapEnt](cro::Entity e, float)
        {
            if (mapEnt.getComponent<cro::Callback>().active)
            {
                e.getComponent<cro::Text>().setString(" ");
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            }
            else
            {
                e.getComponent<cro::Text>().setString(e.getComponent<cro::Callback>().getUserData<cro::String>());
            }
        };
    m_mapRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_strokeDistanceEnt = entity;


    //draws a trail on the mini map when the balls are in flight
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.21f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLE_STRIP);
    entity.getComponent<cro::Drawable2D>().setBlendMode(cro::Material::BlendMode::Additive);
    entity.addComponent<cro::Callback>().setUserData<float>(1.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
            ct = std::max(0.f, ct - (dt * 8.f));

            if (ct == 0)
            {
                e.getComponent<cro::Callback>().active = false;
                e.getComponent<cro::Drawable2D>().getVertexData().clear();
                ct = 1.f;
            }
            e.getComponent<cro::Transform>().setScale(glm::vec2(ct, 1.f));
        };
    m_minimapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_minimapTrail = entity;


    //mini flag icon
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setRotation(cro::Util::Const::degToRad * 90.f);
    entity.getComponent<cro::Transform>().setOrigin({ 0.5f, -0.5f });
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MiniFlag;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::MiniMapFlag];
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, mapEnt](cro::Entity e, float dt)
    {
        e.getComponent<cro::Transform>().setPosition(glm::vec3(m_minimapZoom.toMapCoords(m_holeData[m_currentHole].pin), 0.02f));
        e.getComponent<cro::Transform>().setScale(((m_minimapZoom.mapScale * 1.8f * (1.f + ((m_minimapZoom.zoom - 1.f) * 0.125f))) * 0.75f) * (glm::vec2(1.f) / MapSizeRatio));

        auto miniBounds = mapEnt.getComponent<cro::Transform>().getWorldTransform() * mapEnt.getComponent<cro::Drawable2D>().getLocalBounds();
        //auto flagBounds = glm::inverse(e.getComponent<cro::Transform>().getWorldTransform()) * miniBounds;
        e.getComponent<cro::Drawable2D>().setCroppingArea(miniBounds, true);
    };
    mapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    
    /*auto dbEnt = m_uiScene.createEntity();
    dbEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    dbEnt.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);*/

    //stroke indicator
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>().getVertexData() = getStrokeIndicatorVerts(m_sharedData.decimatePowerBar);
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, mapEnt/*, dbEnt*/](cro::Entity e, float dt) mutable
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

        const auto InverseScale = (1.f / mapEnt.getComponent<cro::Transform>().getScale().x);
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale, 1.f) * InverseScale);

        auto miniBounds = mapEnt.getComponent<cro::Transform>().getWorldTransform() * mapEnt.getComponent<cro::Drawable2D>().getLocalBounds();
        e.getComponent<cro::Drawable2D>().setCroppingArea(miniBounds, true);
        m_minimapTrail.getComponent<cro::Drawable2D>().setCroppingArea(miniBounds, true);

        //when the drawable is cropped the area is transformed by the ent's world tx
        //so we're doing this here purely to visualise
        //miniBounds = miniBounds.transform(e.getComponent<cro::Transform>().getWorldTransform());

        /*std::vector<cro::Vertex2D> verts =
        {
            cro::Vertex2D(glm::vec2(miniBounds.left, miniBounds.bottom), cro::Colour::Magenta),
            cro::Vertex2D(glm::vec2(miniBounds.left + miniBounds.width, miniBounds.bottom), cro::Colour::Magenta),
            cro::Vertex2D(glm::vec2(miniBounds.left + miniBounds.width, miniBounds.bottom + miniBounds.height), cro::Colour::Magenta),
            cro::Vertex2D(glm::vec2(miniBounds.left, miniBounds.bottom + miniBounds.height), cro::Colour::Magenta),
            cro::Vertex2D(glm::vec2(miniBounds.left, miniBounds.bottom), cro::Colour::Magenta),
        };
        dbEnt.getComponent<cro::Drawable2D>().setVertexData(verts);*/

    };
    mapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_minimapIndicatorEnt = entity;


    auto greenEntRoot = m_uiScene.createEntity();
    greenEntRoot.addComponent<cro::Transform>();
    greenEntRoot.addComponent<cro::Callback>().active = true;
    greenEntRoot.getComponent<cro::Callback>().setUserData<float>(0.f);
    greenEntRoot.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& progress = e.getComponent<cro::Callback>().getUserData<float>();

            //zooms based on the state of the display
            if (m_flightCam.getComponent<cro::Camera>().active
                && m_sharedData.zoomFollowCam)
            {
                const auto diff = 1.f - progress;
                progress = std::min(1.f, progress + (diff * (dt * 5.f)));

                const float zoom = std::max(1.f, 1.f + progress);

                e.getComponent<cro::Transform>().setScale(glm::vec2(zoom));

                auto offset = glm::vec2(m_overheadBuffer.getSize() / 8u) / m_viewScale;
                offset *= (zoom - 1.f);
                offset.x *= -1.f;
                e.getComponent<cro::Transform>().setOrigin(offset);
            }
            else
            {
                progress = 0.f;
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::Transform>().setOrigin(glm::vec2(0.f));
            }
        };
    infoEnt.getComponent<cro::Transform>().addChild(greenEntRoot.getComponent<cro::Transform>());


    //green close up view
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 0.f, 0.f }); //position is set in UI cam callback, below
    entity.addComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(ShaderID::Minimap));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MiniGreen; //dunno why we need this, we store the ent as a class member...
    entity.addComponent<cro::Sprite>(); //set to m_overheadBuffer by resize callback, below
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
                //m_greenCam.getComponent<cro::Callback>().active = true;
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
                m_flightCam.getComponent<cro::Camera>().active = false;
            }
        }
    };

    //infoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    greenEntRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto greenEnt = entity;
    m_miniGreenEnt = entity;

    createScoreboard();


    //set up the overhead cam for the mini map - this renders the entire hole
    //to a texture - the view of the minimap is controlled in the shader which draws it
    //see minimapZoom
    auto updateMiniView = [&, mapEnt](cro::Camera& miniCam) mutable
    {
        constexpr glm::uvec2 texSize = MapSize * MapSizeMultiplier;
        m_mapTextureMRT.setPrecision(cro::TexturePrecision::Low);
        m_mapTextureMRT.create(texSize.x, texSize.y, MRTIndex::Count + 1); //colour, pos, normal, *unused - sigh*, terrain mask
        m_mapTextureMRT.setBorderColour(cro::Colour::Transparent);
        m_sharedData.minimapData.mrt = &m_mapTextureMRT;

        mapEnt.getComponent<cro::Sprite>().setTexture(m_mapTextureMRT.getTexture());
        mapEnt.getComponent<cro::Transform>().setOrigin({ texSize.x / 2.f, texSize.y / 2.f });
        mapEnt.getComponent<cro::Callback>().getUserData<MinimapData>().textureRatio = static_cast<float>(MapSizeMultiplier * 2);
        m_minimapZoom.mapScale = texSize / MapSize;
        m_minimapZoom.pan = texSize / 2u;
        m_minimapZoom.textureSize = texSize;
        m_minimapZoom.updateShader();

        glm::vec2 viewSize(MapSize);
        miniCam.setOrthographic(-viewSize.x / 2.f, viewSize.x / 2.f, -viewSize.y / 2.f, viewSize.y / 2.f, -0.1f, 60.f);
        miniCam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };


    auto mapCam = m_mapScene.createEntity();
    mapCam.addComponent<cro::Transform>().setPosition({ static_cast<float>(MapSize.x) / 2.f, 36.f, -static_cast<float>(MapSize.y) / 2.f});
    mapCam.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    auto& miniCam = mapCam.addComponent<cro::Camera>();
    updateMiniView(miniCam);
    m_mapScene.setActiveCamera(mapCam);
    //miniCam.resizeCallback = updateMiniView; //don't do this on resize as recreating the buffer clears it..



    //and the mini view of the green
    auto updateGreenView = [&, greenEnt](cro::Camera& greenCam) mutable
    {
        auto texSize = MiniMapSize.y / 2u;

        auto windowScale = getViewScale();
        float scale = m_sharedData.pixelScale ? windowScale : 1.f;
        scale = (windowScale + 1.f) - scale;
        texSize *= static_cast<std::uint32_t>(scale) * 2;

        /*std::uint32_t samples = m_sharedData.pixelScale ? 0 :
            m_sharedData.antialias ? m_sharedData.multisamples : 0;*/

        m_overheadBuffer.setPrecision(m_sharedData.lightmapQuality);
        m_overheadBuffer.create(texSize, texSize, MRTIndex::Count); //yes, it's square

        if (m_sharedData.nightTime)
        {
            m_lightMaps[LightMapID::Overhead].create(texSize, texSize);

            m_lightBlurTextures[LightMapID::Overhead].create(texSize / 4u, texSize / 4u, false);
            m_lightBlurTextures[LightMapID::Overhead].setSmooth(true);
            m_lightBlurQuads[LightMapID::Overhead].setTexture(m_overheadBuffer.getTexture(MRTIndex::Light), glm::uvec2(texSize / 4u));
            m_lightBlurQuads[LightMapID::Overhead].setShader(m_resources.shaders.get(ShaderID::Blur));
        }

        auto targetScale = glm::vec2(1.f / scale) / 2.f;

        greenEnt.getComponent<cro::Sprite>().setTexture(m_overheadBuffer.getTexture());
        greenEnt.getComponent<cro::Transform>().setScale(targetScale);

        greenEnt.getComponent<cro::Transform>().setOrigin({ (texSize / 2), (texSize / 2) }); //must divide to a whole pixel!
        auto& ud = greenEnt.getComponent<cro::Callback>().getUserData<GreenCallbackData>();
        ud.targetScale = targetScale.x;

        if (!greenEnt.getComponent<cro::Callback>().active)
        {
            ud.state = ud.state == 1 ? 0.f : 1.f;
            greenEnt.getComponent<cro::Callback>().active = true;
        }
    };

    m_greenCam = m_gameScene.createEntity();
    m_greenCam.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    auto& greenCam = m_greenCam.addComponent<cro::Camera>();
    greenCam.setRenderFlags(cro::Camera::Pass::Final, RenderFlags::MiniGreen);
    greenCam.resizeCallback = updateGreenView;
    greenCam.active = false;
    greenCam.shadowMapBuffer.create(2, 2);
    //greenCam.shadowMapBuffer.clear();
    //greenCam.shadowMapBuffer.display();
    updateGreenView(greenCam);

    if (m_sharedData.nightTime)
    {
        //can't bind lightmap to shader until it's been created...
        auto lightID = cro::TextureID(m_lightMaps[LightMapID::Overhead].getTexture().getGLHandle());
        greenEnt.getComponent<cro::Drawable2D>().bindUniform("u_lightTexture", lightID);

        auto blurID = cro::TextureID(m_lightBlurTextures[LightMapID::Overhead].getTexture().getGLHandle());
        greenEnt.getComponent<cro::Drawable2D>().bindUniform("u_blurTexture", blurID);
        greenEnt.getComponent<cro::Drawable2D>().bindUniform("u_maskTexture", m_overheadBuffer.getTexture(MRTIndex::Normal));
    }
    greenEnt.getComponent<cro::Drawable2D>().bindUniform("u_depthTexture", m_overheadBuffer.getDepthTexture());
    
    //check the weather first
    if (m_sharedData.weatherType == WeatherType::Rain
        || m_sharedData.weatherType == WeatherType::Showers)
    {
        auto rainShaderID = m_resources.shaders.get(ShaderID::Minimap).getGLHandle();
        auto rainSubrectID = m_resources.shaders.get(ShaderID::Minimap).getUniformID("u_subrect");
        auto rainMixID = m_resources.shaders.get(ShaderID::Minimap).getUniformID("u_distortAmount");

        greenEnt.getComponent<cro::Drawable2D>().bindUniform("u_distortionTexture", cro::TextureID(m_resources.textures.get("assets/golf/images/rain_sheet.png")));
        //create ent to animate texture
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, rainShaderID, rainSubrectID, rainMixID](cro::Entity e, float dt)
            {
                static constexpr float FrameTime = 1.f / 16.f;
                static float animTime = 0.f;
                animTime += dt;

                static constexpr std::int32_t MaxFrames = 24;
                static std::int32_t frameID = 0;

                while (animTime > FrameTime)
                {
                    frameID = (frameID + 1) % MaxFrames;

                    auto row = 5 - (frameID / 4);
                    auto col = frameID % 4;

                    glm::vec4 rect =
                    {
                        (1.f / 4.f) * col,
                        (1.f / 6.f) * row,
                        (1.f / 4.f), (1.f / 6.f)
                    };

                    const float rainAmount = m_gameScene.getSystem<WeatherAnimationSystem>()->getOpacity();

                    glUseProgram(rainShaderID);
                    glUniform4f(rainSubrectID, rect.x, rect.y, rect.z, rect.w);
                    glUniform1f(rainMixID, rainAmount);

                    animTime -= FrameTime;
                }
            };
    }

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
    auto updateView = [&, trophyEnt, courseEnt, infoEnt, spinEnt, windEnt, windEnt2, greenEntRoot, rootNode](cro::Camera& cam) mutable
    {
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -3.5f, 20.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        //m_inputParser.setMouseScale(m_viewScale.x);

        glm::vec2 courseScale(m_sharedData.pixelScale ? m_viewScale.x : 1.f);

        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -1.f));
        courseEnt.getComponent<cro::Transform>().setScale(courseScale);
        courseEnt.getComponent<cro::Callback>().active = true; //makes sure to delay so updating the texture size is complete first

        trophyEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, 3.5f));
        trophyEnt.getComponent<cro::Transform>().setScale(courseScale);
        if (m_sharedData.teamMode)
        {
            //hack to make team mate's name more visible
            trophyEnt.getComponent<cro::Transform>().move({ 0.f, 18.f });
        }
        trophyEnt.getComponent<cro::Callback>().active = true;

        //update minimap
        const auto uiSize = size / m_viewScale;
        constexpr auto mapSize = glm::vec2(MiniMapSize / 4u);
        m_mapRoot.getComponent<cro::Transform>().setPosition({ uiSize.x - mapSize.y - 2.f, uiSize.y - mapSize.x - (UIBarHeight + 2.f), -0.65f }); //map sprite is rotated 90


        greenEntRoot.getComponent<cro::Transform>().setPosition({ 2.f, uiSize.y - std::floor(MiniMapSize.y * 0.6f) - UIBarHeight - 2.f, 0.1f });
        greenEntRoot.getComponent<cro::Transform>().move(glm::vec2(static_cast<float>(MiniMapSize.y) / 4.f));

        windEnt.getComponent<cro::Transform>().setPosition(glm::vec2(WindIndicatorPosition.x, WindIndicatorPosition.y));
        windEnt2.getComponent<cro::Transform>().setPosition(glm::vec2(WindIndicatorPosition.x, WindIndicatorPosition.y));
        spinEnt.getComponent<cro::Transform>().setPosition(glm::vec2(std::floor(uiSize.x / 2.f), 32.f));

        //update the overlay
        auto colour = cro::Colour(0.f, 0.f, 0.f, OverlayAlpha);
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
        if (m_sharedData.useLargePowerBar)
        {
            uiPos.y *= 2.f;
            spinEnt.getComponent<cro::Transform>().move({ 0.f, 32.f, 0.f });
        }

        rootNode.getComponent<cro::Transform>().setPosition(glm::vec3(uiPos, 0.1f));

        //this calls the update for the scoreboard render texture
        updateScoreboard();
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.setRenderFlags(cro::Camera::Pass::Final, ~RenderFlags::Reflection);
    cam.resizeCallback = updateView;
    updateView(cam);
    m_uiScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 5.f });

    m_emoteWheel.build(infoEnt, m_uiScene, m_resources.textures);
}

void GolfState::createPowerBars(cro::Entity rootNode)
{
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    const auto flagDistanceCallback = [&](cro::Entity e, float dt)
        {
            const float vScaleTarget = m_currentPlayer.terrain == TerrainID::Green ? 1.f : 0.f;
            const float BarWidth = e.getComponent<cro::Callback>().getUserData<const float>();

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
                    float cpuPower = hTarget;
                    if (m_sharedData.scoreType == ScoreType::NearestThePin
                        && m_distanceToHole < 1)
                    {
                        cpuPower *= 0.6f;
                    }

                    m_cpuGolfer.setPuttingPower(cpuPower);
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
    
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_resources.textures);

    //small power bar

    //power bar frame
    float scale = m_sharedData.useLargePowerBar ? 0.f : 1.f;
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(scale));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::BarEnt;
    entity.addComponent<cro::Sprite>() = m_sharedData.decimatePowerBar ? m_sprites[SpriteID::PowerBar10] : m_sprites[SpriteID::PowerBar];
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec3(bounds.width / 2.f, bounds.height / 2.f, -0.05f));
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto barEnt = entity;

    //displays the range of the selected putter
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(bounds.width + 10.f, 12.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PuttPower;
    barEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto rangeEnt = entity;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().rotate(cro::Util::Const::PI);
    entity.getComponent<cro::Transform>().setPosition({ 3.f, 11.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("slope_indicator");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PuttPower;
    rangeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //power bar
    auto BarCentre = bounds.width / 2.f;
    auto BarWidth = bounds.width - 8.f;
    auto BarHeight = bounds.height;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(5.f, 0.f, 0.3f)); //TODO expell the magic number!!
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = 
        m_sharedData.useContrastPowerBar ?
        m_sprites[SpriteID::PowerBarInnerHC] : m_sprites[SpriteID::PowerBarInner];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PowerBarInner;
    entity.addComponent<cro::Callback>().active = !m_sharedData.useLargePowerBar;
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(BarCentre, 8.f, 0.55f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::HookBar];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f)));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, BarCentre](cro::Entity e, float)
        {
            glm::vec3 pos(std::round(BarCentre + (BarCentre * m_inputParser.getHook())), 8.f, 0.55f);
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
    entity.getComponent<cro::Callback>().setUserData<const float>(BarWidth);
    entity.getComponent<cro::Callback>().function = flagDistanceCallback;
        
    barEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //large power bar
    scale = scale == 1 ? 0.f : 1.f;

     //power bar frame
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(scale));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::BarEntLarge;
    entity.addComponent<cro::Sprite>() = m_sharedData.decimatePowerBar ? m_sprites[SpriteID::PowerBarDouble10] : m_sprites[SpriteID::PowerBarDouble];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec3(bounds.width / 2.f, (bounds.height / 2.f) - 2.f, -0.07f));
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    barEnt = entity;

    //current selected range
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(bounds.width + 8.f, 12.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PuttPower;
    barEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    rangeEnt = entity;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().rotate(cro::Util::Const::PI);
    entity.getComponent<cro::Transform>().setPosition({ 3.f, 11.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("slope_indicator");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PuttPower;
    rangeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //power bar
    BarCentre = bounds.width / 2.f;
    BarWidth = bounds.width - 16.f;
    BarHeight = bounds.height;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(10.f, 0.f, 0.3f)); //TODO expell the magic number!!
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() =
        m_sharedData.useContrastPowerBar ?
        m_sprites[SpriteID::PowerBarDoubleInnerHC] : m_sprites[SpriteID::PowerBarDoubleInner];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PowerBarInnerLarge;
    entity.addComponent<cro::Callback>().active = m_sharedData.useLargePowerBar;
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(BarCentre, 16.f, 0.55f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::HookBarDouble];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f)));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, BarCentre](cro::Entity e, float)
        {
            glm::vec3 pos(std::round(BarCentre + (BarCentre * m_inputParser.getHook())), 16.f, 0.55f);
            e.getComponent<cro::Transform>().setPosition(pos);
        };
    barEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //flag power/distance when putting
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(2.f, BarHeight, -0.05f));
    entity.getComponent<cro::Transform>().setOrigin({ -12.f, 6.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::MiniFlagLarge];
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<const float>(BarWidth);
    entity.getComponent<cro::Callback>().function = flagDistanceCallback;

    barEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void GolfState::showCountdown(std::uint8_t seconds)
{
    Timeline::setTimelineDesc("Final Scores");
    Timeline::addEvent(Timeline::Event::EndOfRound);

    //allow teams in hotseat
    if (m_sharedData.teamMode
        && m_humanCount != 0)
    {
        Achievements::setActive(true);
        Achievements::awardAchievement(AchievementStrings[AchievementID::Tag]);
    }
    m_roundEnded = true;
    Achievements::setActive(m_allowAchievements); //make sure these are re-enabled in case CPU player was last

#ifdef USE_GNS
    if (m_sharedData.leagueRoundID == LeagueRoundID::Club
        && m_sharedData.scoreType == ScoreType::Stroke)
    {
        Social::incCompletionCount(m_sharedData.mapDirectory, m_sharedData.holeCount);
    }
#endif

    if (m_achievementTracker.eagles > 1
        && m_achievementTracker.birdies > 2)
    {
        Achievements::awardAchievement(AchievementStrings[AchievementID::Nested]);
    }

    if (m_friendlyPlayers
        && m_sharedData.gameMode != GameMode::Tournament)
    {
        Achievements::awardAchievement(AchievementStrings[AchievementID::DiveIn]);
    }

    if (m_currentHole == 17) //full round
    {
        Achievements::incrementStat(StatStrings[StatID::HolesPlayed]);
        
        Achievements::setActive(true); //this unlocks the clubhouse so allow even in hotseat
        Achievements::awardAchievement(AchievementStrings[AchievementID::JoinTheClub]);
        Achievements::setActive(m_allowAchievements);
    }
    if (m_sharedData.scoreType != ScoreType::Skins)
    {
        Social::awardXP(xpValues[XPID::CompleteCourse] / (18 / m_holeData.size()), XPStringID::CourseComplete);
    }

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
            auto isCPU = m_sharedData.localConnectionData.playerData[m_statBoardScores[0].player].isCPU;
            if (!isCPU
                && (m_statBoardScores[0].score != m_statBoardScores[1].score //don't award if drawn in first position
                    || m_sharedData.scoreType == ScoreType::NearestThePin)) //unless this is NTP because the holes scores don't count
            {
                //remember this is auto-disabled if the player is not the only one on the client
                Achievements::awardAchievement(AchievementStrings[AchievementID::LeaderOfThePack]);

                switch (m_sharedData.scoreType)
                {
                default: break;
                case ScoreType::Stroke:
                    Achievements::awardAchievement(AchievementStrings[AchievementID::StrokeOfGenius]);

                    if (m_friendlyPlayers
                        && m_sharedData.gameMode != GameMode::Tournament)
                    {
                        Achievements::awardAchievement(AchievementStrings[AchievementID::QuickStart + m_sharedData.preferredClubSet]);
                    }
                    break;
                case ScoreType::Match:
                    Achievements::awardAchievement(AchievementStrings[AchievementID::NoMatch]);
                    break;
                case ScoreType::Skins:
                    Achievements::awardAchievement(AchievementStrings[AchievementID::SkinOfYourTeeth]);
                    break;
                case ScoreType::Stableford:
                case ScoreType::StablefordPro:
                    Achievements::awardAchievement(AchievementStrings[AchievementID::BarnStormer]);
                    break;
                case ScoreType::MultiTarget:
                    Achievements::awardAchievement(AchievementStrings[AchievementID::HitTheSpot]);
                    break;
                case ScoreType::Elimination:
                    Achievements::awardAchievement(AchievementStrings[AchievementID::TopOfThePile]);
                    break;
                case ScoreType::ClubShuffle:
                    Achievements::awardAchievement(AchievementStrings[AchievementID::LikeTotallyRandom]);
                    break;
                case ScoreType::NearestThePin:
                    if (m_ntpPro)
                    {
                        if (m_achievementTracker.plusFour)
                        {
                            Achievements::awardAchievement(AchievementStrings[AchievementID::PlusFours]);
                        }
                    }
                    else
                    {
                        Achievements::awardAchievement(AchievementStrings[AchievementID::PinKing]);
                    }
                    break;
                }
            }

            //message for audio director
            if (m_humanCount != 0)
            {
                auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::RoundEnd;
                msg->score = isCPU ? 1 : 0;
            }
        }
        else if (m_statBoardScores.back().client == m_sharedData.clientConnection.connectionID
            && m_humanCount != 0)
        {
            auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::RoundEnd;
            msg->score = 1; //lose
        }
    }

    if (m_holeData.size() > 8) //only consider it a round if there are at least 9 holes
    {
        Achievements::incrementStat(StatStrings[StatID::TotalRounds]);
        Achievements::incrementStat(StatStrings[StatID::PlaysInClearWeather + m_sharedData.weatherType]);

        std::int32_t weatherProgress = 0;
        for (auto i = 0; i < WeatherType::Random; ++i)
        {
            if (Achievements::getStat(StatStrings[StatID::PlaysInClearWeather + i])->value != 0)
            {
                weatherProgress++;
            }
        }

        //only show the update if this type of weather is new to the stat
        if (Achievements::getStat(StatStrings[StatID::PlaysInClearWeather + m_sharedData.weatherType])->value == 1)
        {
            if (weatherProgress == 4)
            {
                Achievements::awardAchievement(AchievementStrings[AchievementID::RainOrShine]);
            }
            else
            {
#ifdef USE_GNS
                Achievements::indicateProgress(AchievementID::RainOrShine, weatherProgress, 4);
#endif

                //progress event - hm can't use this as updating the league immediately overrules it
                //auto* msg = cro::App::postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
                //msg->type = Social::SocialEvent::MonthlyProgress;
                //msg->challengeID = -2; //TODO enumerate this properly!!
                //msg->level = weatherProgress;
                //msg->reason = 4;
            }
        }

        if (m_statBoardScores.size() > 3 //this assumes 4 players or more... is this right?
            && m_courseIndex != -1)
        {
            Social::getMonthlyChallenge().updateChallenge(ChallengeID::Four, m_courseIndex);
        }

        if (m_holeData.size() == 18)
        {
            Achievements::setAvgStat(m_sharedData.mapDirectory, m_playTime.asSeconds(), 1.f);
            if (CourseIDs.count(m_sharedData.mapDirectory) != 0)
            {
                Achievements::awardAchievement(AchievementStrings[CourseIDs.at(m_sharedData.mapDirectory)]);
                if (m_sharedData.leagueRoundID == LeagueRoundID::Club)
                {
                    auto type = m_sharedData.scoreType;
                    if (m_ntpPro)
                    {
                        type = ScoreType::NearestThePinPro;
                    }
                    Social::getMonthlyChallenge().updateChallenge(ChallengeID::Three, type);
                }
                if (m_sharedData.scoreType == ScoreType::Elimination
                    && m_currentHole >= 17) //not completely clear if this gets incremented all the way to 18 (pretty sure it doesn't)
                {
                    Achievements::awardAchievement(AchievementStrings[AchievementID::Survivor]);
                }

                if (m_sharedData.nightTime)
                {
                    Achievements::awardAchievement(AchievementStrings[AchievementID::NightOwl]);
                }

                if (Achievements::getActive())
                {
                    m_achievementDebug.awardStatus = "Awarded Course Complete: " + m_sharedData.mapDirectory.toAnsiString();
                }
                else
                {
                    m_achievementDebug.awardStatus = "Tried to award Course Complete but achievements were not active";
                }
            }
            else
            {
                m_achievementDebug.awardStatus = "Tried to award Course Complete but ID for " + m_sharedData.mapDirectory.toAnsiString() + " was not found.";
            }

            //if we're stroke play see if we get the achievement
            //for coming in under par
            const auto pd = m_sharedData.connectionData[m_sharedData.localConnectionData.connectionID];
            for(auto j = 0; j < pd.playerCount; ++j)
            {
                const auto& p = pd.playerData[j];

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

                    if (m_achievementTracker.under15metres
                        && m_sharedData.scoreType == ScoreType::NearestThePin
                        && CourseIDs.count(m_sharedData.mapDirectory) != 0)
                    {
                        float f = 0.f;
                        for (auto s : p.distanceScores)
                        {
                            f += s;
                        }

                        if (f < 15.f)
                        {
                            Achievements::awardAchievement(AchievementStrings[AchievementID::SnugAsABug]);
                        }
                    }
                }
            }
        }
        else if (m_holeData.size() == 9)
        {
            if (m_achievementTracker.under15metres
                && m_sharedData.scoreType == ScoreType::NearestThePin
                && CourseIDs.count(m_sharedData.mapDirectory) != 0)
            {
                const auto pd = m_sharedData.connectionData[m_sharedData.localConnectionData.connectionID];
                for (auto j = 0; j < pd.playerCount; ++j)
                {
                    const auto& p = pd.playerData[j];

                    if (!p.isCPU)
                    {
                        auto f = 0.f;
                        for (auto s = 0; s < 9; ++s)
                        {
                            f += p.distanceScores[s];
                        }

                        if (f < 15.f)
                        {
                            Achievements::awardAchievement(AchievementStrings[AchievementID::ShortDistanceClara]);
                        }
                    }
                }
            }
        }
        else
        {
            if (m_holeData.size() == 12
                && m_sharedData.scoreType == ScoreType::ShortRound)
            {
                Social::getMonthlyChallenge().updateChallenge(ChallengeID::Three, m_sharedData.scoreType);
            }

            m_achievementDebug.awardStatus = "Did not award Course Complete: there were only " + std::to_string(m_holeData.size()) + " holes.";
        }

        //this increments all the aggregated stats
        Social::courseComplete(m_sharedData.mapDirectory, m_sharedData.holeCount);
    }

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //only show trophies in freeplay/tournament
    cro::Entity leagueEntity;
    bool firstPlace = false;
    
    if (m_sharedData.leagueRoundID == LeagueRoundID::Club)
    {
        auto scoreCount = m_statBoardScores.size();
        if (m_sharedData.teamMode)
        {
            scoreCount /= 2;
            scoreCount += m_statBoardScores.size() % 2;
        }

        const auto trophyCount = std::min(std::size_t(3), scoreCount);
        for (auto i = 0u; i < trophyCount; ++i)
        {
            if (!m_sharedData.teamMode //don't give SP awards for team mode
                && m_statBoardScores[i].client == m_sharedData.clientConnection.connectionID //if this is a virtual CPU client is NullVal
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
                    std::int32_t credits = 0;
                    switch (i)
                    {
                    default: break;
                    case 0:
                        xp = static_cast<float>(xpValues[XPID::First]) * multiplier;
                        xpReason = XPStringID::FirstPlace;
                        firstPlace = true;
                        credits = m_sharedData.holeCount == 0 ? CreditID::FreePlayFirst : CreditID::FreePlayFirst / 2;
                        break;
                    case 1:
                        xp = static_cast<float>(xpValues[XPID::Second]) * multiplier;
                        xpReason = XPStringID::SecondPlace;
                        credits = m_sharedData.holeCount == 0 ? CreditID::FreePlaySecond : CreditID::FreePlaySecond / 2;
                        break;
                    case 2:
                        xp = static_cast<float>(xpValues[XPID::Third]) * multiplier;
                        xpReason = XPStringID::ThirdPlace;
                        credits = m_sharedData.holeCount == 0 ? CreditID::FreePlayThird : CreditID::FreePlayThird / 2;
                        break;
                    }
                    Social::awardXP(static_cast<std::int32_t>(xp), xpReason);
                    updateTournament(i == 0);


                    if (m_friendlyPlayers
                        && m_friendlyPlayers->hasRival())
                    {
                        //we're actually playing solo
                        credits = m_sharedData.holeCount == 0 ? CreditID::FreePlayFirst / 2 : CreditID::FreePlayFirst / 4;
                    }

                    //this is irrelevant in anything but free play, but credits aren't awarded outside freeplay *anyway*
                    const auto parScore = m_sharedData.connectionData[m_statBoardScores[i].client].playerData[m_statBoardScores[i].player].parScore;
                    if (parScore < 1)
                    {
                        //10 for par, then 5 for every stroke under
                        //TODO if we do this every hole we get a bigger reward...
                        credits += 10;
                        credits += 5 * (parScore * -1);
                    }

                    if (m_allowAchievements
                        && credits
                        && m_sharedData.gameMode == GameMode::FreePlay)
                    {
                        awardCredits(credits);
                    }
                }
                else
                {
                    //we don't want to give away 1st place prize money if we're the only free-player
                    if (m_allowAchievements
                        && m_sharedData.gameMode == GameMode::FreePlay)
                    {
                        auto credits = m_sharedData.holeCount == 0 ? CreditID::FreePlayFirst / 2 : CreditID::FreePlayFirst / 4;
                    
                        const auto parScore = m_sharedData.connectionData[m_statBoardScores[i].client].playerData[m_statBoardScores[i].player].parScore;
                        if (parScore < 1)
                        {
                            //10 for par, then 5 for every stroke under
                            credits += 10;
                            credits += 5 * (parScore * -1);
                        }

                        awardCredits(credits);
                    }
                }
            }

            m_trophies[i].trophy.getComponent<TrophyDisplay>().state = TrophyDisplay::In;
            //m_trophyLabels[i].getComponent<cro::Callback>().active = true; //this is done by TrophyDisplay (above) to properly delay it

            const auto scoreIndex = m_sharedData.teamMode ? i * 2 : i;
           
            //this might be true if we're playing a Quick Round
            if (m_statBoardScores[scoreIndex].client != ConstVal::NullValue)
            {
                m_trophies[i].badge.getComponent<cro::SpriteAnimation>().play(std::min(5, m_sharedData.connectionData[m_statBoardScores[scoreIndex].client].level / 10));
                m_trophies[i].badge.getComponent<cro::Model>().setDoubleSided(0, true);

                m_trophies[i].label.getComponent<cro::Sprite>().setTexture(m_sharedData.nameTextures[m_statBoardScores[scoreIndex].client].getTexture(), false);
                auto bounds = m_trophies[i].label.getComponent<cro::Sprite>().getTextureBounds();
                bounds.bottom = bounds.height * m_statBoardScores[scoreIndex].player;
                m_trophies[i].label.getComponent<cro::Sprite>().setTextureRect(bounds);

                //team size might actually be 1 if odd number of players or someone
                //left mid-game
                auto teamSize = 1;
                if (m_sharedData.teamMode)
                {
                    const auto teamIndex = m_sharedData.connectionData[m_statBoardScores[scoreIndex].client].playerData[m_statBoardScores[scoreIndex].player].teamIndex;
                    if (m_teams[teamIndex].players[0] != m_teams[teamIndex].players[1])
                    {
                        teamSize = 2;
                    }
                }

                for (auto j = 0; j < teamSize; ++j)
                {
                    const auto idx = scoreIndex + j;
                    if (idx < m_statBoardScores.size())
                    {
                        //choose the relevant player from the sheet
                        bounds = getAvatarBounds(m_statBoardScores[idx].player);
                        m_trophies[i].avatars[j].getComponent<cro::Sprite>().setTexture(m_sharedData.nameTextures[m_statBoardScores[idx].client].getTexture(), false);
                        m_trophies[i].avatars[j].getComponent<cro::Sprite>().setTextureRect(bounds);

                        //offset icons if team mode
                        if (teamSize == 2)
                        {
                            const auto offset = (j * 2) - 1;
                            m_trophies[i].avatars[j].getComponent<cro::Transform>().move({ offset * 22.f, 0.f });

                        }
                        //and add another team label
                        if (j == 1)
                        {
                            auto entity = m_uiScene.createEntity();
                            entity.addComponent<cro::Transform>();
                            entity.addComponent<cro::Drawable2D>();
                            entity.addComponent<cro::Sprite>(*m_trophies[i].avatars[j].getComponent<cro::Sprite>().getTexture());
                            
                            auto bounds = m_trophies[i].label.getComponent<cro::Sprite>().getTextureBounds();
                            bounds.bottom = bounds.height * m_statBoardScores[idx].player;
                            entity.getComponent<cro::Sprite>().setTextureRect(bounds);
                            entity.getComponent<cro::Transform>().move({ 0.f, -bounds.height });
                            m_trophies[i].label.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                        }
                    }
                }
            }
            else
            {
                //this is a league player so there's nothing to show...
                //and should never be true for team modes
                m_trophies[i].avatars[0].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_trophies[i].avatars[0].getComponent<cro::Callback>().active = false; //otherwise this overrides the scale
                m_trophies[i].label.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                //m_trophies[i].label.getComponent<cro::Callback>().active = false;
                //TrophyDisplaySystem sets this true so we override the function to do nothing
                m_trophies[i].label.getComponent<cro::Callback>().function = [](cro::Entity, float) {};
                m_trophies[i].badge.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
        }
        m_trophyScene.getActiveCamera().getComponent<cro::Camera>().active = true;
    }
    else
    {
        //check which position we came in and award prize credits
        const auto count = std::min(std::size_t(3), m_statBoardScores.size());
        for (auto i = 0u; i < count; ++i)
        {
            if (m_statBoardScores[i].client == m_sharedData.clientConnection.connectionID) //virtual players are NullValue
            {
                switch (i)
                {
                default: break;
                case 0:
                    awardCredits(CreditID::LeagueRoundFirst);
                    break;
                case 1:
                    awardCredits(CreditID::LeagueRoundSecond);
                    break;
                case 2:
                    awardCredits(CreditID::LeagueRoundThird);
                    break;
                }
            }
        }

        for (auto fw : m_fireworks)
        {
            fw.getComponent<cro::Callback>().active = true;
        }
        m_trophyScene.getActiveCamera().getComponent<cro::Camera>().active = true;

        //otherwise show some stats about how we did in the league
        leagueEntity = m_uiScene.createEntity();
        leagueEntity.addComponent<cro::Transform>().setPosition({ 200.f + scoreboardExpansion, 183.f, 1.8f });
        leagueEntity.addComponent<cro::Drawable2D>();
        leagueEntity.addComponent<UIElement>().absolutePosition = { 200.f, 183.f };
        leagueEntity.getComponent<UIElement>().depth = 1.8f;
        leagueEntity.getComponent<UIElement>().resizeCallback = [](cro::Entity e)
            {
                e.getComponent<cro::Transform>().move({ scoreboardExpansion, 0.f });
            };
        leagueEntity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
        leagueEntity.addComponent<cro::Text>(font).setCharacterSize(UITextSize * 2);
        leagueEntity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
        leagueEntity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        leagueEntity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        leagueEntity.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing * 2.f);
        leagueEntity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);

        //attaches to scoreboard - hmm surely this can go directly to m_scoreboardEnt?
        cmd.targetFlags = CommandID::UI::Scoreboard;
        cmd.action =
            [leagueEntity](cro::Entity e, float) mutable
            {
                e.getComponent<cro::Transform>().addChild(leagueEntity.getComponent<cro::Transform>());
            };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        //dim the score text which gets in the way of the message
        auto dark = LeaderboardTextDark;
        dark.setAlpha(0.4f);

        auto& ents = m_scoreboardEnt.getComponent<cro::Callback>().getUserData<std::vector<cro::Entity>>();
        for (auto i = 1u; i < ents.size() - 1; ++i)
        {
            //hmmmm as we use multi-colours in a single string now the best we can
            //do for the alpha is reset ALL the colours (although this is post-game
            //so probably doesn't really matter)
            const auto s = ents[i].getComponent<cro::Text>().getString();
            ents[i].getComponent<cro::Text>().setFillColour(dark);
            //calling setString() resets all the colours
            ents[i].getComponent<cro::Text>().setString(s);
        }
        ents[0].getComponent<cro::Text>().setFillColour(dark);
        ents.back().getComponent<cro::Text>().setFillColour(dark);

        //check to see if we are top of the scoreboard
        if (m_achievementTracker.leadingCareerRound
            && !m_resumedFromSave) //no cheese please
        {
            auto active = Achievements::getActive();
            Achievements::setActive(m_allowAchievements);
            switch (m_sharedData.clubSet)
            {
            default: break;
            case 0:
                Achievements::awardAchievement(AchievementStrings[AchievementID::StartingOut]);
                break;
            case 1:
                Achievements::awardAchievement(AchievementStrings[AchievementID::GettingTheHangOfIt]);
                break;
            case 2:
                Achievements::awardAchievement(AchievementStrings[AchievementID::ProShow]);
                break;
            }
            Achievements::setActive(active);
        }
    }

    //award a nominal amount of credits for playing hotseat
    if (m_hotSeat)
    {
        awardCredits(50 + (m_sharedData.localConnectionData.playerCount * 10));
    }

    bool personalBest = false;
    cro::String bestString("PERSONAL BEST!");

#ifndef CRO_DEBUG_
    if (m_sharedData.gameMode == GameMode::Tournament)
    {
        personalBest = true; //this just indicates we want to show the message
        if (!firstPlace)
        {
            bestString = "ELIMINATED";
        }
        else
        {
            bestString = "WINNER";
        }
    }
    else
    {
        //enter score into leaderboard - note this also
        //sets the 'tutorial' string so debug builds will
        //show the wrong message when completing the tutorial
        updateLeaderboardScore(personalBest, bestString);
    }
#endif

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 200.f + scoreboardExpansion, 10.f, 2.23f }); //attaches to scoreboard
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

        if (m_sharedData.gameMode == GameMode::Tutorial
            || m_sharedData.gameMode == GameMode::Tournament
            || m_sharedData.leagueRoundID != LeagueRoundID::Club)
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

    //attach to the scoreboard - surely we can do this directly with m_scoreboardEnt?
    cmd.targetFlags = CommandID::UI::Scoreboard;
    cmd.action =
        [entity](cro::Entity e, float) mutable
    {
        e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    if ((personalBest && Social::getLeaderboardsEnabled())
        || m_sharedData.gameMode == GameMode::Tutorial
        || (personalBest && m_sharedData.gameMode == GameMode::Tournament))
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
        entity.getComponent<cro::Text>().setString(bestString);
        centreText(entity);

        cmd.targetFlags = CommandID::UI::Scoreboard;
        cmd.action =
            [entity](cro::Entity e, float) mutable
        {
            e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        //extra fireworks for a good score (probably don't want them for losing a tournament...)
        if (m_sharedData.gameMode != GameMode::Tournament)
        {
            for (auto fw : m_fireworks)
            {
                fw.getComponent<cro::Callback>().active = true;
            }
        }
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
            for (auto i = 0u; i < ConstVal::MaxClients; ++i)
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

    if (m_courseIndex != -1)
    {
        switch (m_sharedData.scoreType)
        {
        default: break;
        case ScoreType::Stroke:
            m_statResult = std::async(std::launch::async, &GolfState::updateProfileDB, this);
            [[fallthrough]];
        case ScoreType::Stableford:
        case ScoreType::StablefordPro:
        case ScoreType::ShortRound:
        {
            auto& league = Career::instance(m_sharedData).getLeagueTables()[std::max(0, m_sharedData.leagueRoundID - LeagueRoundID::RoundOne)];
            const auto pos = league.getCurrentPosition();
            const auto lastSeason = league.getCurrentSeason();

            cro::String str;
            if (leagueEntity.isValid())
            {
                str = "CAREER ROUND COMPLETE\n\n";// Previous Rank : " + std::to_string(pos);
            }

            updateLeague();

            if (leagueEntity.isValid())
            {
                if (league.getCurrentSeason() == lastSeason)
                {
                    const auto newPos = league.getCurrentPosition();
                    str += "\nCurrent Rank: " + std::to_string(newPos);

                    static const std::array<std::string, 2u> Indicators = { u8" (↓", u8" (↑" };
                    const auto change = newPos - pos;
                    if (change < 0)
                    {
                        //we went up (lower is better)
                        str += cro::String::fromUtf8(Indicators[1].begin(), Indicators[1].end());
                        str += std::to_string(std::abs(change)) + ")";
                    }
                    else if (change > 0)
                    {
                        str += cro::String::fromUtf8(Indicators[0].begin(), Indicators[0].end());
                        str += std::to_string(change) + ")";
                    }
                    else
                    {
                        str += " (NC)";
                    }
                }
                else
                {
                    //we need this to update the previous position value
                    league.readPreviousPlayers();
                    league.getPreviousResults(Social::getPlayerName());
                    const auto finalPos = league.getPreviousPosition();
                    auto posStr = std::to_string(finalPos);
                    switch (finalPos)
                    {
                    default:
                        posStr += "th";
                        break;
                    case 1:
                        posStr += "st";
                        break;
                    case 2:
                        posStr += "nd";
                        break;
                    case 3:
                        posStr += "rd";
                        break;
                    }

                    //this was the final round
                    str += "Season Complete!\nYou Placed: " + posStr;
                }

                leagueEntity.getComponent<cro::Text>().setString(str);
            }
        }
            break;
        }

    }
    refreshUI();

    if (m_sharedData.logCSV
        && m_sharedData.scoreType == ScoreType::Stroke)
    {
        m_csvResult = std::async(std::launch::async, &GolfState::logCSV, this);
    }
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
    rootEnt.addComponent<cro::Transform>().setPosition({ size.x, -size.y, 1.f });
    rootEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::ScoreboardController;
    //use the callback to keep the board centred/scaled
    rootEnt.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        static constexpr float Speed = 10.f;

        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        auto target = glm::vec2(size / 2.f);
        target.y -= e.getComponent<cro::Callback>().getUserData<std::int32_t>() * size.y;

        auto pos = glm::vec2(e.getComponent<cro::Transform>().getPosition());
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
    entity.getComponent<UIElement>().depth = 0.5f;
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

    //title
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

    //course leader if available
    if (!m_courseTitle.empty())
    {
        auto str = m_courseTitle;

        if (m_sharedData.scoreType == ScoreType::Stroke)
        {
            auto leader = Social::getLeader(m_sharedData.mapDirectory, m_sharedData.holeCount);
            if (!leader.empty())
            {
                str += " - " + leader;
            }
            else
            {
                str += " - " + ScoreTypes[m_sharedData.scoreType];
            }
        }
        else
        {
            str += " - " + ScoreTypes[m_sharedData.scoreType];
        }

        if (m_ntpPro)
        {
            str += "+";
        }

        if (m_sharedData.scoreType == ScoreType::Skins)
        {
            str += " - Pot: 1";
        }

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::ScoreTitle;
        entity.addComponent<UIElement>().absolutePosition = { 200.f, 11.f };
        entity.getComponent<UIElement>().depth = 0.5f;
        entity.getComponent<UIElement>().resizeCallback = resizeCentre;
        entity.addComponent<cro::Text>(font).setString(str);
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
            crop.width += RightAdj;
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


            //child ent for red score text
            /*auto f = m_uiScene.createEntity();
            f.addComponent<cro::Transform>().setPosition({ 0.f, -7.f, 0.f });;
            f.addComponent<cro::Drawable2D>();
            f.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
            f.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
            f.getComponent<cro::Text>().setFillColour(TextHighlightColour);
            e.getComponent<cro::Transform>().addChild(f.getComponent<cro::Transform>());
            e.addComponent<cro::Entity>() = f;*/

            if (e != ents.back())
            {
                e.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
                //f.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
            }
        }

        scrollEnt.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
        i++;
    }
    ents.back().getComponent<cro::Transform>().setPosition(glm::vec3(ColumnPositions.back(), 0.5f));
    ents.back().getComponent<UIElement>().absolutePosition = ColumnPositions.back() - glm::vec2(0.f, UITextPosV);


    //net strength icons - note that this works by creating an instance for EVERY player
    //and then sets the animation/avatar texture based on current score positions - the
    //icons themselves DON'T MOVE. As such these need to be identical for human and league
    //players alike.
    glm::vec3 iconPos(8.f, 235.f, 2.2f);
    const auto createIcon = [&](std::uint8_t cl, std::uint8_t pl, bool lp) 
        {
            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(iconPos);
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("strength_meter");
            entity.addComponent<cro::SpriteAnimation>();
            //this is set active only when scoreboard is visible
            entity.addComponent<cro::Callback>().setUserData<NetStatData>(cl, pl, lp);
            entity.getComponent<cro::Callback>().function =
                [&, iconPos](cro::Entity e, float)
                {
                    auto [client, player, leaguePlayer] = e.getComponent<cro::Callback>().getUserData<NetStatData>();

                    if (leaguePlayer ||
                        m_sharedData.connectionData[client].playerData[player].isCPU)
                    {
                        e.getComponent<cro::SpriteAnimation>().play(5);
                    }
                    else
                    {
                        auto idx = m_sharedData.connectionData[client].pingTime / 60;
                        e.getComponent<cro::SpriteAnimation>().play(std::min(4u, idx));
                    }

                    auto pos = iconPos;
                    pos.x += 366.f + (scoreboardExpansion * 2.f);
                    e.getComponent<cro::Transform>().setPosition(pos);
                };
            bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_netStrengthIcons.push_back(entity);

            return entity;
        };
    const auto createAvatar = [&](cro::Entity barEnt, std::uint8_t cl, std::uint8_t pl)
        {
            static constexpr glm::vec2 AVScale = glm::vec2(14.f) / LabelIconSize;

            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setScale(AVScale);
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>(m_sharedData.nameTextures[cl].getTexture());
            entity.getComponent<cro::Sprite>().setTextureRect(getAvatarBounds(pl));
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, barEnt](cro::Entity e, float)
                {
                    if (barEnt.destroyed())
                    {
                        e.getComponent<cro::Callback>().active = false;
                        m_uiScene.destroyEntity(e);
                        return;
                    }

                    //these are set on the ent by updating the scoreboard, rather than rearranging entity positions
                    auto [client, player, leaguePlayer] = barEnt.getComponent<cro::Callback>().getUserData<NetStatData>();
                    if (leaguePlayer)
                    {
                        e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    }
                    else
                    {
                        e.getComponent<cro::Sprite>().setTexture(m_sharedData.nameTextures[client].getTexture(), false);
                        e.getComponent<cro::Sprite>().setTextureRect(getAvatarBounds(player));
                        e.getComponent<cro::Transform>().setPosition({ -(367.f + (scoreboardExpansion * 2.f)), 2.f });
                        e.getComponent<cro::Transform>().setScale(AVScale);
                    }
                };
            barEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        };

    static constexpr float IconSpacing = 14.f;
    for (const auto& c : m_sharedData.connectionData)
    {
        for (auto j = 0u; j < c.playerCount; ++j)
        {
            //strength bar icon
            auto barEnt = createIcon(c.connectionID, j, false);

            //player avatar icon
            createAvatar(barEnt, c.connectionID, j);

            iconPos.y -= IconSpacing;
        }
    }

    if (m_sharedData.leagueRoundID != LeagueRoundID::Club)
    {
        //create 'net strength' icons to display CPU status
        //as well as pad out the net status vector to the correct size
        for (auto j = 0u; j < League::PlayerCount; ++j)
        {
            createAvatar(createIcon(0,0,true), 0, 0);            
            iconPos.y -= IconSpacing;
        }

        //add a nemesis icon to show if a CPU player is hot on our tail
        if (Career::instance(m_sharedData).getLeagueTables()[m_sharedData.leagueRoundID - LeagueRoundID::RoundOne].getNemesis() != -1)
        {
            static constexpr glm::vec3 NemesisPos({ 10.f, 240.f, 2.2f });
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(NemesisPos);
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("nemesis");
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<std::int32_t>(0);
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float)
                {
                    const auto& p = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
                    auto pos = NemesisPos;
                    pos.y -= p * IconSpacing;
                    e.getComponent<cro::Transform>().setPosition(pos);
                };
            bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_nemesisEnt = entity;
        }
    }

    //create leaderboard entries for any 'quick round' players
    if (m_friendlyPlayers)
    {
        for (auto j = 0u; j < m_friendlyPlayers->getPlayerCount(); ++j)
        {
            createAvatar(createIcon(0, 0, true), 0, 0);
            iconPos.y -= IconSpacing;
        }
    }

    updateScoreboard();
}

void GolfState::updateScoreboard(bool updateParDiff)
{
    struct ScoreEntry final
    {
        cro::String name;
        std::vector<std::int32_t> holes;
        std::vector<float> distances;
        std::vector<bool> holeComplete;
        std::int32_t frontNine = 0;
        std::int32_t backNine = 0;
        std::int32_t total = 0;
        std::int32_t parDiff = 0;

        float totalDistance = 0.f;
        float frontNineDistance = 0.f;
        float backNineDistance = 0.f;

        std::uint8_t client = 0;
        std::uint8_t player = 0;
        std::uint8_t lives = 0;

        bool leaguePlayer = false;
        std::int32_t nameIndex = -1;
        std::int32_t teamIndex = -1;
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
            entry.client = clientID; //used to display whether or not this is CPU or a netstrength icon
            entry.player = i; //see above
            entry.lives = client.playerData[i].skinScore; //mostly ignored, except in Elimination
            entry.teamIndex = client.playerData[i].teamIndex;

            bool overPar = false;

            //stupid bug in league play meant that holescores size could be too big when playing 9 holes...
            for (auto j = 0u; j < /*client.playerData[i].holeScores*/m_holeData.size(); ++j)
            {
                auto s = client.playerData[i].holeScores[j];
                entry.holes.push_back(s);
                entry.holeComplete.push_back(client.playerData[i].holeComplete[j]);

                auto f = client.playerData[i].distanceScores[j];
                entry.distances.push_back(f);

                std::int32_t stableScore = 0;

                //this needs to ignore the current hole
                //as the mid-point score looks confusing... however
                //we still want to count the current number of strokes...
                if (s)
                {                    
                    if (updateParDiff 
                        || j < (m_currentHole)
                        || entry.holeComplete.back())
                    {
                        auto diff = static_cast<std::int32_t>(s) - m_holeData[j].par;
                        stableScore = 2 - diff;

                        entry.parDiff += diff;
                        overPar = (diff > 0);
                    }
                }

                if (j < 9)
                {
                    switch (m_sharedData.scoreType)
                    {
                    default: //dear future me: the default type should *ALWAYS* be the same as stroke type. Everywhere.
                    case ScoreType::Elimination:
                    case ScoreType::MultiTarget:
                    case ScoreType::Stroke:
                    case ScoreType::ShortRound:
                        entry.frontNine += client.playerData[i].holeScores[j];
                        break;
                    case ScoreType::Stableford:
                        stableScore = std::max(0, stableScore);
                        entry.frontNine += stableScore;
                        entry.holes.back() = stableScore;
                        break;
                    case ScoreType::StablefordPro:
                        if (stableScore < 2
                            && entry.holeComplete.back())
                        {
                            stableScore -= 2;
                        }
                        entry.frontNine += stableScore;
                        entry.holes.back() = stableScore;
                        break;
                    case ScoreType::Match:
                        entry.frontNine = client.playerData[i].matchScore;
                        break;
                    case ScoreType::Skins:
                        entry.frontNine = client.playerData[i].skinScore;
                        break;
                    case ScoreType::NearestThePin:
                        entry.frontNineDistance += f;
                        entry.frontNine = client.playerData[i].matchScore; //displayed if playing pro mode
                        break;
                    }

                }
                else
                {
                    switch (m_sharedData.scoreType)
                    {
                    default:
                    case ScoreType::Elimination:
                    case ScoreType::MultiTarget:
                    case ScoreType::Stroke:
                    case ScoreType::ShortRound:
                        entry.backNine += client.playerData[i].holeScores[j];
                        break;
                    case ScoreType::Stableford:
                        stableScore = std::max(0, stableScore);
                        entry.backNine += stableScore;
                        entry.holes.back() = stableScore;
                        break;
                    case ScoreType::StablefordPro:
                        if (stableScore < 2
                            && entry.holeComplete.back())
                        {
                            stableScore -= 2;
                        }
                        entry.backNine += stableScore;
                        entry.holes.back() = stableScore;
                        break;
                    case ScoreType::Match:
                        entry.backNine = client.playerData[i].matchScore;
                        break;
                    case ScoreType::Skins:
                        entry.backNine = client.playerData[i].skinScore;
                        break;
                    case ScoreType::NearestThePin:
                        entry.backNineDistance += f;
                        entry.backNine = client.playerData[i].matchScore;
                        break;
                    }
                }
            }
            client.playerData[i].parScore = entry.parDiff;

            
            switch (m_sharedData.scoreType)
            {
            default:
            case ScoreType::Elimination:
            case ScoreType::MultiTarget:
            case ScoreType::Stroke:
            case ScoreType::ShortRound:
                //track achievement make no mistake
                if (client.connectionID == m_sharedData.localConnectionData.connectionID
                    && !client.playerData[i].isCPU
                    && overPar)
                {
                    m_achievementTracker.noHolesOverPar = false;
                }

                [[fallthrough]];
            case ScoreType::Stableford:
            case ScoreType::StablefordPro:
                entry.total = entry.frontNine + entry.backNine;
                break;
            case ScoreType::Skins:
                entry.total = client.playerData[i].skinScore;
                break;
            case ScoreType::Match:
                //entry.total = entry.frontNine;
                entry.total = client.playerData[i].matchScore;
                break;
            case ScoreType::NearestThePin:
                entry.totalDistance = entry.frontNineDistance + entry.backNineDistance;
                entry.total = client.playerData[i].matchScore;
                break;
            }

            //for stat/achievment tracking
            auto& leaderboardEntry = m_statBoardScores.emplace_back();
            leaderboardEntry.client = clientID;
            leaderboardEntry.player = i;
            leaderboardEntry.score = entry.total;
            leaderboardEntry.distance = entry.totalDistance;
            leaderboardEntry.team = entry.teamIndex;
        }
        clientID++;
    }

    //if we're in a career game include the CPU player scores
    if (m_sharedData.leagueRoundID != LeagueRoundID::Club)
    {
        playerCount += League::PlayerCount;

        const auto& league = Career::instance(m_sharedData).getLeagueTables()[m_sharedData.leagueRoundID - LeagueRoundID::RoundOne]; //this is daft, we need to fix it
        const auto& leagueScores = league.getHoleScores();

        //this should only ever be stroke play, so some assumptions are made here
        for (auto i = 0u; i < leagueScores.size(); ++i)
        {
            auto& entry = scores.emplace_back();
            entry.name = m_sharedData.leagueNames[i];
            entry.client = 0;// ConstVal::NullValue; //can't set this to NullValue as something indexes from it some (I can't find it) but the OOR causes a crash elsewhere
            entry.player = 0;// ConstVal::NullValue;
            entry.leaguePlayer = true;
            entry.nameIndex = static_cast<std::int32_t>(i);

            for (auto j = 0u; j < m_holeData.size(); ++j)
            {
                auto s = leagueScores[i][j];
                entry.holes.push_back(s);
                entry.holeComplete.push_back(s != 0);

                if (s)
                {
                    if (updateParDiff
                        || j < (m_currentHole)
                        || entry.holeComplete.back())
                    {
                        auto diff = static_cast<std::int32_t>(s) - m_holeData[j].par;

                        entry.parDiff += diff;
                    }
                }

                //assuming stroke score
                if (j < 9)
                {
                    entry.frontNine += leagueScores[i][j];
                }
                else
                {
                    entry.backNine += leagueScores[i][j];
                }
            }

            entry.total = entry.frontNine + entry.backNine;

            //trophies aren't displayed in career games
            //auto& leaderboardEntry = m_statBoardScores.emplace_back();
            //leaderboardEntry.client = 0; 
            //leaderboardEntry.player = 0;
            //leaderboardEntry.score = entry.total;
            //leaderboardEntry.distance = entry.totalDistance;
        }
    }

    if (m_friendlyPlayers)
    {
        for (auto k = 0u; k < m_friendlyPlayers->getPlayerCount(); ++k)
        {
            const auto& friendlyScores = m_friendlyPlayers->getScores(k);
            const auto i = m_friendlyPlayers->getPlayerIndex(k);

            playerCount++;

            auto& entry = scores.emplace_back();
            entry.name = m_sharedData.leagueNames[i];
            entry.client = 0;
            entry.player = 0;
            entry.leaguePlayer = true;
            entry.nameIndex = static_cast<std::int32_t>(i);

            for (auto j = 0u; j < m_holeData.size(); ++j)
            {
                auto s = friendlyScores[j];
                entry.holes.push_back(s);
                entry.holeComplete.push_back(s != 0);

                if (s)
                {
                    if (updateParDiff
                        || j < (m_currentHole)
                        || entry.holeComplete.back())
                    {
                        auto diff = static_cast<std::int32_t>(s) - m_holeData[j].par;

                        entry.parDiff += diff;
                    }
                }

                //assuming stroke score
                if (j < 9)
                {
                    entry.frontNine += friendlyScores[j];
                }
                else
                {
                    entry.backNine += friendlyScores[j];
                }
            }

            entry.total = entry.frontNine + entry.backNine;

            auto& leaderboardEntry = m_statBoardScores.emplace_back();
            leaderboardEntry.client = ConstVal::NullValue; //this is used when displaying the trophy to make sure only human players have an avatar
            leaderboardEntry.player = ConstVal::NullValue;
            leaderboardEntry.score = entry.total;
            leaderboardEntry.distance = entry.totalDistance;
        }
    }

    //tracks stats and decides on trophy layout on round end (see showCountdown())
    std::sort(m_statBoardScores.begin(), m_statBoardScores.end(),
        [&](const StatBoardEntry& a, const StatBoardEntry& b)
        {
            switch (m_sharedData.scoreType)
            {
            default:
            case ScoreType::Elimination:
            case ScoreType::Stroke:
            case ScoreType::ShortRound:
            case ScoreType::MultiTarget:
                if (a.score == b.score)
                {
                    //hmm this unfairly uses the team ID
                    //to tie-break, but it's necessary to
                    //keep team members together on the board
                    return a.team < b.team;
                }
                return a.score < b.score;
            case ScoreType::Stableford:
            case ScoreType::StablefordPro:
            case ScoreType::Match:
            case ScoreType::Skins:
                if (a.score == b.score)
                {
                    return a.team < b.team;
                }
                return b.score < a.score;
            case ScoreType::NearestThePin:
                if (m_ntpPro)
                {
                    if (a.score == b.score)
                    {
                        return a.distance < b.distance;
                    }
                    return a.score > b.score;
                }
                return a.distance < b.distance;
            }
        });
    //LOG("Table Update", cro::Logger::Type::Info);

    auto& ents = m_scoreboardEnt.getComponent<cro::Callback>().getUserData<std::vector<cro::Entity>>();
    std::sort(scores.begin(), scores.end(),
        [&](const ScoreEntry& a, const ScoreEntry& b)
        {
            switch (m_sharedData.scoreType)
            {
            default:
            case ScoreType::Elimination:
                if (a.total == b.total)
                {
                    return a.lives > b.lives;
                }
                [[fallthrough]];
            case ScoreType::Stroke:
            case ScoreType::ShortRound:
            case ScoreType::MultiTarget:
                if (a.total == b.total)
                {
                    return a.teamIndex < b.teamIndex;
                }
                return a.total < b.total;
            case ScoreType::Stableford:
            case ScoreType::StablefordPro:
            case ScoreType::Skins:
            case ScoreType::Match:
                if (a.total == b.total)
                {
                    return a.teamIndex < b.teamIndex;
                }
                return b.total < a.total;
            case ScoreType::NearestThePin:
                if (m_ntpPro)
                {
                    if (a.total == b.total)
                    {
                        return a.totalDistance < b.totalDistance;
                    }
                    return a.total > b.total;
                }
                return a.totalDistance < b.totalDistance;
            }
        });
    m_achievementTracker.leadingCareerRound = !scores[0].leaguePlayer;

    if (m_nemesisEnt.isValid())
    {
        for (auto i = 0u; i < scores.size(); ++i)
        {
            if (scores[i].nameIndex == Career::instance(m_sharedData).getLeagueTables()[m_sharedData.leagueRoundID - LeagueRoundID::RoundOne].getNemesis())
            {
                m_nemesisEnt.getComponent<cro::Callback>().getUserData<std::int32_t>() = static_cast<std::int32_t>(i);
                break;
            }
        }
    }

    const auto formatDistance = [](float d)
        {
            d *= 100.f;
            d = std::round(d);
            d /= 100.f;

            std::stringstream ss;
            ss.precision(2);
            ss << std::fixed << d;
            return ss.str();
        };

    std::size_t page2 = 0;
    static constexpr std::size_t MaxCols = 9;
    if (holeCount > /*m_scoreColumnCount*/MaxCols)
    {
        page2 = std::min(MaxCols, holeCount - /*m_scoreColumnCount*/MaxCols);
    }

    //store the strings to update the leaderboard texture
    std::vector<LeaderboardEntry> leaderboardEntries;

    //name column
    cro::String nameString = "HOLE\n";
    switch (m_sharedData.scoreType)
    {
    default:
        nameString += "PAR";
        break;
    case ScoreType::Stableford:
    case ScoreType::StablefordPro:
    case ScoreType::NearestThePin:
        nameString += " ";
        break;
    }

    for (auto i = 0u; i < playerCount; ++i)
    {
        nameString += "\n  ";

        std::int32_t offset = 0;
        if (Team::Player(scores[i].client, scores[i].player) == m_snekPlayer)
        {
            offset = 1;
            nameString += cro::String(std::uint32_t(0x1F40D));
        }

        nameString += scores[i].name.substr(0, ConstVal::MaxStringChars - offset);
        m_netStrengthIcons[i].getComponent<cro::Callback>().setUserData<NetStatData>(scores[i].client, scores[i].player, scores[i].leaguePlayer);
    }
    if (page2)
    {
        //pad out for page 2
        for (auto i = 0u; i < 16u - playerCount; ++i)
        {
            nameString += "\n";
        }

        nameString += "\n\nHOLE\n";
        switch (m_sharedData.scoreType)
        {
        default:
            nameString += "PAR";
            break;
        case ScoreType::Stableford:
        case ScoreType::StablefordPro:
        case ScoreType::NearestThePin:
            nameString += " ";
            break;
        }

        for (auto i = 0u; i < playerCount; ++i)
        {
            //nameString += "\n  " + scores[i].name.substr(0, ConstVal::MaxStringChars);
            nameString += "\n  ";

            std::int32_t offset = 0;
            if (Team::Player(scores[i].client, scores[i].player) == m_snekPlayer)
            {
                offset = 1;
                nameString += cro::String(std::uint32_t(0x1F40D));
            }

            nameString += scores[i].name.substr(0, ConstVal::MaxStringChars - offset);
        }
    }
    ents[0].getComponent<cro::Text>().setString(nameString);
    leaderboardEntries.emplace_back(ents[0].getComponent<cro::Transform>().getPosition() + glm::vec3(2.f, 0.f, 0.f), nameString);

    //score columns
    for (auto i = 1u; i < ents.size() - 1; ++i)
    {
        auto holeNumber = i;
        /*if (m_sharedData.holeCount == 2)
        {
            holeNumber += 9;
        }*/

        std::vector<std::pair<cro::Colour, std::uint32_t>> stringColours;

        std::string scoreString = std::to_string(holeNumber) + "\n";
        switch (m_sharedData.scoreType)
        {
        default:
            scoreString += std::to_string(m_holeData[i - 1].par);
            break;
        case ScoreType::Stableford:
        case ScoreType::StablefordPro:
        case ScoreType::NearestThePin:
            scoreString += " ";
            break;
        }

        
        for (auto j = 0u; j < playerCount; ++j)
        {
            scoreString += "\n";
            
            auto holeIndex = i - 1;
            auto s = scores[j].holes[holeIndex];
            switch (m_sharedData.scoreType)
            {
            default:
            if (s)
            {
                auto c = s == m_holeData[holeIndex].par ? LeaderboardTextDark : CD32::Colours[CD32::GreenMid];
                if (s > m_holeData[holeIndex].par)
                {
                    c = CD32::Colours[CD32::Red];
                }

                stringColours.emplace_back(c, static_cast<std::uint32_t>(scoreString.size()));
                scoreString += std::to_string(s);
            }            
            break;
            case ScoreType::Stableford:
            case ScoreType::StablefordPro:
                if (scores[j].holeComplete[holeIndex])
                {
                    auto c = LeaderboardTextDark;
                    if(s < 1)
                    {
                        c = CD32::Colours[CD32::Red];
                    }
                    stringColours.emplace_back(c, static_cast<std::uint32_t>(scoreString.size()));
                    scoreString += std::to_string(s);
                }
                break;
            case ScoreType::NearestThePin:
                if (s > MaxNTPStrokes)
                {
                    stringColours.emplace_back(CD32::Colours[CD32::Red], static_cast<std::uint32_t>(scoreString.size()));
                    scoreString += "F";
                }
                else
                {
                    if (s == 1 &&
                        holeIndex != m_currentHole)
                    {
                        //this is used to mark a hole winner
                        stringColours.emplace_back(CD32::Colours[CD32::GreenMid], static_cast<std::uint32_t>(scoreString.size()));
                        scoreString += "W";
                    }
                    else if (s != 0)
                    {
                        stringColours.emplace_back(LeaderboardTextDark, static_cast<std::uint32_t>(scoreString.size()));
                        scoreString += "-";
                    }
                }
                break;
            case ScoreType::Elimination:
                if (s == 12)
                {
                    stringColours.emplace_back(CD32::Colours[CD32::Red], static_cast<std::uint32_t>(scoreString.size()));
                    scoreString += "X";
                }
                else
                {
                    stringColours.emplace_back(LeaderboardTextDark, static_cast<std::uint32_t>(scoreString.size()));
                    if (s != 0)
                    {
                        scoreString += std::to_string(s);
                    }
                }
                break;
            }
        }

        if (page2)
        {
            stringColours.emplace_back(LeaderboardTextDark, static_cast<std::uint32_t>(scoreString.size()));

            for (auto j = 0u; j < 16 - playerCount; ++j)
            {
                scoreString += "\n";
            }

            auto holeIndex = (i + MaxCols) - 1;
            if (holeIndex < m_holeData.size())
            {
                scoreString += "\n\n" + std::to_string(i + MaxCols) + "\n";

                switch (m_sharedData.scoreType)
                {
                default:
                    scoreString += std::to_string(m_holeData[holeIndex].par);
                    break;
                case ScoreType::Stableford:
                case ScoreType::StablefordPro:
                case ScoreType::NearestThePin:
                    scoreString += " ";
                    break;
                }

                for (auto j = 0u; j < playerCount; ++j)
                {
                    scoreString += "\n";

                    auto s = scores[j].holes[holeIndex];

                    switch (m_sharedData.scoreType)
                    {
                    default:
                        if (s)
                        {
                            auto c = s == m_holeData[holeIndex].par ? LeaderboardTextDark : CD32::Colours[CD32::GreenMid];
                            if (s > m_holeData[holeIndex].par)
                            {
                                c = CD32::Colours[CD32::Red];
                            }

                            stringColours.emplace_back(c, static_cast<std::uint32_t>(scoreString.size()));
                            scoreString += std::to_string(s);
                        }
                        break;
                    case ScoreType::Stableford:
                    case ScoreType::StablefordPro:
                        if (scores[j].holeComplete[holeIndex])
                        {
                            if (s > 0)
                            {
                                stringColours.emplace_back(LeaderboardTextDark, static_cast<std::uint32_t>(scoreString.size()));
                            }
                            else
                            {
                                stringColours.emplace_back(CD32::Colours[CD32::Red], static_cast<std::uint32_t>(scoreString.size()));
                            }
                            scoreString += std::to_string(s);
                        }
                        break;
                    case ScoreType::NearestThePin:
                        if (s > MaxNTPStrokes)
                        {
                            stringColours.emplace_back(CD32::Colours[CD32::Red], static_cast<std::uint32_t>(scoreString.size()));
                            scoreString += "F";
                        }
                        else
                        {
                            if (s == 1
                                && holeIndex != m_currentHole)
                            {
                                //this is used to mark a hole winner
                                stringColours.emplace_back(CD32::Colours[CD32::GreenMid], static_cast<std::uint32_t>(scoreString.size()));
                                scoreString += "W";
                            }
                            else if (s != 0)
                            {
                                stringColours.emplace_back(LeaderboardTextDark, static_cast<std::uint32_t>(scoreString.size()));
                                scoreString += "-";
                            }
                        }
                        break;
                    case ScoreType::Elimination:
                        if (s == 12)
                        {
                            stringColours.emplace_back(CD32::Colours[CD32::Red], static_cast<std::uint32_t>(scoreString.size()));
                            scoreString += "X";
                        }
                        else
                        {
                            stringColours.emplace_back(LeaderboardTextDark, static_cast<std::uint32_t>(scoreString.size()));
                            if (s != 0)
                            {
                                scoreString += std::to_string(s);
                            }
                        }
                        break;
                    }
                }
            }
        }

        ents[i].getComponent<cro::Text>().setString(scoreString);
        for (const auto& [colour, idx] : stringColours)
        {
            ents[i].getComponent<cro::Text>().setFillColour(colour, idx);
        }

        leaderboardEntries.emplace_back(glm::vec3(ents[i].getComponent<UIElement>().absolutePosition - glm::vec2(ColumnMargin, -UITextPosV), 0.f), scoreString);
    }

    //total column
    std::int32_t par = 0;
    for (auto i = 0u; i < MaxCols && i < m_holeData.size(); ++i)
    {
        par += m_holeData[i].par;
    }

    std::string totalString = "TOTAL\n";
    switch (m_sharedData.scoreType)
    {
    default:
        totalString += std::to_string(par);
        break;
    case ScoreType::Stableford:
    case ScoreType::StablefordPro:
        if (page2)
        {
            totalString += "F9";
        }
        else
        {
            totalString += " ";
        }
        break;
    case ScoreType::NearestThePin:
        totalString += " ";
        break;
    }

    std::int32_t padCount = 0;
    for (auto i = 0u; i < playerCount; ++i)
    {
        padCount = 0;
        if (m_sharedData.scoreType == ScoreType::NearestThePin)
        {
            auto distScore = scores[i].frontNineDistance;
            if (m_sharedData.imperialMeasurements)
            {
                distScore *= ToYards;
            }
            if (distScore < 100)
            {
                padCount++;
                if (distScore < 10)
                {
                    padCount++;
                }
            }
            totalString += "\n" + formatDistance(distScore);
        }
        else
        {
            totalString += "\n" + std::to_string(scores[i].frontNine);
        }

        switch (m_sharedData.scoreType)
        {
        default:
        case ScoreType::Elimination:
        case ScoreType::MultiTarget:
        case ScoreType::ShortRound:
        case ScoreType::Stroke:
        {
            std::size_t strLen = scores[i].frontNine < 10 ? 0 : 
                scores[i].frontNine < 100 ? 1 : 2;
            if (scores[i].parDiff > 0)
            {
                const cro::String str = " (+" + std::to_string(scores[i].parDiff) + ")";
                strLen += str.size();
                totalString += str;
            }
            else if (scores[i].parDiff < 0)
            {
                const cro::String str = " (" + std::to_string(scores[i].parDiff) + ")";
                strLen += str.size();
                totalString += str;
            }
            else
            {
                const cro::String str = " (0)";
                strLen = str.size();
                totalString += str;
            }

            if (m_sharedData.scoreType == ScoreType::Elimination)
            {
                cro::String str;
                for (auto j = 0u; j < 10 - strLen; ++j)
                {
                    str += " ";
                }
                if (scores[i].lives == 1)
                {
                    str += "1 Life";
                }
                else
                {
                    str += std::to_string(scores[i].lives) + " Lives";
                }
                totalString += str;
            }
        }
            break;
        case ScoreType::Stableford:
        case ScoreType::StablefordPro:
        case ScoreType::Match:
            if (scores[i].frontNine == 1)
            {
                totalString += " POINT";
            }
            else
            {
                totalString += " POINTS";
            }
            break;
        case ScoreType::Skins:
            if (scores[i].frontNine == 1)
            {
                totalString += " SKIN";
            }
            else
            {
                totalString += " SKINS";
            }
            break;
        case ScoreType::NearestThePin:
            if (m_ntpPro)
            {
                if (m_sharedData.imperialMeasurements)
                {
                    totalString += "yd";
                }
                else
                {
                    totalString += "m";
                }

                for(auto j = 0; j < padCount; ++j)
                {
                    totalString += " ";
                }
                totalString += " - " + std::to_string(scores[i].frontNine) + " Point";

                if (scores[i].frontNine != 1)
                {
                    totalString += "s";
                }
            }
            else
            {
                if (m_sharedData.imperialMeasurements)
                {
                    totalString += " YARDS";
                }
                else
                {
                    totalString += " METRES";
                }
            }
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

        totalString += "\n\nTOTAL\n";
        
        switch (m_sharedData.scoreType)
        {
        default:
            totalString += std::to_string(par) + separator + std::to_string(par + frontPar);
            break;
        case ScoreType::Stableford:
        case ScoreType::StablefordPro:
            totalString += "B9 - FINAL";
            break;
        case ScoreType::NearestThePin:

            break;
        }
        
        for (auto i = 0u; i < playerCount; ++i)
        {
            if (m_sharedData.scoreType == ScoreType::NearestThePin)
            {
                padCount = 0;

                float distScore = scores[i].frontNineDistance + scores[i].backNineDistance;
                if (m_sharedData.imperialMeasurements)
                {
                    distScore *= ToYards;
                }
                if (distScore < 100)
                {
                    padCount++;
                    if (distScore < 10)
                    {
                        padCount++;
                    }
                }
                totalString += "\n" + formatDistance(distScore);
            }
            else
            {
                separator = getSeparator(scores[i].backNine);
                totalString += "\n" + std::to_string(scores[i].backNine);
            }
            switch (m_sharedData.scoreType)
            {
            default:
            case ScoreType::Elimination:
            case ScoreType::MultiTarget:
            case ScoreType::ShortRound:
            case ScoreType::Stroke:
            {
                std::size_t strLen = scores[i].backNine < 10 ? 0 :
                    scores[i].backNine < 100 ? 1 : 2;
                totalString += separator + std::to_string(scores[i].total);
                if (scores[i].parDiff > 0)
                {
                    const cro::String str = " (+" + std::to_string(scores[i].parDiff) + ")";
                    strLen += str.size();
                    totalString += str;
                }
                else if (scores[i].parDiff < 0)
                {
                    const cro::String str = " (" + std::to_string(scores[i].parDiff) + ")";
                    strLen += str.size();
                    totalString += str;
                }
                else
                {
                    const cro::String str = " (0)";
                    strLen += str.size();
                    totalString += str;
                }

                if (m_sharedData.scoreType == ScoreType::Elimination)
                {
                    cro::String str;
                    for (auto j = 0u; j < 8 - strLen; ++j)
                    {
                        str += " ";
                    }
                    if (scores[i].lives == 1)
                    {
                        str += "1 Life";
                    }
                    else
                    {
                        str += std::to_string(scores[i].lives) + " Lives";
                    }
                    totalString += str;
                }
            }
                break;
            case ScoreType::Stableford:
            case ScoreType::StablefordPro:
                //align with back/total string
                if (scores[i].backNine < 10 && scores[i].backNine > -1)
                {
                    totalString += " ";
                }
                totalString += " - " + std::to_string(scores[i].total);
                if (scores[i].total == 1)
                {
                    totalString += " POINT";
                }
                else
                {
                    totalString += " POINTS";
                }
                break;
            case ScoreType::Match:
                if (scores[i].backNine == 1)
                {
                    totalString += " POINT";
                }
                else
                {
                    totalString += " POINTS";
                }
                break;
            case ScoreType::Skins:
                if (scores[i].backNine == 1)
                {
                    totalString += " SKIN";
                }
                else
                {
                    totalString += " SKINS";
                }
                break;
            case ScoreType::NearestThePin:
                if (m_ntpPro)
                {
                    if (m_sharedData.imperialMeasurements)
                    {
                        totalString += "yd";
                    }
                    else
                    {
                        totalString += "m";
                    }
                    for (auto j = 0; j < padCount; ++j)
                    {
                        totalString += " ";
                    }
                    //font nine and back nine both contain the total so don't sum them
                    totalString += " - " + std::to_string(/*scores[i].frontNine +*/ scores[i].backNine) + " Point";

                    if ((scores[i].frontNine + scores[i].backNine) != 1)
                    {
                        totalString += "s";
                    }
                }
                else
                {
                    if (m_sharedData.imperialMeasurements)
                    {
                        totalString += " YARDS";
                    }
                    else
                    {
                        totalString += " METRES";
                    }
                }
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

void GolfState::logCSV() const
{
    //if (m_roundEnded)
    {
        auto courseName = m_courseTitle.toAnsiString();
        std::replace(courseName.begin(), courseName.end(), ' ', '_');
        std::replace(courseName.begin(), courseName.end(), ',', '_');
        if (m_sharedData.holeCount == 1)
        {
            courseName += "_front_nine";
        }
        else if (m_sharedData.holeCount == 2)
        {
            courseName += "_back_nine";
        }

        std::string fileName = cro::SysTime::dateString() + "-" + cro::SysTime::timeString();
        std::replace(fileName.begin(), fileName.end(), '/', '-');
        std::replace(fileName.begin(), fileName.end(), ':', '-');
        fileName += "_" + courseName + ".csv";

        fileName = Content::getBaseContentPath() + fileName;

        cro::RaiiRWops out;
        out.file = SDL_RWFromFile(fileName.c_str(), "w");
        if (out.file)
        {
            std::stringstream ss;
            ss << "name,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11,h12,h13,h14,h15,h16,h17,h18,total\r\n";

            std::int32_t totalPar = 0;
            ss << "par,";
            for (const auto& hole : m_holeData)
            {
                ss << hole.par << ",";
                totalPar += hole.par;
            }
            if (m_holeData.size() < 18u)
            {
                for (auto i = m_holeData.size(); i < 18u; ++i)
                {
                    ss << "0,";
                }
            }
            ss << totalPar << "\r\n";

            for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
            {
                for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
                {
                    const auto scoreCount = m_sharedData.connectionData[i].playerData[j].holeScores.size();

                    const auto nameBytes = m_sharedData.connectionData[i].playerData[j].name.toUtf8();
                    const std::string nameString(nameBytes.begin(), nameBytes.end());
                    
                    ss << nameString << ",";
                    std::int32_t total = 0;
                    for (auto score : m_sharedData.connectionData[i].playerData[j].holeScores)
                    {
                        ss << (int)score << ",";
                        total += score;
                    }
                    if (scoreCount < 18)
                    {
                        for (auto k = scoreCount; k < 18u; ++k)
                        {
                            ss << "0,";
                        }
                    }
                    ss << total << "\r\n";
                }
            }
            
            auto str = ss.str();
            SDL_RWwrite(out.file, str.data(), str.size(), 1);
            LogI << "Saved " << fileName << std::endl;
        }
        else
        {
            LogE << "failed to open CSV file for writing: " << fileName << std::endl;
        }
    }
    /*else
    {
        LogI << "Can't save CSV - Round not ended." << std::endl;
    }*/
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

    m_cpuGolfer.suspend(visible);

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
        const float knots = direction.y * KnotsPerMetre; //use this anyway to set the colour :3
        const float speed = direction.y * 2.f; //this is an arbitrary display anyway...

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
        bounds.width = std::round(bounds.width / 2.f);
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

    //cmd.targetFlags = CommandID::Flag;
    //cmd.action = [direction](cro::Entity e, float)
    //    {
    //        e.getComponent<cro::Skeleton>().setPlaybackRate(std::max(1.f, direction.y * 2.f));
    //    };
    //m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

float GolfState::estimatePuttPower()
{
    auto target = m_holeData[m_currentHole].pin;
    auto targetDist = m_distanceToHole;
        
    auto maxDist = Clubs[ClubID::Putter].getTarget(targetDist);
    float guestimation = (targetDist / maxDist);

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
    if (targetDist > 0.005f)
    {
        slope = glm::dot(cro::Transform::Y_AXIS, target - m_currentPlayer.position) / targetDist;
    }
    return std::clamp(guestimation + (0.25f * slope), 0.f, 1.f);
}

void GolfState::calcTerrainLevel()
{
    const auto terrain = m_collisionMesh.getTerrain(m_currentPlayer.position);
    if (terrain.terrain != TerrainID::Green
        && m_currentPlayer.position != m_holeData[m_currentHole].tee)
    {
        const auto fwd = glm::normalize(m_holeData[m_currentHole].pin - m_currentPlayer.position);
        const auto up = terrain.normal;
        m_terrainLevel = glm::inverse(m_cameras[CameraID::Player].getComponent<cro::Transform>().getWorldRotation()) * glm::cross(fwd, up);
    }
    else
    {
        m_terrainLevel = { 1.f, 0.f, 0.f };
    }
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
    textEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    textEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });

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
    case MessageBoardID::EliminatedStroke:
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

        //if this is forfeit we won't have received the score yet either
        if (m_sharedData.scoreType == ScoreType::MultiTarget
            && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].targetHit)
        {
            //this is a forfeit
            score = m_holeData[m_currentHole].puttFromTee ? 6 : 12;
        }


        //if this is a HIO we want to track birdie/eagle/albatross too
        std::int32_t statScore = 0;

        if (score > 1)
        {
            score -= m_holeData[m_currentHole].par;
            score += ScoreID::ScoreOffset;
            statScore = score;
        }
        else
        {
            statScore = /*score*/1 - m_holeData[m_currentHole].par;
            statScore += ScoreID::ScoreOffset;

            //hio is also technically an eagle or birdie
            //etc, so we need to differentiate
            score = ScoreID::HIO;
            auto xp = xpValues[XPID::HIO];
            if (m_holeData[m_currentHole].puttFromTee)
            {
                //xp /= 5;
                xp = 200;
            }

            if (m_sharedData.showPuttingPower)
            {
                xp /= 2;
            }
#ifndef CRO_DEBUG_
            Social::awardXP(xp, XPStringID::HIO);
#endif
        }
        
        //used to calculate the amount of XP based on player settings
        std::int32_t divisor = (m_sharedData.showPuttingPower && getClub() == ClubID::Putter) ? 2 : 1;
        std::int32_t offset = m_sharedData.showPuttingPower ? 1 : 0;

        //if this is a local player update achievements
        if (!m_groupIdle &&
            m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
        {
            if (m_achievementTracker.hadFoul && overPar < 1
                && m_sharedData.scoreType != ScoreType::Elimination)
            {
                Achievements::awardAchievement(AchievementStrings[AchievementID::Boomerang]);
                Social::getMonthlyChallenge().updateChallenge(ChallengeID::Five, 0);
            }

            switch (statScore)
            {
            default: break;
            case ScoreID::Birdie:
                Achievements::incrementStat(StatStrings[StatID::Birdies]);
                if (!m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
                {
                    m_achievementTracker.birdies++;

                    if (m_sharedData.holeCount != 2
                        && m_currentHole < 9)
                    {
                        m_achievementTracker.birdieChallenge++;
                        if (m_achievementTracker.birdieChallenge == 4
                            && m_courseIndex != -1)
                        {
                            Social::getMonthlyChallenge().updateChallenge(ChallengeID::Nine, m_courseIndex);
                        }
                    }
                }
                break;
            case ScoreID::Eagle:
                Achievements::incrementStat(StatStrings[StatID::Eagles]);
                Achievements::awardAchievement(AchievementStrings[AchievementID::Soaring]);
                if (!m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
                {
                    m_achievementTracker.eagles++;

                    if (m_sharedData.holeCount == 2
                        || m_currentHole > 8)
                    {
                        if (m_achievementTracker.eagles == 1
                            && m_courseIndex != -1)
                        {
                            Social::getMonthlyChallenge().updateChallenge(ChallengeID::Ten, m_courseIndex);
                        }
                    }
                }
                break;
            case ScoreID::Albatross:
                Achievements::incrementStat(StatStrings[StatID::Albatrosses]);
                Achievements::awardAchievement(AchievementStrings[AchievementID::BigBird]);
                break;
            }

            if (score == ScoreID::HIO)
            {
                Achievements::incrementStat(StatStrings[StatID::HIOs]);
                Achievements::awardAchievement(AchievementStrings[AchievementID::HoleInOne]);

                if (m_sharedData.scoreType == ScoreType::MultiTarget)
                {
                    Achievements::awardAchievement(AchievementStrings[AchievementID::BeholdTheImpossible]);
                }

                //award supplimentary XP
                switch (statScore)
                {
                default: break;
                case ScoreID::Albatross:
                    Social::awardXP(xpValues[XPID::Albatross] / divisor, XPStringID::Albatross + offset);
                    break;
                case ScoreID::Eagle:
                    Social::awardXP(xpValues[XPID::Eagle] / divisor, XPStringID::Eagle + offset);
                    break;
                case ScoreID::Birdie:
                    Social::awardXP(xpValues[XPID::Birdie] / divisor, XPStringID::Birdie + offset);
                    break;
                //would we ever have a HIO which is also par? :)
                }
            }
        }

        if (messageType == MessageBoardID::HoleScore)
        {
            //this triggers the VO which we only want if it went in the hole.
            auto playerDir = m_holeData[m_currentHole].pin - m_currentPlayer.position;

            auto* msg = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::Scored;
            msg->score = static_cast<std::uint8_t>(score);
            msg->travelDistance = glm::length2(playerDir);
            msg->club = getClub();
            
            if (msg->club == ClubID::Putter
                && m_currentPlayer.terrain != TerrainID::Green)
            {
                //we scored from the fringe - so fudge the distance
                //causing the sound director to do a nice putt callout
                msg->travelDistance = 7.f * 7.f;
            }

            const float t = playerDir.x;
            playerDir.x = -playerDir.z;
            playerDir.z = t;
            playerDir *= 2.5f;
            
            msg->position = m_holeData[m_currentHole].pin + playerDir;

            if (score == ScoreID::HIO)
            {
                auto* msg2 = postMessage<GolfEvent>(MessageID::GolfMessage);
                msg2->type = GolfEvent::HoleInOne;
                msg2->position = m_holeData[m_currentHole].pin;

                if (getClub() != ClubID::Putter)
                {
                    Social::awardXP(5, XPStringID::GIR);
                    Social::awardXP(2, XPStringID::FIR);
                }
            }

            if (score <= ScoreID::Par
                && m_sharedData.showBeacon)
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(0.4f);
                entity.getComponent<cro::Callback>().function =
                    [&](cro::Entity e, float dt)
                    {
                        auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
                        ct -= dt;

                        if (ct < 0)
                        {
                            //display ring animation
                            cro::Command cmd;
                            cmd.targetFlags = CommandID::HoleRing;
                            cmd.action = [](cro::Entity e2, float)
                                {
                                    e2.getComponent<cro::Callback>().active = true;
                                };
                            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                            e.getComponent<cro::Callback>().active = false;
                            m_gameScene.destroyEntity(e);
                        }
                    };
            }
        }


        if (score < ScoreID::Count)
        {
            textEnt.getComponent<cro::Text>().setString(ScoreStrings[score]);
            textEnt.getComponent<cro::Transform>().move({ 0.f, -10.f, 0.f });


            auto addIcon = [&](std::int32_t left, std::int32_t right) mutable
            {
                auto e = m_uiScene.createEntity();
                e.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, 0.05f });
                e.addComponent<cro::Callback>().active = true;
                e.getComponent<cro::Callback>().function =
                    [&,entity](cro::Entity f, float)
                {
                    if (entity.destroyed())
                    {
                        f.getComponent<cro::Callback>().active = false;
                        m_uiScene.destroyEntity(f);
                    }
                };
                if (left == SpriteID::Hio)
                {
                    e.getComponent<cro::Transform>().move(glm::vec2(0.f, -4.f));
                }
                const auto leftBounds = m_sprites[left].getTextureBounds();
                const auto leftRect = m_sprites[left].getTextureRectNormalised();
                const auto rightRect = m_sprites[right].getTextureRectNormalised();
                const float Spacing = left == SpriteID::Hio ? 30.f : 50.f;
                const glm::vec2 leftPos(-Spacing - leftBounds.width, -std::floor(leftBounds.height / 2.f));
                const glm::vec2 rightPos(Spacing, -std::floor(leftBounds.height / 2.f));

                e.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
                e.getComponent<cro::Drawable2D>().setVertexData({
                    cro::Vertex2D(glm::vec2(leftPos.x, leftPos.y + leftBounds.height), glm::vec2(leftRect.left, leftRect.bottom + leftRect.height)),                                     //0-------2
                    cro::Vertex2D(leftPos, glm::vec2(leftRect.left, leftRect.bottom)),                                                                                                   //|
                    cro::Vertex2D(glm::vec2(leftPos.x + leftBounds.width, leftPos.y + leftBounds.height), glm::vec2(leftRect.left + leftRect.width, leftRect.bottom + leftRect.height)), //1

                    cro::Vertex2D(glm::vec2(leftPos.x + leftBounds.width, leftPos.y + leftBounds.height), glm::vec2(leftRect.left + leftRect.width, leftRect.bottom + leftRect.height)), //        3
                    cro::Vertex2D(leftPos, glm::vec2(leftRect.left, leftRect.bottom)),                                                                                                   //        |
                    cro::Vertex2D(glm::vec2(leftPos.x + leftBounds.width, leftPos.y), glm::vec2(leftRect.left + leftRect.width, leftRect.bottom)),                                       //4-------5

                    
                    cro::Vertex2D(glm::vec2(rightPos.x, rightPos.y + leftBounds.height), glm::vec2(rightRect.left, rightRect.bottom + rightRect.height)),
                    cro::Vertex2D(rightPos, glm::vec2(rightRect.left, rightRect.bottom)),
                    cro::Vertex2D(glm::vec2(rightPos.x + leftBounds.width, rightPos.y + leftBounds.height), glm::vec2(rightRect.left + rightRect.width, rightRect.bottom + rightRect.height)),

                    cro::Vertex2D(glm::vec2(rightPos.x + leftBounds.width, rightPos.y + leftBounds.height), glm::vec2(rightRect.left + rightRect.width, rightRect.bottom + rightRect.height)),
                    cro::Vertex2D(rightPos, glm::vec2(rightRect.left, rightRect.bottom)),
                    cro::Vertex2D(glm::vec2(rightPos.x + leftBounds.width, rightPos.y), glm::vec2(rightRect.left + rightRect.width, rightRect.bottom))                    
                    });
                e.getComponent<cro::Drawable2D>().setTexture(m_sprites[left].getTexture());
                entity.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
            };


            switch (score)
            {
            case ScoreID::Albatross:
                Social::awardXP(xpValues[XPID::Albatross] / divisor, XPStringID::Albatross + offset);
                addIcon(SpriteID::AlbatrossLeft, SpriteID::AlbatrossRight);
                Timeline::addEvent(Timeline::Event::Albatross, m_strokeTimer);
                break;
            case ScoreID::Eagle:
                Social::awardXP(xpValues[XPID::Eagle] / divisor, XPStringID::Eagle + offset);
                addIcon(SpriteID::EagleLeft, SpriteID::EagleRight);
                textEnt.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
                textEnt.getComponent<cro::Text>().setFillColour(TextGoldColour);
                textEnt.getComponent<cro::Transform>().move({ 0.f, 2.f, 0.f });
                Timeline::addEvent(Timeline::Event::Eagle, m_strokeTimer);
                break;
            case ScoreID::Birdie:
                Social::awardXP(xpValues[XPID::Birdie] / divisor, XPStringID::Birdie + offset);
                addIcon(SpriteID::BirdieLeft, SpriteID::BirdieRight);
                textEnt.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
                textEnt.getComponent<cro::Text>().setFillColour({0xadd9b7ff});
                textEnt.getComponent<cro::Transform>().move({ 0.f, 2.f, 0.f });
                Timeline::addEvent(Timeline::Event::Birdie, m_strokeTimer);
                break;
            case ScoreID::Par:
                Social::awardXP(xpValues[XPID::Par] / divisor, XPStringID::Par + offset);
                textEnt.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
                textEnt.getComponent<cro::Transform>().move({ 0.f, 2.f, 0.f });
                Timeline::addEvent(Timeline::Event::HoleOut, m_strokeTimer);
                break;
            case ScoreID::Bogie:
                textEnt.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
                [[fallthrough]];
            case ScoreID::DoubleBogie:
            case ScoreID::TripleBogie:
                textEnt.getComponent<cro::Text>().setFillColour(TextHighlightColour);
                Timeline::addEvent(Timeline::Event::HoleOut, m_strokeTimer);
                break;
            case ScoreID::HIO:
                addIcon(SpriteID::Hio, SpriteID::Hio);
                textEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
                textEnt.getComponent<cro::Text>().setVerticalSpacing(3.f);
                textEnt.getComponent<cro::Transform>().move({ 0.f, 4.f, 0.f });
                Timeline::addEvent(Timeline::Event::HIO, m_strokeTimer);
                break;
            default:
                break;
            }

            if (special
                && score != ScoreID::HIO)
            {
                textEnt3.getComponent<cro::Text>().setString("Nice Putt!");
                textEnt3.getComponent<cro::Text>().setFillColour(TextGoldColour);
                textEnt3.getComponent<cro::Transform>().move({ 0.f, -11.f });

                textEnt.getComponent<cro::Transform>().move({ 0.f, 3.f, 0.f });
            }

            //overwrite special text if this is a forfeit
            if (m_sharedData.scoreType == ScoreType::MultiTarget
                && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].targetHit)
            {
                textEnt3.getComponent<cro::Text>().setString("Target Not Hit");
                textEnt3.getComponent<cro::Text>().setFillColour(TextGoldColour);
                textEnt3.getComponent<cro::Transform>().move({ 0.f, -10.f });

                textEnt.getComponent<cro::Transform>().move({ 0.f, 2.f, 0.f });
            }

            imgEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::BounceAnim];
            imgEnt.addComponent<cro::SpriteAnimation>().play(0);
            imgEnt.getComponent<cro::Transform>().setPosition({ 86.f, 25.f, 0.1f });
            if (cro::Util::Random::value(0, 1) == 0)
            {
                imgEnt.getComponent<cro::Transform>().setScale({ -1.f, 1.f });
                imgEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            bounds = m_sprites[SpriteID::BounceAnim].getTextureBounds();
        }
        else
        {
            textEnt.getComponent<cro::Text>().setString("Bad Luck!");
            textEnt2.getComponent<cro::Text>().setString(std::to_string(overPar) + " Over Par");
            Timeline::addEvent(Timeline::Event::HoleOut, m_strokeTimer);
        }
    }
    break;
    case MessageBoardID::Bunker:
        textEnt.getComponent<cro::Text>().setString("Bunker!");
        imgEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::Bunker];
        bounds = m_sprites[SpriteID::Bunker].getTextureBounds();

        Timeline::addEvent(Timeline::Event::Bunker, m_strokeTimer);
        break;
    case MessageBoardID::PlayerName:
        textEnt.getComponent<cro::Text>().setString(m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].name.substr(0, 19));
        textEnt.getComponent<cro::Text>().setFillColour(TextGoldColour);
        textEnt2.getComponent<cro::Text>().setString("Stroke: " + std::to_string(m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole] + 1));
        if (m_sharedData.scoreType == ScoreType::Skins && m_suddenDeath)
        {
            textEnt3.getComponent<cro::Text>().setString("First To Hole Wins");
        }
        else
        {
            if (m_sharedData.scoreType == ScoreType::Elimination)
            {
                cro::String str = ScoreTypes[m_sharedData.scoreType] + ": ";

                auto lives = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].skinScore;

                str += std::to_string(lives);
                //str += std::uint32_t(0x2764); //red heart
                //str += std::uint32_t(0xfe0f);
                str += std::uint32_t(0x1F497); //growing heart
                textEnt3.getComponent<cro::Text>().setString(str);
            }
            else
            {
                auto s = ScoreTypes[m_sharedData.scoreType];
                if (m_ntpPro)
                {
                    s += "+";
                }
                textEnt3.getComponent<cro::Text>().setString(s);
            }
        }
        break;
    case MessageBoardID::Eliminated:
    case MessageBoardID::NTPForfeit:
    case MessageBoardID::NTPDistance:
        if (messageType == MessageBoardID::NTPDistance)
        {
            std::stringstream ss;
            ss.precision(2);
            if (m_sharedData.imperialMeasurements)
            {
                ss << std::fixed << m_NTPDistance * ToYards << "yds";
            }
            else
            {
                ss << std::fixed << m_NTPDistance << "m";
            }
            if (m_ntpPro)
            {
                textEnt.getComponent<cro::Text>().setString("Nearest The Pin+");
            }
            else
            {
                textEnt.getComponent<cro::Text>().setString("Nearest The Pin");
            }
            textEnt.getComponent<cro::Text>().setFillColour(TextGoldColour);
            textEnt3.getComponent<cro::Text>().setString("Distance: " + ss.str());
        }
        else
        {
            textEnt.getComponent<cro::Text>().setString(messageType == MessageBoardID::Eliminated ? "Eliminated!" : "Forfeit!");
            textEnt.getComponent<cro::Text>().setFillColour(TextGoldColour);
            textEnt3.getComponent<cro::Text>().setString("Bad Luck!");
        }
        imgEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::BounceAnim];
        imgEnt.addComponent<cro::SpriteAnimation>().play(0);
        imgEnt.getComponent<cro::Transform>().setPosition({ 86.f, 25.f, 0.1f });
        if (cro::Util::Random::value(0, 1) == 0)
        {
            imgEnt.getComponent<cro::Transform>().setScale({ -1.f, 1.f });
            imgEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
        }
        bounds = m_sprites[SpriteID::BounceAnim].getTextureBounds();
        Timeline::addEvent(Timeline::Event::Elimination);
        break;
    case MessageBoardID::Scrub:
    case MessageBoardID::Water:
        textEnt.getComponent<cro::Text>().setString("Foul!");
        textEnt.getComponent<cro::Text>().setFillColour(TextGoldColour);
        textEnt2.getComponent<cro::Text>().setString(m_sharedData.scoreType == ScoreType::NearestThePin ? "Hole Forfeit" : "1 Stroke Penalty");
        imgEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::Foul];
        bounds = m_sprites[SpriteID::Foul].getTextureBounds();

        Timeline::addEvent(Timeline::Event::Water, m_strokeTimer);
        break;
    }
    imgEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, 0.f });

    if (textEnt.getComponent<cro::Text>().getAlignment() != cro::Text::Alignment::Centre)
    {
        centreText(textEnt);
    }
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

    const float offsetScale = m_sharedData.useLargePowerBar ? 2.f : 1.f;

    glm::vec2 size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    glm::vec3 position((size.x / 2.f), (UIBarHeight + (14.f * offsetScale)) * m_viewScale.y, 0.8f);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.getComponent<cro::Transform>().setScale(m_viewScale);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(msg);
    //entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    //entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    centreText(entity);

    entity.addComponent<FloatingText>().basePos = position;
    entity.getComponent<FloatingText>().colour = TextNormalColour; //CD32::Colours[CD32::GreyLight];//TextGoldColour; // 
}

void GolfState::createTransition()
{
    const auto createWelcomeMessage =
        [&]()
        {
            if (!m_courseTitle.empty())
            {
                struct MessageData final
                {
                    float timer = 2.f;

                    std::int32_t state = 0;
                };

                auto entity = m_uiScene.createEntity();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<MessageData>();
                entity.getComponent<cro::Callback>().function =
                    [&](cro::Entity e, float dt)
                    {
                        static constexpr float DisplayTime = 4.f;
                        auto& [currTime, state] = e.getComponent<cro::Callback>().getUserData<MessageData>();
                        currTime -= dt;
                        if (currTime < 0)
                        {
                            currTime += DisplayTime;
                            switch (state)
                            {
                            default:
                            case 0:
                            {
                                cro::String s("Welcome to ");
                                s += m_courseTitle;
                                s += "...";
                                m_textChat.printToScreen(s, TextGoldColour);
                                postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::ChatMessage;
                            }
                            break;
                            case 1:
                            {
                                cro::String s("You're playing ");
                                s += ScoreTypes[m_ntpPro ? ScoreType::NearestThePinPro : m_sharedData.scoreType] + " on ";

                                switch (m_sharedData.holeCount)
                                {
                                default:
                                case 0:
                                    s += "all " + std::to_string(m_holeData.size()) + " holes.";
                                    break;
                                case 1:
                                    s += "the front " + std::to_string(m_holeData.size()) + ".";
                                    break;
                                case 2:
                                    s += "the back " + std::to_string(m_holeData.size()) + ".";
                                    break;
                                }
                                m_textChat.printToScreen(s, TextGoldColour);
                                postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::ChatMessage;
                            }
                            break;
                            case 2:
                            {
                                cro::String s("Happy Golfing! ");
                                //fudge because the golfer faces the other way on windows
#ifdef __linux__
                                s += std::uint32_t(0x26F3);
                                s += std::uint32_t(0x1F3CC);
#else
                                s += std::uint32_t(0x1F3CC);
                                s += std::uint32_t(0x26F3);
#endif
                                m_textChat.printToScreen(s, TextGoldColour);
                                postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::ChatMessage;


                                e.getComponent<cro::Callback>().active = false;
                                m_uiScene.destroyEntity(e);
                            }
                            break;
                            }
                            state++;
                        }
                    };
            }
        };


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
        [&, shaderID, timeID, createWelcomeMessage](cro::Entity e, float dt)
    {
        static constexpr float MaxTime = 2.f - (1.f/60.f);
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(MaxTime, currTime + dt);

        glCheck(glUseProgram(shaderID));
        glCheck(glUniform1f(timeID, currTime));

        if (currTime == MaxTime)
        {
            if (m_sharedData.gameMode != GameMode::Tutorial)
            {
                createWelcomeMessage();
            }
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
            const auto name = m_sharedData.connectionData[data[0]].playerData[0].name;
            const auto achievement = AchievementLabels[data[1]];

            //showNotification(name + " achieved " + achievement);
            m_textChat.printToScreen(name + " achieved " + achievement, CD32::Colours[CD32::BlueLight]);

            auto* msg = postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
            msg->type = Social::SocialEvent::PlayerAchievement;
            msg->level = 0; //cheer
        }
    }
}

void GolfState::showNotification(const cro::String& msg)
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f * m_viewScale.x, (UIBarHeight - 3.f) * m_viewScale.y * 2.f });
    entity.getComponent<cro::Transform>().setScale(m_viewScale);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::UI));
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize/* * static_cast<std::uint32_t>(m_viewScale.y)*/);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
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
    //showNotification(msg);
    m_textChat.printToScreen(msg, CD32::Colours[CD32::BlueLight]);
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
                        auto idx = m_humanCount == 1 ? cro::GameController::controllerID(m_inputParser.getLastActiveController()) : activeControllerID(m_currentPlayer.player);

                        //set correct button icon
                        if (hasPSLayout(idx))
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
                || cro::GameController::isButtonPressed(activeControllerID(m_currentPlayer.player), m_sharedData.inputBinding.buttons[InputBinding::Action])
                || (m_humanCount == 1 && m_buttonStates.buttonA)) //TODO this breaks if we ever get around to reassigning controller buttons
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

void GolfState::catAuth()
{
    cro::SpriteSheet sheet;
    if (sheet.loadFromFile("assets/golf/sprites/rockit.spt", m_resources.textures))
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -380.f, 120.f, 0.1f });
        entity.getComponent<cro::Transform>().setScale(m_viewScale);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = sheet.getSprite("rockit");
        entity.addComponent<cro::SpriteAnimation>().play(0);
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<std::pair<std::int32_t, float>>(0, 2.f);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
            {
                const glm::vec2 Speed = glm::vec2(420.f * m_viewScale.x, 0.f);
                auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>();
                switch (state)
                {
                case 0:
                    e.getComponent<cro::Transform>().move(Speed * dt);
                    if (e.getComponent<cro::Transform>().getPosition().x > (static_cast<float>(cro::App::getWindow().getSize().x)/* / m_viewScale.x*/) * 2.f)
                    {
                        state = 1;
                        e.getComponent<cro::Transform>().setScale({ -m_viewScale.x / 2.f, m_viewScale.y / 2.f });
                        e.getComponent<cro::Transform>().move({ 0.f, 120.f * m_viewScale.y });
                        e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                    }
                    break;
                case 1:
                    currTime -= dt;
                    if (currTime < 0)
                    {
                        state = 2;
                    }
                    break;
                case 2:
                    e.getComponent<cro::Transform>().move(-Speed * dt);
                    if (e.getComponent<cro::Transform>().getPosition().x < 0)
                    {
                        e.getComponent<cro::Callback>().active = false;
                        m_uiScene.destroyEntity(e);
                    }
                    break;
                default:
                    m_uiScene.destroyEntity(e);
                    break;
                }
            };
    }
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
        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, ratio, 0.01f, 20.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto& cam = m_trophyScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = updateCam;
    //cam.isStatic = true;
    cam.active = false; //we'll activate this once the scene is shown
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
    emitterSettings.blendmode = cro::EmitterSettings::BlendMode::Add;
    //emitterSettings.useRandomColour = true;

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

            const auto avCount = m_sharedData.teamMode ? 2 : 1;

            //icon
            const glm::vec3 iconPos = { bounds.width / 2.f, bounds.height - 8.f, 0.1f };
            for (auto j = 0; j < avCount; ++j)
            {
                entity = m_uiScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition(iconPos);
                entity.addComponent<cro::Drawable2D>();
                entity.addComponent<cro::Sprite>(m_sharedData.nameTextures[0].getTexture());
                bounds = { 0.f, LabelTextureSize.y - (LabelIconSize.y * 4.f), LabelIconSize.x, LabelIconSize.y };
                entity.getComponent<cro::Sprite>().setTextureRect(/*bounds*/{});
                entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, -14.f, -0.1f });
                m_trophies[i].label.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                m_trophies[i].avatars[j] = entity;

                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(1.f);
                entity.getComponent<cro::Callback>().function =
                    [&, i](cro::Entity e, float dt)
                    {
                        if (m_trophies[i].label.getComponent<cro::Callback>().active)
                        {
                            //don't do this, texture is set when the round end is displayed
                            //e.getComponent<cro::Sprite>().setTexture(*m_trophies[i].label.getComponent<cro::Sprite>().getTexture(), false);

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

            //fireworks for end of career round
            struct FireWorkData final
            {
                float currentTime = 0.f;
                std::int32_t count = 8;
                explicit FireWorkData(float t) : currentTime(t) {};
            };
            entity = m_trophyScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(position);
            entity.addComponent<cro::ParticleEmitter>().settings = emitterSettings;
            entity.addComponent<cro::AudioEmitter>() = as.getEmitter("firework");
            entity.getComponent<cro::AudioEmitter>().setPitch(static_cast<float>(cro::Util::Random::value(8, 11)) / 10.f);
            entity.getComponent<cro::AudioEmitter>().setLooped(false);
            entity.addComponent<cro::Callback>().setUserData<FireWorkData>(0.5f + (0.5f * i));
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
                {
                    auto& [currTime, count] = e.getComponent<cro::Callback>().getUserData<FireWorkData>();
                    currTime -= dt;

                    if (currTime < 0)
                    {
                        e.getComponent<cro::ParticleEmitter>().start();
                        
                        const float r = static_cast<float>(cro::Util::Random::value(-6,6)) / 10.f;
                        e.getComponent<cro::AudioEmitter>().setPitch(1.f + r);

                        if (e.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Playing)
                        {
                            e.getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(0.f));
                        }
                        else
                        {
                            e.getComponent<cro::AudioEmitter>().play();
                        }
                        currTime += 1.f;
                        currTime += static_cast<float>(cro::Util::Random::value(-5, 5)) / 10.f;

                        count--;
                        if (count == 0)
                        {
                            e.getComponent<cro::Callback>().active = false;
                        }
                    }
                };
            m_fireworks[i] = entity;
        }
        ++i;
    }

    //we have to make sure everything is processed at least once
    //as a hacky way of making sure the double sided property is
    //applied to the badge materials.
    m_trophyScene.simulate(0.f);
}

void GolfState::updateMinimapTexture()
{
    //TODO assert if we need to do this every pass
    if (m_sharedData.scoreType == ScoreType::MultiTarget)
    {
        auto* shader = &m_resources.shaders.get(ShaderID::MinimapModel);
        m_targetShader.shaderID = shader->getGLHandle();
        m_targetShader.vpUniform = shader->getUniformID("u_targetViewProjectionMatrix");

        m_targetShader.size = 5.f; //we don't actually know what size has been chosen, so this is a rough average
        if (m_holeData[m_currentHole].puttFromTee)
        {
            m_targetShader.size *= 0.032f;
        }
        m_targetShader.position = m_holeData[m_currentHole].target;
        m_targetShader.update();
    }

    //16 pass for 4x4 smaller renders
    /*glm::vec2 viewSize(MapSize);
    auto& miniCam = m_mapCam.getComponent<cro::Camera>();
    miniCam.setOrthographic(-viewSize.x / 8.f, viewSize.x / 8.f, -viewSize.y / 8.f, viewSize.y / 8.f, -0.1f, 60.f);
    miniCam.viewport = { 0.f, 0.f, 1.f/4.f, 1.f/4.f };*/


    m_minimapModels[m_currentHole].getComponent<cro::Model>().setHidden(false);
    m_mapScene.getSystem<cro::CameraSystem>()->process(0.f);
    m_mapScene.getSystem<cro::ModelRenderer>()->process(0.f);


    //auto vCount = m_mapScene.getSystem<cro::ModelRenderer>()->getVisibleCount(m_mapCam.getComponent<cro::Camera>().getDrawListIndex());
    //LogI << "Visible count: " << vCount << std::endl;

    //auto entCount = m_mapScene.getSystem<cro::ModelRenderer>()->getEntities().size();
    //LogI << "Entity count: " << entCount << std::endl;


    cro::Colour c = cro::Colour::Transparent;
    //cro::Colour c(std::uint8_t(39), 56, 153);
    
    if (m_minimapTexturePass == 0)
    {
        m_minimapTrail.getComponent<cro::Drawable2D>().getVertexData().clear();

        m_mapTextureMRT.clear(c);
    }
    else
    {
        m_mapTextureMRT.activate(true);
    }
    m_mapScene.render();
    m_minimapTexturePass++;

    m_minimapModels[m_currentHole].getComponent<cro::Model>().setHidden(true);

    if (m_minimapTexturePass == MaxMinimapPasses)
    {
        m_mapTextureMRT.display();
    }
    else
    {
        m_mapTextureMRT.activate(false);
    }

    ////m_mapTextureMRT.setBorderColour(c);


    //only finalise the minimap once all passes are complete
    if (m_minimapTexturePass == MaxMinimapPasses)
    {
        //this triggers a map refresh so don't set it until
        //we know the texture is up to date.
        m_sharedData.minimapData.holeNumber = m_currentHole;

        retargetMinimap(true);

        auto* msg = postMessage<SceneEvent>(MessageID::SceneMessage);
        msg->type = SceneEvent::MinimapUpdated;

        if (m_sharedData.scoreType == ScoreType::MultiTarget)
        {
            m_targetShader.size = 0.f;
            m_targetShader.update();
        }
    }
}

void GolfState::updateMiniMap()
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::MiniMap;
    cmd.action = [&](cro::Entity en, float)
    {
        //trigger animation - this does the actual render
        en.getComponent<cro::Callback>().active = true;
        //m_mapCam.getComponent<cro::Camera>().active = true;
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

    static constexpr float MinZoom = 0.5f;
    static constexpr float MaxZoom = 32.f;

    if (reset)
    {
        //create a default view around the bounds of the hole model
        target.end.tilt = 0.f; //TODO this could be wound several times past TAU and should be only fmod this value

        auto bb = m_holeData[m_currentHole].modelEntity.getComponent<cro::Model>().getAABB();
        auto centre = bb.getCentre();
        target.end.pan = glm::vec2(centre.x, -centre.z) * m_minimapZoom.mapScale;

        auto xZoom = std::clamp(static_cast<float>(MiniMapSize.x) / ((bb[1].x - bb[0].x) * 1.6f), MinZoom, MaxZoom);
        auto zZoom = std::clamp(static_cast<float>(MiniMapSize.y) / ((bb[1].z - bb[0].z) * 1.6f), MinZoom, MaxZoom);
        target.end.zoom = xZoom > zZoom ? xZoom : zZoom;
    }
    else
    {
        bool isMultiTarget = (m_sharedData.scoreType == ScoreType::MultiTarget 
            && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].targetHit);

        //find vec between player and flag
        auto pin = isMultiTarget ? m_holeData[m_currentHole].target : m_holeData[m_currentHole].pin;
        auto player = m_currentPlayer.position;

        //rotate minimap so flag is at top
        glm::vec2 dir(pin.x - player.x, -pin.z + player.z);
        auto rotation = std::atan2(-dir.y, dir.x) + cro::Util::Const::PI;
        target.end.tilt = m_minimapZoom.tilt + cro::Util::Maths::shortestRotation(m_minimapZoom.tilt, rotation);


        target.end.pan = glm::vec2(player.x, -player.z);

        //if we have a tight dogleg, such as on the mini putt
        //check if the primary target is in between and shift
        //towards it to better centre the hole
        const auto targ = findTargetPos(player);
        glm::vec2 targDir(targ.x - player.x, -targ.z + player.z);
        const auto dirNorm = glm::normalize(dir);
        const auto d = glm::dot(dirNorm, glm::normalize(targDir));
        const auto l2 = glm::length2(targDir);
        if (!isMultiTarget &&
            (d > 0 && d < 0.8f && l2 > 2.25f && l2 < glm::length2(dir)))
        {
            auto p = dir / 2.f;
            
            //find the distance to the target point and offset by perpendicular amount
            const glm::vec2 perpNormal(dirNorm.y, -dirNorm.x);
            const glm::vec2 targPos(targ.x, -targ.z);
            const float offset = /*std::abs*/((targPos.x - target.end.pan.x) * dirNorm.y - (targPos.y - target.end.pan.y) * dirNorm.x) / 2.f;
            p += perpNormal * offset;
            target.end.pan += p;
        }
        else
        {
            //centre view between player and flag
            target.end.pan += (dir / 2.f);
        }
        //(pan is in texture coords hum)
        target.end.pan *= m_minimapZoom.mapScale;

        //get distance between flag and player and expand by 1.4 (about 3m around a putting hole)
        //TODO this should be fixed 3m - as a percentage it's HUGE on big maps when fully zoomed
        float viewLength = std::max(glm::length(dir), m_inputParser.getEstimatedDistance()) * 1.45f; //remember this is world coords

        //scale zoom on long edge of map by box length and clamp to 32x
        target.end.zoom = std::clamp(static_cast<float>(MiniMapSize.x) / viewLength, MinZoom, MaxZoom);
    }

    //create a temp ent to interp between start and end values
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<MapZoomData>(target);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<MapZoomData>();

        //const auto speed = 0.4f + (0.7f * (1.f - std::clamp(glm::length2(data.start.pan - data.end.pan) / (100.f * 100.f), 0.f, 1.f)));
        const auto speed = 0.4f + (0.7f * (1.f - std::clamp(glm::length(data.start.pan - data.end.pan) / 100.f, 0.f, 1.f)));
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

glm::vec3 GolfState::findTargetPos(glm::vec3 playerPos) const
{
    if (glm::length2(m_holeData[m_currentHole].subtarget) < MaxSubTarget)
    {
        //this is (hopefully) a valid value and not the default
        const auto t = m_holeData[m_currentHole].target - playerPos;
        const auto m = m_holeData[m_currentHole].subtarget - playerPos;

        const float d = glm::dot(glm::normalize(glm::vec2(t.x, -t.z)), glm::normalize(glm::vec2(m.x, -m.z)));
        const float tLen = glm::length2(t);
        const float mLen = glm::length2(m);


        if (d > 0.6f) //both are more or less in front
        {
            //go for the sub-target if it is the furthest
            if (mLen > tLen
                && std::sqrt(mLen) / 2.f > std::sqrt(tLen)) //and more than twice as far
            {
                return m_holeData[m_currentHole].subtarget;
            }
        }

        if (tLen < (20.f * 20.f) //current pos < 20m to default target
            || mLen < tLen //sub-target closer than default target
            || d < 0.1f) //default target is almost behind us
        {
            return m_holeData[m_currentHole].subtarget;
        }
    }
    return m_holeData[m_currentHole].target;
}

void GolfState::updateProfileDB() const
{
    if (CourseIDs.count(m_sharedData.mapDirectory.toAnsiString()) != 0)
    {
        const auto courseID = static_cast<std::int32_t>(m_sharedData.courseIndex);// CourseIDs.at(m_sharedData.mapDirectory.toAnsiString()) - AchievementID::Complete01;
        const auto localCount = m_sharedData.localConnectionData.playerCount;
        const auto clientID = m_sharedData.localConnectionData.connectionID;
        const auto& localPlayers = m_sharedData.localConnectionData.playerData;
        const auto& playerData = m_sharedData.connectionData[clientID].playerData;

        ProfileDB db;
        for (auto i = 0u; i < localCount; ++i)
        {
            auto dbPath = Content::getUserContentPath(Content::UserContent::Profile) + localPlayers[i].profileID + "/profile.db3";
            if (db.open(dbPath))
            {
                CourseRecord record;
                auto scores = playerData[i].holeScores;

                switch (m_sharedData.holeCount)
                {
                default: continue;
                case 0:
                    if (m_sharedData.reverseCourse)
                    {
                        std::reverse(scores.begin(), scores.end());
                        std::reverse(m_personalBests[i].begin(), m_personalBests[i].end());
                    }
                    for (auto j = 0; j < 18; ++j)
                    {
                        record.holeScores[j] = scores[j];
                        record.total += scores[j];

                        m_personalBests[i][j].hole = j;
                        db.insertPersonalBestRecord(m_personalBests[i][j]);
                    }
                    break;
                case 1:
                    if (m_sharedData.reverseCourse)
                    {
                        std::reverse(scores.begin(), scores.begin() + 9);
                        std::reverse(m_personalBests[i].begin(), m_personalBests[i].begin() + 9);
                    }
                    for (auto j = 0; j < 9; ++j)
                    {
                        record.holeScores[j] = scores[j];
                        record.total += scores[j];

                        m_personalBests[i][j].hole = j;
                        db.insertPersonalBestRecord(m_personalBests[i][j]);
                    }
                    break;
                case 2:
                    if (m_sharedData.reverseCourse)
                    {
                        std::reverse(scores.begin(), scores.begin() + 9);
                        std::reverse(m_personalBests[i].begin(), m_personalBests[i].begin() + 9);
                    }
                    for (auto j = 0; j < 9; ++j)
                    {
                        record.holeScores[j + 9] = scores[j];
                        record.total += scores[j];

                        m_personalBests[i][j].hole = j + 9;
                        db.insertPersonalBestRecord(m_personalBests[i][j]);
                    }
                    break;
                }

                record.totalPar = playerData[i].parScore;
                record.holeCount = m_sharedData.holeCount;
                record.courseIndex = courseID;
                record.wasCPU = localPlayers[i].isCPU ? 1 : 0;

                if (m_sharedData.scoreType == ScoreType::Stroke)
                {
                    db.insertCourseRecord(record);
                }
            }
        }
    }
}

void GolfState::updateLeague()
{
    if (m_allowAchievements
        && m_sharedData.gameMode != GameMode::Tournament)
    {
        std::array<std::int32_t, 18u> parVals;
        for (auto i = 0u; i < m_holeData.size(); ++i)
        {
            parVals[i] = m_holeData[i].par;
        }

        League* league = nullptr;
        League clubLeague(LeagueRoundID::Club, m_sharedData);

        if (m_sharedData.leagueRoundID == LeagueRoundID::Club)
        {
            league = &clubLeague;
        }
        else
        {
            league = &Career::instance(m_sharedData).getLeagueTables()[m_sharedData.leagueRoundID - LeagueRoundID::RoundOne];
        }

        //we assume that as achievments are allowed that
        //there's only one human player - though they may not
        //necessarily be first in the player list
        for (const auto& player : m_sharedData.connectionData[m_sharedData.localConnectionData.connectionID].playerData)
        {
            if (!player.isCPU)
            {
                league->iterate(parVals, player.holeScores, m_holeData.size());
#ifdef USE_GNS
                //logGameScores(parVals, player.holeScores, m_holeData.size());
#endif
                break;
            }
        }

        //if this is the final league and the last round
        if (m_sharedData.leagueRoundID == LeagueRoundID::RoundSix
            && league->getCurrentSeason() > 1) //iterating above will have incremented this on completion
        {
            Achievements::awardAchievement(AchievementStrings[AchievementID::SemiRetired]);
        }

        //if all the leagues are gold...
        //remember this might happen in any order
        //if (league->getCurrentBest() == 1)
        if (m_sharedData.leagueRoundID != LeagueRoundID::Club)
        {
            std::int32_t bestCount = 0;
            for (auto i = 0u; i < 6u; ++i)
            {
                bestCount += Career::instance(m_sharedData).getLeagueTables()[i].getCurrentBest();
            }

            if (bestCount == 6)
            {
                Achievements::awardAchievement(AchievementStrings[AchievementID::AllTime]);
            }
        }
    }
}

void GolfState::updateLeagueHole()
{
    switch (m_sharedData.scoreType)
    {
    default: break;
    case ScoreType::Stroke:
    case ScoreType::ShortRound:
    case ScoreType::Stableford:
    case ScoreType::StablefordPro:
        //if this is allowed then we assume there's only one human player
        if (m_allowAchievements)
        {
            const auto playerScore = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole];
            const auto par = m_holeData[m_currentHole].par;

            //calc the likelyhood of wind affecting the CPU score
            auto holeDir = m_holeData[m_currentHole].tee - m_holeData[m_currentHole].pin;
            holeDir.y = 0.f;

            auto windDir = m_windUpdate.windVector;
            windDir.y = 0.f;

            const float headWind = (glm::dot(glm::normalize(holeDir), glm::normalize(windDir)) + 1.f) * 0.5f;

            static constexpr float BaseReduction = 0.9f;
            float resultF = headWind * BaseReduction * m_windUpdate.windVector.y;
            resultF /= (1.f + static_cast<float>(m_sharedData.clubSet));

            const std::int32_t windChance = static_cast<std::int32_t>(std::round(resultF * 100.f));

            switch (m_sharedData.leagueRoundID)
            {
            default: break;
            case LeagueRoundID::RoundOne:
            case LeagueRoundID::RoundTwo:
            case LeagueRoundID::RoundThree:
            case LeagueRoundID::RoundFour:
            case LeagueRoundID::RoundFive:
            case LeagueRoundID::RoundSix:
            {
                auto& league = Career::instance(m_sharedData).getLeagueTables()[m_sharedData.leagueRoundID - LeagueRoundID::RoundOne];
                //this may have been saved previously
                if (league.getHoleScores()[0][m_currentHole] == 0)
                {
                    league.updateHoleScores(m_currentHole, par, playerScore > (par + 1), windChance);
                    updateScoreboard();
                }
            }
            break;
            case LeagueRoundID::Club:
            {
                League league(m_sharedData.leagueRoundID, m_sharedData);
                if (league.getHoleScores()[0][m_currentHole] == 0)
                {
                    league.updateHoleScores(m_currentHole, par, playerScore > (par + 1), windChance);
                }
            }
            break;
            }

            if (m_friendlyPlayers)
            {
                m_friendlyPlayers->updateHoleScores(m_currentHole, par, playerScore > par, windChance);

                if (m_sharedData.gameMode == GameMode::Tournament)
                {
                    //copy CPU scores to tournament data and update the save
                    m_sharedData.tournaments[m_sharedData.activeTournament].opponentScores =
                        m_friendlyPlayers->getHoleScores(m_sharedData.tournaments[m_sharedData.activeTournament].opponentStats.nameIndex);

                    //and player score
                    m_sharedData.tournaments[m_sharedData.activeTournament].scores[m_currentHole] 
                        = m_sharedData.connectionData[0].playerData[0].holeScores[m_currentHole];
                    writeTournamentData(m_sharedData.tournaments[m_sharedData.activeTournament]);
                }
            }
        }
        break;
    }
}

void GolfState::setUIHidden(bool hidden)
{
    if (hidden)
    {
        //hide UI by bringing scene ent to front
        auto origin = m_courseEnt.getComponent<cro::Transform>().getOrigin();
        origin.z = -3.f;
        m_courseEnt.getComponent<cro::Transform>().setOrigin(origin);
    }
    else
    {
        auto origin = m_courseEnt.getComponent<cro::Transform>().getOrigin();
        origin.z = 0.5f;
        m_courseEnt.getComponent<cro::Transform>().setOrigin(origin);

        m_droneTextEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        m_droneTextEnt.getComponent<cro::Callback>().active = false;
    }
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

    matrix = glm::scale(matrix, glm::vec3(MapSizeRatio, 1.f));

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
    if (sharedData.gameMode == GameMode::Tutorial)
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
            std::string("sad_large"),

            std::string("rb"),
            std::string("lb"),
            std::string("lt"),
            std::string("rt")
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

            if (i > 3)
            {
                entity.addComponent<cro::SpriteAnimation>();
            }

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

            glm::vec3 originOffset = i < 4 ? glm::vec3(0.f, 20.f, 0.f) : glm::vec3(0.f);

            auto labelEnt = uiScene.createEntity();
            labelEnt.addComponent<cro::Transform>().setPosition(entity.getComponent<cro::Transform>().getOrigin() + originOffset);
            labelEnt.addComponent<cro::Drawable2D>();
            labelEnt.addComponent<cro::Text>(font);
            labelEnt.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
            /*labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f,-1.f });*/
            labelEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
            entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
            labelNodes[i] = labelEnt;
        }

        //background
        entity = uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, 0.13f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        auto bgEnt = entity;

        //chat label if using controller
        entity = uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -4.f, -18.f, 0.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString("Chat Window");
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        chatNode = entity;

        entity = uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -17.f, -11.f, 0.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("chat_button");
        entity.addComponent<cro::SpriteAnimation>();
        chatNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        chatButtonNode = entity;

        refreshLabels(); //must do this last!
    }
}

bool GolfState::EmoteWheel::handleEvent(const cro::Event& evt)
{
    if (sharedData.gameMode == GameMode::Tutorial)
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

            switch (evt.key.keysym.sym)
            {
            default: break;
            case SDLK_7:
            case SDLK_8:
            case SDLK_9:
            case SDLK_0:
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

            /*switch (evt.key.keysym.sym)
            {
            default: break;
            case SDLK_7:
                m_textChat.quickEmote(TextChat::Applaud);
                cooldown = 6.f;
                buttonNodes[5].getComponent<cro::Callback>().active = true;
                return true;
            case SDLK_8:
                m_textChat.quickEmote(TextChat::Happy);
                cooldown = 6.f;
                buttonNodes[4].getComponent<cro::Callback>().active = true;
                return true;
            case SDLK_9:
                m_textChat.quickEmote(TextChat::Laughing);
                cooldown = 6.f;
                buttonNodes[6].getComponent<cro::Callback>().active = true;
                return true;
            case SDLK_0:
                m_textChat.quickEmote(TextChat::Angry);
                cooldown = 6.f;
                buttonNodes[7].getComponent<cro::Callback>().active = true;
                return true;
            }*/
        }
    }


    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        auto controllerID = activeControllerID(sharedData.inputBinding.playerID);
        if (cro::GameController::controllerID(evt.cbutton.which) == controllerID
            || sharedData.localConnectionData.playerCount == 1)
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonY:
                targetScale = targetScale == 1.f ? 0.f : 1.f;

                {
                    auto anim = hasPSLayout(controllerID) ? 1 : 0;
                    for (auto i = 4u; i < buttonNodes.size(); ++i)
                    {
                        buttonNodes[i].getComponent<cro::SpriteAnimation>().play(anim);
                        chatButtonNode.getComponent<cro::SpriteAnimation>().play(anim);
                    }
                }

                return true;
            }
        }

        //prevent these getting forwarded to input parser if wheel is open
        //if (cro::GameController::isButtonPressed(controllerID, cro::GameController::ButtonY))
        if (targetScale == 1)
        {
            switch (evt.cbutton.button)
            {
            default: return false;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            case cro::GameController::ButtonLeftShoulder:
            case cro::GameController::ButtonRightShoulder:
            case cro::GameController::ButtonLeftStick:
            case cro::GameController::ButtonRightStick:

            case cro::GameController::ButtonA:
            case cro::GameController::ButtonX:
                return true;
            }
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        //the controller ID is actually used to select the play name in this case
        auto controllerID = activeControllerID(sharedData.localConnectionData.playerCount == 1 ? 0 :
            sharedData.inputBinding.playerID);

        /*if (cro::GameController::controllerID(evt.cbutton.which) == controllerID)
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonY:
                targetScale = 0.f;
                return true;
            }
        }*/

        if (currentScale == 1)
        {
            switch (evt.cbutton.button)
            {
            default: return false;
            case cro::GameController::ButtonA:
                //consume this so we don't mess with the swing
                return true;
            case cro::GameController::ButtonX:
                m_textChat.toggleWindow(true, true, true);
                //close emote wheel automatically
                targetScale = 0.f;
                return true;
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
            case cro::GameController::ButtonLeftShoulder:
                m_textChat.quickEmote(TextChat::Applaud);
                cooldown = 6.f;
                buttonNodes[5].getComponent<cro::Callback>().active = true;
                return true;
            case cro::GameController::ButtonRightShoulder:
                m_textChat.quickEmote(TextChat::Happy);
                cooldown = 6.f;
                buttonNodes[4].getComponent<cro::Callback>().active = true;
                return true;
            case cro::GameController::ButtonLeftStick:
                m_textChat.quickEmote(TextChat::Laughing);
                cooldown = 6.f;
                buttonNodes[6].getComponent<cro::Callback>().active = true;
                return true;
            case cro::GameController::ButtonRightStick:
                m_textChat.quickEmote(TextChat::Angry);
                cooldown = 6.f;
                buttonNodes[7].getComponent<cro::Callback>().active = true;
                return true;
            }
        }
    }

    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (targetScale ==  1.f)
        {
            switch (evt.caxis.axis)
            {
            default: return false;
            case cro::GameController::TriggerLeft:
            case cro::GameController::TriggerRight:
                return true;
            }
        }
    }

    return false;
}

void GolfState::EmoteWheel::update(float dt)
{
    if (sharedData.gameMode == GameMode::Tutorial
        || sharedData.gameMode == GameMode::Reset) //might be quitting from the menu
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
    if (sharedData.gameMode == GameMode::Tutorial)
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

    for (auto i = 0u; i < /*labelNodes.size()*/InputMap.size(); ++i)
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


    //hide trigger icons if no controller
    for (auto i = InputMap.size(); i < labelNodes.size(); ++i)
    {
        if (cro::GameController::getControllerCount() == 0)
        {
            buttonNodes[i].getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            chatNode.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
        else
        {
            buttonNodes[i].getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            chatNode.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }
    }
}

void GolfState::showEmote(std::uint32_t data)
{
    std::uint8_t client = (data & 0x00ff0000) >> 16;
    std::uint8_t player = (data & 0x0000ff00) >> 8;
    std::uint8_t emote = (data & 0x000000ff);

    client = std::min(client, std::uint8_t(ConstVal::MaxClients - 1));
    player = std::min(player, std::uint8_t(ConstVal::MaxPlayers - 1));

    auto msg = m_sharedData.connectionData[client].playerData[player].name;
    //msg += " is ";

    std::int32_t emoteID = SpriteID::EmoteHappy;
    switch (emote)
    {
    default:
        msg += "undecided";
        break;
    case Emote::Happy:
        msg += " is happy";
        break;
    case Emote::Grumpy:
        msg += " is grumpy";
        emoteID = SpriteID::EmoteGrumpy;
        break;
    case Emote::Laughing:
        msg += " is laughing";
        emoteID = SpriteID::EmoteLaugh;
        break;
    case Emote::Sad:
        msg += " applauds";
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

    glm::vec3 pos(32.f, -16.f, 0.2f + (static_cast<float>(client) / 10.f));
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
                postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::ChatMessage;
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
    postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::ChatMessage;
}
