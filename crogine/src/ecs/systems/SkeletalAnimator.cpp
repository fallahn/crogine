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
        glm::vec3 trans = mix(a.translation, b.translation, time);
        glm::quat rot = glm::slerp(a.rotation, b.rotation, time);
        glm::vec3 scale = mix(a.scale, b.scale, time);

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
    auto camPos = getScene()->getActiveCamera().getComponent<cro::Transform>().getWorldPosition();
    auto camDir = getScene()->getActiveCamera().getComponent<cro::Transform>().getForwardVector();

    auto& entities = getEntities();
    for (auto& entity : entities)
    {      
        //get skeleton
        auto& skel = entity.getComponent<Skeleton>();

        //update current frame if running
        if (skel.m_nextAnimation < 0)
        {
            //update current animation
            auto& anim = skel.m_animations[skel.m_currentAnimation];
            skel.m_currentFrameTime += dt * anim.playbackRate;

            auto nextFrame = ((anim.currentFrame - anim.startFrame) + 1) % anim.frameCount;
            nextFrame += anim.startFrame;

            if (skel.m_currentFrameTime > skel.m_frameTime)
            {
                //frame is done, move to next
                skel.m_currentFrameTime -= skel.m_frameTime;
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
                        anim.playbackRate = 0.f;
                    }
                }


                //raise notification events
                for (auto [joint, uid] : skel.m_notifications[anim.currentFrame])
                {
                    glm::vec4 position = skel.m_frames[(anim.currentFrame * skel.m_frameSize) + joint].worldMatrix * glm::vec4(0.f, 0.f, 0.f, 1.f);
                    auto* msg = postMessage<cro::Message::SkeletalAnimEvent>(cro::Message::SkeletalAnimationMessage);
                    msg->position = position;
                    msg->userType = uid;
                    msg->entity = entity;
                }
            }
                   
            //only interpolate if model is visible and close to the active camera
            if (!entity.getComponent<Model>().isHidden()
                && anim.playbackRate != 0)
            {
                //check distance to camera and check if actually in front
                auto direction = entity.getComponent<cro::Transform>().getWorldPosition() - camPos;
                if (glm::dot(direction, camDir) > 0
                    && glm::length2(direction) < 250.f) //arbitrary distance of 50 units
                {
                    float interpTime = skel.m_currentFrameTime / skel.m_frameTime;
                    interpolate(anim.currentFrame, nextFrame, interpTime, skel);
                }
            }
        }
        else
        {
            //TODO blend to next animation
            //this is a bit of a kludge which blends from the current frame to the
            //first frame of the next anim. Really we should interpolate the current
            //position of both animations, and then blend the results according to
            //the current blend time.
            skel.m_currentBlendTime += dt;
            if (entity.getComponent<Model>().isVisible())
            {
                float interpTime = std::min(1.f, skel.m_currentBlendTime / skel.m_blendTime);
                interpolate(skel.m_animations[skel.m_currentAnimation].currentFrame, skel.m_animations[skel.m_nextAnimation].startFrame, interpTime, skel);
            }

            if (skel.m_currentBlendTime > skel.m_blendTime)
            {
                skel.m_animations[skel.m_currentAnimation].playbackRate = 0.f;
                skel.m_currentAnimation = skel.m_nextAnimation;
                skel.m_nextAnimation = -1;
                skel.m_frameTime = 1.f / skel.m_animations[skel.m_currentAnimation].frameRate;
                skel.m_currentFrameTime = 0.f;
                skel.m_animations[skel.m_currentAnimation].playbackRate = skel.m_playbackRate;
                skel.m_animations[skel.m_currentAnimation].currentFrame = skel.m_animations[skel.m_currentAnimation].startFrame;

                skel.buildKeyframe(skel.m_animations[skel.m_currentAnimation].currentFrame);
            }
        }

        //update the position of attachments.
        //TODO only do this if the frame was updated
        //TODO set the model transform in a way which doesn't call decompose internally
        for (auto i = 0u; i < skel.m_attachments.size(); ++i)
        {
            auto& ap = skel.m_attachments[i];
            if (ap.m_model.isValid())
            {
                ap.m_model.getComponent<cro::Transform>().setLocalTransform(skel.getAttachment(i));
            }
        }
    }
}

//private
void SkeletalAnimator::onEntityAdded(Entity entity)
{
    auto& skeleton = entity.getComponent<Skeleton>();

    //TODO reject this if it has no animations (or atleast prevent div0)

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

void SkeletalAnimator::interpolate(std::size_t a, std::size_t b, float time, Skeleton& skeleton)
{
    //TODO interpolate hit boxes for key frames(?)

    //NOTE a and b are FRAME INDICES not indices directly into the frame array
    std::size_t startA = a * skeleton.m_frameSize;
    std::size_t startB = b * skeleton.m_frameSize;
    for (auto i = 0u; i < skeleton.m_frameSize; ++i)
    {
        glm::mat4 worldMatrix = mixJoint(skeleton.m_frames[startA + i], skeleton.m_frames[startB + i], time);

        std::int32_t parent = skeleton.m_frames[startA + i].parent;
        while (parent != -1)
        {
            worldMatrix = mixJoint(skeleton.m_frames[startA + parent], skeleton.m_frames[startB + parent], time) * worldMatrix;
            parent = skeleton.m_frames[startA + parent].parent;
        }

        skeleton.m_currentFrame[i] = skeleton.m_rootTransform * worldMatrix *  skeleton.m_invBindPose[i];
    }
}

void SkeletalAnimator::updateBoundsFromCurrentFrame(Skeleton& dest, const Mesh::Data& source)
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