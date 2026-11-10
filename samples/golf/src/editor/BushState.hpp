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
    std::filesystem::path m_lastSkybox;

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

    struct ShaderUniforms final
    {
        std::uint32_t shaderID = 0;
        std::int32_t size = -1;
        std::int32_t randomness = -1;
        std::int32_t colour = -1;
        std::int32_t targetHeight = -1;
        std::int32_t colourRotation = -1;
    }m_shaderUniform;


    std::int32_t m_bushMaterial;
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

    bool m_editSkybox; //TODO remove this

    void addSystems();
    void loadAssets();
    void createScene();

    //assigned to camera resize callback
    void updateView(cro::Camera&);

    void registerWindows();
    void drawUI();

    struct UIWindow final
    {
        bool visible = false;
        std::function<void(bool*)> draw;
    };
    std::vector<UIWindow> m_windows;

    void drawMenuBar(bool*);
    void drawProperties(bool*);
    void drawBillboard(bool*);
    void drawThumbnail(bool*);
    void drawSkyboxEditor(bool*);

    void loadWindowStates();
    void saveWindowStates();

    void loadModel(const std::filesystem::path&);
    void loadPreset(const std::filesystem::path&);
    void savePreset(const std::filesystem::path&);
    void loadSkyboxFile();
    void saveSkyboxFile();
    void addSkyboxModel();

    void createThumbnails();
};