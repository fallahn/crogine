/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
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

#include "SpriteState.hpp"
#include "SharedStateData.hpp"
#include "UIConsts.hpp"
#include "Messages.hpp"
#include "GLCheck.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/gui/Gui.hpp>

namespace
{
    std::int32_t nameNumber = 0; //hack to increment names
    std::string newSpriteName;
}

void SpriteState::openSprite(const std::string& path)
{
    if (m_spriteSheet.loadFromFile(path, m_textures, m_sharedData.workingDirectory + "/"))
    {
        auto* texture = m_spriteSheet.getTexture();
        m_entities[EntityID::Root].getComponent<cro::Sprite>().setTexture(*texture);
        m_entities[EntityID::Root].getComponent<cro::Drawable2D>().setCullingEnabled(false);

        auto size = glm::vec2(texture->getSize());
        auto position = glm::vec2(cro::App::getWindow().getSize()) - size;
        position /= 2.f;
        m_entities[EntityID::Root].getComponent<cro::Transform>().setPosition(position);


        //update the border (transparent textures won't easliy stand out from the background)
        m_entities[EntityID::Border].getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f), cro::Colour::Cyan),
                cro::Vertex2D(glm::vec2(0.f, size.y), cro::Colour::Cyan),
                cro::Vertex2D(glm::vec2(size), cro::Colour::Cyan),
                cro::Vertex2D(glm::vec2(size.x, 0.f), cro::Colour::Cyan),
                cro::Vertex2D(glm::vec2(0.f), cro::Colour::Cyan),
            }
        );

        m_currentFilePath = path;
        m_activeSprite = nullptr;
    }
}

void SpriteState::save()
{
    if (!m_currentFilePath.empty())
    {
        m_spriteSheet.saveToFile(m_currentFilePath);
    }
    else
    {
        saveAs();
    }
}

void SpriteState::saveAs()
{
    auto path = cro::FileSystem::saveFileDialogue(m_currentFilePath, "spt");
    if (!path.empty())
    {
        m_spriteSheet.saveToFile(path);
        m_currentFilePath = path;
    }
}

void SpriteState::updateBoundsEntity()
{
    if (m_activeSprite)
    {
        m_entities[EntityID::Bounds].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

        auto bounds = m_activeSprite->second.getTextureRect();
        glm::vec2 position(bounds.left, bounds.bottom);
        glm::vec2 size(bounds.width, bounds.height);

        m_entities[EntityID::Bounds].getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(position, cro::Colour::Red),
                cro::Vertex2D(glm::vec2(position.x, position.y + size.y), cro::Colour::Red),
                cro::Vertex2D(position + size, cro::Colour::Red),
                cro::Vertex2D(glm::vec2(position.x + size.x, position.y), cro::Colour::Red),
                cro::Vertex2D(position, cro::Colour::Red)
            }
        );
    }
    else
    {
        m_entities[EntityID::Bounds].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }
}

void SpriteState::createUI()
{
	registerWindow([this]() 
		{
			drawMenuBar();
            drawInspector();
            drawSpriteWindow();
            drawPreferences();
            drawNewSprite();
            drawPalette();
		});
    
    getContext().mainWindow.setTitle("Sprite Editor");
}

void SpriteState::drawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        //file menu
        if (ImGui::BeginMenu("File##sprite"))
        {
            if (ImGui::MenuItem("New##sprite"))
            {
                if (m_spriteSheet.getTexture()
                    && cro::FileSystem::showMessageBox("Save?", "Save Current Sprite Sheet?", cro::FileSystem::YesNo))
                {
                    save();
                }

                auto path = cro::FileSystem::openFileDialogue("", "png,jpg");
                if (!path.empty())
                {
                    std::replace(path.begin(), path.end(), '\\', '/');
                    if (auto pathPos = path.find(m_sharedData.workingDirectory); pathPos == std::string::npos)
                    {
                        cro::FileSystem::showMessageBox("Error", "Texture not opened from working directory");
                    }
                    else
                    {
                        m_spriteSheet = {};
                        m_spriteSheet.setTexture(path.substr(m_sharedData.workingDirectory.length() + 1), m_textures, m_sharedData.workingDirectory + "/");

                        auto* texture = m_spriteSheet.getTexture();
                        m_entities[EntityID::Root].getComponent<cro::Sprite>().setTexture(*texture);
                    }
                }
            }
            if (ImGui::MenuItem("Open##sprite", nullptr, nullptr))
            {
                auto path = cro::FileSystem::openFileDialogue("", "spt");
                if (!path.empty())
                {
                    std::replace(path.begin(), path.end(), '\\', '/');
                    if (path.find(m_sharedData.workingDirectory) == std::string::npos)
                    {
                        cro::FileSystem::showMessageBox("Error", "Sprite Sheet not opened from working directory");
                    }
                    else
                    {
                        openSprite(path);
                    }
                }
            }
            if (ImGui::MenuItem("Save##sprite", nullptr, nullptr))
            {
                save();
            }

            if (ImGui::MenuItem("Save As...##sprite", nullptr, nullptr))
            {
                saveAs();
            }

            if (getStateCount() > 1)
            {
                if (ImGui::MenuItem("Return To World Editor"))
                {
                    getContext().mainWindow.setTitle("Crogine Editor");
                    requestStackPop();
                }
            }

            if (ImGui::MenuItem("Quit##sprite", nullptr, nullptr))
            {
                if (cro::FileSystem::showMessageBox("Question", "Save Sprite Sheet?", cro::FileSystem::YesNo))
                {
                    save();
                }
                cro::App::quit();
            }
            ImGui::EndMenu();
        }

        //view menu
        if (ImGui::BeginMenu("View##sprite"))
        {
            ImGui::MenuItem("Options", nullptr, &m_showPreferences);
            ImGui::MenuItem("Palette", nullptr, &m_showPalette);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void SpriteState::drawInspector()
{
    ImGui::SetNextWindowSize({ 480.f, 700.f });
    ImGui::Begin("Inspector##sprite", nullptr, ImGuiWindowFlags_NoCollapse);

    if (m_spriteSheet.getTexture())
    {
        if (ImGui::Button("Add##sprite"))
        {
            m_activeSprite = nullptr;
            updateBoundsEntity();

            m_showNewSprite = true;
        }
    }
    if (m_activeSprite)
    {
        ImGui::SameLine();
        if (ImGui::Button("Remove##sprite"))
        {
            m_activeSprite = nullptr;
            updateBoundsEntity();
        }
    }

    const auto* texture = m_spriteSheet.getTexture();
    glm::vec2 texSize(1.f);
    if (texture)
    {
        texSize = { texture->getSize() };

        ImGui::Text("Texture Path: %s", m_spriteSheet.getTexturePath().c_str());
        ImGui::SameLine();
        if (ImGui::Button("Browse##texPath"))
        {
            auto path = cro::FileSystem::openFileDialogue("", "png,jpg");
            if (!path.empty())
            {
                std::replace(path.begin(), path.end(), '\\', '/');
                if (auto pathPos = path.find(m_sharedData.workingDirectory); pathPos == std::string::npos)
                {
                    cro::FileSystem::showMessageBox("Error", "Texture not opened from working directory");
                }
                else
                {
                    m_spriteSheet.setTexture(path.substr(m_sharedData.workingDirectory.length() + 1), m_textures, m_sharedData.workingDirectory + "/");

                    auto* texture = m_spriteSheet.getTexture();
                    m_entities[EntityID::Root].getComponent<cro::Sprite>().setTexture(*texture);
                }
            }
        }
        ImGui::Text("Texture Size: %d, %d", texture->getSize().x, texture->getSize().y);
        ImGui::NewLine();
    }

    for (auto& spritePair : m_spriteSheet.getSprites())
    {
        auto& [name, sprite] = spritePair;

        /*std::string label = "Sprite - " + name + "##" + std::to_string(i);
        if (ImGui::TreeNode(label.c_str()))
        {
            if (ImGui::IsItemClicked())
            {
                m_activeSprite = &spritePair;
                updateBoundsEntity();
            }

            ImGui::Indent(uiConst::IndentSize);

            auto bounds = sprite.getTextureRect();

            if (texture)
            {
                ImVec2 uv0(bounds.left / texSize.x, ((bounds.bottom + bounds.height) / texSize.y));
                ImVec2 uv1((bounds.left + bounds.width) / texSize.x, (bounds.bottom / texSize.y));

                float ratio = bounds.height / bounds.width;
                ImVec2 size(std::min(bounds.width, 240.f), 0.f);
                size.y = size.x * ratio;

                ImGui::ImageButton(*texture, size, uv0, uv1);
            }

            ImGui::Text("Bounds: %3.3f, %3.3f, %3.3f, %3.3f", bounds.left, bounds.bottom, bounds.width, bounds.height);
            ImGui::NewLine();
            ImGui::Indent(uiConst::IndentSize);

            auto j = 0u;
            for (const auto& anim : sprite.getAnimations())
            {
                label = "Frame Rate: " + std::to_string(anim.framerate);
                ImGui::Text("%s", label.c_str());

                if (anim.looped)
                {
                    label = "Looped: true";
                }
                else
                {
                    label = "Looped: false";
                }
                ImGui::Text("%s", label.c_str());

                label = "Loop Start: " + std::to_string(anim.loopStart);
                ImGui::Text("%s", label.c_str());

                label = "Frame Count: " + std::to_string(anim.frames.size());
                ImGui::Text("%s", label.c_str());
                ImGui::NewLine();

                j++;
            }
            ImGui::Unindent(uiConst::IndentSize);
            ImGui::Unindent(uiConst::IndentSize);
            ImGui::TreePop();
        }
        if (ImGui::IsItemClicked())
        {
            m_activeSprite = &spritePair;
            updateBoundsEntity();
        }*/
    }

    
    if (!m_spriteSheet.getSprites().empty())
    {
        ImGui::ListBoxHeader("##", std::min(20, static_cast<std::int32_t>(m_spriteSheet.getSprites().size() + 1)));
        for (auto& spritePair : m_spriteSheet.getSprites())
        {
            if (ImGui::Selectable(spritePair.first.c_str(), &spritePair == m_activeSprite))
            {
                m_activeSprite = &spritePair;
                updateBoundsEntity();
            }
        }
        ImGui::ListBoxFooter();
    }
    ImGui::End();
}

void SpriteState::drawSpriteWindow()
{
    if (m_activeSprite)
    {
        auto& [name, sprite] = *m_activeSprite;

        ImGui::SetNextWindowSize({ 480.f, 600.f });
        std::string title("Edit Sprite - ");
        title += name;

        ImGui::Begin(title.c_str());

        auto* texture = sprite.getTexture();
        glm::vec2 texSize(texture->getSize());

        if (texture)
        {
            auto bounds = sprite.getTextureRect();

            ImGui::Text("Bounds");
            auto intBounds = cro::IntRect(bounds);
            if (ImGui::InputInt("Left##bounds", &intBounds.left))
            {
                intBounds.left = std::max(0, std::min(intBounds.left, static_cast<std::int32_t>(texSize.x - (intBounds.width - 1))));
                sprite.setTextureRect(cro::FloatRect(intBounds));
                updateBoundsEntity();
            }

            if (ImGui::InputInt("Bottom##bounds", &intBounds.bottom))
            {
                intBounds.bottom = std::max(0, std::min(intBounds.bottom, static_cast<std::int32_t>(texSize.y -(intBounds.height - 1))));
                sprite.setTextureRect(cro::FloatRect(intBounds));
                updateBoundsEntity();
            }

            if (ImGui::InputInt("Width##bounds", &intBounds.width))
            {
                intBounds.width = std::max(1, std::min(intBounds.width, static_cast<std::int32_t>(texSize.x)));
                if (intBounds.left + intBounds.width > texSize.x)
                {
                    intBounds.left -= (intBounds.left + intBounds.width) - static_cast<std::int32_t>(texSize.x);
                }

                sprite.setTextureRect(cro::FloatRect(intBounds));
                updateBoundsEntity();
            }

            if (ImGui::InputInt("Height##bounds", &intBounds.height))
            {
                intBounds.height = std::max(1, std::min(intBounds.height, static_cast<std::int32_t>(texSize.y)));
                if (intBounds.bottom + intBounds.height > texSize.y)
                {
                    intBounds.bottom -= (intBounds.bottom + intBounds.height) - static_cast<std::int32_t>(texSize.y);
                }

                sprite.setTextureRect(cro::FloatRect(intBounds));
                updateBoundsEntity();
            }

            if (ImGui::Button("Duplicate"))
            {
                ImGui::OpenPopup("Duplicate Sprite");
                newSpriteName = name + std::to_string(nameNumber);
            }

            if (ImGui::BeginPopupModal("Duplicate Sprite"))
            {
                ImGui::InputText("Name", &newSpriteName);

                if (ImGui::Button("OK"))
                {
                    if (m_spriteSheet.addSprite(newSpriteName))
                    {
                        m_spriteSheet.getSprites().at(newSpriteName) = sprite;
                        nameNumber++; //only increment this if accepted...

                        cro::FileSystem::showMessageBox("Success", "Added Sprite " + newSpriteName);
                    }
                    else
                    {
                        cro::FileSystem::showMessageBox("Duplicate Failed", newSpriteName + " already exists?");
                    }

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }


            const auto& anims = sprite.getAnimations();
            if (!anims.empty())
            {
                //TODO make this a member so we can properly reset it
                static const cro::Sprite::Animation* currentAnim = &anims[0];

                ImGui::Text("Animations");

                auto i = 0;
                ImGui::ListBoxHeader("##buns", std::min(20, static_cast<std::int32_t>(anims.size() + 1)));
                for (const auto& anim : anims)
                {
                    std::string label = "Add Name Here##" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), &anim == currentAnim))
                    {
                        currentAnim = &anim;
                    }

                    i++;
                }
                ImGui::ListBoxFooter();

                if (currentAnim)
                {
                    ImGui::Text("Frame Rate: %3.2f", currentAnim->framerate);

                    std::string label;
                    if (currentAnim->looped)
                    {
                        label = "Looped: true";
                    }
                    else
                    {
                        label = "Looped: false";
                    }
                    ImGui::Text("%s", label.c_str());
                    ImGui::Text("Loop Start: %d", currentAnim->loopStart);
                    ImGui::Text("Frame Count:  %d", currentAnim->frames.size());
                    ImGui::NewLine();
                }
            }

            //preview
            ImVec4 c(sprite.getColour().getVec4());
            if (ImGui::ColorButton("Colour", c))
            {
                m_showPalette = true;
            }

            ImVec2 uv0(bounds.left / texSize.x, ((bounds.bottom + bounds.height) / texSize.y));
            ImVec2 uv1((bounds.left + bounds.width) / texSize.x, (bounds.bottom / texSize.y));

            float ratio = bounds.height / bounds.width;
            ImVec2 size(std::min(bounds.width, 440.f), 0.f);
            size.y = size.x * ratio;

            ImGui::BeginChild("##tex", ImVec2(460.f, 200.f));
            ImGui::ImageButton(*texture, size, uv0, uv1);
            ImGui::EndChild();


        }

        ImGui::End();
    }
}

void SpriteState::drawPreferences()
{
    if (m_showPreferences)
    {
        ImGui::SetNextWindowSize({ 400.f, 260.f });
        if (ImGui::Begin("Preferences##sprite", &m_showPreferences))
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
                uiConst::showTipMessage(m_sharedData.workingDirectory.c_str());
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

            if (!m_showPreferences ||
                ImGui::Button("Close"))
            {
                //notify so the global prefs are also written
                auto* msg = getContext().appInstance.getMessageBus().post<UIEvent>(MessageID::UIMessage);
                msg->type = UIEvent::WrotePreferences;

                m_showPreferences = false;
            }
        }
        ImGui::End();
    }
}

void SpriteState::drawNewSprite()
{
    if (m_showNewSprite)
    {
        ImGui::SetNextWindowSize({ 240.f, 60.f });
        if (ImGui::Begin("Add Sprite", &m_showNewSprite, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
        {
            static std::string nameBuffer;
            ImGui::InputText("Name", &nameBuffer);
            ImGui::SameLine();
            if (ImGui::Button("Add"))
            {
                m_spriteSheet.addSprite(nameBuffer);
                nameBuffer.clear();
            }
        }
        ImGui::End();
    }
}

void SpriteState::drawPalette()
{
    if (m_showPalette)
    {
        ImGui::SetNextWindowSize({ 240.f, 220.f });
        if (ImGui::Begin("Palette"))
        {
            if (ImGui::Button("Open"))
            {
                auto path = cro::FileSystem::openFileDialogue("", "ase");
                if (!path.empty())
                {
                    m_palette.loadFromFile(path);
                }
            }

            const auto& swatches = m_palette.getSwatches();
            auto i = 0;
            for (const auto& swatch : swatches)
            {
                for (const auto& colour : swatch.colours)
                {
                    ImVec4 c(colour.colour.getVec4());
                    std::string label = colour.name.toAnsiString() + "##" + std::to_string(i);

                    if (ImGui::ColorButton(label.c_str(), c))
                    {
                        if (m_activeSprite)
                        {
                            m_activeSprite->second.setColour(colour.colour);
                        }
                    }

                    if ((i++ % 5) != 4)
                    {
                        ImGui::SameLine();
                    }
                }
            }
            ImGui::NewLine();

            if (m_activeSprite)
            {
                glm::vec4 outColour = m_activeSprite->second.getColour().getVec4();
                if (ImGui::ColorPicker4("Sprite Colour", &outColour[0]))
                {
                    m_activeSprite->second.setColour(cro::Colour(outColour));
                }
            }
        }
        ImGui::End();
    }
}