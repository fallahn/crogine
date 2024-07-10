/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2024
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

#pragma once

//attempts to parse an m3u playlist into a list of file strings
//currently only supports the 'file:///' protocol

#include <string>
#include <vector>
#include <cstdint>
#include <string>

namespace cro
{
    class AudioResource;
}

class M3UPlaylist final
{
public:
    //takes a single directory through which to search for
    //playlists. Doesn't support recursive searching (no subdirs)
    explicit M3UPlaylist(const std::string& searchFolder, std::uint32_t maxFiles = 25);

    //load another file and appeand it to the list
    bool loadPlaylist(const std::string& path);

    //manually add a path to a music file
    void addTrack(const std::string&);

    //shuffles playlist if it is loaded
    void shuffle();

    //increments the current index
    void nextTrack();

    //decrements the current index
    void prevTrack();


    const std::string& getCurrentTrack() const;

    std::size_t getCurrentIndex() const { return m_currentIndex; }
    std::size_t getTrackCount() const { return m_filePaths.size(); }


    const std::vector<std::string>& getFilePaths() const { return m_filePaths; }

private:
    std::size_t m_currentIndex;

    std::vector<std::string> m_filePaths;
};
