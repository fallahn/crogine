/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include <crogine/audio/AudioSource.hpp>

#include <crogine/ecs/components/AudioEmitter.hpp>

using namespace cro;

void AudioSource::addUser(AudioEmitter* emitter) const
{
    CRO_ASSERT(emitter != nullptr, "");
    if (std::find(m_users.begin(), m_users.end(), emitter) == m_users.end())
    {
        m_users.push_back(emitter);
    }
    //LogI << "a user " << emitter << ", " << m_users.size() << std::endl;
}

void AudioSource::removeUser(AudioEmitter* emitter) const
{
    CRO_ASSERT(emitter != nullptr, "");
    m_users.erase(std::remove_if(m_users.begin(), m_users.end(), 
        [emitter](const AudioEmitter* e) 
        {
            return e == emitter;
        }), m_users.end());

    //LogI << "r user " << emitter << ", " << m_users.size() << std::endl;
}

void AudioSource::resetUsers()
{
    for (auto* user : m_users)
    {
        user->reset(this);
    }
}