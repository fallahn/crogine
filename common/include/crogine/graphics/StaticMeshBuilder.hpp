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

#ifndef CRO_STATIC_MESH_BUILDER_HPP_
#define CRO_STATIC_MESH_BUILDER_HPP_

#include <crogine/graphics/MeshBuilder.hpp>

namespace cro
{
    /*!
    \brief Static Mesh builder class.
    Static meshes have no skeletal animation, but support
    lightmapping via prebaked maps. The meshes are pre-compiled
    as a *.cmf file using the Model convert program. The format
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
    currently this is fixed at 1 and exists for possible future use

    int32[indexArrayCount] an array of values representing the offset,
    in bytes, from the beginning of the file to the beginning of each
    index array.

    int32[indexArrayCount] an array of array sizes. Each value
    corresponds to the size of each array in the offset array above.

    float[] vbo data. Interleaved vbo data containing each attribute
    marked in the flags byte. Starts at sizeof header, ends at
    offsetArray[0] - 1. Header size is calculated as:

    sizeof(uint8) flags +
    sizeof(unit8) array count + 
    (sizeof(int32) * array count) +
    (sizeof(int32) * array count)

    uint32[arraySizes[0]] array of uint32 values making up the first
    index array - followed by any further index arrays (not yet used)

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

        ~StaticMeshBuilder();

    private:
        std::string m_path;
        mutable FILE* m_file;
        Mesh::Data build() const override;

        bool checkError(size_t readCount) const;
    };
}

#endif //CRO_STATIC_MESH_BUILDER_HPP_