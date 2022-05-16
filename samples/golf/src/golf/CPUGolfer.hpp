/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <crogine/detail/Types.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <vector>

namespace cro
{
    class Message;
}

class InputParser;
class CollisionMesh;
struct ActivePlayer;
class CPUGolfer final : public cro::GuiClient
{
public:

    CPUGolfer(const InputParser&, const ActivePlayer&, const CollisionMesh&);

    void handleMessage(const cro::Message&);
    void activate(glm::vec3);
    void update(float, glm::vec3);
    bool thinking() const { return m_thinking; }

private:

    const InputParser& m_inputParser; //only reads the state - actual commands are send by raising events.
    const ActivePlayer& m_activePlayer;
    const CollisionMesh& m_collisionMesh;
    glm::vec3 m_target;

    enum class State
    {
        Inactive,
        PickingClub, //assess distance to pin from player
        Aiming, //account for wind speed and direction
        Stroke, //power bar is active
        Watching //ball is mid-flight but turn not yet ended
    }m_state = State::Inactive;

    std::int32_t m_clubID;
    std::int32_t m_prevClubID;
    std::int32_t m_searchDirection; //which way we search for a club
    float m_searchDistance;

    float m_aimDistance;
    float m_aimAngle;

    float m_targetPower;
    float m_targetAccuracy;

    float m_prevPower;
    float m_prevAccuracy;
    enum class StrokeState
    {
        Power, Accuracy
    }m_strokeState = StrokeState::Power;

    bool m_thinking; //not a state per se, rather used to pause/idle while in specific states
    float m_thinkTime;
    void startThinking(float);
    void think(float);

    void pickClub(float, glm::vec3);
    void aim(float, glm::vec3);
    void stroke(float);

    //for each pressed event we need a release event the next frame
    std::vector<cro::Event> m_popEvents;
    void sendKeystroke(std::int32_t, bool autoRelease = true);
};
