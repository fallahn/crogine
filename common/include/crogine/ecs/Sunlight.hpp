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

#ifndef CRO_SUNLIGHT_HPP_
#define CRO_SUNLIGHT_HPP_

#include <crogine/Config.hpp>
#include <crogine/graphics/Colour.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace cro
{
    /*!
    \brief Default light source for a Scene.
    Each scene contains a single Sunlight object which
    acts as a directional light providing default lighting.
    The sunlight object is also the default (and only) shadow
    caster due to limitations of mobile hardware - although
    that's not to say projects can't implement their own shadow
    mapping. VertexLit and Unlit shaders are both capable of
    receiving shadows by default, assuming the rx_shadow flag
    is set when creating them. VertixLit materials will also
    apply the sunlight colour and direction when being rendered.
    For objects to cast shadows the Model component must contain
    shadow casting materials (automatically applied in model config
    files when cast_shadow = true) and a Scene has a ShadowMapRenderer
    system within it.
    \see ShadowMapRenderer
    */
    class CRO_EXPORT_API Sunlight final
    {
    public:
        Sunlight();
        
        /*!
        \brief Sets the sunlight colour.
        This affects models with VertexLit materials, or
        materials whose shaders implement the u_sunlightColour uniform.
        */
        void setColour(cro::Colour);

        /*!
        \brief Returns the current sunlight colour
        */
        cro::Colour getColour() const;

        /*!
        \brief Sets the direction in which the Sunlight is pointing
        */
        void setDirection(glm::vec3);

        /*!
        \brief Returns the current direction of the Sunlight
        */
        glm::vec3 getDirection() const;

        /*!
        \brief Gets the rotation of the light object as a matrix
        with a forward vector equivalent to the direction.
        */
        const glm::mat4& getRotation() const;

        /*!
        \brief Sets the projection matrix.
        Usually shadow mapping with a directional light such as sunlight
        requires is best perfomed with an orthogonal projection. Setting
        this allows the user to define the area and depth of the scene
        covered by the Sunlight shadow map
        */
        void setProjectionMatrix(const glm::mat4&);

        /*!
        \brief Returns the projection matrix (without transformation)
        of the Sunlight - useful for shadow mapping.
        */
        const glm::mat4& getProjectionMatrix() const;

        /*!
        \brief Sets the view / projection matrix of the sunlight.
        This is usually the last view set by the ShadowMapRenderer
        so that it can be used by the ModelRenderer when drawing shadows
        */
        void setViewProjectionMatrix(const glm::mat4&);

        /*!
        \brief Returns the current view / projection matrix.
        \see setViewProjectionMatrix()
        */
        const glm::mat4& getViewProjectionMatrix() const;

        /*!
        \brief Returns the GL handle of the texture the sunlight
        was last rendered to.
        */
        int32 getMapID() const;

    private:
        cro::Colour m_colour;
        glm::vec3 m_direction;
        glm::mat4 m_rotation;
        glm::mat4 m_projection;
        glm::mat4 m_viewProjection;

        cro::int32 m_textureID;
        friend class ShadowMapRenderer;
    };
}

#endif //CRO_SUNLIGHT_HPP_