/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/ecs/Renderable.hpp>
#include <crogine/core/App.hpp>
#include "../detail/GLCheck.hpp"

using namespace cro;

//protected
IntRect Renderable::applyViewport(FloatRect vp, const RenderTarget& rt)
{
    glm::vec2 size(rt.getSize());

    glCheck(glGetIntegerv(GL_VIEWPORT, m_previousViewport.data()));
    IntRect rect(static_cast<std::int32_t>(size.x * vp.left), static_cast<std::int32_t>(size.y * vp.bottom),
                static_cast<std::int32_t>(size.x * vp.width), static_cast<std::int32_t>(size.y * vp.height));
    glViewport(rect.left, rect.bottom, rect.width, rect.height);

    return rect;
}

void Renderable::restorePreviousViewport()
{
    glViewport(m_previousViewport[0], m_previousViewport[1], m_previousViewport[2], m_previousViewport[3]);
}