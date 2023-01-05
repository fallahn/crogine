/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#pragma once

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/graphics/MeshData.hpp>

namespace cro
{
    /*!
    \brief System used to update any models which have a skeleton component
    */
    class CRO_EXPORT_API SkeletalAnimator : public System
    {
    public:
        explicit SkeletalAnimator(MessageBus&);

        void process(float) override;

    private:
        void onEntityAdded(Entity) override;


        struct AnimationContext final
        {
            glm::vec3 camPos = glm::vec3(0.f);
            glm::vec3 camDir = glm::vec3(0.f);
            glm::mat4 worldTransform = glm::mat4(1.f);
            static constexpr float dt = 1.f / 60.f;
        };

        void updateAnimation(SkeletalAnim& anim, Skeleton& skeleton, Entity entity, const AnimationContext& ctx) const;

        //interpolates frames within a single animation (move this to anim struct?)
        void interpolateAnimation(SkeletalAnim& source, std::size_t targetFrame, float time, Skeleton& skeleton) const;

        void blendAnimations(const SkeletalAnim&, const SkeletalAnim&, Skeleton&) const;

        void updateBoundsFromCurrentFrame(Skeleton& dest, const Mesh::Data&) const;
    };
}