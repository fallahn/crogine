/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "ParticleDirector.hpp"

#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <array>

namespace cro
{
    class TextureResource;
}

struct SharedStateData;
class GolfParticleDirector final : public ParticleDirector
{
public:
    GolfParticleDirector(cro::TextureResource&, const SharedStateData&, bool partyMode = false);

    void handleMessage(const cro::Message&) override;

    //hack to prevent stall the first time particles are spawned
    void init() { resizeEmitters(); }
private:

    const SharedStateData& m_sharedData;
    const bool m_partyMode;

    struct ParticleID final
    {
        enum
        {
            Grass, GrassDark, Water, Sand,
            Sparkle, HIO, Bird,
            Drone, Explode, Blades, PowerShot,
            Puff, Star, Trail, Firework,
            Bubbles, Confetti, Balloons,
            Count
        };
    };

    std::array<cro::EmitterSettings, ParticleID::Count> m_emitterSettings = {};

    //TODO recycle these - or do we use so few it doesn't matter?
    cro::Sprite m_ringSprite;
    void spawnRings(glm::vec3);

    std::vector<glm::vec3> m_fireworkPositions;
    void launchFireworks();
};