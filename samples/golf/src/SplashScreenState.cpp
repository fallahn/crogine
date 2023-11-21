/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "SplashScreenState.hpp"
#include "ErrorCheck.hpp"
#include "golf/SharedStateData.hpp"

#include <Social.hpp>

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/graphics/Image.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    const std::string VertexShader = 
        R"(
        uniform mat4 u_worldViewMatrix;
        uniform mat4 u_projectionMatrix;

        ATTRIBUTE vec2 a_position;
        ATTRIBUTE MED vec2 a_texCoord0;
        ATTRIBUTE LOW vec4 a_colour;

        VARYING_OUT LOW vec4 v_colour;
        VARYING_OUT MED vec2 v_texCoord;

        void main()
        {
            gl_Position = u_projectionMatrix * u_worldViewMatrix * vec4(a_position, 0.0, 1.0);
            v_colour = a_colour;
            v_texCoord = a_texCoord0;
        })";

    const std::string NoiseFragment = R"(

        uniform sampler2D u_texture;
        uniform float u_time;
        uniform float u_lineCount = 6000.0;

        VARYING_IN vec4 v_colour;
        VARYING_IN vec2 v_texCoord;

        const float noiseStrength = 0.7;
        const float maxOffset = 1.0 / 450.0;

        const float centreDistanceSquared = 0.25;
        float distanceSquared(vec2 coord)
        {
            return dot(coord, coord);
        }

        OUTPUT

        void main()
        {
            vec2 texCoord = v_texCoord;

            vec2 offset = vec2((maxOffset / 2.0) - (texCoord.x * maxOffset), (maxOffset / 2.0) - (texCoord.y * maxOffset));
            vec3 colour = vec3(0.0);

            colour.r = TEXTURE(u_texture, texCoord + offset).r;
            colour.g = TEXTURE(u_texture, texCoord).g;
            colour.b = TEXTURE(u_texture, texCoord - offset).b;

        /*noise*/
            float x = (texCoord.x + 4.0) * texCoord.y * u_time * 10.0;
            x = mod(x, 13.0) * mod(x, 123.0);
            float grain = mod(x, 0.01) - 0.005;
            vec3 result = colour + vec3(clamp(grain * 100.0, 0.0, 0.07));

        /*scanlines*/
            vec2 sinCos = vec2(sin(texCoord.y * u_lineCount), cos(texCoord.y * u_lineCount + u_time));
            result += colour * vec3(sinCos.x, sinCos.y, sinCos.x) * (noiseStrength * 0.08);
            colour += (result - colour) * noiseStrength;
            FRAG_OUT = vec4(colour, 1.0) * v_colour;
        })";

    const std::string ScanLineFragment = R"(
        uniform sampler2D u_texture;

        VARYING_IN vec4 v_colour;
        VARYING_IN vec2 v_texCoord;

        OUTPUT

        void main()
        {
            vec3 colour = TEXTURE(u_texture, v_texCoord).rgb;
            colour.rgb *= mod(floor(gl_FragCoord.y), 2)* 0.5 + 0.5;
            FRAG_OUT = vec4(colour, 0.5) * v_colour;
        })";

    constexpr float ScanlineCount = 6500.f;
    constexpr float FadeTime = 1.f;
    constexpr float HoldTime = 3.5f;

    constexpr glm::vec2 DisplaySize(1920.f, 1080.f);
}

SplashState::SplashState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_uiScene           (context.appInstance.getMessageBus()),
    m_timer             (0.f),
    m_windowRatio       (1.f),
    m_timeUniform       (-1),
    m_scanlineUniform   (-1)
{
    Social::setStatus(Social::InfoID::Menu, { "Watching The Intro" });

    addSystems();
    loadAssets();
    createUI();
}

//public
bool SplashState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYUP
        || evt.type == SDL_CONTROLLERBUTTONUP
        || evt.type == SDL_MOUSEBUTTONUP)
    {
        requestStackClear();
        requestStackPush(StateID::Menu);
        if (m_sharedData.showTutorialTip)
        {
            m_sharedData.errorMessage = "Welcome";
            requestStackPush(StateID::MessageOverlay);
        }
#ifdef USE_GNS
#undef USE_RSS
        else
        {
            if (!Social::isSteamdeck())
            {
                requestStackPush(StateID::News);
            }
        }
#endif
#ifdef USE_RSS
        else
        {
            requestStackPush(StateID::News);
        }
#endif
    }

    m_uiScene.forwardEvent(evt);
    return false;
}

void SplashState::handleMessage(const cro::Message& msg)
{
    m_uiScene.forwardMessage(msg);
}

bool SplashState::simulate(float dt)
{
    m_timer += dt;

    static float accum = 0.f;
    accum += dt;

    glCheck(glUseProgram(m_noiseShader.getGLHandle()));
    glCheck(glUniform1f(m_timeUniform, accum * (1.0f * m_windowRatio)));

    m_uiScene.simulate(dt);
    m_video.update(dt);

    if (m_video.getDuration() != 0 &&
        m_video.getPosition() / m_video.getDuration() == 1)
    {
        requestStackClear();
        requestStackPush(StateID::Menu);
        if (m_sharedData.showTutorialTip)
        {
            m_sharedData.errorMessage = "Welcome";
            requestStackPush(StateID::MessageOverlay);
        }
#ifdef USE_GNS
#undef USE_RSS
        else
        {
            if (!Social::isSteamdeck())
            {
                requestStackPush(StateID::News);
            }
        }
#endif
#ifdef USE_RSS
    else
    {
        requestStackPush(StateID::News);
    }
#endif
    }

    return false;
}

void SplashState::render()
{
    m_uiScene.render();
}

//private
void SplashState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioSystem>(mb);
}

void SplashState::loadAssets()
{
    m_noiseShader.loadFromString(VertexShader, NoiseFragment);
    m_scanlineShader.loadFromString(VertexShader, ScanLineFragment);

    m_timeUniform = m_noiseShader.getUniformID("u_time");
    m_scanlineUniform = m_noiseShader.getUniformID("u_lineCount");

    if (m_video.loadFromFile("assets/intro.mpg"))
    {
        glm::vec2 size(m_video.getTexture().getSize());
        
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ DisplaySize.x / 2.f, DisplaySize.y / 2.f, -0.2f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>(m_video.getTexture());

        auto scale = DisplaySize.x / size.x;
        entity.getComponent<cro::Transform>().setScale({ scale, scale });
        entity.getComponent<cro::Transform>().setOrigin(size / 2.f);


        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            //we can get away with static var because splash screen
            //is only ever shown one per game run.
            static float currTime = 2.f;
            currTime -= dt;

            if (currTime < 0)
            {
                m_video.play();
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
        };
    }
    else
    {
        if (!m_texture.loadFromFile("assets/images/startup.png"))
        {
            cro::Image img;
            img.create(128, 128, cro::Colour::Magenta);
            m_texture.loadFromImage(img);
        }

        //static image
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
        entity.addComponent<cro::Drawable2D>().setShader(&m_noiseShader);
        entity.addComponent<cro::Sprite>(m_texture);

        //audio
        if (auto id = m_audioResource.load("assets/sound/startup.wav"); id > 0)
        {
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::AudioEmitter>(m_audioResource.get(id));
            entity.getComponent<cro::AudioEmitter>().setMixerChannel(2);
            entity.getComponent<cro::AudioEmitter>().play();
        }


        //flickering image
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 9.f, 7.f, -0.2f });
        entity.addComponent<cro::Drawable2D>().setShader(&m_scanlineShader);
        entity.addComponent<cro::Sprite>(m_texture);
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float)
        {
            if (cro::Util::Random::value(0, 10) == 10)
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            }
            else
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            }
        };

        //black fade
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
        entity.addComponent<cro::Drawable2D>().getVertexData() =
        {
            cro::Vertex2D(glm::vec2(0.f, 1080.f), cro::Colour::Black),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::Black),
            cro::Vertex2D(glm::vec2(1920.f, 1080.f), cro::Colour::Black),
            cro::Vertex2D(glm::vec2(1920.f, 0.f), cro::Colour::Black)
        };
        entity.getComponent<cro::Drawable2D>().updateLocalBounds();

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(0.f, 0);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            auto& [currTime, state] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
            switch (state)
            {
            default: break;
            case 0:
                currTime = std::min(FadeTime, currTime + dt);
                {
                    float alpha = 1.f - (currTime / FadeTime);
                    auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
                    for (auto& v : verts)
                    {
                        v.colour.setAlpha(alpha);
                    }

                    if (currTime == FadeTime)
                    {
                        state = 1;
                        currTime = 0.f;
                    }
                }
                break;
            case 1:
                currTime = std::min(HoldTime, currTime + dt);
                if (currTime == HoldTime)
                {
                    currTime = 0.f;
                    state = 2;
                }
                break;
            case 2:
                currTime = std::min(FadeTime, currTime + dt);
                {
                    float alpha = currTime / FadeTime;
                    auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
                    for (auto& v : verts)
                    {
                        v.colour.setAlpha(alpha);
                    }

                    if (currTime == FadeTime)
                    {
                        e.getComponent<cro::Callback>().active = false;
                        requestStackClear();
                        requestStackPush(StateID::Menu);
                        if (m_sharedData.showTutorialTip)
                        {
                            m_sharedData.errorMessage = "Welcome";
                            requestStackPush(StateID::MessageOverlay);
                        }
#ifdef USE_RSS
                        else
                        {
                            requestStackPush(StateID::News);
                        }
#endif
                    }
                }
                break;
            }
        };
    }
}

void SplashState::createUI()
{
    //this is called when the window is resized to automatically update the camera's matrices/viewport
    auto camEnt = m_uiScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&SplashState::updateView, this, std::placeholders::_1);
}

void SplashState::updateView(cro::Camera& cam)
{
    //fixed view @ 1080p (letterboxed to window)
    glm::vec2 size(cro::App::getWindow().getSize());

    float windowRatio = size.x / size.y;
    static constexpr float viewRatio = DisplaySize.x / DisplaySize.y;
    cro::FloatRect vp = { 0.f, 0.f, 1.f, 1.f };

    if (windowRatio > viewRatio)
    {
        vp.width = viewRatio / windowRatio;
        vp.left = (1.f - vp.width) / 2.f;
    }
    else
    {
        vp.height = windowRatio / viewRatio;
        vp.bottom = (1.f - vp.height) / 2.f;
    }

    cam.setOrthographic(0.f, DisplaySize.x, 0.f, DisplaySize.y, -2.f, 10.f);
    cam.viewport = vp;

    m_windowRatio = size.y / DisplaySize.y;

    glCheck(glUseProgram(m_noiseShader.getGLHandle()));
    glCheck(glUniform1f(m_scanlineUniform, m_windowRatio * ScanlineCount));
}