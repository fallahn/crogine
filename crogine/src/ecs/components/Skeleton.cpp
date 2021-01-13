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
#include <crogine/ecs/components/Skeleton.hpp>

using namespace cro;

void Skeleton::play(std::size_t idx, float blendingTime)
{
    CRO_ASSERT(idx < animations.size(), "Index out of range");
    if (idx != currentAnimation)
    {
        nextAnimation = static_cast<cro::int32>(idx);
        blendTime = blendingTime;
        currentBlendTime = 0.f;
    }
    else
    {
        animations[idx].playing = true;
    }
}

void Skeleton::prevFrame()

{
    CRO_ASSERT(!animations.empty(), "No animations loaded");
    if (currentFrameTime > 0)
    {
        currentFrameTime = 0.f;
    }
    else
    {
        auto& anim = animations[currentAnimation];
        auto frame = anim.currentFrame - anim.startFrame;
        frame = (frame + (anim.frameCount - 1)) % anim.frameCount;
        anim.currentFrame = frame + anim.startFrame;
    }
}

void Skeleton::nextFrame()
{
    CRO_ASSERT(!animations.empty(), "No animations loaded");
    auto& anim = animations[currentAnimation];
    auto frame = anim.currentFrame - anim.startFrame;
    frame = (frame + 1) % anim.frameCount;
    anim.currentFrame = frame + anim.startFrame;
    currentFrameTime = 0.f;
}

void Skeleton::stop()
{
    CRO_ASSERT(!animations.empty(), "");
    animations[currentAnimation].playing = false;
}