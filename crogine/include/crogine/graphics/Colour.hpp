/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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
#include <crogine/detail/Assert.hpp>

#include <crogine/detail/glm/vec4.hpp>

namespace cro
{
    /*!
    \brief A class representing an RGBA colour.
    Colours are stored as normalised values so that
    they may be used to represent greater than 8 bit values
    */
    class CRO_EXPORT_API Colour final
    {
    public:

        /*!
        \brief Default constructor. Initialises to black.
        */
        constexpr Colour()
            : r(0.f), g(0.f), b(0.f), a(1.f) {}

        /*!
        \brief Construct the colour from 3 or 4 8-bit values
        */
        constexpr Colour(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255)
            : r(static_cast<float>(red) / 255.f),
            g(static_cast<float>(green) / 255.f),
            b(static_cast<float>(blue) / 255.f),
            a(static_cast<float>(alpha) / 255.f) {}

        /*!
        \brief Constructs the colour from a 32bit bitmask of 4 8-bit values
        in RGBA order
        */
        constexpr Colour(std::uint32_t mask)
            : r(static_cast<float>((mask >> 24) & 0xFF) / 255.f),
            g(static_cast<float>((mask >> 16) & 0xFF) / 255.f),
            b(static_cast<float>((mask >> 8) & 0xFF) / 255.f),
            a(static_cast<float>(mask & 0xFF) / 255.f) {}
        
        /*!
        \brief Constructs the colour from 3 or 4 normalised values
        */
        constexpr Colour(float red, float green, float blue, float alpha = 1.f)
            : r(red), g(green), b(blue), a(alpha)
        {
            //CRO_ASSERT((r >= 0 && r <= 1) && (g >= 0 && g <= 1) && (b >= 0 && b <= 1) && (a >= 0 && a <= 1), "Values must be normalised");
            //static_assert((r >= 0 && r <= 1) && (g >= 0 && g <= 1) && (b >= 0 && b <= 1) && (a >= 0 && a <= 1), "");
        }


        /*!
        \brief Constructs the colour from a vector 3
        The alpha value defaults to 1
        */
        explicit Colour(glm::vec3 vector);

        /*!
        \brief Constructs the colour from a vector 4
        */
        Colour(glm::vec4 vector);

        /*
        \brief Creates a colour by assigning a RGBA packed 32bit integer
        */
        constexpr Colour& operator = (std::uint32_t mask)
        {
            r = (static_cast<float>((mask >> 24) & 0xFF) / 255.f);
            g = (static_cast<float>((mask >> 16) & 0xFF) / 255.f);
            b = (static_cast<float>((mask >> 8) & 0xFF) / 255.f);
            a = (static_cast<float>(mask & 0xFF) / 255.f);
            return *this;
        }

        /*!
        \brief Assignment operator for glm::vec3
        Assumes the alpha channel is 1
        */
        Colour& operator = (glm::vec3);

        /*!
        \brief Assignment operator for glm::vec4
        */
        Colour& operator = (glm::vec4);

        /*!
        \brief sets the red channel value
        */
        void setRed(std::uint8_t red);

        void setRed(float red);

        /*!
        \brief sets the green channel value
        */
        void setGreen(std::uint8_t green);

        void setGreen(float green);

        /*!
        \brief sets the blue channel value
        */
        void setBlue(std::uint8_t blue);

        void setBlue(float blue);

        /*!
        \brief sets the alpha channel value
        */
        void setAlpha(std::uint8_t alpha);

        void setAlpha(float alpha);


        /*!
        \brief Returns the red channel value as a byte
        */
        constexpr std::uint8_t getRedByte() const
        {
            return static_cast<std::uint8_t>(255.f * r);
        }

        /*!
        \brief Returns the green channel value as a byte
        */
        constexpr std::uint8_t getGreenByte() const
        {
            return static_cast<std::uint8_t>(255.f * g);
        }

        /*!
        \brief Returns the blue channel value as a byte
        */
        constexpr std::uint8_t getBlueByte() const
        {
            return static_cast<std::uint8_t>(255.f * b);
        }

        /*!
        \brief Returns the alpha channel value as a byte
        */
        constexpr std::uint8_t getAlphaByte() const
        {
            return static_cast<std::uint8_t>(255.f * a);
        }

        /*!
        \brief Returns the red channel as a normalised value
        */
        float getRed() const;
        /*!
        \brief Returns the green channel as a normalised value
        */
        float getGreen() const;
        /*!
        \brief Returns the blue channels as a normalised value
        */
        float getBlue() const;
        /*!
        \brief Returns the alpha channel as a normalised value
        */
        float getAlpha() const;

        /*!
        \brief Returns the colour as a RGBA packed 32bit integer
        */
        std::uint32_t getPacked() const;

        /*!
        \brief Returns the colour as a RGBA vec4 float
        */
        glm::vec4 getVec4() const;

        static const Colour Red;
        static const Colour Green;
        static const Colour Blue;
        static const Colour Cyan;
        static const Colour Magenta;
        static const Colour Yellow;
        static const Colour Black;
        static const Colour White;
        static const Colour Transparent;

        static const Colour AliceBlue;
        static const Colour CornflowerBlue;
        static const Colour DarkGrey;
        static const Colour Gainsboro;
        static const Colour LightGrey;
        static const Colour Plum;
        static const Colour Teal;

        float* asArray() { return &r; }
        const float* asArray() const { return &r; }

        explicit operator std::uint32_t() const;
        explicit operator glm::vec4() const;
        explicit operator float* ();
        explicit operator const float* () const;

    private:

        float r;
        float g;
        float b;
        float a;

        friend CRO_EXPORT_API bool operator == (const Colour&, const Colour&);
        friend CRO_EXPORT_API bool operator != (const Colour&, const Colour&);
        friend CRO_EXPORT_API Colour operator + (const Colour&, const Colour&);
        friend CRO_EXPORT_API Colour operator - (const Colour&, const Colour&);
        friend CRO_EXPORT_API Colour operator * (const Colour&, const Colour&);

        friend CRO_EXPORT_API Colour& operator += (Colour&, const Colour&);
        friend CRO_EXPORT_API Colour& operator -= (Colour&, const Colour&);
        friend CRO_EXPORT_API Colour& operator *= (Colour&, const Colour&);
    };

    CRO_EXPORT_API bool operator == (const Colour&, const Colour&);
    CRO_EXPORT_API bool operator != (const Colour&, const Colour&);
    CRO_EXPORT_API Colour operator + (const Colour&, const Colour&);
    CRO_EXPORT_API Colour operator - (const Colour&, const Colour&);
    CRO_EXPORT_API Colour operator * (const Colour&, const Colour&);

    CRO_EXPORT_API Colour& operator += (Colour&, const Colour&);
    CRO_EXPORT_API Colour& operator -= (Colour&, const Colour&);
    CRO_EXPORT_API Colour& operator *= (Colour&, const Colour&);

    namespace Detail
    {
        static constexpr std::uint32_t Red            = 0xff0000ff;
        static constexpr std::uint32_t Green          = 0x00ff00ff;
        static constexpr std::uint32_t Blue           = 0x0000ffff;
        static constexpr std::uint32_t Cyan           = 0x00ffffff;
        static constexpr std::uint32_t Magenta        = 0xff00ffff;
        static constexpr std::uint32_t Yellow         = 0xffff00ff;
        static constexpr std::uint32_t Black          = 0x000000ff;
        static constexpr std::uint32_t White          = 0xffffffff;
        static constexpr std::uint32_t Transparent    = 0x00000000;

        static constexpr std::uint32_t AliceBlue      = 0x00F0F8FF;
        static constexpr std::uint32_t CornflowerBlue = 0x6495EDFF;
        static constexpr std::uint32_t DarkGrey       = 0xA9A9A9FF;
        static constexpr std::uint32_t Gainsboro      = 0xDCDCDCFF;
        static constexpr std::uint32_t LightGrey      = 0xD3D3D3FF;
        static constexpr std::uint32_t Plum           = 0xDDA0DDFF;
        static constexpr std::uint32_t Teal           = 0x008080FF;


        /*!
        \brief Low Precision Colour
        Helper class to enable normalised integer values in vertex data
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
}