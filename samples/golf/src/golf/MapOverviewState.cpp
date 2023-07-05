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

namespace
{
    const std::string MinimapFragment = 
        R"(
            uniform sampler2D u_texture;

            uniform sampler2D u_worldPos;
            uniform sampler2D u_normal;

            VARYING_IN vec2 v_texCoord;
            VARYING_IN vec4 v_colour;

            OUTPUT

            #define TAU 6.283185

            const vec4 ContourColour = vec4(1.0, 0.0, 0.0, 1.0);
            const float ContourSpacing = 2.0 * TAU;

            const vec3 BaseColour = vec3(0.827, 0.599, 0.91); //stored as HSV to save on a conversion
            vec3 hsv2rgb(vec3 c)
            {
                vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
                vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
                return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
            }


            void main()
            {
                vec4 colour = texture(u_texture, v_texCoord) * v_colour;
                vec3 pos = texture(u_worldPos, v_texCoord).rgb;

                float contourAmount = step(0.9, sin(pos.z * ContourSpacing));
                FRAG_OUT = mix(colour, ContourColour, contourAmount);

                vec3 c = BaseColour;
                c.x += mod(pos.y / 8.0, 1.0);
                c = hsv2rgb(c);

                FRAG_OUT.rgb = mix(FRAG_OUT.rgb, c, 0.5);
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
}

MapOverviewState::MapOverviewState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus()),
    m_sharedData        (sd),
    m_previousMap       (-1),
    m_viewScale         (2.f),
    m_zoomScale         (1.f)
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
                //TODO increment effect
                refreshMap();
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
            {
                //TODO increment effect
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
        case cro::GameController::ButtonLeftStick:
            quitState();
            return false;
        case cro::GameController::ButtonRightShoulder:
        case cro::GameController::ButtonLeftShoulder:
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
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
        if (evt.motion.state & SDL_BUTTON_MIDDLE)
        {
            const float Scale = 1.f / m_mapEnt.getComponent<cro::Transform>().getScale().x;

            auto position = m_mapEnt.getComponent<cro::Transform>().getOrigin();
            position.x -= static_cast<float>(evt.motion.xrel) * Scale;
            position.y += static_cast<float>(evt.motion.yrel) * Scale;

            position.x = std::floor(position.x);
            position.y = std::floor(position.y);

            glm::vec2 bounds(m_renderBuffer.getSize());

            position.x = std::clamp(position.x, 0.f, bounds.x);
            position.y = std::clamp(position.y, 0.f, bounds.y);

            m_mapEnt.getComponent<cro::Transform>().setOrigin(position);
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
    if (len2 == 0)
    {
        //TODO check controller analogue
    }

    if (len2 > 1)
    {
        movement /= std::sqrt(len2);
    }
    
    if (len2 != 0)
    {
        auto origin = m_mapEnt.getComponent<cro::Transform>().getOrigin();
        origin += (glm::vec3(movement, 0.f) * 650.f * (1.f / m_zoomScale)) * dt;
        glm::vec2 bounds(m_renderBuffer.getSize());
        
        origin.x = std::clamp(origin.x, 0.f, bounds.x);
        origin.y = std::clamp(origin.y, 0.f, bounds.y);
        m_mapEnt.getComponent<cro::Transform>().setOrigin(origin);
    }

    //update shader properties
    glUseProgram(m_slopeShader.getGLHandle());
    glUniform1f(m_shaderUniforms.transparency, m_zoomScale / MaxZoom);


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

    m_mapQuad.setTexture(m_sharedData.minimapData.mrt->getTexture(0), m_renderBuffer.getSize());
    m_mapQuad.setShader(m_mapShader);

    m_slopeShader.loadFromString(cro::RenderSystem2D::getDefaultVertexShader(), MiniSlopeFragment);
    m_shaderUniforms.transparency = m_slopeShader.getUniformID("u_transparency");
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

            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;

                m_scene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                //check hole number and compare with the last time this
                //menu was opened - then recentre the map if it's a new hole.
                if (m_previousMap != m_sharedData.minimapData.holeNumber)
                {
                    m_mapText.getComponent<cro::Text>().setString(m_sharedData.minimapData.courseName);

                    recentreMap();
                    updateNormals();
                    m_previousMap = m_sharedData.minimapData.holeNumber;
                }

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
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 1.6f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("message_board");
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
    m_mapEnt.getComponent<cro::Transform>().setScale(glm::vec2(windowSize.x / mapSize.x) / m_viewScale.x);
    m_zoomScale = 1.f;
}

void MapOverviewState::rescaleMap()
{
    glm::vec2 windowSize(cro::App::getWindow().getSize());
    glm::vec2 mapSize(m_renderBuffer.getSize());

    const float baseScale = (windowSize.x / mapSize.x) / m_viewScale.x;
    m_mapEnt.getComponent<cro::Transform>().setScale(glm::vec2(baseScale * m_zoomScale));
}

void MapOverviewState::refreshMap()
{
    static constexpr std::int32_t PosSlot = 6;

    glActiveTexture(GL_TEXTURE0 + PosSlot);
    glBindTexture(GL_TEXTURE_2D, m_sharedData.minimapData.mrt->getTexture(1).textureID);

    glUseProgram(m_mapShader.getGLHandle());
    glUniform1i(m_shaderUniforms.posMap, PosSlot);

    m_renderBuffer.clear(cro::Colour::Transparent);
    m_mapQuad.draw();
    m_renderBuffer.display();
}

void MapOverviewState::updateNormals()
{
    const auto imageSize = m_renderBuffer.getSize();

    //so much for doing this all in the shader...
    std::vector<float> image(imageSize.x * imageSize.y * 4);
    glBindTexture(GL_TEXTURE_2D, m_sharedData.minimapData.mrt->getTexture(2).textureID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, image.data());

    //TODO we already have an image mask stored from which we could get the terrain
    std::vector<float> mask(imageSize.x * imageSize.y);
    glBindTexture(GL_TEXTURE_2D, m_sharedData.minimapData.mrt->getTexture(3).textureID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, mask.data());


    const auto PixelsPerMetre = imageSize.x / MapSize.x;
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
            }
        }
    }

    m_mapNormals.getComponent<cro::Drawable2D>().setVertexData(verts);
}