/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_HDR
#define STBI_ONLY_TGA
#define STB_IMAGE_IMPLEMENTATION
#ifdef CRO_DEBUG_
#define STBI_FAILURE_USERMSG
#endif
#include "stb_image.h"

#include "SDLImageRead.hpp"

#include <crogine/detail/Assert.hpp>

#include <cstring>

namespace cro
{

    std::int32_t STBIMG__io_read(void* user, char* data, std::int32_t size)
    {
        STBIMG_stbio_RWops* io = (STBIMG_stbio_RWops*)user;

        auto ret = SDL_RWread(io->src, data, sizeof(char), size);
        if (ret == 0)
        {
            //we're at EOF or some error happened
            io->atEOF = 1;
        }
        return static_cast<std::int32_t>(ret * sizeof(char));
    }

    void STBIMG__io_skip(void* user, std::int32_t n)
    {
        STBIMG_stbio_RWops* io = (STBIMG_stbio_RWops*)user;

        if (SDL_RWseek(io->src, n, RW_SEEK_CUR) == -1)
        {
            //an error happened during seeking, hopefully setting EOF will make stb_image abort
            io->atEOF = 2; //set this to 2 for "aborting because seeking failed" (stb_image only cares about != 0)
        }
    }

    std::int32_t STBIMG__io_eof(void* user)
    {
        STBIMG_stbio_RWops* io = (STBIMG_stbio_RWops*)user;
        return io->atEOF;
    }

    void stbi_callback_from_RW(SDL_RWops* src, STBIMG_stbio_RWops* out)
    {
        CRO_ASSERT(src && out, "Cannot be nullptr!");

        //make sure out is at least initialized to something deterministic
        std::memset(out, 0, sizeof(*out));

        out->src = src;
        out->atEOF = 0;
        out->stb_cbs.read = cro::STBIMG__io_read;
        out->stb_cbs.skip = cro::STBIMG__io_skip;
        out->stb_cbs.eof = cro::STBIMG__io_eof;
    }

    void image_write_func(void* context, void* data, int size)
    {
        SDL_RWops* file = (SDL_RWops*)context;
        SDL_RWwrite(file, data, size, 1);
    }
}