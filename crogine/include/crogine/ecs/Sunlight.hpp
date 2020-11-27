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
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Spatial.hpp>

#include <crogine/detail/glm/mat4x4.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <array>

namespace cro
{
    /*!
    \brief Default light source for a Scene.
    Each scene contains a single Sunlight object which
    acts as a directional light providing default lighting.
    The sunlight object is also the default (and only) shadow
    caster due to limitations of mobile hardware - although
    that's not to say projects can't implement their own shadow
    mapping. PBR, VertexLit and Unlit shaders are all capable of
    receiving shadows by default, assuming the rx_shadow flag
    is set when creating them. PBR and VertexLit materials will also
    apply the sunlight colour and direction when being rendered.
    For objects to cast shadows the Model component must contain
    shadow casting materials (automatically applied in model config
    files when cast_shadow = true) and a Scene has a ShadowMapRenderer
    system within it.

    Entities with a Sunlight component also require a Transform
    component, which not only allows setting the orientation of the
    sunlight (the Direction property is multiplied by the transform)
    but also allows for the shadow map projection to follow the
    Scene's active camera if necessary.

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
        \brief Sets the direction in which the Sunlight is pointing without rotation.
        By default this is straight down the z axis (negative Z). The direction
        is multiplied by any transform on the Sunlight's entity. Normally there is no
        need to set this, instead relying on updating the Sunlight entity's transform
        property, as the camera's orientation is used when rendering shadows.
        \param direction A vec3 representing the direction the sun is facing. This is
        automatically normalised when it is set.
        */
        void setDirection(glm::vec3 direction);

        /*!
        \brief Returns the current direction of the Sunlight.
        \returns The normalised direction as set with setDirection()
        multiplied with the parent entity's rotation. Note that
        this is only accurate when the sunlight component is
        currently active in the Scene.
        */
        glm::vec3 getDirection() const;

        /*!
        \brief Sets the projection matrix.
        Usually shadow mapping with a directional light such as sunlight
        requires is best perfomed with an orthographic projection. Setting
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
        glm::vec3 m_directionRotated;

        glm::mat4 m_viewMatrix;
        glm::mat4 m_projectionMatrix;
        glm::mat4 m_viewProjectionMatrix;

        cro::int32 m_textureID;
        friend class ShadowMapRenderer;
        friend class Scene;

        std::array<Plane, 6u> m_frustum;
        Box m_aabb; //used for early testing with AABB tree
    };
}