/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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
#include <crogine/ecs/Director.hpp>
#include <crogine/ecs/Entity.hpp>

#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/graphics/TextureResource.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <vector>
#include <optional>
#include <functional>

/*
\brief Fires a requested particle effect at a given position in the Scene.
*/

struct ParticleEvent final
{
    std::size_t id = 0;
    glm::vec3 position = glm::vec3(0.f);
    glm::vec2 velocity = glm::vec2(0.f);
};

class ParticleDirector final : public cro::Director 
{
public:
    /*!
    \brief Handles messages forwarded by handleMessage()
    In order to customise which particles are fired by a particular event
    a custom message handler should be supplied to the ParticleDirector
    via setMessageHandler().
    The handler returns a std::optional<std::pair<std::size_t, glm::vec3>>,
    or std::nullopt as a default. The std::pair contains the index of the
    loaded ParticleEmitter to fire, and a glm::vec3 containing the world
    coordinates at which to start the emitter.

    The indices for ParticleEmitter settings are returned by loadSettings()

    \begincode
    std::array indices = 
    {
        director.loadSettings("explosion.xyp),
        director.loadSettings("sparks.xyp)
    };

    auto handler = [indices](const cro::Message& msg)->std::optional<std::pair<std::size_t, glm::vec3>>
    {
        if(msg.id == MessageID::SomeEffect)
        {
            const auto& evt = msg.getData<EffectEvent>();
            if(evt.type == Explosion)
            {
                return std::make_pair(indices[0], evt.position);
            }
            else if(evt.type == Spark)
            {
                return std::make_pair(indices[1], evt.position);
            }
        }

        return std::nullopt
    };

    director.setMessageHandler(handler);
    \endcode
    */

    using MessageHandler = std::function<std::optional<ParticleEvent>(const cro::Message&)>;

    explicit ParticleDirector(cro::TextureResource&);

    void handleMessage(const cro::Message&) override;

    void handleEvent(const cro::Event&) override;

    void process(float) override;

    std::size_t getBufferSize() const { return m_emitters.size(); }

    //returns the index of the loaded settings
    //note settings may not successfully load from file
    std::size_t loadSettings(const std::string&); 

    //registers a message handler
    void setMessageHandler(MessageHandler mh) { m_messageHandler = mh; }

private:

    cro::TextureResource& m_textures;
    std::vector<cro::EmitterSettings> m_emitterSettings;

    std::size_t m_nextFreeEmitter;
    std::vector<cro::Entity> m_emitters;

    MessageHandler m_messageHandler;

    void resizeEmitters();
    cro::Entity getNextEntity();
};