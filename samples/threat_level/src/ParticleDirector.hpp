/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#pragma once
#include <crogine/ecs/Director.hpp>
#include <crogine/ecs/Entity.hpp>

#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <vector>
#include <array>

namespace cro
{
    struct ResourceCollection;
}

class ParticleDirector final : public cro::Director 
{
public:
    explicit ParticleDirector(cro::ResourceCollection&);

    void handleMessage(const cro::Message&) override;

    void handleEvent(const cro::Event&) override;

    void process(float) override;

    std::size_t getBufferSize() const { return m_emitters.size(); }

private:

    enum SettingsID
    {
        Explosion,

        Count
    };

    std::array<cro::EmitterSettings, SettingsID::Count> m_settings;

    std::size_t m_nextFreeEmitter;
    std::vector<cro::Entity> m_emitters;

    void resizeEmitters();
    cro::Entity getNextEntity();
};