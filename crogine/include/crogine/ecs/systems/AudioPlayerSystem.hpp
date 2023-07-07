/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/ecs/System.hpp>

namespace cro
{
    /*!
    \brief Processes the Scene's AudioEmitter components
    without 3D spatialisation.
    Because the audio subsystem at a hardware level only has a
    single listener point when using multiple scenes (for
    example a second 2D scene to render a UI) only one of
    these scenes should have an AudioSystem active within it.
    Using this system with secondary scenes such as those playing
    music or rendering a UI will play unpositioned audio without
    interfering with positional audio in other scenes.
    \see AudioSystem
    */
    class CRO_EXPORT_API AudioPlayerSystem final : public System
    {
    public:
        explicit AudioPlayerSystem(MessageBus&);

        void process(float) override;

    private:

    };
}
