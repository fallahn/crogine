/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "GameConsts.hpp"
#include "../M3UPlaylist.hpp"

void createMusicPlayer(cro::Scene& scene, cro::AudioResource& audio, cro::Entity gameMusic)
{
    //parse any music files into a playlist
    M3UPlaylist m3uPlaylist(audio, Social::getBaseContentPath() + "music/");

    if (m3uPlaylist.getTrackCount() == 0)
    {
        //look in the fallback dir
        const auto MusicDir = "assets/golf/sound/music/";
        if (cro::FileSystem::directoryExists(cro::FileSystem::getResourcePath() + MusicDir))
        {
            const auto files = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + MusicDir);
            for (const auto& file : files)
            {
                //this checks the file has a valid extension
                //and limits the number of files loaded
                m3uPlaylist.addTrack(MusicDir + file);
            }
            m3uPlaylist.shuffle();
        }
    }

    if (cro::Console::getConvarValue<bool>("shuffle_music"))
    {
        m3uPlaylist.shuffle();
    }


    //if the playlist isnt empty, create the music playing entities
    if (auto trackCount = m3uPlaylist.getTrackCount(); trackCount != 0)
    {
        struct MusicPlayerData final
        {
            std::vector<cro::Entity> playlist;
            std::size_t currentIndex = 0;
        };

        auto playerEnt = scene.createEntity();
        playerEnt.addComponent<cro::Transform>();
        playerEnt.addComponent<cro::Callback>().active = true;
        playerEnt.getComponent<cro::Callback>().setUserData<MusicPlayerData>();
        playerEnt.getComponent<cro::Callback>().function =
            [gameMusic](cro::Entity e, float dt) mutable
            {
                //set the mixer channel to inverse valaue of main music channel
                 //while the incidental music is playing
                if (gameMusic.isValid())
                {
                    //fade out if the menu music is playing, ie in a transition
                    const float target = gameMusic.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Playing ? 1.f - std::ceil(cro::AudioMixer::getVolume(MixerChannel::Music)) : 1.f;
                    float vol = cro::AudioMixer::getPrefadeVolume(MixerChannel::UserMusic);
                    if (target < vol)
                    {
                        vol = std::max(0.f, vol - (dt * 2.f));
                    }
                    else
                    {
                        vol = std::min(1.f, vol + dt);
                    }
                    cro::AudioMixer::setPrefadeVolume(vol, MixerChannel::UserMusic);
                }


                //if the playlist is empty just disable this ent
                auto& [playlist, currentIndex] = e.getComponent<cro::Callback>().getUserData<MusicPlayerData>();
                if (playlist.empty())
                {
                    e.getComponent<cro::Callback>().active = false;
                    return;
                }


                //else check the current music state and pause when volume is low else play the
                //next track when we stop playing.
                auto musicEnt = playlist[currentIndex];
                const float currVol = cro::AudioMixer::getVolume(MixerChannel::UserMusic);
                auto state = musicEnt.getComponent<cro::AudioEmitter>().getState();

                if (state == cro::AudioEmitter::State::Playing)
                {
                    if (currVol < MinMusicVolume)
                    {
                        musicEnt.getComponent<cro::AudioEmitter>().pause();
                    }
                }
                else if (state == cro::AudioEmitter::State::Paused
                    && currVol > MinMusicVolume)
                {
                    musicEnt.getComponent<cro::AudioEmitter>().play();
                }
                else if (state == cro::AudioEmitter::State::Stopped)
                {
                    currentIndex = (currentIndex + 1) % playlist.size();
                    playlist[currentIndex].getComponent<cro::AudioEmitter>().play();
                }
            };


        auto& playlist = playerEnt.getComponent<cro::Callback>().getUserData<MusicPlayerData>().playlist;

        //this is a fudge because there's a bug preventing reassigning streams
        for (auto i = 0u; i < trackCount; ++i)
        {
            const auto id = m3uPlaylist.getCurrentTrack();
            m3uPlaylist.nextTrack();

            auto entity = scene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::AudioEmitter>().setSource(audio.get(id));
            entity.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::UserMusic);
            entity.getComponent<cro::AudioEmitter>().setVolume(0.6f);

            playlist.push_back(entity);
        }
        if (!playlist.empty())
        {
            playlist[0].getComponent<cro::AudioEmitter>().play();
        }
    }
}