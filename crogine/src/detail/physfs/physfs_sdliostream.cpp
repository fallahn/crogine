/*----------------------------------------------------------------------------
                                 MIT License
Copyright 2026 Matt Marchant

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Based on SDL3 wrapper for Physfs by Ryan C. Gordon

-----------------------------------------------------------------------------*/

#include "physfs.h"
#include "physfs_sdliostream.hpp"

#include <crogine/core/Log.hpp>

namespace
{
    Sint64 physfsIOStreamSize(void* userdata)
    {
        return static_cast<Sint64>(PHYSFS_fileLength(static_cast<PHYSFS_File*>(userdata)));
    }

    Sint64 physfsIOStreamSeek(void* userdata, Sint64 offset, SDL_IOWhence whence)
    {
        PHYSFS_File* handle = static_cast<PHYSFS_File*>(userdata);
        PHYSFS_sint64 pos = 0;

        if (whence == SDL_IO_SEEK_SET)
        {
            pos = (PHYSFS_sint64)offset;
        }
        else if (whence == SDL_IO_SEEK_CUR)
        {
            const PHYSFS_sint64 current = PHYSFS_tell(handle);
            if (current == -1)
            {
                return SDL_SetError("Can't find position in file: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            }

            if (offset == 0)
            {  /* this is a "tell" call. We're done. */
                return static_cast<Sint64>(current);
            }

            pos = current + (static_cast<PHYSFS_sint64>(offset));
        }
        else if (whence == SDL_IO_SEEK_END)
        {
            const PHYSFS_sint64 len = PHYSFS_fileLength(handle);
            if (len == -1)
            {
                return SDL_SetError("Can't find end of file: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            }

            pos = len + (static_cast<PHYSFS_sint64>(offset));
        }
        else
        {
            return SDL_SetError("Invalid 'whence' parameter.");
        }

        if (pos < 0)
        {
            return SDL_SetError("Attempt to seek past start of file.");
        }

        if (!PHYSFS_seek(handle, static_cast<PHYSFS_uint64>(pos)))
        {
            return SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        }

        return static_cast<Sint64>(pos);
    }

    std::size_t physfsIOStreamRead(void* userdata, void* ptr, std::size_t size, SDL_IOStatus* status)
    {
        PHYSFS_File* handle = static_cast<PHYSFS_File*>(userdata);
        const PHYSFS_uint64 readlen = static_cast<PHYSFS_uint64>(size);
        const PHYSFS_sint64 rc = PHYSFS_readBytes(handle, ptr, readlen);

        if (rc != ((PHYSFS_sint64)readlen))
        {
            /* not EOF? Must be an error. */
            if (!PHYSFS_eof(handle))
            {
                /* Setting an SDL error makes SDL take care of `status` for you. */
                SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
                return 0;
            }
        }
        return static_cast<std::size_t>(rc);
    }

    std::size_t physfsIOStreamWrite(void* userdata, const void* ptr, std::size_t size, SDL_IOStatus* status)
    {
        PHYSFS_File* handle = static_cast<PHYSFS_File*>(userdata);
        const PHYSFS_uint64 writelen = static_cast<PHYSFS_uint64>(size);
        const PHYSFS_sint64 rc = PHYSFS_writeBytes(handle, ptr, writelen);
        
        if (rc != ((PHYSFS_sint64)writelen))
        {
            /* Setting an SDL error makes SDL take care of `status` for you. */
            SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        }
        return static_cast<std::size_t>(rc);
    }

    bool physfsIOStreamClose(void* userdata)
    {
        if (!PHYSFS_close(static_cast<PHYSFS_File*>(userdata)))
        {
            return SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        }
        return true;
    }

    SDL_IOStream* createIOStream(PHYSFS_File* handle)
    {
        if (handle == nullptr)
        {
            //DLogE("IOStream from PhysFS: {} (NULL handle)", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            return nullptr;
        }

        SDL_IOStreamInterface iface = {};
        SDL_INIT_INTERFACE(&iface);
        iface.size = physfsIOStreamSize;
        iface.seek = physfsIOStreamSeek;
        iface.read = physfsIOStreamRead;
        iface.write = physfsIOStreamWrite;
        iface.close = physfsIOStreamClose;
        return SDL_OpenIO(&iface, handle);
    }
}



namespace cro::Detail::SDLFS
{
    SDL_IOStream* openRead(const char* path)
    {
        return createIOStream(PHYSFS_openRead(path));
    }

    SDL_IOStream* openWrite(const char* path)
    {
        return createIOStream(PHYSFS_openWrite(path));
    }

    SDL_IOStream* openAppend(const char* path)
    {
        return createIOStream(PHYSFS_openAppend(path));
    }

    SDL_IOStream* ioStreamFromPhysFS(PHYSFS_File* handle)
    {
        //this will warn/error message for us
        return createIOStream(handle);
    }
}