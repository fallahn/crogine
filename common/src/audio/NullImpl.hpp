/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include "AudioRenderer.hpp"

namespace cro
{
    namespace Detail
    {
        //implements a null audio backend should creating a valid renderer fail
        class NullImpl final : public AudioRendererImpl
        {
        public:
            bool init() override { return true; }
            void shutdown() override {}

            void setListenerPosition(glm::vec3) override {}
            void setListenerOrientation(glm::vec3, glm::vec3) override {}
            void setListenerVolume(float) override {}

            cro::int32 requestNewBuffer(const std::string&) override { return -1; }
            cro::int32 requestNewBuffer(const Detail::PCMData&) override { return -1; }
            void deleteBuffer(cro::int32) override {}

            cro::int32 requestNewStream(const std::string&) override { return -1; }
            void updateStream(int32) override {}
            void deleteStream(cro::int32) override {}

            cro::int32 requestAudioSource(cro::int32, bool) override { return -1; }
            void deleteAudioSource(cro::int32) override {}

            void playSource(cro::int32, bool) override {}
            void pauseSource(cro::int32) override {}
            void stopSource(cro::int32) override {}

            int32 getSourceState(cro::int32) const override { return -1; }

            void setSourcePosition(int32, glm::vec3) override {}
            void setSourcePitch(int32, float) override {}
            void setSourceVolume(int32, float) override {}
            void setSourceRolloff(int32, float) override {}
        };
    }
}