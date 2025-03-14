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

#include <crogine/core/ConfigFile.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/TextureResource.hpp>

using namespace cro;

SpriteSheet::SpriteSheet()
    : m_texture (nullptr)
{

}

//public
bool SpriteSheet::loadFromFile(const std::string& path, TextureResource& textures, const std::string& workingDirectory)
{
    ConfigFile sheetFile;
    if (!sheetFile.loadFromFile(path))
    {
        return false;
    }

    m_sprites.clear();
    m_animations.clear();
    m_texturePath.clear();
    m_texture = nullptr;

    std::size_t count = 0;

    Texture* texture = nullptr;
    Material::BlendMode blendMode = Material::BlendMode::Alpha;

    //validate sprites, increase count
    if (auto* p = sheetFile.findProperty("src"))
    {
        m_texturePath = p->getValue<std::string>();
        texture = &textures.get(workingDirectory + m_texturePath);
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

    if (auto* p = sheetFile.findProperty("repeat"))
    {
        texture->setRepeated(p->getValue<bool>());
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
                if (sprOb.getName() == "animation"
                    && spriteComponent.m_animations.size() < Sprite::MaxAnimations)
                {
                    auto& animation = spriteComponent.m_animations.emplace_back();

                    std::vector<glm::ivec2> frameEvents;

                    const auto& properties = sprOb.getProperties();
                    for (const auto& p : properties)
                    {
                        std::string name = p.getName();
                        if (name == "frame"
                            && animation.frames.size() < Sprite::MaxFrames)
                        {
                            animation.frames.emplace_back(p.getValue<FloatRect>());
                        }
                        else if (name == "event")
                        {
                            //x is event id, y is the frame number
                            auto evt = p.getValue<glm::vec2>();
                            frameEvents.emplace_back(static_cast<std::int32_t>(evt.x), static_cast<std::int32_t>(evt.y));
                        }
                        else if (name == "framerate")
                        {
                            animation.framerate = p.getValue<float>();
                        }
                        else if (name == "loop")
                        {
                            animation.looped = p.getValue<bool>();
                        }
                        else if (name == "loop_start")
                        {
                            animation.loopStart = p.getValue<std::int32_t>();
                        }
                    }

                    for (const auto& evt : frameEvents)
                    {
                        if (static_cast<std::size_t>(evt.y) < animation.frames.size())
                        {
                            animation.frames[evt.y].event = evt.x;
                        }
                    }

                    auto animId = sprOb.getId();
                    m_animations[spriteName].push_back(animId);
                }
            }

            m_sprites.insert(std::make_pair(spriteName, spriteComponent));
            count++;
        }
    }

    m_texture = texture;

    //LOG("Found " + std::to_string(count) + " sprites in " + path, Logger::Type::Info);
    return count > 0;
}

bool SpriteSheet::saveToFile(const std::string& path)
{
    auto sheetName = FileSystem::getFileName(path);
    sheetName = sheetName.substr(0, sheetName.find_last_of('.'));

    ConfigFile sheetFile("spritesheet", sheetName);
    sheetFile.addProperty("src").setValue(m_texturePath);// ("\"" + m_texturePath + "\"");

    if (m_texture)
    {
        sheetFile.addProperty("smooth").setValue(m_texture->isSmooth());
        sheetFile.addProperty("repeat").setValue(m_texture->isRepeated());
    }
    else
    {
        sheetFile.addProperty("smooth").setValue(false);
        sheetFile.addProperty("repeat").setValue(false);
    }

    for (const auto& [name, sprite] : m_sprites)
    {
        auto sprObj = sheetFile.addObject("sprite", name);
        sprObj->addProperty("bounds").setValue(sprite.getTextureRect());
        sprObj->addProperty("colour").setValue(sprite.getColour());

        auto& anims = sprite.getAnimations();
        for (auto i(0u); i < sprite.getAnimations().size(); i++)
        {
            auto animObj = sprObj->addObject("animation", m_animations[name][i]);
            animObj->addProperty("framerate").setValue(anims[i].framerate);
            animObj->addProperty("loop").setValue(anims[i].looped);
            animObj->addProperty("loop_start").setValue(static_cast<std::int32_t>(anims[i].loopStart));

            const auto& frames = anims[i].frames;
            std::vector<glm::vec2> events;
            for (auto j = 0u; j < anims[i].frames.size(); j++)
            {
                animObj->addProperty("frame").setValue(frames[j].frame);

                if (anims[i].frames[j].event > -1)
                {
                    events.emplace_back(anims[i].frames[j].event, j);
                }
            }

            for (const auto& evt : events)
            {
                animObj->addProperty("event").setValue(evt);
            }
        }
    }

    return sheetFile.save(path);
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

std::int32_t SpriteSheet::getAnimationIndex(const std::string& name, const std::string& spriteName) const
{
    if (m_animations.count(spriteName) != 0)
    {
        const auto& anims = m_animations[spriteName];
        const auto& result = std::find(anims.cbegin(), anims.cend(), name);
        if (result == anims.cend()) return 0;

        return static_cast<std::int32_t>(std::distance(anims.cbegin(), result));
    }
    return 0;
}

bool SpriteSheet::hasAnimation(const std::string& name, const std::string& spriteName) const
{
    if (m_animations.count(spriteName) != 0)
    {
        const auto& anims = m_animations[spriteName];
        const auto& result = std::find(anims.cbegin(), anims.cend(), name);
        return result != anims.cend();
    }
    return false;
}

void SpriteSheet::setTexture(const std::string& path, TextureResource& tr, const std::string& workingDir)
{
    m_texturePath = path;
    m_texture = &tr.get(workingDir + path);

    for (auto& sprite : m_sprites)
    {
        sprite.second.setTexture(*m_texture);
    }
}

bool SpriteSheet::addSprite(const std::string& name)
{
    if (name.empty())
    {
        LogE << "Could not add sprite: name cannot be empty" << std::endl;
        return false;
    }

    if (!m_texture)
    {
        LogE << "Failed adding " << name << ": no texture is set." << std::endl;
        return false;
    }

    if (m_sprites.count(name) == 0)
    {
        m_sprites.insert(std::make_pair(name, Sprite()));
        m_sprites[name].setTexture(*m_texture);
        return true;
    }
    return false;
}