/*-----------------------------------------------------------------------

Matt Marchant 2017
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

using namespace cro;

SkeletalAnimator::SkeletalAnimator(MessageBus& mb)
    : System(mb, typeid(SkeletalAnimator))
{
    requireComponent<Model>();
    requireComponent<Skeleton>();
}

//public
void SkeletalAnimator::process(Time dt)
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
            if (anim.playing)
            {
                auto nextFrame = (anim.currentFrame + 1) % anim.frameCount;

                skel.currentFrameTime += dt.asSeconds();
                float interpTime = std::min(1.f, skel.currentFrameTime / skel.frameTime);
                interpolate(anim.currentFrame, nextFrame, interpTime, skel);

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
                //DPRINT("Current Frame", std::to_string(anim.currentFrame));
            }
        }
        else
        {
            //blend to next animation
        }
    }
}

//private
void SkeletalAnimator::onEntityAdded(Entity entity)
{
    auto& skeleton = entity.getComponent<Skeleton>();
    entity.getComponent<Model>().setSkeleton(&skeleton.currentFrame[0], skeleton.frameSize);
    skeleton.frameTime = 1.f / skeleton.animations[0].frameRate;
}

void SkeletalAnimator::interpolate(std::size_t a, std::size_t b, float time, Skeleton& skeleton)
{
    //TODO interpolate hit boxes for key frames
    //TODO interp tx and rot seperately and convert to 4x3 to free up some uniform space

    //NOTE a and b are FRAME INDICES not indices directly into the frame array
    std::size_t startA = a * skeleton.frameSize;
    std::size_t startB = b * skeleton.frameSize;
    for (auto i = 0u; i < skeleton.frameSize; ++i)
    {
        if (skeleton.jointIndices[i] < 0)
        {
            //root bone
            skeleton.currentFrame[i] = glm::mix(skeleton.frames[startA + i], skeleton.frames[startB + i], time);
        }
        else
        {
            //multiply by our parent
            skeleton.currentFrame[i] = skeleton.currentFrame[skeleton.jointIndices[i]] * glm::mix(skeleton.frames[startA + i], skeleton.frames[startB + i], time);
        }
    }
}