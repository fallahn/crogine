/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "ProgressIcon.hpp"

#include <MonthlyChallenge.hpp>

#include <crogine/core/App.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/util/String.hpp>

namespace
{
    constexpr glm::vec2 IconSize(284.f, 90.f);
    constexpr float BandHeight = 20.f;
    constexpr glm::vec2 BarPos(8.f, BandHeight + 4.f);
    constexpr glm::vec2 BarSize(IconSize.x - (BarPos.x * 2.f), 8.f);
    constexpr float PauseTime = 5.f;

    constexpr cro::Colour BackgroundColourDark(0x161b22ff);
    constexpr cro::Colour BackgroundColourLight(0x1f242aff);
    constexpr cro::Colour GradientLight(0x3a9ceeff);
    constexpr cro::Colour GradientDark(0x235dcfff);
    constexpr cro::Colour GradientBackground(0x969696ff);
}

ProgressIcon::ProgressIcon(const cro::Font& font)
    : m_active  (false),
    m_state     (ScrollIn),
    m_pauseTime (0.f)
{
    //assemble parts
    m_vertexData = 
    {
        //background
        cro::Vertex2D(glm::vec2(0.f, IconSize.y), BackgroundColourDark),
        cro::Vertex2D(glm::vec2(0.f), BackgroundColourDark),
        cro::Vertex2D(IconSize, BackgroundColourDark),
            
        cro::Vertex2D(IconSize, BackgroundColourDark),
        cro::Vertex2D(glm::vec2(0.f), BackgroundColourDark),
        cro::Vertex2D(glm::vec2(IconSize.x, 0.f), BackgroundColourDark),


        cro::Vertex2D(glm::vec2(1.f, IconSize.y - 1.f), BackgroundColourLight),
        cro::Vertex2D(glm::vec2(1.f, BandHeight + 1.f), cro::Colour::Transparent),
        cro::Vertex2D(IconSize - glm::vec2(1.f), BackgroundColourLight),

        cro::Vertex2D(IconSize - glm::vec2(1.f), BackgroundColourLight),
        cro::Vertex2D(glm::vec2(1.f, BandHeight + 1.f), cro::Colour::Transparent),
        cro::Vertex2D(glm::vec2(IconSize.x - 1.f, BandHeight * 2.f), cro::Colour::Transparent),



        //band
        cro::Vertex2D(glm::vec2(0.f, BandHeight), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(IconSize.x, BandHeight), cro::Colour::Black),
            
        cro::Vertex2D(glm::vec2(IconSize.x, BandHeight), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(IconSize.x, 0.f), cro::Colour::Black),

        //progress bar
        cro::Vertex2D(glm::vec2(BarPos.x, BarPos.y + BarSize.y), GradientBackground),
        cro::Vertex2D(BarPos, GradientBackground),
        cro::Vertex2D(glm::vec2(BarPos.x + BarSize.x, BarPos.y + BarSize.y), GradientBackground),
            
        cro::Vertex2D(glm::vec2(BarPos.x + BarSize.x, BarPos.y + BarSize.y), GradientBackground),
        cro::Vertex2D(BarPos, GradientBackground),
        cro::Vertex2D(glm::vec2(BarPos.x + BarSize.x, BarPos.y), GradientBackground),


        cro::Vertex2D(glm::vec2(BarPos.x, BarPos.y + BarSize.y), GradientLight),
        cro::Vertex2D(BarPos, GradientLight),
        cro::Vertex2D(glm::vec2(BarPos.x + BarSize.x, BarPos.y + BarSize.y), GradientDark),

        cro::Vertex2D(BarPos, GradientLight),
        cro::Vertex2D(glm::vec2(BarPos.x + BarSize.x, BarPos.y), GradientDark),
        cro::Vertex2D(glm::vec2(BarPos.x + BarSize.x, BarPos.y + BarSize.y), GradientDark),
    };
    m_background.setVertexData(m_vertexData);
    m_background.setPrimitiveType(GL_TRIANGLES);

    m_text.setFont(font);
    m_text.setCharacterSize(16);
    m_text.setPosition({ std::floor(IconSize.x / 2.f), 60.f });
    m_text.setAlignment(cro::SimpleText::Alignment::Centre);
    m_text.setVerticalSpacing(1.f);

    m_titleText.setFont(font);
    m_titleText.setCharacterSize(16);
    m_titleText.setString("CHALLENGE PROGRESS");
    m_titleText.setPosition({ std::floor(IconSize.x / 2.f), 6.f });
    m_titleText.setAlignment(cro::SimpleText::Alignment::Centre);

    setPosition({ 0.f, -IconSize.y });
}

//public
void ProgressIcon::show(std::int32_t index, std::int32_t progress, std::int32_t total)
{
    //rare, but we don't want, say, a league notification clobbering an active challenge
    if (!m_active)
    {
        //update progress bar verts
        float pc = static_cast<float>(progress) / total;
        auto idx = m_vertexData.size() - 1;
        const auto pos = (BarSize.x * pc) + BarPos.x;
        m_vertexData[idx].position.x = pos;
        m_vertexData[idx - 1].position.x = pos;
        m_vertexData[idx - 3].position.x = pos;
        m_background.setVertexData(m_vertexData);

        //update text - TODO this should be enumerated proper, like
        if (index == -1)
        {
            m_titleText.setString("CLUB LEAGUE UPDATED");

            if (progress == total)
            {
                m_text.setString("Season Complete!");
            }
            else
            {
                auto str = std::to_string(progress) + "/" + std::to_string(total);
                m_text.setString("Season Progress " + str);
            }
        }
        else if (index == -2)
        {
            m_titleText.setString("COME RAIN OR SHINE");
            auto str = std::to_string(progress) + "/" + std::to_string(total);
            m_text.setString("Progress " + str);
        }
        else
        {
            auto str = cro::Util::String::wordWrap(ChallengeDescriptions[index], 40, 128);
            m_text.setString(str);

            if (progress == total)
            {
                m_titleText.setString("CHALLENGE COMPLETE! 500XP");
            }
            else
            {
                str = std::to_string(progress) + "/" + std::to_string(total);
                m_titleText.setString("CHALLENGE PROGRESS " + str);
            }
        }

        m_active = true;
        m_state = ScrollIn;
        m_pauseTime = PauseTime;

        const auto windowSize = glm::vec2(cro::App::getWindow().getSize());
        setPosition({ windowSize.x - IconSize.x, windowSize.y });
    }
}

//public
void ProgressIcon::update(float dt)
{
    static const float Speed = 240.f;
    if (m_active)
    {
        const auto windowSize = glm::vec2(cro::App::getWindow().getSize());
        if (m_state == ScrollIn)
        {
            move({ 0.f, -Speed * dt });
            if (getPosition().y < windowSize.y - IconSize.y)
            {
                setPosition({ windowSize.x - IconSize.x, windowSize.y - IconSize.y });
                m_state = Paused;
            }
        }
        else if (m_state == Paused)
        {
            m_pauseTime -= dt;
            if (m_pauseTime < 0)
            {
                m_state = ScrollOut;
            }
        }
        else
        {
            move({ 0.f, Speed * dt });
            if (getPosition().y > windowSize.y)
            {
                m_active = false;
            }
        }
    }
}

void ProgressIcon::draw()
{
    if (m_active)
    {
        auto tx = getTransform();

        m_background.draw(tx);

        m_text.draw(tx);
        m_titleText.draw(tx);
    }
}