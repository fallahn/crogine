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

#include "SnookerDirector.hpp"

SnookerDirector::SnookerDirector()
    : m_currentPlayer(0)
{

}

//public
void SnookerDirector::handleMessage(const cro::Message&)
{

}

const std::vector<BallInfo>& SnookerDirector::getBallLayout() const
{
    return m_ballLayout;
}

glm::vec3 SnookerDirector::getCueballPosition() const
{
    return { 0.f, BallHeight, 0.87f };
}

std::uint32_t SnookerDirector::getTargetID(glm::vec3) const
{
    return 0;
}

//private