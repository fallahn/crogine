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

#include <crogine/detail/QuadTree.hpp>
#include <crogine/detail/Assert.hpp>

#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Transform.hpp>

namespace
{
    struct Direction final
    {
        enum
        {
            None = -1,
            NW, NE, SW, SE,

            Count
        };
    };
}

using namespace cro;
using namespace cro::Detail;

QuadTree::QuadTree(FloatRect rootArea)
    : m_rootArea    (rootArea),
    m_rootNode      (std::make_unique<QuadTree::Node>())
{

}

//public
void QuadTree::add(Entity member)
{
    CRO_ASSERT(member.hasComponent<Drawable2D>(), "");
    CRO_ASSERT(member.hasComponent<Transform>(), "");
    add(m_rootNode.get(), 0, m_rootArea, member);
}

void QuadTree::remove(Entity member)
{
    removeFromNode(m_rootNode.get(), m_rootArea, member);
}

std::vector<Entity> QuadTree::query(FloatRect queryArea) const
{
    std::vector<Entity> retVal;
    query(m_rootNode.get(), m_rootArea, queryArea, retVal);

    return retVal;
}

std::vector<std::pair<Entity, Entity>> QuadTree::getIntersecting() const
{
    std::vector<std::pair<Entity, Entity>> retVal;
    findIntersections(m_rootNode.get(), retVal);

    return retVal;
}


//private
bool QuadTree::isLeaf(const Node* node) const
{
    return node->children[0] == nullptr;
}

FloatRect QuadTree::calcBox(FloatRect aabb, std::int32_t direction) const
{
    glm::vec2 origin(aabb.left, aabb.bottom);
    glm::vec2 childSize(aabb.width / 2.f, aabb.height / 2.f);

    switch (direction)
    {
    default: 
        CRO_ASSERT(false, "invalid direction");
        return {};
    case Direction::NE:
        return { origin + childSize, childSize };
    case Direction::NW:
        return { glm::vec2(origin.x, origin.y + childSize.y), childSize };
    case Direction::SE:
        return { glm::vec2(origin.x + childSize.x, origin.y), childSize };
    case Direction::SW:
        return { origin, childSize };
    }
}

std::int32_t QuadTree::getQuadrant(FloatRect nodeBounds, FloatRect aabb) const
{
    glm::vec2 centre(nodeBounds.left + (nodeBounds.width / 2.f), nodeBounds.bottom + (nodeBounds.height / 2.f));
    if (aabb.left + aabb.width < centre.x)
    {
        if (aabb.bottom > centre.y)
        {
            return Direction::NW;
        }

        if (aabb.bottom + aabb.height <= centre.y)
        {
            return Direction::SW;
        }

        //not completely contained
        return Direction::None;
    }

    if (aabb.left >= centre.x)
    {
        if (aabb.bottom > centre.y)
        {
            return Direction::NE;
        }

        if (aabb.bottom + aabb.height <= centre.y)
        {
            return Direction::SE;
        }

        //not completely contained
        return Direction::None;
    }

    return Direction::None;
}

void QuadTree::add(Node* node, std::size_t depth, FloatRect area, Entity member)
{
    CRO_ASSERT(node, "");
    CRO_ASSERT(area.contains(getAABB(member)), "");

    if (isLeaf(node))
    {
        if (depth >= MaxDepth || node->members.size() < Threshold)
        {
            node->members.push_back(member);
        }
        else
        {
            //no room at the inn
            split(node, area);
            add(node, depth, area, member);
        }
    }
    else
    {
        //place in child if fully contained, else add to
        //this node
        auto dir = getQuadrant(area, getAABB(member));
        if (dir != Direction::None)
        {
            add(node->children[dir].get(), depth + 1, calcBox(area, dir), member);
        }
        else
        {
            node->members.push_back(member);
        }
    }
}

void QuadTree::split(Node* node, FloatRect area)
{
    CRO_ASSERT(node, "");
    CRO_ASSERT(isLeaf(node), "");

    for (auto& c : node->children)
    {
        c = std::make_unique<Node>();
    }

    std::vector<Entity> newMembers;
    for (auto m : node->members)
    {
        auto dir = getQuadrant(area, getAABB(m));
        if (dir != Direction::None)
        {
            node->children[dir]->members.push_back(m);
        }
        else
        {
            newMembers.push_back(m);
        }
    }
    node->members.swap(newMembers);
}

bool QuadTree::removeFromNode(Node* node, FloatRect area, Entity member)
{
    CRO_ASSERT(node, "");
    CRO_ASSERT(area.contains(getAABB(member)), "");

    if (isLeaf(node))
    {
        removeMember(node, member);
        return true;
    }

    auto dir = getQuadrant(area, getAABB(member));
    if (dir != Direction::None)
    {
        if (removeFromNode(node->children[dir].get(), calcBox(area, dir), member))
        {
            return tryMerge(node);
        }
    }
    removeMember(node, member);
    return false;
}

void QuadTree::removeMember(Node* node, Entity member)
{
    auto result = std::find_if(node->members.begin(), node->members.end(),
        [member](Entity e)
        {
            return member == e;
        });

    //should always exist when searching...
    CRO_ASSERT(result != node->members.end(), "");
    *result = node->members.back();
    node->members.pop_back();
}

bool QuadTree::tryMerge(Node* node)
{
    CRO_ASSERT(node, "");
    CRO_ASSERT(!isLeaf(node), "");

    auto memberCount = node->members.size();
    for (const auto& c : node->children)
    {
        if (isLeaf(c.get()))
        {
            return false;
        }
        memberCount += c->members.size();
    }

    if (memberCount < Threshold)
    {
        for (auto& c : node->children)
        {
            for (auto m : c->members)
            {
                node->members.push_back(m);
            }
            c.reset();
        }
        return true;
    }

    return false;
}

void QuadTree::query(Node* node, FloatRect area, FloatRect queryArea, std::vector<Entity>& dst) const
{
    CRO_ASSERT(node, "");
    CRO_ASSERT(queryArea.intersects(area), "");

    for (auto m : node->members)
    {
        if (queryArea.intersects(getAABB(m)))
        {
            dst.push_back(m);
        }
    }

    if (!isLeaf(node))
    {
        for (auto i = 0; i < Direction::Count; ++i)
        {
            auto childArea = calcBox(area, i);
            if (childArea.intersects(queryArea))
            {
                query(node->children[i].get(), childArea, queryArea, dst);
            }
        }
    }
}

void QuadTree::findIntersections(Node* node, std::vector<std::pair<Entity, Entity>>& dst) const
{
    for (auto i = 0u; i < node->members.size(); ++i)
    {
        for (auto j = 0u; j < i; ++j)
        {
            if (getAABB(node->members[i]).intersects(getAABB(node->members[j])))
            {
                dst.emplace_back(node->members[i], node->members[j]);
            }
        }
    }

    if (!isLeaf(node))
    {
        for (const auto& c : node->children)
        {
            for (auto m : c->members)
            {
                findChildIntersections(c.get(), m, dst);
            }

            findIntersections(c.get(), dst);
        }
    }
}

void QuadTree::findChildIntersections(Node* node, Entity member, std::vector<std::pair<Entity, Entity>>& dst) const
{
    auto memberBounds = getAABB(member);
    for (auto m : node->members)
    {
        if (memberBounds.intersects(getAABB(m)))
        {
            dst.emplace_back(member, m);
        }
    }

    if (!isLeaf(node))
    {
        for (const auto& c : node->children)
        {
            findChildIntersections(c.get(), member, dst);
        }
    }
}

FloatRect QuadTree::getAABB(Entity entity) const
{
    CRO_ASSERT(entity.hasComponent<Transform>(), "");
    CRO_ASSERT(entity.hasComponent<Drawable2D>(), "");

    return entity.getComponent<Transform>().getWorldTransform() * entity.getComponent<Drawable2D>().getLocalBounds();
}