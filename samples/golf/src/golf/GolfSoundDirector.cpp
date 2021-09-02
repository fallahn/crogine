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

#include "GolfSoundDirector.hpp"
#include "MessageIDs.hpp"

#include <crogine/audio/AudioResource.hpp>

#include <crogine/ecs/components/AudioEmitter.hpp>

GolfSoundDirector::GolfSoundDirector(cro::AudioResource& ar)
{
    //this must match with AudioID enum
    static const std::array<std::string, AudioID::Count> FilePaths =
    {
        "assets/golf/sound/ball/putt01.wav",
        "assets/golf/sound/ball/putt02.wav",
        "assets/golf/sound/ball/putt03.wav",
        
        "assets/golf/sound/ball/swing01.wav",
        "assets/golf/sound/ball/swing02.wav",
        "assets/golf/sound/ball/swing03.wav",
    };

    std::fill(m_audioSources.begin(), m_audioSources.end(), nullptr);
    for (auto i = 0; i < AudioID::Count; ++i)
    {
        auto id = ar.load(FilePaths[i]); //TODO do we really want to assume none of these are streamed??
        if (id != -1)
        {
            m_audioSources[i] = &ar.get(id);
        }
    }
}

//public
void GolfSoundDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::SpriteAnimationMessage)
    {
        const auto& data = msg.getData<cro::Message::SpriteAnimationEvent>();
        if (data.userType == 0)
        {
            //LogI << "THWOCK!" << std::endl;
        }
    }
}