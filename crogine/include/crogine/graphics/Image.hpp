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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>

#include <crogine/detail/glm/vec2.hpp>

#include <string>
#include <vector>

namespace cro
{
    class Colour;

    /*!
    \brief CPU side representation of an image.
    Images can be loaded from file formats BMP, GIF, JPEG, LBM, PCX, PNG, PNM, TGA
    (desktop platforms can load all formats supported by SDL2_image).
    Images can have their pixels manipulated directly, but can only be drawn once
    they have been loaded on to the GPU via cro::Texture. Unlike texture Images 
    are copyable - although this is a heavy operation.
    */
    class CRO_EXPORT_API Image final
    {
    public:
        /*!
        \brief Constructor.
        Images are invalid until either create() or load*() functions have been
        called and returned successfully.
        */
        Image();

        /*!
        \brief Creates an empty image.
        \param width Width of image to create. On mobile platforms this should be pow2
        \param height Height of image to create. On mobile platforms this should be pow2
        \param colour Colour to fill image with
        \param format Image Format. Can be RGB or RGBA, defaults to RGBA
        */
        void create(uint32 width, uint32 height, Colour colour, ImageFormat::Type format = ImageFormat::RGBA);

        /*!
        \brief Attempts to load an image from a file on disk.
        On mobile platforms images should have power 2 dimensions.
        \returns true on success, else false
        */
        bool loadFromFile(const std::string& path);

        /*!
        \brief Attemps to load an image from raw pixels in memory.
        Pixels must be 8-bit and in RGB or RGBA format
        \param px Pointer to array of pixels in memory
        \param width Width of image to create. On mobile platforms this should be pow2
        \param height Height of image to create. On mobile platforms this should be pow2
        \param format Image Format. Can be RGB or RGBA
        */
        bool loadFromMemory(const uint8* px, uint32 width, uint32 height, ImageFormat::Type format);

        /*!
        \brief Sets pixels of a particular colour to transparent.
        Only works on RGBA format images which have been successfully loaded.
        \param colour Pixels this colour will be made transparent
        */
        void setTransparencyColour(Colour colour);

        /*!
        \brief Returns the dimensions of the image.
        */
        glm::uvec2 getSize() const;

        /*!
        \brief Returns the currently loaded image format.
        */
        ImageFormat::Type getFormat() const;

        /*!
        \brief Returns a pointer to the underlying pixel data
        */
        const uint8* getPixelData() const;


    private:
        glm::uvec2 m_size = glm::uvec2(0);
        ImageFormat::Type m_format;
        std::vector<uint8> m_data;
        bool m_flipped;
    };
}