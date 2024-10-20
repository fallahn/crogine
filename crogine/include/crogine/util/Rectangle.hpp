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

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/graphics/Rectangle.hpp>

namespace cro
{
    namespace Util
    {
        /*!
        \brief Functions for manipulating rectangle types
        */
        namespace Rectangle
        {
            /*!
            \brief Returns true if the second given Rect is fully contained within the first
            */
            template <typename T>
            bool contains(const cro::Rectangle<T>& first, const cro::Rectangle<T>& second)
            {
                if (second.left < first.left) return false;
                if (second.bottom < first.bottom) return false;
                if (second.left + second.width > first.left + first.width) return false;
                if (second.bottom + second.height > first.bottom + first.height) return false;

                return true;
            }

            /*!
            \brief Returns the centre point of a given Rect
            */
            template <typename T>
            glm::tvec2<T> centre(const cro::Rectangle<T>& rect)
            {
                return glm::tvec2<T>(rect.left + (rect.width / 2.f), rect.bottom + (rect.height / 2.f));
            }

            /*!
            \brief Returns a new Rect from the given upper and lower bounds.
            */
            template <typename T>
            cro::Rectangle<T> fromBounds(const glm::tvec2<T>& lower, const glm::tvec2<T>& upper)
            {
                return cro::Rectangle<T>(lower.x, lower.y, upper.x - lower.x, upper.y - lower.y);
            }

            /*!
            \brief Returns the total perimeter of the given rectangle
            */
            template <typename T>
            T getPerimeter(cro::Rectangle<T> rect)
            {
                return 2 * (rect.width + rect.height);
            }

            /*!
            \brief Merges 2 given rectangles and returns the result
            */
            template <typename T>
            cro::Rectangle<T> combine(cro::Rectangle<T> a, cro::Rectangle<T> b)
            {
                cro::Rectangle<T> ret(std::min(a.left, b.left),
                                    std::min(a.bottom, b.bottom),
                                    std::max(a.left + a.width, b.left + b.width),
                                    std::max(a.bottom + a.height, b.bottom + b.height));
                ret.width -= ret.left;
                ret.height -= ret.bottom;

                return ret;
            }
        }
    }
}
