/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "Q3Bsp.hpp"
#include "Patch.hpp"

#include <crogine/gui/GuiClient.hpp>

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>

#include <crogine/graphics/BoundingBox.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/MaterialData.hpp>

namespace Q3
{
    using EntityData = std::vector<std::pair<std::string, std::string>>;

    struct SpawnPoint final
    {
        glm::vec3 position = glm::vec3(0.f);
        float rotation = 0.f;
    };

    struct PointLight final
    {
        glm::vec3 position = glm::vec3(0.f);
        cro::Colour colour;
    };
}

class Q3BspSystem final : public cro::System, public cro::Renderable, public cro::GuiClient
{
public:
    explicit Q3BspSystem(cro::MessageBus&);
    ~Q3BspSystem();

    Q3BspSystem(const Q3BspSystem&) = delete;
    Q3BspSystem(Q3BspSystem&&) = delete;
    const Q3BspSystem& operator = (const Q3BspSystem&) = delete;
    Q3BspSystem& operator = (Q3BspSystem&&) = delete;

    void process(float) override;
    void updateDrawList(cro::Entity camera) override;
    void render(cro::Entity camera, const cro::RenderTarget& target) override;

    bool loadMap(const std::string&);
    const std::vector<Q3::SpawnPoint>& getSpawnPoints() const { return m_spawnPoints; }

private:

    bool m_loaded;

    //----raw map data----//

    //geometry
    std::vector<std::int32_t> m_indices;
    std::vector<Q3::Face> m_faces;
    struct FaceMatData final
    {
        std::int32_t materialID = 0;
        std::int32_t lightmapID = 0;
        std::uint32_t combinedID = 0;
    };

    std::vector<std::int32_t> m_patchIndices;
    std::vector<Patch> m_patches;

    //material data
    std::vector<FaceMatData> m_faceMatIDs;
    std::vector<cro::Texture> m_lightmaps;
    FaceMatData m_activeMatData;

    //visibility data
    std::vector<glm::vec4> m_planes;
    std::vector<Q3::Node> m_nodes;
    std::vector<Q3::Leaf> m_leaves;
    Q3::VisiData m_clusters;
    std::vector<std::int8_t> m_clusterBitsets; //the m_clusters.bitset points to this to save on manual memory management.
    std::vector<cro::Box> m_leafBoundingBoxes;
    std::vector<std::int32_t> m_leafFaces;


    //----Parsed data for rendering----//
    //we're using these structs for convenience
    //not necessarily all fields are populated
    struct MeshData final
    {
        std::vector<std::pair<cro::Mesh::IndexData, FaceMatData>> submeshes;
        cro::Mesh::Data mesh;
        std::size_t activeSubmeshCount = 0;

        enum
        {
            Brush, Patch,

            Count
        };
    };
    std::array<MeshData, MeshData::Count> m_meshes = {};

    cro::Shader m_shader;
    cro::Material::Data m_material;

    struct UniformLocation final
    {
        enum
        {
            WorldMatrix,
            WorldViewMatrix,
            ViewProjectionMatrix,
            NormalMatrix,

            ClipPlane,

            Texture0,
            Texture1,

            Count
        };
    };
    std::array<std::int32_t, UniformLocation::Count> m_uniforms = {};

    std::vector<Q3::EntityData> m_entityData;
    void parseEntities(const std::vector<char>&);

    std::vector<Q3::SpawnPoint> m_spawnPoints;

    void buildLightmaps(SDL_RWops* file, std::uint32_t count);
    void createMesh(const std::vector<Q3::Vertex>&, std::size_t);
    void createPatchMesh(const std::vector<float>&);
    void initMaterial();

    template <typename T>
    void parseLump(std::vector<T>& dest, SDL_RWops* file, Q3::Lump lumpInfo) const
    {
        auto lumpCount = lumpInfo.length / sizeof(T);
        dest.resize(lumpCount);
        SDL_RWseek(file, lumpInfo.offset, RW_SEEK_SET);
        SDL_RWread(file, dest.data(), sizeof(T), lumpCount);
    }

    //visibility calc
    std::int32_t findLeaf(glm::vec3) const;
    bool clusterVisible(std::int32_t currentCluster, std::int32_t testCluster) const;

    void flushEntity(cro::Entity) override {};
};