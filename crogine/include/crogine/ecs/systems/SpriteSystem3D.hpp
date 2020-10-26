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

#include <crogine/graphics/Shader.hpp>

namespace cro
{
    class MeshResource;

    /*!
    \brief 3D Sprite System
    Updates entities which have a Sprite component and Model
    component attached so that the Sprite can be drawn in a 3D scene
    */
    class SpriteSystem3D final : public cro::System
    {
    public:
        /*!
        \brief Constructor
        \param mb A reference to the active MessageBus
        \param mr A reference to the MeshResource used by the
        Scene to which this system was added.
        */
        SpriteSystem3D(MessageBus& mb, MeshResource& mr);

        void process(float) override;

    private:

        Shader m_colouredShader;
        Shader m_texturedShader;

        MeshResource& m_meshResource;
        void onEntityAdded(Entity) override;

    };
}
