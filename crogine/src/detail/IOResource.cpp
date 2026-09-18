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

namespace
{
    std::vector<std::filesystem::path> listEntries(const std::filesystem::path& path, PHYSFS_FileType type)
    {
        if (!PHYSFS_isInit())
        {
            LogE << "[physfs] listEntries: Physfs is not initialised" << std::endl;
            return {};
        }

        std::vector<std::filesystem::path> ret;

        char** rc = PHYSFS_enumerateFiles(U8PATH_CAST(path));
        if (rc)
        {
            char** i;
            for (i = rc; *i != NULL; i++)
            {
                auto fullPath = path / *i;

                PHYSFS_Stat st = {};
                PHYSFS_stat(U8PATH_CAST(fullPath), &st);

                if (st.filetype == type)
                {
                    ret.emplace_back(*i);
                }
            }
            PHYSFS_freeList(rc);
        }

        return ret;
    }
}

bool IOResource::addPath(const std::filesystem::path& path, const std::filesystem::path& rootPath)
{
    //this will crash if PHYSFS_init() failed
    //in which case we need to test here first before adding the path
    if (!PHYSFS_isInit())
    {
        LogE << "PHYSFS was not successfully initialised - PHYSFS functions are unavailable." << std::endl;
        return false;
    }

    if (PHYSFS_mount(U8PATH_CAST(path), U8PATH_CAST(rootPath), 1) == 0)
    {
        //PHYSFS_ErrorCode
        LogE << "Failed to add " << path << " to search paths, reason: " << PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()) << std::endl;
        return false;
    }
    //LogI << "added " << path.generic_string() << " at " << rootPath.generic_string() << std::endl;
    return true;
}

IOStream IOResource::open(const std::filesystem::path& path)
{
    IOStream retVal;

    if (PHYSFS_isInit())
    {
        if (auto f = openRead(U8PATH_CAST(path)); f != nullptr)
        {
            //LogI << "Found " << path.filename() << " in mounted filesystem " << std::endl;
            retVal.file = f;
        }
        else
        {
            //LogI << path.filename() <<  " was not found in the mounted filesystem, trying CWD instead..." << std::endl;
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
    if (!PHYSFS_isInit() ||
        !PHYSFS_exists(U8PATH_CAST(path)))
    {
        return false;
    }
    return true;
}

std::vector<std::filesystem::path> IOResource::listFiles(const std::filesystem::path& path)
{
    return listEntries(path, PHYSFS_FILETYPE_REGULAR);
}

std::vector<std::filesystem::path> IOResource::listDirectories(const std::filesystem::path& path)
{
    return listEntries(path, PHYSFS_FILETYPE_DIRECTORY);
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