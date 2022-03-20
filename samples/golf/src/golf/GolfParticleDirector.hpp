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

#include "ParticleDirector.hpp"

#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <array>

namespace cro
{
    class TextureResource;
}

class GolfParticleDirector final : public ParticleDirector
{
public:
    explicit GolfParticleDirector(cro::TextureResource&);

    void handleMessage(const cro::Message&) override;
private:

    struct ParticleID final
    {
        enum
        {
            Grass, Water, Sand,
            Sparkle, HIO,

            Count
        };
    };

    std::array<cro::EmitterSettings, ParticleID::Count> m_emitterSettings = {};

    //TODO recycle these - or do we use so few it doesn't matter?
    cro::Sprite m_ringSprite;
    void spawnRings(glm::vec3);
};
