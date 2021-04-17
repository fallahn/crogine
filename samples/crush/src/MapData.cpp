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

#include "MapData.hpp"
#include "CommonConsts.hpp"

#include <tmxlite/Map.hpp>
#include <tmxlite/ObjectGroup.hpp>

namespace
{
    constexpr float MinGroundThickness = 2.f;
}

MapData::MapData()
{

}

//public

bool MapData::loadFromFile(const std::string& path, bool binary)
{
    m_collisionRects[0].clear();
    m_collisionRects[1].clear();

    m_teleportRects[0].clear();
    m_teleportRects[1].clear();

    m_crateSpawns[0].clear();
    m_crateSpawns[1].clear();

    m_playerSpawns.clear();

    if (!binary)
    {
        tmx::Map map;
        if (map.load(path))
        {
            const float mapWidth = static_cast<float>(map.getTileCount().x * map.getTileSize().x);
            const float mapHeight = static_cast<float>(map.getTileCount().y * map.getTileSize().y);

            const auto& layers = map.getLayers();
            for (const auto& layer : layers)
            {
                if (layer->getType() == tmx::Layer::Type::Object)
                {
                    const auto& name = layer->getName();

                    if (name == "collision"
                        && m_collisionRects[0].empty())
                    {
                        const auto& rects = layer->getLayerAs<tmx::ObjectGroup>().getObjects();
                        for (const auto& rect : rects)
                        {
                            if (rect.getShape() == tmx::Object::Shape::Rectangle)
                            {
                                auto bounds = rect.getAABB();
                                cro::FloatRect collisionBounds = { bounds.left, mapHeight - (bounds.top + bounds.height), bounds.width, bounds.height };
                                collisionBounds.left -= mapWidth / 2.f;
                                collisionBounds.left /= ConstVal::MapUnits;
                                collisionBounds.bottom /= ConstVal::MapUnits;
                                collisionBounds.width /= ConstVal::MapUnits;
                                collisionBounds.height /= ConstVal::MapUnits;

                                auto type = rect.getType();
                                if (type == "Collision")
                                {
                                    if (collisionBounds.bottom <= 0)
                                    {
                                        collisionBounds.bottom -= MinGroundThickness;
                                        collisionBounds.height += MinGroundThickness;
                                    }

                                    m_collisionRects[0].push_back(collisionBounds);
                                }
                                else if (type == "Teleport")
                                {
                                    m_teleportRects[0].push_back(collisionBounds);
                                }
                                else if (type == "Crate")
                                {
                                    m_crateSpawns[0].emplace_back(collisionBounds.left + (collisionBounds.width / 2.f), collisionBounds.bottom + (collisionBounds.height / 2.f));
                                }
                            }
                        }
                    }
                    else if (name == "players"
                        && m_playerSpawns.empty())
                    {
                        const auto& spawns = layer->getLayerAs<tmx::ObjectGroup>().getObjects();
                        for (const auto& spawn : spawns)
                        {
                            auto bounds = spawn.getAABB();
                            bounds.left -= mapWidth / 2.f;

                            auto& position = m_playerSpawns.emplace_back(bounds.left + (bounds.width / 2.f), mapHeight - (bounds.top + bounds.height));
                            position /= ConstVal::MapUnits;
                        }
                    }
                }
            }

#ifdef CRO_DEBUG_
            if (m_collisionRects[0].empty())
            {
                LogE << "No collision data was loaded from the map file" << std::endl;
            }
            if (m_playerSpawns.size() != 4)
            {
                LogE << "Player spawn points was " << m_playerSpawns.size() << std::endl;
            }

            //dupe the rects just to test other layer building
            m_collisionRects[1] = m_collisionRects[0];
            m_teleportRects[1] = m_teleportRects[0];
            m_crateSpawns[1] = m_crateSpawns[0];
#endif

            return (!m_collisionRects[0].empty() && !m_teleportRects.empty() && m_playerSpawns.size() == 4);
        }
    }
    else
    {
        //TODO
    }


    return false;
}