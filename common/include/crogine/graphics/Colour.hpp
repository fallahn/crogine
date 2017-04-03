/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef CRO_COLOUR_HPP_
#define CRO_COLOUR_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/Assert.hpp>

namespace cro
{
	/*!
	\brief Struct representing an RGBA colour.
	Colours are stored as normalised values so that
	they may be used to represent greater than 8 bit values
	*/
	class CRO_EXPORT_API Colour final
	{
	public:

		/*!
		\brief Default constructor. Initialises to black.
		*/
		Colour() = default;
		/*!
		\brief Construct the colour from 3 or 4 8-bit values
		*/
		explicit Colour(uint8 red, uint8 green, uint8 blue, uint8 alpha = 255)
			: r(static_cast<float>(red) / 255.f),
			g(static_cast<float>(green) / 255.f),
			b(static_cast<float>(blue) / 255.f),
			a(static_cast<float>(alpha) / 255.f) {}
		/*!
		\brief Constructs the colour from a 32bit bitmask of 4 8-bit values
		in RGBA order
		*/
		explicit Colour(uint32 mask)
			:r(static_cast<float>((mask >> 24) & 0xFF) / 255.f),
			g(static_cast<float>((mask >> 16) & 0xFF) / 255.f),
			b(static_cast<float>((mask >> 8) & 0xFF) / 255.f),
			a(static_cast<float>(mask & 0xFF) / 255.f) {}
		/*!
		\brief Constructs the colour from 3 or 4 normalised values
		*/
		explicit Colour(float red, float green, float blue, float alpha = 1.f)
			: r(red), g(green), b(blue), a(alpha)
		{
			CRO_ASSERT((r >= 0 && r <= 1) && (g >= 0 && g <= 1) && (b >= 0 && b <= 1) && (a >= 0 && a <= 1), "Values must be normalised");
		}

		/*!
		\brief sets the red channel value
		*/
		void setRed(uint8 red)
		{
			r = static_cast<float>(red) / 255.f;
		}
		void setRed(float red)
		{
			CRO_ASSERT(red >= 0 && red <= 1, "Value must be normalised");
			r = red;
		}
		/*!
		\brief sets the green channel value
		*/
		void setGreen(uint8 green)
		{
			g = static_cast<float>(green) / 255.f;
		}
		void setGreen(float green)
		{
			CRO_ASSERT(green >= 0 && green <= 1, "Value must be normalised");
			g = green;
		}
		/*!
		\brief sets the blue channel value
		*/
		void setBlue(uint8 blue)
		{
			b = static_cast<float>(blue) / 255.f;
		}
		void setBlue(float blue)
		{
			CRO_ASSERT(blue >= 0 && blue <= 1, "Value must be normalised");
			b = blue;
		}
		/*!
		\brief sets the alpha channel value
		*/
		void setAlpha(uint8 alpha)
		{
			a = static_cast<float>(alpha) / 255.f;
		}
		void setAlpha(float alpha)
		{
			CRO_ASSERT(alpha >= 0 && alpha <= 1, "Value must be normalised");
			a = alpha;
		}

		/*!
		\brief Returns the red channel value as a byte
		*/
		uint8 getRedByte() const
		{
			return static_cast<uint8>(255.f * r);
		}

		/*!
		\brief Returns the green channel value as a byte
		*/
		uint8 getGreenByte() const
		{
			return static_cast<uint8>(255.f * g);
		}

		/*!
		\brief Returns the blue channel value as a byte
		*/
		uint8 getBlueByte() const
		{
			return static_cast<uint8>(255.f * b);
		}

		/*!
		\brief Returns the alpha channel value as a byte
		*/
		uint8 getAlphaByte() const
		{
			return static_cast<uint8>(255.f * a);
		}

		/*!
		\brief Returns the red channel as a normalised value
		*/
		float getRed() const { return r; }
		/*!
		\brief Returns the green channel as a normalised value
		*/
		float getGreen() const { return g; }
		/*!
		\brief Returns the blue channels as a normalised value
		*/
		float getBlue() const { return b; }
		/*!
		\brief Returns the alpha channel as a normalised value
		*/
		float getAlpha() const { return a; }

		/*!
		\brief Returns the colour as a RGBA packed 32bit integer
		*/
		int32 getPacked() const
		{
			return (getRedByte() << 24 | getGreenByte() << 16 | getBlueByte() << 8 | getAlphaByte());
		}

		static const Colour Red;
		static const Colour Green;
		static const Colour Blue;
		static const Colour Cyan;
		static const Colour Magenta;
		static const Colour Yellow;
		static const Colour Black;
		static const Colour White;
		static const Colour Transparent;

	private:

		float r = 0.f;
		float g = 0.f;
		float b = 0.f;
		float a = 1.f;

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

#endif //CRO_COLOUR_HPP_