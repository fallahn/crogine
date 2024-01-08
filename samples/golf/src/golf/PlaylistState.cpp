/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

#include "PlaylistState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "TextAnimCallback.hpp"
#include "PoissonDisk.hpp"
#include "Billboard.hpp"
#include "Treeset.hpp"
#include "../GolfGame.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/core/Mouse.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/CircleMeshBuilder.hpp>
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
#include <crogine/ecs/components/ShadowCaster.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/String.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include "../ErrorCheck.hpp"

#include <sstream>
#include <iomanip>

namespace
{
#include "shaders/WaterShader.inl"
#include "shaders/CelShader.inl"
#include "shaders/BillboardShader.inl"
#include "shaders/TreeShader.inl"
#include "shaders/ShadowMapping.inl"
#include "shaders/CloudShader.inl"
#include "shaders/ShaderIncludes.inl"

    const std::string SkyboxPath = "assets/golf/skyboxes/";
    const std::string ShrubPath = "assets/golf/shrubs/";
    const std::string CoursePath = "assets/golf/courses/";
    const std::string ThumbPath = "assets/golf/thumbs/";

    const std::string UserCoursePath = "courses/";
    const std::string UserCourseExport = "courses/export/";
    const std::string SaveFileExtension = ".ucs";

    constexpr float TabAreaHeight = 0.33f; //percent of screen
    constexpr float ItemSpacing = 10.f;
    constexpr float ThumbnailOffset = 108.f; //left hand spacing
    constexpr float PlaylistOffset = 100.f;
    constexpr float RootOffset = 8.f; //left hand offset for menu root nodes
    constexpr float InfoOffset = 154.f; //offset from right for info board
    
    constexpr std::size_t MaxSkyboxes = 40;
    constexpr std::size_t MaxShrubs = 40;
    constexpr std::size_t MaxCourses = 40;
    constexpr std::size_t MaxHoles = 18;
    constexpr std::size_t MaxSaves = 16;

    cro::Entity helpOverlay;
    struct BgData final
    {
        std::int32_t state = 0;
        float progress = 0.f;
        std::vector<cro::Entity> children;
    };

    struct ScrollData final
    {
        explicit ScrollData(std::int32_t ic) : itemCount(ic) {}
        std::int32_t itemCount = 0;

        std::int32_t currIndex = 0;
        std::int32_t targetIndex = 0;
        glm::vec3 basePosition = glm::vec3(0.f);

        float normalisedPosition = 0.f;
    };

    struct ScrollNodeCallback final
    {
        void operator ()(cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<ScrollData>();

            auto targetPos = data.basePosition;
            targetPos.y += ItemSpacing * data.targetIndex;

            auto movement = targetPos - e.getComponent<cro::Transform>().getPosition();
            if (glm::length2(movement) > 1)
            {
                e.getComponent<cro::Transform>().move(movement * (dt * 30.f));
            }
            else
            {
                e.getComponent<cro::Transform>().setPosition(targetPos);
                data.currIndex = data.targetIndex;
            }

            if (data.itemCount > 1)
            {
                data.normalisedPosition = (e.getComponent<cro::Transform>().getPosition().y - data.basePosition.y) / ((data.itemCount - 1) * ItemSpacing);
            }
            else
            {
                data.normalisedPosition = 0.f;
            }
        };
    };

    struct ListItemCallback final
    {
        const cro::FloatRect& croppingArea;
        explicit ListItemCallback(const cro::FloatRect& c)
            : croppingArea(c) {}

        void operator ()(cro::Entity e, float)
        {
            auto cropRect = croppingArea;
            auto localBounds = e.getComponent<cro::Drawable2D>().getLocalBounds();
            auto pos = e.getComponent<cro::Transform>().getWorldPosition();
            cropRect.left -= pos.x;
            cropRect.bottom -= pos.y;
            cropRect.bottom -= localBounds.bottom + localBounds.height;
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropRect);

            float scale = 1.f;
            pos.y -= (localBounds.height / 2.f);
            if (!croppingArea.contains(pos))
            {
                scale = 0.f;
            }
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        }
    };

    struct SliderData final
    {
        cro::Entity scrollNode;
        cro::Entity buttonTop;
        cro::Entity buttonBottom;
        cro::FloatRect bounds;

        SliderData(cro::Entity s, cro::Entity t, cro::Entity b, cro::FloatRect r)
            : scrollNode(s), buttonTop(t), buttonBottom(b), bounds(r) {}
        void updatePosition(glm::vec2 position)
        {
            auto& scrollData = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            if (scrollData.itemCount > 1)
            {
                auto topPos = buttonTop.getComponent<cro::Transform>().getWorldPosition().y;
                auto bottomPos = buttonBottom.getComponent<cro::Transform>().getWorldPosition().y;
                auto height = topPos - bottomPos;
                auto mouseHeight = topPos - position.y;

                float relPos = std::min(1.f, std::max(0.f, mouseHeight / height));
                relPos *= scrollData.itemCount;
                scrollData.targetIndex = std::min(static_cast<std::int32_t>(std::floor(relPos)), scrollData.itemCount - 1);
            }
        }
    };

    struct SliderCallback final
    {
        void operator()(cro::Entity e, float)
        {
            auto data = e.getComponent<cro::Callback>().getUserData<SliderData>();
            auto scrollTop = data.buttonTop.getComponent<cro::Transform>().getPosition();
            auto scrollBottom = data.buttonBottom.getComponent<cro::Transform>().getPosition();
            float length = scrollTop.y - scrollBottom.y - (data.bounds.height * 2.f);

            e.getComponent<cro::Transform>().setPosition({ scrollTop.x + 1.f, scrollTop.y - data.bounds.height, 0.2f });

            e.getComponent<cro::Transform>().move({ 0.f, -length * data.scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().normalisedPosition, 0.f });
        };
    };

    struct PopupData final
    {
        enum { In, Idle, Out };
        std::int32_t state = In;
        float progress = 0.f;
    };

    cro::Entity createHighlight(cro::Scene& scene, const cro::SpriteSheet& spriteSheet, bool scroll = true)
    {
        auto entity = scene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Drawable2D>();
        if (scroll)
        {
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("scroll_highlight");
        }
        else
        {
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("add_remove_highlight");
        }
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f), -0.3f });

        entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();

        return entity;
    }
}

PlaylistState::PlaylistState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State                (ss, ctx),
    m_skyboxScene               (ctx.appInstance.getMessageBus()),
    m_gameScene                 (ctx.appInstance.getMessageBus()),
    m_uiScene                   (ctx.appInstance.getMessageBus(), 512),
    m_sharedData                (sd),
    m_scaleBuffer               ("PixelScale"),
    m_resolutionBuffer          ("ScaledResolution"),
    m_windBuffer                ("WindValues"),
    m_viewScale                 (2.f),
    m_skyboxIndex               (0),
    m_shrubIndex                (0),
    m_holeDirIndex              (0),
    m_thumbnailIndex            (0),
    m_playlistIndex             (0),
    m_saveFileIndex             (0),
    m_playlistActivatedCallback (0),
    m_playlistSelectedCallback  (0),
    m_playlistUnselectedCallback(0),
    m_currentTab                (0)
{
    ctx.mainWindow.setMouseCaptured(false);

    ctx.mainWindow.loadResources(
        [this]() 
        {
            addSystems();
            loadAssets();
            buildScene();
            buildUI();
        });

    Social::setStatus(Social::InfoID::Menu, { "Editing a Course" });

#ifdef CRO_DEBUG_
    registerWindow([&]()
        {
            if (ImGui::Begin("buns"))
            {
                //ImGui::Text("Current Slider %u", m_activeSlider.getIndex());
                /*static float maxDist = 50.f;
                if (ImGui::SliderFloat("Distance", &maxDist, 1.f, 50.f))
                {
                    m_gameScene.getActiveCamera().getComponent<cro::Camera>().setMaxShadowDistance(maxDist);
                }

                static float overshoot = 0.f;
                if (ImGui::SliderFloat("Overshoot", &overshoot, 0.f, 20.f))
                {
                    m_gameScene.getActiveCamera().getComponent<cro::Camera>().setShadowExpansion(overshoot);
                }*/
                const auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
                ImGui::Image(cam.reflectionBuffer.getTexture(), {300.f, 300.f}, {0.f, 1.f}, {1.f, 0.f});
            }
            ImGui::End();
        }, true);
#endif
}

//public
bool PlaylistState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse)
    {
        return false;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
            quitState();
            return false;
        case SDLK_1:
            setActiveTab(0);
            break;
        case SDLK_2:
            setActiveTab(1);
            break;
        case SDLK_3:
            setActiveTab(2);
            break;
        case SDLK_4:
            setActiveTab(3);
            break;
        case SDLK_p:
        case SDLK_PAUSE:
        case SDLK_ESCAPE:
            requestStackPush(StateID::Pause);
            break;
#ifdef CRO_DEBUG_
        case SDLK_F9:
            requestStackPush(StateID::Bush);
            break;
        case SDLK_KP_0:
            confirmSave();
            break;
#endif
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
        static constexpr std::size_t MaxTabs = 4;

        cro::App::getWindow().setMouseCaptured(true);
        switch (evt.cbutton.button)
        {
        case cro::GameController::ButtonB:
        case cro::GameController::ButtonBack:
            quitState();
            return false;
        case cro::GameController::ButtonLeftShoulder:
            m_currentTab = (m_currentTab + (MaxTabs - 1)) % MaxTabs;
            setActiveTab(m_currentTab);
            break;
        case cro::GameController::ButtonRightShoulder:
            m_currentTab = (m_currentTab + 1) % MaxTabs;
            setActiveTab(m_currentTab);
            break;
        case cro::GameController::ButtonStart:
            requestStackPush(StateID::Pause);
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            m_activeSlider = {};
        }
        else if (evt.button.button == SDL_BUTTON_RIGHT
            && helpOverlay.isValid())
        {
            quitState();
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_LEFT
            && !helpOverlay.isValid())
        {
            //see if any sliders on the active tab are under the mouse
            for (auto e : m_sliders[m_currentTab])
            {
                auto bounds = e.getComponent<cro::Transform>().getWorldTransform() * e.getComponent<cro::Drawable2D>().getLocalBounds();
                auto pos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(glm::vec2(evt.button.x, evt.button.y));
                if (bounds.contains(pos))
                {
                    m_activeSlider = e;
                }
            }
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        if (m_activeSlider.isValid())
        {
            //update the associated scrollbar position
            auto pos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(glm::vec2(evt.button.x, evt.button.y));
            m_activeSlider.getComponent<cro::Callback>().getUserData<SliderData>().updatePosition(pos);
        }
        cro::App::getWindow().setMouseCaptured(false);
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        if (evt.wheel.y > 0)
        {
            cro::ButtonEvent fakeEvent;
            fakeEvent.type = SDL_MOUSEBUTTONDOWN;
            fakeEvent.button.button = SDL_BUTTON_LEFT;

            //up
            auto menuID = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
            switch (menuID)
            {
            default: break;
            case MenuID::Skybox:
                m_callbacks[CallbackID::SkyScrollUp](cro::Entity(), fakeEvent);
                break;
            case MenuID::Shrubbery:
                m_callbacks[CallbackID::ShrubScrollUp](cro::Entity(), fakeEvent);
                break;
            case MenuID::Holes:
                //read mouse position because we may want to scroll playlist instead.
            {
                auto mousePos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
                if (mousePos.x < ThumbnailOffset * m_viewScale.x)
                {
                    m_callbacks[CallbackID::HoleDirScrollUp](cro::Entity(), fakeEvent);
                }
                else
                {
                    m_callbacks[CallbackID::PlaylistScrollUp](cro::Entity(), fakeEvent);
                }
            }
                break;
            case MenuID::FileSystem:
                m_callbacks[CallbackID::SaveScrollUp](cro::Entity(), fakeEvent);
                break;
            }
        }
        else if (evt.wheel.y < 0)
        {
            cro::ButtonEvent fakeEvent;
            fakeEvent.type = SDL_MOUSEBUTTONDOWN;
            fakeEvent.button.button = SDL_BUTTON_LEFT;

            //down
            auto menuID = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
            switch (menuID)
            {
            default: break;
            case MenuID::Skybox:
                m_callbacks[CallbackID::SkyScrollDown](cro::Entity(), fakeEvent);
                break;
            case MenuID::Shrubbery:
                m_callbacks[CallbackID::ShrubScrollDown](cro::Entity(), fakeEvent);
                break;
            case MenuID::Holes:
            {
                auto mousePos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
                if (mousePos.x < ThumbnailOffset * m_viewScale.x)
                {
                    m_callbacks[CallbackID::HoleDirScrollDown](cro::Entity(), fakeEvent);
                }
                else
                {
                    m_callbacks[CallbackID::PlaylistScrollDown](cro::Entity(), fakeEvent);
                }
            }
                break;
            case MenuID::FileSystem:
                m_callbacks[CallbackID::SaveScrollDown](cro::Entity(), fakeEvent);
                break;
            }
        }
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (evt.caxis.value > LeftThumbDeadZone)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
    }


    m_uiScene.getSystem<cro::UISystem>()->handleEvent(evt);

    m_skyboxScene.forwardEvent(evt);
    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void PlaylistState::handleMessage(const cro::Message& msg)
{
    m_skyboxScene.forwardMessage(msg);
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool PlaylistState::simulate(float dt)
{
    static WindData windData;
    windData.elapsedTime += dt;
    windData.direction[0] = 1.f;
    windData.direction[1] = 0.6f;

    m_windBuffer.setData(windData);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);

    //do this last to make sure scene cam is up to date
    const auto& srcCam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    auto& dstCam = m_skyboxScene.getActiveCamera().getComponent<cro::Camera>();

    dstCam.viewport = srcCam.viewport;
    dstCam.setPerspective(srcCam.getFOV(), srcCam.getAspectRatio(), 0.5f, 14.f);

    m_skyboxScene.getActiveCamera().getComponent<cro::Transform>().setRotation(
        m_gameScene.getActiveCamera().getComponent<cro::Transform>().getWorldRotation());

    m_skyboxScene.simulate(dt);
    return false;
}

void PlaylistState::render()
{
    m_scaleBuffer.bind(0);
    m_windBuffer.bind(1);
    m_resolutionBuffer.bind(2);

    glEnable(GL_PROGRAM_POINT_SIZE);

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    auto oldVP = cam.viewport;

    cam.viewport = { 0.f,0.f,1.f,1.f };

    cam.setActivePass(cro::Camera::Pass::Reflection);

    auto& skyCam = m_skyboxScene.getActiveCamera().getComponent<cro::Camera>();
    skyCam.setActivePass(cro::Camera::Pass::Reflection);
    skyCam.viewport = { 0.f,0.f,1.f,1.f };

    cam.reflectionBuffer.clear(cro::Colour::Red);
    //don't want to test against skybox depth values.
    m_skyboxScene.render();
    glClear(GL_DEPTH_BUFFER_BIT);
    m_gameScene.render();
    cam.reflectionBuffer.display();

    cam.setActivePass(cro::Camera::Pass::Final);
    cam.viewport = oldVP;

    skyCam.setActivePass(cro::Camera::Pass::Final);
    skyCam.viewport = oldVP;



    m_gameSceneTexture.clear();
    m_skyboxScene.render();

    glClear(GL_DEPTH_BUFFER_BIT);
    m_gameScene.render();
    m_gameSceneTexture.display();

    m_uiScene.render();
}

//private
void PlaylistState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_skyboxScene.addSystem<cro::CallbackSystem>(mb);
    m_skyboxScene.addSystem<cro::SkeletalAnimator>(mb);
    m_skyboxScene.addSystem<cro::CameraSystem>(mb);
    m_skyboxScene.addSystem<cro::ModelRenderer>(mb);

    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::BillboardSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb);

    m_uiScene.addSystem<cro::UISystem>(mb)->setMouseScrollNavigationEnabled(false);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteAnimationSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);
}

void PlaylistState::loadAssets()
{
    for (const auto& [name, str] : IncludeMappings)
    {
        m_resources.shaders.addInclude(name, str);
    }

    //materials
    m_resources.shaders.loadFromString(ShaderID::Horizon, HorizonVert, HorizonFrag);
    auto* shader = &m_resources.shaders.get(ShaderID::Horizon);
    m_materialIDs[MaterialID::Horizon] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Water, WaterVertex, WaterFragment, "#define NO_DEPTH\n");
    shader = &m_resources.shaders.get(ShaderID::Water);
    m_scaleBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Water] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Cloud, CloudOverheadVertex, CloudOverheadFragment, "#define REFLECTION\n#define POINT_LIGHT\n");
    shader = &m_resources.shaders.get(ShaderID::Cloud);
    //m_scaleBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Cloud] = m_resources.materials.add(*shader);


    std::string wobble;
    if (m_sharedData.vertexSnap)
    {
        wobble = "#define WOBBLE\n";
    }

    m_resources.shaders.loadFromString(ShaderID::Cel, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define DITHERED\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Cel);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Cel] = m_resources.materials.add(*shader);

    auto& noiseTex = m_resources.textures.get("assets/golf/images/wind.png");
    noiseTex.setRepeated(true);
    noiseTex.setSmooth(true);

    m_resources.shaders.loadFromString(ShaderID::CelTextured, CelVertexShader, CelFragmentShader, "#define WIND_WARP\n#define TEXTURED\n#define NOCHEX\n#define SUBRECT\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTextured);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTextured] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]).setProperty("u_noiseTexture", noiseTex);

    m_resources.shaders.loadFromString(ShaderID::CelTexturedSkinned, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define DITHERED\n#define SKINNED\n#define NOCHEX\n#define SUBRECT\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTexturedSkinned);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTexturedSkinned] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Course, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define RX_SHADOWS\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Course);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Course] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Billboard, BillboardVertexShader, BillboardFragmentShader);
    shader = &m_resources.shaders.get(ShaderID::Billboard);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Billboard] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::Billboard]).setProperty("u_noiseTexture", noiseTex);


    m_resources.shaders.loadFromString(ShaderID::TreesetBranch, BranchVertex, BranchFragment, "#define INSTANCING\n#define ALPHA_CLIP\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::TreesetBranch);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Branch] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::Branch]).setProperty("u_noiseTexture", noiseTex);

    m_resources.shaders.loadFromString(ShaderID::TreesetLeaf, BushVertex, BushGeom, BushFragment, "#define POINTS\n#define INSTANCING\n#define HQ\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::TreesetLeaf);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Leaf] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::TreesetShadow, ShadowVertex, ShadowFragment, "#define INSTANCING\n#define TREE_WARP\n#define ALPHA_CLIP\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::TreesetShadow);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::BranchShadow] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::BranchShadow]).setProperty("u_noiseTexture", noiseTex);

    std::string alphaClip = m_sharedData.hqShadows ? "#define ALPHA_CLIP\n" : "";
    m_resources.shaders.loadFromString(ShaderID::TreesetLeafShadow, ShadowVertex, ShadowGeom, ShadowFragment, "#define POINTS\n #define INSTANCING\n#define LEAF_SIZE\n" + alphaClip + wobble);
    shader = &m_resources.shaders.get(ShaderID::TreesetLeafShadow);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::LeafShadow] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::BillboardShadow, BillboardVertexShader, ShadowFragment, "#define SHADOW_MAPPING\n#define ALPHA_CLIP\n");
    shader = &m_resources.shaders.get(ShaderID::BillboardShadow);
    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::BillboardShadow] = m_resources.materials.add(*shader);

    //audio - TODO do we need to keep the audio scape as a member?
    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);
    m_audioEnts[AudioID::Accept] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");
}

void PlaylistState::buildScene()
{
    //water plane
    auto meshID = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(100.f, 30));
    auto waterEnt = m_gameScene.createEntity();
    waterEnt.addComponent<cro::Transform>().setPosition({0.f, WaterLevel, 0.f});
    waterEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
    auto material = m_resources.materials.get(m_materialIDs[MaterialID::Water]);
    material.setProperty("u_radius", 100.f);
    waterEnt.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    m_gameScene.setWaterLevel(WaterLevel);
    m_skyboxScene.setWaterLevel(WaterLevel);
    m_skyboxScene.getActiveCamera().getComponent<cro::Camera>().reflectionBuffer.create(2, 2);

    //island
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/golf/models/cart.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 1.f, 0.f });
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
        for (auto i = 1u; i < entity.getComponent<cro::Model>().getMeshData().submeshCount; ++i)
        {
            applyMaterialData(md, material, i);
            entity.getComponent<cro::Model>().setMaterial(i, material);
        }
    }

    if (md.loadFromFile("assets/golf/models/island.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.f });
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Course]);
        for (auto i = 0u; i < entity.getComponent<cro::Model>().getMeshData().submeshCount; ++i)
        {
            applyMaterialData(md, material, i);
            entity.getComponent<cro::Model>().setMaterial(i, material);
        }

        m_collisionMesh.updateCollisionMesh(entity.getComponent<cro::Model>().getMeshData());
    }

    /*if (md.loadFromFile("assets/golf/models/cloud.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 2.25f, -3.f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Cloud]);
        material.setProperty("u_colourA", TextNormalColour);
        material.setProperty("u_colourB", TextGreenColour);
        entity.getComponent<cro::Model>().setMaterial(0, material);
    }*/

    //distributions
    static constexpr float GrassDensity = 1.7f;
    static constexpr float TreeDensity = 4.f;

    static constexpr std::array MinBounds = { 0.f, 0.f };
    static constexpr std::array MaxBounds = { static_cast<float>(MapSize.x) / 5.f, static_cast<float>(MapSize.y) / 5.f };

    auto seed = static_cast<std::uint32_t>(std::time(nullptr));
    auto grass = pd::PoissonDiskSampling(GrassDensity, MinBounds, MaxBounds, 30u, seed);
    auto trees = pd::PoissonDiskSampling(TreeDensity, MinBounds, MaxBounds);
    auto flowers = pd::PoissonDiskSampling(TreeDensity * 0.5f, MinBounds, MaxBounds, 30u, seed / 2);

    auto distributionToWorldPoints = 
        [&](const std::vector<std::array<float, 2u>>& dist, std::vector<glm::vec3>& output, std::int32_t terrainID)
    {
        constexpr glm::vec3 offset(-32.f, 0.f, 20.f);
        for (const auto& p : dist)
        {
            glm::vec3 position(p[0], 0.f, -p[1]);
            position += offset;
            auto result = m_collisionMesh.getTerrain(position);
            if (result.terrain == terrainID
                && result.height > WaterLevel)
            {
                position.y = result.height - 0.05f;
                output.push_back(position);
            }
        }
    };
    distributionToWorldPoints(grass, m_grassDistribution, TerrainID::Rough);
    distributionToWorldPoints(trees, m_treeDistribution, TerrainID::Scrub);
    distributionToWorldPoints(flowers, m_flowerDistribution, TerrainID::Scrub);

    //3D camera
    auto updateView = [&](cro::Camera& cam)
    {
        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        float maxScale = getViewScale();
        float scale = m_sharedData.pixelScale ? maxScale : 1.f;
        auto texSize = winSize / scale;

        std::uint32_t samples = m_sharedData.pixelScale ? 0 :
            m_sharedData.antialias ? m_sharedData.multisamples : 0;

        m_sharedData.antialias =
            m_gameSceneTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y), true, false, samples)
            && m_sharedData.multisamples != 0
            && !m_sharedData.pixelScale;

        auto invScale = (maxScale + 1.f) - scale;
        glCheck(glPointSize(invScale * BallPointSize));
        glCheck(glLineWidth(invScale));

        m_scaleBuffer.setData(invScale);

        m_resolutionUpdate.resolutionData.resolution = texSize / invScale;
        m_resolutionBuffer.setData(m_resolutionUpdate.resolutionData);


        //this lets the shader scale leaf billboards correctly
        auto targetHeight = texSize.y;
        glUseProgram(m_resources.shaders.get(ShaderID::TreesetLeaf).getGLHandle());
        glUniform1f(m_resources.shaders.get(ShaderID::TreesetLeaf).getUniformID("u_targetHeight"), targetHeight);

        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, texSize.x / texSize.y, 0.1f, 115.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({0.f, 2.5f, 15.f});
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -15.f * cro::Util::Const::degToRad);

    static constexpr std::uint32_t ReflectionMapSize = 1024u;
    static constexpr std::uint32_t ShadowMapSize = 2048u;
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    cam.reflectionBuffer.setSmooth(true);
    cam.shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    cam.setMaxShadowDistance(20.f);
    cam.setRenderFlags(cro::Camera::Pass::Reflection, RenderFlags::Reflection);
    cam.resizeCallback = updateView;
    updateView(cam);

    camEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("01");
    camEnt.getComponent<cro::AudioEmitter>().play();

    auto rootEnt = m_gameScene.createEntity();
    rootEnt.addComponent<cro::Transform>().setPosition({ 0.f, 1.f, 0.f });
    rootEnt.addComponent<cro::Callback>().active = true;
    rootEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt * 0.1f);
    };
    rootEnt.getComponent<cro::Transform>().addChild(camEnt.getComponent<cro::Transform>());


    //light direction
    auto sunEnt = m_gameScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -130.f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -75.f * cro::Util::Const::degToRad);
}

void PlaylistState::buildUI()
{
    //renders main scene
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>().setTexture(m_gameSceneTexture.getTexture());
    auto sceneEnt = entity;

    //attach UI to this so we have a single root scale
    auto rootNode = m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>();

    //use a 9-patch to create the tab view background
    auto ninePatch = m_uiScene.createEntity();
    ninePatch.addComponent<cro::Transform>().setPosition({0.f, 0.f, -0.5f});
    ninePatch.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    ninePatch.getComponent<cro::Drawable2D>().setTexture(&m_resources.textures.get("assets/golf/images/tab_patch.png"));
    rootNode.getComponent<cro::Transform>().addChild(ninePatch.getComponent<cro::Transform>());

    //used as the pop-up background, buttons are added and removed by relevant callbacks
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({0.f, 0.f, -0.4f});
    entity.addComponent<cro::Drawable2D>();
    auto popupBG = entity;
    rootNode.getComponent<cro::Transform>().addChild(popupBG.getComponent<cro::Transform>());
    
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/facilities_menu.spt", m_resources.textures);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(-1000.f, -100000.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height, -0.5f });
    entity.addComponent<cro::Callback>().setUserData<PopupData>();
    entity.getComponent<cro::Callback>().function =
        [&, popupBG](cro::Entity e, float dt) mutable
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<PopupData>();
        glm::vec2 targetPos = (glm::vec2(cro::App::getWindow().getSize()) / m_viewScale);
        auto windowSize = targetPos;
        targetPos.x /= 2.f;
        auto bgPos = targetPos;
        targetPos.y /= 2.f;
        targetPos.y += e.getComponent<cro::Sprite>().getTextureBounds().height / 2.f;

        float speed = dt * 4.f;

        switch (data.state)
        {
        default: break;
        case PopupData::In:
        {
            data.progress = std::min(1.f, data.progress + speed);
            float progress = cro::Util::Easing::easeInOutCirc(data.progress);
            targetPos.y *= progress;
            bgPos.y *= progress;

            if (data.progress == 1)
            {
                data.state = PopupData::Idle;
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Popup);
            }
        }
            break;
        case PopupData::Idle:

            break;
        case PopupData::Out:
        {
            data.progress = std::max(0.f, data.progress - speed);
            float progress = cro::Util::Easing::easeOutQuint(data.progress);
            targetPos.y *= progress;
            bgPos.y *= progress;

            if (data.progress == 0)
            {
                data.state = PopupData::In;
                e.getComponent<cro::Callback>().active = false;

                updateInfo();
            }
        }
            break;
        }

        e.getComponent<cro::Transform>().setPosition(targetPos);
        popupBG.getComponent<cro::Transform>().setPosition(bgPos);

        const cro::Colour c(0.f, 0.f, 0.f, BackgroundAlpha * data.progress);
        popupBG.getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(-windowSize.x / 2.f, 0.f), c),
                cro::Vertex2D(glm::vec2(-windowSize.x / 2.f, -windowSize.y), c),
                cro::Vertex2D(glm::vec2(windowSize.x / 2.f, 0.f), c),
                cro::Vertex2D(glm::vec2(windowSize.x / 2.f, -windowSize.y), c)
            });
    };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[MenuID::Popup] = entity;

    auto updateView = [&, sceneEnt, rootNode, ninePatch](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);

        updateNinePatch(ninePatch); //requires scale to be set above

        //resets the sprite size with the updated texture size
        glm::vec2 courseScale(m_sharedData.pixelScale ? m_viewScale.x : 1.f);
        sceneEnt.getComponent<cro::Transform>().setScale(courseScale);
        sceneEnt.getComponent<cro::Sprite>().setTexture(m_gameSceneTexture.getTexture());


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
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());


    auto textSelected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
        });
    auto textUnselected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });

    auto highlightSelected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            if (e.hasComponent<cro::Callback>())
            {
                e.getComponent<cro::Callback>().active = true;
            }
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
        });
    auto highlightUnselected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto showTip = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            //showToolTip(e.getLabel());
        });
    auto hideTip = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            //hideToolTip();
        });

    m_popupIDs[PopupID::TextSelected] = textSelected;
    m_popupIDs[PopupID::TextUnselected] = textUnselected;
    m_popupIDs[PopupID::ClosePopup] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(m_currentTab);

                m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().getUserData<PopupData>().state = PopupData::Out;
            }
        });
    m_popupIDs[PopupID::SaveNew] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                saveCourse(true);

                addSaveFileItem(m_saveFileIndex, glm::vec2(0.f, -ItemSpacing * m_saveFileIndex));

                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::FileSystem);
                m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().getUserData<PopupData>().state = PopupData::Out;
            }
        });
    m_popupIDs[PopupID::SaveOverwrite] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                saveCourse(false);

                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::FileSystem);
                m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().getUserData<PopupData>().state = PopupData::Out;
            }
        });
    m_popupIDs[PopupID::LoadFile] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_saveFileIndex = e.getComponent<cro::Callback>().getUserData<std::size_t>();
                loadCourse();

                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::FileSystem);
                m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().getUserData<PopupData>().state = PopupData::Out;
            }
        });
    m_popupIDs[PopupID::QuitState] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                requestStackClear();
                requestStackPush(StateID::Menu);
            }
        });

    spriteSheet.loadFromFile("assets/golf/sprites/course_tabs.spt", m_resources.textures);

    MenuData md;
    md.textSelected = textSelected;
    md.textUnselected = textUnselected;
    md.scrollSelected = highlightSelected;
    md.scrollUnselected = highlightUnselected;
    md.spriteSheet = &spriteSheet;

    createSkyboxMenu(rootNode, md);
    createShrubberyMenu(rootNode, md);
    createHoleMenu(rootNode, md);
    createFileSystemMenu(rootNode, md);


    m_animationIDs[AnimationID::TabHoles] = spriteSheet.getAnimationIndex("holes", "tab_bar");
    m_animationIDs[AnimationID::TabSkybox] = spriteSheet.getAnimationIndex("skybox", "tab_bar");
    m_animationIDs[AnimationID::TabSaveLoad] = spriteSheet.getAnimationIndex("save_load", "tab_bar");
    m_animationIDs[AnimationID::TabShrubs] = spriteSheet.getAnimationIndex("shrubs", "tab_bar");


    //tab bar
    auto tabEnt = m_uiScene.createEntity();
    tabEnt.addComponent<cro::Transform>();
    tabEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    tabEnt.addComponent<UIElement>().relativePosition = { 0.f, TabAreaHeight };
    tabEnt.addComponent<cro::Drawable2D>();
    tabEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_bar");
    tabEnt.addComponent<cro::SpriteAnimation>().play(m_animationIDs[AnimationID::TabSkybox]);
    rootNode.getComponent<cro::Transform>().addChild(tabEnt.getComponent<cro::Transform>());

    //we should never attempt to access a menu at this index so we'll use it as a hacky convenience
    //to store the entity with the tab graphics (setActiveTab() will want to update the animation)
    m_tabEntities[MenuID::Dummy] = tabEnt;

    auto tabSelect = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                setActiveTab(e.getComponent<cro::Callback>().getUserData<std::int32_t>());
            }
        });

    constexpr glm::vec2 buttonOffset(15.f, 1.f);
    bounds = spriteSheet.getSprite("tab_highlight").getTextureBounds();
    auto tabWidth = spriteSheet.getSprite("tab_bar").getTextureBounds().width / 4.f;

    const std::array<std::string, 4u> TipText =
    {
        "Skybox", "Foliage", "Hole List", "Load/Save"
    };

    for (auto i = 0u; i < MenuID::Popup; ++i)
    {
        glm::vec3 position((i * tabWidth) + buttonOffset.x, buttonOffset.y, 0.1f);
        position.x += bounds.width / 2.f;
        position.y += bounds.height / 2.f;

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
        entity.getComponent<cro::Callback>().setUserData<std::int32_t>(i); //use this with button activated callback to know which index we we are

        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = highlightSelected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = highlightUnselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Enter] = showTip;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Exit] = hideTip;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = tabSelect;

        //entity.setLabel(TipText[i]);

        tabEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_tabEntities[i] = entity;
    }

    //needs a one frame delay before first call
    //to allow entities to be added to the ui system
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        //setActiveTab(MenuID::Shrubbery);
        setActiveTab(MenuID::Skybox);
        e.getComponent<cro::Callback>().active = false;
        m_uiScene.destroyEntity(e);
    };


    //displays info about current selections
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 1.f, 1.f };
    entity.getComponent<UIElement>().absolutePosition = { -InfoOffset, -12.f };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_infoEntity = entity;
    updateInfo();

    //previews the selected hole
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.f, 1.f };
    entity.getComponent<UIElement>().absolutePosition = { 4.f, -104.f };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_holePreview = entity;

    //shows tool tip
    /*entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(cro::Colour::Black);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        auto position = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
        position.x = std::floor(position.x);
        position.y = std::floor(position.y);
        position.z = 2.f;

        static constexpr glm::vec3 Offset(8.f, -4.f, 0.f);
        position += (Offset * m_viewScale.x);

        e.getComponent<cro::Transform>().setPosition(position);
        e.getComponent<cro::Transform>().setScale(m_viewScale);
    };
    m_toolTip = entity;*/

    //info background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -5.f, -92.f, -0.1f });
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("info_background");
    entity.addComponent<cro::Drawable2D>();
    m_infoEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //add buttons for help/quit - these have to be grouped with each tab
    //although that's OK because the help button is different depending on what's active
    auto quitID = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                quitState();
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });
    const auto createQuit = [&](std::uint32_t menuID)
    {
        auto e = m_uiScene.createEntity();
        e.addComponent<cro::Transform>().setPosition({ 2.f, 2.f, 0.1f });
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Sprite>() = spriteSheet.getSprite("leave_highlight");
        e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        e.addComponent<cro::UIInput>().area = e.getComponent<cro::Sprite>().getTextureBounds();
        e.getComponent<cro::UIInput>().setGroup(menuID);
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = highlightSelected;
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = highlightUnselected;
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = quitID;

        entity.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
    };
    createQuit(MenuID::Skybox);
    createQuit(MenuID::Shrubbery);
    createQuit(MenuID::Holes);
    createQuit(MenuID::FileSystem);

    auto helpID = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                showHelp();
            }
        });
    const auto createHelp = [&](std::uint32_t menuID)
    {
        auto e = m_uiScene.createEntity();
        e.addComponent<cro::Transform>().setPosition({ 141.f, 2.f, 0.1f });
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Sprite>() = spriteSheet.getSprite("help_highlight");
        e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        e.addComponent<cro::UIInput>().area = e.getComponent<cro::Sprite>().getTextureBounds();
        e.getComponent<cro::UIInput>().setGroup(menuID);
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = highlightSelected;
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = highlightUnselected;
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = helpID;

        entity.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
    };
    createHelp(MenuID::Skybox);
    createHelp(MenuID::Shrubbery);
    createHelp(MenuID::Holes);
    createHelp(MenuID::FileSystem);
}

void PlaylistState::createSkyboxMenu(cro::Entity rootNode, const MenuData& menuData)
{
    m_menuEntities[MenuID::Skybox] = m_uiScene.createEntity();
    rootNode.getComponent<cro::Transform>().addChild(m_menuEntities[MenuID::Skybox].addComponent<cro::Transform>());

    m_skyboxes = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + SkyboxPath);
    //TODO we want as good a way as possible to validate the files...
    m_skyboxes.erase(std::remove_if(m_skyboxes.begin(), m_skyboxes.end(),
        [](const std::string& box)
        {
            return cro::FileSystem::getFileExtension(box) != ".sbf" || box.find("_n") != std::string::npos;
        }), m_skyboxes.end());
    //just to make consistent across platforms
    std::sort(m_skyboxes.begin(), m_skyboxes.end());

    if (m_skyboxes.empty())
    {
        m_skyboxes.push_back("None Found.");

        //dummy ent to stop crashing if we try selecting this menu group
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::UIInput>().setGroup(MenuID::Skybox);

        return;
    }
    else if (m_skyboxes.size() > MaxSkyboxes)
    {
        //some arbitrary limit
        m_skyboxes.resize(MaxSkyboxes);
    }

    m_menuEntities[MenuID::Skybox].addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    m_menuEntities[MenuID::Skybox].addComponent<UIElement>().absolutePosition = { RootOffset, -8.f - ItemSpacing };
    m_menuEntities[MenuID::Skybox].getComponent<UIElement>().relativePosition = { 0.f, TabAreaHeight };

    auto scrollNode = m_uiScene.createEntity();
    scrollNode.addComponent<cro::Transform>();
    m_menuEntities[MenuID::Skybox].getComponent<cro::Transform>().addChild(scrollNode.getComponent<cro::Transform>());

    //scroll callbacks
    ScrollData scrollData(static_cast<std::int32_t>(m_skyboxes.size()));

    scrollData.basePosition = scrollNode.getComponent<cro::Transform>().getPosition();

    scrollNode.addComponent<cro::Callback>().setUserData<ScrollData>(scrollData);
    scrollNode.getComponent<cro::Callback>().active = true;
    scrollNode.getComponent<cro::Callback>().function = ScrollNodeCallback();

    m_callbacks[CallbackID::SkyScrollDown] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            data.targetIndex = std::min(data.targetIndex + 1, data.itemCount - 1);
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        }
    };
    m_callbacks[CallbackID::SkyScrollUp] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            if (data.currIndex > 0)
            {
                data.targetIndex = data.currIndex - 1;
            }
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        }
    };

    //scroll buttons
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_up");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();

    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, -std::floor(bounds.height * 1.25f) };
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    auto buttonEnt = entity;
    m_menuEntities[MenuID::Skybox].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f, });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Skybox);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::SkyScrollUp]);
    auto buttonTop = buttonEnt;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_down");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, std::floor(bounds.height * 2.f) + ItemSpacing };
    entity.getComponent<UIElement>().relativePosition = { 1.f, -TabAreaHeight };
    buttonEnt = entity;
    m_menuEntities[MenuID::Skybox].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f, });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Skybox);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::SkyScrollDown]);
    auto buttonBottom = buttonEnt;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("slider");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<SliderData>(scrollNode, buttonTop, buttonBottom, bounds);
    entity.getComponent<cro::Callback>().function = SliderCallback();
    m_menuEntities[MenuID::Skybox].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_sliders[MenuID::Skybox].push_back(entity);

    //chequed background
    auto& tex = m_resources.textures.get("assets/golf/images/ed_bg.png");
    tex.setRepeated(true);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -RootOffset / 4.f, 2.f, - 0.1f });
    entity.addComponent<cro::Drawable2D>().setTexture(&tex);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MessageBoard;
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        glm::vec2 texScale(e.getComponent<cro::Drawable2D>().getTexture()->getSize());
        texScale *= m_viewScale;

        e.getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f, m_croppingArea.height / texScale.y)),
                cro::Vertex2D(glm::vec2(0.f, -m_croppingArea.height / m_viewScale.y), glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2(m_croppingArea.width / m_viewScale.x, 0.f), glm::vec2(m_croppingArea.width / texScale.x, m_croppingArea.height / texScale.y)),
                cro::Vertex2D(glm::vec2(m_croppingArea.width / m_viewScale.x, -m_croppingArea.height / m_viewScale.y), glm::vec2(m_croppingArea.width / texScale.x, 0.f))
            });

        e.getComponent<cro::Callback>().active = false;
    };

    m_menuEntities[MenuID::Skybox].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //title
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(0.f, ItemSpacing + 2.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Skyboxes");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    m_menuEntities[MenuID::Skybox].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //list of skybox items
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    glm::vec2 position(0.f);
    for (auto i = 0u; i < m_skyboxes.size(); ++i)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString(m_skyboxes[i]);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = ListItemCallback(m_croppingArea);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::ScoreScroll;

        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Skybox);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = 
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&,i, scrollNode](cro::Entity e) mutable
                {
                    e.getComponent<cro::Text>().setFillColour(TextGoldColour);
                    auto pos = e.getComponent<cro::Transform>().getWorldPosition();
                    pos.y -= ItemSpacing / 2.f;
                    if (!m_croppingArea.contains(pos))
                    {
                        cro::ButtonEvent fakeEvent;
                        fakeEvent.type = SDL_MOUSEBUTTONDOWN;
                        fakeEvent.button.button = SDL_BUTTON_LEFT;

                        if (pos.y < m_croppingArea.bottom)
                        {
                            m_callbacks[CallbackID::SkyScrollDown](cro::Entity(), fakeEvent);
                        }
                        else if (pos.y > (m_croppingArea.bottom + m_croppingArea.height))
                        {
                            scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex = i;
                        }
                    }
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                });
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.textUnselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&, i](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        const auto& ents = m_skyboxScene.getSystem<cro::ModelRenderer>()->getEntities();
                        for (auto e : ents)
                        {
                            m_skyboxScene.destroyEntity(e); //this includes destroying the active cloud ring
                        }

                        SkyboxMaterials materials;
                        materials.horizon = m_materialIDs[MaterialID::Horizon];
                        //materials.horizonSun = m_materialIDs[MaterialID::HorizonSun];
                        materials.skinned = m_materialIDs[MaterialID::CelTexturedSkinned];

                        auto cloudEnt = loadSkybox(SkyboxPath + m_skyboxes[i], m_skyboxScene, m_resources, materials);
                        if (cloudEnt.isValid())
                        {
                            auto material = m_resources.materials.get(m_materialIDs[MaterialID::Cloud]);
                            material.setProperty("u_skyColourTop", m_skyboxScene.getSkyboxColours().top);
                            material.setProperty("u_skyColourBottom", m_skyboxScene.getSkyboxColours().middle);

                            m_cloudRing = cloudEnt;
                            m_cloudRing.getComponent<cro::Model>().setMaterial(0, material);
                        }

                        m_skyboxIndex = i;
                        m_courseData.skyboxPath = SkyboxPath + m_skyboxes[i];

                        updateInfo();

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                });

        position.y -= ItemSpacing;
        scrollNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    //load default skybox if found
    cro::Entity cloudEnt;
    if (!m_skyboxes.empty())
    {
        m_skyboxIndex = 0;

        SkyboxMaterials materials;
        materials.horizon = m_materialIDs[MaterialID::Horizon];
        //materials.horizonSun = m_materialIDs[MaterialID::HorizonSun];
        materials.skinned = m_materialIDs[MaterialID::CelTexturedSkinned];

        auto ent = loadSkybox(SkyboxPath + m_skyboxes[m_skyboxIndex], m_skyboxScene, m_resources, materials);
        //we only want one of these
        if (!cloudEnt.isValid())
        {
            cloudEnt = ent;
        }
        else
        {
            m_skyboxScene.destroyEntity(ent);
        }

        m_courseData.skyboxPath = SkyboxPath + m_skyboxes[m_skyboxIndex];
    }
}

void PlaylistState::createShrubberyMenu(cro::Entity rootNode, const MenuData& menuData)
{
    m_menuEntities[MenuID::Shrubbery] = m_uiScene.createEntity();
    rootNode.getComponent<cro::Transform>().addChild(m_menuEntities[MenuID::Shrubbery].addComponent<cro::Transform>());

    m_shrubs = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + ShrubPath);
    m_shrubs.erase(std::remove_if(m_shrubs.begin(), m_shrubs.end(),
        [](const std::string& box)
        {
            return cro::FileSystem::getFileExtension(box) != ".shb";
        }), m_shrubs.end());
    //just to make consistent across platforms
    std::sort(m_shrubs.begin(), m_shrubs.end());

    if (m_shrubs.empty())
    {
        m_shrubs.push_back("None Found.");

        //dummy ent to stop crashing if we try selecting this menu group
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::UIInput>().setGroup(MenuID::Shrubbery);

        return;
    }
    else if (m_shrubs.size() > MaxShrubs)
    {
        m_shrubs.resize(MaxShrubs);
    }

    m_menuEntities[MenuID::Shrubbery].addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    m_menuEntities[MenuID::Shrubbery].addComponent<UIElement>().absolutePosition = { RootOffset, -8.f - ItemSpacing };
    m_menuEntities[MenuID::Shrubbery].getComponent<UIElement>().relativePosition = { 0.f, TabAreaHeight };
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().setScale(glm::vec2(0.f));


    auto scrollNode = m_uiScene.createEntity();
    scrollNode.addComponent<cro::Transform>();
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(scrollNode.getComponent<cro::Transform>());

    //scroll callbacks
    ScrollData scrollData(static_cast<std::int32_t>(m_shrubs.size()));

    scrollData.basePosition = scrollNode.getComponent<cro::Transform>().getPosition();

    scrollNode.addComponent<cro::Callback>().setUserData<ScrollData>(scrollData);
    scrollNode.getComponent<cro::Callback>().active = true;
    scrollNode.getComponent<cro::Callback>().function = ScrollNodeCallback();

    m_callbacks[CallbackID::ShrubScrollDown] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            data.targetIndex = std::min(data.targetIndex + 1, data.itemCount - 1);
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        }
    };
    m_callbacks[CallbackID::ShrubScrollUp] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            if (data.currIndex > 0)
            {
                data.targetIndex = data.currIndex - 1;
            }
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        }
    };

    //scroll buttons
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_up");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();

    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, -std::floor(bounds.height * 1.25f) };
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    auto buttonEnt = entity;
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Shrubbery);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::ShrubScrollUp]);
    auto buttonTop = buttonEnt;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_down");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, std::floor(bounds.height * 2.f) + ItemSpacing };
    entity.getComponent<UIElement>().relativePosition = { 1.f, -TabAreaHeight };
    buttonEnt = entity;
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Shrubbery);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::ShrubScrollDown]);
    auto buttonBottom = buttonEnt;

    //slider graphic
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("slider");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<SliderData>(scrollNode, buttonTop, buttonBottom, bounds);
    entity.getComponent<cro::Callback>().function = SliderCallback();
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_sliders[MenuID::Shrubbery].push_back(entity);

    //title
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(0.f, ItemSpacing + 2.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Foliage");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //list of shrub items
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    glm::vec2 position(0.f);
    for (auto i = 0u; i < m_shrubs.size(); ++i)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(m_shrubs[i]);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = ListItemCallback(m_croppingArea);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::ScoreScroll;

        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Shrubbery);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&, i, scrollNode](cro::Entity e) mutable
                {
                    e.getComponent<cro::Text>().setFillColour(TextGoldColour);
                    auto pos = e.getComponent<cro::Transform>().getWorldPosition();
                    pos.y -= ItemSpacing / 2.f;
                    if (!m_croppingArea.contains(pos))
                    {
                        cro::ButtonEvent fakeEvent;
                        fakeEvent.type = SDL_MOUSEBUTTONDOWN;
                        fakeEvent.button.button = SDL_BUTTON_LEFT;

                        if (pos.y < m_croppingArea.bottom)
                        {
                            m_callbacks[CallbackID::ShrubScrollDown](cro::Entity(), fakeEvent);
                        }
                        else if (pos.y > (m_croppingArea.bottom + m_croppingArea.height))
                        {
                            scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex = i;
                        }
                    }
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                });
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.textUnselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&, i](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt)
                        && m_shrubIndex != i)
                    {
                        for (auto& shrub : m_shrubberyModels)
                        {
                            shrub.hide();
                        }

                        m_shrubIndex = i;
                        m_courseData.shrubPath = ShrubPath + m_shrubs[i];
                        applyShrubQuality();
                        updateInfo();

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                });

        position.y -= ItemSpacing;
        scrollNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        loadShrubbery(ShrubPath + m_shrubs[i]);
        m_shrubberyModels[i].hide();
    }

    //load default shrub if found
    if (!m_shrubs.empty())
    {
        m_shrubIndex = 0;
        m_courseData.shrubPath = ShrubPath + m_shrubs[m_shrubIndex];

        applyShrubQuality();
    }

    //buttons for toggling quality setting
    static const std::array<std::string, 3u> Labels =
    {
        "Classic", "Low", "High"
    };
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Tree Quality");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.5f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { -RootOffset, 38.f + ItemSpacing };
    centreText(entity);
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(Labels[m_sharedData.treeQuality]);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.5f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { -RootOffset, 24.f + ItemSpacing };
    centreText(entity);
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto labelEnt = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_left");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.5f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { -30.f - RootOffset, 16.f + ItemSpacing };
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto scrollEnt = entity;

    bounds = menuData.spriteSheet->getSprite("scroll_highlight").getTextureBounds();
    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Shrubbery);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, labelEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.treeQuality = (m_sharedData.treeQuality + 2) % 3;
                    labelEnt.getComponent<cro::Text>().setString(Labels[m_sharedData.treeQuality]);
                    centreText(labelEnt);
                    applyShrubQuality();

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    scrollEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_right");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.5f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { 21.f - RootOffset, 16.f + ItemSpacing };
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    scrollEnt = entity;

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Shrubbery);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, labelEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.treeQuality = (m_sharedData.treeQuality + 1) % 3;
                    labelEnt.getComponent<cro::Text>().setString(Labels[m_sharedData.treeQuality]);
                    centreText(labelEnt);
                    applyShrubQuality();

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    scrollEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //chequered bg
    auto& tex = m_resources.textures.get("assets/golf/images/ed_bg.png");
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -RootOffset / 4.f, 2.f, -0.1f });
    entity.addComponent<cro::Drawable2D>().setTexture(&tex);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MessageBoard;
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        glm::vec2 texScale(e.getComponent<cro::Drawable2D>().getTexture()->getSize());
        texScale *= m_viewScale;

        e.getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f, m_croppingArea.height / texScale.y)),
                cro::Vertex2D(glm::vec2(0.f, -m_croppingArea.height / m_viewScale.y), glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2(m_croppingArea.width / m_viewScale.x, 0.f), glm::vec2(m_croppingArea.width / texScale.x, m_croppingArea.height / texScale.y)),
                cro::Vertex2D(glm::vec2(m_croppingArea.width / m_viewScale.x, -m_croppingArea.height / m_viewScale.y), glm::vec2(m_croppingArea.width / texScale.x, 0.f))
            });

        e.getComponent<cro::Callback>().active = false;
    };

    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void PlaylistState::createHoleMenu(cro::Entity rootNode, const MenuData& menuData)
{
    m_menuEntities[MenuID::Holes] = m_uiScene.createEntity();
    rootNode.getComponent<cro::Transform>().addChild(m_menuEntities[MenuID::Holes].addComponent<cro::Transform>());

    //read course dir and only add directory if it contains .hole files which have a corresponding thumbnail
    auto holeDirs = cro::FileSystem::listDirectories(cro::FileSystem::getResourcePath() + CoursePath);
    std::sort(holeDirs.begin(), holeDirs.end());

    if (holeDirs.size() > MaxCourses)
    {
        holeDirs.resize(MaxCourses);
    }

    for (const auto& dir : holeDirs)
    {
        auto files = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + CoursePath + dir);
        files.erase(std::remove_if(files.begin(), files.end(),
            [](const std::string& file)
            {
                return cro::FileSystem::getFileExtension(file) != ".hole";
            }), files.end());
        std::sort(files.begin(), files.end());

        if (files.size() > MaxHoles)
        {
            files.resize(MaxHoles);
        }

        //check if thumb exists and only add if it does.
        //DON'T FORGET YOU HAVE TO MANUALLY GENERATE THE THUMBS!!
        std::vector<std::string> thumbs;
        for (const auto& file : files)
        {
            auto thumb = file.substr(0, file.find_last_of('.')) + ".png";
            auto thumbPath = cro::FileSystem::getResourcePath() + ThumbPath + dir + "/" + thumb;
            if (cro::FileSystem::fileExists(thumbPath))
            {
                thumbs.push_back(thumb);
            }
        }

        if (!thumbs.empty())
        {
            auto& holeDir = m_holeDirs.emplace_back();
            holeDir.name = dir;
            
            for (auto& thumb : thumbs)
            {
                holeDir.holes.emplace_back().name.swap(thumb);
            }
        }
    }


    if (m_holeDirs.empty())
    {
        m_holeDirs.emplace_back().name = "None Found.";

        //add a dummy ent to stop crashing when the tab is selected
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::UIInput>().setGroup(MenuID::Holes);

        return;
    }

    //scroll node for directory list
    m_menuEntities[MenuID::Holes].addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    m_menuEntities[MenuID::Holes].addComponent<UIElement>().absolutePosition = { RootOffset, -8.f - ItemSpacing };
    m_menuEntities[MenuID::Holes].getComponent<UIElement>().relativePosition = { 0.f, TabAreaHeight };

    auto scrollNode = m_uiScene.createEntity();
    scrollNode.addComponent<cro::Transform>();
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(scrollNode.getComponent<cro::Transform>());

    //scroll callbacks
    ScrollData scrollData(static_cast<std::int32_t>(m_holeDirs.size()));
    scrollData.basePosition = scrollNode.getComponent<cro::Transform>().getPosition();

    scrollNode.addComponent<cro::Callback>().setUserData<ScrollData>(scrollData);
    scrollNode.getComponent<cro::Callback>().active = true;
    scrollNode.getComponent<cro::Callback>().function = ScrollNodeCallback();

    m_callbacks[CallbackID::HoleDirScrollDown] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            data.targetIndex = std::min(data.targetIndex + 1, data.itemCount - 1);
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        }
    };
    m_callbacks[CallbackID::HoleDirScrollUp] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            if (data.currIndex > 0)
            {
                data.targetIndex = data.currIndex - 1;
            }
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        }
    };


    //thumbnail label
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].name);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.5f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { -RootOffset, 24.f + ItemSpacing };
    centreText(entity);
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto labelEnt = entity;


    //title
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(0.f, ItemSpacing + 2.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Courses");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //create the list of directories
    glm::vec2 position(0.f);
    for (auto i = 0u; i < m_holeDirs.size(); ++i)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString(m_holeDirs[i].name);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = ListItemCallback(m_croppingArea);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::ScoreScroll;

        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&, i, scrollNode](cro::Entity e) mutable
                {
                    e.getComponent<cro::Text>().setFillColour(TextGoldColour);
                    auto pos = e.getComponent<cro::Transform>().getWorldPosition();
                    pos.y -= ItemSpacing / 2.f;
                    if (!m_croppingArea.contains(pos))
                    {
                        cro::ButtonEvent fakeEvent;
                        fakeEvent.type = SDL_MOUSEBUTTONDOWN;
                        fakeEvent.button.button = SDL_BUTTON_LEFT;

                        if (pos.y < m_croppingArea.bottom)
                        {
                            m_callbacks[CallbackID::SkyScrollDown](cro::Entity(), fakeEvent);
                        }
                        else if (pos.y > (m_croppingArea.bottom + m_croppingArea.height))
                        {
                            scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex = i;
                        }
                    }
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                });
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.textUnselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&, i, labelEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt))
                    {
                        //hide current thumb, reset index
                        m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].thumbEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        m_thumbnailIndex = 0;

                        //hide current menu, set current directory index, show new menu
                        m_holeDirs[m_holeDirIndex].thumbEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        m_holeDirIndex = i;
                        m_holeDirs[m_holeDirIndex].thumbEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                        m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].thumbEnt.getComponent<cro::Transform>().setScale(
                            m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].defaultScale);

                        labelEnt.getComponent<cro::Text>().setString(m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].name);
                        centreText(labelEnt);

                        updateInfo();

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                });

        position.y -= ItemSpacing;
        scrollNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


        //scroll node to which to attach thumbs
        m_holeDirs[i].thumbEnt = m_uiScene.createEntity();
        m_holeDirs[i].thumbEnt.addComponent<cro::Transform>().setScale(glm::vec2(0.f));

        m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(m_holeDirs[i].thumbEnt.getComponent<cro::Transform>());

        for (auto j = 0u; j < m_holeDirs[i].holes.size(); ++j)
        {
            //for each directory create a list of thumbnails
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>(m_resources.textures.get(ThumbPath + m_holeDirs[i].name + "/" + m_holeDirs[i].holes[j].name));
            
            entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
            entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.f };
            entity.getComponent<UIElement>().absolutePosition = { -RootOffset, 0.f };

            auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
            entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height, 0.f });

            m_holeDirs[i].holes[j].thumbEnt = entity;
            m_holeDirs[i].thumbEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }

    //show first entry
    m_holeDirs[m_holeDirIndex].thumbEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].thumbEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));


    //scroll directory buttons
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_up");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();

    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { ThumbnailOffset - (bounds.width * 2.f), -std::floor(bounds.height * 1.25f) };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 0.f };
    auto buttonEnt = entity;
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::HoleDirScrollUp]);
    auto buttonTop = buttonEnt;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_down");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = {ThumbnailOffset - (bounds.width * 2.f), std::floor(bounds.height * 2.f) + ItemSpacing};
    entity.getComponent<UIElement>().relativePosition = { 0.f, -TabAreaHeight };
    buttonEnt = entity;
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::HoleDirScrollDown]);
    auto buttonBottom = buttonEnt;

    //slider graphic
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("slider");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<SliderData>(scrollNode, buttonTop, buttonBottom, bounds);
    entity.getComponent<cro::Callback>().function = SliderCallback();
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_sliders[MenuID::Holes].push_back(entity);



    //buttons to scroll thumbnails
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_left");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.5f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { -30.f - RootOffset, 16.f + ItemSpacing };
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto scrollEnt = entity;

    bounds = menuData.spriteSheet->getSprite("scroll_highlight").getTextureBounds();
    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, labelEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].thumbEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));

                    auto holeCount = m_holeDirs[m_holeDirIndex].holes.size();
                    m_thumbnailIndex = (m_thumbnailIndex + (holeCount - 1)) % holeCount;
                    labelEnt.getComponent<cro::Text>().setString(m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].name);
                    centreText(labelEnt);
                    
                    m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].thumbEnt.getComponent<cro::Transform>().setScale(
                        m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].defaultScale);

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    scrollEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_right");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.5f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { 21.f - RootOffset, 16.f + ItemSpacing};
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    scrollEnt = entity;

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, labelEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].thumbEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));

                    auto holeCount = m_holeDirs[m_holeDirIndex].holes.size();
                    m_thumbnailIndex = (m_thumbnailIndex + 1) % holeCount;
                    labelEnt.getComponent<cro::Text>().setString(m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].name);
                    centreText(labelEnt);

                    m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].thumbEnt.getComponent<cro::Transform>().setScale(
                        m_holeDirs[m_holeDirIndex].holes[m_thumbnailIndex].defaultScale);

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    scrollEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //playlist node
    auto sd = ScrollData(0);

    scrollNode = m_uiScene.createEntity();
    scrollNode.addComponent<cro::Transform>();
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(scrollNode.getComponent<cro::Transform>());
    sd.basePosition = scrollNode.getComponent<cro::Transform>().getPosition();
    scrollNode.addComponent<cro::Callback>().setUserData<ScrollData>(sd);
    scrollNode.getComponent<cro::Callback>().active = true;
    scrollNode.getComponent<cro::Callback>().function = ScrollNodeCallback();

    m_callbacks[CallbackID::PlaylistScrollDown] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            data.targetIndex = std::min(data.targetIndex + 1, data.itemCount - 1);
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        }
    };
    m_callbacks[CallbackID::PlaylistScrollUp] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            if (data.currIndex > 0)
            {
                data.targetIndex = data.currIndex - 1;
            }
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        }
    };
    m_playlistScrollNode = scrollNode;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 1.f, -TabAreaHeight / 2.5f };
    entity.getComponent<UIElement>().absolutePosition = { -ThumbnailOffset - 9.f, 9.f };
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto buttonNode = entity;

    //title
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Hole List");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    entity.getComponent<UIElement>().absolutePosition = { -PlaylistOffset - 4.f, ItemSpacing + 2.f };
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //move item up
    position = { 0.f, menuData.spriteSheet->getSprite("add_hole").getTextureBounds().height };
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("move_up");
    buttonNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto highlight = createHighlight(m_uiScene, *menuData.spriteSheet);
    bounds = highlight.getComponent<cro::Sprite>().getTextureBounds();
    highlight.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    highlight.addComponent<cro::UIInput>().area = bounds;
    highlight.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    if (!m_playlist.empty() &&
                        m_playlistIndex > 0)
                    {
                        auto temp = m_playlist[m_playlistIndex];
                        m_playlist[m_playlistIndex] = m_playlist[m_playlistIndex - 1];
                        m_playlist[m_playlistIndex - 1] = temp;
                        m_playlistIndex--;

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                        for (auto i = 0u; i < m_playlist.size(); ++i)
                        {
                            auto pos = m_playlist[i].uiNode.getComponent<cro::Transform>().getPosition();
                            pos.y = i * -ItemSpacing;
                            m_playlist[i].uiNode.getComponent<cro::Transform>().setPosition(pos);
                            m_playlist[i].currentIndex = i;
                        }
                    }
                }
            });
    entity.getComponent<cro::Transform>().addChild(highlight.getComponent<cro::Transform>());

    position.y = 0.f;

    //add item
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("add_hole");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    buttonNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    position.y -= bounds.height;

    m_playlistActivatedCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_playlist[m_playlistIndex].uiNode.getComponent<cro::Text>().setFillColour(TextNormalColour);
                auto posY = e.getComponent<cro::Transform>().getPosition().y;
                m_playlistIndex = static_cast<std::size_t>(-posY / ItemSpacing);
                //e.getComponent<cro::Text>().setFillColour(TextGoldColour);
                
                auto c = m_playlist[m_playlistIndex].courseIndex;
                auto h = m_playlist[m_playlistIndex].holeIndex;

                m_holePreview.getComponent<cro::Sprite>().setTexture(*m_holeDirs[c].holes[h].thumbEnt.getComponent<cro::Sprite>().getTexture());
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                updateInfo();
            }        
        });
    
    m_playlistSelectedCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, scrollNode](cro::Entity e) mutable
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            auto pos = e.getComponent<cro::Transform>().getWorldPosition();
            pos.y -= ItemSpacing / 2.f;
            if (!m_croppingArea.contains(pos))
            {
                cro::ButtonEvent fakeEvent;
                fakeEvent.type = SDL_MOUSEBUTTONDOWN;
                fakeEvent.button.button = SDL_BUTTON_LEFT;

                if (pos.y < m_croppingArea.bottom)
                {
                    m_callbacks[CallbackID::SkyScrollDown](cro::Entity(), fakeEvent);
                }
                else if (pos.y > (m_croppingArea.bottom + m_croppingArea.height))
                {
                    std::int32_t i = std::floor(e.getComponent<cro::Transform>().getPosition().y / -ItemSpacing);

                    scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex = i;
                }
            }
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
        });
    m_playlistUnselectedCallback = menuData.textUnselected;

    highlight = createHighlight(m_uiScene, *menuData.spriteSheet, false);
    bounds = highlight.getComponent<cro::Sprite>().getTextureBounds();
    highlight.getComponent<cro::Transform>().setPosition({ 4.f, 7.f });
    highlight.addComponent<cro::UIInput>().area = bounds;
    highlight.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, scrollNode](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (m_playlist.size() < MaxHoles)
                    {
                        if (!m_playlist.empty())
                        {
                            m_playlist[m_playlistIndex].uiNode.getComponent<cro::Text>().setFillColour(TextNormalColour);
                        }

                        m_playlistIndex = m_playlist.size();
                        float vertPos = -ItemSpacing * m_playlistIndex;

                        std::stringstream ss;
                        ss << std::setw(2) << std::setfill('0') << m_holeDirIndex + 1 << "_";


                        auto& entry = m_playlist.emplace_back();
                        entry.courseIndex = m_holeDirIndex;
                        entry.holeIndex = m_thumbnailIndex;
                        entry.currentIndex = m_playlistIndex;
                        entry.name = m_holeDirs[entry.courseIndex].holes[entry.holeIndex].name;
                        entry.uiNode = m_uiScene.createEntity();
                        entry.uiNode.addComponent<cro::Transform>().setPosition({ (cro::App::getWindow().getSize().x / m_viewScale.x) - PlaylistOffset, vertPos, 0.1f});
                        entry.uiNode.addComponent<cro::Drawable2D>();
                        entry.uiNode.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Info)).setString(ss.str() + entry.name);
                        entry.uiNode.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
                        entry.uiNode.getComponent<cro::Text>().setFillColour(TextGoldColour);
                        
                        entry.uiNode.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entry.uiNode);
                        entry.uiNode.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
                        entry.uiNode.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_playlistActivatedCallback;
                        entry.uiNode.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_playlistSelectedCallback;
                        entry.uiNode.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_playlistUnselectedCallback;
                            

                        entry.uiNode.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::ScoreScroll;
                        entry.uiNode.addComponent<UIElement>().depth = 0.1f;
                        entry.uiNode.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
                        entry.uiNode.getComponent<UIElement>().absolutePosition = { -PlaylistOffset, vertPos };

                        entry.uiNode.addComponent<cro::Callback>().active = true;
                        entry.uiNode.getComponent<cro::Callback>().function = ListItemCallback(m_croppingArea);

                        scrollNode.getComponent<cro::Transform>().addChild(entry.uiNode.getComponent<cro::Transform>());

                        //if(list is too long)
                        auto itemCount = static_cast<std::int32_t>(m_playlist.size());
                        scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().itemCount = itemCount;
                        if (entry.uiNode.getComponent<cro::Transform>().getWorldPosition().y < ItemSpacing * 2.f)
                        {
                            scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex = std::max(0, itemCount - 1);
                        }

                        auto c = m_playlist[m_playlistIndex].courseIndex;
                        auto h = m_playlist[m_playlistIndex].holeIndex;

                        m_holePreview.getComponent<cro::Sprite>().setTexture(*m_holeDirs[c].holes[h].thumbEnt.getComponent<cro::Sprite>().getTexture());

                        updateInfo();

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                }
            }
        );
    entity.getComponent<cro::Transform>().addChild(highlight.getComponent<cro::Transform>());


    //remove item
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("remove_hole");
    buttonNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    highlight = createHighlight(m_uiScene, *menuData.spriteSheet, false);
    bounds = highlight.getComponent<cro::Sprite>().getTextureBounds();
    highlight.getComponent<cro::Transform>().setPosition({ 4.f, 7.f });
    highlight.addComponent<cro::UIInput>().area = bounds;
    highlight.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, scrollNode](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (!m_playlist.empty())
                    {
                        m_uiScene.destroyEntity(m_playlist[m_playlistIndex].uiNode);
                        m_playlist.erase(m_playlist.begin() + m_playlistIndex);

                        auto itemCount = static_cast<std::int32_t>(m_playlist.size());
                        scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().itemCount = itemCount;

                        if (!m_playlist.empty())
                        {
                            m_playlistIndex = std::min(m_playlistIndex, m_playlist.size() - 1);
                            m_playlist[m_playlistIndex].uiNode.getComponent<cro::Text>().setFillColour(TextGoldColour);

                            if (m_playlist.back().uiNode.getComponent<cro::Transform>().getWorldPosition().y > m_croppingArea.bottom + m_croppingArea.height)
                            {
                                scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex = std::max(0, itemCount - 6);
                            }
                        }

                        for (auto i = 0u; i < m_playlist.size(); ++i)
                        {
                            auto pos = m_playlist[i].uiNode.getComponent<cro::Transform>().getPosition();
                            pos.y = i * -ItemSpacing;
                            m_playlist[i].uiNode.getComponent<cro::Transform>().setPosition(pos);
                            m_playlist[i].currentIndex = i;
                        }

                        updateInfo();

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    entity.getComponent<cro::Transform>().addChild(highlight.getComponent<cro::Transform>());



    //move item down
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("move_down");
    buttonNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().move({ 0.f, -bounds.height });

    highlight = createHighlight(m_uiScene, *menuData.spriteSheet);
    bounds = highlight.getComponent<cro::Sprite>().getTextureBounds();
    highlight.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    highlight.addComponent<cro::UIInput>().area = bounds;
    highlight.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    if (!m_playlist.empty() &&
                        m_playlistIndex < m_playlist.size() - 1)
                    {
                        auto temp = m_playlist[m_playlistIndex];
                        m_playlist[m_playlistIndex] = m_playlist[m_playlistIndex + 1];
                        m_playlist[m_playlistIndex + 1] = temp;
                        m_playlistIndex++;

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                        for (auto i = 0u; i < m_playlist.size(); ++i)
                        {
                            auto pos = m_playlist[i].uiNode.getComponent<cro::Transform>().getPosition();
                            pos.y = i * -ItemSpacing;
                            m_playlist[i].uiNode.getComponent<cro::Transform>().setPosition(pos);
                            m_playlist[i].currentIndex = i;
                        }
                    }
                }
            });
    entity.getComponent<cro::Transform>().addChild(highlight.getComponent<cro::Transform>());

    //clear play list
    position.y -= 18.f;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("clear_list");
    buttonNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().move({ 0.f, -bounds.height });

    highlight = createHighlight(m_uiScene, *menuData.spriteSheet);
    bounds = highlight.getComponent<cro::Sprite>().getTextureBounds();
    highlight.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    highlight.addComponent<cro::UIInput>().area = bounds;
    highlight.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    highlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    for (auto& e : m_playlist)
                    {
                        m_uiScene.destroyEntity(e.uiNode);
                    }
                    m_playlist.clear();
                    m_playlistIndex = 0;

                    updateInfo();

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    entity.getComponent<cro::Transform>().addChild(highlight.getComponent<cro::Transform>());

    //scrolls current playlist
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_up");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();

    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, -std::floor(bounds.height * 1.25f) };
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    buttonEnt = entity;
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::PlaylistScrollUp]);
    buttonTop = buttonEnt;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_down");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, std::floor(bounds.height * 2.f) + ItemSpacing};
    entity.getComponent<UIElement>().relativePosition = { 1.f, -TabAreaHeight };
    buttonEnt = entity;
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::PlaylistScrollDown]);
    buttonBottom = buttonEnt;


    //slider graphic
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("slider");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<SliderData>(scrollNode, buttonTop, buttonBottom, bounds);
    entity.getComponent<cro::Callback>().function = SliderCallback();
    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_sliders[MenuID::Holes].push_back(entity);

    //chequered bg
    auto& tex = m_resources.textures.get("assets/golf/images/ed_bg.png");
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -RootOffset / 4.f, 2.f, -0.1f });
    entity.addComponent<cro::Drawable2D>().setTexture(&tex);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MessageBoard;
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        glm::vec2 texScale(e.getComponent<cro::Drawable2D>().getTexture()->getSize());
        texScale *= m_viewScale;

        e.getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f, m_croppingArea.height / texScale.y)),
                cro::Vertex2D(glm::vec2(0.f, -m_croppingArea.height / m_viewScale.y), glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2(ThumbnailOffset, 0.f), glm::vec2(ThumbnailOffset * m_viewScale.x / texScale.x, m_croppingArea.height / texScale.y)),
                cro::Vertex2D(glm::vec2(ThumbnailOffset, -m_croppingArea.height / m_viewScale.y), glm::vec2(ThumbnailOffset * m_viewScale.x / texScale.x, 0.f)),

                cro::Vertex2D(glm::vec2(ThumbnailOffset, 0.f), glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2(ThumbnailOffset, -m_croppingArea.height / m_viewScale.y), glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2((m_croppingArea.width / m_viewScale.x) - ThumbnailOffset, 0.f), glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2((m_croppingArea.width / m_viewScale.x) - ThumbnailOffset, -m_croppingArea.height / m_viewScale.y), glm::vec2(0.f)),

                cro::Vertex2D(glm::vec2((m_croppingArea.width / m_viewScale.x) - ThumbnailOffset, 0.f), glm::vec2(0.f, m_croppingArea.height / texScale.y)),
                cro::Vertex2D(glm::vec2((m_croppingArea.width / m_viewScale.x) - ThumbnailOffset, -m_croppingArea.height / m_viewScale.y), glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2((m_croppingArea.width / m_viewScale.x), 0.f), glm::vec2(ThumbnailOffset* m_viewScale.x / texScale.x, m_croppingArea.height / texScale.y)),
                cro::Vertex2D(glm::vec2((m_croppingArea.width / m_viewScale.x), -m_croppingArea.height / m_viewScale.y), glm::vec2(ThumbnailOffset* m_viewScale.x / texScale.x, 0.f))
            });

        e.getComponent<cro::Callback>().active = false;
    };

    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
}

void PlaylistState::createFileSystemMenu(cro::Entity rootNode, const MenuData& menuData)
{
    m_menuEntities[MenuID::FileSystem] = m_uiScene.createEntity();
    rootNode.getComponent<cro::Transform>().addChild(m_menuEntities[MenuID::FileSystem].addComponent<cro::Transform>());

    const std::string saveDir = cro::App::getPreferencePath() + UserCoursePath;
    if (!cro::FileSystem::directoryExists(saveDir))
    {
        cro::FileSystem::createDirectory(saveDir);
    }

    auto saves = cro::FileSystem::listFiles(saveDir);
    saves.erase(std::remove_if(saves.begin(), saves.end(), 
        [](const std::string& s)
        {
            return cro::FileSystem::getFileExtension(s) != SaveFileExtension;
        }), saves.end());

    if (saves.size() > MaxSaves)
    {
        saves.resize(MaxSaves);
    }
    m_saveFiles.swap(saves);

    //scroll saves list
    m_menuEntities[MenuID::FileSystem].addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    m_menuEntities[MenuID::FileSystem].addComponent<UIElement>().absolutePosition = { RootOffset, -8.f - ItemSpacing };
    m_menuEntities[MenuID::FileSystem].getComponent<UIElement>().relativePosition = { 0.f, TabAreaHeight };

    auto scrollNode = m_uiScene.createEntity();
    scrollNode.addComponent<cro::Transform>();
    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().addChild(scrollNode.getComponent<cro::Transform>());

    //scroll callbacks
    ScrollData scrollData(static_cast<std::int32_t>(m_saveFiles.size()));

    scrollData.basePosition = scrollNode.getComponent<cro::Transform>().getPosition();

    scrollNode.addComponent<cro::Callback>().setUserData<ScrollData>(scrollData);
    scrollNode.getComponent<cro::Callback>().active = true;
    scrollNode.getComponent<cro::Callback>().function = ScrollNodeCallback();
    m_saveFileScrollNode = scrollNode;

    m_callbacks[CallbackID::SaveScrollDown] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            data.targetIndex = std::min(data.targetIndex + 1, data.itemCount - 1);
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        }
    };
    m_callbacks[CallbackID::SaveScrollUp] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            if (data.currIndex > 0)
            {
                data.targetIndex = data.currIndex - 1;
            }
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        }
    };

    //scroll buttons
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_up");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, -std::floor(bounds.height * 1.25f) };
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    auto buttonEnt = entity;
    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f, });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::FileSystem);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::SaveScrollUp]);
    auto buttonTop = buttonEnt;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_down");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, std::floor(bounds.height * 2.f) + ItemSpacing };
    entity.getComponent<UIElement>().relativePosition = { 1.f, -TabAreaHeight };
    buttonEnt = entity;
    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f, });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::FileSystem);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::SaveScrollDown]);
    auto buttonBottom = buttonEnt;

    //slider graphic
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("slider");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<SliderData>(scrollNode, buttonTop, buttonBottom, bounds);
    entity.getComponent<cro::Callback>().function = SliderCallback();
    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_sliders[MenuID::FileSystem].push_back(entity);

    //chequed background
    auto& tex = m_resources.textures.get("assets/golf/images/ed_bg.png");
    tex.setRepeated(true);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -RootOffset / 4.f, 2.f, -0.1f });
    entity.addComponent<cro::Drawable2D>().setTexture(&tex);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MessageBoard;
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        glm::vec2 texScale(e.getComponent<cro::Drawable2D>().getTexture()->getSize());
        texScale *= m_viewScale;

        e.getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f, m_croppingArea.height / texScale.y)),
                cro::Vertex2D(glm::vec2(0.f, -m_croppingArea.height / m_viewScale.y), glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2(m_croppingArea.width / m_viewScale.x, 0.f), glm::vec2(m_croppingArea.width / texScale.x, m_croppingArea.height / texScale.y)),
                cro::Vertex2D(glm::vec2(m_croppingArea.width / m_viewScale.x, -m_croppingArea.height / m_viewScale.y), glm::vec2(m_croppingArea.width / texScale.x, 0.f))
            });

        e.getComponent<cro::Callback>().active = false;
    };

    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //title
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(0.f, ItemSpacing + 2.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Save/Load");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //list of save file items
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    glm::vec2 position(0.f);
    for (auto i = 0u; i < m_saveFiles.size(); ++i)
    {
        addSaveFileItem(i, position);
        position.y -= ItemSpacing;
    }

    //save button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Save Course");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.35f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { -RootOffset, 28.f + ItemSpacing };
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::FileSystem);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.textSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.textUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt)
                    && !m_playlist.empty())
                {
                    confirmSave();
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    centreText(entity);
    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //export button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Export To Game");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.65f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { -RootOffset, 28.f + ItemSpacing };
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::FileSystem);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.textSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.textUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt)
                    && !m_playlist.empty())
                {
                    showExportResult(exportCourse());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    centreText(entity);
    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //brwose filesystem button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Show On Disk");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width - 18.f, ItemSpacing + 2.f };
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::FileSystem);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.textSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.textUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt)
                    && !m_playlist.empty())
                {
                    auto courseDir = cro::App::getPreferencePath() + UserCoursePath;
                    if (!cro::FileSystem::directoryExists(courseDir))
                    {
                        cro::FileSystem::createDirectory(courseDir);
                    }

                    auto exportDir = cro::App::getPreferencePath() + UserCourseExport;
                    if (!cro::FileSystem::directoryExists(exportDir))
                    {
                        cro::FileSystem::createDirectory(exportDir);
                    }

                    cro::Util::String::parseURL(exportDir);
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });

    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().setScale(glm::vec2(0.f));

    if (!m_saveFiles.empty())
    {
        //delay this one frame to make sure everything is set up
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<std::int32_t>(2);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            auto& count = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
            count--;

            if (count == 0)
            {
                loadCourse();
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
        };
    }
}

void PlaylistState::addSaveFileItem(std::size_t i, glm::vec2 position)
{
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(m_saveFiles[i]);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = ListItemCallback(m_croppingArea);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::ScoreScroll;

    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::FileSystem);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, i](cro::Entity e) mutable
            {
                e.getComponent<cro::Text>().setFillColour(TextGoldColour);
                auto pos = e.getComponent<cro::Transform>().getWorldPosition();
                pos.y -= ItemSpacing / 2.f;
                if (!m_croppingArea.contains(pos))
                {
                    cro::ButtonEvent fakeEvent;
                    fakeEvent.type = SDL_MOUSEBUTTONDOWN;
                    fakeEvent.button.button = SDL_BUTTON_LEFT;

                    if (pos.y < m_croppingArea.bottom)
                    {
                        m_callbacks[CallbackID::SkyScrollDown](cro::Entity(), fakeEvent);
                    }
                    else if (pos.y > (m_croppingArea.bottom + m_croppingArea.height))
                    {
                        m_saveFileScrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex = static_cast<std::int32_t>(i);
                    }
                }
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_popupIDs[PopupID::TextUnselected];
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, i](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    confirmLoad(i);
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });

    m_saveFileScrollNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_saveFileScrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().itemCount = static_cast<std::int32_t>(m_saveFiles.size());
}

void PlaylistState::setActiveTab(std::int32_t index)
{
    if (helpOverlay.isValid())
    {
        return;
    }

    index %= MenuID::Count -1;
    m_currentTab = index;

    for (auto i = 0; i < MenuID::Popup; ++i)
    {
        auto tabIndex = m_tabEntities[i].getComponent<cro::Callback>().getUserData<std::int32_t>();
        if (tabIndex == index)
        {
            m_tabEntities[i].getComponent<cro::UIInput>().setGroup(MenuID::Dummy);
            m_tabEntities[i].getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            m_menuEntities[i].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }
        else
        {
            m_tabEntities[i].getComponent<cro::UIInput>().setGroup(index);
            m_menuEntities[i].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
    }

    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(index);

    //disable callbacks on hidden items
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::ScoreScroll;
    cmd.action = [index](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().active =
            e.getComponent<cro::UIInput>().getGroup() == index;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //we've stashed the tab graphic entity in here.
    m_tabEntities[MenuID::Dummy].getComponent<cro::SpriteAnimation>().play(m_animationIDs[index]);
}

void PlaylistState::loadShrubbery(const std::string& path)
{
    auto& shrubbery = m_shrubberyModels.emplace_back();
    
    //parse shrub file and look for valid paths
    cro::ConfigFile shrubFile;
    if (shrubFile.loadFromFile(path))
    {
        std::string billboardModel;
        std::string billboardSprite;
        std::vector<std::string> treesetPaths;
        constexpr std::size_t MaxTreesets = 4;

        const auto& props = shrubFile.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "model")
            {
                billboardModel = p.getValue<std::string>();
            }
            else if (name == "sprite")
            {
                billboardSprite = p.getValue<std::string>();
            }
            else if (name == "treeset")
            {
                if (treesetPaths.size() < MaxTreesets)
                {
                    treesetPaths.push_back(p.getValue<std::string>());
                }
            }
        }

        //TODO do we want to bail if some info is missing? or just load what we can?
        auto setBillboards = [&](const std::string& modelPath, const std::string& spritePath, std::size_t outIndex)
        {
            cro::SpriteSheet spriteSheet;
            if (spriteSheet.loadFromFile(spritePath, m_resources.textures))
            {
                std::array<cro::Billboard, BillboardID::Count> billboardTemplates = {};
                billboardTemplates[BillboardID::Grass01] = spriteToBillboard(spriteSheet.getSprite("grass01"));
                billboardTemplates[BillboardID::Grass02] = spriteToBillboard(spriteSheet.getSprite("grass02"));
                billboardTemplates[BillboardID::Flowers01] = spriteToBillboard(spriteSheet.getSprite("flowers01"));
                billboardTemplates[BillboardID::Flowers02] = spriteToBillboard(spriteSheet.getSprite("flowers02"));
                billboardTemplates[BillboardID::Flowers03] = spriteToBillboard(spriteSheet.getSprite("flowers03"));
                billboardTemplates[BillboardID::Bush01] = spriteToBillboard(spriteSheet.getSprite("hedge01"));
                billboardTemplates[BillboardID::Bush02] = spriteToBillboard(spriteSheet.getSprite("hedge02"));

                billboardTemplates[BillboardID::Tree01] = spriteToBillboard(spriteSheet.getSprite("tree01"));
                billboardTemplates[BillboardID::Tree02] = spriteToBillboard(spriteSheet.getSprite("tree02"));
                billboardTemplates[BillboardID::Tree03] = spriteToBillboard(spriteSheet.getSprite("tree03"));
                billboardTemplates[BillboardID::Tree04] = spriteToBillboard(spriteSheet.getSprite("tree04"));

                cro::ModelDefinition md(m_resources);
                if (md.loadFromFile(modelPath))
                {
                    auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>();
                    md.createModel(entity);

                    auto billboardMat = m_resources.materials.get(m_materialIDs[MaterialID::Billboard]);
                    applyMaterialData(md, billboardMat);
                    entity.getComponent<cro::Model>().setMaterial(0, billboardMat);

                    if (entity.hasComponent<cro::BillboardCollection>())
                    {
                        auto& collection = entity.getComponent<cro::BillboardCollection>();

                        for (auto pos : m_grassDistribution)
                        {
                            float scale = static_cast<float>(cro::Util::Random::value(8, 11)) / 10.f;

                            auto bb = billboardTemplates[cro::Util::Random::value(BillboardID::Grass01, BillboardID::Grass02)];
                            bb.position = pos;
                            bb.size *= scale;
                            collection.addBillboard(bb);
                        }

                        for (auto pos : m_flowerDistribution)
                        {
                            float scale = static_cast<float>(cro::Util::Random::value(8, 11)) / 10.f;

                            auto bb = billboardTemplates[cro::Util::Random::value(BillboardID::Flowers01, BillboardID::Bush02)];
                            bb.position = pos;
                            bb.size *= scale;
                            collection.addBillboard(bb);
                        }
                    }
                    shrubbery.billboardEnts[outIndex] = entity;


                    auto billboardShadowMat = m_resources.materials.get(m_materialIDs[MaterialID::BillboardShadow]);
                    applyMaterialData(md, billboardShadowMat);
                    billboardShadowMat.doubleSided = true; //do this second because applyMaterial() overwrites it
                    entity.getComponent<cro::Model>().setShadowMaterial(0, billboardShadowMat);
                    if (!entity.hasComponent<cro::ShadowCaster>())
                    {
                        entity.addComponent<cro::ShadowCaster>();
                    }

                    //trees are separate as we might want treesets instead
                    md.loadFromFile(modelPath); //reload to create unique VBO
                    entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>();
                    md.createModel(entity);
                    entity.getComponent<cro::Model>().setMaterial(0, billboardMat);

                    entity.getComponent<cro::Model>().setShadowMaterial(0, billboardShadowMat);
                    if (!entity.hasComponent<cro::ShadowCaster>())
                    {
                        entity.addComponent<cro::ShadowCaster>();
                    }
                    
                    if (entity.hasComponent<cro::BillboardCollection>())
                    {
                        auto& collection = entity.getComponent<cro::BillboardCollection>();

                        for (auto i = 0u; i < m_treeDistribution.size(); ++i)
                        {
                            auto pos = m_treeDistribution[i];
                            float scale = static_cast<float>(cro::Util::Random::value(12, 22)) / 10.f;

                            auto idx = BillboardID::Tree01 + (i % ((BillboardID::Tree04 + 1) - BillboardID::Tree01));
                            auto bb = billboardTemplates[idx];
                            bb.position = pos;
                            bb.size *= scale;
                            collection.addBillboard(bb);
                        }
                    }
                    shrubbery.treeBillboardEnts[outIndex] = entity;
                }
            }
        };

        setBillboards(billboardModel, billboardSprite, 0);

        //check if a low quality version is available and load that, else duplicate the
        //current billboard ents into the low Q slot.
        auto billboardModelLow = billboardModel.substr(0, billboardModel.find_last_of('.')) + "_low.cmt";
        auto billboardSpriteLow = billboardSprite.substr(0, billboardSprite.find_last_of('.')) + "_low.spt";

        if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + billboardModelLow)
            && cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + billboardSpriteLow))
        {
            setBillboards(billboardModelLow, billboardSpriteLow, 1);
        }
        else
        {
            shrubbery.billboardEnts[1] = shrubbery.billboardEnts[0];
            shrubbery.treeBillboardEnts[1] = shrubbery.treeBillboardEnts[0];
        }

        //load tree sets
        std::vector<Treeset> treesets;
        std::vector<std::vector<glm::mat4>> transforms;
        for (const auto& ts : treesetPaths)
        {
            Treeset treeset;
            if (treeset.loadFromFile(ts))
            {
                transforms.emplace_back();
                treesets.push_back(treeset);
            }
        }

        cro::ModelDefinition md(m_resources);
        for (auto i = 0u; i < MaxTreesets; ++i)
        {
            if (i < treesets.size())
            {
                //load treeset
                if (md.loadFromFile(treesets[i].modelPath), true)
                {
                    shrubbery.treesetEnts[i] = m_gameScene.createEntity();
                    shrubbery.treesetEnts[i].addComponent<cro::Transform>();
                    md.createModel(shrubbery.treesetEnts[i]);

                    for (auto idx : treesets[i].branchIndices)
                    {
                        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Branch]);
                        applyMaterialData(md, material, idx);
                        shrubbery.treesetEnts[i].getComponent<cro::Model>().setMaterial(idx, material);

                        material = m_resources.materials.get(m_materialIDs[MaterialID::BranchShadow]);
                        applyMaterialData(md, material, idx);
                        shrubbery.treesetEnts[i].getComponent<cro::Model>().setShadowMaterial(idx, material);
                    }

                    auto& meshData = shrubbery.treesetEnts[i].getComponent<cro::Model>().getMeshData();
                    for (auto idx : treesets[i].leafIndices)
                    {
                        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Leaf]);
                        meshData.indexData[idx].primitiveType = GL_POINTS;
                        material.setProperty("u_diffuseMap", m_resources.textures.get(treesets[i].texturePath));
                        material.setProperty("u_leafSize", treesets[i].leafSize);
                        material.setProperty("u_randAmount", treesets[i].randomness);
                        material.setProperty("u_colour", treesets[i].colour);
                        shrubbery.treesetEnts[i].getComponent<cro::Model>().setMaterial(idx, material);

                        material = m_resources.materials.get(m_materialIDs[MaterialID::LeafShadow]);
                        if (m_sharedData.hqShadows)
                        {
                            material.setProperty("u_diffuseMap", m_resources.textures.get(treesets[i].texturePath));
                        }
                        material.setProperty("u_leafSize", treesets[i].leafSize);
                        shrubbery.treesetEnts[i].getComponent<cro::Model>().setShadowMaterial(idx, material);
                    }

                    transforms.emplace_back();
                }
            }
            else
            {
                //create billboard in place
                if (md.loadFromFile(billboardModel))
                {
                    shrubbery.treesetEnts[i] = m_gameScene.createEntity();
                    shrubbery.treesetEnts[i].addComponent<cro::Transform>();
                    md.createModel(shrubbery.treesetEnts[i]);
                    
                    auto billboardMat = m_resources.materials.get(m_materialIDs[MaterialID::Billboard]);
                    applyMaterialData(md, billboardMat);
                    shrubbery.treesetEnts[i].getComponent<cro::Model>().setMaterial(0, billboardMat);
                }
            }
        }

        cro::SpriteSheet spriteSheet;
        spriteSheet.loadFromFile(billboardSprite, m_resources.textures);

        for (auto i = 0u; i < m_treeDistribution.size(); ++i)
        {
            auto idx = i % MaxTreesets;
            if (idx < treesets.size())
            {
                //add to instance positions
                auto tx = glm::translate(glm::mat4(1.f), m_treeDistribution[i]);

                float scale = static_cast<float>(cro::Util::Random::value(16, 20)) / 10.f;
                tx = glm::scale(tx, glm::vec3(scale));

                transforms[idx].push_back(tx);
            }
            else
            {
                const std::array<std::string, 4u> TreeNames =
                {
                    "tree01",
                    "tree02",
                    "tree03",
                    "tree04",
                };

                //add a billboard
                auto bb = spriteToBillboard(spriteSheet.getSprite(TreeNames[idx]));
                bb.size *= static_cast<float>(cro::Util::Random::value(12, 22)) / 10.f;
                bb.position = m_treeDistribution[i];

                if (shrubbery.treesetEnts[idx].hasComponent<cro::BillboardCollection>())
                {
                    shrubbery.treesetEnts[idx].getComponent<cro::BillboardCollection>().addBillboard(bb);
                }
            }
        }

        for (auto i = 0u; i < treesets.size(); ++i)
        {
            shrubbery.treesetEnts[i].getComponent<cro::Model>().setInstanceTransforms(transforms[i]);
        }
    }

    //show/hide appropriate ent based on tree quality setting
    applyShrubQuality();
}

void PlaylistState::applyShrubQuality()
{
    auto& shrubbery = m_shrubberyModels[m_shrubIndex];

    switch (m_sharedData.treeQuality)
    {
    default: break;
    case SharedStateData::Classic:
        if (shrubbery.billboardEnts[0].isValid())
        {
            shrubbery.billboardEnts[0].getComponent<cro::Model>().setHidden(true);
            shrubbery.billboardEnts[1].getComponent<cro::Model>().setHidden(false);
        }

        if (shrubbery.treeBillboardEnts[0].isValid())
        {
            shrubbery.treeBillboardEnts[0].getComponent<cro::Model>().setHidden(true);
            shrubbery.treeBillboardEnts[1].getComponent<cro::Model>().setHidden(false);
        }

        for (auto e : shrubbery.treesetEnts)
        {
            if (e.isValid())
            {
                e.getComponent<cro::Model>().setHidden(true);
            }
        }
        break;
    case SharedStateData::Low:
        if (shrubbery.billboardEnts[0].isValid())
        {
            shrubbery.billboardEnts[0].getComponent<cro::Model>().setHidden(false);
            shrubbery.billboardEnts[1].getComponent<cro::Model>().setHidden(shrubbery.billboardEnts[1] != shrubbery.billboardEnts[0]);
        }

        if (shrubbery.treeBillboardEnts[0].isValid())
        {
            shrubbery.treeBillboardEnts[0].getComponent<cro::Model>().setHidden(false);
            shrubbery.treeBillboardEnts[1].getComponent<cro::Model>().setHidden(shrubbery.treeBillboardEnts[1] != shrubbery.treeBillboardEnts[0]);
        }

        for (auto e : shrubbery.treesetEnts)
        {
            if (e.isValid())
            {
                e.getComponent<cro::Model>().setHidden(true);
            }
        }
        break;
    case SharedStateData::High:
        if (shrubbery.billboardEnts[0].isValid())
        {
            shrubbery.billboardEnts[0].getComponent<cro::Model>().setHidden(false);
            shrubbery.billboardEnts[1].getComponent<cro::Model>().setHidden(shrubbery.billboardEnts[1] != shrubbery.billboardEnts[0]);
        }

        if (shrubbery.treeBillboardEnts[0].isValid())
        {
            //TODO how do we handle partial treesets (ie < 4) where we mix billboards and models?
            shrubbery.treeBillboardEnts[0].getComponent<cro::Model>().setHidden(!shrubbery.treesetEnts.empty());
            shrubbery.treeBillboardEnts[1].getComponent<cro::Model>().setHidden(true);
        }

        for (auto e : shrubbery.treesetEnts)
        {
            if (e.isValid())
            {
                e.getComponent<cro::Model>().setHidden(false);
            }
        }
        break;
    }
}

void PlaylistState::updateNinePatch(cro::Entity entity)
{
    //static constexpr float NinePatchSize = 4.f; //size of a 'patch' in the texture
    
    //0    2
    //|  / |
    //| /  |
    //1    3

    constexpr float PatchU = 1.f / 3.f;
    constexpr float PatchV = 1.f / 3.f;

    constexpr std::array<glm::vec2, 36u> PatchUVs =
    {
        //TL, Bl, TR, BR
        glm::vec2(0.f, 1.f),
        glm::vec2(0.f, PatchV * 2.f),
        glm::vec2(PatchU, 1.f),
        glm::vec2(PatchU, PatchV * 2.f),

        glm::vec2(0.f, PatchV),
        glm::vec2(0.f, 0.f),
        glm::vec2(PatchU, PatchV),
        glm::vec2(PatchU, 0.f),

        glm::vec2(PatchU * 2.f, 1.f),
        glm::vec2(PatchU * 2.f, PatchV * 2.f),
        glm::vec2(1.f, 1.f),
        glm::vec2(1.f, PatchV * 2.f),

        glm::vec2(PatchU * 2.f, PatchV),
        glm::vec2(PatchU * 2.f, 0.f),
        glm::vec2(1.f, PatchV),
        glm::vec2(1.f, 0.f),

        //T, B, L, R
        glm::vec2(PatchU, 1.f),
        glm::vec2(PatchU, PatchV * 2.f),
        glm::vec2(PatchU * 2.f, 1.f),
        glm::vec2(PatchU * 2.f, PatchV * 2.f),

        glm::vec2(PatchU, PatchV),
        glm::vec2(PatchU, 0.f),
        glm::vec2(PatchU * 2.f, PatchV),
        glm::vec2(PatchU * 2.f, 0.f),
        
        glm::vec2(0.f, PatchV * 2.f),
        glm::vec2(0.f, PatchV),
        glm::vec2(PatchU, PatchV * 2.f),
        glm::vec2(PatchU, PatchV),

        glm::vec2(PatchU * 2.f, PatchV * 2.f),
        glm::vec2(PatchU * 2.f, PatchV),
        glm::vec2(1.f, PatchV * 2.f),
        glm::vec2(1.f, PatchV),

        //Centre
        glm::vec2(PatchU, PatchV * 2.f),
        glm::vec2(PatchU, PatchV),
        glm::vec2(PatchU * 2.f, PatchV * 2.f),
        glm::vec2(PatchU * 2.f, PatchV),
    };

    glm::vec2 texSize = entity.getComponent<cro::Drawable2D>().getTexture()->getSize();
    const float PatchWidth = std::floor(texSize.x / 3.f);
    const float PatchHeight = std::floor(texSize.y / 3.f);

    auto size = glm::vec2(cro::App::getWindow().getSize()) / m_viewScale;
    size.x = std::floor(size.x);
    size.y = std::floor(size.y * TabAreaHeight);

    std::array<glm::vec2, 36> positions =
    {
        //TL, Bl, TR, BR
        glm::vec2(0.f, size.y),
        glm::vec2(0.f, size.y - PatchHeight),
        glm::vec2(PatchWidth, size.y),
        glm::vec2(PatchWidth, size.y - PatchHeight),

        glm::vec2(0.f, PatchHeight),
        glm::vec2(0.f, 0.f),
        glm::vec2(PatchWidth, PatchHeight),
        glm::vec2(PatchWidth, 0.f),

        glm::vec2(size.x - PatchWidth, size.y),
        glm::vec2(size.x - PatchWidth, size.y - PatchHeight),
        glm::vec2(size.x, size.y),
        glm::vec2(size.x, size.y - PatchHeight),

        glm::vec2(size.x - PatchWidth, PatchHeight),
        glm::vec2(size.x - PatchWidth, 0.f),
        glm::vec2(size.x, PatchHeight),
        glm::vec2(size.x, 0.f),

        //T, B, L, R
        glm::vec2(PatchWidth, size.y),
        glm::vec2(PatchWidth, size.y - PatchHeight),
        glm::vec2(size.x - PatchWidth, size.y),
        glm::vec2(size.x - PatchWidth, size.y - PatchHeight),

        glm::vec2(PatchWidth, PatchHeight),
        glm::vec2(PatchWidth, 0.f),
        glm::vec2(size.x - PatchWidth, PatchHeight),
        glm::vec2(size.x - PatchWidth, 0.f),

        glm::vec2(0.f, size.y - PatchHeight),
        glm::vec2(0.f, PatchHeight),
        glm::vec2(PatchWidth, size.y - PatchHeight),
        glm::vec2(PatchWidth, PatchHeight),

        glm::vec2(size.x - PatchWidth, size.y - PatchHeight),
        glm::vec2(size.x - PatchWidth, PatchHeight),
        glm::vec2(size.x, size.y - PatchHeight),
        glm::vec2(size.x, PatchHeight),

        //Center
        glm::vec2(PatchWidth, size.y - PatchHeight),
        glm::vec2(PatchWidth, PatchHeight),
        glm::vec2(size.x - PatchWidth, size.y - PatchHeight),
        glm::vec2(size.x - PatchWidth, PatchHeight),
    };

    //would be so much smarter to index the verts instead of duping them
    //but we're in it up to our beards now.
    entity.getComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(positions[0], PatchUVs[0]),
            cro::Vertex2D(positions[1], PatchUVs[1]),
            cro::Vertex2D(positions[2], PatchUVs[2]),
            cro::Vertex2D(positions[1], PatchUVs[1]),
            cro::Vertex2D(positions[3], PatchUVs[3]),
            cro::Vertex2D(positions[2], PatchUVs[2]),


            cro::Vertex2D(positions[4], PatchUVs[4]),
            cro::Vertex2D(positions[5], PatchUVs[5]),
            cro::Vertex2D(positions[6], PatchUVs[6]),
            cro::Vertex2D(positions[5], PatchUVs[5]),
            cro::Vertex2D(positions[7], PatchUVs[7]),
            cro::Vertex2D(positions[6], PatchUVs[6]),


            cro::Vertex2D(positions[8],  PatchUVs[8]),
            cro::Vertex2D(positions[9],  PatchUVs[9]),
            cro::Vertex2D(positions[10], PatchUVs[10]),
            cro::Vertex2D(positions[9],  PatchUVs[9]),
            cro::Vertex2D(positions[11], PatchUVs[11]),
            cro::Vertex2D(positions[10], PatchUVs[10]),


            cro::Vertex2D(positions[12], PatchUVs[12]),
            cro::Vertex2D(positions[13], PatchUVs[13]),
            cro::Vertex2D(positions[14], PatchUVs[14]),
            cro::Vertex2D(positions[13], PatchUVs[13]),
            cro::Vertex2D(positions[15], PatchUVs[15]),
            cro::Vertex2D(positions[14], PatchUVs[14]),


            cro::Vertex2D(positions[16], PatchUVs[16]),
            cro::Vertex2D(positions[17], PatchUVs[17]),
            cro::Vertex2D(positions[18], PatchUVs[18]),
            cro::Vertex2D(positions[17], PatchUVs[17]),
            cro::Vertex2D(positions[19], PatchUVs[19]),
            cro::Vertex2D(positions[18], PatchUVs[18]),


            cro::Vertex2D(positions[20], PatchUVs[20]),
            cro::Vertex2D(positions[21], PatchUVs[21]),
            cro::Vertex2D(positions[22], PatchUVs[22]),
            cro::Vertex2D(positions[21], PatchUVs[21]),
            cro::Vertex2D(positions[23], PatchUVs[23]),
            cro::Vertex2D(positions[22], PatchUVs[22]),


            cro::Vertex2D(positions[24], PatchUVs[24]),
            cro::Vertex2D(positions[25], PatchUVs[25]),
            cro::Vertex2D(positions[26], PatchUVs[26]),
            cro::Vertex2D(positions[25], PatchUVs[25]),
            cro::Vertex2D(positions[27], PatchUVs[27]),
            cro::Vertex2D(positions[26], PatchUVs[26]),


            cro::Vertex2D(positions[28], PatchUVs[28]),
            cro::Vertex2D(positions[29], PatchUVs[29]),
            cro::Vertex2D(positions[30], PatchUVs[30]),
            cro::Vertex2D(positions[29], PatchUVs[29]),
            cro::Vertex2D(positions[31], PatchUVs[31]),
            cro::Vertex2D(positions[30], PatchUVs[30]),


            cro::Vertex2D(positions[32], PatchUVs[32]),
            cro::Vertex2D(positions[33], PatchUVs[33]),
            cro::Vertex2D(positions[34], PatchUVs[34]),
            cro::Vertex2D(positions[33], PatchUVs[33]),
            cro::Vertex2D(positions[35], PatchUVs[35]),
            cro::Vertex2D(positions[34], PatchUVs[34])
        });

    size.y -= ItemSpacing;
    size *= m_viewScale;
    m_croppingArea = { 6.f * m_viewScale.x, 6.f * m_viewScale.y, size.x - (12.f * m_viewScale.x), size.y - (12.f * m_viewScale.y) };

    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::MessageBoard;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    constexpr float ThumbHeight = 100.f;
    for (auto& hole : m_holeDirs)
    {
        for (auto& thumb : hole.holes)
        {
            thumb.defaultScale = glm::vec2(1.f);
            if (ThumbHeight * m_viewScale.y > size.y)
            {
                //thumb.defaultScale /= 2.f;
            }
            if (thumb.thumbEnt.getComponent<cro::Transform>().getScale().y > 0)
            {
                thumb.thumbEnt.getComponent<cro::Transform>().setScale(thumb.defaultScale);
            }
        }
    }
}

void PlaylistState::updateInfo()
{
    std::string info =
        "Skybox: " + m_skyboxes[m_skyboxIndex] +
        "\nShrubs: " + m_shrubs[m_shrubIndex] +
        "\nActive Course: " + m_holeDirs[m_holeDirIndex].name;// +
        //"\nSelected Hole: " + m_playlist[m_playlistIndex].name +
        //"\n" + std::to_string(m_playlist.size()) + " holes added.";

    if (!m_playlist.empty())
    {
        info += "\nSelected Hole: " + m_playlist[m_playlistIndex].uiNode.getComponent<cro::Text>().getString();
    }

    if (m_saveFiles.empty())
    {
        info += "\n\nNo Saved Courses.";
    }
    else
    {
        info += "\n\nCurrent Save: " + m_saveFiles[m_saveFileIndex].substr(0, m_saveFiles[m_saveFileIndex].find_last_of('.'));
    }

    m_infoEntity.getComponent<cro::Text>().setString(info);
}

void PlaylistState::confirmSave()
{
    m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().getUserData<PopupData>().state = PopupData::In;
    m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().active = true;
    
    auto destructCallback = [&](cro::Entity e, float)
    {
        if (!m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().active)
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }
    };

    auto bounds = m_menuEntities[MenuID::Popup].getComponent<cro::Sprite>().getTextureBounds();

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto title = m_uiScene.createEntity();
    title.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), std::floor(bounds.height * 0.65f), 0.6f });
    title.addComponent<cro::Drawable2D>();
    title.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    title.getComponent<cro::Text>().setFillColour(TextNormalColour);
    title.addComponent<cro::Callback>().active = true;
    title.getComponent<cro::Callback>().function = destructCallback;
      
    m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(title.getComponent<cro::Transform>());

    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    auto createItem = [&](const std::string& label, glm::vec2 position)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.6f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString(label);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Popup);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_popupIDs[PopupID::TextSelected];
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_popupIDs[PopupID::TextUnselected];
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = destructCallback;
        centreText(entity);
        m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        return entity;
    };


    //if saves are empty create a new one and save
    glm::vec2 rootPos = glm::vec2(std::floor(bounds.width / 3.f), ItemSpacing * 3.f);
    if (m_saveFiles.empty())
    {
        title.getComponent<cro::Text>().setString("Save Course?");
        centreText(title);

        auto entity = createItem("Yes", rootPos);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_popupIDs[PopupID::SaveNew];
        rootPos.x *= 2.f;
    }
    //else confirm overwriting existing or creating new
    else 
    {
        title.getComponent<cro::Text>().setString("Replace " + m_saveFiles[m_saveFileIndex].substr(0, m_saveFiles[m_saveFileIndex].find_last_of('.')) + "?");
        centreText(title);

        if (m_saveFiles.size() < MaxSaves)
        {
            //allow creating new
            rootPos.x = std::floor(bounds.width / 4.f);

            auto entity = createItem("Yes", rootPos);
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_popupIDs[PopupID::SaveOverwrite];
            rootPos.x *= 2.f;

            entity = createItem("No", rootPos);
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_popupIDs[PopupID::SaveNew];
            rootPos.x += std::floor(bounds.width / 4.f);

            title = m_uiScene.createEntity();
            title.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), std::floor(bounds.height * 0.49f), 0.6f });
            title.addComponent<cro::Drawable2D>();
            title.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
            title.getComponent<cro::Text>().setFillColour(TextNormalColour);
            title.getComponent<cro::Text>().setString("\'No\' to Create New Save");
            title.addComponent<cro::Callback>().active = true;
            title.getComponent<cro::Callback>().function = destructCallback;

            m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(title.getComponent<cro::Transform>());
            centreText(title);
        }
        else
        {
            auto entity = createItem("Yes", rootPos);
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_popupIDs[PopupID::SaveOverwrite];
            rootPos.x *= 2.f;
        }
    }
    
    auto entity = createItem("Cancel", rootPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_popupIDs[PopupID::ClosePopup];
}

void PlaylistState::confirmLoad(std::size_t index)
{
    if (index == m_saveFileIndex)
    {
        return;
    }

    m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().getUserData<PopupData>().state = PopupData::In;
    m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().active = true;

    auto bounds = m_menuEntities[MenuID::Popup].getComponent<cro::Sprite>().getTextureBounds();

    const auto destroyCallback = [&](cro::Entity e, float)
    {
        if (!m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().active)
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }
    };

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto title = m_uiScene.createEntity();
    title.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), std::floor(bounds.height * 0.75f), 0.6f });
    title.addComponent<cro::Drawable2D>();
    title.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    title.getComponent<cro::Text>().setFillColour(TextNormalColour);
    title.getComponent<cro::Text>().setString("Load " + m_saveFiles[index].substr(0, m_saveFiles[index].find_last_of('.')) + "?");
    title.addComponent<cro::Callback>().active = true;
    title.getComponent<cro::Callback>().function = destroyCallback;
        
    m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(title.getComponent<cro::Transform>());
    centreText(title);

    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    //confirm if we want to save current first
    if (!m_playlist.empty())
    {
        //this will overwrite existing
        title = m_uiScene.createEntity();
        title.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), std::floor(bounds.height * 0.56f), 0.6f });
        title.addComponent<cro::Drawable2D>();
        title.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
        title.getComponent<cro::Text>().setFillColour(TextNormalColour);
        title.getComponent<cro::Text>().setString("This Will Overwrite");
        title.addComponent<cro::Callback>().active = true;
        title.getComponent<cro::Callback>().function = destroyCallback;
        m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(title.getComponent<cro::Transform>());
        centreText(title);

        title = m_uiScene.createEntity();
        title.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), std::floor(bounds.height * 0.48f), 0.6f });
        title.addComponent<cro::Drawable2D>();
        title.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
        title.getComponent<cro::Text>().setFillColour(TextNormalColour);
        title.getComponent<cro::Text>().setString("Active Settings");
        title.addComponent<cro::Callback>().active = true;
        title.getComponent<cro::Callback>().function = destroyCallback;
        m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(title.getComponent<cro::Transform>());
        centreText(title);
    }


    auto createItem = [&](const std::string& label, glm::vec2 position)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.6f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString(label);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Popup);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_popupIDs[PopupID::TextSelected];
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_popupIDs[PopupID::TextUnselected];
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            if (!m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().active)
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
        };
        centreText(entity);
        m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        return entity;
    };


    //if saves are empty create a new one and save
    glm::vec2 rootPos = glm::vec2(std::floor(bounds.width / 3.f), ItemSpacing * 3.f);

    auto entity = createItem("OK", rootPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_popupIDs[PopupID::LoadFile];
    entity.getComponent<cro::Callback>().setUserData<std::size_t>(index);
    rootPos.x *= 2.f;

    entity = createItem("Cancel", rootPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_popupIDs[PopupID::ClosePopup];

    updateInfo();
}

void PlaylistState::showExportResult(bool success)
{
    m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().getUserData<PopupData>().state = PopupData::In;
    m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().active = true;

    auto bounds = m_menuEntities[MenuID::Popup].getComponent<cro::Sprite>().getTextureBounds();

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto title = m_uiScene.createEntity();
    title.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), std::floor(bounds.height * 0.65f), 0.6f });
    title.addComponent<cro::Drawable2D>();
    title.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    title.getComponent<cro::Text>().setFillColour(TextNormalColour);
    title.addComponent<cro::Callback>().active = true;
    title.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (!m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().active)
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }
    };
    m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(title.getComponent<cro::Transform>());

    if (success)
    {
        title.getComponent<cro::Text>().setString("Export Successful");

        Achievements::awardAchievement(AchievementStrings[AchievementID::GrandDesign]);
    }
    else
    {
        title.getComponent<cro::Text>().setString("Export Failed");
    }
    centreText(title);



    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    auto createItem = [&](const std::string& label, glm::vec2 position)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.6f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString(label);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Popup);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_popupIDs[PopupID::TextSelected];
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_popupIDs[PopupID::TextUnselected];
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            if (!m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().active)
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
        };
        centreText(entity);
        m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        return entity;
    };


    auto entity = createItem("OK", glm::vec2(std::floor(bounds.width / 2.f), ItemSpacing * 3.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_popupIDs[PopupID::ClosePopup];
}

void PlaylistState::saveCourse(bool createNew)
{
    auto exportDir = cro::App::getPreferencePath() + UserCoursePath;
    if (!cro::FileSystem::directoryExists(exportDir))
    {
        cro::FileSystem::createDirectory(exportDir);
    }
    

    if (createNew)
    {
        m_saveFileIndex = m_saveFiles.size();

        std::stringstream ss;
        ss << "course" << std::setw(2) << std::setfill('0') << m_saveFileIndex << SaveFileExtension;
        m_saveFiles.push_back(ss.str());
    }

    std::stringstream ss;
    ss << exportDir << m_saveFiles[m_saveFileIndex];
    std::string fileName = ss.str();

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(fileName.c_str(), "wb");
    if (file.file)
    {
        SaveFileEntry entry;
        entry.type = EntryType::Shrub;
        entry.directory = static_cast<std::uint8_t>(m_shrubIndex);
        auto written = SDL_RWwrite(file.file, &entry, sizeof(entry), 1);

        entry.type = EntryType::Skybox;
        entry.directory = static_cast<std::uint8_t>(m_skyboxIndex);
        written += SDL_RWwrite(file.file, &entry, sizeof(entry), 1);

        for (const auto& hole : m_playlist)
        {
            entry.type = EntryType::Hole;
            entry.directory = static_cast<std::uint8_t>(hole.courseIndex);
            entry.file = static_cast<std::uint8_t>(hole.holeIndex);
            written += SDL_RWwrite(file.file, &entry, sizeof(entry), 1);
        }

        if (written != m_playlist.size() + 2)
        {
            LogE << "Incomplete save file written. Wrote: " << written << ", expected: " << m_playlist.size() + 2 << std::endl;
        }
    }
    else
    {
        LogE << "Failed opening " << fileName << " for writing." << std::endl;
    }
}

void PlaylistState::loadCourse()
{
    auto exportDir = cro::App::getPreferencePath() + UserCoursePath;
    if (!cro::FileSystem::directoryExists(exportDir))
    {
        cro::FileSystem::createDirectory(exportDir);
    }

    std::stringstream ss;
    ss << exportDir << m_saveFiles[m_saveFileIndex];
    std::string fileName = ss.str();

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(fileName.c_str(), "rb");
    if (file.file)
    {
        for (auto& e : m_playlist)
        {
            m_uiScene.destroyEntity(e.uiNode);
        }
        m_playlist.clear();
        m_playlistIndex = 0;

        static constexpr std::size_t MaxCount = 21; //18 holes + shrub/sky/audio
        std::size_t readTotal = 0;
        std::size_t read = 0;

        do
        {
            SaveFileEntry output;
            read = SDL_RWread(file.file, &output, sizeof(output), 1);

            if (read)
            {
                switch (output.type)
                {
                default:
                case EntryType::Audio:
                    LogW << "Not yet implemented" << std::endl;
                    break;
                case EntryType::Hole:
                {
                    auto currIndex = m_playlist.size();
                    auto& item = m_playlist.emplace_back();
                    item.courseIndex = output.directory;
                    item.holeIndex = output.file;
                    item.currentIndex = currIndex;
                    //if there's an assert here it's probably because no thumbnails
                    //have been generated and m_holeDirs is empty. TODO fix this.
                    item.name =  m_holeDirs[item.courseIndex].holes[item.holeIndex].name;
                }
                    break;
                case EntryType::Skybox:
                    m_skyboxIndex = output.directory;
                    break;
                case EntryType::Shrub:
                    m_shrubIndex = output.directory;
                    break;
                }
            }
            readTotal += read;

        } while (read == 1 && readTotal < MaxCount);

        m_courseData.shrubPath = ShrubPath + m_shrubs[m_shrubIndex];
        m_courseData.skyboxPath = SkyboxPath + m_skyboxes[m_skyboxIndex];

        //apply skybox
        const auto& ents = m_skyboxScene.getSystem<cro::ModelRenderer>()->getEntities();
        for (auto e : ents)
        {
            m_skyboxScene.destroyEntity(e); //this includes destroying the active clouds...
        }
        
        SkyboxMaterials materials;
        materials.horizon = m_materialIDs[MaterialID::Horizon];
        //materials.horizonSun = m_materialIDs[MaterialID::HorizonSun];
        materials.skinned = m_materialIDs[MaterialID::CelTexturedSkinned];
        
        auto cloudEnt = loadSkybox(SkyboxPath + m_skyboxes[m_skyboxIndex], m_skyboxScene, m_resources, materials);
        //hum I didn't think this through did I?
        if (cloudEnt.isValid())
        {
            auto material = m_resources.materials.get(m_materialIDs[MaterialID::Cloud]);
            material.setProperty("u_skyColourTop", m_skyboxScene.getSkyboxColours().top);
            material.setProperty("u_skyColourBottom", m_skyboxScene.getSkyboxColours().middle);

            m_cloudRing = cloudEnt;
            m_cloudRing.getComponent<cro::Model>().setMaterial(0, material);
        }

        //apply shrubs
        for (auto& shrub : m_shrubberyModels)
        {
            shrub.hide();
        }
        applyShrubQuality();

        //create ui nodes for playlist entries
        float vertPos = 0.f;
        for (auto& entry : m_playlist)
        {
            std::stringstream ss;
            ss << std::setw(2) << std::setfill('0') << entry.courseIndex + 1 << "_";

            entry.uiNode = m_uiScene.createEntity();
            entry.uiNode.addComponent<cro::Transform>().setPosition({ (cro::App::getWindow().getSize().x / m_viewScale.x) - PlaylistOffset, vertPos, 0.1f });
            entry.uiNode.addComponent<cro::Drawable2D>();
            entry.uiNode.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Info)).setString(ss.str() + entry.name);
            entry.uiNode.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
            entry.uiNode.getComponent<cro::Text>().setFillColour(TextNormalColour);

            entry.uiNode.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entry.uiNode);
            entry.uiNode.getComponent<cro::UIInput>().setGroup(MenuID::Holes);
            entry.uiNode.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_playlistActivatedCallback;;
            entry.uiNode.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_playlistSelectedCallback;;
            entry.uiNode.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_playlistUnselectedCallback;;


            entry.uiNode.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::ScoreScroll;
            entry.uiNode.addComponent<UIElement>().depth = 0.1f;
            entry.uiNode.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
            entry.uiNode.getComponent<UIElement>().absolutePosition = { -PlaylistOffset, vertPos };

            entry.uiNode.addComponent<cro::Callback>().active = true;
            entry.uiNode.getComponent<cro::Callback>().function = ListItemCallback(m_croppingArea);

            m_playlistScrollNode.getComponent<cro::Transform>().addChild(entry.uiNode.getComponent<cro::Transform>());

            auto itemCount = static_cast<std::int32_t>(m_playlist.size());
            m_playlistScrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().itemCount = itemCount;

            if (entry.uiNode.getComponent<cro::Transform>().getWorldPosition().y < ItemSpacing * 2.f)
            {
                m_playlistScrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex = std::max(0, itemCount - 1);
            }
            vertPos -= ItemSpacing;
        }

        if (!m_playlist.empty())
        {
            m_playlistIndex = m_playlist.size() - 1;

            auto c = m_playlist[m_playlistIndex].courseIndex;
            auto h = m_playlist[m_playlistIndex].holeIndex;
            //m_playlist[m_playlistIndex].uiNode.getComponent<cro::Text>().setFillColour(TextGoldColour);

            m_holePreview.getComponent<cro::Sprite>().setTexture(*m_holeDirs[c].holes[h].thumbEnt.getComponent<cro::Sprite>().getTexture());
        }
    }
    else
    {
        LogE << "Failed opening file " << fileName << " for loading." << std::endl;
    }
    updateInfo();
}

bool PlaylistState::exportCourse()
{
    auto courseDir = cro::App::getPreferencePath() + UserCoursePath;
    if (!cro::FileSystem::directoryExists(courseDir))
    {
        cro::FileSystem::createDirectory(courseDir);
    }

    auto exportDir = cro::App::getPreferencePath() + UserCourseExport;
    if (!cro::FileSystem::directoryExists(exportDir))
    {
        cro::FileSystem::createDirectory(exportDir);
    }

    std::stringstream ss;
    ss << "course" << std::setw(2) << std::setfill('0') << m_saveFileIndex << '/';
    courseDir = ss.str();

    exportDir += courseDir;

    if (!cro::FileSystem::directoryExists(exportDir))
    {
        cro::FileSystem::createDirectory(exportDir);
    }

    cro::ConfigFile cfg;
    cfg.addProperty("skybox").setValue(m_courseData.skyboxPath);
    cfg.addProperty("shrubbery").setValue(m_courseData.shrubPath);
    //TODO audio. We'll let the default take care of it for now

    cfg.addProperty("title").setValue("Custom Course " + std::to_string(m_saveFileIndex + 1));
    cfg.addProperty("description").setValue("User Created Course " + std::to_string(m_saveFileIndex + 1));

    for (const auto& h : m_playlist)
    {
        std::string holePath = CoursePath + m_holeDirs[h.courseIndex].name + "/" + m_holeDirs[h.courseIndex].holes[h.holeIndex].name;
        holePath = holePath.substr(0, holePath.find_last_of('.')) + ".hole";
        cfg.addProperty("hole").setValue(holePath);
    }

    return cfg.save(exportDir + "course.data");
}

//void PlaylistState::showToolTip(const std::string& label)
//{
//    if (label != m_toolTip.getComponent<cro::Text>().getString())
//    {
//        m_toolTip.getComponent<cro::Text>().setString(label);
//    }
//
//    m_toolTip.getComponent<cro::Callback>().active = true;
//}
//
//void PlaylistState::hideToolTip()
//{
//    m_toolTip.getComponent<cro::Callback>().active = false;
//    m_toolTip.getComponent<cro::Transform>().setPosition(glm::vec3(10000.f));
//}

void PlaylistState::showHelp()
{
    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);

    glm::vec2 screenSize = cro::App::getWindow().getSize();
    auto c = cro::Colour(0.f, 0.f, 0.f, 0.5f);

    //background
    auto bgEnt = m_uiScene.createEntity();
    bgEnt.addComponent<cro::Transform>().setPosition({ screenSize.x / 2.f, screenSize.y / 2.f, 1.f });
    bgEnt.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-0.5f, 0.5f), c),
            cro::Vertex2D(glm::vec2(-0.5f), c),
            cro::Vertex2D(glm::vec2(0.5f), c),
            cro::Vertex2D(glm::vec2(0.5f, -0.5f), c)
        });
    bgEnt.addComponent<cro::Callback>().active = true;
    bgEnt.getComponent<cro::Callback>().setUserData<BgData>();
    bgEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<BgData>();
        const float speed = dt * 6.f;
        if (data.state == 0)
        {
            //zoom in
            data.progress = std::min(1.f, data.progress + speed);
        }
        else
        {
            //zoom out
            data.progress = std::max(0.f, data.progress - speed);

            if (data.progress == 0)
            {
                e.getComponent<cro::Callback>().active = false;
                for (auto c : data.children)
                {
                    m_uiScene.destroyEntity(c);
                }
                m_uiScene.destroyEntity(e);
                helpOverlay = {};
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(m_currentTab);
            }
        }

        glm::vec2 screenSize = cro::App::getWindow().getSize();
        //float scaleProgress = std::min(1.f, (e.getComponent<cro::Transform>().getScale().x / screenSize.x) + speed);
        e.getComponent<cro::Transform>().setScale(cro::Util::Easing::easeOutCirc(data.progress) * screenSize);

        //always navigate to screen centre
        auto target = screenSize / 2.f;
        auto diff = target - glm::vec2(e.getComponent<cro::Transform>().getPosition());
        if (glm::length2(diff) > 1)
        {
            e.getComponent<cro::Transform>().move(diff * speed);
        }
        else
        {
            e.getComponent<cro::Transform>().setPosition(target);
        }
    };
    helpOverlay = bgEnt;

    BgData& callbackData = bgEnt.getComponent<cro::Callback>().getUserData<BgData>();

    auto rootNode = m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 1.1f });
    rootNode.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    rootNode.addComponent<UIElement>().depth = 1.1f;
    //rootNode.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    rootNode.addComponent<cro::Callback>().active = true;
    rootNode.getComponent<cro::Callback>().function = 
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setScale(m_viewScale);
    };
    callbackData.children.push_back(rootNode);

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    auto textCallback = [bgEnt](cro::Entity e, float dt)
    {
        const auto& data = bgEnt.getComponent<cro::Callback>().getUserData<BgData>();
        float scale = cro::Util::Easing::easeOutBack(data.progress);
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };

    //info panel title and desc
    const auto createTitle = [&](glm::vec3 position, const std::string& label, glm::vec2 relPos = glm::vec2(0.f))
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition((screenSize / m_viewScale) * relPos);
        entity.getComponent<cro::Transform>().move(position);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
        entity.addComponent<UIElement>().depth = position.z;
        entity.getComponent<UIElement>().relativePosition = relPos;
        entity.getComponent<UIElement>().absolutePosition = { position.x, position.y };
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(largeFont).setString(label);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = textCallback;

        rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        callbackData.children.push_back(entity);
        return entity;
    };
    const auto createDesc = [&](glm::vec3 position, const std::string& label, glm::vec2 relPos = glm::vec2(0.f))
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition((screenSize / m_viewScale) * relPos);
        entity.getComponent<cro::Transform>().move(position);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
        entity.addComponent<UIElement>().depth = position.z;
        entity.getComponent<UIElement>().relativePosition = relPos;
        entity.getComponent<UIElement>().absolutePosition = { position.x, position.y };
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString(label);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setVerticalSpacing(-1.f);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = textCallback;

        rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        callbackData.children.push_back(entity);

        return entity;
    };

    auto entity = createTitle({ -InfoOffset, -10.f, 0.1f }, "Active Settings >", glm::vec2(1.f));
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width * 1.1f), 0.f });

    entity = createDesc({ -InfoOffset, -22.f, 0.1f }, "Current selections\nappear here", glm::vec2(1.f));
    //use above bounds to line up correctly
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width * 1.1f), 0.f });

    switch (m_currentTab)
    {
    default: break;
    case MenuID::Skybox:
    {
        createTitle({ 120.f, -ItemSpacing, 0.1f }, "< Skybox Selection", glm::vec2(0.f, TabAreaHeight));
        createDesc({ 120.f, (-ItemSpacing * 2.f) - 2.f, 0.1f}, "Select which skybox the course has", glm::vec2(0.f, TabAreaHeight));
    }
        break;
    case MenuID::Shrubbery:
    {
        createTitle({ 80.f, -ItemSpacing, 0.1f }, "< Foliage Selection", glm::vec2(0.f, TabAreaHeight));
        createDesc({ 80.f, (-ItemSpacing * 2.f) - 2.f, 0.1f }, "Select which trees and\nbushes the course has", glm::vec2(0.f, TabAreaHeight));

        createTitle({ 34.f, 15.f, 0.1f }, "< Preview Quality", glm::vec2(0.5f, 0.f));
    }
        break;
    case MenuID::Holes:
    {
        createTitle({ 80.f, -ItemSpacing, 0.1f }, "< Course List", glm::vec2(0.f, TabAreaHeight));
        createDesc({ 80.f, (-ItemSpacing * 2.f) - 2.f, 0.1f }, "Select which course to browse", glm::vec2(0.f, TabAreaHeight));

        entity = createTitle({ -26.f, 15.f, 0.1f }, "Cycle Holes >", glm::vec2(0.5f, 0.f));
        bounds = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width * 1.1f), 0.f });

        entity = createTitle({ -ThumbnailOffset, ItemSpacing, 0.1f }, "Update Playlist >", glm::vec2(1.f, TabAreaHeight / 2.f));
        bounds = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width * 1.1f), 0.f });
        entity = createDesc({ -ThumbnailOffset, -2.f, 0.1f }, "Add, Remove or adjust the\norder of selected holes", glm::vec2(1.f, TabAreaHeight / 2.f));
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width * 1.1f), 0.f });
    }
        break;
    case MenuID::FileSystem:
    {
        if (!m_saveFiles.empty())
        {
            createTitle({ 90.f, -ItemSpacing * 2.f, 0.1f }, "< Saved Courses", glm::vec2(0.f, TabAreaHeight));
            createDesc({ 90.f, (-ItemSpacing * 3.f) - 2.f, 0.1f }, "Select a save file to load", glm::vec2(0.f, TabAreaHeight));
        }

        entity = createTitle({ -38.f, 31.f, 0.1f }, "Save\nSelection >", glm::vec2(0.35f, 0.f));
        entity.getComponent<cro::Text>().setVerticalSpacing(4.f);
        bounds = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width), 0.f });

        createTitle({ 48.f, 40.f, 0.1f }, "< Export", glm::vec2(0.65f, 0.f));
        createDesc({ 48.f, 28.f, 0.1f }, "Make this course\navailable in game", glm::vec2(0.65f, 0.f));
    }
        break;
    }


    //cancel any active sliders
    m_activeSlider = {};
}

void PlaylistState::quitState()
{
    if (helpOverlay.isValid())
    {
        helpOverlay.getComponent<cro::Callback>().getUserData<BgData>().state = 1;
        return;
    }


    const auto destructionCallback = [&](cro::Entity e, float)
    {
        if (!m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().active)
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }
    };

    m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().getUserData<PopupData>().state = PopupData::In;
    m_menuEntities[MenuID::Popup].getComponent<cro::Callback>().active = true;

    auto bounds = m_menuEntities[MenuID::Popup].getComponent<cro::Sprite>().getTextureBounds();

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto title = m_uiScene.createEntity();
    title.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), std::floor(bounds.height * 0.77f), 0.6f });
    title.addComponent<cro::Drawable2D>();
    title.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    title.getComponent<cro::Text>().setFillColour(TextNormalColour);
    title.getComponent<cro::Text>().setString("Quit Editor");
    title.addComponent<cro::Callback>().active = true;
    title.getComponent<cro::Callback>().function = destructionCallback;
        
    centreText(title);
    m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(title.getComponent<cro::Transform>());


    title = m_uiScene.createEntity();
    title.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), std::floor(bounds.height * 0.53f), 0.6f });
    title.addComponent<cro::Drawable2D>();
    title.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    title.getComponent<cro::Text>().setFillColour(TextNormalColour);
    title.getComponent<cro::Text>().setString("Are You Sure?");
    title.addComponent<cro::Callback>().active = true;
    title.getComponent<cro::Callback>().function = destructionCallback;

    centreText(title);
    m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(title.getComponent<cro::Transform>());

    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    /*title = m_uiScene.createEntity();
    title.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), std::floor(bounds.height * 0.45f), 0.6f });
    title.addComponent<cro::Drawable2D>();
    title.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    title.getComponent<cro::Text>().setFillColour(TextNormalColour);
    title.getComponent<cro::Text>().setString("Changes Will Be Lost");
    title.addComponent<cro::Callback>().active = true;
    title.getComponent<cro::Callback>().function = destructionCallback;
    centreText(title);
    m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(title.getComponent<cro::Transform>());*/



    auto createItem = [&](const std::string& label, glm::vec2 position)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.6f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString(label);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Popup);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_popupIDs[PopupID::TextSelected];
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_popupIDs[PopupID::TextUnselected];
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = destructionCallback;
        centreText(entity);
        m_menuEntities[MenuID::Popup].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        return entity;
    };

    glm::vec2 rootPos = glm::vec2(std::floor(bounds.width / 3.f), ItemSpacing * 3.f);
    auto entity = createItem("Yes", rootPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_popupIDs[PopupID::QuitState];
    rootPos.x *= 2.f;

    entity = createItem("No", rootPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_popupIDs[PopupID::ClosePopup];
}