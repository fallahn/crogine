/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef TEST_COLOUR_SYS_HPP_
#define TEST_COLOUR_SYS_HPP_

#include <crogine/ecs/System.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/ecs/Entity.hpp>

#include <glm/vec4.hpp>

class ColourSystem final : public cro::System
{
public:
    ColourSystem();

    void process(cro::Time) override;

private:

    void onEntityAdded(cro::Entity) override;
};

struct ColourChanger final
{
    glm::vec4 colour;
    bool up = true;
    glm::vec4::length_type currentChannel = 0;
};

#endif //TEST_COLOUR_SYS_HPP_
