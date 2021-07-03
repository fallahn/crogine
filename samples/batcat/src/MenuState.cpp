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

#include <crogine/util/Wavetable.hpp>

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
    m_tableIndex    (0)
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

    return true;
}

void MenuState::handleMessage(const cro::Message& msg)
{

}

bool MenuState::simulate(float dt)
{
    //update each channel
    for (auto i = 0u; i < UpdatesPerTick; ++i)
    {
        m_leftChannel.push_back(m_wavetable[m_tableIndex]);
        m_rightChannel.push_back(m_wavetable[m_tableIndex]);

        m_tableIndex = (m_tableIndex + 1) % m_wavetable.size();
    }

    static std::int32_t updateTicker = 0;
    updateTicker = (updateTicker + 1) % 8;

    if (updateTicker == 0)
    {
        auto bufferSize = (m_leftChannel.size()) * ChannelCount; //channel count

        static std::vector<float> mixedBuffer(bufferSize);
        for (auto i = 0u; i < bufferSize; ++i)
        {
            mixedBuffer[i] = m_leftChannel.front() * 0.25f; //L
            m_leftChannel.pop_front();
            
             i++;
            mixedBuffer[i] = 0.f; //R       
  

            i++;
            mixedBuffer[i] = 0.f; //C


            i++;
            mixedBuffer[i] = 0.f; //LFE


            i++;
            mixedBuffer[i] = 0.f; //L2

            i++;
            mixedBuffer[i] = m_rightChannel.front() * 0.25f; //R2
            m_rightChannel.pop_front();
        }

        //send buffer to output
        SDL_QueueAudio(audioDevice, mixedBuffer.data(), mixedBuffer.size() * sizeof(float));
    }

    return true;
}

void MenuState::render()
{

}

//private
void MenuState::addSystems()
{


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
        SDL_PauseAudioDevice(audioDevice, SDL_FALSE);
    }


    m_wavetable = cro::Util::Wavetable::sine(880.f, 1.f, 48000.f);
}

void MenuState::createScene()
{
 
}
