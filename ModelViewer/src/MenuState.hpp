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
#include <crogine/graphics/ResourceAutomation.hpp>
#include <crogine/gui/GuiClient.hpp>

#include "StateIDs.hpp"
#include "ImportedMeshBuilder.hpp"

/*!
Creates a state to render a menu.
*/
class MenuState final : public cro::State, public cro::GuiClient
{
public:
	MenuState(cro::StateStack&, cro::State::Context);
	~MenuState() = default;

	cro::StateID getStateID() const override { return States::MainMenu; }

	bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
	bool simulate(float) override;
	void render() override;

private:

    cro::Scene m_scene;
    cro::ResourceCollection m_resources;
    
    float m_zoom;

    struct Preferences final
    {
        std::string workingDirectory;
        std::size_t unitsPerMetre = 2;
    }m_preferences;
    bool m_showPreferences;
    bool m_showGroundPlane;

    cro::ConfigFile m_currentModelConfig;

    void addSystems();
    void loadAssets();
    void createScene();
    void buildUI();

    void openModel();
    void openModelAtPath(const std::string&);
    void closeModel();
    std::string m_lastImportPath;
    std::string m_lastExportPath;

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

    void updateWorldScale();
    void updateNormalVis();
};