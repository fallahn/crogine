/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#ifndef TL_BACKGROUND_CONTROLLER_HPP_
#define TL_BACKGROUND_CONTROLLER_HPP_

#include <crogine/ecs/System.hpp>

struct BackgroundComponent final
{
    cro::uint32 shaderID = 0;
    cro::uint32 uniformLocation = 0;
};

class BackgroundController final : public cro::System
{
public:
    enum class Mode
    {
        Scroll, Shake
    };

    explicit BackgroundController(cro::MessageBus&);

    void process(cro::Time) override;

    void setScrollSpeed(float);

    void setMode(Mode);

private:
    glm::vec2 m_offset;
    float m_speed;
    float m_currentSpeed;
    Mode m_currentMode;
    std::size_t m_currentIndex;
};

#endif //TL_BACKGROUND_CONTROLLER_HPP_