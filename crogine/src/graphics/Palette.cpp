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

#include <crogine/core/Log.hpp>
#include <crogine/graphics/Palette.hpp>

#include <crogine/detail/Types.hpp>

#include <array>
#include <locale>

using namespace cro;

//hum, ASE files are big endian (I guess they started life on 68k macs?)
#ifdef _MSC_VER
#include <intrin.h>

#define swap16(x) _byteswap_ushort(x)
#define swap32(x) _byteswap_ulong(x)

#elif defined __GNUC__ || defined __clang__

#define swap16(x) __builtin_bswap16(x)
#define swap32(x) __builtin_bswap32(x)

#else

std::uint16_t swap16(std::uint16_t v)
{
    return ((v & 0x00ff) << 8) | ((v & 0xff00 >> 8));
}

std::uint32_t swap32(std::uint32_t v)
{
    return ((v & 0x000000ff) << 24) | ((v & 0x0000ff00) << 8) | ((v & 0x00ff0000) >> 8) | ((v & 0xff000000) >> 24);
}

#endif

namespace
{
    constexpr std::uint32_t toUint32(std::uint8_t b1, std::uint8_t b2, std::uint8_t b3, std::uint8_t b4)
    {
        return (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
    }

    struct ColourSpace final
    {
        static constexpr std::uint32_t CMYK = toUint32('C', 'M', 'Y', 'K');
        static constexpr std::uint32_t RGB = toUint32('R', 'G', 'B', ' ');
        static constexpr std::uint32_t LAB = toUint32('L', 'A', 'B', ' ');
        static constexpr std::uint32_t GREY = toUint32('G', 'r', 'a', 'y');
    };

    constexpr std::uint16_t Entry = 0x0001;
    constexpr std::uint16_t GroupStart = 0xc001;
    constexpr std::uint16_t GroupEnd = 0xc002;

    constexpr std::uint16_t CatGlobal = 0;
    constexpr std::uint16_t CatSpot = 1;
    constexpr std::uint16_t CatNormal = 2;

    std::array<float, 3u> lab2rgb(const std::array<float, 3u>& lab)
    {
        float y = (lab[0] + 16.f) / 116.f;
        float x = lab[1] / 500.f + y;
        float z = y - lab[2] / 200.f;
        float r = 0.f;
        float g = 0.f;
        float b = 0.f;

        x = 0.95047f * ((x * x * x > 0.008856f) ? x * x * x : (x - 16.f / 116.f) / 7.787f);
        y = 1.00000f * ((y * y * y > 0.008856f) ? y * y * y : (y - 16.f / 116.f) / 7.787f);
        z = 1.08883f * ((z * z * z > 0.008856f) ? z * z * z : (z - 16.f / 116.f) / 7.787f);

        r = x * 3.2406f + y * -1.5372f + z * -0.4986f;
        g = x * -0.9689f + y * 1.8758f + z * 0.0415f;
        b = x * 0.0557f + y * -0.2040f + z * 1.0570f;

        r = (r > 0.0031308f) ? (1.055f * std::pow(r, 1.f / 2.4f) - 0.055f) : 12.92f * r;
        g = (g > 0.0031308f) ? (1.055f * std::pow(g, 1.f / 2.4f) - 0.055f) : 12.92f * g;
        b = (b > 0.0031308f) ? (1.055f * std::pow(b, 1.f / 2.4f) - 0.055f) : 12.92f * b;

        return { std::min(1.f, std::max(0.f, r)), std::min(1.f, std::max(0.f, g)), std::min(1.f, std::max(0.f, b)) };
    }
}


Palette::Palette()
{
    
}

Palette::Palette(const std::string& path)
{
    //we'll let this function print error on failure
    loadFromFile(path);
}

bool Palette::loadFromFile(const std::string& path, bool append)
{
    if (!append)
    {
        m_swatches.clear();
    }

    auto fullPath = FileSystem::getResourcePath() + path;
    RaiiRWops file;
    file.file = SDL_RWFromFile(fullPath.c_str(), "rb");

    auto fileName = FileSystem::getFileName(path);

    if (!file.file)
    {
        LogE << "SDL: Failed opening " << fileName << ": " << SDL_GetError() << std::endl;
        return false;
    }

    //read header
    std::uint32_t ident = 0;
    file.file->read(file.file, &ident, sizeof(std::uint32_t), 1);
    
    //we'll just skip the byte swap, because we can
    if (ident != toUint32('F', 'E', 'S', 'A'))
    {
        LogE << "Incorrect ident found in " << fileName << std::endl;
        return false;
    }

    std::array<std::uint16_t, 2u> version = {};
    std::uint32_t blockCount = 0;

    file.file->read(file.file, version.data(), sizeof(std::uint16_t), 2);
    file.file->read(file.file, &blockCount, sizeof(std::uint32_t), 1);

    //TODO assert the values are valid here? What's valid in this context?
    //at least make sure the file size matches the expected numbetr of blocks?
    version[0] = swap16(version[0]);
    version[1] = swap16(version[1]);
    blockCount = swap32(blockCount);


    //parse the body of the file
    std::uint16_t currClass = 0;
    std::uint32_t currSize = 0;

    while (blockCount--)
    {
        file.file->read(file.file, &currClass, sizeof(std::uint16_t), 1);
        currClass = swap16(currClass);

        switch (currClass)
        {
        default: 
            LogW << "Found unknown class ident in palette " << fileName << std::endl;
            break;
        case Entry:
        case GroupStart:
        {
            file.file->read(file.file, &currSize, sizeof(currSize), 1);
            currSize = swap32(currSize);

            std::u16string name; //uses wide strings...
            std::uint16_t nameLength = 0;

            file.file->read(file.file, &nameLength, sizeof(std::uint16_t), 1);
            nameLength = swap16(nameLength);

            name.resize(nameLength);
            file.file->read(file.file, name.data(), nameLength * 2, 1);

            //hmm there's probably a container algo for this but hey
            for (auto& c : name)
            {
                c = swap16(c);
            }

            if (currClass == GroupStart)
            {
                //insert a new swatch with our name
                //TODO wstring does NOT imply utf16
                m_swatches.emplace_back().name = cro::String::fromUtf16(name.begin(), name.end());
            }
            else
            {
                if (m_swatches.empty())
                {
                    m_swatches.emplace_back().name = "Default";
                }

                auto& colourEntry = m_swatches.back().colours.emplace_back();
                colourEntry.name = cro::String::fromUtf16(name.begin(), name.end());

                //process the colour data
                std::uint32_t colourSpace = 0;
                file.file->read(file.file, &colourSpace, sizeof(std::uint32_t), 1);
                colourSpace = swap32(colourSpace);

                switch (colourSpace)
                {
                default:
                    LogW << "Found unknown colourspace ident in " << fileName << std::endl;
                    break;
                case ColourSpace::CMYK:
                {
                    std::array<std::uint32_t, 4> data = {};
                    file.file->read(file.file, data.data(), sizeof(std::uint32_t), 4);
                    for (auto& c : data)
                    {
                        c = swap32(c);
                    }

                    std::array<float, 4> c =
                    {
                        *reinterpret_cast<float*>(&data[0]),
                        *reinterpret_cast<float*>(&data[1]),
                        *reinterpret_cast<float*>(&data[2]),
                        *reinterpret_cast<float*>(&data[3])
                    };

                    //convert the colour space to RGB
                    colourEntry.colour = cro::Colour(
                        (1.f - c[0]) * (1.f - c[3]),
                        (1.f - c[1]) * (1.f - c[3]),
                        (1.f - c[2]) * (1.f - c[3])
                        );
                }
                    break;
                case ColourSpace::GREY:
                {
                    std::uint32_t data;
                    file.file->read(file.file, &data, sizeof(std::uint32_t), 1);
                    data = swap32(data);

                    colourEntry.colour = cro::Colour(
                        *reinterpret_cast<float*>(&data),
                        *reinterpret_cast<float*>(&data),
                        *reinterpret_cast<float*>(&data),
                        1.f);
                }
                    break;
                case ColourSpace::LAB:
                {
                    std::array<std::uint32_t, 3> data = {};
                    file.file->read(file.file, data.data(), sizeof(std::uint32_t), 3);
                    for (auto& c : data)
                    {
                        c = swap32(c);
                    }

                    //convert to RGB
                    std::array<float, 3u> lab =
                    {
                        *reinterpret_cast<float*>(&data[0]),
                        *reinterpret_cast<float*>(&data[1]),
                        *reinterpret_cast<float*>(&data[2])
                    };

                    auto rgb = lab2rgb(lab);
                    colourEntry.colour = cro::Colour(rgb[0], rgb[1], rgb[2]);
                }
                    break;
                case ColourSpace::RGB:
                {
                    std::array<std::uint32_t, 3> data = {};
                    file.file->read(file.file, data.data(), sizeof(std::uint32_t), 3);
                    for (auto& c : data)
                    {
                        c = swap32(c);
                    }

                    colourEntry.colour = cro::Colour(
                        *reinterpret_cast<float*>(&data[0]), 
                        *reinterpret_cast<float*>(&data[1]), 
                        *reinterpret_cast<float*>(&data[2]), 1.f );
                }
                    break;
                }

                //not sure we can do anything useful with this
                //but we need to read it anyway for file indexing sake
                std::uint16_t category = 0;
                file.file->read(file.file, &category, sizeof(std::uint16_t), 1);
            }
        }
            break;
        case GroupEnd:

            break;
        }
    }

    return true;
}