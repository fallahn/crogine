/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2025
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

#include "LoadingScreen.hpp"
#include "WebsocketServer.hpp"
#include "Colordome-32.hpp"
#include "golf/GameConsts.hpp"
#include "golf/SharedStateData.hpp"
#include "golf/PacketIDs.hpp"

#include <Social.hpp>

#include <crogine/core/App.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/util/Wavetable.hpp>
#include <crogine/util/Random.hpp>

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>
#include <array>

namespace
{
    const std::array<std::string, 17> TipStrings  =
    {
        std::string("Loading...\n\nTip: Click on an opponent's name in the League Browser to change it"),
        "Loading...\n\nDid You Know: Before golf tees players would shape mounds of sand and place the golf ball on top",
        "Loading...\n\nTip: You can find alternative control schemes such as Swingput in the Options menu",
        "Loading...\n\nDid You Know: Apollo 14 astronaut Alan Shepard used a Wilson 6-iron to play golf on the moon",
        "Loading...\n\nTip: The clubset you choose affects the CPU opponent's difficulty",
        "Loading...\n\nDid You Know: An estimated 600 million golf balls are lost or discarded every year\nmore than 100,000 of which are at the bottom of Loch Ness!",
        "Loading...\n\nTip: Make sure to spend some time on the Driving Range, to really learn your clubs",
        "Loading...\n\nDid You Know: There are playable arcade games waiting to be discovered in the Clubhouse",
        "Loading...\n\nTip: Upgrade your gear at the Clubhouse Equipment Counter",
        "Loading...\n\nDid You Know: You can find a full break down of your stats on the rightmost tab\nof the Options menu",
        "Loading...\n\nTip: You can assign your gear upgrades in the Profile Editor",
        "Loading...\n\nDid You Know: The longest golf hole in the world can be found at Satsuki Golf Club in\nJapan - a whopping 909 yard par 7! Good luck getting a hole in one there!",
        "Loading...\n\nTip: Check out the How To Play guide in the Options menu for a full break-down\nof Super Video Golf's mechanics",
        "Loading...\n\nDid You Know: The chances of making a hole in one in real life is 12,500 to 1.\nThe chances of being struck by lightning in your lifetime is 15,300 to 1!",
        "Loading...\n\nTip: Hitting certain paths and bridges can give you an exaggerated bounce...\nbut be careful what you wish for!",
        "Loading...\n\nDid You Know: There are exactly 336 dimples on a regulation golf ball,\nand they must weigh no more than 45.93 grammes (including logo paint!)",
        "Loading...\n\nTip: You can check out the lie of the green or the fairway ahead at any time!\nUse D-pad Down or Keyboard \"2\" to manually control the drone"
    };
    std::size_t stringIndex = cro::Util::Random::value(0u, TipStrings.size() - 1);
}

LoadingScreen::LoadingScreen(SharedStateData& sd)
    : m_sharedData       (sd)
{
    //fonts not loaded yet, so deferred to launch();
    //m_tipText.setFont(sd.sharedResources->fonts.get(FontID::Info));
    m_tipText.setAlignment(cro::SimpleText::Alignment::Centre);
    m_tipText.setCharacterSize(InfoTextSize);
    m_tipText.setFillColour(TextNormalColour);
    m_tipText.setShadowColour(LeaderboardTextDark);
    m_tipText.setShadowOffset({ 1.f, -1.f });

    const cro::Colour c(0.f, 0.f, 0.f, BackgroundAlpha);
    m_backgroundVerts.setVertexData({
        cro::Vertex2D(glm::vec2(0.f, 1.f), c),
        cro::Vertex2D(glm::vec2(0.f), c),
        cro::Vertex2D(glm::vec2(1.f), c),
        cro::Vertex2D(glm::vec2(1.f, 0.f), c)
        });
}

//public
void LoadingScreen::launch()
{
    WebSock::broadcastPacket(Social::setStatus(Social::InfoID::Menu, { "Loading..." }));

    const glm::vec2 screenSize = cro::App::getWindow().getSize();

    if (!m_tipText.getFont())
    {
        m_tipText.setFont(m_sharedData.sharedResources->fonts.get(FontID::Info));
    }

    const auto viewScale = getViewScale();

    static constexpr float TextHeight = 64.f;
    stringIndex = (stringIndex + 1) % TipStrings.size();
    m_tipText.setString(TipStrings[stringIndex]);
    m_tipText.setPosition({ std::round(screenSize.x / 2.f), TextHeight * viewScale});
    m_tipText.setScale({ viewScale, viewScale });

    m_backgroundVerts.setScale({ screenSize.x, (TextHeight + 28.f) * viewScale });

    const std::array paths =
    {
        "assets/images/loading/01.png",
        "assets/images/loading/02.png",
        "assets/images/loading/03.png",
        "assets/images/loading/04.png",
        "assets/images/loading/05.png",
        "assets/images/loading/06.png",
    };

    auto& loadingTexture = m_sharedData.sharedResources->textures.get(paths[cro::Util::Random::value(0u, paths.size() - 1)]);
    loadingTexture.setBorderColour(cro::Colour::Black);
    loadingTexture.setSmooth(true);
    const auto texSize = glm::vec2(loadingTexture.getSize());

    const float scale = static_cast<float>(screenSize.x) / texSize.x;
    m_loadingQuad.setTexture(loadingTexture);
    m_loadingQuad.setScale({ scale, scale });
    m_loadingQuad.setPosition({ 0.f, std::ceil(screenSize.y - (texSize.y * scale)) / 2.f });
}

void LoadingScreen::update()
{
    
}

void LoadingScreen::draw()
{
    m_loadingQuad.draw();
    m_backgroundVerts.draw();
    m_tipText.draw();
}

void LoadingScreen::setProgress(float)
{

}