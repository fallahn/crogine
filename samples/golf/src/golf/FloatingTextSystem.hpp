/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "../Colordome-32.hpp"

#include <crogine/ecs/System.hpp>

struct FloatingText final
{
    float currTime = 0.f;
    glm::vec3 basePos = glm::vec3(0.f);
    cro::Colour colour = CD32::Colours[CD32::BeigeLight];
    cro::Colour shadowColour = CD32::Colours[CD32::BlueDarkest];

    static constexpr float MaxMove = 40.f;
    static constexpr float MaxTime = 3.f;
};

class FloatingTextSystem final : public cro::System 
{
public:
    explicit FloatingTextSystem(cro::MessageBus&);

    void process(float) override;

private:
    
    void onEntityAdded(cro::Entity) override;
};