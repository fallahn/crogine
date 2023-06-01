/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "SqliteState.hpp"

#include <crogine/core/SysTime.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>

SqliteState::SqliteState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
bool SqliteState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            cro::App::quit();
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void SqliteState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool SqliteState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void SqliteState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void SqliteState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void SqliteState::loadAssets()
{
}

void SqliteState::createScene()
{
    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.8f, 2.f });
}

void SqliteState::createUI()
{
    m_db.open(cro::App::getPreferencePath() + "profile.db3");

    registerWindow([&]() 
        {
            if (ImGui::Begin("DB Window"))
            {
                if (ImGui::Button("Insert Random"))
                {
                    CourseRecord record;
                    for (auto& h : record.holeScores)
                    {
                        h = cro::Util::Random::value(2, 5);
                        record.total += h;
                    }
                    record.totalPar = cro::Util::Random::value(-6, 6);
                    record.holeCount = cro::Util::Random::value(0, 2);
                    record.courseIndex = cro::Util::Random::value(0, 9);
                    record.wasCPU = cro::Util::Random::value(0, 4) == 0 ? 1 : 0;
                    m_db.insertCourseRecord(record);
                }

                static int32_t currentCourse = 0;
                static std::vector<CourseRecord> records = m_db.getCourseRecords(currentCourse);

                ImGui::Text("Course %d", currentCourse + 1);
                ImGui::SameLine();
                if (ImGui::Button("-"))
                {
                    currentCourse = (currentCourse + 9) % 10;
                    records = m_db.getCourseRecords(currentCourse);
                }
                ImGui::SameLine();
                if (ImGui::Button("+"))
                {
                    currentCourse = (currentCourse + 1) % 10;
                    records = m_db.getCourseRecords(currentCourse);
                }

                ImGui::Text("18 Holes: %d records", m_db.getCourseRecordCount(currentCourse, 0));
                ImGui::Text("Front 9 Holes: %d records", m_db.getCourseRecordCount(currentCourse, 1));
                ImGui::Text("Back 9 Holes: %d records", m_db.getCourseRecordCount(currentCourse, 2));

                ImGui::Separator();

                for (const auto& record : records)
                {
                    ImGui::Text("Hole 1: %d, Hole 18: %d - Total: %d  %s", record.holeScores[0], record.holeScores[17], record.total, cro::SysTime::dateString(record.timestamp).c_str());
                }


                ImGui::Separator();
                static std::vector<PersonalBestRecord> pbs = m_db.getPersonalBest(0);
                if (ImGui::Button("Insert PB"))
                {
                    PersonalBestRecord pb;
                    pb.hole = cro::Util::Random::value(0, 17);
                    pb.course = cro::Util::Random::value(0, 9);
                    pb.longestDrive = cro::Util::Random::value(5.141f, 10.f);
                    pb.longestPutt = cro::Util::Random::value(6.1337f, 10.f);
                    pb.score = cro::Util::Random::value(1, 13);
                    m_db.insertPersonalBestRecord(pb);

                    pbs = m_db.getPersonalBest(pb.course);
                }

                for (const auto& record : pbs)
                {
                    ImGui::Text("Hole: %d, Course: %d, Longest Drive: %3.1f, Longest Putt %3.1f, Score: %d",
                        record.hole, record.course, record.longestDrive, record.longestPutt, record.score);
                }
            }
            ImGui::End();
        });


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}