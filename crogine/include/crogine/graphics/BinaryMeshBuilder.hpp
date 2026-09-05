/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2026
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

#include <crogine/detail/ModelBinary.hpp>
#include <crogine/graphics/MeshBuilder.hpp>

#include <filesystem>

namespace cro
{
    /*!
    \brief Class for loading the CroModelBinary format aka *.cmb files
    */
    class CRO_EXPORT_API BinaryMeshBuilder final : public cro::MeshBuilder
    {
    public:
        /*!
        \brief Constructor
        \param path filesystem::path containing path to the model resource file
        \param optimiseOnLoad Attempts to compress the default vertex format
        and assigns shared VBO/IBO resources if true. If false leaves the
        vertex format in uncompressed float for use with the model editor etc
        */
        explicit BinaryMeshBuilder(const std::filesystem::path& path, bool optimseOnLoad = true);

        std::size_t getUID() const override;
        Skeleton getSkeleton() const override;

    private:
        std::filesystem::path m_path;
        bool m_optimiseOnLoad;
        std::size_t m_uid;
        mutable Skeleton m_skeleton;
        Mesh::Data build(AllocationResource*) const override;

        Mesh::Data buildOptimised(AllocationResource*) const;
        Mesh::Data buildDefault() const;

        void calcBounds(Mesh::Data& target, const std::vector<float>& vertData) const;
        void parseSkeleton(RaiiRWops& file, const Detail::ModelBinary::Header& header) const;
    };
}
