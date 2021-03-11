#include "WorldState.hpp"
#include "UIConsts.hpp"
#include "SharedStateData.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/util/Matrix.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

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

            }
            if (ImGui::MenuItem("Model Viewer/Importer", nullptr, nullptr))
            {
                requestStackPush(States::ModelViewer);
                unregisterWindows();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

}

void WorldState::drawInspector()
{
    auto [pos, size] = WindowLayouts[WindowID::Inspector];
    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Inspector##0", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
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
    if (ImGui::Begin("Browser##0", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::BeginTabBar("##0d0d0");

        if (ImGui::BeginTabItem("Models"))
        {
            if (ImGui::Button("Add Model"))
            {
                //browse model files
                auto path = cro::FileSystem::openFileDialogue("", "cmt");
                if (!path.empty())
                {
                    //TODO pre-process path correctly
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
                ImGui::Image(m.thumbnail.getTexture(), { 128.f, 128.f }, { 0.f, 0.f }, { 1.f, 1.f });
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
    ImGuizmo::SetRect(io.DisplaySize.x * ui::InspectorWidth, 0, io.DisplaySize.x - (ui::InspectorWidth * io.DisplaySize.x),
        io.DisplaySize.y - (ui::BrowserHeight * io.DisplaySize.y));

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

void WorldState::updateLayout(std::int32_t w, std::int32_t h)
{
    float width = static_cast<float>(w);
    float height = static_cast<float>(h);
    WindowLayouts[WindowID::Inspector] = 
        std::make_pair(glm::vec2(0.f, ui::TitleHeight),
            glm::vec2(width * ui::InspectorWidth, height - (ui::TitleHeight + ui::InfoBarHeight)));

    WindowLayouts[WindowID::Browser] =
        std::make_pair(glm::vec2(width * ui::InspectorWidth, height - (height * ui::BrowserHeight) - ui::InfoBarHeight),
            glm::vec2(width - (width * ui::InspectorWidth), height * ui::BrowserHeight));

    WindowLayouts[WindowID::Info] =
        std::make_pair(glm::vec2(0.f, height - ui::InfoBarHeight), glm::vec2(width, ui::InfoBarHeight));

    WindowLayouts[WindowID::ViewGizmo] =
        std::make_pair(glm::vec2(width - ui::ViewManipSize, ui::TitleHeight), glm::vec2(ui::ViewManipSize, ui::ViewManipSize));
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
    cro::ModelDefinition modelDef;
    if (modelDef.loadFromFile(m_sharedData.workingDirectory + path, m_resources, &m_environmentMap))
    {
        auto& mdl = m_models.emplace_back();
        mdl.modelDef = modelDef;
        mdl.modelID = 0; 
        //TODO we need to assign existing IDs if loading a scene and adjust the
        //free pool as necessary? Or does it matter as long as all existing IDs are correctly reassigned


        m_previewEntity = m_previewScene.createEntity();
        m_previewEntity.addComponent<cro::Transform>();
        modelDef.createModel(m_previewEntity, m_resources);

        m_previewScene.simulate(0.f);

        mdl.thumbnail.create(128, 128);
        mdl.thumbnail.clear();
        //draw the thumbnail
        m_previewScene.render(mdl.thumbnail);
        mdl.thumbnail.display();

        //TODO can we store thumbnails as images with scene?
        //TODO can we create thumbnails in the model importer?


        m_previewScene.destroyEntity(m_previewEntity);
    }
}