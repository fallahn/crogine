/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>

namespace cro
{
    /*!
    \brief Component which contains information about the currently
    playing sprite animation. Requires a SpriteAnimator system in the scene.
    */
    struct CRO_EXPORT_API SpriteAnimation final
    {
        std::int32_t id = -1;
        bool playing = false;
        float currentFrameTime = 0.f;
        float playbackRate = 1.f; //< normalised value multiplied with base frame rate
        std::uint32_t frameID = 0;

        /*!
        \brief Play the animation at the given index if it exists
        */
        void play(std::int32_t index) { id = index; playing = true; }

        /*!
        \brief Pause the playing animation, if there is one
        */
        void pause() { playing = false; }

        /*!
        \brief Stops the current animation if it is playing and
        rewinds it to the first frame if it is playing or paused
        */
        void stop() { playing = false; frameID = 0; }
    };
}