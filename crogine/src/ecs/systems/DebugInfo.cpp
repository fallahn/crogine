/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/systems/DebugInfo.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/gui/Gui.hpp>

using namespace cro;

DebugInfo::DebugInfo(MessageBus& mb)
    : System(mb, typeid(DebugInfo))
{
    requireComponent<Transform>();

    addStats([&]() 
        {
            ImGui::Text("%lu entities in scene %lu", getEntities().size(), getScene()->getInstanceID());
            if (ImGui::CollapsingHeader("Entity List"))
            {
                for (const auto& s : m_debugLines)
                {
                    ImGui::Text("%s", s.c_str());
                }
            }
        });
}

//public
void DebugInfo::process(float)
{
    m_debugLines.clear();

    auto& entities = getEntities();
    for (auto& e : entities)
    {
        auto& tx = e.getComponent<Transform>();

        auto pos = tx.getPosition();
        auto wtx = tx.getWorldTransform();

        std::string op("Local position: ");
        op += std::to_string(pos.x) + ", " + std::to_string(pos.y);

        op += " World position: " + std::to_string(wtx[3][0]) + ", " + std::to_string(wtx[3][1]);

        m_debugLines.push_back("Entity " + std::to_string(e.getIndex()) + ": " + op);
    }
}