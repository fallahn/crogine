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

#include "AudioDevice.hpp"

#include <crogine/audio/sound_system/AlResource.hpp>

#include <mutex>
#include <memory>

namespace
{
    std::size_t resourceCount = 0;
    std::recursive_mutex mutex;

    std::unique_ptr<cro::Detail::AudioDevice> audioDevice;
}

using namespace cro;

AlResource::AlResource()
{
    std::scoped_lock lock(mutex);

    if (resourceCount == 0)
    {
        //for now we'll rely on a device/context being active
        //when the ECS sudio is initialised by cro

        //audioDevice = std::make_unique<cro::Detail::AudioDevice>();
    }

    resourceCount++;
}

AlResource::~AlResource()
{
    std::scoped_lock lock(mutex);

    resourceCount--;

    if (resourceCount == 0)
    {
        //audioDevice.reset();
    }
}