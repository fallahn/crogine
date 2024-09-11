/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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
#include <crogine/detail/glm/vec4.hpp>
#include <crogine/detail/glm/mat4x4.hpp>

#include <type_traits>
#include <algorithm>
#include <array>
#include <limits>

namespace cro
{
    class Box;

    /*!
    \brief Defines a 2D rectangle, starting at the bottom left position with width and height
    */
    template <class T>
    class Rectangle final
    {
    public:
        T left, bottom, width, height;

        constexpr Rectangle(T l = 0, T b = 0, T w = 0, T h = 0)
            : left(l), bottom(b), width(w), height(h) 
        {
            static_assert(std::is_pod<T>::value, "Only PODs allowed");
        }

        constexpr Rectangle(glm::tvec2<T> position, glm::tvec2<T> size)
            : left(position.x), bottom(position.y), width(size.x), height(size.y)
        {
            static_assert(std::is_pod<T>::value, "Only PODs allowed");
        }

        Rectangle(const Box&)
        {
            static_assert(std::is_floating_point<T>::value, "Only available for FloatRect");
        }

        Rectangle& operator = (const Box&)
        {
            static_assert(std::is_floating_point<T>::value, "Only available for FloatRect");
        }

        constexpr Rectangle& operator *= (T f)
        {
            left *= f;
            bottom *= f;
            width *= f;
            height *= f;
            return *this;
        }

        /*!
        \brief Conversion constructor.
        Creates a rectangle of T when the param is of type U
        */
        template <typename U>
        explicit Rectangle(const Rectangle<U>&);

        /*!
        \brief Returns true if the given rectangle intersects this one
        */
        bool constexpr intersects(Rectangle<T>) const;

        /*!
        \brief Returns true if the given rectangle intersects this one
        \param overlap If a rectangle instance is passed in as a second parameter
        then it will contain the overlapping area of the two rectangles, if they
        overlap
        */
        bool constexpr intersects(Rectangle<T>, Rectangle<T>& overlap) const;

        /*!
        \brief Returns true if this rectangle fully contains the given rectangle
        */
        bool constexpr contains(Rectangle<T>) const;

        /*!
        \brief Returns true if this rectangle contains the given point
        */
        bool constexpr contains(glm::vec2) const;

        /*!
        \brief Returns a copy of this rectangle transformed by the given matrix
        */
        Rectangle<T> transform(const glm::mat4&) const;
    };

    /*Some short cuts for concrete types*/
    using IntRect = Rectangle<std::int32_t>;
    using URect = Rectangle<std::uint32_t>;
    using FloatRect = Rectangle<float>;

    template <>
    FloatRect::Rectangle(const Box&);

    template <>
    FloatRect& FloatRect::operator=(const Box&);

    template <typename T>
    Rectangle<T> operator * (const glm::mat4& mat, const Rectangle<T>& rect)
    {
        return rect.transform(mat);
    }

    template <typename T>
    Rectangle<T> operator * (const Rectangle<T>& rect, T scale)
    {
        return { rect.left * scale, rect.bottom * scale, rect.width * scale, rect.height * scale };
    }

#include "Rectangle.inl"
}