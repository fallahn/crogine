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
#include <crogine/detail/SDLResource.hpp>

namespace cro
{
    /*!
    \brief Loading Screen interface.
    Loading screens are displayed by the window class when loading resources.
    A loading screen can be used to reasonably draw anything - but bear in mind
    loading screen resources themselves are not loaded in a thread and will block
    so loading as few resources as possible is preferred. Some mobile devices
    do not support running a separate thread for a loading screen, so instead only
    the first frame will be drawn.
    */
    class CRO_EXPORT_API LoadingScreen : public Detail::SDLResource
    {
    public:
        LoadingScreen() = default;
        virtual ~LoadingScreen() = default;
        LoadingScreen(const LoadingScreen&) = delete;
        LoadingScreen(LoadingScreen&&) = delete;
        const LoadingScreen& operator = (const LoadingScreen&) = delete;
        LoadingScreen& operator = (LoadingScreen&&) = delete;

        /*!
        \brief This is required to create a valid VAO for the thread
        context, and called when the thread is launched. It is up to the
        user to make sure any VAO resources are released properly
        */
        virtual void launch() = 0;

        /*!
        \brief updates the loading screen.
        This is called by the thread as often as possible, so it's
        up to concrete instances to provide any needed timing
        */
        virtual void update() = 0;

        /*!
        \brief Draws the loading screen
        */
        virtual void draw() = 0;

        /*!
        \brief Optional overload to set the progress bar when manually updating
        \param prog Normalised value representing total progress
        */
        virtual void setProgress(float prog) {}

    private:

    };
}