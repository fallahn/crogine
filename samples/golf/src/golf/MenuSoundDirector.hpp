/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include "SoundEffectsDirector.hpp"

#include <crogine/core/Clock.hpp>

namespace cro
{
    class AudioResource;
    class AudioSource;
}

class MenuSoundDirector final : public SoundEffectsDirector
{
public:
    explicit MenuSoundDirector(cro::AudioResource&, const std::size_t&);

    void handleMessage(const cro::Message&) override;

    cro::Entity playSound(std::int32_t, float = 1.f);

    struct AudioID final
    {
        enum
        {
            Hole,
            Ground,
            Snapshot,
            LobbyJoin,
            LobbyExit,
            Title,
            Woof,
            Fw01,
            Fw02,
            Fw03,

            Bounce01,
            Bounce02,
            Bounce03,
            Flip01,
            Flip02,
            Flip03,
            Flip04,
            Land01,
            Land02,
            Land03,
            BucketSpawn,

            Count
        };
    };

private:
    const std::size_t& m_currentMenu;
    std::array<const cro::AudioSource*, AudioID::Count> m_audioSources = {};
    std::array<cro::Clock, AudioID::Count> m_audioTimers = {};
};