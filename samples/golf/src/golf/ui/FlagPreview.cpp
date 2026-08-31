/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "FlagPreview.hpp"
#include "Content.hpp"
#include "../GameConsts.hpp"

#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/SimpleText.hpp>

namespace
{
    constexpr std::size_t MaxFlags = 32;
    constexpr std::size_t ColCount = 4;
    constexpr float PreviewWidth = 160.f;
    constexpr float PreviewHeight = 120.f;
}

FlagPreview::FlagPreview()
: m_index(0), m_textIndex(0)
{}

void FlagPreview::init(const std::filesystem::path& currPath)
{
    //available flag textures
    const std::filesystem::path flagDir = "assets/golf/images/flags/";
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> mappedFlags;

    auto flags = cro::FileSystem::listFiles(flagDir);

    flags.erase(std::remove_if(flags.begin(), flags.end(),
        [](const std::filesystem::path& f)
        {
            return f.extension() != ".png";
        }), flags.end());

    if (auto pos = std::find_if(flags.begin(), flags.end(), [](const std::filesystem::path& p) { return p.u8string() == u8"flag.png"; });
        pos != flags.end() && pos != flags.begin())
    {
        std::iter_swap(flags.begin(), pos);
    }

    for (const auto& flag : flags)
    {
        mappedFlags.emplace_back(std::make_pair(flagDir, flag));
    }

    const auto userDir = Content::getUserContentPath(Content::UserContent::Flag);
    const auto userFlags = cro::FileSystem::listDirectories(userDir);
    const auto MaxUser = MaxFlags - flags.size();
    for (auto i = 0u; i < MaxUser && i < userFlags.size(); ++i)
    {
        const auto files = cro::FileSystem::listFiles(userDir / userFlags[i]);
        for (auto j = 0u; j < files.size(); ++j)
        {
            //just grab the first png we find
            if (cro::FileSystem::getFileExtension(files[j]) == ".png")
            {
                mappedFlags.emplace_back(std::make_pair(userDir / userFlags[i], files[j]));
                break;
            }
        }
    }

#ifdef USE_GNS
    const auto& paths = Content::getUserItemsPaths(Content::UserContent::Flag);
    for (const auto& p : paths)
    {
        const auto files = cro::FileSystem::listFiles(p);
        for (auto j = 0u; j < files.size(); ++j)
        {
            //just grab the first png we find
            if (cro::FileSystem::getFileExtension(files[j]) == ".png")
            {
                mappedFlags.emplace_back(std::make_pair(std::string(U8PATH_CAST(p)) + "/", files[j]));
                break;
            }
        }
    }
#endif

    const auto RowCount = (mappedFlags.size() / ColCount) + 1;
    m_textures[0].create(ColCount * PreviewWidth, RowCount * PreviewHeight, false);

    //load the flags and render to render target
    std::uint32_t loadedCount = 0;
    cro::Texture tex;
    cro::SimpleQuad quad;

    m_textures[0].clear(cro::Colour::Blue);
    for (const auto& [path, flag] : mappedFlags)
    {
        const auto fullPath = path / flag;
        if (tex.loadFromFile(fullPath))
        {
            //TODO validate texture size
            quad.setTexture(tex);
            //quad.setTextureRect({ 0.f, 0.f, PreviewWidth, PreviewHeight });
            quad.setScale({ 0.5f, 0.5f });
            quad.setPosition({ (loadedCount % ColCount) * PreviewWidth, (loadedCount / ColCount) * PreviewHeight });
            m_flagPaths.push_back(fullPath);

            quad.draw();

            if (flag == cro::FileSystem::getFileName(currPath))
            {
                m_index = loadedCount;
            }
        }

        loadedCount++;

        if (loadedCount == MaxFlags)
        {
            break;
        }
    }
    m_textures[0].display();


    cro::Font font;
    font.loadFromFile("assets/golf/fonts/IBM_CGA.ttf");

    cro::SimpleText text(font);
    text.setCharacterSize(32);
    text.setString("1");
    text.setFillColour(LeaderboardTextDark);

    quad.setTexture(m_textures[0].getTexture());
    quad.setScale(glm::vec2(1.f));
    quad.setPosition(glm::vec2(0.f));

    //render alt versions to preview number
    //would be nice to use an ArrayTexture but GL41
    //only lets us access it via a shader *sigh*
    for (auto i = 1u; i < m_textures.size(); ++i)
    {
        m_textures[i].create(m_textures[0].getSize().x, m_textures[0].getSize().y, false);

        m_textures[i].clear();
        quad.draw();

        for (auto j = 0u; j < m_flagPaths.size(); ++j)
        {
            text.setPosition({ (j % ColCount) * PreviewWidth, (j / ColCount) * PreviewHeight });
            text.move({ 67.f, 44.f });
            text.draw();
        }
        m_textures[i].display();
        text.setFillColour(TextNormalColour);
    }
}

//public
cro::FloatRect FlagPreview::getUV() const
{
    const float left = PreviewWidth * (m_index % ColCount);
    const float bottom = PreviewHeight * (m_index / ColCount);

    return {left, bottom, PreviewWidth, PreviewHeight};
}

glm::vec2 FlagPreview::getSize() const
{
    return { PreviewWidth, PreviewHeight };
}

std::filesystem::path FlagPreview::getPath() const
{
    return m_flagPaths[m_index];
}

void FlagPreview::setIndex(std::int32_t i)
{
    m_index = i % m_flagPaths.size();
}

std::int32_t FlagPreview::getIndex() const
{
    return static_cast<std::int32_t>(m_index);
}

std::int32_t FlagPreview::getCount() const
{
    return static_cast<std::int32_t>(m_flagPaths.size());
}

void FlagPreview::next()
{
    m_index = (m_index + 1) % m_flagPaths.size();
}

void FlagPreview::prev()
{
    m_index = (m_index + (m_flagPaths.size() -1)) % m_flagPaths.size();
}

void FlagPreview::setText(std::size_t index)
{
    m_textIndex = index;
}