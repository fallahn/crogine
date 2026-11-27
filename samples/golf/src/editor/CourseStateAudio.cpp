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

#include <crogine/ecs/components/AudioEmitter.hpp>

void CourseState::createAudioPreview()
{
    //TODO create the preview (remove old if needed)
    //TODO this probably means flushing the audio resource
    //as we can't use the audioscape class without a resource instance

    //TODO this basically duplicates what's in GolfStateAssets.cpp
    //so could be consolidated
    constexpr auto envOffset = glm::vec2(MapSize) / 2.f;
    const auto radius = glm::length(envOffset);
    static constexpr float height = 8.f;

    /*std::size_t currIdx = 0;
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
    };*/

    for (auto i = 0; i < 6; ++i)
    {
        static constexpr float Arc = cro::Util::Const::TAU / 6.f;
        glm::vec2 pos = glm::vec2(std::sin((i * Arc) + (Arc / 2.f)), std::cos((i * Arc) + (Arc / 2.f)));
        pos *= radius;

        //TODO final pos = pos.x, height, -pos.y

        /*if (as.hasEmitter(emitterNames[currIdx]))
        {
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ pos.x, height, -pos.y });
            entity.addComponent<cro::AudioEmitter>() = as.getEmitter(emitterNames[currIdx]);
            entity.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(i));
        }*/

        //currIdx = (currIdx + 1) % emitterNames.size();
    }


    m_editorSettings.audioscapeChanged = false;
}

void CourseState::playAudioPreview()
{
    if (m_editorSettings.audioscapeChanged)
    {
        createAudioPreview();
    }

    //TODO actually play
}

void CourseState::pauseAudioPreview()
{
    //TODO pause
}