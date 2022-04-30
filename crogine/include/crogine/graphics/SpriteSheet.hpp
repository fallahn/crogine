/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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
#include <crogine/ecs/components/Sprite.hpp>

#include <unordered_map>
#include <string>

namespace cro
{
    class TextureResource;

    /*!
    \brief Supports loading multiple sprites from a single
    texture atlas via the ConfigFile format.
    */
    class CRO_EXPORT_API SpriteSheet final
    {
    public:
        SpriteSheet();

        /*!
        \brief Attempts to load a ConfigFile from the given path.
        A reference to a valid texture resource is required to load
        the sprite sheet texture.
        \returns true if successful, else false
        */
        bool loadFromFile(const std::string& path, TextureResource& rx, const std::string& workingDirectory = "");

        /*!
        \brief Writes the contents of the SpriteSheet to a sprite sheet configuration file
        \param path A string containing the path to which to attempt to write the file
        \returns true on success, else false. Error messages are printed to the console
        */
        bool saveToFile(const std::string& path);

        /*!
        \brief Returns a sprite component with the given name as it
        appears in the sprite sheet. If the sprite does not exist an
        empty sprite is returned.
        */
        Sprite getSprite(const std::string& name) const;

        /*!
        \brief Returns the index of the animation with the given name
        on the given sprite if it exists, else returns 0
        */
        std::int32_t getAnimationIndex(const std::string& name, const std::string& sprite) const;

        /*!
        \brief Returns true if the given animation exists on the sprite with the give name
        */
        bool hasAnimation(const std::string& name, const std::string& sprite) const;

        /*!
        \brief Returns the Sprite components defined in the spritesheet, mapped to their name
        */
        const std::unordered_map<std::string, Sprite>& getSprites() const { return m_sprites; }
        std::unordered_map<std::string, Sprite>& getSprites() { return m_sprites; }

        /*!
        \brief Returns the path to the image loaded for the sprite sheet's texture
        */
        const std::string getTexturePath() const { return m_texturePath; }

        /*!
        \brief Returns a pointer to the texture used by this Sprite Sheet if it is loaded
        */
        const cro::Texture* getTexture() const { return m_texture; }

        /*!
        \brief Sets the new texture and texture path of the SpriteSheet overwriting the
        existing one.
        \param path Relative path to the texture to use
        \param textureResource Reference to an active texture resource
        */
        void setTexture(const std::string& path, TextureResource& textureResource, const std::string& workingDirectory = "");

        /*!
        \brief Adds the sprite with the given name, or does nothing if it already exists
        */
        void addSprite(const std::string& spriteName);

    private:
        mutable std::unordered_map<std::string, Sprite> m_sprites;
        mutable std::unordered_map<std::string, std::vector<std::string>> m_animations;
        std::string m_texturePath;
        const cro::Texture* m_texture;
    };
}