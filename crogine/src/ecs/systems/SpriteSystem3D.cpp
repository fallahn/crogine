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

#include <crogine/ecs/systems/SpriteSystem3D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/graphics/MeshResource.hpp>

#include "../../graphics/shaders/Sprite.hpp"

using namespace cro;

SpriteSystem3D::SpriteSystem3D(MessageBus& mb, MeshResource& mr)
    : System        (mb, typeid(SpriteSystem3D)),
    m_meshResource  (mr)
{
    requireComponent<Sprite>();
    requireComponent<Model>();

    m_colouredShader.loadFromString(Shaders::Sprite::Vertex, Shaders::Sprite::Coloured);
    m_texturedShader.loadFromString(Shaders::Sprite::Vertex, Shaders::Sprite::Textured, "#define TEXTURED\n");

    //TODO create a dynamic mesh builder with correct flags
}

//public
void SpriteSystem3D::process(float)
{
    //check sprites for dirty flags and update geom as necessary.
    //remember to switch shaders if a texture is added or removed.
}

//private
void SpriteSystem3D::onEntityAdded(Entity entity)
{
    //init the Model component with the mesh and corresponding shader.
}