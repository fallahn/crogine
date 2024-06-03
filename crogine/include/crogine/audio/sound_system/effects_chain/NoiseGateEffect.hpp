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

#include <crogine/audio/sound_system/effects_chain/BaseEffect.hpp>

namespace cro
{
    /*!
    \brief The noise gate is designed to filter out any audio which
    is below a given threshold.
    */
    class CRO_EXPORT_API NoiseGateEffect final : public BaseEffect
    {
    public:
        NoiseGateEffect();

        /*!
        \brief Implementation override - provides the audio signal to process
        */
        void process(std::vector<float>&) override;

        /*!
        \brief Returns the current output gain of the gate
        */
        float getOutputGain() const { return m_gain; }

        /*!
        \brief Sets the threshold at which the gate starts to open
        \param threshold A value clamped between 0-1
        */
        void setThreshold(float);

        /*!
        \brief Returns the current threshold setting
        */
        float getThreshold() const { return m_threshold; }

        /*!
        \brief Sets the decay time, in milliseconds, of the gate closing.
        This is internally calculated by the number of frames of audio, so
        may not be 100% precise
        \param decay The decay time in milliseconds
        */
        void setDecayTime(std::uint16_t decay);

        /*!
        \brief Returns the current decay time in millisseconds
        */
        std::uint16_t getDecayTime() const { return m_decay; }

        /*!
        \brief Sets the attack time, in milliseconds, which is the time
        it takes for the gate to open once the signal has met the threshold value
        \param attach The attack time in milliseconds
        */
        void setAttackTime(std::uint16_t attack);

        /*!
        \brief Returns the current attack time, in milliseconds
        */
        std::uint16_t getAttackTime() const { return m_attack; }

        void reset() override;

    private:

        float m_gain;

        //this goes up when samples are over threshold, and down when below
        //meeting a certain value then triggers the gate to open or close
        std::int32_t m_thresholdCount;
        //when the count is above or below this the gate is triggered
        //directly proprtional to sample rate for ~10ms
        std::int32_t m_thresholdTime; 

        float m_threshold;
        std::uint16_t m_decay;
        std::uint16_t m_decayFrames;

        std::uint16_t m_decayCount; //counts the number of frames left to decay
        float m_decayStep; //how much to decrease gain by

        std::uint16_t m_attack;
        std::uint16_t m_attackFrames;

        std::uint16_t m_attackCount; //counts the number of frames left to attack
        float m_attackStep; //how much to increase gain by

        struct State final
        {
            enum
            {
                Opening, Open, Closing, Closed
            };
        };
        std::int32_t m_state;

        //called automatically at approx 60Hz in audio signal time
        void processEffect() override;
    };
}
