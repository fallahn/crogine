/*-----------------------------------------------------------------------

Matt Marchant 2023
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
#include <crogine/graphics/Image.hpp>

#include <cstdint>

#include <vector>
#include <string>

namespace cro
{
    /*!
    \brief Utility class for storing raw image data as an array
    Supported formats are uint8_t, uint16_t and float.
    */
    template <class T>
    class ImageArray final
    {
    public:

        ImageArray() = default;
        ImageArray(const ImageArray&) = default;
        ImageArray(ImageArray&&) = default;
        ImageArray& operator = (const ImageArray&) = default;
        ImageArray& operator = (ImageArray&&) = default;

        ImageArray(Image&&)
        {
            static_assert(std::is_same<std::uint8_t, T>::value, "Only 8 bit arrays");
        }

        ImageArray& operator = (Image&&)
        {
            static_assert(std::is_same<std::uint8_t, T>::value, "Only 8 bit arrays");
        }

        /*!
        \brief Attempts to load an image file from the given path
        \param path Path to image file to load. This is an absolute path
        so on macOS MUST include getResourcePath() if loading from a bundle.
        \returns true if successful else false
        */
        bool loadFromFile(const std::string& path)
        {
            static_assert(sizeof(T) == -1, "Only U8, U16 and float supported");
            return false;
        }

        T& operator [](std::size_t i) { return m_data[i]; }
        const T& operator [](std::size_t i) const { return m_data[i]; }

        auto begin() { return m_data.begin(); }
        auto begin() const { return m_data.begin(); }

        auto end() { return m_data.end(); }
        auto end() const { return m_data.end(); }

        auto cbegin() const { return m_data.cbegin(); }
        auto cend() const { return m_data.cend(); }

        auto* data() { return m_data.data(); }
        const auto* data() const { return m_data.data(); }

        auto size() const { return m_data.size(); }
        bool empty() const { return m_data.empty(); }

        void clear()
        {
            m_data.clear();
            m_dimensions = { 0,0 };
            m_channels = 0;
        }

        /*!
        \brief Returns the width and height of the image if it is loaded
        */
        glm::ivec2 getDimensions() const { return m_dimensions; }

        /*!
        \brief Returns the number of colour channels in the image
        Usually 1,3 or 4 - or 0 if no image is loaded.
        */
        std::int32_t getChannels() const { return m_channels; }

        /*!
        \brtief Returns the equivalent cro::Image::Format based on the number of channels
        */
        ImageFormat::Type getFormat() const
        {
            switch (m_channels)
            {
            default: return ImageFormat::None;
            case 1: return ImageFormat::A;
            case 3: return ImageFormat::RGB;
            case 4: return ImageFormat::RGBA;
            }
        }

    private:
        std::vector<T> m_data;
        glm::ivec2 m_dimensions = glm::ivec2(0u);
        std::int32_t m_channels = 0;
    };

    namespace Detail
    {
        CRO_EXPORT_API bool loadFromU8(const std::string& path, std::vector<std::uint8_t>&, glm::ivec2&, std::int32_t&);
        CRO_EXPORT_API bool loadFromU16(const std::string& path, std::vector<std::uint16_t>&, glm::ivec2&, std::int32_t&);
        CRO_EXPORT_API bool loadFromFloat(const std::string& path, std::vector<float>&, glm::ivec2&, std::int32_t&);
    }

    template <>
    inline ImageArray<std::uint8_t>::ImageArray(Image&& img)
    {
        m_data.clear();
        m_data.swap(img.m_data);

        m_dimensions = img.m_size;
        img.m_size = { 0u,0u };

        switch (img.m_format)
        {
        default:
            assert(true);
            break;
        case ImageFormat::A:
            m_channels = 1;
            break;
        case ImageFormat::RGB:
            m_channels = 3;
            break;
        case ImageFormat::RGBA:
            m_channels = 4;
            break;
        }

        img.m_format = ImageFormat::None;
        //rather suspiciously this causes some images to be incorrectly
        //flipped when loaded - presumably because stb_image's flip
        //property is not correctly restored...
        //img.m_flipOnLoad = false;
        //img.m_flipped = false;
    }

    template <>
    inline ImageArray<std::uint8_t>& ImageArray<std::uint8_t>::operator = (Image&& img)
    {
        //TODO share this code with above but without the hassle of friendship.
        m_data.clear();
        m_data.swap(img.m_data);

        m_dimensions = img.m_size;
        img.m_size = { 0u,0u };

        switch (img.m_format)
        {
        default:
            assert(true);
            break;
        case ImageFormat::A:
            m_channels = 1;
            break;
        case ImageFormat::RGB:
            m_channels = 3;
            break;
        case ImageFormat::RGBA:
            m_channels = 4;
            break;
        }

        img.m_format = ImageFormat::None;
        //img.m_flipOnLoad = false;
        //img.m_flipped = false;

        return *this;
    }

    template <>
    inline bool ImageArray<std::uint8_t>::loadFromFile(const std::string& path)
    {
        return cro::Detail::loadFromU8(path, m_data, m_dimensions, m_channels);
    }

    template <>
    inline bool ImageArray<std::uint16_t>::loadFromFile(const std::string& path)
    {
        return cro::Detail::loadFromU16(path, m_data, m_dimensions, m_channels);
    }

    template <>
    inline bool ImageArray<float>::loadFromFile(const std::string& path)
    {
        return cro::Detail::loadFromFloat(path, m_data, m_dimensions, m_channels);
    }
}