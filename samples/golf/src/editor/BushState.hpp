/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2026
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
#include "../golf/GameConsts.hpp"

#include <crogine/core/ConsoleClient.hpp>
#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/Palette.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/gui/detail/imfilebrowser.h>

#include <array>
#include <filesystem>

namespace cro
{
    struct Camera;
}

struct SharedStateData;
class BushState final : public cro::State, public cro::GuiClient, public cro::ConsoleClient
{
public:
    BushState(cro::StateStack&, cro::State::Context, const SharedStateData&);
    ~BushState() = default;

    cro::StateID getStateID() const override { return StateID::Bush; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void preRender() override;
    void render() override;

private:
    const SharedStateData& m_sharedData;
    cro::SimpleQuad m_backgroundQuad; //this renders a shaded background in case this state is rendered on top of another
    cro::Scene m_gameScene;
    cro::Scene m_skyScene;

    cro::ResourceCollection m_resources;
    cro::UniformBuffer<WindData> m_windBuffer;
    cro::UniformBuffer<ResolutionData> m_resolutionBuffer;
    cro::UniformBuffer<float> m_scaleBuffer;

    std::filesystem::path m_lastTreeset;

    //----model preview-----

    struct ModelID final
    {
        enum
        {
            Default, Single, Instanced,

            Count
        };
    };
    cro::Entity m_root;
    std::array<cro::Entity, ModelID::Count> m_models = {};
    std::vector<glm::mat4> m_instanceTransforms;

    struct MaterialSlot final
    {
        std::array<cro::Material::Data, 2u> materials = {};
        std::int32_t activeMaterial = 0;
    };
    std::vector<MaterialSlot> m_materials;

    struct TreesetData final
    {
        float leafSize = 0.25f; //metres
        float randomAmount = 0.2f;
        float colourRotation = 0.25f;
        float scale = 1.f;
        glm::vec3 colour = glm::vec3(1.f);

        std::filesystem::path modelPath;
        std::filesystem::path texturePath;

    }m_treeset;

    struct BushShaderID final
    {
        enum
        {
            Bush,
            BushShadow,
            Branch,

            Count
        };
    };

    struct ShaderUniforms final
    {
        std::uint32_t shaderID = 0;
        std::int32_t size = -1;
        std::int32_t randomness = -1;
        std::int32_t colour = -1;
        std::int32_t targetHeight = -1;
        std::int32_t colourRotation = -1;
    };
    std::array<ShaderUniforms, BushShaderID::Count> m_shaderUniforms = {};

    std::int32_t m_bushMaterial;
    std::int32_t m_bushShadowMaterial;
    std::int32_t m_branchMaterial;

    float m_rotation = 0.f;
    cro::Texture* m_leafTexture;

    WindData m_windData;
    float m_windRotation;

    cro::Palette m_palette;
    glm::vec2 m_rootRotation;
    float m_billboardScaleMultiplier;

    cro::Colour m_skyMid;
    cro::Colour m_skyTop;


    //----scene rendering-----

    struct WindowIndex final
    {
        enum
        {
            MenuBar,
            Properties, Billboard,
            Skybox, Thumbnails, //TODO move these to other parts of editor
            Count
        };
    };
    std::array<std::int32_t, WindowIndex::Count> m_windowIndices = {};

    cro::RenderTexture m_billboardTexture;
    cro::Entity m_billboardCamera;

    cro::RenderTexture m_thumbnailTexture;
    cro::Entity m_thumbnailCamera;

    void addSystems();
    void loadAssets();
    void createScene();

    //assigned to camera resize callback
    void updateView(cro::Camera&);


    //----collision data----


    //internal note - bullet defines cylinder via a vec 3 (radius, halfHeight, radius)
    struct CollisionObject final
    {
        enum
        {
            Sphere, Cylinder,

            TypeCount
        };
        std::uint32_t type = Sphere;
        /*float radius = 1.f; //TODO - do we ned these as they are inferred from the preview transform
        float halfLength = 1.f;*/
        cro::Entity preview;
    };
    std::vector<CollisionObject> m_collisionObjects;
    std::size_t m_selectedCollisionObject;

    std::array<std::unique_ptr<cro::ModelDefinition>, CollisionObject::TypeCount> m_collisionDefs = {};
    ImGuizmo::OPERATION m_gizmoMode;

    //-----windowing----
    void registerWindows();
    void drawUI();

    void loadModel(const std::filesystem::path&);
    void loadPreset(const std::filesystem::path&);
    void savePreset(const std::filesystem::path&);
    void loadSkyboxFile();
    void saveSkyboxFile();
    void addSkyboxModel();

    void createThumbnails();

    struct UIWindow final
    {
        bool visible = false;
        std::function<void(bool*)> draw;
    };
    std::vector<UIWindow> m_windows;

    struct OpenDialogueState final
    {
        enum
        {
            Model, Treeset,

            None
        };
    };
    std::int32_t m_openDialogueState;
    ImGui::FileBrowser m_openFileBrowser;
    std::array<std::filesystem::path, OpenDialogueState::None> m_openFilePaths = {};

    struct SaveDialogueState final
    {
        enum
        {
            Treeset, Billboard,

            None
        };
    };
    std::int32_t m_saveDialogueState;
    ImGui::FileBrowser m_saveFileBrowser;
    std::array<std::filesystem::path, SaveDialogueState::None> m_saveFilePaths = {};


    //BushStateWndows.cpp
    void drawMenuBar(bool*);
    void drawProperties(bool*);
    void drawCollisionPanel();
    void drawBillboard(bool*);
    void drawThumbnail(bool*);
    void drawSkyboxEditor(bool*);

    void selectCollisionObject(std::size_t);
    void drawCollisionManipulator();
    void drawViewMainpulator();


    void showModelOpenDialogue();
    void showPresetOpenDialogue();
    void handleFileDialogues();

    void loadWindowStates();
    void saveWindowStates();
};