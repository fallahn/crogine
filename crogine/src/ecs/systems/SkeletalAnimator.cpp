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

#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/detail/glm/gtx/matrix_decompose.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>

using namespace cro;

SkeletalAnimator::SkeletalAnimator(MessageBus& mb)
    : System(mb, typeid(SkeletalAnimator))
{
    requireComponent<Model>();
    requireComponent<Skeleton>();
}

//public
void SkeletalAnimator::process(float dt)
{
    auto& entities = getEntities();
    for (auto& entity : entities)
    {      
        //get skeleton
        auto& skel = entity.getComponent<Skeleton>();

        //update current frame if running
        if (skel.m_nextAnimation < 0)
        {
            //update current animation
            auto& anim = skel.animations[skel.m_currentAnimation];

            auto nextFrame = ((anim.currentFrame - anim.startFrame) + 1) % anim.frameCount;
            nextFrame += anim.startFrame;

            skel.m_currentFrameTime += dt * anim.playbackRate;
                              
            if (entity.getComponent<Model>().isVisible())
            {
                float interpTime = std::min(1.f, skel.m_currentFrameTime / skel.m_frameTime);
                interpolate(anim.currentFrame, nextFrame, interpTime, skel);
            }

            if (skel.m_currentFrameTime > skel.m_frameTime)
            {
                //frame is done, move to next
                if (nextFrame < anim.currentFrame && !anim.looped)
                {
                    anim.playbackRate = 0.f;
                }

                anim.currentFrame = nextFrame;
                skel.m_currentFrameTime = 0.f;                  
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
                interpolate(skel.animations[skel.m_currentAnimation].currentFrame, skel.animations[skel.m_nextAnimation].startFrame, interpTime, skel);
            }

            if (skel.m_currentBlendTime > skel.m_blendTime)
            {
                skel.animations[skel.m_currentAnimation].playbackRate = 0.f;
                skel.m_currentAnimation = skel.m_nextAnimation;
                skel.m_nextAnimation = -1;
                skel.m_frameTime = 1.f / skel.animations[skel.m_currentAnimation].frameRate;
                skel.m_currentFrameTime = 0.f;
                skel.animations[skel.m_currentAnimation].playbackRate = skel.m_playbackRate;
            }
        }
    }
}

//private
void SkeletalAnimator::onEntityAdded(Entity entity)
{
    auto& skeleton = entity.getComponent<Skeleton>();

    if (skeleton.currentFrame.size() < skeleton.frameSize)
    {
        skeleton.currentFrame.resize(skeleton.frameSize);
    }

    entity.getComponent<Model>().setSkeleton(&skeleton.currentFrame[0], skeleton.frameSize);
    skeleton.m_frameTime = 1.f / skeleton.animations[0].frameRate;
}

void SkeletalAnimator::interpolate(std::size_t a, std::size_t b, float time, Skeleton& skeleton)
{
    //TODO interpolate hit boxes for key frames

    //interp tx and rot separately
    //TODO convert to 4x3 to free up some uniform space
    auto mix = [](const Joint& a, const Joint& b, float time) -> glm::mat4
    {
        glm::vec3 trans = glm::mix(a.translation, b.translation, time);
        glm::quat rot = glm::slerp(a.rotation, b.rotation, time);
        glm::vec3 scale = glm::mix(a.scale, b.scale, time);

        glm::mat4 result = glm::translate(glm::mat4(1.f), trans);
        result *= glm::transpose(glm::toMat4(rot));
        return glm::scale(result, scale);
    };

    //NOTE a and b are FRAME INDICES not indices directly into the frame array
    std::size_t startA = a * skeleton.frameSize;
    std::size_t startB = b * skeleton.frameSize;
    for (auto i = 0u; i < skeleton.frameSize; ++i)
    {
        /*
        NOTE for this to work the animation importer must make sure
        all keyframes have their joints fully pre-multiplied by their
        parents.
        */

        /*if (skeleton.frames[i].parent < 0)
        {*/
            //root bone
            skeleton.currentFrame[i] = mix(skeleton.frames[startA + i], skeleton.frames[startB + i], time);
        //}
        //else
        //{
        //    //multiply by our parent
        //    skeleton.currentFrame[i] = skeleton.currentFrame[skeleton.frames[i].parent] * mix(skeleton.frames[startA + i], skeleton.frames[startB + i], time);
        //}
    }
}