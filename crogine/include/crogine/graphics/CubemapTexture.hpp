/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
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

#include <crogine/Config.hpp>
#include <crogine/detail/SDLResource.hpp>

namespace cro
{
    /*!
    /brief Loads a set of images into a cubmap from a *.ccm file with the following format:
    /begincode
    cubemap
    {
        up = "py.png"
        down = "ny.png"
        right = "nx.png"
        left = "px.png"
        front = "pz.png"
        back = "nz.png"
    }
    /endcode
    Image files may be referenced in the same directory as the *.ccm file, be realtive
    to the current working directory or have absolute paths.
    */
    class CRO_EXPORT_API CubemapTexture : public Detail::SDLResource
    {
    public:
        CubemapTexture();
        ~CubemapTexture();

        CubemapTexture(const CubemapTexture&) = delete;
        CubemapTexture(CubemapTexture&&) noexcept;

        CubemapTexture& operator = (const CubemapTexture&) = delete;
        CubemapTexture& operator = (CubemapTexture&&) noexcept;

        std::uint32_t getGLHandle() const;

        /*!
        \brief Returns the number of loaded cubemaps - eg the size
        of the array if it is a cubmap array, else 1 or 0 if nothing
        has been loaded.
        */
        std::uint32_t getCubemapCount() const { return m_cubemapCount; }

        /*!
        \brief Attempts to load a cubemap from a *.ccm configuration file
        \returns true on success else false.
        */
        bool loadFromFile(const std::string& path);


        /*!
        \brief Attempts to load the given vector of paths to *.ccm files
        into a cubemap array.
        \returns true on success else false
        */
        bool loadFromFiles(const std::vector<std::string>& paths);

        /*!
        \brief Genrates mipmap chain for the texture if data is loaded
        else does nothing.
        */
        void generateMipMaps();

    private:
        std::uint32_t m_handle;
        std::uint32_t m_cubemapCount;

        bool parseInputFile(const std::string& filePath, std::array<std::string, 6u>& outFiles);
    };
}
