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

#include "Collision.hpp"

#include <crogine/detail/Assert.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/graphics/Colour.hpp>

namespace TwoDeeFlags
{
    enum
    {
        Debug = 0x1
    };
}

struct DebugInfo final
{
    std::int32_t nearbyEnts = 0;
    std::int32_t collidingEnts = 0;
    cro::Box bounds;
};

static inline cro::Entity addBoxDebug(cro::Entity parent, cro::Scene& scene, cro::Colour colour = cro::Colour(0.f, 1.f, 0.f))
{
    CRO_ASSERT(parent.hasComponent<CollisionComponent>(), "Requires AABB component");

    const auto& collision = parent.getComponent<CollisionComponent>();
    const std::array colours = { colour, cro::Colour::Black, cro::Colour::Yellow };

    auto entity = scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(parent.getComponent<cro::Transform>().getOrigin());
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    auto& verts = entity.getComponent<cro::Drawable2D>().getVertexData();

    for (auto i = 0u; i < collision.rectCount; ++i)
    {
        const auto& bb = collision.rects[i].bounds;
        verts.emplace_back(glm::vec2(bb.left, bb.bottom), cro::Colour::Transparent);
        verts.emplace_back(glm::vec2(bb.left, bb.bottom), colours[i]);
        verts.emplace_back(glm::vec2(bb.left + bb.width, bb.bottom), colours[i]);
        verts.emplace_back(glm::vec2(bb.left + bb.width, bb.bottom + bb.height), colours[i]);
        verts.emplace_back(glm::vec2(bb.left, bb.bottom + bb.height), colours[i]);
        verts.emplace_back(glm::vec2(bb.left, bb.bottom), colours[i]);
        verts.emplace_back(glm::vec2(bb.left, bb.bottom), cro::Colour::Transparent);
    }
    const auto& bb = collision.sumRect;
    verts.emplace_back(glm::vec2(bb.left, bb.bottom), cro::Colour::Transparent);
    verts.emplace_back(glm::vec2(bb.left, bb.bottom), cro::Colour::Blue);
    verts.emplace_back(glm::vec2(bb.left + bb.width, bb.bottom), cro::Colour::Blue);
    verts.emplace_back(glm::vec2(bb.left + bb.width, bb.bottom + bb.height), cro::Colour::Blue);
    verts.emplace_back(glm::vec2(bb.left, bb.bottom + bb.height), cro::Colour::Blue);
    verts.emplace_back(glm::vec2(bb.left, bb.bottom), cro::Colour::Blue);
    verts.emplace_back(glm::vec2(bb.left, bb.bottom), cro::Colour::Transparent);

    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.getComponent<cro::Drawable2D>().setFilterFlags(TwoDeeFlags::Debug);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&,parent](cro::Entity e, float)
    {
        if (parent.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            scene.destroyEntity(e);
        }
    };

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    return entity;
}
