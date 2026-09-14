/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2026
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

#include "physfs/physfs.h"
#include "physfs/physfs_sdliostream.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/detail/IOStream.hpp>


using namespace cro;
using namespace cro::Detail::SDLFS;

bool IOResource::m_initOK = false;

void IOResource::addPath(const std::filesystem::path& path)
{
    //this will crash if PHYSFS_init() failed
    //in which case we need to test here first before adding the path
    if (!m_initOK)
    {
        LogE << "PHYSFS was not successfully initialised - PHYSFS functions are unavailable." << std::endl;
        return;
    }

    //TODO the third parameter to mount() causes the path to be
    //prepended when 0 else appended to search list. We can make this a param
    //though currently we search archives last so we can override these more easily during deleveopment
    if (PHYSFS_mount(U8PATH_CAST(path), nullptr, 1) == 0)
    {
        //PHYSFS_ErrorCode
        LogE << "Failed to add " << path << " to search paths, reason: " << PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()) << std::endl;
    }
}

IOStream IOResource::open(const std::filesystem::path& path)
{
    IOStream retVal;

    if (m_initOK)
    {
        if (auto f = openRead(U8PATH_CAST(path)); f != nullptr)
        {
            retVal.file = f;
        }
        else
        {
            //DLogW("{} was not found in the mounted filesystem, trying CWD instead...", path.string());
            retVal.open(path, "rb");
        }
    }
    else
    {
        LogW << "openResource() failed: PhysFS is not initialised." << std::endl;
    }
    return retVal;
}

bool IOResource::exists(const std::filesystem::path& path)
{
    if (!m_initOK ||
        !PHYSFS_exists(U8PATH_CAST(path)))
    {
        return false;
    }
    return true;
}


//--------------------------------------------

IOStream::IOStream(IOStream&& other) noexcept
{
    file = other.file;
    other.file = nullptr;
}

IOStream& IOStream::operator = (IOStream&& other) noexcept
{
    if (&other != this)
    {
        file = other.file;
        other.file = nullptr;
    }
    return *this;
}