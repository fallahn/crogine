/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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
#include "PacketIDs.hpp"
#include "CameraFollowSystem.hpp"
#include "AchievementIDs.hpp"
#include "AchievementStrings.hpp"
#include "WeatherAnimationSystem.hpp"
#include "ChunkVisSystem.hpp"

#include <crogine/audio/AudioMixer.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/systems/LightVolumeSystem.hpp>
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
        m_cameraDebugPoints.emplace_back();
    }

    registerWindow([&]()
        {
            if (ImGui::Begin("Camera Points"))
            {
                
                static auto camID = 0;
                static auto stepIndex = 0;
                ImGui::Text("%s", CameraStrings[camID].c_str());
                if (ImGui::InputInt("Camera ID", &camID))
                {
                    camID = (camID + CameraID::Count) % CameraID::Count;
                    stepIndex = 0;
                }
                ImGui::Text("%d of %u", stepIndex, m_cameraDebugPoints[camID].size());

                static glm::quat prevQuat = glm::quat(1.f, 0.f, 0.f, 0.f);
                const auto updateTx = [&]()
                {
                    auto [q, p, _] = m_cameraDebugPoints[camID][stepIndex];
                    m_cameras[CameraID::Player].getComponent<cro::Transform>().setRotation(q);
                    m_cameras[CameraID::Player].getComponent<cro::Transform>().setPosition(p);
                    m_cameras[CameraID::Player].getComponent<cro::Camera>().active = true;
                };


                if (ImGui::Button("Step Back"))
                {
                    if (!m_cameraDebugPoints[camID].empty())
                    {
                        prevQuat = m_cameraDebugPoints[camID][stepIndex].q;
                        stepIndex = (stepIndex + (m_cameraDebugPoints[camID].size() - 1)) % m_cameraDebugPoints[camID].size();
                        updateTx();

                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Step Forward"))
                {
                    if (!m_cameraDebugPoints[camID].empty())
                    {
                        prevQuat = m_cameraDebugPoints[camID][stepIndex].q;
                        stepIndex = (stepIndex + 1) % m_cameraDebugPoints[camID].size();
                        updateTx();
                    }
                }

                if (!m_cameraDebugPoints[camID].empty())
                {
                    
                    auto [q, p, b] = m_cameraDebugPoints[camID][stepIndex];
                    if (q == prevQuat)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
                    }
                    ImGui::Text("Q: %3.5f, %3.5f, %3.5f, %3.5f", q.x, q.y, q.z, q.w);
                    ImGui::PopStyleColor();
                    ImGui::Text("P: %3.3f, %3.3f, %3.3f", p.x, p.y, p.z);
                    if (b)
                    {
                        ImGui::Text("Was Updated");
                    }
                    else
                    {
                        ImGui::Text("Was NOT Updated");
                    }
                }
            }
            ImGui::End();
        });
#endif
}

void GolfState::registerDebugCommands()
{
    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("asefsd"))
    //        {
    //            /*if (m_drone.isValid())
    //            {
    //                auto pos = m_drone.getComponent<cro::Transform>().getPosition();
    //                float height = pos.y - m_collisionMesh.getTerrain(pos).height;
    //                ImGui::Text("height %3.3f", height);
    //            }*/
    //            /*ImGui::Text("Shader ID %d", m_targetShader.shaderID);
    //            ImGui::Text("Shader Uniform %d", m_targetShader.vpUniform);
    //            ImGui::Text("Position %3.2f, %3.2f, %3.2f", m_targetShader.position.x, m_targetShader.position.y, m_targetShader.position.z);
    //            ImGui::Text("Size %3.3f", m_targetShader.size);*/
    //            
    //            const auto* system = m_gameScene.getSystem<ChunkVisSystem>();
    //            ImGui::Text("Visible Chunks %d", system->getIndexList().size());
    //        }
    //        ImGui::End();        
    //    });

    registerCommand("refresh_turn", [&](const std::string&)
        {
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint16_t(ServerCommand::SkipTurn), net::NetFlag::Reliable);
        });

    registerCommand("build_cubemaps",
        [&](const std::string&)
        {
            std::string holeNumber = std::to_string(m_currentHole + 1);
            if (m_currentHole < 9)
            {
                holeNumber = "0" + holeNumber;
            }

            std::string tod = "/d/";
            //TODO if night tod = "/n/"

            auto courseDir = "assets/golf/courses/" + m_sharedData.mapDirectory + "/cmap/" + holeNumber + tod;
            if (!cro::FileSystem::directoryExists(courseDir))
            {
                cro::FileSystem::createDirectory(courseDir);
            }

            cro::Entity cam = m_gameScene.createEntity();
            cam.addComponent<cro::Transform>();
            cam.addComponent<cro::Camera>().setPerspective(90.f * cro::Util::Const::degToRad, 1.f, 0.1f, 280.f);
            cam.getComponent<cro::Camera>().viewport = { 0.f, 0.f, 1.f, 1.f };
            cam.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlags::CubeMap);
            m_gameScene.simulate(0.f); //do this once to integrate the new entity;

            auto oldCam = m_gameScene.setActiveCamera(cam);
            m_skyScene.setActiveCamera(m_skyCameras[SkyCam::Flight]);

            static const std::uint32_t TexSize = 256;
            cro::RenderTexture rt;
            rt.create(TexSize, TexSize);


            //create 3 cubemaps based on pin/tee position etc
            const std::array<glm::vec3, 3u> Positions =
            {
                m_holeData[m_currentHole].tee,
                m_holeData[m_currentHole].target,
                m_holeData[m_currentHole].pin,
            };

            for (auto i = 0; i < 3; ++i)
            {
                auto path = courseDir + std::to_string(i);
                if (!cro::FileSystem::directoryExists(path))
                {
                    cro::FileSystem::createDirectory(path);
                }

                cro::ConfigFile cfg("cubemap");
                cfg.addProperty("up").setValue(path + "/py.png");
                cfg.addProperty("down").setValue(path + "/ny.png");
                cfg.addProperty("left").setValue(path + "/nx.png");
                cfg.addProperty("right").setValue(path + "/px.png");
                cfg.addProperty("front").setValue(path + "/pz.png");
                cfg.addProperty("back").setValue(path + "/nz.png");
                cfg.save(path + "/cmap.ccm");


                struct Rotation final
                {
                    glm::vec3 axis = glm::vec3(0.f);
                    float angle = 0.f;
                    constexpr Rotation(glm::vec3 a, float r) : axis(a), angle(r) {};
                };
                static constexpr std::array<Rotation, 6u> Rotations =
                {
                    Rotation(cro::Transform::Y_AXIS, 0.f),
                    Rotation(cro::Transform::Y_AXIS, cro::Util::Const::PI / 2.f),
                    Rotation(cro::Transform::Y_AXIS, cro::Util::Const::PI),
                    Rotation(cro::Transform::Y_AXIS, -cro::Util::Const::PI / 2.f),
                    Rotation(cro::Transform::X_AXIS, cro::Util::Const::PI / 2.f),
                    Rotation(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f)
                };

                static const std::array<std::string, 6u> FileNames =
                {
                    "/pz.png", "/nx.png", "/nz.png", "/px.png", "/py.png", "/ny.png"
                };

                auto position = Positions[i];
                position.y += 0.5f;

                cam.getComponent<cro::Transform>().setPosition(position);
                for (auto j = 0; j < 6; ++j)
                {
                    cam.getComponent<cro::Transform>().setRotation(Rotations[j].axis, Rotations[j].angle);
                    m_gameScene.simulate(0.f);

                    //we'll use the existing cam as it happens to have the same FOV
                    m_skyCameras[SkyCam::Flight].getComponent<cro::Transform>().setRotation(cam.getComponent<cro::Transform>().getWorldRotation());
                    m_skyCameras[SkyCam::Flight].getComponent<cro::Transform>().setPosition({0.f, position.y / 64.f, 0.f});
                    m_skyScene.simulate(0.f);

                    rt.clear();
                    m_skyScene.render();
                    glClear(GL_DEPTH_BUFFER_BIT);
                    m_gameScene.render();
                    rt.display();

                    rt.saveToFile(path + FileNames[j]);
                }
            }

            m_gameScene.setActiveCamera(oldCam);
            m_gameScene.destroyEntity(cam);

            cro::Console::print("Done!");
        });

    registerCommand("noclip", [&](const std::string&)
        {
            toggleFreeCam();
            if (m_photoMode)
            {
                cro::Console::print("noclip ON");
            }
            else
            {
                cro::Console::print("noclip OFF");
            }
        });


    registerCommand("cl_shownet", [&](const std::string& param)
        {
            if (param == "0" || param == "false")
            {
                m_networkDebugContext.showUI = false;
            }
            else if (param == "1" || param == "true")
            {
                m_networkDebugContext.showUI = true;

                if (!m_networkDebugContext.wasShown)
                {
                    registerWindow([&]()
                    {
                        if (m_networkDebugContext.showUI)
                        {
                            ImGui::SetNextWindowSize({ 300.f, 90.f });
                            if (ImGui::Begin("Network", &m_networkDebugContext.showUI))
                            {
                                float bps = static_cast<float>(m_networkDebugContext.bitrate) / 1024.f;
                                float BPS = bps / 8.f;
                                ImGui::Text("Connection Bitrate: %3.2fkbps (%3.2f KB/s)", bps, BPS);

                                auto KB = static_cast<double>(m_networkDebugContext.total) / 1024.;
                                if (KB > 1024.)
                                {
                                    ImGui::Text("Data Transferred: %3.2fMB this session", KB / 1024.);
                                }
                                else
                                {
                                    ImGui::Text("Data Transferred: %3.2fKB this session", KB);
                                }

                                /*ImGui::NewLine();
                                ImGui::Text("Most frequent packet: %d", m_networkDebugContext.lastHighestID);*/
                            }
                            ImGui::End();
                        }
                    });
                    m_networkDebugContext.wasShown = true;
                }
            }
            else
            {
                cro::Console::print("Usage: cl_shownet <0|1>");
            }
        });


    //nasssssty staticses
    static bool showKickWindow = false;
    if (m_sharedData.hosting)
    {
        registerWindow([&]()
            {
                if (showKickWindow)
                {
                    if (ImGui::Begin("Kick Player", &showKickWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        struct ListItem final
                        {
                            const cro::String* name = nullptr;
                            std::uint8_t client = 0;
                        };
                        static std::vector<ListItem> items;
                        items.clear();

                        for (auto i = 0u; i < ConstVal::MaxClients; ++i)
                        {
                            const auto& client = m_sharedData.connectionData[i];
                            for (auto j = 0u; j < client.playerCount; ++j)
                            {
                                auto& item = items.emplace_back();
                                item.name = &client.playerData[j].name;
                                item.client = i;
                            }
                        }

                        static std::int32_t idx = 0;
                        if (ImGui::BeginListBox("Players", ImVec2(-FLT_MIN, 6.f * ImGui::GetTextLineHeightWithSpacing())))
                        {
                            for (auto n = 0u; n < items.size(); ++n)
                            {
                                const bool selected = (idx == n);
                                if (ImGui::Selectable(reinterpret_cast<char*>(items[n].name->toUtf8().data()), selected))
                                {
                                    idx = n;
                                }

                                if (selected)
                                {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndListBox();
                        }
                        if (ImGui::Button("Kick"))
                        {
                            //again, assuming host is client 0...
                            if (items[idx].client != 0)
                            {
                                std::uint16_t data = std::uint16_t(ServerCommand::KickClient) | ((items[idx].client) << 8);
                                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                                showKickWindow = false;
                            }
                        }
                    }
                    ImGui::End();
                }
            });

        registerCommand("kick", [](const std::string&) {showKickWindow = true; });
    }
#ifdef CRO_DEBUG_

    registerCommand("rain", [&](const std::string&)
        {
            static bool raining = false;
            static constexpr float Density = 0.5f;
            if (!raining)
            {
                createWeather(WeatherType::Rain);

                setFog(Density);

                raining = true;
                m_gameScene.setSystemActive<WeatherAnimationSystem>(true);
                m_gameScene.getSystem<WeatherAnimationSystem>()->setHidden(false);
            }
            else
            {
                static bool hidden = false;
                hidden = !hidden;
                m_gameScene.getSystem<WeatherAnimationSystem>()->setHidden(hidden);

                setFog(hidden ? 0.f : Density);
            }
        });

    registerCommand("fog", [&](const std::string& amount)
        {
            if (amount.empty())
            {
                cro::Console::print("Usage: fog <0 - 1> where value represents density. EG fog 0.5");
            }
            else
            {
                float density = 0.f;
                std::stringstream ss;
                ss << amount;
                ss >> density;
                density = std::clamp(density, 0.f, 1.f);

                setFog(density);
            }
        });




    registerCommand("fast_cpu", [&](const std::string& param)
        {
            if (m_sharedData.hosting)
            {
                const auto sendCmd = [&]()
                    {
                        m_sharedData.clientConnection.netClient.sendPacket<std::uint8_t>(PacketID::FastCPU, m_sharedData.fastCPU ? 1 : 0, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                        m_cpuGolfer.setFastCPU(m_sharedData.fastCPU);

                        //TODO set active or not if current player is CPU

                    };

                if (param == "0")
                {
                    m_sharedData.fastCPU = false;
                    sendCmd();
                }
                else if (param == "1")
                {
                    m_sharedData.fastCPU = true;
                    sendCmd();
                }
                else
                {
                    cro::Console::print("Usage: fast_cpu <0|1>");
                }
            }
        });
#endif

    /*registerWindow([&]()
        {
            const auto printCam = [](const std::string& s, bool active)
                {
                    if (active)
                    {
                        ImGui::ColorButton("##", ImVec4(0.f, 1.f, 0.f, 1.f));
                    }
                    else
                    {
                        ImGui::ColorButton("##", ImVec4(1.f, 0.f, 0.f, 1.f));
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", s.c_str());
                };

            if (ImGui::Begin("Cams"))
            {
                for (auto i = 0u; i < CameraID::Count; ++i)
                {
                    printCam(CameraStrings[i], m_cameras[i].getComponent<cro::Camera>().active);
                }
                printCam("Flight Cam", m_flightCam.getComponent<cro::Camera>().active);
                printCam("Green Cam", m_greenCam.getComponent<cro::Camera>().active);
                printCam("Map Cam", m_mapCam.getComponent<cro::Camera>().active);
            }
            ImGui::End();
        
        });*/

    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("sunlight"))
    //        {
    //            /*static float col[3] = { 1.f, 1.f, 1.f };
    //            if (ImGui::ColorPicker3("Sky", col))
    //            {
    //                m_skyScene.getSunlight().getComponent<cro::Sunlight>().setColour({ col[0], col[1], col[2], 1.f });
    //                m_gameScene.getSunlight().getComponent<cro::Sunlight>().setColour({ col[0], col[1], col[2], 1.f });
    //            }*/

    //            //this only works on night mode obvs
    //            /*auto size = glm::vec2(m_gameSceneMRTexture.getSize() / 4u);
    //            ImGui::SameLine();
    //            ImGui::Image(m_gameSceneMRTexture.getTexture(2), { size.x , size.y }, { 0.f ,1.f }, { 1.f, 0.f });*/

    //            auto size = glm::vec2(m_lightMaps[LightMapID::Scene].getSize()) / 2.f;
    //            ImGui::Image(m_lightMaps[LightMapID::Scene].getTexture(), {size.x , size.y}, {0.f ,1.f}, {1.f, 0.f});
    //            ImGui::Image(m_gameSceneMRTexture.getTexture(MRTIndex::Normal), { size.x , size.y }, { 0.f ,1.f }, { 1.f, 0.f });
    //            /*ImGui::SameLine();
    //            size = glm::vec2(m_lightBlurTextures[LightMapID::Overhead].getSize()) * 2.f;
    //            ImGui::Image(m_lightBlurTextures[LightMapID::Overhead].getTexture(), { size.x , size.y }, { 0.f ,1.f }, { 1.f, 0.f });*/



    //            //const auto& buff = m_lightMaps[LightMapID::Overhead].getTexture();
    //            //auto size = glm::vec2(buff.getSize()/* / 2u*/);
    //            //ImGui::Image(buff, { size.x , size.y }, { 0.f ,1.f }, { 1.f, 0.f });
    //            //ImGui::SameLine();
    //            //ImGui::Image(m_overheadBuffer.getDepthTexture(), {size.x , size.y}, {0.f ,1.f}, {1.f, 0.f});
    //            //ImGui::SameLine();
    //            //ImGui::Image(m_overheadBuffer.getTexture(MRTIndex::Normal), { size.x , size.y }, { 0.f ,1.f }, { 1.f, 0.f });

    //            //size = glm::vec2(m_gameSceneMRTexture.getSize() / 4u);
    //            //ImGui::Image(m_gameSceneMRTexture.getDepthTexture(), {size.x , size.y}, {0.f ,1.f}, {1.f, 0.f});
    //        }
    //        ImGui::End();
    //    });



    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("Weather"))
    //        {
    //            /*auto c = m_skyScene.getSunlight().getComponent<cro::Sunlight>().getColour();
    //            if (ImGui::ColorPicker3("Sun", c.asArray()))
    //            {
    //                m_skyScene.getSunlight().getComponent<cro::Sunlight>().setColour(c);
    //                m_gameScene.getSunlight().getComponent<cro::Sunlight>().setColour(c);
    //            }*/
    //            if (ImGui::Button("Rain"))
    //            {
    //                cro::Console::doCommand("rain");
    //            }
    //        }
    //        ImGui::End();
    //    
    //    });

    //registerCommand("show_stat_window", 
    //    [&](const std::string&)
    //    {
    //        if (!m_achievementDebug.wasActivated)
    //        {
    //            if (m_allowAchievements)
    //            {
    //                m_achievementDebug.achievementEnableReason = "Single human player found on client";
    //            }
    //            else
    //            {
    //                m_achievementDebug.achievementEnableReason = "Multiple human players found on client";
    //            }

    //            //create the window first time
    //            registerWindow([&]() 
    //                {
    //                    if (m_achievementDebug.visible)
    //                    {
    //                        if (ImGui::Begin("Stats & Achievements", &m_achievementDebug.visible))
    //                        {
    //                            ImGui::Text("Achievements active: ");
    //                            ImGui::SameLine();
    //                            if (Achievements::getActive())
    //                            {
    //                                ImGui::PushStyleColor(ImGuiCol_Text, 0xff00ff00);
    //                                ImGui::Text("True");
    //                                ImGui::PopStyleColor();
    //                            }
    //                            else
    //                            {
    //                                ImGui::PushStyleColor(ImGuiCol_Text, 0xffff00ff);
    //                                ImGui::Text("False");
    //                                ImGui::PopStyleColor();
    //                            }
    //                            ImGui::Text("Reason: %s", m_achievementDebug.achievementEnableReason.c_str());

    //                            ImGui::NewLine();
    //                            ImGui::Text("Achievment Status:");

    //                            for (auto i = static_cast<std::int32_t>(AchievementID::Complete01); i <= AchievementID::Complete10; ++i)
    //                            {
    //                                ImGui::Text("%s", AchievementLabels[i].c_str());
    //                                ImGui::SameLine();
    //                                if (Achievements::getAchievement(AchievementStrings[i])->achieved)
    //                                {
    //                                    ImGui::TextColored({ 0.f, 1.f, 0.f, 1.f }, "Achieved");
    //                                }
    //                                else
    //                                {
    //                                    ImGui::TextColored({ 1.f, 0.f, 0.f, 1.f }, "Locked");
    //                                }
    //                            }

    //                            ImGui::NewLine();
    //                            ImGui::Text("Stat Count:");
    //                            for (auto i = static_cast<std::int32_t>(StatID::Course01Complete); i <= StatID::Course10Complete; ++i)
    //                            {
    //                                ImGui::Text("%s: %2.1f", StatLabels[i].c_str(), Achievements::getStat(StatStrings[i])->value);
    //                            }
    //                            ImGui::NewLine();

    //                            if (m_achievementDebug.awardStatus.empty())
    //                            {
    //                                ImGui::Text("Course not yet completed");
    //                            }
    //                            else
    //                            {
    //                                ImGui::Text("%s", m_achievementDebug.awardStatus.c_str());
    //                            }
    //                        }
    //                        ImGui::End();
    //                    }
    //                });

    //            m_achievementDebug.wasActivated = true;
    //        }
    //        m_achievementDebug.visible = !m_achievementDebug.visible;
    //    });
}

void GolfState::registerDebugWindows()
{
    registerWindow([&]()
        {
            //if (ImGui::Begin("Ball Cam"))
            //{
            //    glm::vec2 size(m_flightTexture.getSize());
            //    ImGui::Image(m_flightTexture.getTexture(), { size.x, size.y }, { 0.f, 1.f }, { 1.f, 0.f });

            //    auto& cam = m_flightCam.getComponent<cro::Camera>();
            //    static float fov = 60.f;
            //    if (ImGui::SliderFloat("FOV", &fov, 40.f, 90.f))
            //    {
            //        cam.setPerspective(fov * cro::Util::Const::degToRad, 1.f, 0.001f, static_cast<float>(MapSize.x) * 1.25f/*, m_shadowQuality.cascadeCount*/);
            //    }

            //    static glm::vec3 pos(0.f);
            //    if (ImGui::SliderFloat("Y", &pos.y, 0.f, 0.1f))
            //    {
            //        m_flightCam.getComponent<cro::Transform>().setPosition(m_currentPlayer.position + pos);
            //    }
            //    if (ImGui::SliderFloat("Z", &pos.z, 0.f, 0.1f))
            //    {
            //        m_flightCam.getComponent<cro::Transform>().setPosition(m_currentPlayer.position + pos);
            //    }

            //    static float rotation = 0.f;
            //    if (ImGui::SliderFloat("Rotation", &rotation, -0.2f, 0.2f))
            //    {
            //        m_flightCam.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, rotation);
            //    }
            //}
            //ImGui::End();

            //if (ImGui::Begin("Ach Track"))
            //{
            //    ImGui::Text("No holes over par %s", m_achievementTracker.noHolesOverPar ? "true" : "false");
            //    ImGui::Text("No gimme used %s", m_achievementTracker.noGimmeUsed ? "true" : "false");
            //    ImGui::Text("Two shots spare %s", m_achievementTracker.twoShotsSpare ? "true" : "false");
            //    ImGui::Text("Consistency %s", m_achievementTracker.alwaysOnTheCourse ? "true" : "false");
            //    ImGui::Text("Under two putts %s", m_achievementTracker.underTwoPutts ? "true" : "false");
            //    ImGui::Text("Putt count %d", m_achievementTracker.puttCount);
            //}
            //ImGui::End();

            /*if (ImGui::Begin("Depth Map"))
            {
                glm::vec2 size(m_gameSceneTexture.getSize() / 2u);
                ImGui::Image(m_gameSceneTexture.getDepthTexture(), { size.x, size.y }, { 0.f, 1.f }, { 1.f, 0.f });
            }
            ImGui::End();*/
        });

    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Spin"))
    //        {
    //            auto spin = m_inputParser.getSpin();
    //            ImGui::SliderFloat2("Spin", &spin[0], -1.f, 1.f);
    //            ImGui::End();
    //        }
    //    });

    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Target Info"))
    //        {
    //            auto pos = m_freeCam.getComponent<cro::Transform>().getPosition();
    //            auto dir = m_freeCam.getComponent<cro::Transform>().getForwardVector() * 100.f;
    //            auto result = m_collisionMesh.getTerrain(pos, dir);

    //            if (result.wasRayHit)
    //            {
    //                ImGui::Text("Terrain: %s", TerrainStrings[result.terrain].c_str());
    //            }
    //            else
    //            {
    //                ImGui::Text("Inf.");
    //            }

    //            ImGui::Text("Current Camera %s", CameraStrings[m_currentCamera].c_str());
    //        }        
    //        ImGui::End();

    //        //hacky stand in for reticule :3
    //        if (m_gameScene.getActiveCamera() == m_freeCam)
    //        {
    //            auto size = glm::vec2(cro::App::getWindow().getSize());
    //            const glm::vec2 pointSize(6.f);

    //            auto pos = (size - pointSize) / 2.f;
    //            ImGui::SetNextWindowPos({ pos.x, pos.y });
    //            ImGui::SetNextWindowSize({ pointSize.x, pointSize.y });
    //            ImGui::Begin("Point");
    //            ImGui::End();
    //        }
    //    });

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

    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Depthmap"))
    //        {
    //            /*for (auto y = 4; y >= 0; --y)
    //            {
    //                for (auto x = 0; x < 8; ++x)
    //                {
    //                    auto idx = y * 8 + x;
    //                    ImGui::Image(m_depthMap.getTextureAt(idx), { 80.f, 80.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //                    ImGui::SameLine();
    //                }
    //                ImGui::NewLine();
    //            }*/

    //            const auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    //            for (auto i = 0u; i < cam.shadowMapBuffer.getLayerCount(); ++i)
    //            {
    //                ImGui::Image(cam.shadowMapBuffer.getTexture(i), { 256.f, 256.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //                ImGui::SameLine();
    //            }
    //        }
    //        ImGui::End();
    //    },true);

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