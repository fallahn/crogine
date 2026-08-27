/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2026
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

#include "MapOverviewState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "../GolfGame.hpp"

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

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/OpenGL.hpp>

using namespace cl;

namespace
{
    const std::string MinimapFragment = 
        R"(
            uniform sampler2D u_texture;
            uniform sampler2D u_worldPos;
            //uniform sampler2D u_maskMap;
            uniform sampler2D u_normalMap;

            uniform float u_heatAmount = 0.0;
            uniform float u_gridAmount = 0.0;
            uniform float u_gridScale = 1.0;

            VARYING_IN vec2 v_texCoord;
            VARYING_IN vec4 v_colour;

            OUTPUT

            #define TAU 6.283185
            const float ContourSpacing = 2.0 * TAU;

            const vec3 BaseHeatColour = vec3(0.827, 0.599, 0.91); //stored as HSV to save on a conversion
            vec3 hsv2rgb(vec3 c)
            {
                vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
                vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
                return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
            }

            const float ColourStep = 6.0;
            const float GridThickness = 0.03;
            void main()
            {
                vec4 colour = texture(u_texture, v_texCoord) * v_colour;
                float pos = texture(u_worldPos, v_texCoord).r;

                float heightIntensity = smoothstep(0.1, 5.0, pos);
                heightIntensity = 0.05 + (0.95 * heightIntensity);

                vec4 gridColour = vec4(vec3(0.8, 0.7, 0.2) * heightIntensity, colour.a);




//vec2 gridRes = vec2(320.0, 200.0) * GridThickness * u_gridScale;
//float gridThickness = GridThickness / u_gridScale;
//
//vec2 grid = fract(v_texCoord * gridRes);
//float gridAmount = 1.0 - (step(gridThickness, grid.x) * step(gridThickness, grid.y));

                vec4 normalSample = texture(u_normalMap, v_texCoord);

                float contourAmount = smoothstep(1.0 - (0.12 / u_gridScale), 1.0 - (0.1 / u_gridScale), fract(pos * u_gridScale));
                //contourAmount *= texture(u_maskMap, v_texCoord).r;
                contourAmount *= normalSample.a;

                //stops contours 'spreading' over almost flat areas
                vec3 normal = normalSample.rgb * 2.0 - 1.0;
                contourAmount *= 1.0 - step(0.995, dot(vec3(0.0, 1.0, 0.0), normal));

                FRAG_OUT = colour + (gridColour * contourAmount * u_gridAmount);



                vec3 c = BaseHeatColour;
                c.x += mod(pos / (8.0 - (u_gridScale / 3.0)), 1.0); //6.0 is MaxZoom, 8.0 is just a magic number
                c = hsv2rgb(c);

                c *= clamp(dot(colour.rgb, vec3(0.299, 0.587, 0.114)) * 3.0, 0.0, 1.0); //luma of colour

                c = floor(c * (ColourStep * u_gridScale)) / ((ColourStep * u_gridScale) - 1.0);
                FRAG_OUT.rgb = mix(FRAG_OUT.rgb, c, u_heatAmount);
            }
        )";

    const std::string MiniSlopeFragment = 
        R"(
            OUTPUT

            uniform float u_transparency;

            VARYING_IN vec4 v_colour;

            void main()
            {
                vec4 colour = v_colour;
                colour.a *= u_transparency;

                FRAG_OUT = colour;
            }
        )";

    const std::string DitherFragment = 
R"(
OUTPUT
uniform sampler2D u_texture;
uniform vec4 u_displayArea;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

const float FadeDistance = 40.0;

void main()
{
    ivec2 texSize = textureSize(u_texture, 0);
    float ditherWidth = FadeDistance / texSize.x;
    float ditherHeight = FadeDistance / texSize.y;

    float left = smoothstep(u_displayArea.x, u_displayArea.x + ditherWidth, v_texCoord.x);
    float right = 1.0 - smoothstep((u_displayArea.x + u_displayArea.z) - ditherWidth, (u_displayArea.x + u_displayArea.z), v_texCoord.x);

    float bottom = smoothstep(u_displayArea.y, u_displayArea.y + ditherHeight, v_texCoord.y);
    float top = 1.0 - smoothstep((u_displayArea.y + u_displayArea.w) - ditherHeight, (u_displayArea.y + u_displayArea.w), v_texCoord.y);

    float dither = left * right * bottom * top;
    vec4 colour = TEXTURE(u_texture, v_texCoord) * v_colour;

    FRAG_OUT = vec4(colour.rgb, colour.a * dither);
})";

    constexpr float MaxZoom = 12.f;
    constexpr float MinZoom = 0.75f;
    constexpr float BaseScaleMultiplier = 0.8f;
    constexpr std::int32_t MaxFingers = 2;

    constexpr float CamHeight = 136.f;
    constexpr float CamFar = CamHeight + 4.f;
}

MapOverviewState::MapOverviewState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus()),
    m_sharedData        (sd),
    m_previousMap       (-1),
    m_viewScale         (2.f),
    m_heatTarget        (0.f),
    m_heatAmount        (0.f),
    m_zoomScale         (1.f),
    m_transitionActive  (false),
    m_fingerCount       (0),
    m_ditherUniform     (-1)
{
    ctx.mainWindow.setCursorVisible(true);
    m_scene.setTitle("Map Overview");

    CRO_ASSERT(sd.minimapData.mapScene, "");
    addSystems();
    loadAssets();
    buildScene();

    //registerWindow([&]()
    //    {
    //        ImGui::Begin("Window");
    //        const auto camPos = m_mapCamera.getComponent<cro::Transform>().getPosition();
    //        ImGui::Text("Cam pos %3.2f, %3.2f, %3.2f", camPos.x, camPos.y, camPos.z);

    //        const auto pos = m_mapCamera.getComponent<cro::Camera>().coordsToPixel(camPos, m_mapBuffer.getSize());
    //        ImGui::Text("Screen pos %3.2f, %3.2f", pos.x, pos.y);

    //        const auto pos2 = m_mapCamera.getComponent<cro::Camera>().coordsToPixel(camPos + glm::vec3(1.f,0.f,1.f), m_mapBuffer.getSize()) - pos;
    //        ImGui::Text("Pixels per metre: %3.2f, %3.2f", pos2.x, pos2.y);

    //        /*const auto worldPos = m_mapCamera.getComponent<cro::Camera>().pixelToCoords(pos, m_mapBuffer.getSize());
    //        ImGui::Text("World pos %3.2f, %3.2f, %3.2f", worldPos.x, worldPos.y, worldPos.z);*/
    //        ImGui::End();
    //    });
}

//public
bool MapOverviewState::handleEvent(const cro::Event& evt)
{
    const auto setControlIcon = [&](bool isController)
        {
            if (isController)
            {
                m_controlIcon.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                m_controlText.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
            else
            {
                m_controlIcon.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_controlText.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            }
        };

    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    if (evt.type == SDL_EVENT_KEY_UP)
    {
        switch (evt.key.key)
        {
        default: 
            if (evt.key.key == m_sharedData.inputBinding.keys[InputBinding::NextClub])
            {
                m_heatTarget = 1.f;
            }
            else if (evt.key.key == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
            {
                m_heatTarget = 0.f;
            }
            else if (evt.key.key == m_sharedData.inputBinding.keys[InputBinding::SpinMenu])
            {
                gotoTarget();
            }
            break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
        case SDLK_6: //WHY
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_EVENT_KEY_DOWN)
    {
        setControlIcon(false);

        switch (evt.key.key)
        {
        default: break;
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_LEFT:
        case SDLK_RIGHT:
            cro::App::getWindow().setCursorVisible(false);
            break;
        }
    }
    else if (evt.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
    {
        setControlIcon(true);

        cro::App::getWindow().setCursorVisible(false);
        switch (evt.gbutton.button)
        {
        default: break;
        case cro::GameController::ButtonB:
        case cro::GameController::ButtonStart:
        case cro::GameController::ButtonTrackpad:
            quitState();
            return false;
        case cro::GameController::ButtonRightShoulder:
            m_heatTarget = 1.f;
            break;
        case cro::GameController::ButtonLeftShoulder:
            m_heatTarget = 0.f;
            break;
        case cro::GameController::ButtonRightStick:
            gotoTarget();
            break;
        }
    }
    else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        setControlIcon(false);

        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
            return false;
        }

        /*else if (evt.button.button == SDL_BUTTON_MIDDLE)
        {
            gotoTarget();
        }*/
    }
    else if (evt.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
    {
        if (evt.gaxis.value > cro::GameController::LeftThumbDeadZone)
        {
            setControlIcon(true);
            cro::App::getWindow().setCursorVisible(false);
        }
        m_thumbsticks.setValue(evt.gaxis.axis, evt.gaxis.value);
    }
    else if (evt.type == SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN)
    {
        setControlIcon(true);

        m_fingerCount++;
        if (evt.gtouchpad.finger < MaxFingers)
        {
            m_trackpadFingers[evt.gtouchpad.finger].prevPosition = { evt.gtouchpad.x, 1.f - evt.gtouchpad.y };
            m_trackpadFingers[evt.gtouchpad.finger].currPosition = { evt.gtouchpad.x, 1.f - evt.gtouchpad.y };
        }
        //LogI << "Finger count " << m_fingerCount << " finger id " << evt.gtouchpad.finger << std::endl;
    }
    else if (evt.type == SDL_EVENT_GAMEPAD_TOUCHPAD_UP)
    {
        m_fingerCount--;
        if (evt.gtouchpad.finger < MaxFingers)
        {
            //this effectively resets the velocity to 0
            m_trackpadFingers[evt.gtouchpad.finger].currPosition = m_trackpadFingers[evt.gtouchpad.finger].prevPosition;
        }
        //LogI << "Finger count " << m_fingerCount << " finger id " << evt.gtouchpad.finger << std::endl;
    }
    else if (evt.type == SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION)
    {
        setControlIcon(true);

        if (evt.gtouchpad.finger < MaxFingers)
        {
            glm::vec2 pos({ evt.gtouchpad.x, 1.f - evt.gtouchpad.y });
            m_trackpadFingers[evt.gtouchpad.finger].prevPosition = m_trackpadFingers[evt.gtouchpad.finger].currPosition;
            m_trackpadFingers[evt.gtouchpad.finger].currPosition = pos;
        }
    }
    else if (evt.type == SDL_EVENT_MOUSE_MOTION)
    {
        setControlIcon(false);

        cro::App::getWindow().setCursorVisible(true);
        if (evt.motion.state & (SDL_BUTTON_MIDDLE | SDL_BUTTON_LEFT))
        {
            panCamera({ -evt.motion.xrel, -evt.motion.yrel });
        }
    }
    else if (evt.type == SDL_EVENT_MOUSE_WHEEL)
    {
        setControlIcon(false);

        const auto amount = evt.wheel.y;
        m_zoomScale = std::clamp(m_zoomScale + amount, MinZoom, MaxZoom);
        zoomCamera();
    }

    m_scene.forwardEvent(evt);
    return false;
}

void MapOverviewState::handleMessage(const cro::Message& msg)
{
    //hide this state if the map transition started.
    if (msg.id == MessageID::SceneMessage)
    {
        const auto& data = msg.getData<SceneEvent>();
        if (data.type == SceneEvent::TransitionStart)
        {
            requestStackPop();
        }
        else if (data.type == SceneEvent::MinimapUpdated)
        {
            recentreMap();
        }
    }
    m_scene.forwardMessage(msg);
}

bool MapOverviewState::simulate(float dt)
{
    glm::vec2 movement(0.f);
    if (cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Left]))
    {
        movement.x -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Right]))
    {
        movement.x += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Up]))
    {
        movement.y -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Down]))
    {
        movement.y += 1.f;
    }
    

    auto len2 = glm::length2(movement);
    if (len2 > 1)
    {
        movement /= std::sqrt(len2);
    }

    if (len2 == 0)
    {
        //check controller analogue
        const auto x = m_thumbsticks.getValue(cro::GameController::AxisLeftX);
        if (x > cro::GameController::LeftThumbDeadZone || x < -cro::GameController::LeftThumbDeadZone)
        {
            movement.x = static_cast<float>(x) / cro::GameController::AxisMax;
        }

        const auto y = -m_thumbsticks.getValue(cro::GameController::AxisLeftY);
        if (y > cro::GameController::LeftThumbDeadZone || y < -cro::GameController::LeftThumbDeadZone)
        {
            movement.y = -static_cast<float>(y) / cro::GameController::AxisMax;
        }
        len2 = glm::length2(movement);
        if (len2 != 0)
        {
            movement = glm::normalize(movement) * std::min(1.f, std::pow(std::sqrt(len2), 5.f));
        }


        const auto zoom = -m_thumbsticks.getValue(cro::GameController::AxisRightY);
        if (zoom < -cro::GameController::LeftThumbDeadZone || zoom > cro::GameController::LeftThumbDeadZone)
        {
            float zoomRatio = static_cast<float>(zoom) / cro::GameController::AxisMax;
            m_zoomScale = std::clamp(m_zoomScale + (2.f * zoomRatio * m_zoomScale * dt), MinZoom, MaxZoom);
            zoomCamera();
        }
    }


    if (len2 != 0)
    {
        panCamera(movement * 12.f * m_sharedData.mouseSpeed);
    }

    //hmm - this removes the overlay lag (because it's actually a member of a different state
    //updated *after* this one) but it does mean the system is being double-processed.
    m_sharedData.minimapData.mapScene->getSystem<cro::CameraSystem>()->process(0.f);


    //update shader properties
    if (m_heatAmount != m_heatTarget)
    {
        if (m_heatAmount < m_heatTarget)
        {
            m_heatAmount = std::min(m_heatAmount + (dt * 2.f), m_heatTarget);
        }
        else
        {
            m_heatAmount = std::max(m_heatTarget, m_heatAmount - (dt * 2.f));
        }
        glUseProgram(m_sharedData.minimapData.shaderID);
        glUniform1f(m_sharedData.minimapData.heatUniform, m_heatAmount);
    }

    m_scene.simulate(dt);
    return true;
}

void MapOverviewState::render()
{
    auto oldCam = m_sharedData.minimapData.mapScene->setActiveCamera(m_mapCamera);
    m_mapBuffer.clear(cro::Colour::Transparent);
    m_sharedData.minimapData.mapScene->render();
    m_mapBuffer.display();
    m_sharedData.minimapData.mapScene->setActiveCamera(oldCam);

    m_scene.render();
}

//private
void MapOverviewState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::SpriteAnimator>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);

    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
}

void MapOverviewState::loadAssets()
{
    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_scene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_scene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");

    const auto size = cro::App::getWindow().getSize();
    m_mapBuffer.create(size.x, size.y);

    m_ditherShader.loadFromString(cro::RenderSystem2D::getDefaultVertexShader(), DitherFragment, "#define TEXTURED\n");
    m_ditherUniform = m_ditherShader.getUniformID("u_displayArea");
}

void MapOverviewState::buildScene()
{
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

                //check hole number and compare with the last time this
                //menu was opened - then recentre the map if it's a new hole.
                if (m_previousMap != m_sharedData.minimapData.holeNumber)
                {
                    m_mapTitleText.getComponent<cro::Text>().setString(m_sharedData.minimapData.courseName);

                    recentreMap();
                    updateNormals();
                    m_previousMap = m_sharedData.minimapData.holeNumber;
                }


                if (currTime == 1)
                {
                    state = RootCallbackData::FadeOut;
                    e.getComponent<cro::Callback>().active = false;

                    m_scene.setSystemActive<cro::AudioPlayerSystem>(true);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
                break;
            case RootCallbackData::FadeOut:
                currTime = std::max(0.f, currTime - (dt * 2.f));
                e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
                if (currTime == 0)
                {
                    requestStackPop();

                    state = RootCallbackData::FadeIn;
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

    //map entity
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_mapBuffer.getTexture());
    entity.getComponent<cro::Drawable2D>().setShader(&m_ditherShader);
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());
    auto mapEnt = entity;

    recentreMap();

    //menu background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/overview_controls.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 1.6f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });
    entity.addComponent<UIElement>().relativePosition = { 0.f, -0.49f };
    entity.getComponent<UIElement>().depth = 0.6f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    auto bgNode = entity;
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    //displays the course name
    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), 56.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setVerticalSpacing(2.f);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_mapTitleText = entity;


    //displays the zoom control
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 1.6f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("control_label");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<UIElement>().relativePosition = { -0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 16.f, -(bounds.height + 16.f) };
    entity.getComponent<UIElement>().depth = 0.6f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bgNode = entity;
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 18.f, 8.f, 0.1f });
    //entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("controller");
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_controlIcon = entity;

    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 36.f, 23.f, 0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Hello");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (e.getComponent<cro::Transform>().getScale().x != 0)
            {
                const auto str = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::SpinMenu]);
                e.getComponent<cro::Text>().setString(str);
            }
        };
    bgNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());
    m_controlText = entity;



    const auto calcAlpha = [&](glm::vec2 pos)
        {
            static constexpr float fadeDistance = 40.f;
            const auto vp = m_mapCamera.getComponent<cro::Camera>().viewport;
            const auto widthX = std::round(static_cast<float>(m_mapBuffer.getSize().x) * vp.width);
            const auto leftX = m_mapBuffer.getSize().x - widthX;
            const auto rightX = leftX + widthX;

            const float fadeLeft = smoothstep(leftX, leftX + (fadeDistance * m_viewScale.x), pos.x);
            const float fadeRight = 1.f - smoothstep(rightX - (fadeDistance * m_viewScale.x), rightX, pos.x);

            //TODO something is off and making the vertical calc incorrect...
            const auto heightY = std::round(static_cast<float>(m_mapBuffer.getSize().y)/* * vp.height*/);
            const auto bottomY = m_mapBuffer.getSize().y - heightY;
            const auto topY = bottomY + heightY;

            const float fadeBottom = smoothstep(bottomY, bottomY + (fadeDistance * m_viewScale.y), pos.y);
            const float fadeTop = 1.f - smoothstep(topY - (fadeDistance * m_viewScale.y), topY, pos.y);

            return fadeLeft * fadeRight * fadeBottom * fadeTop;
        };

    //marks approx landing area of ball
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLE_FAN);
    entity.getComponent<cro::Drawable2D>().setBlendMode(cro::Material::BlendMode::Alpha);

    std::vector<cro::Vertex2D> verts;
    auto colour = TextGoldColour;
    colour.setAlpha(0.5f);
    verts.emplace_back(glm::vec2(0.f), colour);
    colour.setAlpha(0.1f);
    static constexpr float Radius = 60.f;
    static constexpr float PointCount = 32.f;
    static constexpr float ArcSize = cro::Util::Const::TAU / PointCount;

    //for (auto i = PointCount; i >= 0; i--)
    for (auto i = 0.f; i < PointCount + 1; i++)
    {
        auto p = glm::vec2(glm::cos(i * ArcSize), glm::sin(i * ArcSize)) * Radius;
        verts.emplace_back(p, colour);
    }
    verts.push_back(verts.front());

    entity.getComponent<cro::Drawable2D>().setVertexData(verts);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, calcAlpha](cro::Entity e, float)
        {
            if (m_mapCamera.isValid())
            {
                e.getComponent<cro::Transform>().setPosition(m_mapCamera.getComponent<cro::Camera>().coordsToPixel(m_sharedData.minimapData.targetPos, m_mapBuffer.getSize()));
                e.getComponent<cro::Transform>().setScale(glm::vec2(m_zoomScale * m_viewScale.y));

                //hmm this doesn't work because we have special case alpha value...
                /*const auto alpha = calcAlpha(e.getComponent<cro::Transform>().getPosition());
                for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
                {
                    v.colour.setAlpha(alpha);
                }*/
            }
        };
    mapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //marks the tee position;
    const auto& teeFont = m_sharedData.sharedResources->fonts.get(FontID::Label);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.3f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(teeFont).setString("T");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, calcAlpha](cro::Entity e, float)
        {
            if (m_mapCamera.isValid())
            {
                e.getComponent<cro::Transform>().setPosition(m_mapCamera.getComponent<cro::Camera>().coordsToPixel(m_sharedData.minimapData.teePos, m_mapBuffer.getSize()));
                e.getComponent<cro::Transform>().setScale(glm::vec2(std::round(m_zoomScale * m_viewScale.y)));

                const float alpha = calcAlpha(e.getComponent<cro::Transform>().getPosition());
                auto fill = TextNormalColour;
                fill.setAlpha(alpha);

                auto shadow = LeaderboardTextDark;
                shadow.setAlpha(alpha);
                e.getComponent<cro::Text>().setFillColour(fill);
                e.getComponent<cro::Text>().setShadowColour(shadow);
            }
        };

    //marks the pin position
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.3f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("map_flag");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, calcAlpha](cro::Entity e, float)
        {
            if (m_mapCamera.isValid())
            {
                e.getComponent<cro::Transform>().setPosition(m_mapCamera.getComponent<cro::Camera>().coordsToPixel(m_sharedData.minimapData.pinPos, m_mapBuffer.getSize()));
                e.getComponent<cro::Transform>().setScale(glm::vec2(std::max(1.f, std::round((m_zoomScale / 2.f) * m_viewScale.y))));

                const float alpha = calcAlpha(e.getComponent<cro::Transform>().getPosition());
                auto c = cro::Colour::White;
                c.setAlpha(alpha);
                e.getComponent<cro::Sprite>().setColour(c);
            }
        };



    auto updateView = [&, rootNode, mapEnt](cro::Camera& cam) mutable
    {
        const glm::vec2 size(GolfGame::getActiveTarget()->getSize());
        m_viewScale = glm::vec2(getViewScale());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_mapBuffer.create(static_cast<std::uint32_t>(size.x), static_cast<std::uint32_t>(size.y));
        mapEnt.getComponent<cro::Sprite>().setTextureRect({ glm::vec2(0.f), size });
        mapEnt.getComponent<cro::Transform>().setOrigin(size / 2.f);
        mapEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f)/m_viewScale);

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

        //calls the viewport resize for potential new aspect ratio
        zoomCamera();
    };

    entity = m_scene.getActiveCamera();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    updateView(entity.getComponent<cro::Camera>());

    m_scene.simulate(0.f);




    //camera for 3D scene
    entity = m_sharedData.minimapData.mapScene->createEntity();
    entity.setLabel("Overview Camera");
    entity.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    entity.addComponent<cro::Camera>().active = false;
    m_mapCamera = entity;

    zoomCamera();
}

void MapOverviewState::quitState()
{
    //m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}

void MapOverviewState::recentreMap()
{
    if (m_mapCamera.isValid())
    {
        m_mapCamera.getComponent<cro::Transform>().setPosition({ m_sharedData.minimapData.mapCentre.x, CamHeight, m_sharedData.minimapData.mapCentre.z });
        zoomCamera();
    }
}

void MapOverviewState::updateNormals()
{
    //const auto imageSize = m_renderBuffer.getSize();
    //constexpr auto Components = 4;

    ////so much for doing this all in the shader...
    //std::vector<std::uint8_t> image(imageSize.x * imageSize.y * Components);
    //glBindTexture(GL_TEXTURE_2D, m_sharedData.minimapData.mrt->getTexture(MRTIndex::Normal).textureID);
    //glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data());

    //const auto PixelsPerMetre = (imageSize.x / MapSize.x) * 2;
    //const auto Stride = Components * PixelsPerMetre;

    ////TODO remind ourself how to do the vert building with std::async
    //std::vector<cro::Vertex2D> verts;
    //for (auto y = 0u; y < imageSize.y; y += PixelsPerMetre)
    //{
    //    for (auto x = 0u; x < (imageSize.x * Components); x += Stride)
    //    {
    //        const auto index = y * (imageSize.x * Components) + x;

    //        if (image[index + 3] > 126
    //            /* && image[index + 1] < 0.9999f*/) //more than this we kinda assume the normal is vertical and skip it
    //        {
    //            glm::vec3 n = glm::vec3(image[index], image[index + 1], image[index + 2]);
    //            n /= 255.f;
    //            n *= 2.f;
    //            n -= 1.f;

    //            glm::vec2 normal = glm::vec2(n.x,-n.z) * 50.f;
    //            const glm::vec2 position(x / 4, y);

    //            //TODO how do we clamp the length without normalising?

    //            auto c = cro::Colour::Yellow;
    //            verts.emplace_back(position, c);
    //            float g = 1.f - std::min(1.f, glm::length2(normal) / (8.f * 8.f));
    //            c.setGreen(g);
    //            verts.emplace_back((position + normal), c);

    //            auto endPoint = verts.back().position;

    //            normal *= 0.8f;
    //            glm::vec2 cross(-normal.y, normal.x);
    //            cross *= 0.2f;
    //            verts.emplace_back(position + normal + cross, c);
    //            verts.emplace_back(position + normal - cross, c);
    //            verts.emplace_back(position + normal - cross, c);
    //            verts.emplace_back(endPoint, c);
    //            verts.emplace_back(endPoint, c);
    //            verts.emplace_back(position + normal + cross, c);
    //        }
    //    }
    //}

    //m_mapNormals.getComponent<cro::Drawable2D>().setVertexData(verts);
}

void MapOverviewState::onCachedPush()
{
    m_sharedData.minimapData.mapScene->getActiveCamera().getComponent<cro::Camera>().active = false;
    m_mapCamera.getComponent<cro::Camera>().active = true;
}

void MapOverviewState::onCachedPop()
{
    m_sharedData.minimapData.mapScene->getActiveCamera().getComponent<cro::Camera>().active = true;
    m_mapCamera.getComponent<cro::Camera>().active = false;

    m_heatAmount = 0.f;
    glUseProgram(m_sharedData.minimapData.shaderID);
    glUniform1f(m_sharedData.minimapData.heatUniform, 0.f);
}

void MapOverviewState::zoomCamera()
{
    if (m_mapCamera.isValid())
    {
        //view size actually gets smaller to make the zoom 'larger'
        const auto scale = 1.f / (m_zoomScale * BaseScaleMultiplier);
        const auto viewSize = (m_sharedData.minimapData.mapSize * scale) / 2.f;

        auto& cam = m_mapCamera.getComponent<cro::Camera>();
        //cam.setOrthographic(-viewSize.x, viewSize.x, -viewSize.y, viewSize.y, 1.f, 40.f);
        
        const auto ratio = viewSize.x / viewSize.y;
        const auto fov = 2 * std::atan(viewSize.y/CamHeight);
        cam.setPerspective(fov, ratio, 1.f, CamFar);
        
        float left = 0.f;
        float bottom = 0.f;
        float width = 1.f;
        float height = 1.f;

        glm::vec2 targetSize(m_mapBuffer.getSize());
        const float targetRatio = targetSize.x / targetSize.y;
        const float viewRatio = viewSize.x / viewSize.y;

        if (targetRatio > viewRatio)
        {
            width = viewRatio / targetRatio;
            left = (1.f - width) / 2.f;
        }
        else
        {
            height = targetRatio / viewRatio;
            bottom = (1.f - height) / 2.f;
        }
        cam.viewport = { left, bottom, width, height };

        glUseProgram(m_ditherShader.getGLHandle());
        glUniform4f(m_ditherUniform, left, bottom, width, height);

        glUseProgram(m_sharedData.minimapData.shaderID);
        glUniform1f(m_sharedData.minimapData.zoomUniform, m_zoomScale);
    }
}

void MapOverviewState::panCamera(glm::vec2 movement)
{
    if (m_mapCamera.isValid())
    {
        movement /= pixelsPerMetre();
        auto worldPos = m_mapCamera.getComponent<cro::Transform>().getPosition() + (glm::vec3(movement.x, 0.f, movement.y));
        worldPos.y = CamHeight;

        const auto minX = m_sharedData.minimapData.mapCentre.x - (m_sharedData.minimapData.mapSize.x / 2.f);
        const auto maxX = minX + m_sharedData.minimapData.mapSize.x;
        worldPos.x = std::clamp(worldPos.x, minX, maxX);
        const auto minZ = m_sharedData.minimapData.mapCentre.z - (m_sharedData.minimapData.mapSize.y / 2.f);
        const auto maxZ = minZ + m_sharedData.minimapData.mapSize.y;
        worldPos.z = std::clamp(worldPos.z, minZ, maxZ);

        m_mapCamera.getComponent<cro::Transform>().setPosition(worldPos);
    }
}

float MapOverviewState::pixelsPerMetre() const
{
    //as the camera is always centerd in pixel space we can just subtract the
    //centre from the camera's pixel position offset by 1m
    constexpr auto offset = glm::vec3(1.f, -CamFar, 1.f); //also offsets bar the far plane (as we're in perspective)
    
    const auto camPos = m_mapCamera.getComponent<cro::Transform>().getPosition();
    const auto ppm = m_mapCamera.getComponent<cro::Camera>().coordsToPixel(camPos + offset, m_mapBuffer.getSize()) - glm::vec2(cro::App::getWindow().getSize() / 2u);
    return ppm.x; //assumes the aspect ratio is correct.
}

void MapOverviewState::gotoTarget()
{
    //check if transition exists and skip this
    if (m_transitionActive)
    {
        return;
    }

    const auto startPos = m_mapCamera.getComponent<cro::Transform>().getPosition();
    auto endPos = m_sharedData.minimapData.targetPos;
    endPos.y = CamHeight;

    const auto startZ = m_zoomScale;

    cro::Entity entity = m_scene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, startPos, endPos, startZ](cro::Entity e, float dt)
        {
            auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
            ct = std::min(1.f, ct + dt);

            const auto progress = cro::Util::Easing::easeOutQuint(ct);

            m_mapCamera.getComponent<cro::Transform>().setPosition(glm::mix(startPos, endPos, progress));

            if (startZ < MaxZoom)
            {
                m_zoomScale = glm::mix(startZ, MaxZoom, progress);
                zoomCamera();
            }


            if (ct == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                m_scene.destroyEntity(e);

                m_transitionActive = false;
            }
        };


    m_transitionActive = true;
}
