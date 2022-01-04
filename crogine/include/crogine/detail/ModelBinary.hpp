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

#pragma once

#include <crogine/Config.hpp>

#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/components/Skeleton.hpp>

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

#include <string>
#include <cstring>

namespace cro::Detail::ModelBinary
{
    //appears at the beginning of the file
    static constexpr std::uint32_t MAGIC = 0x736e7542;
    struct CRO_EXPORT_API Header
    {
        //magic
        std::uint32_t magic = MAGIC;
        //version. Indicates which of these members are used
        std::uint32_t version = 1;
        //offset from the beginning of the file to the beginning
        //of the MeshHeader in bytes. If 0 no mesh is defined
        std::uint32_t meshOffset = 0;
        //offset from the beginning of the file to the beginning
        //of the SkeletonHeader in bytes. If 0 no skeleton is defined
        std::uint32_t skeletonOffset = 0;

        //reserved for future expansion
        std::uint32_t reserved0 = 0;
        std::uint32_t reserved1 = 0;
        std::uint32_t reserved2 = 0;
        std::uint32_t reserved3 = 0;
    };
    /*
    Model binaries may contain either mesh data, skeleton data
    or both. They should not be empty. The version number
    is used to discover which of the subsequent properties are valid.
    */

    //version 2 header for updated skeletal data
    //version 2 includes SkeletonHeaderV2 rather than
    //SkeletonHeader and the inverse bind pose.
    struct CRO_EXPORT_API HeaderV2 final : public Header
    {
        HeaderV2() { version = 2; };
    };


    //appears at Header::meshOffset bytes from beginning of the file
    struct CRO_EXPORT_API MeshHeader final
    {
        //bytes from the beginning of the file to
        //the start of the index arrays
        std::uint32_t indexArrayOffset = 0;
        //vertex attribute flags. These match cro::VertexProperty
        //must be at least 1 (position data)
        std::uint16_t flags = 0;
        //number of index arrays. If this is zero
        //the vertices are expected to be drawn with glDrawArrays()
        std::uint16_t indexArrayCount = 0;
    };
    /*!
    Mesh data follows the header:
        std::uint32_t arraySizes[MeshHeader::indexArrayCount] //array of sizes for each of the index arrays
        float vertexData[vertexSize * sizeof(float)] //vertex size is calculated from the flags which define vertex components
        std::uint32_t indexArrays //contiguous arrays of std::uint32_t of arraySizes[i] * arrayCount
    
    Component sizes:
        Position, 3 float XYZ
        Colour, 4 float RGBA normalised components
        Normal, 3 float XYZ normalised vector
        Tangent, 4 float XYZW normalised vector plus sign. Bitangent = cross(normal, tangent.xyz) * tangent.w
        Bitangent - Unused (see above)
        UV0, 2 float normalised UV coords
        UV1, 2 float normalised UV coords
        BlendIndices, 4 float
        BlendWeights, 4 float, should sum as close as possible to 1

    Vertex data is interleaved in the above order
    */




    //appears at Header::skeletonOffset bytes from the beginning of the file
    //and always after the mesh data, if the mesh data exists
    struct CRO_EXPORT_API SkeletonHeader
    {
        std::size_t frameSize = 0; //number of joints in a frame
        std::size_t frameCount = 0;

        std::uint32_t animationCount = 0;
        std::uint32_t notificationCount = 0; //array of frame IDs that have notifications
        std::uint32_t attachmentCount = 0; //number of attachment points

        std::uint32_t reserved0 = 0;

        SkeletonHeader() = default;

        SkeletonHeader& operator = (const cro::Skeleton& skel)
        {
            frameSize = skel.m_frameSize;
            frameCount = skel.m_frameCount;
            animationCount = static_cast<std::uint32_t>(skel.m_animations.size());
            attachmentCount = static_cast<std::uint32_t>(skel.m_attachmentPoints.size());

            for (const auto& nFrame : skel.m_notifications)
            {
                notificationCount += static_cast<std::uint32_t>(nFrame.size());
            }

            return *this;
        }
    };

    //skeleton v2 header includes a root transform for animations
    struct CRO_EXPORT_API SkeletonHeaderV2 final : public SkeletonHeader
    {
        float rootTransform[16] =
        {
            1.f,0.f,0.f,0.f,
            0.f,1.f,0.f,0.f,
            0.f,0.f,1.f,0.f,
            0.f,0.f,0.f,1.f
        };

        SkeletonHeaderV2& operator = (const cro::Skeleton& skel)
        {
            SkeletonHeader::operator=(skel);

            std::memcpy(rootTransform, &skel.m_rootTransform[0][0], sizeof(float) * 16);
            return *this;
        }
    };

    /*
    Array of joints [frameSize * frameCount]
    Array of animations [animationCount]
    Array of notifications [notificationCount]
    Array of attachments [attachmentCount]

    Joints are stored as Joint struct
    Animations are stored as SerialAnimation struct
    Notifications are stored as SerialNotification struct
    Attachments are stored as SerialAttachment struct

    */

    struct CRO_EXPORT_API SerialAnimation final
    {
        static constexpr std::size_t MaxChar = 64;
        char name[MaxChar] = {}; //empty space packed with zero

        std::uint32_t startFrame = 0;
        std::uint32_t frameCount = 0;
        std::uint32_t looped = false;
        float frameRate = 12.f;

        SerialAnimation()
        {
            std::fill(std::begin(name), std::end(name), 0);
        }

        explicit SerialAnimation(const cro::SkeletalAnim& anim)
            : startFrame(anim.startFrame),
            frameCount  (anim.frameCount),
            looped      (anim.looped ? 1 : 0),
            frameRate   (anim.frameRate)
        {
            std::fill(std::begin(name), std::end(name), 0);
            strcpy(name, anim.name.substr(0, MaxChar - 1).c_str());
        }

        SerialAnimation& operator = (const SkeletalAnim& anim)
        {
            std::strcpy(name, anim.name.substr(0, MaxChar - 1).c_str());

            startFrame = anim.startFrame;
            frameCount = anim.frameCount;
            looped = anim.looped ? 1 : 0;
            frameRate = anim.frameRate;
            return *this;
        }
    };

    struct CRO_EXPORT_API SerialNotification final
    {
        std::uint32_t frameID = 0;
        std::int32_t jointID = -1;
        std::int32_t userID = -1;

        SerialNotification() = default;
        SerialNotification(std::uint32_t f, std::int32_t j, std::int32_t u)
            : frameID(f), jointID(j), userID(u)
        {}
    };

    struct CRO_EXPORT_API SerialAttachment final
    {
        glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
        glm::vec3 translation = glm::vec3(0.f);
        std::int32_t parent = -1;

        SerialAttachment() = default;
        explicit SerialAttachment(const cro::AttachmentPoint& ap)
            : rotation  (ap.m_rotation),
            translation (ap.m_translation),
            parent      (ap.m_parent)
        {

        }
        SerialAttachment& operator = (const cro::AttachmentPoint& ap)
        {
            rotation = ap.m_rotation;
            translation = ap.m_translation;
            parent = ap.m_parent;
            return *this;
        }
    };

    CRO_EXPORT_API bool write(cro::Entity, const std::string&, bool includeSkeleton = true);
}