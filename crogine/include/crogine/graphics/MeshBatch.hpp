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
#include <crogine/detail/glm/mat4x4.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace cro
{
    namespace Mesh
    {
        struct Data;
    }

    /*!
    \brief Static Mesh batcher.
    This is a utility class for batching the vertex data of multiple
    static meshes or many instances of the same mesh, with multiple transforms.

    All meshes must have the same vertex format, and will share the same material
    or set of materials.

    This class only processes the vertex and index data - once it is processed
    it must be uploaded to a valid VBO via a MeshData struct, which has been previously
    created by the MeshResource class. Once the data has been applied to a mesh
    this class can safely be destroyed, usually by letting it drop out of scope.

    */

    class CRO_EXPORT_API MeshBatch final
    {
    public:
        /*!
        \brief Constructor.
        \param flags A bitmask of cro::VertexProperty flags indicating which
        vertex attributes this batch should expect. If a given mesh does not
        match these flags then it will not be added to the batch.
        */
        explicit MeshBatch(std::uint32_t flags);

        /*!
        \brief Add the given mesh to the batch.
        \param path A string containing the path to a *.cmf file to add to the batch
        \param transforms A vector of transforms to apply to the mesh. For each
        transform in the vector a new instance of the given mesh will be added,
        scaled, rotated and translated by that transform.
        \returns true on success, or false if something went wrong loading the mesh data.
        */
        bool addMesh(const std::string& path, const std::vector<glm::mat4>& transforms);

        /*!
        \brief Applies the vertex data to the given Mesh::Data struct.
        Any vertex data / index data currently loaded by the MeshBatch will be uploaded
        to the VBO/IBOs for the given struct. This function expects the MeshData
        to be valid, having been created via the MeshResource class, and that the
        MeshData have the same vertex attributes that were supplied when the
        MeshBatch was created.
        \param data MeshData struct containing the buffer data to which the MeshBatch
        will be applied. Note that this is a *reference* as the MeshData will be modified.
        Passing in a copy here will have undesirable results - ie cause bugs ^_^
        */
        void updateMeshData(Mesh::Data& data) const;

    private:

        std::uint32_t m_flags;
        std::vector<float> m_vertexData;
        std::array<std::vector<std::uint32_t>, 32u> m_indexData;
    };
}