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

#ifndef CRO_SCENE_NODE_HPP_
#define CRO_SCENE_NODE_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>

#include <array>

namespace cro
{
    class Entity;
    /*!
    \brief Lists the parent and children entity IDs of a node in a scene graph
    */
    class CRO_EXPORT_API SceneNode
    {
    public:
        enum
        {
            MaxChildren = 16
        };
        
        SceneNode();

        /*!
        \brief Sets the parent entity if this node in the scene graph.
        Parents must have both a Transform and SceneNode component
        */
        void setParent(Entity);

        /*!
        \brief Removes the parent entity if it exists so that this
        and all child nodes are orphaned
        */
        void removeParent();

        /*!
        \brief Sets the child node at the given index.
        \param id ID of the entity to add as a child.
        No more than MaxChildren may be added to any one node
        */
        void addChild(uint32 id);

        /*!
        \brief Removes a child with the given entity ID if it exists.
        */
        void removeChild(uint32 id);

    private:
        int32 m_parent;
        int32 m_id;
        std::array<int32, MaxChildren> m_children = {-1};
        bool m_needsUpdate;

        friend class SceneGraph;
    };
}

#endif //CRO_SCENE_NODE_HPP_
