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

#ifndef TL_ITEMS_DIRECTOR_HPP_
#define TL_ITEMS_DIRECTOR_HPP_

#include <crogine/ecs/Director.hpp>

#include <array>

namespace cro
{
    class MeshResource;
    class MaterialResource;
}

//in charge of spawning new items
class ItemDirector final : public cro::Director
{
public:
    ItemDirector();

private:
    void handleMessage(const cro::Message&) override;
    void handleEvent(const cro::Event&) override;
    void process(float) override;

    float m_releaseTime;
    std::array<cro::int32, 10u>  m_itemList;
    std::size_t m_itemIndex;

    bool m_active;
};

#endif //TL_ITEMS_DIRECTOR_HPP_