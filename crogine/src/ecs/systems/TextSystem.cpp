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

#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/graphics/Font.hpp>

#include "../../detail/glad.hpp"

using namespace cro;

TextSystem::TextSystem(MessageBus& mb)
    : System(mb, typeid(TextSystem))
{
    requireComponent<Drawable2D>();
    requireComponent<Text>();
}

void TextSystem::process(float)
{
    auto entities = getEntities();
    for (auto entity : entities)
    {
        auto& drawable = entity.getComponent<Drawable2D>();
        auto& text = entity.getComponent<Text>();

        if (text.m_dirty || text.m_font->pageUpdated())
        {
            text.updateVertices(drawable);
            drawable.setTexture(&text.getFont()->getTexture(text.getCharacterSize()));
            drawable.setPrimitiveType(GL_TRIANGLES);
        }
    }
}