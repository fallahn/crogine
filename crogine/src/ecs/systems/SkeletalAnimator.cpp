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
        if (skel.nextAnimation < 0)
        {
            //update current animation
            auto& anim = skel.animations[skel.currentAnimation];

            auto nextFrame = ((anim.currentFrame - anim.startFrame) + 1) % anim.frameCount;
            nextFrame += anim.startFrame;

            if (anim.playing)
            {
                skel.currentFrameTime += dt;
            }
                              
            if (entity.getComponent<Model>().isVisible())
            {
                float interpTime = std::min(1.f, skel.currentFrameTime / skel.frameTime);
                interpolate(anim.currentFrame, nextFrame, interpTime, skel);
            }

            if (skel.currentFrameTime > skel.frameTime)
            {
                //frame is done, move to next
                if (nextFrame < anim.currentFrame && !anim.looped)
                {
                    anim.playing = false;
                }

                anim.currentFrame = nextFrame;
                skel.currentFrameTime = 0.f;                  
            }
        }
        else
        {
            //TODO blend to next animation
            //this is a bit of a kludge which blends from the current frame to the
            //first frame of the next anim. Really we should interpolate the current
            //position of both animations, and then blend the results according to
            //the current blend time.
            skel.currentBlendTime += dt;
            if (entity.getComponent<Model>().isVisible())
            {
                float interpTime = std::min(1.f, skel.currentBlendTime / skel.blendTime);
                interpolate(skel.animations[skel.currentAnimation].currentFrame, skel.animations[skel.nextAnimation].startFrame, interpTime, skel);
            }

            if (skel.currentBlendTime > skel.blendTime)
            {
                skel.animations[skel.currentAnimation].playing = false;
                skel.currentAnimation = skel.nextAnimation;
                skel.nextAnimation = -1;
                skel.frameTime = 1.f / skel.animations[skel.currentAnimation].frameRate;
                skel.currentFrameTime = 0.f;
                skel.animations[skel.currentAnimation].playing = true;
            }
        }
    }
}

//private
void SkeletalAnimator::onEntityAdded(Entity entity)
{
    auto& skeleton = entity.getComponent<Skeleton>();

    if (skeleton.currentFrame.size() <  skeleton.frameSize)
    {
        skeleton.currentFrame.resize(skeleton.frameSize);
    }

    entity.getComponent<Model>().setSkeleton(&skeleton.currentFrame[0], skeleton.frameSize);
    skeleton.frameTime = 1.f / skeleton.animations[0].frameRate;
}

void SkeletalAnimator::interpolate(std::size_t a, std::size_t b, float time, Skeleton& skeleton)
{
    //TODO interpolate hit boxes for key frames

    //interp tx and rot separately
    //TODO convert to 4x3 to free up some uniform space
    //TODO store frames as components and combine only for output
    auto mix = [](const glm::mat4& a, const glm::mat4& b, float time) -> glm::mat4
    {
        glm::vec3 scaleA(1.f);
        glm::vec3 scaleB(1.f);
        glm::quat rotA(1.f, 0.f, 0.f, 0.f);
        glm::quat rotB(1.f, 0.f, 0.f, 0.f);
        glm::vec3 transA(0.f);
        glm::vec3 transB(0.f);
        glm::vec3 skewA(0.f);
        glm::vec3 skewB(0.f);
        glm::vec4 perspA(0.f);
        glm::vec4 perspB(0.f);
        glm::decompose(a, scaleA, rotA, transA, skewA, perspA);
        glm::decompose(b, scaleB, rotB, transB, skewB, perspB);

        glm::vec3 scale = glm::mix(scaleA, scaleB, time);
        glm::quat rot = glm::slerp(glm::conjugate(rotA), glm::conjugate(rotB), time);
        glm::vec3 trans = glm::mix(transA, transB, time);

        glm::mat4 result = glm::translate(glm::mat4(1.f), trans);
        result *= glm::transpose(glm::toMat4(rot));
        return glm::scale(result, scale);

        //return glm::mix(a, b, time);
    };

    //NOTE a and b are FRAME INDICES not indices directly into the frame array
    std::size_t startA = a * skeleton.frameSize;
    std::size_t startB = b * skeleton.frameSize;
    for (auto i = 0u; i < skeleton.frameSize; ++i)
    {
        if (skeleton.jointIndices[i] < 0)
        {
            //root bone
            skeleton.currentFrame[i] = mix(skeleton.frames[startA + i], skeleton.frames[startB + i], time);
        }
        else
        {
            //multiply by our parent
            skeleton.currentFrame[i] = skeleton.currentFrame[skeleton.jointIndices[i]] * mix(skeleton.frames[startA + i], skeleton.frames[startB + i], time);
        }
    }
}