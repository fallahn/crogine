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

#include <crogine/ecs/System.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/MaterialData.hpp>

#include <unordered_map>
#include <memory>

namespace cro
{
    class MaterialResource;

    /*!
    \brief 3D Sprite System
    Updates entities which have a Sprite component and Model
    component attached so that the Sprite can be drawn in a 3D scene.

    When an entity is added to this system it is automatically assigned
    a default sprite shader which is either textured or just vertex coloured.
    These materials can be overridden with more complex ones, for example
    ones which support 3D lighting, by setting the material directly on
    the Model component. (currently this must be done AFTER the system
    has been updated at least once - this will be fixed eventually)
    */
    class CRO_EXPORT_API SpriteSystem3D final : public cro::System
    {
    public:
        /*!
        \brief Constructor
        \param mb A reference to the active MessageBus
        \param pixelsPerUnit By default sprite units are pixels, whereas
        a 3D scene is metres - so a 64 pixel wide sprite will be
        64 metres in the game world! Set this to the number of sprite
        pixels equivalent to a single 3D world unit to have the system
        automatically scale sprites to the 3D scene in whic they appear.
        */
        explicit SpriteSystem3D(MessageBus& mb, float pixelsPerUnit = 1.f);

        void process(float) override;

        /*!
        \brief Returns the number of pixels mapped to a world unit
        */
        float getPixelsPerUnit() const { return m_pixelsPerUnit; }

    private:

        float m_pixelsPerUnit;
        Shader m_colouredShader;
        Shader m_texturedShader;

        Material::Data m_colouredMaterial;
        Material::Data m_texturedMaterial;

        std::unique_ptr<MeshBuilder> m_meshBuilder; //needs the polymorphism I'm afraid

        void onEntityAdded(Entity) override;
        void onEntityRemoved(Entity) override;
        Material::Data createMaterial(const Shader&);
    };
}
