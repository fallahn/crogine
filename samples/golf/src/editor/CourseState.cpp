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
#include "EditorConsts.hpp"

#include <crogine/detail/IOStream.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/util/Constants.hpp>

CourseState::CourseState(cro::StateStack& stack, cro::State::Context context, const SharedStateData&)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_skyboxIndex   (0),
    m_shrubIndex    (0),
    m_audioIndex    (0)
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
    //handle hotkey/shortcuts here
    if (evt.type == SDL_EVENT_KEY_DOWN)
    {
        if (evt.key.mod & SDL_KMOD_CTRL)
        {
            switch (evt.key.scancode)
            {
            default: break;
            case SDL_SCANCODE_N:
                //new project
                menuNewProject();
                break;
            case SDL_SCANCODE_O:
                //open project
                menuOpenProject();
                break;
            case SDL_SCANCODE_S:
                //save project
                if (!m_projectSettings.workingDirectory.empty())
                {
                    saveProject(m_projectSettings.workingDirectory);
                }
                LogI << "Project Saved..." << std::endl;
                break;
            case SDL_SCANCODE_Q:
                confirmQuit();
                break;
            }
        }
    }


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
    m_windowStates[WindowID::Properties].second = std::bind(&CourseState::drawProjectProperties, this);
    m_windowStates[WindowID::AudioEditor].second = std::bind(&CourseState::drawAudioEditor, this);
}

void CourseState::addToHistory(const std::filesystem::path& item)
{
    m_history.erase(std::remove_if(m_history.begin(), m_history.end(),
        [&item](const std::filesystem::path str)
        {
            return str == item;
        }), m_history.end());

    m_history.push_front(item);

    if (m_history.size() > MaxHistorySize)
    {
        m_history.pop_back();
    }
}

void CourseState::loadProject(const std::filesystem::path& path)
{
    //unmount any existing project
    if (!m_projectSettings.workingDirectory.empty())
    {
        addToHistory(m_projectSettings.workingDirectory);

        cro::IOResource::removePath(m_projectSettings.workingDirectory);

        //we have to do this to make sure we remove any files
        //which were listed from the previously mounted dir
        refreshAudioFiles();
        refreshSkyboxFiles();
        refreshShrubFiles();
    }

    m_projectSettings = {};
    m_courseData = {};
    m_previewTexture = {};
    m_windowStates[WindowID::Properties].first = false;

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
                    m_courseData.displayTitleBuffer = str.toUtf8Char();
                }
                else if (name == "description")
                {
                    auto str = prop.getValue<cro::String>();
                    if (str.size() > MaxDescChar)
                    {
                        str = str.substr(0, MaxDescChar);
                    }
                    m_courseData.displayDescription = str;
                    m_courseData.displayDescriptionBuffer = str.toUtf8Char();
                }
                else if (name == "hole")
                {
                    //make sure the path belongs to this project
                    //and that the file exists
                    const auto path = prop.getValue<std::filesystem::path>();
                    if (!std::filesystem::relative(path, std::filesystem::path("assets/golf/courses") / m_projectSettings.id).empty()
                        && cro::FileSystem::fileExists(path))
                    {
                        assertCourseDataPath(path, m_courseData.holePaths[holeIndex++]);
                    }
                }
            }
            if (m_courseData.displayTitle.empty())
            {
                //*SIGH*
                m_courseData.displayTitle = m_projectSettings.name;
                m_courseData.displayTitleBuffer = m_projectSettings.name;
            }
            if (m_courseData.displayDescription.empty())
            {
                m_courseData.displayDescription = "Enter Description";
                m_courseData.displayDescriptionBuffer = "Enter Description";
            }

            const std::filesystem::path previewImage = m_projectSettings.workingDirectory / "golf/courses" / m_projectSettings.id / "preview.png";
            if (cro::FileSystem::fileExists(previewImage))
            {
                m_previewTexture.loadFromFile(previewImage);
            }

            /*std::int32_t i = 0;
            for (auto& h : m_courseData.holePaths)
            {
                h = std::to_string(i++) + ".hole";
            }*/

            setWindowTitle();

            cro::IOResource::addPath(m_projectSettings.workingDirectory, std::filesystem::path("assets"));
            addToHistory(m_projectSettings.workingDirectory);
            refreshAudioFiles();
            refreshSkyboxFiles();
            refreshShrubFiles();

            m_editorSettings.lastLoadDirectory = path;
            saveSettings();

            m_windowStates[WindowID::Properties].first = true;
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
            if (!path.empty()
                && cro::FileSystem::fileExists(path))
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
            else if (name == "history_item")
            {
                addToHistory(prop.getValue<std::filesystem::path>());
            }
        }
    }
}

void CourseState::saveSettings() const
{
    cro::ConfigFile cfg;
    cfg.addProperty("open_location").setValue(m_editorSettings.lastLoadDirectory.empty() ? std::filesystem::current_path() : m_editorSettings.lastLoadDirectory);
    cfg.addProperty("save_location").setValue(m_editorSettings.lastSaveDirectory.empty() ? std::filesystem::current_path() : m_editorSettings.lastSaveDirectory);

    for (const auto& item : m_history)
    {
        cfg.addProperty("history_item").setValue(item);
    }

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

void CourseState::refreshSkyboxFiles()
{
    //TODO these paths get referenced a lot so
    //we probably want to make a const array somewhere
    const std::filesystem::path BasePath("assets/golf/skyboxes/");

    m_skyboxFiles.clear();
    const auto files = cro::FileSystem::listFiles(BasePath);
    for (const auto& file : files)
    {
        //we need to (crudely) remove the _n variants as these
        //are automatically applied at night time and not
        //listed in the course data files specifically.
        if (file.extension() == ".sbf")
        {
            if (file.string().find("_n") == std::string::npos)
            {
                m_skyboxFiles.emplace_back(BasePath / file);
            }
        }
    }

    m_skyboxIndex = 0;
    if (const auto res = std::find_if(m_skyboxFiles.begin(), m_skyboxFiles.end(),
        [this](const std::filesystem::path& p)
        {
            return p == m_courseData.skybox;
        });
        res != m_skyboxFiles.end())
    {
        m_skyboxIndex = std::distance(m_skyboxFiles.begin(), res);
    }
}

void CourseState::refreshShrubFiles()
{
    const std::filesystem::path BasePath("assets/golf/shrubs/");

    m_shrubFiles.clear();

    const auto files = cro::FileSystem::listFiles(BasePath);
    for (const auto& file : files)
    {
        if (file.extension() == ".shb")
        {
            m_shrubFiles.emplace_back(BasePath / file);
        }
    }

    m_shrubIndex = 0;
    if (const auto res = std::find_if(m_shrubFiles.begin(), m_shrubFiles.end(),
        [this](const std::filesystem::path& p)
        {
            return p == m_courseData.shrubs;
        });
        res != m_shrubFiles.end())
    {
        m_shrubIndex = std::distance(m_shrubFiles.begin(), res);
    }
}

void CourseState::refreshAudioFiles()
{
    const std::filesystem::path BasePath("assets/golf/sound/");

    m_audioFiles.clear();

    const auto files = cro::FileSystem::listFiles(BasePath);
    for (const auto& file : files)
    {
        //we only want ambience files, and not the night version
        //as these aren't specified in the data file
        if (file.extension() == ".xas"
            && file.string().find("ambience") == 0
            && file.string().find("_n") == std::string::npos)
        {
            m_audioFiles.emplace_back(BasePath / file);
        }
    }

    m_audioIndex = 0;
    if (const auto res = std::find_if(m_audioFiles.begin(), m_audioFiles.end(),
        [this](const std::filesystem::path& p)
        {
            return p == m_courseData.audio;
        });
        res != m_audioFiles.end())
    {
        m_audioIndex = std::distance(m_audioFiles.begin(), res);
    }
}

void CourseState::loadPreviewTexture(const std::filesystem::path& path)
{
    cro::Image image(true);
    if (image.loadFromFile(path))
    {
        if (image.getSize().x != 640 && image.getSize().y != 480)
        {
            image.resize({ 640u, 480u });
        }
        image.write(m_projectSettings.workingDirectory / "golf/courses" / m_projectSettings.id / "preview.png", true);

        m_previewTexture.loadFromImage(image);
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Failed opening image. See console for details");
    }
}