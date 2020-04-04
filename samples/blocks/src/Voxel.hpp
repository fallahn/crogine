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

#pragma once

#include <crogine/detail/glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>

namespace vx
{
    enum class MeshStyle : std::uint8_t 
    {
        Voxel = 0,
        Cross,
        None,

        Error
    };

    enum class Type : std::uint8_t
    {
        Solid = 0,
        Liquid,
        Gas,
        
        Detail,

        Error
    };

    enum CommonType
    {
        Air = 0,
        Stone,
        Sand,
        Dirt,
        Grass,
        Water,

        Count,

        OutOfBounds = 255
    };

    enum Side
    {
        South, North, East, West, Top, Bottom
    };

    struct Data final
    {
        bool collidable = true;
        std::uint8_t id = 0;
        MeshStyle style = MeshStyle::Voxel;
        Type type = Type::Solid;
        //indices into the tile map for each side
        //indexed by vx::Side
        std::array<std::uint16_t, 6u> tileIDs = {};
        std::string name;
    };

    struct Face final
    {
        glm::ivec3 position = glm::ivec3(0);
        //these are the same BL, BR, TL, TR as the positions
        //when creating a quad
        std::array<std::uint8_t, 4u> ao = { 3,3,3,3 };
        float offset = 0.f; //eg if a water surface should be slightly lower
        std::uint16_t textureIndex = 0;
        bool visible = true;
        Side direction = Top;
        std::uint8_t id;

        bool operator == (const Face& other)
        {
            std::uint32_t aoMask = *reinterpret_cast<std::uint32_t*>(ao.data());
            std::uint32_t otherAoMask = *reinterpret_cast<const std::uint32_t*>(other.ao.data());
            bool aoMatch = (aoMask == otherAoMask);

            return (other.id == id && other.visible == visible && aoMatch && other.textureIndex == textureIndex);
        }

        bool operator != (const Face& other)
        {
            return !(*this == other);
        }
    };

    class DataManager final
    {
    public:
        DataManager();

        std::uint8_t addVoxel(const Data&);

        const Data& getVoxel(std::uint8_t) const;
        const Data& getVoxel(const std::string&) const;

        std::uint8_t getID(CommonType) const;
        std::uint8_t getID(const std::string&) const;

        const std::vector<Data>& getData() const;

    private:
        std::vector<Data> m_voxels;
        std::vector<std::uint8_t> m_commonVoxels;
        std::unordered_map<std::string, std::uint8_t> m_voxelMap;

        Data m_outOfBoundsData;
    };

    struct Update final
    {
        glm::ivec3 position;
        std::uint8_t id;
    };

    //fast voxel traversal implementation from
    //https://github.com/francisengelmann/fast_voxel_traversal (MIT license)
    std::vector<glm::ivec3> intersectedVoxel(glm::vec3 start, glm::vec3 direction, float range);
}