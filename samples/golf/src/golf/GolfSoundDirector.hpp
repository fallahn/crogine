/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

namespace cro
{
    class AudioResource;
    class AudioSource;
}

class GolfSoundDirector final : public SoundEffectsDirector
{
public:
    explicit GolfSoundDirector(cro::AudioResource&);

    void handleMessage(const cro::Message&) override;

private:
    struct AudioID final
    {
        enum
        {
            Putt01,
            Putt02,
            Putt03,

            Swing01,
            Swing02,
            Swing03,

            Wedge,

            Hole,
            Water,
            Ground,
            Scrub,

            ScoreAlbatross,
            ScoreBirdie,
            ScoreBogie,
            ScoreDoubleBogie,
            ScoreTripleBogie,
            ScoreEagle,
            ScoreHole,
            ScorePar,

            Applause,

            TerrainBunker01,
            TerrainBunker02,
            TerrainBunker03,
            TerrainBunkre04,
            TerrainBunker05,
            TerrainFairway01,
            TerrainFairway02,
            TerrainGreen01,
            TerrainGreen02,
            TerrainRough01,
            TerrainRough02,
            TerrainScrub01,
            TerrainScrub02,
            TerrainScrub03,
            TerrainScrub04,
            TerrainWater01,
            TerrainWater02,
            TerrainWater03,

            NiceSwing01,
            NiceSwing02,
            NiceSwing03,

            Hook,
            Slice,

            Count
        };
    };
    std::array<const cro::AudioSource*, AudioID::Count> m_audioSources = {};

    void playSound(std::int32_t, glm::vec3, float = 1.f);
    void playSoundDelayed(std::int32_t, glm::vec3, float, float = 1.f);
};