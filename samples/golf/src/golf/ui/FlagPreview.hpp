/*-----------------------------------------------------------------------

Matt Marchant 2025 - 2026
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include <crogine/graphics/RenderTexture.hpp>

#include <filesystem>
#include <vector>

//renders available flags into a single atlas for UI previews

class FlagPreview final
{
public:
    FlagPreview();

    //sigh we need to lazy-load this (actually we don't now...)
    void init(const std::filesystem::path&);

    const cro::Texture& getTexure() const { return m_textures[m_textIndex].getTexture(); }
    cro::FloatRect getUV() const;
    glm::vec2 getSize() const;
    std::filesystem::path getPath() const;

    void setIndex(std::int32_t);
    std::int32_t getIndex() const;
    std::int32_t getCount() const;

    void next();
    void prev();
    void setText(std::size_t);

private:
    std::array<cro::RenderTexture, 3u> m_textures;
    std::vector<std::filesystem::path> m_flagPaths;
    std::size_t m_index;
    std::size_t m_textIndex;
};