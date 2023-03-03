/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "GolfState.hpp"
#include "CameraFollowSystem.hpp"

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/gui/Gui.hpp>

namespace
{
    const std::array<std::string, CameraID::Count> CameraStrings =
    {
        "Player", "Bystander", "Sky", "Green", "Transition", "Idle", "Drone"
    };
}

#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

#include "../ErrorCheck.hpp"

#ifdef PATH_TRACING
namespace
{
    cro::Mesh::Data* meshData = nullptr;
}

void GolfState::initBallDebug()
{
    auto materialID = m_materialIDs[MaterialID::WireFrame];
    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    
    auto material = m_resources.materials.get(materialID);
    material.enableDepthTest = false;
    meshData = &m_resources.meshes.getMesh(meshID);
    meshData->boundingBox = { glm::vec3(0.f), glm::vec3(320.f, 100.f, -200.f) };
    meshData->boundingSphere = meshData->boundingBox;

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(*meshData, material);

    //this has been copied into the Model, so we need to point
    //to the correct copy else we update the wrong indexData
    meshData = &entity.getComponent<cro::Model>().getMeshData();
}

void GolfState::beginBallDebug()
{
    //set VBO index count to 0
    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = 0;

    m_ballDebugActive = true;
}

void GolfState::updateBallDebug(glm::vec3 position)
{
    static const std::array Colours =
    {
        cro::Colour::Red, cro::Colour::Blue
    };
    static std::size_t idx = 0;

    if (m_ballDebugActive)
    {
        m_ballDebugIndices.push_back(static_cast<std::uint32_t>(m_ballDebugPoints.size()));
        m_ballDebugPoints.emplace_back(position, Colours[idx].getVec4());
        idx = (idx + 1) % 2;
    }
}

void GolfState::endBallDebug()
{
    m_ballDebugActive = false;

    //update VBO
    meshData->vertexCount = m_ballDebugPoints.size();
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, m_ballDebugPoints.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = m_ballDebugIndices.size();
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), m_ballDebugIndices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    m_ballDebugPoints.clear();
    m_ballDebugIndices.clear();
}

#endif

void GolfState::addCameraDebugging()
{
#ifdef CAMERA_TRACK
    auto materialID = m_materialIDs[MaterialID::WireFrame];

    for (auto c : m_cameras)
    {
        if (c.hasComponent<CameraFollower>())
        {
            auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINES));

            auto material = m_resources.materials.get(materialID);
            material.enableDepthTest = false;
            auto meshData = m_resources.meshes.getMesh(meshID);
            meshData.boundingBox = { glm::vec3(0.f), glm::vec3(320.f, 100.f, -200.f) };
            meshData.boundingSphere = meshData.boundingBox;

            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::Model>(meshData, material);

            c.getComponent<CameraFollower>().debugEntity = entity;
        }
    }
#endif
}

void GolfState::registerDebugWindows()
{
    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Spin"))
    //        {
    //            auto spin = m_inputParser.getSpin();
    //            ImGui::SliderFloat2("Spin", &spin[0], -1.f, 1.f);
    //            ImGui::End();
    //        }
    //    });

    registerWindow([&]()
        {
            if (ImGui::Begin("Target Info"))
            {
                auto pos = m_freeCam.getComponent<cro::Transform>().getPosition();
                auto dir = m_freeCam.getComponent<cro::Transform>().getForwardVector() * 100.f;
                auto result = m_collisionMesh.getTerrain(pos, dir);

                if (result.wasRayHit)
                {
                    ImGui::Text("Terrain: %s", TerrainStrings[result.terrain].c_str());
                }
                else
                {
                    ImGui::Text("Inf.");
                }

                ImGui::Text("Current Camera %s", CameraStrings[m_currentCamera].c_str());
            }        
            ImGui::End();

            //hacky stand in for reticule :3
            if (m_gameScene.getActiveCamera() == m_freeCam)
            {
                auto size = glm::vec2(cro::App::getWindow().getSize());
                const glm::vec2 pointSize(6.f);

                auto pos = (size - pointSize) / 2.f;
                ImGui::SetNextWindowPos({ pos.x, pos.y });
                ImGui::SetNextWindowSize({ pointSize.x, pointSize.y });
                ImGui::Begin("Point");
                ImGui::End();
            }
        }, true);

    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Sun"))
    //        {
    //            if (ImGui::SliderFloat("ToD", &m_skyData.tod, 0.f, 1.f))
    //            {
    //                float angle = SkyData::MinAngle + (m_skyData.tod * (SkyData::MaxAngle - SkyData::MinAngle));
    //                m_gameScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -angle * cro::Util::Const::degToRad);
    //                //m_skyData.sunRoot.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -angle * cro::Util::Const::degToRad);

    //                /*if (auto w = m_skyData.sunPalette.getSize().x; w != 0)
    //                {
    //                    auto index = (w - 1) * m_skyData.tod;
    //                    auto* colour = m_skyData.sunPalette.getPixel(static_cast<std::uint32_t>(index), 0);
    //                    m_skyData.sunModel.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour(colour[0], colour[1], colour[2]));
    //                }

    //                if (auto w = m_skyData.lightPalette.getSize().x; w != 0)
    //                {
    //                    auto index = (w - 1) * m_skyData.tod;
    //                    auto* colour = m_skyData.sunPalette.getPixel(static_cast<std::uint32_t>(index), 0);
    //                    
    //                    glm::vec4 lightColour(static_cast<float>(colour[0]) / 255.f, static_cast<float>(colour[1]) / 255.f, static_cast<float>(colour[3]) / 255.f, 1.f);
    //                    auto colours = m_skyData.skyColours;
    //                    colours.top *= lightColour;
    //                    colours.middle *= lightColour;
    //                    m_skyScene.setSkyboxColours(colours.bottom, colours.middle, colours.top);

    //                    cro::Colour sLight(colour[0], colour[1], colour[2]);
    //                    m_skyScene.getSunlight().getComponent<cro::Sunlight>().setColour(sLight);
    //                    m_gameScene.getSunlight().getComponent<cro::Sunlight>().setColour(sLight);
    //                }*/
    //            }
    //        }
    //        ImGui::End();
    //    });

    registerWindow([&]()
        {
            if (ImGui::Begin("Depthmap"))
            {
                /*for (auto y = 4; y >= 0; --y)
                {
                    for (auto x = 0; x < 8; ++x)
                    {
                        auto idx = y * 8 + x;
                        ImGui::Image(m_depthMap.getTextureAt(idx), { 80.f, 80.f }, { 0.f, 1.f }, { 1.f, 0.f });
                        ImGui::SameLine();
                    }
                    ImGui::NewLine();
                }*/

                const auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
                for (auto i = 0u; i < cam.shadowMapBuffer.getLayerCount(); ++i)
                {
                    ImGui::Image(cam.shadowMapBuffer.getTexture(i), { 256.f, 256.f }, { 0.f, 1.f }, { 1.f, 0.f });
                    ImGui::SameLine();
                }
            }
            ImGui::End();
        },true);

    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Network"))
    //        {
    //            auto size = m_greenBuffer.getSize();
    //            ImGui::Text("Buffer Size %u, %u", size.x, size.y);

    //            ImGui::Text("Connection Bitrate: %3.3fkbps", static_cast<float>(bitrate) / 1024.f);

    //            auto terrain = m_collisionMesh.getTerrain(m_freeCam.getComponent<cro::Transform>().getPosition());
    //            ImGui::Text("Terrain %s", TerrainStrings[terrain.terrain].c_str());
    //        }
    //        ImGui::End();
    //    }, true);
}

void GolfState::dumpBenchmark()
{
    std::string outFile = cro::App::getPreferencePath() + "benchmark/";

    if (!cro::FileSystem::directoryExists(outFile))
    {
        cro::FileSystem::createDirectory(outFile);
    }
    outFile += m_sharedData.mapDirectory + "/";

    if (!cro::FileSystem::directoryExists(outFile))
    {
        cro::FileSystem::createDirectory(outFile);
    }
    outFile += std::to_string(m_currentHole) + ".bmk";

    const std::array<std::string, 3u> TreeTypes
    {
        "Classic", "Low", "High"
    };

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(outFile.c_str(), "a");
    if (file.file)
    {
        std::string dateTime = cro::SysTime::dateString();
        dateTime += " - " + cro::SysTime::timeString() 
#ifdef CRO_DEBUG_
            + " (DEBUG BUILD)"
#endif
            + ":\n";

        std::string stat = "Min fps: " + std::to_string(m_benchmark.minRate) 
            + " | Max fps: " + std::to_string (m_benchmark.maxRate) 
            + " | Average fps: " + std::to_string(m_benchmark.getAverage());

        std::string shadowQ = m_sharedData.hqShadows ? "High" : "Low";
        std::string vsync = cro::App::getWindow().getVsyncEnabled() ? " | vsync: ON" : " | vsync: OFF";
        std::string vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        std::string renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        std::string settings = "\nTree Quality: " + TreeTypes[m_sharedData.treeQuality] + " | Shadow Quality: " + shadowQ
            + vsync
            + "\nVendor: " + vendor + " | Renderer: " + renderer + "\n\n";

        SDL_RWwrite(file.file, dateTime.c_str(), dateTime.length(), 1);
        SDL_RWwrite(file.file, stat.c_str(), stat.length(), 1);
        SDL_RWwrite(file.file, settings.c_str(), settings.length(), 1);
    }
    else
    {
        LOG("Failed opening benchmark file " + outFile, cro::Logger::Type::Warning);
        LogE << SDL_GetError() << std::endl;
    }

    m_benchmark.reset();
}