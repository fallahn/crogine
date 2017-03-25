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

#include <crogine/graphics/Texture.hpp>

using namespace cro;

Texture::Texture()
    : m_format  (Graphics::None),
    m_handle    (0)
{

}

Texture::~Texture()
{

}

//public
void Texture::create(uint32 width, uint32 height, Graphics::Format format)
{

}

bool Texture::loadFromFile(const std::string& path)
{
    return false;
}

bool Texture::update(const uint8* pixels, URect area)
{
    return false;
}

glm::uvec2 Texture::getSize() const
{
    return m_size;
}

Graphics::Format Texture::getFormat() const
{
    return m_format;
}

uint32 Texture::getGLHandle() const
{
    return m_handle;
}