/*-----------------------------------------------------------------------

Matt Marchant 2022
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
#include <crogine/ecs/Entity.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <memory>

namespace cro::Detail
{
    /*
    Quad tree implementation based on this article
    https://pvigier.github.io/2019/08/04/quadtree-collision-detection.html
    
    Quad trees are by their nature 2 dimensional, and work best with
    mostly static geometry. This implementation is designed to be used
    with enities in a Scene which make up a menu or UI for example,
    where the members are mostly static and can be quickly culled when
    rendering. For more dynamic scenes consider using the DynamicTreeSystem.
    \see DynamicTreeSystem
    */

    class CRO_EXPORT_API QuadTree final
    {
    public:
        /*!
        \brief Constructor
        \param rootArea The maximum area this QuadTree will cover.
        */
        explicit QuadTree(FloatRect rootArea);

        /*!
        \brief Add an entity to the quad tree.
        Entities must have a Drawable2D added to them,
        and a Transform component from which to derive
        an AABB for the entity.
        \param member Entity to add to the tree
        */
        void add(Entity member);

        /*!
        \brief Remove a member entity from the quad tree
        This needs to be done if an entity is deleted, or
        of an entity is moved, so that it can be reinserted
        in the correct place. Highly dynamic entities should
        consider using a BalancedTree or the DynamicTreeSystem
        \param member Entity to remove from the tree
        */
        void remove(Entity member);

        /*!
        \brief Query the tree for potential colliders or
        renderables which appear within the given FloatRect
        \param area Area to query within the tree, in world coords
        \returns vector of entities whose AABB are contained within
        or intersect the given query area.
        Useful for returning entities whose position currently
        interects the active render area (ie 2D frustum culling)
        */
        std::vector<Entity> query(FloatRect area) const;

        /*!
        \brief Returns a vector of pairs of entities whose
        AABBs intersect.
        Useful as a broadphase in collision detection
        */
        std::vector<std::pair<Entity, Entity>> getIntersecting() const;

    private:
        static constexpr std::size_t MaxDepth = 8;
        static constexpr std::size_t Threshold = 16;

        FloatRect m_rootArea;
        struct Node final
        {
            std::array<std::unique_ptr<Node>, 4u> children;
            std::vector<Entity> members;
        };
        std::unique_ptr<Node> m_rootNode;

        bool isLeaf(const Node*) const;
        FloatRect calcBox(FloatRect, std::int32_t dir) const;
        std::int32_t getQuadrant(FloatRect nodeBounds, FloatRect entBounds) const;
        void add(Node* target, std::size_t depth, FloatRect aabb, Entity member);
        void split(Node*, FloatRect bounds);
        bool removeFromNode(Node*, FloatRect bounds, Entity member);
        void removeMember(Node*, Entity);
        bool tryMerge(Node*);
        void query(Node*, FloatRect bounds, FloatRect queryBounds, std::vector<Entity>& dst) const;
        void findIntersections(Node*, std::vector<std::pair<Entity, Entity>>& dst) const;
        void findChildIntersections(Node*, Entity parent, std::vector<std::pair<Entity, Entity>>& dst) const;


        FloatRect getAABB(cro::Entity entity) const;
    };
}