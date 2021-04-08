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
    namespace Detail::ModelBinary
    {
        struct SkeletonHeader;
        struct SerialAttachment;
    }

    /*!
    \brief Describes an animation made up from a series of
    frames within a skeleton.
    This is used as part of a Skeleton component, rather than
    as a stand-alone component itself
    */
    struct CRO_EXPORT_API SkeletalAnim final
    {
        std::string name;
        std::uint32_t startFrame = 0;
        std::uint32_t frameCount = 0;
        std::uint32_t currentFrame = 0;
        float frameRate = 12.f;
        bool looped = false;
        float playbackRate = 0.f;
    };

    /*!
    \brief Represents a Joint in the skeleton
    These are used to make up the jointsof a key frame.
    Each transform is assumed to *already* be transformed
    by its parents - done by the model loader when the 
    key frames were created.
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

        //contains the object space transform of the joint
        //used for calculating joint world positions when
        //raising notifications or calculating attachments
        glm::mat4 worldMatrix = glm::mat4(1.f);
    };

    /*!
    \brief Represents and attachment point on a skeleton.
    Attachment points are used for attaching other models,
    for example weapons or items, to a skinned mesh.
    Attachment points contain the ID of the joint to which
    they are associated, as well as a relative translation
    and rotation. Once an attachment is added its position
    is fixed relative to its parent joint.
    */
    struct AttachmentPoint final
    {
    public:
        AttachmentPoint(std::int32_t parent, glm::vec3 translation, glm::quat rotation)
            : m_parent(parent), m_translation(translation), m_rotation(rotation)
        {
            CRO_ASSERT(parent > -1, "");
            m_transform = glm::translate(glm::mat4(1.f), translation);
            m_transform *= glm::toMat4(rotation);
        }
        glm::mat4 getLocalTransform() const { return m_transform; }

    private:
        std::int32_t m_parent;
        glm::vec3 m_translation;
        glm::quat m_rotation;
        glm::mat4 m_transform;

        friend class Skeleton;
        friend class SkeletalAnimator;
        friend struct cro::Detail::ModelBinary::SerialAttachment;
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

        enum State
        {
            Playing, Stopped
        };
        /*!
        \brief Return the current animation state of the Skeleton
        */
        State getState() const;

        /*!
        \brief Returns a reference to the array of animations for this skeleton
        */
        std::vector<SkeletalAnim>& getAnimations() { return m_animations; }
        const std::vector<SkeletalAnim>& getAnimations() const { return m_animations; }

        /*!
        \brief Returns the index of the current animation
        */
        std::int32_t getCurrentAnimation() const { return m_currentAnimation; }

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
        \brief Returns the index of the current frame from the beginning of
        the frames array, not the beginning of the current animation.
        */
        std::size_t getCurrentFrame() const;

        /*!
        \brief Returns the number of joints in a single frame, or zero if no animations are added
        */
        std::size_t getFrameSize() const { return m_frameSize; }

        /*!
        \brief Return the total number of frames of animation
        */
        std::size_t getFrameCount() const { return m_frameCount; }

        /*!
        \brief Returns the current, normalised, time between frames
        */
        float getCurrentFrameTime() const { return m_currentFrameTime / m_frameTime; }

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

        /*!
        \brief Represents a notification to be raised on animation events
        */
        struct Notification final
        {
            std::int32_t jointID = -1; //!< The joint which should raise this notification
            std::int32_t userID = -1; //!< The user defined ID to include in the notification message
        };

        /*!
        \brief Adds a notification raised by animation events.
        Notifications allow events to be raised when a certain frame is
        displayed, containing a user definable event ID, and the world
        position of the joint which raised the event. The event is relayed
        via the MessageBus and appears in a SkeletalAnimEvent. This is
        useful for example when a character places a foot on the ground
        for triggering audio and/or particle effects.
        \param frameID The index of the frame which triggers this event
        \param notification A notification struct
        \see Notification
        */
        void addNotification(std::size_t frameID, Notification notification);

        /*!
        \brief Returns a reference to the vector of notifications associated with the skeleton
        */
        const std::vector<std::vector<Notification>>& getNotifications() const { return m_notifications; }

        /*!
        \brief Adds an attachment point to the skeleton.
        The attachment point's translation should be relative to
        the Skeleton's joint to which it is attached.
        \returns ID of the created attament point.
        */
        std::int32_t addAttachmentPoint(const AttachmentPoint&);

        /*!
        \brief Returns the requested attachment transform in object local space.
        That is, the transform relative to the skinned mesh. Multiply it with
        the parent entity's world transform to get the attachment point in world space.
        \param id The id of the attachment point to retrieve. This is the ID returned
        by addAttachment()
        */
        glm::mat4 getAttachmentPoint(std::int32_t id) const;

        /*!
        \brief Return a reference to the vector of attachments
        */
        const std::vector<AttachmentPoint>& getAttachmentPoints() const { return m_attachmentPoints; }

    private:

        float m_playbackRate;

        std::int32_t m_currentAnimation;
        std::int32_t m_nextAnimation;

        float m_blendTime;
        float m_currentBlendTime;

        float m_frameTime;
        float m_currentFrameTime;


        std::size_t m_frameSize; //joints in a frame
        std::size_t m_frameCount;
        std::vector<Joint> m_frames; //indexed by steps of frameSize
        std::vector<glm::mat4> m_currentFrame; //current interpolated output

        std::vector<SkeletalAnim> m_animations;

        std::vector<std::vector<Notification>> m_notifications; //for each frame a pair of jointID and message ID

        std::vector<AttachmentPoint> m_attachmentPoints;

        friend class SkeletalAnimator;
        friend struct Detail::ModelBinary::SkeletonHeader;
    };
}