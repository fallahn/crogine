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

#pragma once

//define this to create macros which replace
//the stdio FILE funcs with SDL IOStream
#ifdef SDL_IO
#include <SDL3/SDL.h>

#ifndef EOF
#define UNDEF_EOF
#define EOF -1
#endif

static size_t sdlRead(void* dst, size_t size, size_t count, SDL_IOStream* f)
{
    //returns number of *objects* read
    return SDL_ReadIO(f, dst, size * count) / size;
}

static int sdlSeek(SDL_IOStream* f, long origin, int whence)
{
    return SDL_SeekIO(f, origin, (SDL_IOWhence)whence) == -1 ? -1 : 0;
}

static long sdlTell(SDL_IOStream* f)
{
    return (long)SDL_TellIO(f);
}

static int sdlGetc(SDL_IOStream* f)
{
    //returns char or EOF (-1)
    uint8_t c = 0;
    return SDL_ReadIO(f, &c, 1) == 0 ? EOF : c;
}

#ifdef UNDEF_EOF
#undef EOF
#endif

#define FOPEN(x, y) SDL_IOFromFile(x, y)
#define FCLOSE(x) SDL_CloseIO(x)
#define FREAD(x,y,z,w) sdlRead(x,y,z,w)
#define FSEEK(x,y,z) sdlSeek(x,y,z)
#define FTELL(x) sdlTell(x)
#define FGET(x) sdlGetc(x)
#define FHANDLE SDL_IOStream

#define IO_SET SDL_IO_SEEK_SET
#define IO_END SDL_IO_SEEK_END
#define IO_CUR SDL_IO_SEEK_CUR

#else
#define FOPEN(x, y) fopen(x,y)
#define FCLOSE(x) fclose(x)
#define FREAD(x,y,z,w) fread(x,y,z,w)
#define FSEEK(x,y,z) fseek(x,y,z)
#define FTELL(x) ftell(x)
#define FGET(x) fgetc(x)
#define FHANDLE FILE

#define IO_SET SEEK_SET
#define IO_END SEEK_END
#define IO_CUR SEEK_CUR

#endif
