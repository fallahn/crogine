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

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

#include <deque>

struct SharedStateData;
class CourseState final : public cro::State, public cro::GuiClient
{
public:
    CourseState(cro::StateStack&, cro::State::Context, const SharedStateData&);

    cro::StateID getStateID() const override { return StateID::EditorCourse; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void preRender() override;

    void render() override;

private:

    cro::Scene m_gameScene;
    cro::ResourceCollection m_resources;

    //hmm seems a shame to have to maintain this outside
    //of the gui parts :/
    ImVec4 m_messageColour;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    struct ProjectSettings final
    {
        std::string name;
        std::string id;

        //this is an absolute path to the project directory
        //so that it can be mounted / unmounted in the virtual FS
        std::filesystem::path workingDirectory;
    }m_projectSettings;

    //used to hold input from the new project window
    ProjectSettings m_newProjectBuffer;

    static constexpr std::size_t MaxHistorySize = 10;
    std::deque<std::filesystem::path> m_history;
    void addToHistory(const std::filesystem::path&);

    void loadProject(const std::filesystem::path&);
    void saveProject(const std::filesystem::path&);
    std::array<std::filesystem::path, 12u> getProjectDirectories(const std::string& id) const;
    bool validateProjectDirectories(const std::string& id) const;
    void setWindowTitle();

    struct EditorSettings final
    {
        std::filesystem::path lastSaveDirectory;
        std::filesystem::path lastLoadDirectory;

        bool previewNightMode = false;
        bool audioscapeChanged = true; //updated when selecting a new audioscape so we know if we need to rebuild on play
        bool audioscapePlaying = false;
    }m_editorSettings;

    void loadSettings();
    void saveSettings() const;
    void confirmQuit();

    //represents the course.data file
    struct CourseData final
    {
        //NOTE all paths are relative to assets
        //directory - that is they all start assets/golf
        std::filesystem::path skybox; //sbf
        std::filesystem::path shrubs; //shb
        std::filesystem::path instanceModel; //optional cmt
        std::filesystem::path audio; //xas

        cro::String displayTitle;
        std::string displayTitleBuffer; //needed for ImGui
        cro::String displayDescription;
        std::string displayDescriptionBuffer; //for imGui

        //note that these may be partially empty
        std::array<std::filesystem::path, 18u> holePaths = {};
    }m_courseData;
    void assertCourseDataPath(const std::filesystem::path& inPath, std::filesystem::path& dst);


    //lists the available resources, used to draw the interface
    std::size_t m_skyboxIndex;
    std::vector<std::filesystem::path> m_skyboxFiles;
    void refreshSkyboxFiles();
    std::size_t m_shrubIndex;
    std::vector<std::filesystem::path> m_shrubFiles;
    void refreshShrubFiles();
    //HMMM how to list instanced models without doing stupid things like instancing buildings?
    std::size_t m_audioIndex;
    std::vector<std::filesystem::path> m_audioFiles;
    void refreshAudioFiles(); //again we only want enviro files...

    struct AudioID final
    {
        enum
        {
            One, Two, Three,
            Four, Five, Six,
            Inc01, Inc02,
            Count
        };
    };
    struct AudioPreview final
    {
        std::filesystem::path inputFile;
        std::string label;
        std::string toolTip;

        float volume = 1.f;
        std::uint32_t channels = 1;

        AudioPreview() = default;
        AudioPreview(const std::string& l, const std::string& t, std::uint32_t c = 1)
            :label(l), toolTip(t), channels (c) { }
    };
    std::array<AudioPreview, AudioID::Count> m_audioPreviewsDay;
    std::array<AudioPreview, AudioID::Count> m_audioPreviewsNight;
    void exportAudioFile();
    
    struct AudioPaths final
    {
        std::filesystem::path base;
        std::filesystem::path day;
        std::filesystem::path night;
    };
    AudioPaths getAudioFilePaths() const;

    void createAudioPreview();
    void playAudioPreview();
    void pauseAudioPreview();
    std::array<cro::Entity, 6u> m_audioEntities = {};


    cro::Texture m_previewTexture;
    void loadPreviewTexture(const std::filesystem::path&);

    struct WindowID final
    {
        enum
        {
            NewProject, Properties,
            AudioEditor,

            Count
        };
    };
    std::array<std::pair<bool, std::function<void()>>, WindowID::Count> m_windowStates = {};

    //CourseStateWindows.cpp//
    void drawMenuBar();
    void drawNewProject();
    void drawProjectProperties();
    void drawAudioEditor();

    void menuConfirmThenOpen(const std::filesystem::path&);
    void menuNewProject();
    void menuOpenProject();
};