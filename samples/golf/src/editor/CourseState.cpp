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

#include <crogine/detail/IOStream.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>

CourseState::CourseState(cro::StateStack& stack, cro::State::Context context, const SharedStateData&)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        setWindowTitle();
        cro::App::getInstance().setClearColour(cro::Colour::CornflowerBlue);
        std::fill(m_windowStates.begin(), m_windowStates.end(), std::make_pair(false, std::function<void()>()));

        addSystems();
        loadAssets();
        createScene();
        createUI();

        loadSettings();
    });
}

//public
bool CourseState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_EVENT_QUIT)
    {
        confirmQuit();
    }

    //if (evt.type == SDL_EVENT_KEY_DOWN)
    //{
    //    switch (evt.key.key)
    //    {
    //    default: break;
    //    case SDLK_BACKSPACE:
    //    case SDLK_ESCAPE:
    //        requestStackClear();
    //        requestStackPush(0);
    //        break;
    //    }
    //}

    m_gameScene.forwardEvent(evt);
    return true;
}

void CourseState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped)
        {
            setWindowTitle();
        }
    }

    m_gameScene.forwardMessage(msg);
}

bool CourseState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    return true;
}

void CourseState::preRender()
{
    if (getStateCount() == 1)
    {
        m_gameScene.preRender();
    }
}

void CourseState::render()
{
    if (getStateCount() == 1)
    {
        m_gameScene.render();
    }
}

//private
void CourseState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void CourseState::loadAssets()
{
}

void CourseState::createScene()
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

void CourseState::createUI()
{
    registerWindow([this]() 
        {
            //don't draw if we're not the top state
            if (getStateCount() == 1)
            {
                drawMenuBar();

                for (auto& [state, window] : m_windowStates)
                {
                    if (state)
                    {
                        window();
                    }
                }
            }
        });

    m_windowStates[WindowID::NewProject].second = std::bind(&CourseState::drawNewProject, this);
}

void CourseState::loadProject(const std::filesystem::path& path)
{
    //unmount any existing project
    if (!m_projectSettings.workingDirectory.empty())
    {
        cro::IOResource::removePath(m_projectSettings.workingDirectory);
    }

    m_projectSettings = {};
    m_courseData = {};

    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path / "settings.svgproj"))
    {
        for (const auto& prop : cfg.getProperties())
        {
            const auto name = prop.getName();
            if (name == "name")
            {
                m_projectSettings.name = prop.getValue<std::string>();
            }
            else if (name == "id")
            {
                m_projectSettings.id = prop.getValue<std::string>();
            }
        }

        m_projectSettings.workingDirectory = path;
        
        if (!validateProjectDirectories(m_projectSettings.id))
        {
            cro::FileSystem::showMessageBox("Error", "One or more project directories are missing");
            m_projectSettings = {};
        }
        else
        {
            std::int32_t holeIndex = 0;

            cro::ConfigFile holeData;
            holeData.loadFromFile(path / "golf/courses" / m_projectSettings.id / "course.data");

            static constexpr std::size_t MaxDescChar = 34;

            for (const auto& prop : holeData.getProperties())
            {
                const auto& name = prop.getName();
                if (name == "skybox")
                {
                    assertCourseDataPath(prop.getValue<std::filesystem::path>(), m_courseData.skybox);
                }
                else if (name == "shrubbery")
                {
                    assertCourseDataPath(prop.getValue<std::filesystem::path>(), m_courseData.shrubs);
                }
                else if (name == "audio")
                {
                    assertCourseDataPath(prop.getValue<std::filesystem::path>(), m_courseData.audio);
                }
                else if (name == "instance_model")
                {
                    assertCourseDataPath(prop.getValue<std::filesystem::path>(), m_courseData.instanceModel);
                }
                else if (name == "title")
                {
                    auto str = prop.getValue<cro::String>();
                    if (str.size() > MaxDescChar)
                    {
                        str = str.substr(0, MaxDescChar);
                    }
                    m_courseData.displayTitle = str;
                }
                else if (name == "description")
                {
                    auto str = prop.getValue<cro::String>();
                    if (str.size() > MaxDescChar)
                    {
                        str = str.substr(0, MaxDescChar);
                    }
                    m_courseData.displayDescription = str;
                }
                else if (name == "hole")
                {
                    assertCourseDataPath(prop.getValue<std::filesystem::path>(), m_courseData.holePaths[holeIndex++]);
                }
            }

            setWindowTitle();

            cro::IOResource::addPath(m_projectSettings.workingDirectory, std::filesystem::path("assets"));
            m_editorSettings.lastLoadDirectory = path;
            saveSettings();
        }
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Could not load project settings");
    }
}

void CourseState::saveProject(const std::filesystem::path& dirPath)
{
    //make sure this is the correct directory
    const auto dirlist = cro::FileSystem::listDirectories(dirPath);
    if (dirlist.empty() || dirlist[0] != "golf")
    {
        cro::FileSystem::showMessageBox("Error", "Not a valid project directory");
        LogE << "Failed saving to " << dirPath << std::endl;
    }
    else
    {
        //save the course file
        cro::ConfigFile courseCfg("course");
        courseCfg.addProperty("skybox").setValue(m_courseData.skybox);
        courseCfg.addProperty("shrubbery").setValue(m_courseData.shrubs);
        courseCfg.addProperty("audio").setValue(m_courseData.audio);
        if (!m_courseData.instanceModel.empty())
        {
            courseCfg.addProperty("instance_model").setValue(m_courseData.instanceModel);
        }
        courseCfg.addProperty("title").setValue(m_courseData.displayTitle);
        courseCfg.addProperty("description").setValue(m_courseData.displayDescription);

        for (const auto& path : m_courseData.holePaths)
        {
            if (!path.empty())
            {
                courseCfg.addProperty("hole").setValue(path);
            }
        }
        courseCfg.save(dirPath / "golf/courses" / m_projectSettings.id / "course.data");

        //save the project settings
        cro::ConfigFile cfg;
        cfg.addProperty("name").setValue(m_projectSettings.name);
        cfg.addProperty("id").setValue(m_projectSettings.id);

        cfg.save(dirPath / "settings.svgproj");

        m_editorSettings.lastSaveDirectory = dirPath;
        saveSettings();
    }
}

std::array<std::filesystem::path, 11u> CourseState::getProjectDirectories(const std::string& id) const
{
    return
    {
        std::filesystem::path("golf"),
        std::filesystem::path("golf/courses"),
        std::filesystem::path("golf/courses") / id,
        std::filesystem::path("golf/images"),
        std::filesystem::path("golf/images/skybox"),
        std::filesystem::path("golf/images/terrain"),
        std::filesystem::path("golf/models"),
        std::filesystem::path("golf/models/") / id,
        std::filesystem::path("golf/models/skybox"),
        std::filesystem::path("golf/skyboxes"),
        std::filesystem::path("golf/treesets")
    };
}

bool CourseState::validateProjectDirectories(const std::string& id) const
{
    const auto dirs = getProjectDirectories(id);
    for (const auto d : dirs)
    {
        if (!cro::FileSystem::directoryExists(m_projectSettings.workingDirectory / d))
        {
            LogE << d << ": project directory missing or not found" << std::endl;
            return false;
        }
    }
    return true;
}

void CourseState::setWindowTitle()
{
    std::string title = "Super Video Golf - Course Editor";
    if (!m_projectSettings.name.empty())
    {
        title += " (" + m_projectSettings.name + ")";
    }
    cro::App::getWindow().setTitle(title);
}

void CourseState::loadSettings()
{
    cro::ConfigFile cfg;
    if (cfg.loadFromFile(cro::App::getPreferencePath() / "course_editor.cfg"))
    {
        const auto& props = cfg.getProperties();
        for (const auto& prop : props)
        {
            const auto& name = prop.getName();
            if (name == "open_location")
            {
                m_editorSettings.lastLoadDirectory = prop.getValue<std::filesystem::path>();
            }
            else if (name == "save_location")
            {
                m_editorSettings.lastSaveDirectory = prop.getValue<std::filesystem::path>();
            }
        }
    }
}

void CourseState::saveSettings() const
{
    cro::ConfigFile cfg;
    cfg.addProperty("open_location").setValue(m_editorSettings.lastLoadDirectory.empty() ? std::filesystem::current_path() : m_editorSettings.lastLoadDirectory);
    cfg.addProperty("save_location").setValue(m_editorSettings.lastSaveDirectory.empty() ? std::filesystem::current_path() : m_editorSettings.lastSaveDirectory);

    cfg.save(cro::App::getPreferencePath() / "course_editor.cfg");
}

void CourseState::confirmQuit()
{
    if (!m_projectSettings.workingDirectory.empty())
    {
        if (cro::FileSystem::showMessageBox("Confirm", "Do you want to save the current project?", cro::FileSystem::ButtonType::YesNo))
        {
            saveProject(m_projectSettings.workingDirectory);
        }
    }
    cro::App::quit();
}

void CourseState::assertCourseDataPath(const std::filesystem::path& inPath, std::filesystem::path& dest)
{
    if (inPath.empty())
    {
        return;
    }

    static const std::filesystem::path Base("assets");

    //make sure the resource paths are all assets/golf/etc...
    if (const auto rel = std::filesystem::relative(inPath, Base); rel.empty())
    {
        dest = Base / inPath;
    }
    else
    {
        dest = inPath;
    }

    if (!cro::FileSystem::fileExists(dest))
    {
        dest.clear();
        LogW << dest << ": File not found" << std::endl;
    }
}