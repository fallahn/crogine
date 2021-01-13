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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>

#include <crogine/detail/glm/mat4x4.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

#include <vector>
#include <string>

namespace cro
{
    /*!
    \brief Describes an animation made up from a series of
    frames within a skeleton.
    This is used as part of a Skeleton component, rather than
    as a stand-alone component itself
    */
    struct CRO_EXPORT_API SkeletalAnim final
    {
        std::string name;
        uint32 startFrame = 0;
        uint32 frameCount = 1;
        uint32 currentFrame = 0;
        float frameRate = 12.f;
        bool looped = false;
        bool playing = false;
    };

    /*!
    \brief Represents a Joint in the skeleton
    An array of these are store in the skeleton representing
    the resting positions for the joints. Useful for debug
    rendering.
    */
    struct CRO_EXPORT_API Joint final
    {
        glm::vec3 translation = glm::vec3(0.f);
        glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
        glm::vec3 scale = glm::vec3(1.f);
        std::int32_t parent = -1;
    };

    /*!
    \brief A hierarchy of bones stored as
    transforms, used to animate 3D models.
    These are updated by the SkeletalAnimation system.
    */
    class CRO_EXPORT_API Skeleton final
    {
    public:
        std::size_t frameSize = 0; //joints in a frame
        std::size_t frameCount = 1;
        std::vector<glm::mat4> frames; //indexed by steps of frameSize
        std::vector<glm::mat4> currentFrame; //current interpolated output

        operator bool() const
        {
            return (!frames.empty() && (frames.size() == frameSize * frameCount));
        }

        //lists the parent IDs if the parents should be included
        //during interpolation
        //TODO fix this disparity between glTF and iqm format meshes.
        std::vector<int32> jointIndices;

        std::vector<SkeletalAnim> animations;
        int32 currentAnimation = 0;
        int32 nextAnimation = -1;

        float blendTime = 1.f;
        float currentBlendTime = 0.f;

        float frameTime = 1.f;
        float currentFrameTime = 0.f;

        /*!
        \brief Joints which make up the bind pose, in local coords
        To convert to world coords walk each joints tree and multiply
        by its parent transform
        */
        std::vector<Joint> bindPose;

        /*!
        \brief Plays the animation at the given index
        \param idx Index of the animation to play
        \param blendTime time in seconds to blend / overlap the new animation
        with any previously playing animation
        */
        void play(std::size_t idx, float blendingTime = 0.1f);

        /*!
        \brief Steps the current animation to the previous frame if it is not playing
        */
        void prevFrame();

        /*!
        \brief Steps the current animation to the next frame if it not playing
        */
        void nextFrame();

        /*!
        \brief Stops any active animations
        */
        void stop();

    private:
    };
}