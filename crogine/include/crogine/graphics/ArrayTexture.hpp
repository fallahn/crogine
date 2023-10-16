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
#include <crogine/core/Log.hpp>
#include <crogine/graphics/ImageArray.hpp>
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/vec2.hpp>

#include <cstdint>
#include <vector>
#include <type_traits>

namespace cro
{
    /*!
    \brief Template class for creating array textures

    Currently only supports 4 channel image data
    */
    template <class T, std::uint32_t Layers, std::uint32_t Width, std::uint32_t Height>
    class ArrayTexture final
    {
    public:
        ArrayTexture()
        {
            if constexpr (std::is_same<T, float>::value)
            {
                m_type = GL_FLOAT;
                m_format = GL_RGBA32F;
                createTexture();
            }

            else if constexpr (std::is_same<T, std::uint16_t>::value)
            {
                m_type = GL_UNSIGNED_SHORT;
                m_format = GL_RGBA16;
                createTexture();
            }

            else if constexpr (std::is_same<T, std::uint8_t>::value)
            {
                m_type = GL_UNSIGNED_BYTE;
                m_format = GL_RGBA8;
                createTexture();
            }
            else
            {
                static_assert(false, "Use float, U16 or U8");
            }
        }

        ~ArrayTexture()
        {
            if (m_handle)
            {
                glDeleteTextures(1, &m_handle);
            }
        }

        //TODO implement moving
        ArrayTexture(const ArrayTexture&) = delete;
        ArrayTexture(ArrayTexture&&) = delete;
        const ArrayTexture& operator = (const ArrayTexture&) = delete;
        const ArrayTexture& = operator (ArrayTexture&&) = delete;

        operator TextureID() { return TextureID(m_handle, true;) }

        std::uint32_t getGLHandle() const { return m_handle; }
        constexpr glm::uvec2 getSize() noexcept const return { glm::uvec2(Width, Height); }

        bool insertLayer(const cro::ImageArray<T>& data, std::uint32_t layer)
        {
            //validate incoming data
            if (data.getFormat() != ImageFormat::RGBA)
            {
                LogE << __FILE__ << " image array not RGBA" << std::endl;
                return false;
            }

            if (data.getSize() != getSize())
            {
                LogE << __FILE__ << " image array incorrect dimensions" << std::endl;
                return false;
            }

            if (layer >= Layers)
            {
                LogE << __FILE__ << " image array layer out of range" << std::endl;
                return false;
            }

            //bind tex and upload
            return updateTexture(data.data(), layer);
        }

        bool insertLayer(const std::vector<T>& data, std::uint32_t layer)
        {
            //validate incoming data
            if (data.size() % 4 != 0)
            {
                LogE << __FILE__ << " array texture data probably not RGBA" << std::endl;
                return false;
            }

            if (data.size() != (Width * Height * 4))
            {
                LogE << __FILE__ << " array texture data not correct dimensions" << std::endl;
                return false;
            }

            if (layer >= Layers)
            {
                LogE << __FILE__ << " array texture data layer out of range" << std::endl;
                return false;
            }

            //bind tex and upload
            return updateTexture(data.data(), layer);
        }
    private:

        std::uint32_t m_handle = 0;
        std::uint32_t m_type = 0;
        std::uint32_t m_format = 0;

        void createTexture()
        {
            glGenTextures(1, &m_handle);
            glBindTexture(GL_TEXTURE_2D_ARRAY, m_handle);
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 0, m_type, Width, Height, Layers);
        }

        bool updateTexture(const void* data, std::uint32_t layer)
        {
            if (m_handle)
            {
                glBindTexture(GL_TEXTURE_2D_ARRAY, m_handle);
                glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, m_format, Width, Height, layer, 0, GL_RGBA, m_type, data);
                return true;
            }
            return false;
        }
    };
}