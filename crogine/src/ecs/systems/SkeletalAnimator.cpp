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
    glm::mat4 mixJoint(const Joint& a, const Joint& b, float time, Joint& output)
    {
        output.translation = glm::mix(a.translation, b.translation, time);
        output.rotation = glm::slerp(a.rotation, b.rotation, time);
        output.scale = glm::mix(a.scale, b.scale, time);

        output.parent = a.parent; //if a and b's parents differ then something is very wrong

        glm::mat4 result = glm::translate(glm::mat4(1.f), output.translation);
        result *= glm::toMat4(output.rotation);
        return glm::scale(result, output.scale);
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


        //update current animation
        updateAnimation(skel.m_animations[skel.m_currentAnimation], skel, entity, ctx);

        //if we have a new animation start updating it and blend its output
        //with the current anim according to blend time
        if (skel.m_nextAnimation > -1)
        {
            //update the next animation to start blending it in
            updateAnimation(skel.m_animations[skel.m_nextAnimation], skel, entity, ctx);

            //blend to next animation
            skel.m_currentBlendTime += dt;
            if (entity.getComponent<Model>().isVisible())
            {
                float interpTime = std::min(1.f, skel.m_currentBlendTime / skel.m_blendTime);
                blendAnimations(skel.m_animations[skel.m_currentAnimation], skel.m_animations[skel.m_nextAnimation], interpTime, skel);
            }

            if (skel.m_currentBlendTime > skel.m_blendTime
                || !skel.m_useInterpolation)
            {
                //update to current animation to next animation
                skel.m_animations[skel.m_currentAnimation].playbackRate = 0.f;
                skel.m_currentAnimation = skel.m_nextAnimation;

                skel.m_nextAnimation = -1;
                skel.m_frameTime = 1.f / skel.m_animations[skel.m_currentAnimation].frameRate;
                skel.m_currentBlendTime = 0.f;

                auto& anim = skel.m_animations[skel.m_currentAnimation];
                anim.playbackRate = skel.m_playbackRate;
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
    for (auto i = 0u; i < skeleton.m_animations.size(); ++i)
    {
        auto& anim = skeleton.m_animations[i];

        //probably don't need this as it should be done when anim was added to skel
        anim.interpolationOutput.resize(skeleton.m_frameSize);

        //copies the initial frame so we have something to mix when
        //blending an animation if it hasn't been interpolated at least once
        auto startIndex = i * skeleton.m_frameSize;
        for (auto j = 0; j < skeleton.m_frameSize; ++j)
        {
            anim.interpolationOutput[j] = skeleton.m_frames[startIndex + j];
        }
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

        //rebuild the anim cache in case we need it for blending
        auto startIndex = anim.currentFrame * skel.m_frameSize;
        for (auto i = 0; i < skel.m_frameSize; ++i)
        {
            anim.interpolationOutput[i] = skel.m_frames[startIndex + i];
        }

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
        && anim.playbackRate != 0)
    {
        if (skel.m_useInterpolation)
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
}

void SkeletalAnimator::interpolateAnimation(SkeletalAnim& source, std::size_t targetFrame, float time, Skeleton& skeleton) const
{
    //TODO interpolate hit boxes for key frames(?)

    //NOTE FRAME INDICES are not indices directly into the frame array
    std::size_t startA = source.currentFrame * skeleton.m_frameSize;
    std::size_t startB = targetFrame * skeleton.m_frameSize;

    //stores interpolated output in source so we can use it to blend.
    for (auto i = 0u; i < skeleton.m_frameSize; ++i)
    {
        glm::mat4 worldMatrix = mixJoint(skeleton.m_frames[startA + i], skeleton.m_frames[startB + i], time, source.interpolationOutput[i]);

        std::int32_t parent = skeleton.m_frames[startA + i].parent;
        while (parent != -1)
        {
            worldMatrix = mixJoint(skeleton.m_frames[startA + parent], skeleton.m_frames[startB + parent], time, source.interpolationOutput[parent]) * worldMatrix;
            parent = skeleton.m_frames[startA + parent].parent;
        }

        //note this gets overwritten if blending animations - might be
        //useful to prevent it happening in those cases?
        skeleton.m_currentFrame[i] = skeleton.m_rootTransform * worldMatrix *  skeleton.m_invBindPose[i];
    }
}

void SkeletalAnimator::blendAnimations(const SkeletalAnim& a, const SkeletalAnim& b, float time, Skeleton& skeleton) const
{
    Joint temp; //we need something to pass as a func param
    for (auto i = 0u; i < skeleton.m_frameSize; ++i)
    {
        auto worldMat = mixJoint(a.interpolationOutput[i], b.interpolationOutput[i], time, temp);

        auto parent = a.interpolationOutput[i].parent;
        while (parent != -1)
        {
            worldMat = mixJoint(a.interpolationOutput[parent], b.interpolationOutput[parent], time, temp) * worldMat;
            parent = a.interpolationOutput[parent].parent;
        }

        skeleton.m_currentFrame[i] = skeleton.m_rootTransform * worldMat * skeleton.m_invBindPose[i];
    }
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