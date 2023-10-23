/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include <crogine/audio/sound_system/SoundStream.hpp>

#include <memory>

namespace cro
{
    namespace Detail
    {
        class AudioFile;
    }

    /*!
    \brief Streaming music player.
    Supports playing audio from mono or stereo *ogg, *.mp3 and wav files.
    Requires the OpenAL AudioRenderer to be available.
    */
    class CRO_EXPORT_API MusicPlayer final : public SoundStream
    {
    public:
        MusicPlayer();
        ~MusicPlayer();

        MusicPlayer(const MusicPlayer&) = delete;
        MusicPlayer(MusicPlayer&&) = delete;
        const MusicPlayer& operator = (const MusicPlayer&) = delete;
        MusicPlayer& operator = (MusicPlayer&&) = delete;

        /*!
        \brief Attempts to load the file at the given path.
        Supported types are *.wav, *.mp3 and *.ogg
        \param path A string containing the path to the file to open
        \returns true on success, else false.
        */
        bool loadFromFile(const std::string& path);

        Time getDuration() const;

    private:
        std::unique_ptr<Detail::AudioFile> m_audioFile;
        std::int32_t m_bytesPerSample;

        bool onGetData(cro::SoundStream::Chunk&) override;
        void onSeek(std::int32_t) override;

    };
}
