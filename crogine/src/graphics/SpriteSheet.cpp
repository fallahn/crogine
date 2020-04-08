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

#include <crogine/core/ConfigFile.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/TextureResource.hpp>

using namespace cro;

SpriteSheet::SpriteSheet()
{

}

//public
bool SpriteSheet::loadFromFile(const std::string& path, TextureResource& textures)
{
    ConfigFile sheetFile;
    if (!sheetFile.loadFromFile(path))
    {
        return false;
    }

    m_sprites.clear();

    std::size_t count = 0;

    Texture* texture = nullptr;
    Material::BlendMode blendMode = Material::BlendMode::Alpha;

    //validate sprites, increase count
    if (auto* p = sheetFile.findProperty("src"))
    {
        texture = &textures.get(p->getValue<std::string>());
    }
    else
    {
        LOG(sheetFile.getId() + " missing texture property", Logger::Type::Error);
        return false;
    }

    if (auto* p = sheetFile.findProperty("blendmode"))
    {
        std::string mode = p->getValue<std::string>();
        if (mode == "add") blendMode = Material::BlendMode::Additive;
        else if (mode == "multiply") blendMode = Material::BlendMode::Multiply;
        else if (mode == "none") blendMode = Material::BlendMode::None;
    }

    if (auto* p = sheetFile.findProperty("smooth"))
    {
        texture->setSmooth(p->getValue<bool>());
    }

    const auto& sheetObjs = sheetFile.getObjects();
    for (const auto& spr : sheetObjs)
    {
        if (spr.getName() == "sprite")
        {
            std::string spriteName = spr.getId();
            if (m_sprites.count(spriteName) > 0)
            {
                Logger::log(spriteName + " already exists in sprite sheet", Logger::Type::Error);
                continue;
            }

            Sprite spriteComponent;
            spriteComponent.setTexture(*texture);

            if (auto* p = spr.findProperty("blendmode"))
            {
                //override sheet mode
                std::string mode = p->getValue<std::string>();
                if (mode == "add") spriteComponent.m_blendMode = Material::BlendMode::Additive;
                else if (mode == "multiply") spriteComponent.m_blendMode = Material::BlendMode::Multiply;
                else if (mode == "none") spriteComponent.m_blendMode = Material::BlendMode::None;
            }
            else
            {
                spriteComponent.m_blendMode = blendMode;
            }
            spriteComponent.m_overrideBlendMode = true;

            if (auto* p = spr.findProperty("bounds"))
            {
                spriteComponent.setTextureRect(p->getValue<FloatRect>());
            }

            if (auto* p = spr.findProperty("colour"))
            {
                spriteComponent.setColour(p->getValue<Colour>());
            }

            const auto& spriteObjs = spr.getObjects();
            for (const auto& sprOb : spriteObjs)
            {
                if (sprOb.getName() == "animation")
                {
                    auto& animation = spriteComponent.m_animations.emplace_back();

                    const auto& properties = sprOb.getProperties();
                    for (const auto& p : properties)
                    {
                        std::string name = p.getName();
                        if (name == "frame")
                        {
                            //auto& anim = spriteComponent.m_animations[spriteComponent.m_animationCount];
                            animation.frames.emplace_back() = p.getValue<FloatRect>();
                        }
                        else if (name == "framerate")
                        {
                            //spriteComponent.m_animations[spriteComponent.m_animationCount]
                            animation.framerate = p.getValue<float>();
                        }
                        else if (name == "loop")
                        {
                            //spriteComponent.m_animations[spriteComponent.m_animationCount]
                            animation.looped = p.getValue<bool>();
                        }
                    }
                    //spriteComponent.m_animationCount++;
                }
            }

            m_sprites.insert(std::make_pair(spriteName, spriteComponent));
            count++;
        }
    }

    //LOG("Found " + std::to_string(count) + " sprites in " + path, Logger::Type::Info);
    return count > 0;
}

Sprite SpriteSheet::getSprite(const std::string& name) const
{
    if (m_sprites.count(name) != 0)
    {
        return m_sprites[name];
    }
    LOG(name + " not found in sprite sheet", Logger::Type::Warning);
    return {};
}
