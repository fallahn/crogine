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

#include <crogine/Config.hpp>

#include <SDL3/SDL.h>

#include <filesystem>

namespace cro
{
    //used to automatically close SDL_IOStream files
    struct CRO_EXPORT_API IOStream final
    {
        ~IOStream()
        {
            close();
        }
        IOStream() : file(nullptr) {}
        IOStream(const IOStream&) = delete;
        IOStream& operator = (const IOStream&) = delete;

        IOStream(IOStream&&) noexcept;
        IOStream& operator = (IOStream&&) noexcept;

        //ensures u8 filepaths are properly cast to a compatible type
        bool open(const std::filesystem::path& p, const char* mode)
        {
            if (file)
            {
                //hmm is this expected behaviour or should
                //we assert because someone is currently using
                //our file handle?
                close();
            }
            file = SDL_IOFromFile(reinterpret_cast<const char*>(p.u8string().c_str()), mode);
            return file != nullptr;
        }

        //closes the file and resets the pointer to null
        void close()
        {
            if (file)
            {
                SDL_CloseIO(file);
                file = nullptr;
            }
        }

        //returns a copy of the file pointer - note that
        //this is owned by IOStream and should not be manually closed!
        //use the close() function instead.
        SDL_IOStream* filePtr() const { return file; }

        operator bool() { return file != nullptr; }

    private:
        friend class IOResource;
        SDL_IOStream* file = nullptr;
    };

    class CRO_EXPORT_API IOResource final
    {
    public:
        /*!
        \brief Adds a resource path to search when calling classes with a loadFromFile() function.
        This includes adding archives such as zip files and uses PHSYFS behind the scenes.
        \param path A utf8 encoded filesystem::path containing the path to the resource directory or archive
        \param rootPath A utf8 encoded filesystem::path prepended to any of the mounted file paths
        \begincode
        IOResource::addPath("assets"); //adds a directory to search
        IOResource::addPath("assets/images.zip", "assets"); //adds an archive to the search paths
        \endcode
        */
        static void addPath(const std::filesystem::path& path, const std::filesystem::path& rootPath);


        /*!
        \brief Searches mounted filesystem (via physfs) for given path and returns a handle if the file was opened.
        If the file is not found in the mounted file system the absolute path on the filesystem is tried.
        Note that this is intended for resources and should always be considered READ-ONLY and in BINARY mode.
        Handle may be null if opening failed.
        \param path The path to the resource to open
        \returns IOStream wrapper around an SDL_IOStream handle.
        */
        static IOStream open(const std::filesystem::path& path);

        /*!
        \brief Checks if a given file exists in the mounted file system.
        NOTE this does not check the regular filesystem, in which case
        std::filesystem::path::exists() should be used. As a convenience
        cro::FileSystem::fileExists() searches both the mounted file system
        followed by the physics file system
        */
        static bool exists(const std::filesystem::path& path);

        /*!
        \brief Lists all the files in the given mounted directory, if it exists.
        \returns A vector of file names found in the directory, which will be
        empty if no files exist or the directory doesn't exist
        */
        static std::vector<std::filesystem::path> listFiles(const std::filesystem::path& path);

    private:

        friend class App;
        static bool m_initOK;
    };
}