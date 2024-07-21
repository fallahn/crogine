/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/util/Matrix.hpp>

#include <crogine/detail/glm/gtx/quaternion.hpp>

//#define PARALLEL_DISABLE
#ifdef PARALLEL_DISABLE
#undef USE_PARALLEL_PROCESSING
#endif

#ifdef USE_PARALLEL_PROCESSING
#include <execution>
#endif

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

        CRO_ASSERT(a.parent == b.parent, "");
        output.parent = a.parent; //if a and b's parents differ then something is very wrong

        glm::mat4 result = glm::translate(glm::mat4(1.f), output.translation);
        result *= glm::toMat4(output.rotation);
        return glm::scale(result, output.scale);
    }

    float playbackRate = 1.f;
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
    dt *= playbackRate;

    const auto camPos = getScene()->getActiveCamera().getComponent<cro::Transform>().getWorldPosition();
    const auto camDir = cro::Util::Matrix::getForwardVector(getScene()->getActiveCamera().getComponent<cro::Transform>().getWorldTransform());


    const auto& entities = getEntities();
    
    //const auto policy = entities.size() > 5 ? std::execution::par : std::execution::seq;

#ifdef USE_PARALLEL_PROCESSING
    std::for_each(std::execution::par, entities.cbegin(), entities.cend(), 
        [&](cro::Entity entity)
#else
    for (auto entity : entities)
#endif
        {
            auto& skel = entity.getComponent<Skeleton>();

            //check the model is roughly in front of the camera and within interp distance
            const auto direction = entity.getComponent<cro::Transform>().getWorldPosition() - camPos;
            bool useInterpolation = (glm::dot(direction, camDir) > 0 //could squeeze a bit more out of this if we take FOV into account...
                && glm::length2(direction) < skel.m_interpolationDistance
                && skel.m_useInterpolation);

            const AnimationContext ctx =
            {
                useInterpolation,
                entity.getComponent<cro::Transform>().getWorldTransform(),
                dt,
                skel.m_nextAnimation < 0
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
                if (!entity.getComponent<Model>().isHidden())
                {
                    //hmm if interpolation is disabled we probably only want to blend once
                    //per frame at the current framerate - although blend times are so short
                    //in most cases it's probably not worth the effort
                    float interpTime = std::min(1.f, skel.m_currentBlendTime / skel.m_blendTime);
                    blendAnimations(skel.m_animations[skel.m_currentAnimation], skel.m_animations[skel.m_nextAnimation], interpTime, skel);
                }

                if (skel.m_currentBlendTime > skel.m_blendTime)
                {
                    //update to current animation to next animation
                    skel.m_animations[skel.m_currentAnimation].playbackRate = 0.f;
                    skel.m_currentAnimation = skel.m_nextAnimation;

                    skel.m_nextAnimation = -1;
                    skel.m_currentBlendTime = 0.f;
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
#ifdef USE_PARALLEL_PROCESSING       
        );
#endif
}

void SkeletalAnimator::debugUI() const
{
    ImGui::SliderFloat("Playback Rate", &playbackRate, 0.1f, 2.f);

    const std::int32_t max = static_cast<std::int32_t>(getEntities().size());
    static std::int32_t start = 0;
    static std::int32_t end = std::min(2, max);
    if (ImGui::InputInt("Start", &start))
    {
        start = std::max(0, std::min(start, end - 1));
    }
    if (ImGui::InputInt("End", &end))
    {
        end = std::max(start, std::min(end, max));
    }
    
    for(auto i = start; i < end; ++i)
    {
        auto entity = getEntities()[i];

        const auto& skel = entity.getComponent<Skeleton>();
        const auto& anim = skel.getAnimations()[skel.getCurrentAnimation()];
        ImGui::Text("Animation %d: %s", skel.getCurrentAnimation(), anim.name.c_str());
        ImGui::ProgressBar(anim.currentFrameTime / anim.frameTime);

        float currentFrameTime = 0.f;
        std::string nextAnimName("None");
        if (skel.m_nextAnimation > -1)
        {
            const auto& nextAnim = skel.getAnimations()[skel.m_nextAnimation];
            currentFrameTime = nextAnim.currentFrameTime;
            nextAnimName = nextAnim.name;
        }

        ImGui::Text("Next Animation %d: %s", skel.m_nextAnimation, nextAnimName.c_str());
        ImGui::ProgressBar(currentFrameTime / anim.frameTime);

        ImGui::Text("Blend Progress");
        ImGui::ProgressBar(skel.m_currentBlendTime / skel.m_blendTime);

        ImGui::Separator();
    }
}

float SkeletalAnimator::getPlaybackRate() const
{
    return playbackRate;
}

//private
void SkeletalAnimator::onEntityAdded(Entity entity)
{
    auto& skeleton = entity.getComponent<Skeleton>();

    //TODO reject this if it has no animations (or at least prevent div0)
    CRO_ASSERT(skeleton.m_animations[0].frameRate > 0, "");
    CRO_ASSERT(!skeleton.m_animations.empty(), "");
    CRO_ASSERT(skeleton.m_frameSize != 0, "");

    entity.getComponent<Model>().setSkeleton(&skeleton.m_currentFrame[0], skeleton.m_frameSize);

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

    if (anim.currentFrameTime > anim.frameTime)
    {
        //frame is done, move to next
        anim.currentFrameTime -= anim.frameTime;
        anim.currentFrame = nextFrame;

        nextFrame = ((anim.currentFrame - anim.startFrame) + 1) % anim.frameCount;
        nextFrame += anim.startFrame;

        //apply the current frame
        skel.buildKeyframe(anim.currentFrame);

        //rebuild the anim cache in case we need it for blending
        anim.resetInterp(skel);

        auto& meshData = entity.getComponent<Model>().getMeshData();
        meshData.boundingBox = skel.m_keyFrameBounds[anim.currentFrame];
        meshData.boundingSphere = skel.m_keyFrameBounds[anim.currentFrame];

        //stop playback if frame ID has looped
        if (nextFrame < anim.currentFrame)
        {
            if (!anim.looped)
            {
                if (skel.m_nextAnimation == -1)
                {
                    skel.stop();

                    auto* msg = postMessage<Message::SkeletalAnimationEvent>(Message::SkeletalAnimationMessage);
                    msg->userType = Message::SkeletalAnimationEvent::Stopped;
                    msg->entity = entity;
                    msg->animationID = skel.getCurrentAnimation();
                }
                else
                {
                    anim.playbackRate = 0.f;
                }
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
        if (ctx.useInterpolation)
        {
            float interpTime = anim.currentFrameTime / anim.frameTime;
            interpolateAnimation(anim, nextFrame, interpTime, skel, ctx.writeOutput);
        }
    }
}

void SkeletalAnimator::interpolateAnimation(SkeletalAnim& source, std::size_t targetFrame, float time, Skeleton& skeleton, bool output) const
{
    //TODO interpolate hit boxes for key frames(?)

    //NOTE FRAME INDICES are not indices directly into the frame array
    std::size_t startA = source.currentFrame * skeleton.m_frameSize;
    std::size_t startB = targetFrame * skeleton.m_frameSize;

    //stores interpolated output in source so we can use it to blend.
    //we mix all the joints first to prevent it happening multiple times
    //when we create the world transforms.

    std::vector<glm::mat4> mixBuffer(skeleton.m_frameSize);
    for (auto i = 0u; i < skeleton.m_frameSize; ++i)
    {
        mixBuffer[i] = mixJoint(skeleton.m_frames[startA + i], skeleton.m_frames[startB + i], time, source.interpolationOutput[i]);
    }

    for (auto i = 0u; i < skeleton.m_frameSize; ++i)
    {
        glm::mat4 worldMatrix = mixBuffer[i];

        std::int32_t parent = skeleton.m_frames[startA + i].parent;
        while (parent != -1)
        {
            worldMatrix = mixBuffer[parent] * worldMatrix;
            parent = skeleton.m_frames[startA + parent].parent;
        }

        //note this gets overwritten if blending animations - might be
        //useful to prevent it happening in those cases?
        if (output)
        {
            skeleton.m_currentFrame[i] = skeleton.m_rootTransform * worldMatrix * skeleton.m_invBindPose[i];
        }
        
    }
}

void SkeletalAnimator::blendAnimations(const SkeletalAnim& a, const SkeletalAnim& b, float time, Skeleton& skeleton) const
{
    Joint temp; //we need something to pass as a func param
    std::vector<glm::mat4> mixBuffer(skeleton.m_frameSize);

    for (auto i = 0u; i < skeleton.m_frameSize; ++i)
    {
        mixBuffer[i] = mixJoint(a.interpolationOutput[i], b.interpolationOutput[i], time, temp);
    }

    for (auto i = 0u; i < skeleton.m_frameSize; ++i)
    {
        auto worldMat = mixBuffer[i];

        auto parent = a.interpolationOutput[i].parent;
        while (parent != -1)
        {
            worldMat = mixBuffer[parent] * worldMat;
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

#ifdef PARALLEL_DISABLE
#ifndef PARALLEL_GLOBAL_DISABLE
#define USE_PARALLEL_PROCESSING
#endif
#endif