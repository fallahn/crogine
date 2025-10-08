/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include <crogine/detail/Assert.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/graphics/Colour.hpp>

#include <crogine/util/Maths.hpp>

#include <limits>

namespace cro
{
    namespace Detail
    {
        //the real question is will all this processing
        //actually offset the advantage of smaller vertices?
        template <typename T> 
        constexpr T normaliseTo(float v)
        {
            static_assert(std::is_pod<T>::value, "");
            if constexpr (std::is_unsigned<T>::value)
            {
                CRO_ASSERT(v >= 0, "");
                return static_cast<T>(static_cast<float>(std::numeric_limits<T>::max()) * v);
            }
            else
            {
                const auto s = Util::Maths::sgn(v);

                const auto vs = v * s;
                v = (vs - std::floor(vs));
                return static_cast<T>(static_cast<float>(std::numeric_limits<T>::max()) * v) * s;
            }
        }

        /*
        Low Precision Colour
        Helper class to enable normalised integer values in Vertex2D
        */
        struct ColourLowP final
        {
            std::uint8_t r = 0;
            std::uint8_t g = 0;
            std::uint8_t b = 0;
            std::uint8_t a = 255;

            ColourLowP() = default;
            constexpr ColourLowP(const Colour& c)
                : r(c.getRedByte()), g(c.getGreenByte()),
                b(c.getBlueByte()), a(c.getAlphaByte())
            {

            }

            constexpr ColourLowP& operator = (Colour& c)
            {
                r = c.getRedByte();
                g = c.getBlueByte();
                b = c.getGreenByte();
                a = c.getAlphaByte();

                return *this;
            }

            constexpr ColourLowP& operator = (glm::vec4 c)
            {
                setRed(c.r);
                setGreen(c.g);
                setBlue(c.b);
                setAlpha(c.a);
                return *this;
            }

            constexpr void setRed(std::uint8_t v) { r = v; }
            constexpr void setGreen(std::uint8_t v) { g = v; }
            constexpr void setBlue(std::uint8_t v) { b = v; }
            constexpr void setAlpha(std::uint8_t v) { a = v; }

            constexpr void setRed(float v) { r = static_cast<std::uint8_t>(v * 255.f); }
            constexpr void setGreen(float v) { g = static_cast<std::uint8_t>(v * 255.f); }
            constexpr void setBlue(float v) { b = static_cast<std::uint8_t>(v * 255.f); }
            constexpr void setAlpha(float v) { a = static_cast<std::uint8_t>(v * 255.f); }

            float getRed() const { return static_cast<float>(r) / 255.f; }
            float getGreen() const { return static_cast<float>(g) / 255.f; }
            float getBlue() const { return static_cast<float>(b) / 255.f; }
            float getAlpha() const { return static_cast<float>(a) / 255.f; }
        };
    }

    /*!
    \brief 2D vertex data struct
    Used to describe the layout of 2D drawable vertices
    vertex data contains position, colour and UV coordinates.
    Vertex2D is used in conjunction with Drawable2D components
    providing a fixed layout. Custom Drawable2D shaders should
    provide these attributes in the vertex shader.

    Vertex2D can also be used with SimpleVertexArray

    Texture coords should be provided in the normalised (0 - 1) range.
    \see Drawable2D
    \see SimpleVertexArray
    */
    struct CRO_EXPORT_API Vertex2D final
    {
        constexpr Vertex2D() = default;
        constexpr Vertex2D(glm::vec2 pos) : position(pos) {}
        constexpr Vertex2D(glm::vec2 pos, glm::vec2 coord) : position(pos), UV(coord) {}
        constexpr Vertex2D(glm::vec2 pos, Colour c) : position(pos), colour(c) {}
        constexpr Vertex2D(glm::vec2 pos, glm::vec2 coord, Colour c) : position(pos), UV(coord), colour(c) {}

        glm::vec2 position = glm::vec2(0.f);
        glm::vec2 UV = glm::vec2(0.f);
        Detail::ColourLowP colour = Colour(Detail::White);
    };
}