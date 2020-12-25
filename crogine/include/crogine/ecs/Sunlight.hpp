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

    Entities with a Sunlight component also require a Transform
    component, from the orientation of which the light direction
    is derived.
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
        property.
        \param direction A vec3 representing the direction the sun is facing. This is
        automatically normalised when it is set.
        */
        void setDirection(glm::vec3 direction);

        /*!
        \brief Returns the current world space direction of the Sunlight.
        \returns The normalised direction as set with setDirection()
        multiplied with the parent entity's rotation. Note that
        this is only accurate when the sunlight component is
        currently active in the Scene.
        */
        glm::vec3 getDirection() const;


    private:
        cro::Colour m_colour;
        glm::vec3 m_direction;
        glm::vec3 m_directionRotated;

        friend class Scene;
    };
}