/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

#include <crogine/graphics/BoundingBox.hpp>
#include <crogine/ecs/Entity.hpp>

#include <vector>
#include <string>

namespace cro
{
    /*
    Note that the following structs cannot be updated as
    they are serialised as binary data in model files.    
    */
    namespace Detail::ModelBinary
    {
        struct SkeletonHeader;
        struct SkeletonHeaderV2;
    }

    /*!
    \brief Represents a Joint in the skeleton
    These are used to make up the joints of a key frame.
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
        float currentFrameTime = 0.f;
        float frameTime = 1.f / frameRate;

        //holds the current state of interpolation pre-transform
        //so it can be mixed with other animations before creating
        //final output
        std::vector<Joint> interpolationOutput;
        void resetInterp(const class Skeleton&);
    };

    
    /*!
    \brief Represents and attachment on a skeleton.
    Attachments are used for attaching other models,
    for example weapons or items, to a skinned mesh.
    Attachments contain the ID of the joint to which
    they are associated, as well as a relative translation
    and rotation. The skeletal animation system will automatically
    update the transform of any attached model to match
    that of its parent joint.
    */
    struct CRO_EXPORT_API Attachment final
    {
    public:
        Attachment() {};
        explicit Attachment(cro::Entity model)
            : m_model(model) {}

        void setParent(std::int32_t parentID);
        void setModel(cro::Entity model);

        void setPosition(glm::vec3 position);
        void setRotation(glm::quat rotation);
        void setScale(glm::vec3 scale);
        void setName(const std::string&);

        std::int32_t getParent() const { return m_parent; }
        cro::Entity getModel() const { return m_model; }
        glm::vec3 getPosition() const { return m_position; }
        glm::quat getRotation() const { return m_rotation; }
        glm::vec3 getScale() const { return m_scale; }
        const glm::mat4& getLocalTransform() const;
        const std::string& getName() const { return m_name; }

        static constexpr std::size_t MaxNameLength = 30;

    private:
        cro::Entity m_model;
        std::int32_t m_parent = 0;
        glm::vec3 m_position = glm::vec3(0.f);
        glm::quat m_rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
        glm::vec3 m_scale = glm::vec3(1.f);
        std::string m_name = "Attachment";
        
        //mutable to allow for dirty optimisation
        mutable glm::mat4 m_transform = glm::mat4(1.f);
        mutable bool m_dirtyTx = true;


        void updateLocalTransform() const;
    };

    /*!
    \brief A hierarchy of bones stored as
    transforms, used to animate 3D models.
    These are updated by the SkeletalAnimator system.
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
        \param rate A speed multiplier at which to play the animation eg 1 is normal speed
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
        \brief Jumps to the given frame in the current animation
        \param frame Index of the frame relative to the beginning of
        the animation. Does nothing if the frame is out of range.
        */
        void gotoFrame(std::uint32_t frame);

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
        \brief Returns the index of the animation with the given name if it exists
        else returns -1
        */
        std::int32_t getAnimationIndex(const std::string& name) const;

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
        \brief Copies an animation from an existing Skeleton component into this one
        If an animation with the name already exists then it is overwritten!
        \param source The skeleton component to copy from
        \param idx The index of the animation to copy. If this is out of range nothing is copied
        Note that animations in this component will be re-indexed so existing indices will be invalid
        \returns true on success else returns false
        */
        bool addAnimation(const Skeleton& source, std::size_t idx);

        /*!
        \brief Adds a frame of animation.
        If this is the first frame then this will set the frame size (aka joint count)
        of a single frame. Subsequent frames must match this number of joints.
        */
        void addFrame(const std::vector<Joint>&);

        /*!
        \brief Removes the animation at the given index.
        Note that all animations with subsequent indices will be
        re-indexed.
        Returns false if no animation was removed - eg index was out of range
        */
        bool removeAnimation(std::size_t index);

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
        float getCurrentFrameTime() const;

        /*!
        \brief Returns the normalised progress through the current animation
        */
        float getAnimationProgress() const;

        operator bool() const
        {
            return (!m_frames.empty() && (m_frames.size() == m_frameSize * m_frameCount));
        }

        /*!
        \brief Returns a reference to the vector of joints which make up the key frames
        Note that the stride of each frame is equal the number of joints in a frame
        aka FrameSize
        */
        const std::vector<Joint>& getFrames() const { return m_frames; }

        /*!
        \brief Represents a notification to be raised on animation events
        */
        struct Notification final
        {
            std::uint32_t jointID = 0; //!< The joint which should raise this notification
            std::int32_t userID = -1; //!< The user defined ID to include in the notification message
            std::string name; //!< Name as it appears in the editor
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
        std::vector<std::vector<Notification>>& getNotifications() { return m_notifications; }

        /*!
        \brief Adds an attachment to the skeleton.
        The attachment's translation should be relative to
        the Skeleton's joint to which it is attached.
        \returns ID of the created attachment point.
        */
        std::int32_t addAttachment(const Attachment&);

        /*!
        \brief Returns the index of the attachment with the given
        name, or -1 if the attachment does not exist. If more than
        one attachment with the same name exists the first result is returned.
        */
        std::int32_t getAttachmentIndex(const std::string& name) const;

        /*!
        \brief Returns the requested attachment transform in world space.
        This transform is automatically applied to the attached model by the
        SkeletalAnimationSystem.
        \param id The id of the attachment point to retrieve. This is the ID returned
        by addAttachment()
        */
        glm::mat4 getAttachmentTransform(std::int32_t id) const;

        /*!
        \brief Return a reference to the vector of attachments
        */
        const std::vector<Attachment>& getAttachments() const { return m_attachments; }
        std::vector<Attachment>& getAttachments() { return m_attachments; }

        /*
        \brief Set the inverse bind pose for the skeleton.
        \param invBindPose A std::vector of glm::mat4 which contain
        the inverse bind pose (world to bone space) matrices.
        Size must match the frame size. That is there must be
        as many matrices as there are joints.
        */
        void setInverseBindPose(const std::vector<glm::mat4>& invBindPose);

        /*!
        \brief Returns a reference to the inverse bind pose
        */
        const std::vector<glm::mat4>& getInverseBindPose() const { return m_invBindPose; }

        /*!
        \brief Sets the root transform applied to a frame during interpolation.
        Use this to rescale/orientate a model, for example if importing a z-up model
        */
        void setRootTransform(const glm::mat4& transform);

        /*!
        \brief Gets the current root transform of the skeleton.
        */
        const glm::mat4& getRootTransform() const { return m_rootTransform; }

        /*!
        \brief Sets whether or not the animation frames are interpolated
        Setting this to false can improve performance with a lot of models
        (or during debugging) but animation will appear less smooth
        */
        void setInterpolationEnabled(bool enabled) { m_useInterpolation = enabled; }

        /*!
        \brief Returns whether interpolation between frames is currently enabled
        \see setInterpolationEnabled
        */
        bool getInterpolationEnabled() const { return m_useInterpolation; }

        /*!
        \brief Sets the maximum distance from the camera to use interpolation.
        Models further from this will still animate but the frames will skip
        from one to the next without being interpolated in between.
        Defaults to 50 units
        */
        void setMaxInterpolationDistance(float distance) { m_interpolationDistance = std::max(1.f, distance * distance); }

        /*!
        \brief Returns the current blend time if blending between two animations
        */
        float getCurrentBlendTime() const { return m_blendTime; }

        /*!
        \brief Returns the index of the currently playing animation and next animation
        if the animations are currently being blended - else returns -1
        */
        std::pair<std::int32_t, std::int32_t> getActiveAnimations() const { return std::make_pair(m_currentAnimation, m_nextAnimation); }

    private:

        std::int32_t m_currentAnimation;
        std::int32_t m_nextAnimation;
        State m_state;

        float m_blendTime;
        float m_currentBlendTime;

        bool m_useInterpolation;
        float m_interpolationDistance;

        std::size_t m_frameSize; //joints in a frame
        std::size_t m_frameCount;
        std::vector<Joint> m_frames; //indexed by steps of frameSize
        std::vector<glm::mat4> m_currentFrame; //current interpolated output
        std::vector<glm::mat4> m_invBindPose;
        std::vector<glm::mat4> m_bindPose;
        glm::mat4 m_rootTransform = glm::mat4(1.f);

        std::vector<SkeletalAnim> m_animations;

        std::vector<std::vector<Notification>> m_notifications; //for each frame a pair of jointID and message ID

        std::vector<Attachment> m_attachments;

        std::vector<cro::Box> m_keyFrameBounds; //calc'd on joining the System for each key frame

        friend class SkeletalAnimator;
        friend struct SkeletalAnim;
        friend struct Detail::ModelBinary::SkeletonHeader;
        friend struct Detail::ModelBinary::SkeletonHeaderV2;

        void buildKeyframe(std::size_t frame);
    };
}