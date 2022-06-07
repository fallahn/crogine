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
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <crogine/detail/glm/vec2.hpp>

#include <string>

namespace cro
{
    class Image;

    /*!
    \brief Generic texture wrapper for OpenGL RGB or RGBA textures.
    This class is intended for use with mesh texturing, rather than any
    advanced texture usage.
    */
    class CRO_EXPORT_API Texture final : public Detail::SDLResource
    {
    public:
        /*!
        \brief Constructor.
        By default Textures are invalid until create() has been called on
        them, or they have had a successful call to loadFromFile() or
        loadFromImage()
        */
        Texture();
        ~Texture();

        Texture(const Texture&) = delete;
        Texture(Texture&&) noexcept;
        Texture& operator = (const Texture&) = delete;
        Texture& operator = (Texture&&) noexcept;

        /*!
        \brief Creates an empty texture.
        \param width Width of the texture to create. On mobile platforms this should be pow2
        \param height Height of the texture to create. On mobile platforms this should be pow2
        \param format Graphics::Format type - valid values are RGB or RGBA
        */
        void create(std::uint32_t width, std::uint32_t height, ImageFormat::Type format = ImageFormat::RGBA);

        /*!
        \brief Attempts to load the file in the given file path.
        \param path Path to file to load. The image file should have pow2 dimensions on mobile platforms
        \param createMipMaps Set true to automatically create mipmap levels for this texture
        \returns true on success, else false
        */
        bool loadFromFile(const std::string& path, bool createMipMaps = false);

        /*!
        \brief Attempts to create the texture from a given Image.
        \param image A reference to a loaded image from which to create a texture
        \param createMipMaps Set true to automatically create mipmap levels for this texture
        \returns true on success, else false
        \see Image
        */
        bool loadFromImage(const Image& image, bool createMipmaps = false);

        /*!
        \brief Updates the pixel data for the texture.
        Ensure the texture is valid by calling create() or successfully calling loadFromFile()
        before attempting to update it.
        \param pixels Pointer to array containing pixel data. The data must match the same format
        returned by Texture::getFormat() else results are undefined.
        \param area InRect representing the area of the texture to update. If the size is zero
        the entire texture will be updated.
        */
        bool update(const std::uint8_t* pixels, bool createMipMaps = false, URect area = {});

        /*
        \brief Updates the texture with the information stored in the given texture
        */
        bool update(const Texture& other, std::uint32_t xPos = 0, std::uint32_t yPos = 0);

        /*!
        \brief Returns the dimensions of the image which makes up this texture
        */
        glm::uvec2 getSize() const;

        /*!
        \brief Returns the current format of the texture
        */
        ImageFormat::Type getFormat() const;

        /*!
        brief Returns the OpenGL handle used by this texture.
        */
        std::uint32_t getGLHandle() const;

        /*!
        \brief Enables texture smoothing
        */
        void setSmooth(bool);

        /*!
        \brief Returns true is texture smoothing is enabled
        */
        bool isSmooth() const;

        /*!
        \brief Enables or disables texture repeating
        */
        void setRepeated(bool);

        /*!
        \brief Returns true if texture repeating is enabled
        */
        bool isRepeated() const;

        /*!
        \brief Returns the max texture size for the current platform
        */
        static std::uint32_t getMaxTextureSize();

        /*!
        \brief Swaps this texture with the given texture
        */
        void swap(Texture& other);

        /*!
        \brief Converts a subrect whose value is in pixels to
        normalised coordinates according to this texture's size.
        \param rect A FloatRect containing the cordinates to convert
        \returns FloatRect containing normalised coordinates, which will be
        empty if there is no texture currently loaded.
        */
        FloatRect getNormalisedSubrect(FloatRect rect) const;

        /*
        \brief Saves the texture to a png file if it is a valid texture.
        If the texture contains no data, or create() had not been called
        then this function does nothing.
        \param path A string containg a path to same the texture to.
        \returns true if successful else returns false
        */
        bool saveToFile(const std::string& path) const;

        /*
        \brief Saves the texture to the given image file.
        Resizes the image to the texture size if necessary
        \param image The destination image
        \returns true if successful, else false
        */
        bool saveToImage(cro::Image& image) const;

    private:
        glm::uvec2 m_size;
        ImageFormat::Type m_format;
        std::uint32_t m_handle;
        bool m_smooth;
        bool m_repeated;
        bool m_hasMipMaps;

        bool isFloat(SDL_RWops* file);
        bool loadAsFloat(SDL_RWops* file, bool createMipmaps);
        bool loadAsByte(SDL_RWops* file, bool createMipmaps);
        void generateMipMaps();
    };
}