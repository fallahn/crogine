/*-----------------------------------------------------------------------

Matt Marchant 2024
http://trederia.blogspot.com

crogine - Zlib license.

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

#pragma once

#include <crogine/Config.hpp>

#include <vector>

namespace cro
{
    /*!
    \brief Effects chain objects which inherit BaseEffect are used with
    the SoundRecorder to modify the incoming recorded signal before it
    is returned with SoundRecorder::getPCMData() or 
    SoundRecorde::getEncodedPacket();
    */

    class CRO_EXPORT_API BaseEffect
    {
    public:
        explicit BaseEffect();
        virtual ~BaseEffect() {}

        /*!
        \brief Automatically called for all registered effects, the current
        captured buffer is passed as a vector of floats. If this effect is
        stereo then the float values are interleaved L/R.
        */
        virtual void process(std::vector<float>&) = 0;

        /*!
        \brief Called by the sound recorder when the recording device
        is opened or closed. Optionall implement this if an effect needs resetting
        */
        virtual void reset() {}

    protected:
        //by optionally calling tick() during process with the
        //size of the incoming buffer, we can regularly call this
        //with a fixed time-step to apply parameters such as filter
        //changes within a custom effect.
        virtual void processEffect() {};
        
        void tick(std::size_t);

        /*!
        \brief Sets the audio details of the expected buffer.
        This is automatically called by SoundRecorder - don't attempt to set this yourself
        */
        void setAudioParameters(std::int32_t sampleRate, std::int32_t channels);
        

        std::int32_t getChannelCount() const { return m_channels; }
        std::int32_t getSampleRate() const { return m_sampleRate; }

        friend class SoundRecorder;

    private:

        std::int32_t m_sampleRate;
        std::int32_t m_channels;

        //by passing in the size of the incoming buffer
        //we can calculate based on the channels and sample
        //rate how much relative time of the waveform has
        //passed and enter it into an accumulator to create
        //a fixed step update to process custom effect parameters
        std::int32_t m_accumulator;
        std::int32_t m_accumulatorStep;
    };
}
