/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "NetConf.hpp"

#include <iostream>

using namespace gns;

std::unique_ptr<NetConf> NetConf::instance;

NetConf::NetConf()
    : m_initOK(false)
{
#ifdef GNS_OS
    SteamDatagramErrMsg err;
    if (m_initOK = GameNetworkingSockets_Init(nullptr, err); !m_initOK)
    {
        std::cerr << "ERROR: Failed init GNS: " << err << "\n";
    }
#else
    std::cout << __FILE__ << "Steam networking goes here.\n"
#endif
}

NetConf::~NetConf()
{
    if (m_initOK)
    {
#ifdef GNS_OS
        GameNetworkingSockets_Kill();
#else

#endif
    }
}