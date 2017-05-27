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

#ifndef CRO_MODEL_COMPONENT_HPP_
#define CRO_MODEL_COMPONENT_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/MaterialData.hpp>

#include <glm/mat4x4.hpp>

namespace cro
{
    class CRO_EXPORT_API Model final
    {
    public:
        Model() = default;
        Model(Mesh::Data, Material::Data); //applied to all meshes by default
        
        /*!
        \brief Applies the given material to the given sub-mesh index
        */
        void setMaterial(std::size_t, Material::Data);

        /*!
        \brief Sets a parameter on the material applied at the given index
        */
        template <typename T>
        void setMaterialProperty(std::size_t idx, const std::string& str, T val)
        {
            CRO_ASSERT(idx < m_materials.size(), "Index out of range");
            m_materials[idx].setProperty(str, val);
        }

        /*!
        \brief Returns a reference to the mesh data for this model.
        This can be used to update vertex data, but care should be taken to
        not modify the attribute layout as this will already be bound to the
        model's material.
        */
        Mesh::Data& getMeshData() { return m_meshData; };

        /*!
        \brief Sets a model's optional skeleton
        \param frame Pointer to the first transform in the skeleton
        \param size Number of joints in the skeleton.
        This is used internally by the SkeletalAnimator system, setting this
        manually will most likely cause undefined results
        */
        void setSkeleton(glm::mat4* frame, std::size_t size);

    private:
        Mesh::Data m_meshData;
        std::array<Material::Data, Mesh::IndexData::MaxBuffers> m_materials{};
        
        void bindMaterial(Material::Data&);
        
        glm::mat4* m_skeleton;
        std::size_t m_jointCount;

        friend class SceneRenderer;
    };
}


#endif //CRO_MODEL_COMPONENT_HPP_
