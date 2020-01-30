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

    uint8 Flags. A single byte containing the following flags
    marking the vertex attributes interleaved in the VBO data:
    
    Position
    Colour
    Normal
    Tangent
    Bitangent
    UV0 - for diffuse mapping
    UV1 - for light mapping

    uint8 index array count. The number of index arrays in this mesh

    int32 the offset, in bytes, from the beginning of the file to the
    beginning of the index array.

    int32[indexArrayCount] an array of array sizes. Each value
    corresponds to the size of each array in the offset array, in bytes.

    float[] vbo data. Interleaved vbo data containing each attribute
    marked in the flags byte. Starts at sizeof header, ends at
    indec array offset - 1. Header size is calculated as:

    sizeof(uint8) flags +
    sizeof(unit8) array count + 
    sizeof(int32) array offset +
    (sizeof(int32) * array count)

    uint32[fileSize - arrayOffset] array of concatenated index
    arrays, the sizes of which are stored in array size array

    Be Aware: binary files are little endian (intel) by default.

    */
    namespace VertexProperty
    {
        enum
        {
            Position = (1 << 0),
            Colour = (1 << 1),
            Normal = (1 << 2),
            Tangent = (1 << 3),
            Bitangent = (1 << 4),
            UV0 = (1 << 5),
            UV1 = (1 << 6)
        };
    }

    class CRO_EXPORT_API StaticMeshBuilder final : public MeshBuilder
    {
    public:
        /*!
        \brief Constructor
        \param path Relative path to cmf file to load
        */
        explicit StaticMeshBuilder(const std::string& path);

        ~StaticMeshBuilder();

        /*!
        \brief Implements the UID based on the path given to the ctor
        */
        std::size_t getUID() const override { return m_uid; }

    private:
        std::string m_path;
        std::size_t m_uid;
        mutable SDL_RWops* m_file;
        Mesh::Data build() const override;

        bool checkError(std::size_t readCount) const;
    };
}