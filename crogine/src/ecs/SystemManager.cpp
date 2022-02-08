/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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

#include <crogine/core/Clock.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/ecs/InfoFlags.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/gui/Gui.hpp>

#include <sstream>

using namespace cro;

namespace
{
    constexpr float SystemTimeUpdateRate = 0.5f;
}

SystemManager::SystemManager(Scene& scene, ComponentManager& cm, std::uint32_t infoFlags) 
    : m_scene                   (scene),
    m_componentManager          (cm),
    m_infoFlags                 (infoFlags),
    m_systemUpdateAccumulator   (0.f)
{
    //TODO refactor this into a single window with panes for each flag
    if (infoFlags & INFO_FLAG_SYSTEMS_ACTIVE)
    {
        registerWindow(
            [&]()
        {
            ImGui::SetNextWindowSize({ 220.f, 300.f }, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints({ 220.f, 300.f }, { 1920.f, 1080.f });

            if (ImGui::Begin("Active Systems"))
            {
                if (ImGui::Button("Save To File"))
                {
                    std::string filename = SysTime::timeString() + "-" + SysTime::dateString() + ".txt";
                    std::replace(filename.begin(), filename.end(), '/', '-');
                    std::replace(filename.begin(), filename.end(), ':', '-');

                    RaiiRWops file;
                    file.file = SDL_RWFromFile(filename.c_str(), "w");

                    if (file.file)
                    {
                        std::stringstream ss;
                        for (const auto* s : m_activeSystems)
                        {
                            ss << s->getType().name() << "Entities: " << s->getEntities().size() << "\n";
                        }

                        SDL_RWwrite(file.file, ss.str().c_str(), ss.str().size(), 1);
                    }
                }

                ImGui::BeginChild("inner");
                for (const auto* s : m_activeSystems)
                {
                    ImGui::Text("%s\n\tEntities: %lu\n", s->getType().name(), s->getEntities().size());
                }
                ImGui::EndChild();
            }
            ImGui::End();
        });
    }

    if ((infoFlags & INFO_FLAG_SYSTEM_TIME) != 0)
    {
        registerWindow(
            [&]()
        {
            ImGui::SetNextWindowSize({ 220.f, 300.f }, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints({ 220.f, 300.f }, { 1920.f, 1080.f });

            if (ImGui::Begin("System Time"))
            {
                std::sort(m_systemSamples.begin(), m_systemSamples.end(), 
                    [](const SystemSample& a, const SystemSample& b)
                {
                        return a.elapsed > b.elapsed;
                });

                for (const auto& [system, elapsed] : m_systemSamples)
                {
                    ImGui::Text("%s: %2.4fms", system->getType().name(), elapsed);
                }
            }
            ImGui::End();
        });
    }
}

void SystemManager::addToSystems(Entity entity)
{
    const auto& entMask = entity.getComponentMask();
    for (auto& sys : m_systems)
    {
        const auto& sysMask = sys->getComponentMask();
        if ((entMask & sysMask) == sysMask)
        {
            sys->addEntity(entity);
        }
    }
}

void SystemManager::removeFromSystems(Entity entity)
{
    for (auto& sys : m_systems)
    {
        sys->removeEntity(entity);
    }
}

void SystemManager::forwardMessage(const Message& msg)
{
    for (auto& sys : m_systems)
    {
        sys->handleMessage(msg);
    }
}

void SystemManager::process(float dt)
{
    //hmm I wish this could be conditionally compiled...
    /*if (m_infoFlags)
    {        
        m_systemUpdateAccumulator += m_systemTimer.restart();

        if (m_systemUpdateAccumulator > SystemTimeUpdateRate)
        {
            m_systemUpdateAccumulator -= SystemTimeUpdateRate;
            m_systemSamples.clear();
        
            for (auto& system : m_activeSystems)
            {
                system->process(dt);
                m_systemSamples.emplace_back(system, m_systemTimer.restart() * 1000.f);
            }
        }
        else
        {
            for (auto& system : m_activeSystems)
            {
                system->process(dt);
            }
        }
    }
    else*/
    {
        for (auto& system : m_activeSystems)
        {
            system->process(dt);
        }
    }
}