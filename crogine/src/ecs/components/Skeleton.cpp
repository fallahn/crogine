/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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
#include <crogine/ecs/components/Transform.hpp>

using namespace cro;

void SkeletalAnim::resetInterp(const Skeleton& skel)
{
    //make sure the interp output is correct so we can blend with it
    auto startIndex = currentFrame * skel.m_frameSize;
    for (auto i = 0u; i < skel.m_frameSize; ++i)
    {
        interpolationOutput[i] = skel.m_frames[startIndex + i];
    }
}


Skeleton::Skeleton()
    : //m_playbackRate        (1.f),
    m_currentAnimation      (0),
    m_nextAnimation         (-1),
    m_state                 (Stopped),
    m_blendTime             (1.f),
    m_currentBlendTime      (0.f),
    m_useInterpolation      (true),
    m_interpolationDistance (2500.f),
    m_frameSize             (0),
    m_frameCount            (0)
{

}

void Skeleton::play(std::size_t idx, float rate, float blendingTime)
{
    CRO_ASSERT(idx < m_animations.size(), "Index out of range");
    CRO_ASSERT(rate >= 0, "");
    CRO_ASSERT(blendingTime >= 0, "");

    if (idx >= m_animations.size())
    {
        return;
    }

    m_animations[idx].playbackRate = rate;
    m_animations[idx].currentFrame = m_animations[idx].startFrame;

    if (idx != m_currentAnimation)
    {
        if (blendingTime > 0)
        {
            //blend if we're already playing
            m_nextAnimation = static_cast<std::int32_t>(idx);
            m_blendTime = blendingTime;
            m_currentBlendTime = 0.f;

            if (!m_animations[idx].looped)
            {
                //clamp the blending time so it's no longer than the incoming animation
                auto duration = (static_cast<float>(m_animations[idx].frameCount) / m_animations[idx].frameRate) * m_animations[idx].playbackRate;
                m_blendTime *= std::min(1.f, duration / blendingTime);
            }
        }
        else
        {
            //go straight to next anim
            m_animations[m_currentAnimation].playbackRate = 0.f;
            m_currentAnimation = static_cast<std::int32_t>(idx);
        }
        //reset initial interp frame
        m_animations[idx].resetInterp(*this);
    }

    m_state = Playing;
}

//void Skeleton::setPlaybackRate(float rate)
//{
//    CRO_ASSERT(rate > 0, "");
//    if (m_state == Playing)
//    {
//        m_animations[m_currentAnimation].playbackRate = rate;
//
//        if (m_nextAnimation != m_currentAnimation)
//        {
//            m_animations[m_nextAnimation].playbackRate = rate;
//        }
//    }
//}

void Skeleton::prevFrame()
{
    CRO_ASSERT(!m_animations.empty(), "No animations loaded");

    auto& anim = m_animations[m_currentAnimation];
    auto frame = anim.currentFrame - anim.startFrame;
    frame = (frame + (anim.frameCount - 1)) % anim.frameCount;
    anim.currentFrame = frame + anim.startFrame;
    anim.currentFrameTime = 0.f;
    buildKeyframe(anim.currentFrame);
}

void Skeleton::nextFrame()
{
    CRO_ASSERT(!m_animations.empty(), "No animations loaded");
    auto& anim = m_animations[m_currentAnimation];
    auto frame = anim.currentFrame - anim.startFrame;
    frame = (frame + 1) % anim.frameCount;
    anim.currentFrame = frame + anim.startFrame;
    anim.currentFrameTime = 0.f;
    buildKeyframe(anim.currentFrame);
}

void Skeleton::gotoFrame(std::uint32_t frame)
{
    CRO_ASSERT(!m_animations.empty(), "No animations loaded");
    auto& anim = m_animations[m_currentAnimation];
    if (frame < anim.frameCount)
    {
        anim.currentFrame = frame + anim.startFrame;
        anim.currentFrameTime = 0.f;
        buildKeyframe(anim.currentFrame);
    }
}

void Skeleton::stop()
{
    CRO_ASSERT(!m_animations.empty(), "");
    m_animations[m_currentAnimation].playbackRate = 0.f;
    m_state = Stopped;
}

Skeleton::State Skeleton::getState() const
{
    CRO_ASSERT(!m_animations.empty(), "");
    return m_state;
}

std::int32_t Skeleton::getAnimationIndex(const std::string& name) const
{
    if (auto result = std::find_if(m_animations.begin(), m_animations.end(), 
        [&name](const SkeletalAnim& a) {return a.name == name;}); result != m_animations.end())
    {
        return static_cast<std::int32_t>(std::distance(m_animations.begin(), result));
    }

    return -1;
}

void Skeleton::addAnimation(const SkeletalAnim& anim)
{
    CRO_ASSERT(m_frameCount >= (anim.startFrame + anim.frameCount), "animation is out of frame range");
    m_animations.push_back(anim);
    m_animations.back().frameTime = 1.f / m_animations.back().frameRate;
    m_animations.back().interpolationOutput.resize(m_frameSize);
    m_animations.back().resetInterp(*this);
}

bool Skeleton::addAnimation(const Skeleton& source, std::size_t idx)
{
    if (m_frameSize != 0 && source.m_frameSize != m_frameSize)
    {
        LogE << "Cannot add animation - bone counts are different" << std::endl;
        return false;
    }

    const auto& srcAnims = source.getAnimations();
    if (srcAnims.empty())
    {
        LogE << "Cannot add animation - source animations are empty" << std::endl;
        return false;
    }

    if (idx >= srcAnims.size())
    {
        LogE << "Cannot add animation - source index is out of range" << std::endl;
        return false;
    }

    stop();
    m_currentAnimation = 0;

    const auto& srcAnim = srcAnims[idx];
    if (auto exists = getAnimationIndex(srcAnim.name); exists != -1)
    {
        removeAnimation(exists);
        //LogI << "Replaced animation " << srcAnim.name << std::endl;
    }

    if (m_frameSize == 0)
    {
        m_frameSize = source.m_frameSize;
        m_currentFrame.resize(m_frameSize);
    }

    auto dstAnim = srcAnim;
    dstAnim.startFrame = m_frameCount;

    const auto rangeStart = (srcAnim.startFrame * m_frameSize);
    const auto rangeEnd = rangeStart + (srcAnim.frameCount * m_frameSize);

    m_frames.insert(m_frames.end(), source.m_frames.begin() + rangeStart, source.m_frames.begin() + rangeEnd);
    m_notifications.insert(m_notifications.end(),
        source.m_notifications.begin() + srcAnim.startFrame, source.m_notifications.begin() + srcAnim.startFrame + srcAnim.frameCount);

    m_frameCount += srcAnim.frameCount;
    addAnimation(dstAnim);

    return true;
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

bool Skeleton::removeAnimation(std::size_t idx)
{
    if (idx >= m_animations.size()
        || m_animations.empty())
    {
        return false;
    }

    stop();
    m_currentAnimation = 0;

    auto rangeStart = m_animations[idx].startFrame * m_frameSize;
    auto rangeEnd = rangeStart + (m_animations[idx].frameCount * m_frameSize);

    m_frames.erase(m_frames.begin() + rangeStart, m_frames.begin() + rangeEnd);
    m_notifications.erase(m_notifications.begin() + m_animations[idx].startFrame, m_notifications.begin() + m_animations[idx].startFrame + m_animations[idx].frameCount);

    auto frameCount = m_animations[idx].frameCount;
    m_animations.erase(m_animations.begin() + idx);

    if (idx < m_animations.size())
    {
        for (auto i = idx; i < m_animations.size(); ++i)
        {
            m_animations[i].startFrame -= frameCount;
        }
    }
    m_frameCount -= frameCount;

    return true;
}

std::size_t Skeleton::getCurrentFrame() const
{
    CRO_ASSERT(!m_animations.empty(), "");
    return m_animations[m_currentAnimation].currentFrame;
}

float Skeleton::getCurrentFrameTime() const
{
    CRO_ASSERT(!m_animations.empty(), "");
    CRO_ASSERT(m_animations[m_currentAnimation].frameTime > 0.f, "");
    return m_animations[m_currentAnimation].currentFrameTime / m_animations[m_currentAnimation].frameTime;
}

float Skeleton::getAnimationProgress() const
{
    return (static_cast<float>(m_animations[m_currentAnimation].currentFrame - m_animations[m_currentAnimation].startFrame) + getCurrentFrameTime()) / m_animations[m_currentAnimation].frameCount;
}

void Skeleton::addNotification(std::size_t frameID, Notification n)
{
    CRO_ASSERT(frameID < m_frameCount, "Out of range");
    CRO_ASSERT(n.jointID < m_frameSize, "Out of range");
    m_notifications[frameID].push_back(n);
}

std::int32_t Skeleton::addAttachment(const Attachment& ap)
{
    m_attachments.push_back(ap);
    return static_cast<std::int32_t>(m_attachments.size() - 1);
}

std::int32_t Skeleton::getAttachmentIndex(const std::string& name) const
{
    if (auto result = std::find_if(m_attachments.begin(), m_attachments.end(),
        [&name](const Attachment& a)
        {
            return name == a.getName();
        }); result != m_attachments.end())
    {
        return static_cast<std::int32_t>(std::distance(m_attachments.begin(), result));
    }

    return -1;
}

glm::mat4 Skeleton::getAttachmentTransform(std::int32_t id) const
{
    CRO_ASSERT(id > -1 && id < m_attachments.size(), "");

    const auto& ap = m_attachments[id];
    return  m_currentFrame[ap.getParent()] * m_bindPose[ap.getParent()] * ap.getLocalTransform();
}

void Skeleton::setInverseBindPose(const std::vector<glm::mat4>& invBindPose)
{
    m_invBindPose = invBindPose; 
    m_bindPose.resize(invBindPose.size());

    for (auto i = 0u; i < invBindPose.size(); ++i)
    {
        m_bindPose[i] = glm::inverse(invBindPose[i]);
    }
}

void Skeleton::setRootTransform(const glm::mat4& transform)
{
    auto undoTx = glm::inverse(m_rootTransform);
    m_rootTransform = transform;

    for(auto i = 0u; i < m_frameCount; ++i)
    {
        //make sure to rebuild our bounding volumes
        buildKeyframe(i);

        if (i < m_keyFrameBounds.size())
        {
            auto bounds = undoTx * m_keyFrameBounds[i];
            m_keyFrameBounds[i] = m_rootTransform * bounds;
        }
    }
}

//private
void Skeleton::buildKeyframe(std::size_t frame)
{
    auto offset = m_frameSize * frame;
    for (auto i = 0u; i < m_frameSize; ++i)
    {
       m_currentFrame[i] = m_rootTransform * m_frames[offset + i].worldMatrix * m_invBindPose[i];
    }
}

//----attachment struct-----//
void Attachment::setParent(std::int32_t parent)
{
    m_parent = parent;
}

void Attachment::setModel(cro::Entity model)
{
    //reset any existing transform
    if (m_model != model &&
        m_model.isValid() &&
        m_model.hasComponent<cro::Transform>())
    {
        m_model.getComponent<cro::Transform>().m_attachmentTransform = glm::mat4(1.f);
    }

    m_model = model;
}

void Attachment::setPosition(glm::vec3 position)
{
    m_position = position;
    //updateLocalTransform();
    m_dirtyTx = true;
}

void Attachment::setRotation(glm::quat rotation)
{
    m_rotation = rotation;
    //updateLocalTransform();
    m_dirtyTx = true;
}

void Attachment::setScale(glm::vec3 scale)
{
    m_scale = scale;
    //updateLocalTransform();
    m_dirtyTx = true;
}

void Attachment::setName(const std::string& name)
{
    m_name = name;
    if (name.size() > MaxNameLength)
    {
        LogW << name << ": max character length is " << MaxNameLength << ", this name will be truncated when the model is saved." << std::endl;
    }
}

const glm::mat4& Attachment::getLocalTransform() const
{
    if (m_dirtyTx)
    {
        updateLocalTransform();
    }
    return m_transform;
}

void Attachment::updateLocalTransform() const
{
    m_transform = glm::translate(glm::mat4(1.f), m_position);
    m_transform *= glm::toMat4(m_rotation);
    m_transform = glm::scale(m_transform, m_scale);

    m_dirtyTx = false;
}