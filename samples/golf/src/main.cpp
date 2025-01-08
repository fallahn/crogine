/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include "WebsocketServer.hpp"

#include <SDL.h>

#include "GolfGame.hpp"

#include <iostream>


int main(int argc, char** argsv)
{
    bool safeMode = false;

    GolfGame game;
    if (argc > 1)
    {
        std::string str(argsv[1]);
        safeMode = (str == "safe_mode");

#ifdef _WIN32
        AllocConsole();
#endif

    }
    WebSock::start(8080);
    game.setSafeModeEnabled(safeMode);
    game.run(safeMode);
    WebSock::stop();

#ifdef _WIN32
    if (safeMode)
    {
        FreeConsole();
    }
#endif

    return 0;
}