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

#include <crogine/ecs/System.hpp>
#include <crogine/gui/GuiClient.hpp>

struct GolfCart final
{
    std::size_t pathIndex = 0;
    std::size_t pointIndex = 0;

    enum
    {
        Wait, Driving
    }state = Wait;

    float currentTime = 0.f;
    float rotation = 0.f;
};

class GolfCartSystem final : public cro::System, public cro::GuiClient
{
public:
    explicit GolfCartSystem(cro::MessageBus&);

    void process(float) override;

private:

    std::size_t m_nextFreePath;
    float m_maxPitch;
    void onEntityAdded(cro::Entity) override;
};
