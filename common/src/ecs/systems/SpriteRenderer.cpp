/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/core/Clock.hpp>

#include "../../glad/glad.h"
#include "../../glad/GLCheck.hpp"

using namespace cro;

SpriteRenderer::SpriteRenderer()
    : System(this)
{
    //load shader

    //get shader attrib map
    //get shader texture uniform loc

    //setup projection
}

SpriteRenderer::~SpriteRenderer()
{
    for (auto& p : m_buffers)
    {
        glCheck(glDeleteBuffers(1, &p.first));
    }
}

//public
void SpriteRenderer::process(Time)
{
    //TODO callbacks for adding entities
    //callback needs to check if VBO exists for new texture and dirty flag it

    //TODO callback for removing entities, reverse of above (IE delete empty VBO)

    //get list of entities, sort by sprite texture

    //TODO check for dirty flag
    //rebatch all sprites with texture, check for resizing

    //if depth sorted set Z to -Y
}

void SpriteRenderer::render()
{
    //TODO enable / disable depth testing as per setting
    //bind shader and attrib arrays
    //foreach vbo/texture bind and draw
}