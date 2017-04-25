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

#ifndef CRO_FONT_RESOURCE_HPP_
#define CRO_FONT_RESOURCE_HPP_

#include <crogine/graphics/Font.hpp>

#include <map>
#include <memory>

namespace cro
{
    /*!
    \brief Manages the lifetime of fonts and ensures only a single instance exists
    */
    class CRO_EXPORT_API FontResource final : public Detail::SDLResource
    {
    public:
        FontResource();
        ~FontResource() = default;

        FontResource(const FontResource&) = delete;
        FontResource(FontResource&&) = delete;
        const FontResource& operator = (const FontResource&) = delete;
        FontResource& operator = (FontResource&&) = delete;

        /*!
        \brief Creates a new font and associates it with a given ID if it doesn't
        yet exist, and returns a reference to it.
        By default fonts are not loaded, merely added to the resource.
        The returned font needs to be properly created using Font::loadFromFile()
        or Font::loadFromImage() at least once before it can be used.
        */
        Font& get(uint32 id);

    private:
        std::map<uint32, std::unique_ptr<Font>> m_fonts;
    };
}

#endif //CRO_FONT_RESOURCE_HPP_