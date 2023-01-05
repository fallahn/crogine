/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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

#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/detail/glm/gtx/quaternion.hpp>

using namespace cro;

namespace
{
    //interp tx and rot separately
    //TODO convert to 4x3 to free up some uniform space
    glm::mat4 mixJoint(const Joint& a, const Joint& b, float time)
    {
        glm::vec3 trans = glm::mix(a.translation, b.translation, time);
        glm::quat rot = glm::slerp(a.rotation, b.rotation, time);
        glm::vec3 scale = glm::mix(a.scale, b.scale, time);

        glm::mat4 result = glm::translate(glm::mat4(1.f), trans);
        result *= glm::toMat4(rot);
        return glm::scale(result, scale);
    }
}

SkeletalAnimator::SkeletalAnimator(MessageBus& mb)
    : System(mb, typeid(SkeletalAnimator))
{
    requireComponent<Model>();
    requireComponent<Skeleton>();
}

//public
void SkeletalAnimator::process(float dt)
{
    const auto camPos = getScene()->getActiveCamera().getComponent<cro::Transform>().getWorldPosition();
    const auto camDir = getScene()->getActiveCamera().getComponent<cro::Transform>().getForwardVector();

    //TODO we might increase perf a bit if we split the entities across
    //a series of joblists each in its own thread, then wait for those lists to complete
    //gotta try it to find out. I have a feeling it'll actually be worse on low numbers
    //also remember raising messages is not thread safe so might have to do all those
    //on the main thread anyway...

    auto& entities = getEntities();
    for (auto& entity : entities)
    {      
        //get skeleton
        auto& skel = entity.getComponent<Skeleton>();
        const AnimationContext ctx = 
        {
            camPos, 
            camDir, 
            entity.getComponent<cro::Transform>().getWorldTransform() 
        };

        //update current frame if running
        if (skel.m_nextAnimation < 0)
        {
            //update current animation
            updateAnimation(skel.m_animations[skel.m_currentAnimation], skel, entity, ctx);
        }
        else
        {
            
            //blend to next animation
            skel.m_currentBlendTime += dt;
            if (entity.getComponent<Model>().isVisible())
            {
                /*auto& anim = skel.m_animations[skel.m_currentAnimation];
                float interpTime = std::min(1.f, skel.m_currentBlendTime / skel.m_blendTime);

                auto startFrame = static_cast<std::uint32_t>(anim.currentFrame + std::round(anim.currentFrameTime / skel.m_frameTime));
                startFrame %= anim.frameCount;

                interpolate(startFrame, skel.m_animations[skel.m_nextAnimation].startFrame, interpTime, skel);*/
            }

            if (skel.m_currentBlendTime > skel.m_blendTime
                || !skel.m_useInterpolation)
            {
                skel.m_animations[skel.m_currentAnimation].playbackRate = 0.f;
                skel.m_currentAnimation = skel.m_nextAnimation;

                skel.m_nextAnimation = -1;
                skel.m_frameTime = 1.f / skel.m_animations[skel.m_currentAnimation].frameRate;
                skel.m_currentBlendTime = 0.f;

                auto& anim = skel.m_animations[skel.m_currentAnimation];
                anim.currentFrameTime = skel.m_currentBlendTime - skel.m_blendTime;
                anim.playbackRate = skel.m_playbackRate;
                anim.currentFrame = anim.startFrame;

                skel.buildKeyframe(anim.currentFrame);
            }
        }

        //update the position of attachments.
        //TODO only do this if the frame was updated (? won't account for entity transform changing though)
        for (auto i = 0u; i < skel.m_attachments.size(); ++i)
        {
            auto& ap = skel.m_attachments[i];
            if (ap.getModel().isValid())
            {
                ap.getModel().getComponent<cro::Transform>().m_attachmentTransform = ctx.worldTransform * skel.getAttachmentTransform(i);
            }
        }
    }
}

//private
void SkeletalAnimator::onEntityAdded(Entity entity)
{
    auto& skeleton = entity.getComponent<Skeleton>();

    //TODO reject this if it has no animations (or at least prevent div0)
    CRO_ASSERT(skeleton.m_animations[0].frameRate > 0, "");
    CRO_ASSERT(!skeleton.m_animations.empty(), "");
    CRO_ASSERT(skeleton.m_frameSize != 0, "");

    //initialise the interpolated output to the correct size.
    for (auto& anim : skeleton.m_animations)
    {
        anim.interpolationOutput.resize(skeleton.m_currentFrame.size());
    }

    entity.getComponent<Model>().setSkeleton(&skeleton.m_currentFrame[0], skeleton.m_frameSize);
    skeleton.m_frameTime = 1.f / skeleton.m_animations[0].frameRate;

    if (skeleton.m_invBindPose.empty())
    {
        skeleton.m_invBindPose.resize(skeleton.m_frameSize);
    }

    //update the bounds for each key frame
    for (auto i = 0u; i < skeleton.m_frameCount; ++i)
    {
        skeleton.buildKeyframe(i);
        updateBoundsFromCurrentFrame(skeleton, entity.getComponent<Model>().getMeshData());
    }
    entity.getComponent<Model>().getMeshData().boundingBox = skeleton.m_keyFrameBounds[0];
    entity.getComponent<Model>().getMeshData().boundingSphere = skeleton.m_keyFrameBounds[0];
}

void SkeletalAnimator::updateAnimation(SkeletalAnim& anim, Skeleton& skel, Entity entity, const AnimationContext& ctx) const
{
    anim.currentFrameTime += ctx.dt * anim.playbackRate;

    auto nextFrame = ((anim.currentFrame - anim.startFrame) + 1) % anim.frameCount;
    nextFrame += anim.startFrame;

    if (anim.currentFrameTime > skel.m_frameTime)
    {
        //frame is done, move to next
        anim.currentFrameTime -= skel.m_frameTime;
        anim.currentFrame = nextFrame;

        nextFrame = ((anim.currentFrame - anim.startFrame) + 1) % anim.frameCount;
        nextFrame += anim.startFrame;

        //apply the current frame
        skel.buildKeyframe(anim.currentFrame);

        auto& meshData = entity.getComponent<Model>().getMeshData();
        meshData.boundingBox = skel.m_keyFrameBounds[anim.currentFrame];
        meshData.boundingSphere = skel.m_keyFrameBounds[anim.currentFrame];

        //stop playback if frame ID has looped
        if (nextFrame < anim.currentFrame)
        {
            if (!anim.looped)
            {
                skel.stop();

                auto* msg = postMessage<Message::SkeletalAnimationEvent>(Message::SkeletalAnimationMessage);
                msg->userType = Message::SkeletalAnimationEvent::Stopped;
                msg->entity = entity;
                msg->animationID = skel.getCurrentAnimation();
            }
        }


        //raise notification events
        for (auto [joint, uid, _] : skel.m_notifications[anim.currentFrame])
        {
            glm::vec4 position =
                (ctx.worldTransform * skel.m_rootTransform *
                    skel.m_frames[(anim.currentFrame * skel.m_frameSize) + joint].worldMatrix)[3];

            auto* msg = postMessage<Message::SkeletalAnimationEvent>(Message::SkeletalAnimationMessage);
            msg->position = position;
            msg->userType = uid;
            msg->entity = entity;
            msg->animationID = skel.getCurrentAnimation();
        }
    }

    //only interpolate if model is visible and close to the active camera
    if (!entity.getComponent<Model>().isHidden()
        && anim.playbackRate != 0
        && skel.m_useInterpolation)
    {
        //check distance to camera and check if actually in front
        auto direction = entity.getComponent<cro::Transform>().getWorldPosition() - ctx.camPos;
        if (glm::dot(direction, ctx.camDir) > 0
            && glm::length2(direction) < skel.m_interpolationDistance)
        {
            float interpTime = anim.currentFrameTime / skel.m_frameTime;
            interpolateAnimation(anim, nextFrame, interpTime, skel);
        }
    }
}

void SkeletalAnimator::interpolateAnimation(SkeletalAnim& source, std::size_t targetFrame, float time, Skeleton& skeleton) const
{
    //TODO interpolate hit boxes for key frames(?)

    //NOTE FRAME INDICES are not indices directly into the frame array
    std::size_t startA = source.currentFrame * skeleton.m_frameSize;
    std::size_t startB = targetFrame * skeleton.m_frameSize;


    for (auto i = 0u; i < skeleton.m_frameSize; ++i)
    {
        glm::mat4 worldMatrix = mixJoint(skeleton.m_frames[startA + i], skeleton.m_frames[startB + i], time);

        std::int32_t parent = skeleton.m_frames[startA + i].parent;
        while (parent != -1)
        {
            worldMatrix = mixJoint(skeleton.m_frames[startA + parent], skeleton.m_frames[startB + parent], time) * worldMatrix;
            parent = skeleton.m_frames[startA + parent].parent;
        }

        source.interpolationOutput[i] = worldMatrix;
        skeleton.m_currentFrame[i] = skeleton.m_rootTransform * worldMatrix *  skeleton.m_invBindPose[i];
    }
}

void SkeletalAnimator::blendAnimations(const SkeletalAnim& a, const SkeletalAnim& b, Skeleton& skeleton) const
{

}

void SkeletalAnimator::updateBoundsFromCurrentFrame(Skeleton& dest, const Mesh::Data& source) const
{
    //store these in case we want to update the bounds
    std::vector<glm::vec3> positions;
    for (auto i = 0u; i < dest.m_frameSize; ++i)
    {
        positions.push_back(glm::vec3(dest.m_currentFrame[i] * glm::inverse(dest.m_invBindPose[i]) * glm::vec4(0.f, 0.f, 0.f, 1.f)));
    }

    if (!positions.empty())
    {
        if (positions.size() > 1)
        {
            //update the bounds - this only covers the skeleton however
            auto [minX, maxX] = std::minmax_element(positions.begin(), positions.end(),
                [](const glm::vec3& l, const glm::vec3& r)
                {
                    return l.x < r.x;
                });

            auto [minY, maxY] = std::minmax_element(positions.begin(), positions.end(),
                [](const glm::vec3& l, const glm::vec3& r)
                {
                    return l.y < r.y;
                });

            auto [minZ, maxZ] = std::minmax_element(positions.begin(), positions.end(),
                [](const glm::vec3& l, const glm::vec3& r)
                {
                    return l.z < r.z;
                });

            cro::Box aabb(glm::vec3(minX->x, minY->y, minZ->z), glm::vec3(maxX->x, maxY->y, maxZ->z) + glm::vec3(0.001f));

            //check the ratios of the boxes - if there are few bones or all
            //bones are aligned on a plane then the box probably isn't covering
            //the model very well
            auto defaultVolume = source.boundingBox.getVolume();
            auto newVolume = aabb.getVolume();
            if (auto ratio = newVolume / defaultVolume; ratio < 0.1f)
            {
                //new volume is considerably smaller
                //best fit is default bounds, although this might be very different
                //if the skeleton is creating a lot of deformation
                aabb = dest.m_rootTransform * source.boundingBox;
            }

            dest.m_keyFrameBounds.push_back(aabb);
        }
        else
        {
            //re-centre existing bounds on position
            auto boundingBox = dest.m_currentFrame[0] * source.boundingBox;
            dest.m_keyFrameBounds.push_back(boundingBox);
        }
    }
    else
    {
        dest.m_keyFrameBounds.push_back(source.boundingBox);
    }
}