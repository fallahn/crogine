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
#include <crogine/detail/glm/gtx/quaternion.hpp>

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
        uint32 frameCount = 0;
        uint32 currentFrame = 0;
        float frameRate = 12.f;
        bool looped = false;
        float playbackRate = 0.f;
    };

    /*!
    \brief Represents a Joint in the skeleton
    An array of these is stored in the skeleton representing
    the resting positions for the joints. Useful for debug
    rendering. These are also used to make up the joints
    of a key frame. Each transform is assumed to *already*
    be transformed by its parents - done by the model loader
    when the key frames were created.
    */
    struct CRO_EXPORT_API Joint final
    {
        Joint() = default;
        Joint(glm::vec3 t, glm::quat r, glm::vec3 s)
            : translation(t), rotation(r), scale(s) {}

        glm::vec3 translation = glm::vec3(0.f);
        glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
        glm::vec3 scale = glm::vec3(1.f);
        std::int32_t parent = -1;

        static glm::mat4 combine(const Joint& j)
        {
            glm::mat4 m(1.f);
            m = glm::translate(m, j.translation);
            m *= glm::toMat4(j.rotation);
            m = glm::scale(m, j.scale);
            return m;
        }

        glm::mat4 worldMatrix = glm::mat4(1.f);
    };

    /*!
    \brief A hierarchy of bones stored as
    transforms, used to animate 3D models.
    These are updated by the SkeletalAnimation system.
    */
    class CRO_EXPORT_API Skeleton final
    {
    public:
        /*!
        \brief Default Constructor
        */
        Skeleton();

        /*!
        \brief Plays the animation at the given index
        \param idx Index of the animation to play
        \param blendTime time in seconds to blend / overlap the new animation
        with any previously playing animation
        */
        void play(std::size_t idx, float rate = 1.f, float blendingTime = 0.1f);

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

        /*!
        \brief Returns the index of the current animation
        */
        std::int32_t getCurrentAnimation() const { return m_currentAnimation; }

        /*!
        \brief Returns a reference to the array of animations for this skeleton
        */
        std::vector<SkeletalAnim>& getAnimations() { return m_animations; }
        const std::vector<SkeletalAnim>& getAnimations() const { return m_animations; }

        /*!
        \brief Adds an animation to the skeleton.
        The required frames must have already been added to the skeleton
        */
        void addAnimation(const SkeletalAnim&);

        /*!
        \brief Adds a frame of animation.
        If this is the first frame then this will set the frame size (aka joint count)
        of a single frame. Subsequent frames must match this number of joints.
        */
        void addFrame(const std::vector<Joint>&);

        /*!
        \brief Returns the number of joints in a single frame, or zero if no animations are added
        */
        std::size_t getFrameSize() const { return m_frameSize; }

        /*!
        \brief Return the total number of frames of animation
        */
        std::size_t getFrameCount() const { return m_frameCount; }

        operator bool() const
        {
            return (!m_frames.empty() && (m_frames.size() == m_frameSize * m_frameCount));
        }

        /*!
        \brief Returns a reference to the vector of joints which make up the key frames
        Note that the stride of each frame is equal the the  number of joints in a frame
        aka FrameSize
        */
        const std::vector<Joint>& getFrames() const { return m_frames; }

    private:

        float m_playbackRate;

        int32 m_currentAnimation;
        int32 m_nextAnimation;

        float m_blendTime;
        float m_currentBlendTime;

        float m_frameTime;
        float m_currentFrameTime;


        std::size_t m_frameSize; //joints in a frame
        std::size_t m_frameCount;
        std::vector<Joint> m_frames; //indexed by steps of frameSize
        std::vector<glm::mat4> m_currentFrame; //current interpolated output

        std::vector<SkeletalAnim> m_animations;

        friend class SkeletalAnimator;
    };
}