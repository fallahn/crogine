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

    const auto& objs = sheetFile.getObjects();
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

    const auto& spriteObjs = sheetFile.getObjects();
    for (const auto& spr : spriteObjs)
    {
        if (spr.getName() != "sprite"
            || spr.getId().empty())
        {
            if (spr.getId().empty()) Logger::log("Sprite has no ID in " + path, Logger::Type::Error);
            continue;
        }

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
            if (mode == "add") spriteComponent.setBlendMode(Material::BlendMode::Additive);
            else if (mode == "multiply") spriteComponent.setBlendMode(Material::BlendMode::Multiply);
            else if (mode == "none") spriteComponent.setBlendMode(Material::BlendMode::None);
        }
        else
        {
            spriteComponent.setBlendMode(blendMode);
        }

        if (auto* p = spr.findProperty("bounds"))
        {
            auto bounds = p->getValue<glm::vec4>();
            spriteComponent.setTextureRect({ bounds.x, bounds.y, bounds.z, bounds.w });
        }

        m_sprites.insert(std::make_pair(spriteName, spriteComponent));
        count++;
    }

    LOG("Found " + std::to_string(count) + " sprites in " + path, Logger::Type::Info);
    return count > 0;
}

Sprite SpriteSheet::getSprite(const std::string& name)
{
    if (m_sprites.count(name) != 0)
    {
        return m_sprites[name];
    }
    LOG(name + " not found in sprite sheet", Logger::Type::Warning);
    return {};
}