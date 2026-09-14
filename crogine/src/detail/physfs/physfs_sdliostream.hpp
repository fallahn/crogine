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

#pragma once

#include <SDL3/SDL.h>

namespace cro::Detail::SDLFS
{
    /*!
    Open a file for reading via PhysFS and return an SDL_IOStream handle
    PhysFS handles are automatically closed when the SDL handle is closed.
    */
    SDL_IOStream* openRead(const char* path);

    /*!
    Open a file for writing via PhysFS and return an SDL_IOStream handle
    */
    SDL_IOStream* openWrite(const char* path);

    /*!
    Open a file for appending via PhysFS and return an SDL_IOStream handle
    */
    SDL_IOStream* openAppend(const char* path);

    /*!
    \brief Creates an SDL_IOStream handle from a PhysFS handle
    */
    SDL_IOStream* ioStreamFromPhysFS(struct PHYSFS_File*);
}