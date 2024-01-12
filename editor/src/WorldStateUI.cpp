/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2023
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "WorldState.hpp"
#include "UIConsts.hpp"
#include "SharedStateData.hpp"
#include "Palettiser.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/graphics/Image.hpp>

#include <crogine/util/Matrix.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <fstream>
#include <iomanip>

namespace
{
    enum WindowID
    {
        Inspector,
        Browser,
        ViewGizmo,
        Info,

        Count
    };

    std::array<std::pair<glm::vec2, glm::vec2>, WindowID::Count> WindowLayouts =
    {
        std::make_pair(glm::vec2(), glm::vec2())
    };

    void outputIcon(const cro::Image& img, const std::string& outPath)
    {
        CRO_ASSERT(img.getFormat() == cro::ImageFormat::RGBA, "");
        std::stringstream ss;
        ss << "static const unsigned char icon[] = {" << std::endl;
        ss << std::showbase << std::internal << std::setfill('0');

        auto i = 0;
        for (auto y = 0; y < 16; ++y)
        {
            for (auto x = 0; x < 16; ++x)
            {
                ss << std::hex << (int)img.getPixelData()[i++] << ", ";
                ss << std::hex << (int)img.getPixelData()[i++] << ", ";
                ss << std::hex << (int)img.getPixelData()[i++] << ", ";
                ss << std::hex << (int)img.getPixelData()[i++] << ", ";
            }
            ss << std::endl;
        }

        ss << "};";

        std::ofstream file(outPath);
        file << ss.rdbuf();
        file.close();
    }
}

void WorldState::initUI()
{
    loadPrefs();

    registerWindow(
        [&]()
        {
            //ImGui::ShowDemoWindow();
            drawMenuBar();
            drawInspector();
            drawBrowser();
            drawInfo();
            drawGizmo();
        });



    auto size = getContext().mainWindow.getSize();
    updateLayout(size.x, size.y);
}

void WorldState::drawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        //file menu
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", nullptr, nullptr))
            {

            }
            if (ImGui::MenuItem("Save", nullptr, nullptr))
            {

            }

            if (ImGui::MenuItem("Save As...", nullptr, nullptr))
            {

            }
            /*if (ImGui::MenuItem("Import", nullptr, nullptr))
            {

            }*/
            if (ImGui::MenuItem("Quit", nullptr, nullptr))
            {
                cro::App::quit();
            }
            ImGui::EndMenu();
        }

        //view menu
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Options", nullptr, nullptr))
            {
                m_showPreferences = !m_showPreferences;
            }
            if (ImGui::MenuItem("Model Viewer/Importer", nullptr, nullptr))
            {
                requestStackPush(States::ModelViewer);
                unregisterWindows();
            }
            if (ImGui::MenuItem("Particle Editor", nullptr, nullptr))
            {
                requestStackPush(States::ParticleEditor);
                unregisterWindows();
            }
            if (ImGui::MenuItem("Layout Editor", nullptr, nullptr))
            {
                requestStackPush(States::LayoutEditor);
                unregisterWindows();
            }
            if (ImGui::MenuItem("Sprite Editor", nullptr, nullptr))
            {
                requestStackPush(States::SpriteEditor);
                unregisterWindows();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Set Skybox", nullptr, nullptr))
            {
                auto path = cro::FileSystem::openFileDialogue("", "ccm");
                if (!path.empty())
                {
                    m_scene.setCubemap(path);
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Convert Icon"))
            {
                auto path = cro::FileSystem::openFileDialogue("", "png,jpg,bmp");
                if (!path.empty())
                {
                    cro::Image img(true);
                    if (img.loadFromFile(path))
                    {
                        if (img.getSize().x == 16 &&
                            img.getSize().y == 16)
                        {
                            path.clear();
                            path = cro::FileSystem::saveFileDialogue("", "hpp");
                            if (!path.empty())
                            {
                                outputIcon(img, path);
                            }
                        }
                        else
                        {
                            cro::FileSystem::showMessageBox("","Image not 16x16");
                        }
                    }
                }
            }
            ImGui::SameLine();
            uiConst::showToolTip("Convert a 16x16 image to byte array");

            if (ImGui::MenuItem("Image To Array"))
            {
                auto path = cro::FileSystem::openFileDialogue("", "png,jpg,bmp");
                if (!path.empty())
                {
                    cro::Image img;
                    if (img.loadFromFile(path))
                    {
                        auto outpath = cro::FileSystem::saveFileDialogue("", "hpp");
                        if (!outpath.empty() &&
                            imageToArray(img, outpath))
                        {
                            cro::FileSystem::showMessageBox("Success", "Header File Written Successfully");
                        }
                    }
                }
            }
            ImGui::SameLine();
            uiConst::showToolTip("Convert an image to an array of cro::Colour");

            if (ImGui::MenuItem("Create Look-up Palette"))
            {
                auto path = cro::FileSystem::openFileDialogue("", "png,jpg,bmp");
                if (!path.empty())
                {
                    cro::Image img;
                    if (img.loadFromFile(path))
                    {
                        auto outpath = cro::FileSystem::saveFileDialogue("", "png");
                        if (!outpath.empty() &&
                            pt::processPalette(img, outpath))
                        {
                            cro::FileSystem::showMessageBox("Success", "Palette File Written Successfully");
                        }
                    }
                }
            }
            ImGui::SameLine();
            uiConst::showToolTip("Creates a look-up table for the given image palette\nYou probably don't want to do this with large images.");

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (m_showPreferences)
    {
        drawOptions();
    }
}

void WorldState::drawInspector()
{
    auto [pos, size] = WindowLayouts[WindowID::Inspector];
    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Inspector##0", nullptr, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        if (ImGui::BeginTabBar("##0"))
        {
            if (ImGui::BeginTabItem("Navigator"))
            {
                //imgui demo 706

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Selected"))
            {
                if (m_selectedEntity.isValid())
                {
                    auto name = m_selectedEntity.getLabel();
                    if (name.empty())
                    {
                        name = "Untitled";
                    }
                    if (ImGui::InputText("Name", &name))
                    {
                        if (name.empty())
                        {
                            name = "Untitled";
                        }
                        m_selectedEntity.setLabel(name);
                    }


                    auto& tx = m_selectedEntity.getComponent<cro::Transform>();
                    auto pos = tx.getPosition();
                    if (ImGui::DragFloat3("Position", &pos[0], 0.1f))
                    {
                        tx.setPosition(pos);
                    }
                    auto scale = tx.getScale();
                    if (ImGui::DragFloat3("Scale", &scale[0], 0.1f))
                    {
                        tx.setScale(scale);
                    }
                    auto rotation = glm::degrees(glm::eulerAngles(tx.getRotation()));
                    if (ImGui::DragFloat3("Rotation", &rotation[0], 0.1f))
                    {
                        glm::quat q(glm::radians(rotation));
                        tx.setRotation(q);
                    }
                }
                else
                {
                    ImGui::Text("Nothing Selected");
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("World Properties"))
            {
                auto cc = getContext().appInstance.getClearColour();
                float c[3]{cc.getRed(),cc.getGreen(),cc.getBlue()};

                if (ImGui::ColorPicker3("Sky Colour", c))
                {
                    getContext().appInstance.setClearColour(cro::Colour(c[0], c[1], c[2]));
                    //savePrefs(); //this probably happens too frequently
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void WorldState::drawBrowser()
{
    auto [pos, size] = WindowLayouts[WindowID::Browser];

    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Browser##0", nullptr, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::BeginTabBar("##0d0d0");

        if (ImGui::BeginTabItem("Models"))
        {
            if (ImGui::Button("Add Model"))
            {
                if (m_sharedData.workingDirectory.empty())
                {
                    if (cro::FileSystem::showMessageBox("", "Working directory currently not set. Would you like to set one now?", cro::FileSystem::YesNo, cro::FileSystem::Question))
                    {
                        auto path = cro::FileSystem::openFolderDialogue(m_sharedData.workingDirectory);
                        if (!path.empty())
                        {
                            m_sharedData.workingDirectory = path;
                            std::replace(m_sharedData.workingDirectory.begin(), m_sharedData.workingDirectory.end(), '\\', '/');
                        }
                    }
                }

                //browse model files
                auto path = cro::FileSystem::openFileDialogue("", "cmt");
                if (!path.empty())
                {
                    std::replace(path.begin(), path.end(), '\\', '/');
                    if (path.find(m_sharedData.workingDirectory) == std::string::npos)
                    {
                        cro::FileSystem::showMessageBox("Warning", "This model was not opened from the current working directory.");
                    }

                    openModel(path);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Import Model"))
            {
                //launch model importer
                requestStackPush(States::ModelViewer);
                unregisterWindows();
            }

            ImGui::SameLine();
            //TODO if selected
            if (ImGui::Button("Remove"))
            {
                //TODO warn this will remove all instances
                //TODO remove model from project
            }

            ImGui::BeginChild("##modelChild");
            //TODO layout the thumb nails / drag n drop

            for (const auto& m : m_models)
            {
                ImGui::Image(m.thumbnail.getTexture(), { 128.f, 128.f }, { 0.f, 1.f }, { 1.f, 0.f });
                ImGui::SameLine();
            }


            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Collision"))
        {
            //TODO is this really what we want?
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void WorldState::drawInfo()
{
    auto [pos, size] = WindowLayouts[WindowID::Info];
    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("InfoBar", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, m_messageColour);
        ImGui::Text("%s", cro::Console::getLastOutput().c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine();
        auto cursor = ImGui::GetCursorPos();
        cursor.y -= 4.f;
        ImGui::SetCursorPos(cursor);

        if (ImGui::Button("More...", ImVec2(56.f, 20.f)))
        {
            cro::Console::show();
        }
    }
    ImGui::End();
}

void WorldState::drawGizmo()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(io.DisplaySize.x * uiConst::InspectorWidth, 0, io.DisplaySize.x - (uiConst::InspectorWidth * io.DisplaySize.x),
        io.DisplaySize.y - (uiConst::BrowserHeight * io.DisplaySize.y));

    auto [pos, size] = WindowLayouts[WindowID::ViewGizmo];

    //view cube doohickey
    auto tx = glm::inverse(m_entities[EntityID::ArcBall].getComponent<cro::Transform>().getLocalTransform());
    ImGuizmo::ViewManipulate(&tx[0][0], 10.f, ImVec2(pos.x, pos.y), ImVec2(size.x, size.y), 0/*x10101010*/);
    m_entities[EntityID::ArcBall].getComponent<cro::Transform>().setRotation(glm::inverse(tx));

    //tooltip - TODO change this as it's not accurate
    if (io.MousePos.x > pos.x && io.MousePos.x < pos.x + size.x
        && io.MousePos.y > pos.y && io.MousePos.y < pos.y + size.y)
    {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, 0);
        ImGui::PushStyleColor(ImGuiCol_Border, 0);
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);

        auto p = ImGui::GetCursorPos();
        ImGui::PushStyleColor(ImGuiCol_Text, 0xff000000);
        ImGui::TextUnformatted("Left Click Rotate\nMiddle Mouse Pan\nScroll Zoom");
        ImGui::PopStyleColor();

        p.x -= 1.f;
        p.y -= 1.f;
        ImGui::SetCursorPos(p);
        ImGui::TextUnformatted("Left Click Rotate\nMiddle Mouse Pan\nScroll Zoom");

        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
        ImGui::PopStyleColor(2);
    }

    //draw gizmo if selected ent is valid
    if (m_selectedEntity.isValid())
    {
        const auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
        tx = m_selectedEntity.getComponent<cro::Transform>().getLocalTransform();
        ImGuizmo::Manipulate(&cam.getActivePass().viewMatrix[0][0], &cam.getProjectionMatrix()[0][0], static_cast<ImGuizmo::OPERATION>(m_gizmoMode), ImGuizmo::MODE::LOCAL, &tx[0][0]);
        m_selectedEntity.getComponent<cro::Transform>().setLocalTransform(tx);
    }
}

void WorldState::drawOptions()
{
    if (m_showPreferences)
    {
        ImGui::SetNextWindowSize({ 400.f, 260.f });
        if (ImGui::Begin("Preferences##world", &m_showPreferences))
        {
            ImGui::Text("%s", "Working Directory:");
            if (m_sharedData.workingDirectory.empty())
            {
                ImGui::Text("%s", "Not Set");
            }
            else
            {
                auto dir = m_sharedData.workingDirectory.substr(0, 30) + "...";
                ImGui::Text("%s", dir.c_str());
                ImGui::SameLine();
                uiConst::showToolTip(m_sharedData.workingDirectory.c_str());
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse"))
            {
                auto path = cro::FileSystem::openFolderDialogue(m_sharedData.workingDirectory);
                if (!path.empty())
                {
                    m_sharedData.workingDirectory = path;
                    std::replace(m_sharedData.workingDirectory.begin(), m_sharedData.workingDirectory.end(), '\\', '/');
                }
            }

            ImGui::NewLine();
            ImGui::Separator();
            ImGui::NewLine();

            auto skyColour = getContext().appInstance.getClearColour();
            if (ImGui::ColorEdit3("Sky Colour", skyColour.asArray()))
            {
                getContext().appInstance.setClearColour(skyColour);
            }

            ImGui::NewLine();
            ImGui::NewLine();
            if (!m_showPreferences ||
                ImGui::Button("Close"))
            {
                savePrefs();
                m_showPreferences = false;
            }
        }
        ImGui::End();
    }
}

void WorldState::updateLayout(std::int32_t w, std::int32_t h)
{
    float width = static_cast<float>(w);
    float height = static_cast<float>(h);
    WindowLayouts[WindowID::Inspector] = 
        std::make_pair(glm::vec2(0.f, uiConst::TitleHeight),
            glm::vec2(width * uiConst::InspectorWidth, height - (uiConst::TitleHeight + uiConst::InfoBarHeight)));

    WindowLayouts[WindowID::Browser] =
        std::make_pair(glm::vec2(width * uiConst::InspectorWidth, height - (height * uiConst::BrowserHeight) - uiConst::InfoBarHeight),
            glm::vec2(width - (width * uiConst::InspectorWidth), height * uiConst::BrowserHeight));

    WindowLayouts[WindowID::Info] =
        std::make_pair(glm::vec2(0.f, height - uiConst::InfoBarHeight), glm::vec2(width, uiConst::InfoBarHeight));

    WindowLayouts[WindowID::ViewGizmo] =
        std::make_pair(glm::vec2(width - uiConst::ViewManipSize, uiConst::TitleHeight), glm::vec2(uiConst::ViewManipSize, uiConst::ViewManipSize));
}

void WorldState::updateMouseInput(const cro::Event& evt)
{
    //TODO this isn't really consistent with the controls for model viewer...

    const float moveScale = 0.004f;
    if (evt.motion.state & SDL_BUTTON_RMASK)
    {
        float pitchMove = static_cast<float>(evt.motion.yrel) * moveScale * m_viewportRatio;
        float yawMove = static_cast<float>(evt.motion.xrel) * moveScale;

        auto& tx = m_scene.getActiveCamera().getComponent<cro::Transform>();
        tx.rotate(cro::Transform::X_AXIS, -pitchMove);
        tx.rotate(cro::Transform::Y_AXIS, -yawMove);
    }
    //else if (evt.motion.state & SDL_BUTTON_RMASK)
    //{
    //    //do roll
    //    float rollMove = static_cast<float>(-evt.motion.xrel) * moveScale;

    //    auto& tx = m_entities[EntityID::ArcBall].getComponent<cro::Transform>();
    //    tx.rotate(cro::Transform::Z_AXIS, -rollMove);
    //}
    else if (evt.motion.state & SDL_BUTTON_MMASK)
    {
        auto camTx = m_scene.getActiveCamera().getComponent<cro::Transform>().getWorldTransform();

        auto& tx = m_entities[EntityID::ArcBall].getComponent<cro::Transform>();
        tx.move(cro::Util::Matrix::getRightVector(camTx) * -static_cast<float>(evt.motion.xrel) / 160.f);
        tx.move(cro::Util::Matrix::getUpVector(camTx) * static_cast<float>(evt.motion.yrel) / 160.f);
    }
}

void WorldState::openModel(const std::string& path)
{
    cro::ModelDefinition modelDef(m_resources, &m_environmentMap, m_sharedData.workingDirectory);
    if (modelDef.loadFromFile(path))
    {
        auto& mdl = m_models.emplace_back();
        //mdl.modelDef = modelDef;
        mdl.modelID = 0; 
        //TODO we need to assign existing IDs if loading a scene and adjust the
        //free pool as necessary? Or does it matter as long as all existing IDs are correctly reassigned


        m_previewEntity = m_previewScene.createEntity();
        m_previewEntity.addComponent<cro::Transform>().rotate(glm::vec3(0.f, 1.f, 0.f), 45.f * cro::Util::Const::degToRad);
        //m_previewEntity.getComponent<cro::Transform>().rotate(glm::vec3(0.f, 0.f, 1.f), 25.f * cro::Util::Const::degToRad);
        modelDef.createModel(m_previewEntity);

        auto bb = m_previewEntity.getComponent<cro::Model>().getMeshData().boundingSphere;

        m_previewEntity.getComponent<cro::Transform>().move(-bb.centre);
        m_previewEntity.getComponent<cro::Transform>().move({ 0.f, 0.f, -bb.radius }); //TODO move back until no longer intersecting fristum

        m_previewScene.simulate(0.f);

        mdl.thumbnail.create(128, 128);
        mdl.thumbnail.clear();
        //draw the thumbnail
        m_previewScene.render();
        mdl.thumbnail.display();

        //TODO can we store thumbnails as images with scene?
        //TODO can we create thumbnails in the model importer?


        m_previewScene.destroyEntity(m_previewEntity);
    }
}

bool WorldState::imageToArray(const cro::Image& img, const std::string& outpath) const
{
    auto type = img.getFormat();
    std::int32_t stride = 0;
    switch (type)
    {
    case cro::ImageFormat::A:
    case cro::ImageFormat::None:
    default: return false;
    case cro::ImageFormat::RGB:
        stride = 3;
        break;
    case cro::ImageFormat::RGBA:
        stride = 4;
        break;
    }

    std::vector<std::uint32_t> colours;
    auto imageSize = img.getSize().x * img.getSize().y * stride;
    for (auto i = 0u; i < imageSize; i += stride)
    {
        std::uint32_t colour = 0xff;
        std::int32_t shift = 24;

        for (auto j = 0; j < stride; ++j)
        {
            std::uint32_t byte = img.getPixelData()[i + j];
            colour |= (byte << shift);
            shift -= 8;
        }

        if (auto result = std::find_if(colours.begin(), colours.end(), 
            [colour](std::uint32_t c) 
            {
                return c == colour;
            }); result == colours.end())
        {
            colours.push_back(colour);
        }
    }

    if (!colours.empty())
    {
        std::ofstream file(outpath);
        if (file.is_open() && file.good())
        {
            file << "#pragma once\n\n#include <array>\n#include <crogine/graphics/Colour.hpp>\n\n";
            file << "static constexpr inline std::array<cro::Colour, " << colours.size() << "u> Colours =\n{\n";

            for (auto c : colours)
            {
                file << "    cro::Colour(0x" << std::hex << c << "),\n";
            }

            file << "};";

            return true;
        }

        return false;
    }

    return false;
}