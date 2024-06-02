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

#include <array>

namespace cro
{
    /*!
    \brief Simple effect which controls the gain of the incoming
    recorded audio, and demonstrates a possible implementation
    of the BaseEffect API
    */
    class CRO_EXPORT_API VolumeEffect final : public BaseEffect
    {
    public:
        VolumeEffect();

        /*!
        \brief Sets the normalised gain of the audio.
        Values are clamped between 0 and 1.5
        \param gain New value to which to set the gain.
        Default value is 1
        */
        void setGain(float gain);

        /*!
        \brief Returns the current gain value
        */
        float getGain() const { return m_gain; }

        /*!
        \brief Implementation override
        */
        void process(std::vector<float>&) override;

        /*!
        \brief Returns the peak value of the left channel or main channel if the recorder is mono.
        */
        float getPeakLeft() const { return m_peakLeft; }

        /*!
        \brief Returns the peak value of the right channel or zero if the recorder is mono
        */
        float getPeakRight() const { return m_peakRight; }

        /*!
        \brief Returns the VU value of the left channel or main channel if the recorder is mono.
        */
        float getVULeft() const { return m_VU[0]; }

        /*!
        \brief Returns the VU value of the right channel or zero if the recorder is mono
        */
        float getVURight() const { return m_VU[1]; }

        /*!
        \brief Returns the VU left channel in decibels, or the main channel if the recorder is mono
        */
        float getVUDecibelsLeft() const;

        /*!
        \brief Returns the VU right channel in decibels, or zero if the recorder is mono
        */
        float getVUDecibelsRight() const;

        void reset() override;

    private:
        float m_gain;

        float m_peakLeft;
        float m_peakRight;

        std::array<float, 2u> m_VU = {};

        std::int32_t m_callbackCount;
        void processEffect() override;
    };
}
