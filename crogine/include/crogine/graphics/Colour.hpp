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
        Colour();
        /*!
        \brief Construct the colour from 3 or 4 8-bit values
        */
        explicit constexpr Colour(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255);
            
        /*!
        \brief Constructs the colour from a 32bit bitmask of 4 8-bit values
        in RGBA order
        */
        explicit constexpr Colour(std::uint32_t mask);

        /*!
        \brief Constructs the colour from 3 or 4 normalised values
        */
        explicit Colour(float red, float green, float blue, float alpha = 1.f);

        /*!
        \brief Constructs the colour from a vector 4
        */
        Colour(glm::vec4 vector);

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
        std::uint8_t getRedByte() const;

        /*!
        \brief Returns the green channel value as a byte
        */
        std::uint8_t getGreenByte() const;

        /*!
        \brief Returns the blue channel value as a byte
        */
        std::uint8_t getBlueByte() const;

        /*!
        \brief Returns the alpha channel value as a byte
        */
        std::uint8_t getAlphaByte() const;

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
        std::int32_t getPacked() const;

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

        float* asArray() { return reinterpret_cast<float*>(this); }

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
}