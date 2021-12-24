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
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <crogine/graphics/RenderTarget.hpp>
#include "../detail/GLCheck.hpp"

using namespace cro;

std::size_t RenderTarget::m_bufferIndex = 0;
std::array<const RenderTarget*, RenderTarget::MaxActiveTargets> RenderTarget::m_bufferStack = { nullptr };

IntRect RenderTarget::getViewport(FloatRect normalised) const
{
    float width = static_cast<float>(getSize().x);
    float height = static_cast<float>(getSize().y);

    return IntRect(static_cast<int>(0.5f + width * normalised.left),
        static_cast<int>(0.5f + height * normalised.bottom),
        static_cast<int>(0.5f + width * normalised.width),
        static_cast<int>(0.5f + height * normalised.height));
}

IntRect RenderTarget::getViewport() const
{
    return m_viewport;
}

IntRect RenderTarget::getDefaultViewport() const
{
    return { 0, 0, static_cast<std::int32_t>(getSize().x), static_cast<std::int32_t>(getSize().y) };
}

void RenderTarget::setViewport(IntRect viewport)
{
    m_viewport = viewport;
}

const RenderTarget* RenderTarget::getActiveTarget()
{
    return m_bufferStack[m_bufferIndex];
}

FloatRect RenderTarget::getView() const
{
    return m_view;
}

void RenderTarget::setView(FloatRect view)
{
    m_view = view;

    m_projectionMatrix = glm::ortho(view.left, view.left + view.width, view.bottom, view.bottom + view.height, -1.f, 1.f);
}

void RenderTarget::setActive(bool active)
{
    if (active)
    {
        CRO_ASSERT(getFrameBufferID() != RenderTarget::m_bufferStack[RenderTarget::m_bufferIndex]->getFrameBufferID(), "Target currently active");

        RenderTarget::m_bufferStack[++RenderTarget::m_bufferIndex] = this;
        CRO_ASSERT(RenderTarget::m_bufferIndex < RenderTarget::m_bufferStack.size(), "Max active buffers reached");

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, getFrameBufferID()));

        //store existing viewport - and apply ours
        glCheck(glGetIntegerv(GL_VIEWPORT, m_previousViewport.data()));
        glCheck(glViewport(m_viewport.left, m_viewport.bottom, m_viewport.width, m_viewport.height));
    }
    else
    {
        CRO_ASSERT(getFrameBufferID() == RenderTarget::m_bufferStack[RenderTarget::m_bufferIndex]->getFrameBufferID(), "Attempting to deactivate non-active render target");

        RenderTarget::m_bufferIndex--;
        CRO_ASSERT(RenderTarget::m_bufferIndex < RenderTarget::m_bufferStack.size(), "Uneven calls to setActive detected");

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, RenderTarget::m_bufferStack[RenderTarget::m_bufferIndex]->getFrameBufferID()));

        //restore viewport
        glCheck(glViewport(m_previousViewport[0], m_previousViewport[1], m_previousViewport[2], m_previousViewport[3]));
    }
}

const glm::mat4& RenderTarget::getProjectionMatrix() const
{
    return m_projectionMatrix;
}