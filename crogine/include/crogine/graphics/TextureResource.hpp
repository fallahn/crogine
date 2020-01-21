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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/Colour.hpp>

#include <unordered_map>
#include <string>
#include <memory>

//hash for colours
namespace std
{
    template<>
    struct hash<cro::Colour>
    {
        std::size_t operator()(const cro::Colour& c) const
        {
            return c.getPacked();
        }
    };
}

namespace cro
{
    /*!
    \brief Used to manage the lifetime of textures as well as ensure single instances
    are loaded.
    */
    class CRO_EXPORT_API TextureResource final : public Detail::SDLResource
    {
    public:
        TextureResource();
        ~TextureResource() = default;

        TextureResource(const TextureResource&) = delete;
        TextureResource(TextureResource&&) noexcept = default;
        const TextureResource& operator = (const TextureResource&) = delete;
        TextureResource& operator = (TextureResource&&) noexcept = default;
        
        /*!
        \brief Attempts to load the given texture if not already loaded and
        returns a reference to it.
        If the texture fails to load then a texture containing the current
        fallback colour is returned.
        \param path String containing the path of the image to attempt to load
        \param createMipMaps Attempts to create the default MipMap levels 
        when loading the texture the first time. If the texture is already
        loaded then this has no effect.
        */
        Texture& get(const std::string& path, bool createMipMaps = true);

        /*!
        \brief Sets the current fallback colour.
        If a texture fails to load then a texture filled with the current
        fallback colour is returned. Defaults to magenta.
        */
        void setFallbackColour(Colour);

        /*!
        \brief Returns the current fallback colour.
        \see setFallbackColour()
        */
        Colour getFallbackColour() const;

    private:
        std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
        std::unordered_map<Colour, std::unique_ptr<Texture>> m_fallbackTextures;
        Colour m_fallbackColour;
    };
}