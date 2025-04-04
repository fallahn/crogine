/*-----------------------------------------------------------------------

Copyright (c) 2009 Erin Catto http://www.box2d.org
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

#include <crogine/detail/BalancedTree.hpp>

#include <crogine/ecs/components/Transform.hpp>

using namespace cro;
using namespace cro::Detail;

namespace
{
    const float DisplacementMultiplier = 2.f;
}

BalancedTree::BalancedTree(float fattenAmount)
     : m_root       (TreeNode::Null),
    m_nodeCount     (0),
    m_nodeCapacity  (64),
    m_nodes         (m_nodeCapacity),
    m_freeList      (0),
    m_insertionCount(0),
    m_fattenAmount  (fattenAmount)
{
    CRO_ASSERT(fattenAmount > 0, "Must be a positive value");

    for (auto i = 0u; i < m_nodeCapacity - 1; ++i)
    {
        m_nodes[i].next = static_cast<std::int32_t>(i + 1);
        m_nodes[i].height = -1;
    }
    m_nodes.back().next = TreeNode::Null;
    m_nodes.back().height = -1;
}

//public
std::int32_t BalancedTree::addToTree(Entity entity, Box bounds)
{
    auto treeID = allocateNode();

    const auto& tx = entity.getComponent<Transform>();

    bounds += tx.getOrigin();
    bounds = tx.getWorldTransform() * bounds;

    //fatten AABB
    bounds[0] -= m_fattenAmount;
    bounds[1] += m_fattenAmount;


    m_nodes[treeID].fatBounds = bounds;
    m_nodes[treeID].entity = entity;
    m_nodes[treeID].height = 0;

    insertLeaf(treeID);

    return treeID;
}

void BalancedTree::removeFromTree(std::int32_t treeID)
{
    CRO_ASSERT(treeID > -1 && treeID < m_nodeCapacity, "Invalid tree id");
    CRO_ASSERT(m_nodes[treeID].isLeaf(), "Not a leaf node!");

    removeLeaf(treeID);
    freeNode(treeID);
}

//private
bool BalancedTree::moveNode(std::int32_t treeID, Box worldArea, glm::vec3 displacement)
{
    CRO_ASSERT(treeID > -1 && treeID < m_nodeCapacity, "Invalid tree id");
    CRO_ASSERT(m_nodes[treeID].isLeaf(), "Not a leaf node!");

    if (m_nodes[treeID].fatBounds.contains(worldArea))
    {
        return false;
    }

    removeLeaf(treeID);

    //expand the new aabb and reinsert in tree
    worldArea[0] -= m_fattenAmount;
    worldArea[1] += m_fattenAmount;


    //displacment prediction
    displacement *= DisplacementMultiplier;

    if (displacement.x < 0) //not really understanding the original here, so quite possibly creating a bug!
    {
        worldArea[0].x += displacement.x;
    }
    else
    {
        worldArea[1].x += displacement.x;
    }

    if (displacement.y < 0)
    {
        worldArea[0].y += displacement.y;
    }
    else
    {
        worldArea[1].y += displacement.y;
    }

    if (displacement.z < 0)
    {
        worldArea[0].z += displacement.z;
    }
    else
    {
        worldArea[1].z += displacement.z;
    }

    //reinsert
    m_nodes[treeID].fatBounds = worldArea;
    insertLeaf(treeID);

    return true;
}

Box BalancedTree::getFatAABB(std::int32_t treeID) const
{
    CRO_ASSERT(treeID > -1 && treeID < m_nodeCapacity, "Invalid tree id");
    return m_nodes[treeID].fatBounds;
}

std::int32_t BalancedTree::getMaxBalance() const
{
    std::int32_t maxBalance = 0;

    for (auto i = 0u; i < m_nodeCapacity; ++i)
    {
        const auto& node = m_nodes[i];
        if (node.height <= 1)
        {
            continue;
        }

        CRO_ASSERT(!node.isLeaf(), "We shouldn't be at the end!");

        auto balance = std::abs(m_nodes[node.childB].height - m_nodes[node.childA].height);
        maxBalance = std::max(balance, maxBalance);
    }
    return maxBalance;
}

float BalancedTree::getAreaRatio() const
{
    if (m_root == TreeNode::Null)
    {
        return 0.f;
    }

    const auto& rootNode = m_nodes[m_root];
    auto rootArea = rootNode.fatBounds.getPerimeter();

    float totalArea = 0.f;
    for (auto i = 0u; i < m_nodeCapacity; ++i)
    {
        const auto& node = m_nodes[i];
        if (node.height > -1) //not a free node
        {
            totalArea += node.fatBounds.getPerimeter();
        }
    }

    return totalArea / rootArea;
}

std::int32_t BalancedTree::allocateNode()
{
    //grow the node list if full
    if (m_freeList == TreeNode::Null)
    {
        CRO_ASSERT(m_nodeCount == m_nodeCapacity, "List not actually full?");

        m_nodeCapacity *= 2;
        m_nodes.resize(m_nodeCapacity);

        //LOG("Resized tree capacity to " + std::to_string(m_nodeCapacity), xy::Logger::Type::Info);

        //update the linked list for the new capacity
        for (auto i = m_nodeCount; i < m_nodeCapacity - 1; ++i)
        {
            m_nodes[i].next = static_cast<std::int32_t>(i + 1);
            m_nodes[i].height = -1;
        }
        m_nodes.back().next = TreeNode::Null;
        m_nodes.back().height = -1;
        m_freeList = static_cast<std::int32_t>(m_nodeCount); //we doubled therefore this is how many are free
    }

    //set next node as free - remember we're recycling these so can't rely on default values
    auto treeID = m_freeList;
    m_freeList = m_nodes[treeID].next;

    m_nodes[treeID].parent = TreeNode::Null;
    m_nodes[treeID].childA = TreeNode::Null;
    m_nodes[treeID].childB = TreeNode::Null;
    m_nodes[treeID].height = 0;
    m_nodes[treeID].entity = {};
    m_nodeCount++;

    return treeID;
}

void BalancedTree::freeNode(std::int32_t treeID)
{
    CRO_ASSERT(treeID > TreeNode::Null && treeID < m_nodeCapacity, "Invalid tree id");
    CRO_ASSERT(m_nodeCount > 0, "No nodes exist to free");

    m_nodes[treeID].next = m_freeList;
    m_nodes[treeID].height = -1;
    m_freeList = treeID;
    m_nodeCount--;
}

void BalancedTree::insertLeaf(std::int32_t treeID)
{
    m_insertionCount++;

    if (m_root == TreeNode::Null)
    {
        m_root = treeID;
        m_nodes[m_root].parent = TreeNode::Null;
        return;
    }

    //walk the tree for a suitable leaf position
    auto leafBounds = m_nodes[treeID].fatBounds;
    auto index = m_root;

    while (!m_nodes[index].isLeaf())
    {
        auto childA = m_nodes[index].childA;
        auto childB = m_nodes[index].childB;

        float perimeter = m_nodes[index].fatBounds.getPerimeter();

        auto combinedAABB = Box::merge(m_nodes[index].fatBounds, leafBounds);
        auto combinedPerimeter = combinedAABB.getPerimeter();

        //cost of creating a new node / parent for the leaf
        float cost = 2.f * combinedPerimeter;

        //minimum cost for pushing the leaf down the tree
        float inheritedCost = 2.f * (combinedPerimeter - perimeter);

        //cost of descending to childA
        float costA = 0.f;
        if (m_nodes[childA].isLeaf())
        {
            auto bounds = Box::merge(leafBounds, m_nodes[childA].fatBounds);
            costA = bounds.getPerimeter() + inheritedCost;
        }
        else
        {
            auto bounds = Box::merge(leafBounds, m_nodes[childA].fatBounds);
            auto oldPerimeter = m_nodes[childA].fatBounds.getPerimeter();
            auto newPerimenter = bounds.getPerimeter();
            costA = (newPerimenter - oldPerimeter) + inheritedCost;
        }


        //cost of descending to childB
        float costB = 0.f;
        if (m_nodes[childB].isLeaf())
        {
            auto bounds = Box::merge(leafBounds, m_nodes[childB].fatBounds);
            costB = bounds.getPerimeter() + inheritedCost;
        }
        else
        {
            auto bounds = Box::merge(leafBounds, m_nodes[childB].fatBounds);
            auto oldPerimeter = m_nodes[childB].fatBounds.getPerimeter();
            auto newPerimenter = bounds.getPerimeter();
            costB = (newPerimenter - oldPerimeter) + inheritedCost;
        }

        //and descend according to least cost
        if (cost < costA && cost < costB)
        {
            break; //we arrived!
        }

        if (costA < costB)
        {
            index = childA;
        }
        else
        {
            index = childB;
        }
    }

    auto sibling = index;

    //create new parent
    auto oldParent = m_nodes[sibling].parent;
    auto newParent = allocateNode();
    m_nodes[newParent].parent = oldParent;
    m_nodes[newParent].entity = {};
    m_nodes[newParent].fatBounds = Box::merge(leafBounds, m_nodes[sibling].fatBounds);
    m_nodes[newParent].height = m_nodes[sibling].height + 1;

    if (oldParent != TreeNode::Null)
    {
        //we're not attaching to the root
        if (m_nodes[oldParent].childA == sibling)
        {
            m_nodes[oldParent].childA = newParent;
        }
        else
        {
            m_nodes[oldParent].childB = newParent;
        }

        m_nodes[newParent].childA = sibling;
        m_nodes[newParent].childB = treeID;
        m_nodes[sibling].parent = newParent;
        m_nodes[treeID].parent = newParent;
    }
    else
    {
        m_nodes[newParent].childA = sibling;
        m_nodes[newParent].childB = treeID;
        m_nodes[sibling].parent = newParent;
        m_nodes[treeID].parent = newParent;
        m_root = newParent;
    }

    //walk back up the tree updating heights and bounds
    index = m_nodes[treeID].parent;
    while (index != TreeNode::Null)
    {
        index = balance(index);

        auto childA = m_nodes[index].childA;
        auto childB = m_nodes[index].childB;

        CRO_ASSERT(childA != TreeNode::Null, "Can't be null");
        CRO_ASSERT(childB != TreeNode::Null, "Can't be null");

        m_nodes[index].height = std::max(m_nodes[childA].height, m_nodes[childB].height);
        m_nodes[index].fatBounds = Box::merge(m_nodes[childA].fatBounds, m_nodes[childB].fatBounds);

        index = m_nodes[index].parent;
    }
}

void BalancedTree::removeLeaf(std::int32_t treeID)
{
    if (treeID == m_root)
    {
        m_root = TreeNode::Null;
        return;
    }

    auto parent = m_nodes[treeID].parent;
    auto grandparent = m_nodes[parent].parent;
    auto sibling = TreeNode::Null;

    if (m_nodes[parent].childA == treeID)
    {
        sibling = m_nodes[parent].childB;
    }
    else
    {
        sibling = m_nodes[parent].childA;
    }

    if (grandparent != TreeNode::Null)
    {
        if (m_nodes[grandparent].childA == parent)
        {
            m_nodes[grandparent].childA = sibling;
        }
        else
        {
            m_nodes[grandparent].childB = sibling;
        }
        m_nodes[sibling].parent = grandparent;
        freeNode(parent);

        //update bounds
        auto index = grandparent;
        while (index != TreeNode::Null)
        {
            index = balance(index);

            auto childA = m_nodes[index].childA;
            auto childB = m_nodes[index].childB;

            m_nodes[index].fatBounds = Box::merge(m_nodes[childA].fatBounds, m_nodes[childB].fatBounds);
            m_nodes[index].height = std::max(m_nodes[childA].height, m_nodes[childB].height) + 1;

            index = m_nodes[index].parent;
        }
    }
    else
    {
        m_root = sibling;
        m_nodes[sibling].parent = TreeNode::Null;
        freeNode(parent);
    }
}

std::int32_t BalancedTree::balance(std::int32_t iA)
{
    //performs left or right rotation if a is imbalanced
    //returns the new root

    CRO_ASSERT(iA != TreeNode::Null, "Invalid node");

    auto& A = m_nodes[iA];
    if (A.isLeaf() || A.height < 2)
    {
        return iA;
    }

    auto iB = A.childA;
    auto iC = A.childB;

    CRO_ASSERT(iB > -1 && iB < m_nodeCapacity, "Invalid node");
    CRO_ASSERT(iC > -1 && iC < m_nodeCapacity, "Invalid node");

    auto& B = m_nodes[iB];
    auto& C = m_nodes[iC];

    auto balance = C.height - B.height;

    //rotate C up
    if (balance > 1)
    {
        auto iF = C.childA;
        auto iG = C.childB;
        auto& F = m_nodes[iF];
        auto& G = m_nodes[iG];

        CRO_ASSERT(iF > -1 && iF < m_nodeCapacity, "Invalid node");
        CRO_ASSERT(iG > -1 && iG < m_nodeCapacity, "Invalid node");

        //swap A and C
        C.childA = iA;
        C.parent = A.parent;
        A.parent = iC;

        //A's old parent should point to C
        if (C.parent != TreeNode::Null)
        {
            if (m_nodes[C.parent].childA == iA)
            {
                m_nodes[C.parent].childA = iC;
            }
            else
            {
                CRO_ASSERT(m_nodes[C.parent].childB == iA, "");
                m_nodes[C.parent].childB = iC;
            }
        }
        else
        {
            m_root = iC;
        }

        //rotate
        if (F.height > G.height)
        {
            C.childB = iF;
            A.childB = iG;
            G.parent = iA;
            A.fatBounds = Box::merge(B.fatBounds, G.fatBounds);
            C.fatBounds = Box::merge(A.fatBounds, F.fatBounds);

            A.height = std::max(B.height, G.height) + 1;
            C.height = std::max(A.height, F.height) + 1;
        }
        else
        {
            C.childB = iG;
            A.childB = iF;
            F.parent = iA;
            A.fatBounds = Box::merge(B.fatBounds, F.fatBounds);
            C.fatBounds = Box::merge(A.fatBounds, G.fatBounds);

            A.height = std::max(B.height, F.height) + 1;
            C.height = std::max(A.height, G.height) + 1;
        }

        return iC;
    }

    //rotate B up
    if (balance < -1)
    {
        auto iD = B.childA;
        auto iE = B.childB;
        auto& D = m_nodes[iD];
        auto& E = m_nodes[iE];
        CRO_ASSERT(iD > -1 && iD < m_nodeCapacity, "Invalid node");
        CRO_ASSERT(iE > -1 && iE < m_nodeCapacity, "Invalid node");

        //swap A and B
        B.childA = iA;
        B.parent = A.parent;
        A.parent = iB;

        //A's old parent should point to B
        if (B.parent != TreeNode::Null)
        {
            if (m_nodes[B.parent].childA == iA)
            {
                m_nodes[B.parent].childA = iB;
            }
            else
            {
                CRO_ASSERT(m_nodes[B.parent].childB == iA, "");
                m_nodes[B.parent].childB = iB;
            }
        }
        else
        {
            m_root = iB;
        }

        //rotate
        if (D.height > E.height)
        {
            B.childB = iD;
            A.childA = iE;
            E.parent = iA;
            A.fatBounds = Box::merge(C.fatBounds, E.fatBounds);
            B.fatBounds = Box::merge(A.fatBounds, D.fatBounds);

            A.height = std::max(C.height, E.height) + 1;
            B.height = std::max(A.height, D.height) + 1;
        }
        else
        {
            B.childB = iE;
            A.childA = iD;
            D.parent = iA;
            A.fatBounds = Box::merge(C.fatBounds, D.fatBounds);
            B.fatBounds = Box::merge(A.fatBounds, E.fatBounds);

            A.height = std::max(C.height, D.height) + 1;
            B.height = std::max(A.height, E.height) + 1;
        }

        return iB;
    }
    return iA;
}

std::int32_t BalancedTree::computeHeight() const
{
    return computeHeight(m_root);
}

std::int32_t BalancedTree::computeHeight(std::int32_t treeID) const
{
    CRO_ASSERT(treeID > TreeNode::Null && treeID < m_nodeCapacity, "Invalid tree id");

    if (m_nodes[treeID].isLeaf())
    {
        return 0;
    }

    auto heightA = computeHeight(m_nodes[treeID].childA);
    auto heightB = computeHeight(m_nodes[treeID].childB);

    return std::max(heightA, heightB) + 1;
}