/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/graphics/Spatial.hpp>

#include <crogine/detail/glm/mat4x4.hpp>

#include <functional>

namespace cro
{
    class CRO_EXPORT_API Model final
    {
    public:
        Model();
        Model(Mesh::Data, Material::Data); //applied to all meshes by default
        
#ifdef PLATFORM_DESKTOP
        //on desktop we own our VAO so we need to manage the handle
        ~Model();

        Model(const Model&) = delete;
        Model& operator = (const Model&) = delete;

        Model(Model&&) noexcept;
        Model& operator = (Model&&) noexcept;
#endif

        /*!
        \brief Applies the given material to the given sub-mesh index
        \param idx The index of the sub-mesh to which to apply the material
        \param material Data struct containing the material definition
        */
        void setMaterial(std::size_t idx, Material::Data material);

        /*!
        \brief Sets a parameter on the material applied at the given index
        */
        template <typename T>
        void setMaterialProperty(std::size_t idx, const std::string& str, T val)
        {
            CRO_ASSERT(idx < m_materials[Mesh::IndexData::Final].size(), "Index out of range");
            m_materials[Mesh::IndexData::Final][idx].setProperty(str, val);
        }

        /*!
        \brief Sets a parameter on the shadow map material applied at the given index
        This is generally only useful for custom shadow map shaders, as the default has
        no parameters to set.
        */
        template <typename T>
        void setShadowMaterialProperty(std::size_t idx, const std::string& str, T val)
        {
            CRO_ASSERT(idx < m_materials[Mesh::IndexData::Shadow].size(), "Index out of range");
            m_materials[Mesh::IndexData::Shadow][idx].setProperty(str, val);
        }

        /*!
        \brief Enables or disables depth testing for a material at the given index
        \param idx Index of the material to set the depth test parameter on
        \param enabled Set to true to enable depth testing
        This doesn't affect shadow pass materials. Useful for overlay materials
        such as wireframe meshes.
        */
        void setDepthTestEnabled(std::size_t idx, bool enabled);

        /*!
        \brief Enables or disables double-sided rendering for a material at the given index
        \param idx Index of the material to set the double sided property on
        \param doubleSided Set to true for double sided rendering else false
        */
        void setDoubleSided(std::size_t idx, bool doubleSided);

        /*!
        \brief Returns a reference to the mesh data for this model.
        This can be used to update vertex data, but care should be taken to
        not modify the attribute layout as this will already be bound to the
        model's material.
        */
        Mesh::Data& getMeshData() { return m_meshData; };
        const Mesh::Data& getMeshData() const { return m_meshData; };

        /*!
        \brief Sets a model's optional skeleton
        \param frame Pointer to the first transform in the skeleton
        \param size Number of joints in the skeleton.
        This is used internally by the SkeletalAnimator system, setting this
        manually will most likely cause undefined results
        */
        void setSkeleton(glm::mat4* frame, std::size_t size);

        /*!
        \brief Sets the material used when casting shadows.
        This material is used by ShadowMap systems to render the depth data
        of the model.
        \param idx The index of the sub-mesh to which to apply the material
        \param material Data struct containing the material definition
        */
        void setShadowMaterial(std::size_t idx, Material::Data material);

        /*!
        \brief Sets the model hidden or unhidden. If this is true then the
        model won't be drawn.
        */
        void setHidden(bool hidden) { m_hidden = hidden; }

        /*!
        \brief Returns whether or not this model is currently hidden from rendering
        */
        bool isHidden() const { return m_hidden; }

        /*!
        \brief Sets the render flags for this model.
        If the render flags, when AND'd with the current render flags of the active camera,
        are non-zero then the model is drawn, else the model is skipped by rendering.
        Defaults to std::numeric_limits<std::uint64_t>::max() (all flags set)
        \see Camera::renderFlags
        */
        void setRenderFlags(std::uint64_t flags) { m_renderFlags = flags; }

        /*!
        \brief Returns the current render flags of this model.
        */
        std::uint64_t getRenderFlags() const { return m_renderFlags; }

        /*!
        \brief Used to determine if this model should be rendered with faces front
        facing or rear facing. By default models are rendered with front-facing
        triangles, but in cases where a model is scaled negatively on 1 or 3
        axes then the model will appear 'inside out'. Setting the model's facing
        parameter to Facing::Back will then reverse the direction of the faces causing
        the model to appear correctly again.
        \see setFacing()
        */
        enum class Facing
        {
            Front, Back
        };

        /*!
        \brief Sets the direction of the model's faces when rendering. Ultimately
        a wrapper for glFrontFace(). The default value is Facing::Front (aka GL_CCW)
        \param direction Facing::Front for default behaviour or Facing::Back for inverse
        \see Facing
        */
        void setFacing(Facing direction);

        /*!
        \brief Returns the current Facing value.
        \see Facing
        */
        Facing getFacing() const;

        /*!
        \brief Returns the joint count if this model has a skeleton
        */
        std::size_t getJointCount() const { return m_jointCount; }

        /*!
        \brief Returns the material data associated with the given sub-mesh
        at the given pass.
        \param pass Can be IndexData::Final or IndexData::Shadow
        \param sub-mesh Index of the sub-mesh to retrieve.
        \returns const reference to the material if it exists
        */
        const Material::Data& getMaterialData(Mesh::IndexData::Pass pass, std::size_t submesh) const;

        /*!
        \brief Sets the transform data for instanced models
        \param transforms A std::vector of glm::mat4 containing the transforms
        for each instance.

        To enable this make sure to load the model using a ModelDefinition with the
        'instanced' parameter set to true, or with a material that has a compatible
        instancing shader.
        Note that it is not generally recommended to update this frequently as the VBO
        containing the transform and normal matrix data is completely recalculated, which
        can take a long time for large arrays.
        Transform data is copied from the vector, so the data may safely be discarded
        */
        void setInstanceTransforms(const std::vector<glm::mat4>& transforms);

        /*!
        \brief Updates the transform data for an instanced model.
        This assumes that setInstanceTransforms() has been called at least once.
        Bounding boxes and normal matrices are NOT recalculated - and the vertex
        attributes of materials are not updated. This function is intended for
        updating the the transform and normal matrix buffers as quickly as possible.

        Pointers to multiple vectors are used so that each vector of data can be
        uploaded sequentially with glBufferSubData(). The total size must NOT exceed
        the size of the original buffer created with setInstanceTransforms()

        \param transforms A std::vector containing the transforms with which to render
        \param normalMatricies A std::vector containing the inverse transpose of each
        transform
        */
        void updateInstanceTransforms(const std::vector<const std::vector<glm::mat4>*>& transforms, const std::vector<const std::vector<glm::mat3>*>& normalMatrices);

        /*!
        \brief Returns the bounding sphere of the Model
        Note that this may not necessarily be the same as the of the Model's
        MeshData, as it is expanded to include any instanced geometry.
        Useful for render culling.
        */
        cro::Sphere getBoundingSphere() const { return m_boundingSphere; }

        /*!
        \brief Returns the bounding AABB of the Model
        Note that this may not necessarily be the same as the of the Model's
        MeshData, as it is expanded to include any instanced geometry.
        Useful for render culling.
        */
        cro::Box getAABB() const { return m_boundingBox; }

#ifdef PLATFORM_DESKTOP
        /*!
        \brief Used to implement custom draw functions for the Model.
        This is used internally to set whether the Model is drawn with instancing
        or not. Overriding this yourself probably won't do what  you expect
        */
        std::function<void(std::int32_t, std::int32_t)> draw;
#endif

    private:

        bool m_hidden;
        std::uint64_t m_renderFlags;
        std::uint32_t m_facing;
        cro::Sphere m_boundingSphere;
        cro::Box m_boundingBox;
        cro::Box m_meshBox; //AABB received from mesh data

        Mesh::Data m_meshData;
        std::array<std::array<Material::Data, Mesh::IndexData::MaxBuffers>, Mesh::IndexData::Count> m_materials = {};       

        using VAOPair = std::array<std::uint32_t, Mesh::IndexData::Pass::Count>;
        std::array<VAOPair, Mesh::IndexData::MaxBuffers> m_vaos = {};

        std::vector<std::pair<std::size_t, Material::Property*>> m_animations;
        void initMaterialAnimation(std::size_t);
        void updateMaterialAnimations(float);

        void bindMaterial(Material::Data&);
        void updateBounds();
        
#ifdef PLATFORM_DESKTOP
        void updateVAO(std::size_t materialIndex, std::int32_t passIndex);

        struct DrawSingle final
        {
            void operator()(std::int32_t, std::int32_t) const;

            const Model& m_model;
            DrawSingle(const Model& m) : m_model(m) {}
        };

        struct DrawInstanced final
        {
            void operator()(std::int32_t, std::int32_t) const;

            const Model& m_model;
            DrawInstanced(const Model& m) : m_model(m) {}
        };

        struct InstanceBuffers final
        {
            std::uint32_t transformBuffer = 0;
            std::uint32_t normalBuffer = 0;
            std::uint32_t instanceCount = 0;
        }m_instanceBuffers;
#endif //DESKTOP

        glm::mat4* m_skeleton = nullptr;
        std::size_t m_jointCount = 0;

        //used with BalancedTree if active in frustum culling
        std::int32_t m_treeID = -1;
        glm::vec3 m_lastWorldPosition = glm::vec3(0.f);

        friend class ModelRenderer;
        friend class ShadowMapRenderer;
        friend class DeferredRenderSystem;
    };
}