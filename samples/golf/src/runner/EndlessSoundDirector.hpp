/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#pragma once

#include "../golf/SoundEffectsDirector.hpp"

namespace cro
{
    class AudioResource;
    class AudioSource;
}

class EndlessSoundDirector final : public SoundEffectsDirector
{
public:
    explicit EndlessSoundDirector(cro::AudioResource&);

    void handleMessage(const cro::Message&) override;

private:
    struct AudioID final
    {
        enum
        {
            Flag, FlagVoice,
            
            Ball,
            Tree, Cart,

            LapLine,
            StreakLost,
            Toot,

            Count
        };
    };
    std::array<const cro::AudioSource*, AudioID::Count> m_audioSources = {};

    cro::Entity playSound(std::int32_t, float volume = 1.f);
    void playSoundDelayed(std::int32_t, float delay, float volume = 1.f, std::uint8_t channel = 1);
};