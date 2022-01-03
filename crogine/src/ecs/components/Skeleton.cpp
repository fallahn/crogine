/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <crogine/detail/Assert.hpp>
#include <crogine/detail/glm/gtx/matrix_interpolation.hpp>
#include <crogine/ecs/components/Skeleton.hpp>

using namespace cro;

Skeleton::Skeleton()
    : m_playbackRate    (1.f),
    m_currentAnimation  (0),
    m_nextAnimation     (-1),
    m_blendTime         (1.f),
    m_currentBlendTime  (0.f),
    m_frameTime         (1.f),
    m_currentFrameTime  (0.f),
    m_frameSize         (0),
    m_frameCount        (0)
{

}

void Skeleton::play(std::size_t idx, float rate, float blendingTime)
{
    CRO_ASSERT(idx < m_animations.size(), "Index out of range");
    CRO_ASSERT(rate > 0, "");
    m_playbackRate = rate;
    if (idx != m_currentAnimation)
    {
        m_nextAnimation = static_cast<std::int32_t>(idx);
        m_blendTime = blendingTime;
        m_currentBlendTime = 0.f;
    }
    else
    {
        m_animations[idx].playbackRate = rate;
        m_animations[idx].currentFrame = m_animations[idx].startFrame;
    }
}

void Skeleton::prevFrame()

{
    CRO_ASSERT(!m_animations.empty(), "No animations loaded");
    if (m_currentFrameTime > 0)
    {
        m_currentFrameTime = 0.f;
    }
    else
    {
        auto& anim = m_animations[m_currentAnimation];
        auto frame = anim.currentFrame - anim.startFrame;
        frame = (frame + (anim.frameCount - 1)) % anim.frameCount;
        anim.currentFrame = frame + anim.startFrame;
    }
}

void Skeleton::nextFrame()
{
    CRO_ASSERT(!m_animations.empty(), "No animations loaded");
    auto& anim = m_animations[m_currentAnimation];
    auto frame = anim.currentFrame - anim.startFrame;
    frame = (frame + 1) % anim.frameCount;
    anim.currentFrame = frame + anim.startFrame;
    m_currentFrameTime = 0.f;
}

void Skeleton::stop()
{
    CRO_ASSERT(!m_animations.empty(), "");
    m_animations[m_currentAnimation].playbackRate = 0.f;
}


Skeleton::State Skeleton::getState() const
{
    CRO_ASSERT(!m_animations.empty(), "");
    return m_animations[m_currentAnimation].playbackRate == 0 ? Stopped : Playing;
}

void Skeleton::addAnimation(const SkeletalAnim& anim)
{
    CRO_ASSERT(m_frameCount >= (anim.startFrame + anim.frameCount), "animation is out of frame range");
    m_animations.push_back(anim);
}

void Skeleton::addFrame(const std::vector<Joint>& frame)
{
    if (m_frameSize == 0)
    {
        m_frameSize = frame.size();
        m_currentFrame.resize(m_frameSize);
    }

    CRO_ASSERT(frame.size() == m_frameSize, "Incorrect frame size");
    m_frames.insert(m_frames.end(), frame.begin(), frame.end());
    m_notifications.emplace_back();
    m_frameCount++;
}

std::size_t Skeleton::getCurrentFrame() const
{
    CRO_ASSERT(!m_animations.empty(), "");
    return m_animations[m_currentAnimation].currentFrame;
}

void Skeleton::addNotification(std::size_t frameID, Notification n)
{
    CRO_ASSERT(frameID < m_frameCount, "Out of range");
    CRO_ASSERT(n.jointID > -1 && n.jointID < m_frameSize, "Out of range");
    m_notifications[frameID].push_back(n);
}

std::int32_t Skeleton::addAttachmentPoint(const AttachmentPoint& ap)
{
    CRO_ASSERT(ap.m_parent < m_frameSize, "");
    m_attachmentPoints.push_back(ap);
    return static_cast<std::int32_t>(m_attachmentPoints.size() - 1);
}

glm::mat4 Skeleton::getAttachmentPoint(std::int32_t id) const
{
    CRO_ASSERT(id > -1 && id < m_attachmentPoints.size(), "");

    const auto& currentAnim = m_animations[m_currentAnimation];
    auto nextFrame = ((currentAnim.currentFrame - currentAnim.startFrame) + 1) % currentAnim.frameCount;
    nextFrame += currentAnim.startFrame;

    auto frameOffsetA = m_frameSize * currentAnim.currentFrame;
    auto frameOffsetB = m_frameSize * nextFrame;

    const auto& ap = m_attachmentPoints[id];

    const auto& jointA = m_frames[frameOffsetA + ap.m_parent];
    const auto& jointB = m_frames[frameOffsetB + ap.m_parent];

    return glm::interpolate(jointA.worldMatrix, jointB.worldMatrix, m_currentFrameTime / m_frameTime) * ap.m_transform;
}