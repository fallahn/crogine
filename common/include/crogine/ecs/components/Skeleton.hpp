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

#ifndef CRO_SKELETON_HPP_
#define CRO_SKELETON_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>

#include <glm/mat4x4.hpp>

#include <vector> //TODO replace this with our own type
#include <string>

namespace cro
{
    /*!
    \brief Describes an animation made up from a series of
    frames within a skeleton
    */
    struct CRO_EXPORT_API SkeletalAnim final
    {
        std::string name;
        uint32 startFrame = 0;
        uint32 frameCount = 0;
        float frameRate = 12.f;
        bool looped = true;
    };

    /*!
    \brief A hierarchy of bones stored as
    transforms, used to animate 3D models.
    */
    class CRO_EXPORT_API Skeleton final
    {
    public:
        using Frame = std::vector<glm::mat4>;

        std::vector<Frame> frames; //TODO make this a single array with all the frames as sub arrays
        Frame currentFrame; //current interpolated output

        std::vector<SkeletalAnim> animations;
        std::size_t currentAnimation = 0;
        std::size_t nextAnimation = 0;

    private:
    };
}

#endif //CRO_SKELETON_HPP_