/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <crogine/detail/Assert.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

#include <crogine/graphics/Colour.hpp>

static inline cro::Entity addBoxDebug(cro::Entity parent, cro::Scene& scene, cro::Colour colour = cro::Colour(0.f, 1.f, 0.f))
{
    CRO_ASSERT(parent.hasComponent<cro::DynamicTreeComponent>(), "Requires AABB component");

    auto bb = parent.getComponent<cro::DynamicTreeComponent>().getArea();

    auto entity = scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(parent.getComponent<cro::Transform>().getOrigin());
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    entity.getComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(bb[0].x, bb[0].y), colour),
        cro::Vertex2D(glm::vec2(bb[0].x, bb[1].y), colour),
        cro::Vertex2D(glm::vec2(bb[1].x, bb[1].y), colour),
        cro::Vertex2D(glm::vec2(bb[1].x, bb[0].y), colour),
        cro::Vertex2D(glm::vec2(bb[0].x, bb[0].y), colour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    return entity;
}
