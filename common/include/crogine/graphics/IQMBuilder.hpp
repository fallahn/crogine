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

#ifndef CRO_IQM_BUILDER_HPP_
#define CRO_IQM_BUILDER_HPP_

#include <crogine/graphics/MeshBuilder.hpp>

#include <SDL_rwops.h>

namespace cro
{
    /*!
    \brief Parser for IQM format model files.
    IQM files are a binary mesh format which, unlike cmf files, support
    skinned / skeletal animation. While IQM models may also be static
    they do not support secondary UV coordinates for lightmapping - in
    which case cmf mesh should be preferred.
    */
    class CRO_EXPORT_API IqmBuilder final : public MeshBuilder
    {
    public:
        /*!
        \brief Constructor
        \param path Path to file to parse
        */
        explicit IqmBuilder(const std::string& path);
        ~IqmBuilder();

    private:
        std::string m_path;
        mutable SDL_RWops* m_file;
        Mesh::Data build() const override;

    };
}

#endif //CRO_IQM_BULDER_HPP_