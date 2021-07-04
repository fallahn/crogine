/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "MenuState.hpp"

#include <crogine/core/App.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>

#include <crogine/graphics/Shape2D.hpp>

#include <crogine/util/Wavetable.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <SDL_audio.h>

namespace
{
    constexpr std::size_t SampleRate = 48000;
    constexpr std::size_t UpdateRate = 60;
    constexpr std::size_t UpdatesPerTick = SampleRate / UpdateRate;

    constexpr std::uint8_t ChannelCount = 6;

    SDL_AudioDeviceID audioDevice = 0;

}

MenuState::MenuState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_scene         (context.appInstance.getMessageBus())
{
    //add systems to scene
    addSystems();
    //load assets (textures, shaders, models etc)
    loadAssets();
    //create some entities
    createScene();

    context.appInstance.setClearColour(cro::Colour(0.2f, 0.2f, 0.26f));
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_MOUSEMOTION)
    {
        if (evt.motion.state & SDL_BUTTON_LMASK)
        {
            m_sourceEnt.getComponent<cro::Transform>().setPosition(m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords({ evt.motion.x, evt.motion.y }));
        }
    }

    m_scene.forwardEvent(evt);
    return true;
}

void MenuState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool MenuState::simulate(float dt)
{
    static std::size_t addedSamples = 0;
    
    //update each channel
    for (auto i = 0u; i < UpdatesPerTick; ++i)
    {
        m_leftChannel.push_back(m_audioSource.wavetable[m_audioSource.tableIndex] * 0.2f);
        m_rightChannel.push_back(m_audioSource.wavetable[m_audioSource.tableIndex] * 0.2f);
        m_centreChannel.push_back(m_audioSource.wavetable[m_audioSource.tableIndex] * 0.2f);
        m_leftChannelRear.push_back(m_audioSource.wavetable[m_audioSource.tableIndex] * 0.2f);
        m_rightChannelRear.push_back(m_audioSource.wavetable[m_audioSource.tableIndex] * 0.2f);

        m_audioSource.tableIndex = (m_audioSource.tableIndex + 1) % m_audioSource.wavetable.size();
        addedSamples++;
    }

    static std::int32_t updateTicker = 0;
    updateTicker = (updateTicker + 1) % 6;

    if (updateTicker == 0)
    {
        auto bufferSize = /*(m_leftChannel.size())*/addedSamples * ChannelCount; //channel count
        //bufferSize /= 2;
        addedSamples = 0;

        static std::vector<float> mixedBuffer(bufferSize);
        for (auto i = 0u; i < bufferSize; ++i)
        {
            mixedBuffer[i] = m_leftChannel.front(); //L
            m_leftChannel.pop_front();
            
            i++;
            mixedBuffer[i] = m_rightChannel.front(); //R
            m_rightChannel.pop_front();

             i++;
            mixedBuffer[i] = m_centreChannel.front(); //C
            m_centreChannel.pop_front();

            i++;
            mixedBuffer[i] = 0.f; //LFE - TODO research how this is calc'd - sum of output through low pass filter?


            i++;
            mixedBuffer[i] = m_leftChannelRear.front(); //L2
            m_leftChannelRear.pop_front();


            i++;
            mixedBuffer[i] = m_rightChannelRear.front(); //R2
            m_rightChannelRear.pop_front();
        }

        //send buffer to output
        SDL_QueueAudio(audioDevice, mixedBuffer.data(), mixedBuffer.size() * sizeof(float));
    }

    m_scene.simulate(dt);
    return true;
}

void MenuState::render()
{
    m_scene.render(getContext().mainWindow);
}

//private
void MenuState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
}

void MenuState::loadAssets()
{
    SDL_AudioSpec spec;
    SDL_zero(spec);

    spec.freq = SampleRate;
    spec.format = AUDIO_F32;
    spec.channels = ChannelCount;
    
    audioDevice = SDL_OpenAudioDevice(nullptr, SDL_FALSE, &spec, nullptr, 0);
    if (audioDevice == 0)
    {
        LogI << SDL_GetError() << std::endl;
    }
    else
    {
        std::vector<float> padBuffer(1024);
        std::fill(padBuffer.begin(), padBuffer.end(), 0.f);

        SDL_PauseAudioDevice(audioDevice, SDL_FALSE);
        SDL_QueueAudio(audioDevice, padBuffer.data(), padBuffer.size() * sizeof(float));
    }


    m_audioSource.wavetable = cro::Util::Wavetable::sine(880.f, 1.f, 48000.f);
}

void MenuState::createScene()
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(cro::Shape::circle(0.02f, cro::Colour::Red, 16));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    auto listener = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(-0.5f, 0.5f));
    entity.addComponent<cro::Drawable2D>().setVertexData(cro::Shape::circle(0.02f, cro::Colour::Blue, 16));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    listener.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(0.5f, 0.5f));
    entity.addComponent<cro::Drawable2D>().setVertexData(cro::Shape::circle(0.02f, cro::Colour::Blue, 16));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    listener.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(-0.55f, -0.5f));
    entity.addComponent<cro::Drawable2D>().setVertexData(cro::Shape::circle(0.02f, cro::Colour::Blue, 16));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    listener.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(0.55f, -0.5f));
    entity.addComponent<cro::Drawable2D>().setVertexData(cro::Shape::circle(0.02f, cro::Colour::Blue, 16));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    listener.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(0.f, 0.6f));
    entity.addComponent<cro::Drawable2D>().setVertexData(cro::Shape::circle(0.02f, cro::Colour::Blue, 16));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    listener.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    m_sourceEnt = m_scene.createEntity();
    m_sourceEnt.addComponent<cro::Transform>();
    m_sourceEnt.addComponent<cro::Drawable2D>().setVertexData(cro::Shape::circle(0.025f, cro::Colour::Yellow));
    m_sourceEnt.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);


    auto camCallback =
        [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());

        float width = 3.f;
        float height = width * (size.y / size.x);

        cam.setOrthographic(-width / 2.f, width / 2.f, -height / 2.f, height / 2.f, -0.1f, 10.f);

    };
    auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = camCallback;
    camCallback(cam);
}
