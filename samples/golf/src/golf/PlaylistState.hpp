/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2025
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
#include "GameConsts.hpp"
#include "CollisionMesh.hpp"

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/gui/GuiClient.hpp>

struct SharedStateData;

namespace cro
{
    class SpriteSheet;
}

class PlaylistState final : public cro::State, public cro::GuiClient
{
public:
    PlaylistState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Playlist; }

private:

    cro::Scene m_skyboxScene;
    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    SharedStateData& m_sharedData;

    cro::ResourceCollection m_resources;
    cro::RenderTexture m_gameSceneTexture;

    CollisionMesh m_collisionMesh;

    cro::UniformBuffer<float> m_scaleBuffer;
    cro::UniformBuffer<ResolutionData> m_resolutionBuffer;
    cro::UniformBuffer<WindData> m_windBuffer;
    struct ResolutionUpdate final
    {
        ResolutionData resolutionData;
        float targetFade = 0.1f;
    }m_resolutionUpdate;

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    struct MenuID final
    {
        enum
        {
            Skybox, Shrubbery,
            Holes, FileSystem,

            Popup,

            Dummy,
            Count
        };
    };
    std::array<cro::Entity, MenuID::Count> m_menuEntities = {};
    std::array<cro::Entity, MenuID::Count> m_tabEntities = {};

    void updateTabIndices();

    enum //button indices
    {
        SkyboxUp = 100,
        SkyboxDown,
        SkyboxList,
        
        ShrubberyUp = 200,
        ShrubberyDown,
        ShrubberyLeft,
        ShrubberyRight,
        ShrubberyList,

        Holes = 300,
        HolesCourseUp,
        HolesCourseDown,
        HolesLeft,
        HolesRight,
        HolesPlusPriority,
        HolesMinusPriority,
        HolesAdd,
        HolesRemove,
        HolesClear,
        HolesListUp,
        HolesListDown,
        HolesCourseList,
        HolesList = HolesCourseList + 20,

        FileSystemSave = 400,
        FileSystemExport,
        FileSystemDown,
        FileSystemUp,
        FileSystemShow,
        FileSystemList,

        TabSkybox = 500,
        TabShrub,
        TabHoles,
        TabFileSystem,

        QuitSkybox = 600,
        QuitShrubbery,
        QuitHoles,
        QuitFileSystem,

        HelpSkybox = 700,
        HelpShrubbery,
        HelpHoles,
        HelpFileSystem
    };

    struct MaterialID final
    {
        enum
        {
            Horizon, Water, Cloud,
            Cel, CelTextured, CelTexturedSkinned,
            Course, Billboard, BillboardShadow,
            Branch, Leaf,
            BranchShadow, LeafShadow,

            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    glm::vec2 m_viewScale;
    
    struct CourseData final
    {
        std::string skyboxPath;
        std::string shrubPath;
    }m_courseData;
    std::vector<std::string> m_skyboxes;
    std::size_t m_skyboxIndex;
    cro::Entity m_cloudRing;

    std::vector<glm::vec3> m_grassDistribution;
    std::vector<glm::vec3> m_flowerDistribution;
    std::vector<glm::vec3> m_treeDistribution;

    std::vector<std::string> m_shrubs;
    std::size_t m_shrubIndex;

    struct Shrubbery final
    {
        std::array<cro::Entity, 2u> billboardEnts = {}; //high q and classic
        std::array<cro::Entity, 2u> treeBillboardEnts = {};
        std::array<cro::Entity, 4u> treesetEnts = {};

        void hide()
        {
            for (auto e : billboardEnts)
            {
                if (e.isValid())
                {
                    e.getComponent<cro::Model>().setHidden(true);
                }
            }

            for (auto e : treeBillboardEnts)
            {
                if (e.isValid())
                {
                    e.getComponent<cro::Model>().setHidden(true);
                }
            }

            for (auto e : treesetEnts)
            {
                if (e.isValid())
                {
                    e.getComponent<cro::Model>().setHidden(true);
                }
            }
        }
    };
    std::vector<Shrubbery> m_shrubberyModels;

    struct Thumbnail final
    {
        std::string name;
        cro::Entity thumbEnt;
        glm::vec2 defaultScale = glm::vec2(1.f);
    };

    struct HoleDirectory final
    {
        std::string name;
        std::vector<Thumbnail> holes;
        cro::Entity thumbEnt;
    };
    std::vector<HoleDirectory> m_holeDirs;
    std::size_t m_holeDirIndex;
    std::size_t m_thumbnailIndex;

    cro::Entity m_holePreview;

    struct PlaylistEntry final
    {
        std::string name;
        std::size_t courseIndex = 0;
        std::size_t holeIndex = 0;
        std::size_t currentIndex = 0; //current position in the playlist so we can correctly update when adding/removing items
        cro::Entity uiNode;
    };
    std::vector<PlaylistEntry> m_playlist;
    std::size_t m_playlistIndex;
    cro::Entity m_playlistScrollNode;

    std::vector<std::string> m_saveFiles;
    std::size_t m_saveFileIndex;
    cro::Entity m_saveFileScrollNode;

    struct CallbackID final
    {
        enum
        {
            SkyScrollUp,
            SkyScrollDown,

            ShrubScrollUp,
            ShrubScrollDown,

            HoleDirScrollUp,
            HoleDirScrollDown,

            PlaylistScrollUp,
            PlaylistScrollDown,

            SaveScrollUp,
            SaveScrollDown,

            Count
        };
    };
    std::array<std::function<void(cro::Entity, const cro::ButtonEvent&)>, CallbackID::Count> m_callbacks = {};
    std::uint32_t m_playlistActivatedCallback;
    std::uint32_t m_playlistSelectedCallback;
    std::uint32_t m_playlistUnselectedCallback;
    
    std::array<std::vector<cro::Entity>, MenuID::Count> m_sliders = {};
    cro::Entity m_activeSlider;

    struct PopupID final
    {
        enum
        {
            TextSelected,
            TextUnselected,
            SaveNew,
            SaveOverwrite,
            LoadFile,
            ClosePopup,
            QuitState,

            Count
        };
    };
    std::array<std::uint32_t, PopupID::Count> m_popupIDs = {};

    //really we should be using menu ID here
    struct AnimationID final
    {
        enum
        {
            TabSkybox,
            TabShrubs,
            TabHoles,
            TabSaveLoad,

            Count
        };
    };
    std::array<std::int32_t, AnimationID::Count> m_animationIDs = {};
    cro::FloatRect m_croppingArea;

    void addSystems();
    void loadAssets();
    void buildScene();
    void buildUI();

    struct MenuData final
    {
        std::uint32_t scrollSelected = 0;
        std::uint32_t scrollUnselected = 0;
        std::uint32_t textSelected = 0;
        std::uint32_t textUnselected = 0;
        cro::SpriteSheet* spriteSheet = nullptr;
    };
    void createSkyboxMenu(cro::Entity, const MenuData&);
    void createShrubberyMenu(cro::Entity, const MenuData&);
    void createHoleMenu(cro::Entity, const MenuData&);
    void createFileSystemMenu(cro::Entity, const MenuData&);
    void addSaveFileItem(std::size_t, glm::vec2);

    std::int32_t m_currentTab;
    void setActiveTab(std::int32_t);
    void loadShrubbery(const std::string&);
    void applyShrubQuality();
    void updateNinePatch(cro::Entity);

    cro::Entity m_infoEntity;
    void updateInfo();

    enum class EntryType : std::uint8_t
    {
        Skybox, Shrub, Audio, Hole
    };
    struct SaveFileEntry final
    {
        EntryType type = EntryType::Skybox;
        std::uint8_t directory = 0;
        std::uint8_t file = 0;
        std::uint8_t padding = 0;
    };

    void confirmSave();
    void confirmLoad(std::size_t);
    void showExportResult(bool);
    void saveCourse(bool);
    void loadCourse();
    bool exportCourse();

    /*cro::Entity m_toolTip;
    void showToolTip(const std::string&);
    void hideToolTip();*/
    void showHelp();

    void quitState();
};