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

#include <crogine/graphics/TextureResource.hpp>
#include <crogine/graphics/Image.hpp>

using namespace cro;

namespace
{
    std::uint32_t fallbackID = std::numeric_limits<std::uint32_t>::max();
}

TextureResource::TextureResource()
    : m_fallbackColour(Colour::Magenta)
{

}

//public
bool TextureResource::load(std::uint32_t id, const std::string& path, bool createMipMaps)
{
    if (m_textures.count(id) == 0)
    {
        std::unique_ptr<Texture> tex = std::make_unique<Texture>();
        if (!tex->loadFromFile(path, createMipMaps))
        {
            //loadFromFile() should print error message
            return false;
        }
        m_textures.insert(std::make_pair(id, std::make_pair(path, std::move(tex))));
        return true;
    }
    else
    {
        //return whether or not this ID was assigned to the same path already
        const auto& currentPath = m_textures.at(id).first;
        LogI << "Texture ID " << id << " already assigned to " << currentPath << std::endl;
        return path == currentPath;
    }

    return false;
}

bool TextureResource::loaded(std::uint32_t id) const
{
    return m_textures.count(id) != 0;
}

Texture& TextureResource::get(std::uint32_t id)
{
    if (m_textures.count(id) == 0)
    {
        //find the fallback
        return getFallbackTexture();
    }
    return *m_textures.at(id).second;
}

Texture& TextureResource::getByHandle(std::uint32_t handle)
{
    CRO_ASSERT(handle != 0, "");
    auto result = std::find_if(m_textures.begin(), m_textures.end(), 
        [handle](const auto& p)
        {
            return p.second.second->getGLHandle() == handle;
        });
    if (result != m_textures.end())
    {
        return *result->second.second;
    }
    return getFallbackTexture();
}

Texture& TextureResource::get(const std::string& path, bool useMipMaps)
{
    auto result = std::find_if(m_textures.begin(), m_textures.end(), 
        [&path](const auto& pair)
        {
            return pair.second.first == path;
        });

    if (result == m_textures.end())
    {
        auto tex = std::make_unique<Texture>();
        if (!tex->loadFromFile(path, useMipMaps))
        {
            return getFallbackTexture();
        }

        auto id = fallbackID--;
        m_textures.insert(std::make_pair(id, std::make_pair(path, std::move(tex))));
        return *m_textures.at(id).second;
    }
    return *result->second.second;
}

void TextureResource::setFallbackColour(Colour colour)
{
    m_fallbackColour = colour;
}

Colour TextureResource::getFallbackColour() const
{
    return m_fallbackColour;
}

//provate
Texture& TextureResource::getFallbackTexture()
{
    if (m_fallbackTextures.count(m_fallbackColour) == 0)
    {
        Image img;
        img.create(32, 32, m_fallbackColour);
        std::unique_ptr<Texture> fbTex = std::make_unique<Texture>();
        fbTex->create(32, 32);
        fbTex->update(img.getPixelData());

        m_fallbackTextures.insert(std::make_pair(m_fallbackColour, std::move(fbTex)));
    }
    return *m_fallbackTextures.at(m_fallbackColour);
}