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

#define glCheck(x) x

namespace
{
    const std::array<std::string, 11u> TipStrings =
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
        "Loading...\n\nTip: You can assign your gear upgrades in the Profile Editor"
    };
    std::size_t stringIndex = cro::Util::Random::value(0u, TipStrings.size() - 1);

    constexpr float ProgressHeight = 10.f;
    constexpr float timestep = 1.f / 60.f;

}

LoadingScreen::LoadingScreen(SharedStateData& sd)
    :m_sharedData       (sd),
    m_previousProgress  (0.f),
    m_targetProgress    (0.f),
    m_progressScale     (0.f)
{
    //if (!m_loadingTexture.loadFromFile("assets/images/loading02.png"))
    //{
    //    cro::Image img;
    //    img.create(12, 12, cro::Colour::Magenta);
    //    m_loadingTexture.loadFromImage(img);
    //}

    //m_loadingQuad.setTexture(loadingTexture);
    //m_loadingQuad.setOrigin(glm::vec2(m_loadingTexture.getSize()) / 2.f);

    cro::Image img;
    img.create(1, 1, CD32::Colours[CD32::BeigeLight]);
    m_progressTexture.loadFromImage(img);
    m_progressTexture.setBorderColour(cro::Colour::Black);
    m_progressBar.setTexture(m_progressTexture);

    //fonts not loaded yet, so deferred to launch();
    //m_tipText.setFont(sd.sharedResources->fonts.get(FontID::Info));
    m_tipText.setAlignment(cro::SimpleText::Alignment::Centre);
    m_tipText.setCharacterSize(InfoTextSize);
    m_tipText.setFillColour(TextNormalColour);
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

    stringIndex = (stringIndex + 1) % TipStrings.size();
    m_tipText.setString(TipStrings[stringIndex]);
    m_tipText.setPosition({ std::round(screenSize.x / 2.f), 56.f * viewScale});
    m_tipText.setScale({ viewScale, viewScale });

    const auto& loadingTexture = m_sharedData.sharedResources->textures.get("assets/images/loading02.png");

    m_loadingQuad.setTexture(loadingTexture);
    m_loadingQuad.setOrigin(glm::vec2(loadingTexture.getSize()) / 2.f);
    m_loadingQuad.setPosition(screenSize / 2.f);
    m_loadingQuad.setScale({ viewScale, viewScale });

    m_progressBar.setScale({ 0.f, 0.f });
    m_progressBar.setPosition({ 0.f, 4.f * viewScale });

    
    m_previousProgress = 0.f;
    m_progressScale = 0.f;

    auto& window = cro::App::getWindow();
    const auto size = window.getSize().x;
    m_progressBar.setScale({ 0.f, 0.f });

    window.clear();
    draw();
    window.display();
}

void LoadingScreen::update()
{
    static std::int32_t updateCount = 0;
    
    static float accumulator = 0.f;
    accumulator += m_clock.restart();
    
    auto& window = cro::App::getWindow();
    auto old = window.getVsyncEnabled();
    window.setVsyncEnabled(false); //this causes the loading screen to wait otherwise

    const float stepCount = std::floor(accumulator / timestep);
    const float progressSize = (m_targetProgress - m_previousProgress) / stepCount;

    while (accumulator > timestep)
    {
        updateCount++;

        const glm::vec2 windowSize = glm::vec2(cro::App::getWindow().getSize());
        const auto scale = getViewScale(windowSize);

        m_progressScale += progressSize;
        m_progressBar.setScale({ m_progressScale * windowSize.x, scale * ProgressHeight });

        m_loadingQuad.setRotation(90.f * (updateCount % 4));

        accumulator -= timestep;


        //we must pump the queue during loading else we timeout
        if (m_sharedData.clientConnection.connected)
        {
            net::NetEvent evt;
            while (m_sharedData.clientConnection.netClient.pollEvent(evt))
            {
                //HOWEVER we want to ignore irrelevant packets else this
                //buffer can become incredibly LARGE before we get around
                //to handling it in out destination state.
                switch (evt.packet.getID())
                {
                default:
                    m_sharedData.clientConnection.eventBuffer.emplace_back(std::move(evt));
                    break;
                case PacketID::ActorUpdate:
                case PacketID::WindDirection:
                case PacketID::DronePosition:
                case PacketID::ClubChanged:
                case PacketID::PingTime:
                    //skip these while loading it just fills up the buffer
                    break;
                }
                evt = {}; //not strictly necessary but squashes warning about re-using a moved object
            }
        }

        if (m_sharedData.voiceConnection.connected)
        {
            //this NEEDS to be done to stop the voice channels backlogging the
            //connection - however it means voice will cut out until the loading
            //screen has exited - we ideally want to be able to pass it to a decoder
            //immediately (although there'll be no active audio source anyway...)
            net::NetEvent evt;
            while (m_sharedData.voiceConnection.netClient.pollEvent(evt)) {}
        }

        window.clear();
        draw();
        window.display();
    }
    window.setVsyncEnabled(old);
}

void LoadingScreen::draw()
{
    m_tipText.draw();
    m_progressBar.draw();
    m_loadingQuad.draw();
}

void LoadingScreen::setProgress(float p)
{
    auto& window = cro::App::getWindow();

    m_targetProgress = p;

    if (p == 0)
    {
        m_previousProgress = 0.f;
        m_progressScale = 0.f;

        const auto size = window.getSize().x;
        m_progressBar.setScale({ 0.f, 0.f });
    }

    window.clear();
    draw();
    window.display();
    
    //this also draws!!
    update();
}