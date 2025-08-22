/*-----------------------------------------------------------------------

Matt Marchant 2025
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

Portions of this code are based on the public domain source found at
https://www.mumble.info/documentation/developer/positional-audio/link-plugin/ 

-----------------------------------------------------------------------*/

#include <crogine/audio/MumbleLink.hpp>
#include <crogine/core/Log.hpp>
#include <crogine/util/Matrix.hpp>

#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <fcntl.h> /* For O_* constants */
#endif // _WIN32

namespace cro
{
    struct LinkedMem final
    {
    #ifdef _WIN32
        UINT32	version;
        DWORD	tick;
    #else
        uint32_t version;
        uint32_t tick;
    #endif
        float	srcPosition[3];
        float	srcForward[3];
        float	srcUp[3];
        wchar_t	name[256];
        float	listenerPosition[3];
        float	listenerForward[3];
        float	listenerUp[3];
        wchar_t	identity[256];
    #ifdef _WIN32
        UINT32	contextLen;
    #else
        uint32_t contextLen;
    #endif
        unsigned char context[256];
        wchar_t description[2048];
    };
}

using namespace cro;

MumbleLink::MumbleLink(const cro::String& name, const cro::String& description)
    : m_output		(nullptr),
    m_description	(),
    m_name          (),
    m_identity		()
{
    if (name.empty())
    {
        m_name = L"Unnamed Link";
    }
    else
    {
        auto tempStr = name.toWideString();
        if (tempStr.length() >= MaxIdentity)
        {
            tempStr = tempStr.substr(0, MaxIdentity - 1);
        }
        m_name.swap(tempStr);
    }

    if (description.empty())
    {
        m_description = L"No Description Given";
    }
    else
    {
        auto tempStr = description.toWideString();
        if (tempStr.length() >= MaxIdentity)
        {
            tempStr = tempStr.substr(0, MaxDescription - 1);
        }
        m_description.swap(tempStr);
    }
}

MumbleLink::~MumbleLink()
{
    disconnect();
}

//public
void MumbleLink::setIdentity(const cro::String& id)
{
    //use the cro::String class to hack around the fact that the
    //link expects wchar strings
    auto tempStr = id.toWideString();
    if (tempStr.length() >= MaxIdentity)
    {
        tempStr = tempStr.substr(0, MaxIdentity - 1);
    }

    if (tempStr != m_identity)
    {
        //only update if this changed
        m_identity.swap(tempStr);

        if (connected())
        {
            std::wcsncpy(m_output->identity, m_identity.c_str(), MaxIdentity);
        }
    }
}

bool MumbleLink::connect()
{
    //urrrgggh I don't know enough about this to know that it's *SAFE*
    //it's copy pasta from the mumble site - I don't know how to detect if
    //any of this is released properly, and it just seems WRONG

    if (!m_output)
    {
        if (m_identity.empty())
        {
            LogW << "Mumble ID is currently empty - recommend setting this *first*" << std::endl;
        }


#ifdef _WIN32
        m_fileHandle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"MumbleLink");
        if (!m_fileHandle)
        {
            LogE << "MumbleLink: Unable to open file mapping - is the link running?" << std::endl;
            return false;
        }

        m_output = (LinkedMem*)MapViewOfFile(m_fileHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LinkedMem));

        if (!m_output)
        {
            LogE << "MumbleLink: Unable to obtain mapped memory" << std::endl;
            CloseHandle(m_fileHandle);
            m_fileHandle = nullptr;

            return false;
        }
#else
        snprintf(m_memname, 256, "/MumbleLink.%d", getuid());

        int shmfd = shm_open(m_memname, O_RDWR, S_IRUSR | S_IWUSR);

        if (shmfd < 0)
        {
            LogE << "MumbleLink: Unable to open file mapping - is the link running?" << std::endl;
            return false;
        }

        m_output = (LinkedMem*)(mmap(NULL, sizeof(struct LinkedMem), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0));

        if (m_output == (void*)(-1))
        {
            LogE << "MumbleLink: Unable to obtain mapped memory" << std::endl;
            shm_unlink(m_memname);
            m_output = nullptr;

            return false;
        }
#endif

        //if we're here assume everything went OK and set the initial params
        if (m_output->version != 2)
        {
            std::wcsncpy(m_output->name, m_name.c_str(), MaxIdentity);
            std::wcsncpy(m_output->description, m_description.c_str(), MaxDescription);
            m_output->version = 2;
        }

        if (!m_identity.empty())
        {
            std::wcsncpy(m_output->identity, m_identity.c_str(), MaxIdentity);
        }

        //TODO we need to set this to *something* so that mumble etc know to apply positional audio
        //(matching contexts are activated then grouped in the client). Should probably be a property
        std::memcpy(m_output->context, "Happies!", 8);
        m_output->contextLen = 8;

    }
    return true;
}

void MumbleLink::disconnect()
{
    if (m_output)
    {
#ifdef _WIN32
        if (m_fileHandle)
        {
            CloseHandle(m_fileHandle);
        }
        m_fileHandle = nullptr;
#else
        shm_unlink(m_memname);
#endif


        m_output = nullptr;
    }
}

void MumbleLink::setSpeakerPosition(const glm::mat4& tx)
{
    if (m_output)
    {
        //SIGH we have to normalise because we can't make gaurantees about scale...
        const auto fwd = glm::normalize(Util::Matrix::getForwardVector(tx));
        const auto up = glm::normalize(Util::Matrix::getUpVector(tx));
        const auto& pos = tx[3];

        m_output->srcForward[0] = fwd.x;
        m_output->srcForward[1] = fwd.y;
        m_output->srcForward[2] = -fwd.z; //left handed

        m_output->srcUp[0] = up.x;
        m_output->srcUp[1] = up.y;
        m_output->srcUp[2] = -up.z;

        m_output->srcPosition[0] = pos.x;
        m_output->srcPosition[1] = pos.y;
        m_output->srcPosition[2] = -pos.z;
    }
}

void MumbleLink::setListenerPosition(const glm::mat4& tx)
{
    if (m_output)
    {
        //SIGH we have to normalise because we can't make gaurantees about scale...
        const auto fwd = glm::normalize(Util::Matrix::getForwardVector(tx));
        const auto up = glm::normalize(Util::Matrix::getUpVector(tx));
        const auto& pos = tx[3];

        m_output->listenerForward[0] = fwd.x;
        m_output->listenerForward[1] = fwd.y;
        m_output->listenerForward[2] = -fwd.z; //left handed

        m_output->listenerUp[0] = up.x;
        m_output->listenerUp[1] = up.y;
        m_output->listenerUp[2] = -up.z;

        m_output->listenerPosition[0] = pos.x;
        m_output->listenerPosition[1] = pos.y;
        m_output->listenerPosition[2] = -pos.z;
    }
}

void MumbleLink::update()
{
    if (m_output)
    {
        m_output->tick++;
    }
}