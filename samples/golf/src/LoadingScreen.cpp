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
#include "golf/GameConsts.hpp"
#include "golf/SharedStateData.hpp"
#include "golf/PacketIDs.hpp"

#include <Social.hpp>

#include <crogine/core/App.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/util/Wavetable.hpp>

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>
#include <array>

#define glCheck(x) x

namespace
{
    const std::array<std::string, 8u> TipStrings =
    {
        std::string("Tip: Click on an opponent's name in the League Browser to change it"),
        "Did You Know: Before golf tees players would shape mounds of sand and place the golf ball on top",
        "Tip: You can find alternative control schemes such as Swingput in the Options menu",
        "Did You Know: Apollo 14 astronaut Alan Shepard used a Wilson 6-iron to play golf on the moon",
        "Tip: The clubset you choose affects the CPU opponent's difficulty",
        "Did You Know: An estimated 600 million golf balls are lost or discarded every year\nmore than 100,000 of which are at the bottom of Loch Ness!",
        "Tip: Make sure to spend some time on the Driving Range, to really learn your clubs",
        "Did You Know: There are playable arcade games waiting to be discovered in the Clubhouse"
    };
    std::size_t stringIndex = 7;

    constexpr std::uint32_t vertexSize = 2 * sizeof(float);
    constexpr float timestep = 1.f / 60.f;

    const std::string vertex = R"(
        ATTRIBUTE vec4 a_position;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_projectionMatrix;

        VARYING_OUT vec2 v_texCoord;

        void main()
        {
            gl_Position = u_projectionMatrix * u_worldMatrix * a_position;
            v_texCoord = a_position.xy;
        }
    )";

    const std::string fragment = R"(
        uniform sampler2D u_texture;
        uniform float u_frameNumber;
        
        VARYING_IN vec2 v_texCoord;
        OUTPUT

        const float FrameHeight = 1.0/8.0;

        void main()
        {
            vec2 texCoord = v_texCoord;
            texCoord.y *= FrameHeight;
            texCoord.y += 1.0 - (u_frameNumber * FrameHeight);

            FRAG_OUT = TEXTURE(u_texture, texCoord);
        }
    )";
}

LoadingScreen::LoadingScreen(SharedStateData& sd)
    :m_sharedData       (sd),
    m_vao               (0),
    m_vbo               (0),
    m_projectionIndex   (-1),
    m_transformIndex    (-1),
    m_frameIndex        (-1),
    m_currentFrame      (0),
    m_transform         (1.f),
    m_projectionMatrix  (1.f),
    m_viewport          (cro::App::getWindow().getSize())
{
    if (!m_texture.loadFromFile("assets/images/loading.png"))
    {
        cro::Image img;
        img.create(12, 12, cro::Colour::Magenta);
        m_texture.loadFromImage(img);
    }
    m_texture.setRepeated(true);

    if (m_shader.loadFromString(vertex, fragment))
    {
        m_transformIndex = m_shader.getUniformID("u_worldMatrix");
        m_projectionIndex = m_shader.getUniformID("u_projectionMatrix");
        m_frameIndex = m_shader.getUniformID("u_frameNumber");

        glCheck(glUseProgram(m_shader.getGLHandle()));
        glCheck(glUniform1i(m_shader.getUniformID("u_texture"), 0));
        glCheck(glUseProgram(0));

        //create VBO
        //0------2
        //|      |
        //|      |
        //1------3

        std::vector<float> verts =
        {
            0.f, 1.f,
            0.f, 0.f,
            1.f, 1.f,
            1.f, 0.f,
        };
        glCheck(glGenBuffers(1, &m_vbo));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, vertexSize * verts.size(), verts.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }
}

LoadingScreen::~LoadingScreen()
{
    if (m_vbo)
    {
        glCheck(glDeleteBuffers(1, &m_vbo));
    }

    if (m_vao)
    {
        glCheck(glDeleteVertexArrays(1, &m_vao));
    }
}

//public
void LoadingScreen::launch()
{
    WebSock::broadcastPacket(Social::setStatus(Social::InfoID::Menu, { "Loading..." }));

    if (!m_vao)
    {
        CRO_ASSERT(m_vbo, "");

        glCheck(glGenVertexArrays(1, &m_vao));

        glCheck(glBindVertexArray(m_vao));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

        //pos
        glCheck(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vertexSize, (void*)0));
        glCheck(glEnableVertexAttribArray(0));

        glCheck(glBindVertexArray(0));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }

    //this has to be created in the launch thread because VAOs / OpenGL etc etc
    if (!m_tipText)
    {
        m_font.loadFromFile("assets/golf/fonts/MCPixel.otf");
        m_tipText = std::make_unique<cro::SimpleText>(m_font);
        m_tipText->setAlignment(cro::SimpleText::Alignment::Centre);
        m_tipText->setCharacterSize(InfoTextSize);
        m_tipText->setFillColour(TextNormalColour);
    }

    stringIndex = (stringIndex + 1) % TipStrings.size();
    m_tipText->setString(TipStrings[stringIndex]);
}

void LoadingScreen::update()
{
    static float accumulator = 0.f;
    accumulator += m_clock.restart().asSeconds();
    
    static std::int32_t frameCounter = 0;
    static constexpr std::int32_t MaxFrames = 6;

    while (accumulator > timestep)
    {
        m_viewport = cro::App::getWindow().getSize();
        const glm::vec2 windowSize = glm::vec2(m_viewport);

        const auto scale = glm::vec2(getViewScale(windowSize));
        m_tipText->setPosition({ std::round(windowSize.x / 2.f), 36.f });
        m_tipText->setScale(scale);

        m_projectionMatrix = glm::ortho(0.f, windowSize.x, 0.f, windowSize.y, -0.1f, 10.f);

        float texSize = static_cast<float>(m_texture.getSize().x) * scale.x;
        m_transform = glm::translate(glm::mat4(1.f), { (windowSize.x - texSize) / 2.f, (windowSize.y - texSize) / 2.f, 0.f });
        m_transform = glm::scale(m_transform, { texSize, texSize, 1.f });

        frameCounter = (frameCounter + 1) % MaxFrames;
        if (frameCounter == 0)
        {
            m_currentFrame = (m_currentFrame + 1) % FrameCount;
        }
        
        glCheck(glUseProgram(m_shader.getGLHandle()));
        glCheck(glUniformMatrix4fv(m_projectionIndex, 1, GL_FALSE, glm::value_ptr(m_projectionMatrix)));
        glCheck(glUniformMatrix4fv(m_transformIndex, 1, GL_FALSE, glm::value_ptr(m_transform)));
        glCheck(glUniform1f(m_frameIndex, static_cast<float>(m_currentFrame)));

        accumulator -= timestep;

        if (m_sharedData.clientConnection.connected)
        {
            net::NetEvent evt;
            while (m_sharedData.clientConnection.netClient.pollEvent(evt))
            {
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
    }
}

void LoadingScreen::draw()
{
    m_tipText->draw();

    std::int32_t oldView[4];
    glCheck(glGetIntegerv(GL_VIEWPORT, oldView));
    glCheck(glEnable(GL_BLEND));
    glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glCheck(glBlendEquation(GL_FUNC_ADD));

    glCheck(glViewport(0, 0, m_viewport.x, m_viewport.y));
    glCheck(glUseProgram(m_shader.getGLHandle()));

    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture.getGLHandle()));

    glCheck(glBindVertexArray(m_vao));
    glCheck(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

    glCheck(glUseProgram(0));

    glCheck(glViewport(oldView[0], oldView[1], oldView[2], oldView[3]));
    glCheck(glDisable(GL_BLEND));
}