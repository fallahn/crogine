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

#include <crogine/detail/Assert.hpp>

namespace vx
{
    DataManager::DataManager()
        : m_commonVoxels(CommonType::Count)
    {
        //let's call these 'built in' types
        Data data;
        data.name = "sand";
        addVoxel(data);

        data.name = "stone";
        addVoxel(data);

        data.name = "water";
        addVoxel(data);

        data.name = "dirt";
        addVoxel(data);

        data.name = "grass";
        addVoxel(data);

        data.name = "air";
        data.collidable = false;
        addVoxel(data);
        

        m_commonVoxels[CommonType::Sand] = getID("sand");
        m_commonVoxels[CommonType::Stone] = getID("stone");
        m_commonVoxels[CommonType::Air] = getID("air");
        m_commonVoxels[CommonType::Water] = getID("water");
        m_commonVoxels[CommonType::Dirt] = getID("dirt");
        m_commonVoxels[CommonType::Grass] = getID("grass");
    }

    std::uint8_t DataManager::addVoxel(const Data& voxel)
    {
        CRO_ASSERT(!voxel.name.empty(), "voxel must be named!");
        CRO_ASSERT(m_voxels.size() < std::numeric_limits<std::uint8_t>::max(), "Too many voxel types");
        m_voxelMap.emplace(voxel.name, static_cast<std::uint8_t>(m_voxels.size()));

        auto& data = m_voxels.emplace_back(voxel);
        data.id = static_cast<std::uint8_t>(m_voxels.size() - 1);
        return data.id;
    }

    const Data& DataManager::getVoxel(std::uint8_t id) const
    {
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
}