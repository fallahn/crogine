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

#include "../../detail/GLCheck.hpp"

#include <crogine/ecs/systems/ReflectionMapRenderer.hpp>

#include <crogine/ecs/components/Model.hpp>

using namespace cro;

ReflectionMapRenderer::ReflectionMapRenderer(MessageBus& mb, std::uint32_t mapSize)
    : System(mb, typeid(ReflectionMapRenderer))
{
    requireComponent<Model>();

    if (!m_reflectionMap.create(mapSize, mapSize))
    {
        LogE << "Failed creating Reflection Map buffer." << std::endl;
    }

    if (!m_refractionMap.create(mapSize, mapSize))
    {
        LogE << "Failed creating Refraction Map buffer." << std::endl;
    }
}

//public
void ReflectionMapRenderer::updateDrawList(Entity camera)
{
    //TODO create the reflection camera / viewport based on the
    //settings of the camera passed in. Then store the resulting
    //viewProjection matrix in the camera's reflection properties
    //this is necessary as there may be multiple cameras in the scene.

    //reverse the x/z rotations y is the same. vertical distance is negative
    //of that of the camera above the clip plane, times 2

    //save the draw list in the camera for the visible reflected models.
    //we'll rely on the model renderer to keep the visible list of
    //models up to date for the refraction pass.
}

void ReflectionMapRenderer::render(Entity camera, const RenderTarget&)
{
    //enable clipping in OpenGL
    glCheck(glEnable(GL_CLIP_DISTANCE0));

    //render all models in the camera's reflection list using the reflection VP
    m_reflectionMap.clear(cro::Colour::Red());
    m_reflectionMap.display();

    //flip the clip plane.


    //render all models in the camera's regular list using the regular matrices
    //but the appropriate viewport for the buffer. Must remember to hid the water plane
    m_refractionMap.clear(cro::Colour::Blue());
    m_refractionMap.display();

    //disable clipping again
    glCheck(glDisable(GL_CLIP_DISTANCE0));
}