/*-----------------------------------------------------------------------

Matt Marchant 2026
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

#include "ImGuiIOStream.hpp"

#ifndef SDL_IO
#define SDL_IO
#endif
#include "../detail/IO_MACRO.inl"

#include <crogine/detail/IOStream.hpp>

#include <cstring>

ImFileHandle ImFileOpen(const char* filename, const char* mode)
{
    cro::IOStream file;
    if (std::strcmp(mode, "rb") == 0)
    {
        //check the mounted filesystem - only supports read-binary
        file = cro::IOResource::open(filename);
    }
    else
    {
        file.open(filename, mode);
    }

    if (file)
    {
        return file.release();
    }
    return nullptr;
    //return SDL_IOFromFile(filename, mode);
}
bool ImFileClose(ImFileHandle file)
{
    return SDL_CloseIO(file);
}
ImU64 ImFileGetSize(ImFileHandle file)
{
    return (ImU64)SDL_GetIOSize(file);
}
ImU64 ImFileRead(void* data, ImU64 size, ImU64 count, ImFileHandle file)
{
    return (ImU64)FREAD(data, (size_t)size, (size_t)count, file);
}
ImU64 ImFileWrite(const void* data, ImU64 size, ImU64 count, ImFileHandle file)
{
    return (ImU64)FWRITE(data, size, count, file);
}