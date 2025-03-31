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

#include <PersonalBest.hpp>
#include <Content.hpp>

#include <crogine/core/FileSystem.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/detail/SDLResource.hpp>

namespace
{
    const std::string fileName = "leaders.pbst";
}

PersonalBest::PersonalBest()
{

}

//public
void PersonalBest::load()
{
    const auto path = Content::getBaseContentPath() + fileName;
    if (cro::FileSystem::fileExists(path))
    {
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "rb");
        if (file.file)
        {
            SDL_RWread(file.file, m_entries.data(), sizeof(m_entries), 1);
        }
    }
}

void PersonalBest::save() const
{
    const auto path = Content::getBaseContentPath() + fileName;

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "wb");
    if (file.file)
    {
        SDL_RWwrite(file.file, m_entries.data(), sizeof(m_entries), 1);
    }
}

void PersonalBest::insertScore(std::size_t index, std::int32_t score, const std::vector<std::uint8_t>& holeScores)
{
    CRO_ASSERT(index < m_entries.size(), "");
    CRO_ASSERT(!holeScores.empty() && holeScores.size() <= 18, "");

    if (m_entries[index].score == 0 ||
        m_entries[index].score >= score)
    {
        m_entries[index].score = score;

        std::fill(m_entries[index].holeScores.begin(), m_entries[index].holeScores.end(), 0);
        std::copy(holeScores.begin(), holeScores.end(), m_entries[index].holeScores.begin());

        save();
    }
}

void PersonalBest::fetchScore(std::size_t index, std::int32_t& scoreDst, std::vector<std::uint8_t>& holeDst) const
{
    CRO_ASSERT(index < m_entries.size(), "");
    CRO_ASSERT(!holeScores.empty() && holeScores.size() <= 18, "");

    scoreDst = m_entries[index].score;
    std::copy(m_entries[index].holeScores.begin(), m_entries[index].holeScores.begin() + holeDst.size(), holeDst.begin());
}

std::int32_t PersonalBest::getRoundScore(std::size_t idx) const
{
    CRO_ASSERT(idx < m_entries.size(), "");
    return m_entries[idx].score;
}