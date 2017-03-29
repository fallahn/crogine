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

#ifndef CRO_MATERIAL_RESOURCE_HPP_
#define CRO_MATERIAL_RESOURCE_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/MaterialData.hpp>

#include <unordered_map>

namespace cro
{
    class Shader;

    /*!
    \brief Resource manager to manage the life span of Material data.
    Material data applied to a model is only valid if it is retrieved
    via a MaterialResource instance.
    */
    class CRO_EXPORT_API MaterialResource final : public Detail::SDLResource
    {
    public:
        MaterialResource() = default;

        ~MaterialResource() = default;
        MaterialResource(const MaterialResource&) = delete;
        MaterialResource(MaterialResource&&) = default;
        MaterialResource& operator = (const MaterialResource&) = delete;
        MaterialResource& operator = (MaterialResource&&) = default;

        /*!
        \brief Adds a material to the resource.
        \param ID An integer value to map to the Material
        \param shader A reference to the shader to use when drawing this material
        \returns Reference of the new material data
        */
        Material::Data& add(int32 ID, const Shader& shader);

        /*!
        \brief Returns a copy of the material data for the given ID
        if it has been found to have been added to the MaterialResource.
        \param ID Integer value previously mapped to a material.
        Material data returns is a copy so that it may be configured independently
        of other instances, before being added to a model component.
        */
        Material::Data get(int32 ID) const;

    private:
        std::unordered_map<int32, Material::Data> m_materials;
    };
}

#endif //CRO_MATERIAL_RESOURCE_HPP_