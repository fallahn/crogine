/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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

#include <crogine/core/State.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/gui/GuiClient.hpp>

#include "StateIDs.hpp"
#include "ImportedMeshBuilder.hpp"
#include "MaterialDefinition.hpp"

#include <map>
#include <memory>
#include <optional>

class ModelState final : public cro::State, public cro::GuiClient
{
public:
	ModelState(cro::StateStack&, cro::State::Context);

	cro::StateID getStateID() const override { return States::ModelViewer; }

	bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
	bool simulate(float) override;
	void render() override;

private:

    cro::Scene m_scene;
    cro::Scene m_previewScene;
    cro::ResourceCollection m_resources;

    float m_fov;

    struct Preferences final
    {
        std::string workingDirectory;
        std::size_t unitsPerMetre = 2;
        cro::Colour skyBottom = cro::Colour(0.82f, 0.98f, 0.99f);
        cro::Colour skyTop = cro::Colour(0.21f, 0.5f, 0.96f);

        std::string lastImportDirectory;
        std::string lastExportDirectory;
        std::string lastModelDirectory;
    }m_preferences;
    bool m_showPreferences;
    bool m_showGroundPlane;
    bool m_showSkybox;

    cro::ConfigFile m_currentModelConfig;

    void addSystems();
    void loadAssets();
    void createScene();
    void buildUI();

    void openModel();
    void openModelAtPath(const std::string&);
    void closeModel();

    CMFHeader m_importedHeader;
    std::vector<float> m_importedVBO;
    std::vector<std::vector<std::uint32_t>> m_importedIndexArrays;
    struct ImportTransform final
    {
        glm::vec3 rotation = glm::vec3(0.f);
        float scale = 1.f;
    }m_importedTransform;
    void importModel();
    void exportModel();
    void applyImportTransform();

    void importIQM(const std::string&); //used to apply modified transforms

    void loadPrefs();
    void savePrefs();

    bool m_showAABB;
    bool m_showSphere;
    void updateWorldScale();
    void updateNormalVis();
    void updateGridMesh(cro::Mesh::Data&, std::optional<float>, std::optional<cro::Box>);

    void updateMouseInput(const cro::Event&);


    struct MaterialTexture final
    {
        std::unique_ptr<cro::Texture> texture;
        std::string name;
        std::string relPath; //inc trailing '/'
    };
    std::map<std::uint32_t, MaterialTexture> m_materialTextures;
    std::uint32_t m_selectedTexture;
    std::uint32_t addTextureToBrowser(const std::string&);
    void addMaterialToBrowser(MaterialDefinition&&);

    cro::Texture m_blackTexture;
    cro::Texture m_magentaTexture;
    std::vector<MaterialDefinition> m_materialDefs;
    std::size_t m_selectedMaterial;
    cro::Entity m_previewEntity;
    void applyPreviewSettings(MaterialDefinition&);

    std::vector<std::int32_t> m_activeMaterials; //indices into the materials array of materials used on the currently open model. -1 means default material is applied

    void updateLayout(std::int32_t, std::int32_t);
    void drawInspector();
    void drawBrowser();
    void exportMaterial();
    void importMaterial(const std::string&);
};