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

#include <crogine/detail/glm/mat4x4.hpp>

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/RenderTexture.hpp>

namespace cro
{
    /*!
    \brief Renders the reflection and refraction maps used in reflective surfaces.
    Currently supports a single surface whose level, in world units, is set by the
    Scene class. Requires that any model materials which do not use the built in
    shaders to have shaders whose property include a u_clipPlane uniform defined by
    a vec4(vec3 normal, float distance))
    */
    class CRO_EXPORT_API ReflectionMapRenderer final : public cro::System, public cro::Renderable
    {
    public:

        /*!
        \brief Constructor
        \param mb A reference to the active MessageBus
        \param mapResolution The length of one side of the map texture, in pixels.
        Default value is 512. Higher values will create higher quality reflections
        and refreactions, at the cost of performance.
        */
        explicit ReflectionMapRenderer(cro::MessageBus&, std::uint32_t mapResolution = 512u);

        /*!
        \brief Implements the Renderable interface (don't call this manually)
        */
        void updateDrawList(cro::Entity camera) override;

        /*!
        \brief Implements the Renderable interface (don't call this manually)
        */
        void render(Entity camera, const RenderTarget&) override;

        /*!
        \brief Returns the reflection map texture
        */
        const Texture& getReflectionTexture() const { return m_reflectionMap.getTexture(); }

        /*!
        \brief Returns the refraction map texture
        */
        const Texture& getRefractionTexture() const { return m_refractionMap.getTexture(); }

        /*!
        \brief Sets the height of the reflective plane
        */
        void setPlaneHeight(float height) { m_plane.a = -height; }

    private:

        glm::vec4 m_plane;

        RenderTexture m_reflectionMap;
        RenderTexture m_refractionMap;

        void renderList(Entity camera, const RenderTarget&, const glm::mat4& v, const glm::mat4& vp, UniqueType, std::uint32_t);
    };
}
