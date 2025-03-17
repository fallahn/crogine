/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include "StaticMeshFile.hpp"

#include <crogine/core/Log.hpp>

#include <SDL_rwops.h>

namespace cro::Detail
{
    bool readCMF(const std::string& path, MeshFile& output)
    {
        auto* file = SDL_RWFromFile(path.c_str(), "rb");

        if (!file)
        {
            LogE << "SDL: " << path << ": " << SDL_GetError() << std::endl;
            return false;
        }

        auto checkError = [&](std::size_t readCount)
        {
            if (readCount == 0)
            {
                LogE << path << ": Unexpected End of File" << std::endl;
                SDL_RWclose(file);
                file = nullptr;
                return true;
            }
            return false;
        };


        auto readCount = SDL_RWread(file, &output.flags, sizeof(std::uint8_t), 1);
        if (checkError(readCount))
        {
            return false;
        }
        readCount = SDL_RWread(file, &output.arrayCount, sizeof(std::uint8_t), 1);
        if (checkError(readCount))
        {
            return false;
        }

        std::int32_t indexArrayOffset = 0;
        std::vector<std::int32_t> indexSizes(output.arrayCount);
        readCount = SDL_RWread(file, &indexArrayOffset, sizeof(std::int32_t), 1);
        if (checkError(readCount))
        {
            return false;
        }
        readCount = SDL_RWread(file, indexSizes.data(), sizeof(std::int32_t), output.arrayCount);
        if (checkError(readCount))
        {
            return false;
        }

        std::size_t headerSize = sizeof(output.flags) + sizeof(output.arrayCount) +
            sizeof(indexArrayOffset) + ((sizeof(std::int32_t) * output.arrayCount));

        std::size_t vboSize = (indexArrayOffset - headerSize) / sizeof(float);
        output.vboData.resize(vboSize);
        readCount = SDL_RWread(file, output.vboData.data(), sizeof(float), vboSize);
        if (checkError(readCount))
        {
            return false;
        }

        output.indexArrays.resize(output.arrayCount);
        for (auto i = 0; i < output.arrayCount; ++i)
        {
            output.indexArrays[i].resize(indexSizes[i] / sizeof(std::uint32_t));
            readCount = SDL_RWread(file, output.indexArrays[i].data(), sizeof(std::uint32_t), indexSizes[i] / sizeof(std::uint32_t));
            if (checkError(readCount))
            {
                return false;
            }
        }

        SDL_RWclose(file);

        return true;
    }
}