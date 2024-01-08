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
            uniform sampler2D u_maskMap;
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
                vec3 pos = texture(u_worldPos, v_texCoord).rgb;

float heightIntensity = smoothstep(0.1, 5.0, pos.y);
heightIntensity = 0.05 + (0.95 * heightIntensity);

vec4 gridColour = vec4(vec3(0.8, 0.7, 0.2) * heightIntensity, colour.a);




//vec2 gridRes = vec2(320.0, 200.0) * GridThickness * u_gridScale;
//float gridThickness = GridThickness / u_gridScale;
//
//vec2 grid = fract(v_texCoord * gridRes);
//float gridAmount = 1.0 - (step(gridThickness, grid.x) * step(gridThickness, grid.y));



float contourAmount = smoothstep(1.0 - (0.12 / u_gridScale), 1.0 - (0.1 / u_gridScale), fract(pos.y * u_gridScale));
contourAmount *= texture(u_maskMap, v_texCoord).r;

//stops contours 'spreading' over almost flat areas
vec3 normal = texture(u_normalMap, v_texCoord).rgb;
contourAmount *= 1.0 - step(0.995, dot(vec3(0.0, 1.0, 0.0), normal));

FRAG_OUT = colour + (gridColour * contourAmount * u_gridAmount);



                vec3 c = BaseHeatColour;
                c.x += mod(pos.y / (8.0 - (u_gridScale / 3.0)), 1.0); //6.0 is MaxZoom, 8.0 is just a magic number
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

    constexpr float MaxZoom = 12.f;
    constexpr float MinZoom = 1.f;
    constexpr float BaseScaleMultiplier = 0.8f;
    constexpr std::int32_t MaxFingers = 2;
}

MapOverviewState::MapOverviewState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus()),
    m_sharedData        (sd),
    m_previousMap       (-1),
    m_viewScale         (2.f),
    m_shaderValueIndex  (0),
    m_zoomScale         (1.f),
    m_fingerCount       (0)
{
    ctx.mainWindow.setMouseCaptured(false);

    CRO_ASSERT(sd.minimapData.mrt, "");
    addSystems();
    loadAssets();
    buildScene();
}

//public
bool MapOverviewState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: 
            if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
            {
                m_shaderValueIndex = (m_shaderValueIndex + 1) % m_shaderValues.size();
                refreshMap();
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
            {
                m_shaderValueIndex = (m_shaderValueIndex + (m_shaderValues.size() - 1)) % m_shaderValues.size();
                refreshMap();
            }
            break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
        case SDLK_6:
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
        case cro::GameController::ButtonTrackpad:
            quitState();
            return false;
        case cro::GameController::ButtonRightShoulder:
            m_shaderValueIndex = (m_shaderValueIndex + 1) % m_shaderValues.size();
            refreshMap();
            break;
        case cro::GameController::ButtonLeftShoulder:
            m_shaderValueIndex = (m_shaderValueIndex + (m_shaderValues.size() - 1)) % m_shaderValues.size();
            refreshMap();
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
    else if (evt.type == SDL_CONTROLLERTOUCHPADDOWN)
    {
        m_fingerCount++;
        if (evt.ctouchpad.finger < MaxFingers)
        {
            m_trackpadFingers[evt.ctouchpad.finger].prevPosition = { evt.ctouchpad.x, 1.f - evt.ctouchpad.y };
            m_trackpadFingers[evt.ctouchpad.finger].currPosition = { evt.ctouchpad.x, 1.f - evt.ctouchpad.y };
        }
        //LogI << "Finger count " << m_fingerCount << " finger id " << evt.ctouchpad.finger << std::endl;
    }
    else if (evt.type == SDL_CONTROLLERTOUCHPADUP)
    {
        m_fingerCount--;
        if (evt.ctouchpad.finger < MaxFingers)
        {
            //this effectively resets the velocity to 0
            m_trackpadFingers[evt.ctouchpad.finger].currPosition = m_trackpadFingers[evt.ctouchpad.finger].prevPosition;
        }
        //LogI << "Finger count " << m_fingerCount << " finger id " << evt.ctouchpad.finger << std::endl;
    }
    else if (evt.type == SDL_CONTROLLERTOUCHPADMOTION)
    {
        if (evt.ctouchpad.finger < MaxFingers)
        {
            glm::vec2 pos({ evt.ctouchpad.x, 1.f - evt.ctouchpad.y });
            m_trackpadFingers[evt.ctouchpad.finger].prevPosition = m_trackpadFingers[evt.ctouchpad.finger].currPosition;
            m_trackpadFingers[evt.ctouchpad.finger].currPosition = pos;
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
        if (evt.motion.state & SDL_BUTTON_MIDDLE)
        {
            const float Scale = 1.f / m_mapEnt.getComponent<cro::Transform>().getScale().x;

            glm::vec2 movement(0.f);
            movement.x -= static_cast<float>(evt.motion.xrel) * Scale;
            movement.y += static_cast<float>(evt.motion.yrel) * Scale;
            movement /= m_viewScale;
            
            pan(movement);
        }
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        const auto amount = evt.wheel.preciseY;
        m_zoomScale = std::clamp(m_zoomScale + amount, MinZoom, MaxZoom);
        rescaleMap();
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
            refreshMap();
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
        movement.y += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Down]))
    {
        movement.y -= 1.f;
    }
    

    auto len2 = glm::length2(movement);
    if (len2 > 1)
    {
        movement /= std::sqrt(len2);
    }

    if (len2 == 0)
    {
        auto controllerID = activeControllerID(m_sharedData.inputBinding.playerID);

        //check controller analogue
        auto x = cro::GameController::getAxisPosition(controllerID, cro::GameController::AxisLeftX);
        if (x > LeftThumbDeadZone || x < -LeftThumbDeadZone)
        {
            movement.x = static_cast<float>(x) / cro::GameController::AxisMax;
        }

        auto y = cro::GameController::getAxisPosition(controllerID, cro::GameController::AxisLeftY);
        if (y > LeftThumbDeadZone || y < -LeftThumbDeadZone)
        {
            movement.y = -static_cast<float>(y) / cro::GameController::AxisMax;
        }
        len2 = glm::length2(movement);
        if (len2 != 0)
        {
            movement = glm::normalize(movement) * std::min(1.f, std::pow(std::sqrt(len2), 5.f));
        }


        auto zoom = -cro::GameController::getAxisPosition(controllerID, cro::GameController::AxisRightY);
        if (zoom < -LeftThumbDeadZone || zoom > LeftThumbDeadZone)
        {
            zoom /= cro::GameController::AxisMax;
            m_zoomScale = std::clamp(m_zoomScale + (2.f * zoom * m_zoomScale * dt), MinZoom, MaxZoom);
            rescaleMap();
        }
    }


    if (len2 != 0)
    {
        auto origin = m_mapEnt.getComponent<cro::Transform>().getOrigin();
        origin += (glm::vec3(movement, 0.f) * 1650.f * (1.f / m_zoomScale)) * dt;
        glm::vec2 bounds(m_renderBuffer.getSize());
        
        origin.x = std::clamp(origin.x, 0.f, bounds.x);
        origin.y = std::clamp(origin.y, 0.f, bounds.y);
        m_mapEnt.getComponent<cro::Transform>().setOrigin(origin);
    }



    //check for touchpad input
    //if (m_fingerCount == 1)
    //{
    //    const auto doPan = [&](std::int32_t finger)
    //    {
    //        //map finger to screen space (correct for aspect ratio? pad is not necessarily a fixed shape)

    //        glm::vec2 screenMotion = glm::vec2(cro::App::getWindow().getSize()) * (m_trackpadFingers[finger].prevPosition - m_trackpadFingers[finger].currPosition);

    //        //convert screen space to world coords
    //        const float Scale = 1.f / m_mapEnt.getComponent<cro::Transform>().getScale().x;
    //        screenMotion *= Scale;
    //        screenMotion /= m_viewScale;

    //        //pan as if mouse movement
    //        pan(screenMotion);
    //    };

    //    //pan
    //    if (auto vel = m_trackpadFingers[0].currPosition - m_trackpadFingers[0].prevPosition; glm::length2(vel) != 0)
    //    {
    //        doPan(0);
    //    }
    //    else
    //    {
    //        vel = m_trackpadFingers[1].currPosition - m_trackpadFingers[1].prevPosition;
    //        doPan(1);
    //    }
    //}
    //else if (m_fingerCount == 2)
    //{
    //    //zoom - move the position of the second finger
    //    //relative to the first and adjust velocity
    //    glm::vec2 f2 = m_trackpadFingers[1].currPosition - m_trackpadFingers[0].currPosition;
    //    glm::vec2 f2v = (m_trackpadFingers[1].currPosition - m_trackpadFingers[1].prevPosition) - (m_trackpadFingers[0].currPosition - m_trackpadFingers[0].prevPosition);

    //    //then test to see if new velocity moves towards
    //    //the first finger, or away
    //    float amount = 0.f;
    //    if (glm::length2(f2 + f2v) > glm::length2(f2))
    //    {
    //        //moving away
    //        amount = 0.1f;
    //    }
    //    else
    //    {
    //        //moving towards each other
    //        amount = -0.1f;
    //    }

    //    if (amount != 0)
    //    {
    //        m_zoomScale = std::clamp(m_zoomScale + amount, MinZoom, MaxZoom);
    //        rescaleMap();
    //    }
    //}

    //update shader properties
    glUseProgram(m_slopeShader.getGLHandle());
    glUniform1f(m_shaderUniforms.transparency, (m_zoomScale / MaxZoom) * (1.f - (m_shaderValues[m_shaderValueIndex].first + m_shaderValues[m_shaderValueIndex].second)));


    m_scene.simulate(dt);
    return true;
}

void MapOverviewState::render()
{
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

    auto buffSize = m_sharedData.minimapData.mrt->getSize();
    m_renderBuffer.create(buffSize.x, buffSize.y, false);

    m_mapShader.loadFromString(cro::SimpleQuad::getDefaultVertexShader(), MinimapFragment);
    m_shaderUniforms.posMap = m_mapShader.getUniformID("u_worldPos");
    m_shaderUniforms.maskMap = m_mapShader.getUniformID("u_maskMap");
    m_shaderUniforms.normalMap = m_mapShader.getUniformID("u_normalMap");
    m_shaderUniforms.heatAmount = m_mapShader.getUniformID("u_heatAmount");
    m_shaderUniforms.gridAmount = m_mapShader.getUniformID("u_gridAmount");
    m_shaderUniforms.gridScale = m_mapShader.getUniformID("u_gridScale");

    m_mapQuad.setTexture(m_sharedData.minimapData.mrt->getTexture(MRTIndex::Colour), m_renderBuffer.getSize());
    m_mapQuad.setShader(m_mapShader);

    m_slopeShader.loadFromString(cro::RenderSystem2D::getDefaultVertexShader(), MiniSlopeFragment);
    m_shaderUniforms.transparency = m_slopeShader.getUniformID("u_transparency");

    m_mapString.setFont(m_sharedData.sharedResources->fonts.get(FontID::Label));
    m_mapString.setFillColour(TextNormalColour);
    m_mapString.setShadowColour(LeaderboardTextDark);
    m_mapString.setShadowOffset({ 8.f, -8.f });
    m_mapString.setCharacterSize(LabelTextSize * 8); //really should be reading the texture scale
    m_mapString.setAlignment(2);
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
                m_mapText.getComponent<cro::Text>().setString(m_sharedData.minimapData.courseName);

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

                refreshMap();
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
    refreshMap();
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_renderBuffer.getTexture());
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());
    m_mapEnt = entity;
    recentreMap();

    //menu background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 1.6f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_sharedData.sharedResources->textures.get("assets/golf/images/overview.png"));
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });
    entity.addComponent<UIElement>().relativePosition = {0.f, -0.49f };
    entity.getComponent<UIElement>().depth = 0.6f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    auto bgNode = entity;
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), 56.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setVerticalSpacing(2.f);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_mapText = entity;

    //draws the slope based on normals
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINES);
    entity.getComponent<cro::Drawable2D>().setShader(&m_slopeShader);

    m_mapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_mapNormals = entity;

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

        rescaleMap();
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());

    m_scene.simulate(0.f);
}

void MapOverviewState::quitState()
{
    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}

void MapOverviewState::recentreMap()
{
    glm::vec2 windowSize(cro::App::getWindow().getSize());
    glm::vec2 mapSize(m_renderBuffer.getSize());

    m_mapEnt.getComponent<cro::Transform>().setOrigin(mapSize / 2.f);
    m_mapEnt.getComponent<cro::Transform>().setScale((glm::vec2(windowSize.x / mapSize.x) / m_viewScale.x) * BaseScaleMultiplier);
    m_zoomScale = 1.f;
}

void MapOverviewState::rescaleMap()
{
    glm::vec2 windowSize(cro::App::getWindow().getSize());
    glm::vec2 mapSize(m_renderBuffer.getSize());

    const float baseScale = ((windowSize.x / mapSize.x) / m_viewScale.x) * BaseScaleMultiplier;
    m_mapEnt.getComponent<cro::Transform>().setScale(glm::vec2(baseScale * m_zoomScale));

    refreshMap();
}

void MapOverviewState::refreshMap()
{
    static constexpr std::int32_t PosSlot = 6;
    static constexpr std::int32_t MaskSlot = 7;
    static constexpr std::int32_t NormalSlot = 8;

    glActiveTexture(GL_TEXTURE0 + PosSlot);
    glBindTexture(GL_TEXTURE_2D, m_sharedData.minimapData.mrt->getTexture(MRTIndex::Position).textureID);

    glActiveTexture(GL_TEXTURE0 + MaskSlot);
    glBindTexture(GL_TEXTURE_2D, m_sharedData.minimapData.mrt->getTexture(MRTIndex::Count).textureID);

    glActiveTexture(GL_TEXTURE0 + NormalSlot);
    glBindTexture(GL_TEXTURE_2D, m_sharedData.minimapData.mrt->getTexture(MRTIndex::Normal).textureID);

    glUseProgram(m_mapShader.getGLHandle());
    glUniform1i(m_shaderUniforms.posMap, PosSlot);
    glUniform1i(m_shaderUniforms.maskMap, MaskSlot);
    glUniform1i(m_shaderUniforms.normalMap, NormalSlot);

    //glUniform1f(m_shaderUniforms.gridAmount, m_shaderValues[m_shaderValueIndex].first);
    glUniform1f(m_shaderUniforms.heatAmount, m_shaderValues[m_shaderValueIndex].second);
    glUniform1f(m_shaderUniforms.gridScale, /*std::ceil(m_zoomScale / 4.f)*/std::round(m_zoomScale));

    const float MapScale = static_cast<float>(m_renderBuffer.getSize().x) / MapSize.x;

    glm::vec2 teePos = 
    {
        std::round(m_sharedData.minimapData.teePos.x),
        std::round(-m_sharedData.minimapData.teePos.z)
    };
    //glm::vec2 pinPos =
    //{
    //    std::round(m_sharedData.minimapData.pinPos.x),
    //    std::round(-m_sharedData.minimapData.pinPos.z),
    //};

    auto charScale = std::round(MaxZoom - (m_zoomScale - MinZoom));
    charScale = std::round((charScale / MaxZoom) * 8.f);
    m_mapString.setCharacterSize(LabelTextSize * charScale);
    m_mapString.setShadowOffset({ charScale, -charScale });

    m_renderBuffer.clear(cro::Colour::Transparent);
    m_mapQuad.draw();
    m_mapString.setString("T");
    m_mapString.setPosition(teePos * MapScale);
    m_mapString.draw();
    //
    //m_mapString.setString("P");
    //m_mapString.setPosition(pinPos * MapScale);
    //m_mapString.draw();

    m_renderBuffer.display();
}

void MapOverviewState::updateNormals()
{
    const auto imageSize = m_renderBuffer.getSize();

    //so much for doing this all in the shader...
    std::vector<float> image(imageSize.x * imageSize.y * 4);
    glBindTexture(GL_TEXTURE_2D, m_sharedData.minimapData.mrt->getTexture(MRTIndex::Normal).textureID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, image.data());

    //TODO we already have an image mask stored from which we could get the terrain
    std::vector<float> mask(imageSize.x * imageSize.y);
    glBindTexture(GL_TEXTURE_2D, m_sharedData.minimapData.mrt->getTexture(MRTIndex::Count).textureID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, mask.data());


    const auto PixelsPerMetre = (imageSize.x / MapSize.x) * 2;
    const auto Stride = 4 * PixelsPerMetre;

    //TODO remind ourself how to do the vert building with std::async
    std::vector<cro::Vertex2D> verts;
    for (auto y = 0u; y < imageSize.y; y += PixelsPerMetre)
    {
        for (auto x = 0u; x < (imageSize.x * 4); x += Stride)
        {
            const auto index = y * (imageSize.x * 4) + x;

            if (mask[index / 4] > 0.5f &&
                image[index + 1] < 0.9999f) //more than this we kinda assume the normal is vertical and skip it
            {
                glm::vec2 normal = glm::vec2(image[index], -image[index + 2]) * 50.f;
                glm::vec2 position(x / 4, y);

                //TODO how do we clamp the length without normalising?

                auto c = cro::Colour::Yellow;
                verts.emplace_back(position, c);
                float g = 1.f - std::min(1.f, glm::length2(normal) / (8.f * 8.f));
                c.setGreen(g);
                verts.emplace_back((position + normal), c);

                auto endPoint = verts.back().position;

                normal *= 0.8f;
                glm::vec2 cross(-normal.y, normal.x);
                cross *= 0.2f;
                verts.emplace_back(position + normal + cross, c);
                verts.emplace_back(position + normal - cross, c);
                verts.emplace_back(position + normal - cross, c);
                verts.emplace_back(endPoint, c);
                verts.emplace_back(endPoint, c);
                verts.emplace_back(position + normal + cross, c);
            }
        }
    }

    m_mapNormals.getComponent<cro::Drawable2D>().setVertexData(verts);
}

void MapOverviewState::pan(glm::vec2 movement)
{
    auto position = m_mapEnt.getComponent<cro::Transform>().getOrigin();
    position += glm::vec3(movement, 0.f);

    position.x = std::floor(position.x);
    position.y = std::floor(position.y);

    glm::vec2 bounds(m_renderBuffer.getSize());

    position.x = std::clamp(position.x, 0.f, bounds.x);
    position.y = std::clamp(position.y, 0.f, bounds.y);

    m_mapEnt.getComponent<cro::Transform>().setOrigin(position);
}
