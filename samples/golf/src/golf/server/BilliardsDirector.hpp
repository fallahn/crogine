/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/ecs/Director.hpp>

#include <vector>

struct BallInfo final
{
    glm::vec3 position = glm::vec3(0.f);
    std::int8_t id = -1;

    BallInfo() = default;
    BallInfo(glm::vec3 p, std::int32_t i) : position(p), id(i) {}
};

class BilliardsDirector : public cro::Director
{
public:

    virtual ~BilliardsDirector() = default;

    virtual void handleMessage(const cro::Message&) override = 0;

    virtual const std::vector<BallInfo>& getBallLayout() const = 0;

    virtual glm::vec3 getCueballPosition() const = 0;

    virtual std::size_t getCurrentPlayer() const = 0;

    virtual void setCueball(cro::Entity e) { m_cueball = e; }

    virtual std::uint32_t getTargetID() const = 0; //server ID of look-at target

protected:
    cro::Entity getCueball() const { return m_cueball; }

private:
    cro::Entity m_cueball;
};