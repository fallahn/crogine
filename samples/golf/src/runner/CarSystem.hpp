/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "TrackSprite.hpp"

#include <crogine/ecs/System.hpp>

class TrackCamera;
struct Car final
{
    TrackSprite sprite;
    float speed = 10.f;
    float z = 0.f;

    //don't update vehicles in a pending piece of track
    bool active = false;

    //base audio volume when fading with distance
    float baseVolume = 1.f;

    struct Frame final
    {
        enum
        {
            HardLeft, Left,
            Straight,
            Right, HardRight,

            Count
        };
    };
    std::array<cro::FloatRect, Frame::Count> frames = {};
};

class Track;
class CarSystem final : public cro::System
{
public:
    CarSystem(cro::MessageBus&, Track&, const TrackCamera&);

    void process(float) override;

private:
    Track& m_track;
    const TrackCamera& m_camera;

    void onEntityAdded(cro::Entity) override;
};