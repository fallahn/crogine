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

#include <crogine/detail/BalancedTree.hpp>
#include <crogine/ecs/System.hpp>

#include <memory>

namespace cro
{
    class CRO_EXPORT_API DynamicTreeSystem final : public System
    {
    public:
        /*!
        \brief Constructor
        \param mb A reference to the active MessageBus
        \param unitsPerMetre This should be set to reflect the world scale in which
        this system is used. It affects the 'fatten' amount of the partitions, so
        too large a value will return many unnecessary results, too small will
        return too few. The default value should be fine for 3D scenes, but should
        be adjusted accordingly for 2D scenes where the units are often in pixels.
        */
        explicit DynamicTreeSystem(MessageBus& mb, float unitsPerMetre = 1.f);

        void process(float) override;
        void onEntityAdded(Entity) override;
        void onEntityRemoved(Entity) override;

        /*!
        \brief returns a list of entities whose dynamic tree component bounds
        intersect the given query area.
        \param area Area in world coordinates to query
        \param filter Only entities with DynamicTreeComponents matching
        the given bit flags are returned. Defaults to all flags set.
        */
        std::vector<Entity> query(Box area, std::uint64_t filter = std::numeric_limits<std::uint64_t>::max()) const;

    private:
        Detail::BalancedTree m_tree;
    };
}