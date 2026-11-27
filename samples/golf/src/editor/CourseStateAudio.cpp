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

#include "CourseState.hpp"
#include "../golf/GameConsts.hpp"

#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

void CourseState::createAudioPreview()
{
    for (auto e : m_audioEntities)
    {
        if (e.isValid()) //surely this should be a check when destroying?
        {
            m_gameScene.destroyEntity(e);
        }
    }

    std::filesystem::path audioscapePath;
    if (cro::FileSystem::fileExists(m_audioFiles[m_audioIndex]))
    {
        //this is probably only true for the in-game
        //files and not our project file
        audioscapePath = m_audioFiles[m_audioIndex];
    }
    else
    {
        //try our working directory
        LogW << "Implement me " << FILE_LINE << std::endl;

        //TODO we need to be able to flush audio files from
        //the resource handler as we might be updating them
        //via the editor and they will need reloading.
    }

    //modify for night mode
    if (m_editorSettings.previewNightMode)
    {
        audioscapePath.replace_extension();
        audioscapePath += "_n";
        audioscapePath.replace_extension(".xas");
    }


    cro::AudioScape as;
    if (!audioscapePath.empty()
        && as.loadFromFile(audioscapePath, m_resources.audio))
    {
        //TODO this basically duplicates what's in GolfStateAssets.cpp
        //so could be consolidated
        constexpr auto envOffset = glm::vec2(MapSize) / 2.f;
        const auto radius = glm::length(envOffset);
        static constexpr float height = 8.f;

        std::size_t currIdx = 0;
        const std::array emitterNames =
        {
            std::string("01"),
            std::string("02"),
            std::string("03"),
            std::string("04"),
            std::string("05"),
            std::string("06"),
            std::string("03"),
            std::string("04"),
        };

        for (auto i = 0u; i < m_audioEntities.size(); ++i)
        {
            static constexpr float Arc = cro::Util::Const::TAU / 6.f;
            glm::vec2 pos = glm::vec2(std::sin((i * Arc) + (Arc / 2.f)), std::cos((i * Arc) + (Arc / 2.f)));
            pos *= radius;

            if (as.hasEmitter(emitterNames[currIdx]))
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ pos.x, height, -pos.y });
                entity.addComponent<cro::AudioEmitter>() = as.getEmitter(emitterNames[currIdx]);
                entity.getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(i));
                m_audioEntities[i] = entity;
            }

            currIdx = (currIdx + 1) % emitterNames.size();
        }

    }
    else
    {
        LogW << "Could not find audioscape at " << audioscapePath << std::endl;
    }
    m_editorSettings.audioscapeChanged = false;
}

void CourseState::playAudioPreview()
{
    if (m_editorSettings.audioscapeChanged)
    {
        createAudioPreview();
    }

    for (auto e : m_audioEntities)
    {
        if (e.isValid())
        {
            e.getComponent<cro::AudioEmitter>().play();
            m_editorSettings.audioscapePlaying = true;
        }
    }
}

void CourseState::pauseAudioPreview()
{
    for (auto e : m_audioEntities)
    {
        if (e.isValid())
        {
            e.getComponent<cro::AudioEmitter>().pause();
        }
    }

    m_editorSettings.audioscapePlaying = false;
}