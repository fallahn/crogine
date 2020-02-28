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

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

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
        Water,

        Count
    };

    struct Data final
    {
        bool collidable = true;
        std::uint8_t id = 0;
        MeshStyle style = MeshStyle::Voxel;
        Type type = Type::Solid;

        std::string name;
    };

    class DataManager final
    {
    public:
        DataManager();

        void initTypes();

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
    };
}