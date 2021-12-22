/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include <crogine/core/App.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/graphics/RenderTarget.hpp>
#include "../detail/GLCheck.hpp"

using namespace cro;

std::size_t RenderTarget::m_bufferIndex = 0;
std::array<const RenderTarget*, RenderTarget::MaxActiveTargets> RenderTarget::m_bufferStack = { nullptr };

struct cro::NullTarget final
{
    NullTarget()
    {
        RenderTarget::m_bufferStack[0] = &cro::App::getWindow();
    }
};

const RenderTarget* RenderTarget::getActiveTarget()
{
    return m_bufferStack[m_bufferIndex];
}

void RenderTarget::setActive(bool active)
{
    if (active)
    {
        static NullTarget nt; //places the window as the default target at the bottom of the stack

        CRO_ASSERT(getFrameBufferID() != RenderTarget::m_bufferStack[RenderTarget::m_bufferIndex]->getFrameBufferID(), "Target currently active");

        RenderTarget::m_bufferStack[++RenderTarget::m_bufferIndex] = this;
        CRO_ASSERT(RenderTarget::m_bufferIndex < RenderTarget::m_bufferStack.size(), "Max active buffers reached");

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, getFrameBufferID()));
    }
    else
    {
        CRO_ASSERT(getFrameBufferID() == RenderTarget::m_bufferStack[RenderTarget::m_bufferIndex]->getFrameBufferID(), "Attempting to deactivate non-active render target");

        RenderTarget::m_bufferIndex--;
        CRO_ASSERT(RenderTarget::m_bufferIndex < RenderTarget::m_bufferStack.size(), "Uneven calls to setActive detected");

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, RenderTarget::m_bufferStack[RenderTarget::m_bufferIndex]->getFrameBufferID()));
    }
}