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

#include <crogine/graphics/MeshBuilder.hpp>

#include <SDL_rwops.h>

namespace cro
{
    /*!
    \brief Static Mesh builder class.
    Static meshes have no skeletal animation, but support
    lightmapping via prebaked maps. The meshes are pre-compiled
    as a *.cmf file using the Model converter program. The format
    is as follows:

    std::uint8_t Flags. A single byte containing the following flags
    marking the vertex attributes interleaved in the VBO data:
    
    Position
    Colour
    Normal
    Tangent
    Bitangent
    UV0 - for diffuse mapping
    UV1 - for light mapping

    std::uint8_t index array count. The number of index arrays in this mesh

    std::int32_t the offset, in bytes, from the beginning of the file to the
    beginning of the index array.

    std::int32_t[indexArrayCount] an array of array sizes. Each value
    corresponds to the size of each array in the offset array, in bytes.

    float[] vbo data. Interleaved vbo data containing each attribute
    marked in the flags byte. Starts at sizeof header, ends at
    indec array offset - 1. Header size is calculated as:

    sizeof(std::uint8_t) flags +
    sizeof(unit8) array count + 
    sizeof(std::int32_t) array offset +
    (sizeof(std::int32_t) * array count)

    std::uint32_t[fileSize - arrayOffset] array of concatenated index
    arrays, the sizes of which are stored in array size array

    Be Aware: binary files are little endian (intel) by default.

    */

    class CRO_EXPORT_API StaticMeshBuilder final : public MeshBuilder
    {
    public:
        /*!
        \brief Constructor
        \param path Relative path to cmf file to load
        */
        explicit StaticMeshBuilder(const std::string& path);

        /*!
        \brief Implements the UID based on the path given to the ctor
        */
        std::size_t getUID() const override { return m_uid; }

    private:
        std::string m_path;
        std::size_t m_uid;
        Mesh::Data build(AllocationResource*) const override;
    };
}