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

#include <vector>

namespace cro
{
    class AudioSource;
    class AudioResource;
}

//dear future me: you managed to duplicate this exactly for sports ball, 
//so instead of doing that *again* just use another instance of this...
class ScrubSoundDirector final : public SoundEffectsDirector
{
public:
    ScrubSoundDirector();

    void handleMessage(const cro::Message&) override {}

    void loadSounds(const std::vector<std::string>&, cro::AudioResource&);

    cro::Entity playSound(std::int32_t, std::uint8_t, float = 1.f);

private:
    std::vector<const cro::AudioSource*> m_audioSources;
};