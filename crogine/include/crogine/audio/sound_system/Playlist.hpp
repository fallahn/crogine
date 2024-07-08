/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include <string>
#include <vector>
#include <array>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>

namespace cro
{
    /*
    \brief Loads and buffers audio files asynchronously so
    that they may be streamed continuously to a buffer such
    as DynamicAudioStream. Currently only supports 44.1KHz, 16 bit Stereo
    */
    class CRO_EXPORT_API Playlist final
    {
    public:
        Playlist(); //TODO specify a format the audio must match - currently fixed to 16bit stereo
        ~Playlist();

        Playlist(const Playlist&) = delete;
        Playlist(Playlist&&) = delete;
        Playlist& operator = (const Playlist) = delete;
        Playlist& operator = (Playlist&&) = delete;


        /*!
        \brief Add a file path to a file to addd to the playlist
        NOTE this must be the FULL PATH to the file, if a relative
        path is intended, eg in the assets directory, then make
        sure to prepend the path with FileSystem::getResourcePath()
        */
        void addPath(const std::string&);


        /*!
        \brief returns a pointer 16 bit Stereo PCM samples
        \param count A reference to a std::int32_t which is filled
        with the number of *samples* pointed to by the return value
        */
        const std::int16_t* getData(std::int32_t& count);

        /*!
        \brief Returns a COPY of the tracklist (as it otherwise gets manipulated
        by multiple threads...)
        */
        std::vector<std::string> getTrackList() const;

    private:

        static constexpr std::size_t MaxBuffers = 3u;

        //main thread only
        std::array<std::int16_t, 100u> m_silence = {}; //returned when no audio is available
        std::atomic<std::size_t> m_outBuffer; //current buffer from which data is returned
        std::int32_t m_outputOffset; //where we are in the current buffer

        //worker thread only
        std::atomic<std::size_t> m_inBuffer; //next buffer to be filled with data from thread

        //touched by both threads
        std::vector<std::string> m_filePaths;
        std::atomic<std::size_t> m_fileIndex;

        std::array<std::vector<std::int16_t>, MaxBuffers> m_buffers = {};


        std::atomic_bool m_threadRunning;
        std::atomic_bool m_loadNextFile;
        std::thread m_thread;
        mutable std::mutex m_mutex;

        void threadFunc();
    };
}