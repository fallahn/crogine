/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2025
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

#include <crogine/audio/sound_system/MusicPlayer.hpp>

#include <crogine/graphics/SimpleDrawable.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/Transformable2D.hpp>

#include <deque>

struct ProgressMessage final
{
    enum
    {
        Challenge, League, Message,
        AchievementProgress
    }type = Message;

    std::int32_t index = 0;
    std::int32_t progress = 0;
    std::int32_t total = 0;

    std::string title;
    std::string message;

    enum
    {
        None,
        ScreenShot, Reward
    }audioID = None;
};

class ProgressIcon final : public cro::Transformable2D
{
public:
    explicit ProgressIcon(const cro::Font&);

    void queueMessage(const ProgressMessage&);
    void update(float);
    void draw();

private:
    bool m_active;
    cro::SimpleVertexArray m_background;
    std::vector<cro::Vertex2D> m_vertexData;

    cro::SimpleText m_text;
    cro::SimpleText m_titleText;

    std::deque<ProgressMessage> m_messageQueue;

    cro::MusicPlayer m_rewardEffect;

    enum
    {
        ScrollIn, Paused, ScrollOut
    }m_state;
    float m_pauseTime;

    void showChallenge(std::int32_t index, std::int32_t progress, std::int32_t total);
    void showAchievement(std::int32_t index, std::int32_t progress, std::int32_t total);
    void showLeague(std::int32_t index, std::int32_t progress, std::int32_t total);
    void showMessage(const std::string& title, const std::string& msg);
};