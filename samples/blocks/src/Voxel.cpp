/*
World Generation code based on Matthew Hopson's Open Builder
https://github.com/Hopson97/open-builder

MIT License

Copyright (c) 2019 Matthew Hopson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "Voxel.hpp"
#include "Coordinate.hpp"

#include <crogine/detail/Assert.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace vx
{
    DataManager::DataManager()
        : m_commonVoxels(CommonType::Count)
    {
        //let's call these 'built in' types
        //TODO load some sort of config file to get
        //the tile indices
        Data data;

        //adding air first ensures it has the ID of 0
        //unfortunately this is an assumption made in
        //some places where the true ID of a block type
        //is ont accessible, for instance the default value
        //of a chunk.
        data.name = "air";
        data.collidable = false;
        data.type = vx::Type::Gas;
        addVoxel(data);

        data.name = "water";
        data.collidable = true;
        data.type = vx::Type::Liquid;
        data.tileIDs = { 17,17,17,17,17,17 };
        addVoxel(data);

        data.name = "sand";
        data.type = vx::Type::Solid;
        data.tileIDs = { 1,1,1,1,1,1 };
        addVoxel(data);

        data.name = "stone";
        data.tileIDs = { 9,9,9,9,9,9 };
        addVoxel(data);

        data.name = "dirt";
        data.tileIDs = { 0,0,0,0,0,0 };
        addVoxel(data);

        data.name = "grass";
        data.tileIDs[Side::Top] = 16;
        data.tileIDs[Side::Bottom] = 0;
        data.tileIDs[Side::North] = 8;
        data.tileIDs[Side::East] = 8;
        data.tileIDs[Side::South] = 8;
        data.tileIDs[Side::West] = 8;
        addVoxel(data);
        

        m_commonVoxels[CommonType::Sand] = getID("sand");
        m_commonVoxels[CommonType::Stone] = getID("stone");
        m_commonVoxels[CommonType::Air] = getID("air");
        m_commonVoxels[CommonType::Water] = getID("water");
        m_commonVoxels[CommonType::Dirt] = getID("dirt");
        m_commonVoxels[CommonType::Grass] = getID("grass");

        data.name = "sand_grass";
        data.tileIDs = { 7,7,7,7,7,7 };
        data.style = vx::MeshStyle::Cross;
        data.collidable = false;
        data.type = Type::Detail;
        addVoxel(data);

        data.name = "short_grass01";
        data.tileIDs[0] = 15;
        addVoxel(data);

        data.name = "short_grass02";
        data.tileIDs[0] = 23;
        addVoxel(data);

        //return this data if we try accessing a voxel outside of the map
        m_outOfBoundsData.collidable = false;
        m_outOfBoundsData.id = CommonType::OutOfBounds;
        m_outOfBoundsData.style = MeshStyle::None;
        m_outOfBoundsData.type = Type::Gas;
    }

    std::uint8_t DataManager::addVoxel(const Data& voxel)
    {
        CRO_ASSERT(!voxel.name.empty(), "voxel must be named!");
        CRO_ASSERT(m_voxels.size() < std::numeric_limits<std::uint8_t>::max() - 1, "Too many voxel types");
        m_voxelMap.emplace(voxel.name, static_cast<std::uint8_t>(m_voxels.size()));

        auto& data = m_voxels.emplace_back(voxel);
        data.id = static_cast<std::uint8_t>(m_voxels.size() - 1);
        return data.id;
    }

    const Data& DataManager::getVoxel(std::uint8_t id) const
    {
        if (id == OutOfBounds)
        {
            return m_outOfBoundsData;
        }

        return m_voxels[id];
    }

    const Data& DataManager::getVoxel(const std::string& name) const
    {
        return m_voxels[getID(name)];
    }

    std::uint8_t DataManager::getID(CommonType type) const
    {
        return m_commonVoxels[type];
    }

    std::uint8_t DataManager::getID(const std::string& name) const
    {
        return m_voxelMap.at(name);
    }

    const std::vector<Data>& DataManager::getData() const
    {
        return m_voxels;
    }

    //see https://github.com/francisengelmann/fast_voxel_traversal/blob/master/main.cpp (MIT license)
    std::vector<glm::ivec3> intersectedVoxel(glm::vec3 start, glm::vec3 direction, float range)
    {
        direction = glm::normalize(direction);
        auto end = start + (direction * range);
        auto startVoxel = toVoxelPosition(start);

        std::int32_t stepX = (direction.x > 0) ? 1 : (direction.x < 0) ? -1 : 0;
        std::int32_t stepY = (direction.y > 0) ? 1 : (direction.y < 0) ? -1 : 0;
        std::int32_t stepZ = (direction.z > 0) ? 1 : (direction.z < 0) ? -1 : 0;

        static constexpr float MaxFloat = std::numeric_limits<float>::max();
        float deltaX = (stepX != 0) ? std::min(stepX / (end.x - start.x), MaxFloat) : MaxFloat;
        float deltaY = (stepY != 0) ? std::min(stepY / (end.y - start.y), MaxFloat) : MaxFloat;
        float deltaZ = (stepZ != 0) ? std::min(stepZ / (end.z - start.z), MaxFloat) : MaxFloat;

        float maxX = (stepX > 0) ? deltaX * (1.f - start.x + startVoxel.x) : deltaX * (start.x - startVoxel.x);
        float maxY = (stepY > 0) ? deltaY * (1.f - start.y + startVoxel.y) : deltaY * (start.y - startVoxel.y);
        float maxZ = (stepZ > 0) ? deltaZ * (1.f - start.z + startVoxel.z) : deltaZ * (start.z - startVoxel.z);

        auto currVoxel = startVoxel;
        std::vector<glm::ivec3> retVal;
        retVal.push_back(startVoxel);

        while (retVal.size() < range * 3)
        {
            if (maxX < maxY)
            {
                if (maxX < maxZ)
                {
                    currVoxel.x += stepX;
                    maxX += deltaX;
                }
                else
                {
                    currVoxel.z += stepZ;
                    maxZ += deltaZ;
                }
            }
            else
            {
                if (maxY < maxZ)
                {
                    currVoxel.y += stepY;
                    maxY += deltaY;
                }
                else
                {
                    currVoxel.z += stepZ;
                    maxZ += deltaZ;
                }
            }

            if (maxX > 1 && maxY > 1 && maxZ > 1)
            {
                break;
            }
            retVal.push_back(currVoxel);
        }
        return retVal;
    }
}

