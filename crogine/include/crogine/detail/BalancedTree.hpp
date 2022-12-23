/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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

#include <crogine/ecs/Entity.hpp>
#include <crogine/graphics/BoundingBox.hpp>

#include <vector>
#include <limits>
#include <cstdint>
#include <array>

namespace cro::Detail
{
    //node struct used by the tree
    struct TreeNode final
    {
        static constexpr std::int32_t Null = -1;
        bool isLeaf() const
        {
            return childA == Null;
        }

        //this is in world coordinates
        Box fatBounds;
        Entity entity;

        union
        {
            std::int32_t parent;
            std::int32_t next;
        };

        std::int32_t childA = Null;
        std::int32_t childB = Null;

        //leaf == 0, else Null if free
        std::int32_t height = Null;
    };

    /*!
    \brief Dynamic AABB tree for broadphase queries. Ported from
    xygine https://github.com/fallahn/xygine which is, in turn, based on
    Erin Catto's dynamic tree in Box2D (http://www.box2d.org)
    which is in turn inspired by Nathanael Presson's btDbvt.
    (https://pybullet.org/wordpress/)
    */

    class BalancedTree final
    {
    public:
        explicit BalancedTree(float unitsPerMetre);

        std::int32_t addToTree(Entity, Box bounds);
        void removeFromTree(std::int32_t);

        //moves a proxy with the specified treeID. If the entity
        //has moved outside of the node's fattened AABB then it
        //is removed from the tree and reinserted.
        bool moveNode(std::int32_t, Box, glm::vec3);

        std::int32_t getRoot() const { return m_root; }

        const std::vector<TreeNode>& getNodes() const { return m_nodes; }

    private:
        std::int32_t m_root;

        std::size_t m_nodeCount;
        std::size_t m_nodeCapacity;
        std::vector<TreeNode> m_nodes;

        std::int32_t m_freeList; //must be signed!

        std::size_t m_insertionCount;

        glm::vec3 m_fattenAmount;

        Box getFatAABB(std::int32_t) const;
        std::int32_t getMaxBalance() const;
        float getAreaRatio() const;

        std::int32_t allocateNode();
        void freeNode(std::int32_t);

        void insertLeaf(std::int32_t);
        void removeLeaf(std::int32_t);

        std::int32_t balance(std::int32_t);

        std::int32_t computeHeight() const;
        std::int32_t computeHeight(std::int32_t) const;
    };

    //fixed stack using preallocated memory
    template <typename T, std::size_t SIZE>
    class FixedStack final
    {
    public:

        T pop()
        {
            CRO_ASSERT(m_size != 0, "Stack is empty!");
            m_size--;
            return m_data[m_size];
        }

        void push(T data)
        {
            CRO_ASSERT(m_size < m_data.size(), "Stack is full!");

            m_data[m_size++] = data;
        }

        std::size_t size() const
        {
            return m_size;
        }

    private:
        std::array<T, SIZE> m_data = {};
        std::size_t m_size = 0; //current size / next free index
    };
}
