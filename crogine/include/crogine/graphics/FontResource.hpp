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
        \brief Loads a font and assigns the given ID to it
        \param id ID to assign to the newly loaded font
        \param path Path to the font to attempt to load
        \returns bool True if the font loaded successfully, else false
        */
        bool load(uint32_t id, const std::string& path);

        /*!
        \brief Returns a reference to the font assigned the given ID
        \param id ID of the font to fetch.
        If the given ID does not map to a loaded font a new, empty font
        is returned. This can be then loaded directly using Font::loadFromFile()
        \see FontResource::load()
        */
        Font& get(std::uint32_t id);


        /*!
        \brief Unloads all currently loaded fonts.
        Use with caution!
        */
        void flush();

    private:
        std::map<std::uint32_t, std::unique_ptr<Font>> m_fonts;
    };
}