/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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
#include "GolfSoundDirector.hpp"
#include "CommandIDs.hpp"
#include "SpectatorSystem.hpp"
#include "SpectatorAnimCallback.hpp"
#include "PropFollowSystem.hpp"
#include "PoissonDisk.hpp"
#include "Career.hpp"
#include "AvatarRotationSystem.hpp"
#include "BannerTexture.hpp"

#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/util/Maths.hpp>

#include <Input.hpp>

#include "../Colordome-32.hpp"
#include "../ErrorCheck.hpp"

namespace pd = thinks;

namespace
{
#include "shaders/ShaderIncludes.inl"
#include "shaders/CelShader.inl"
#include "shaders/GlowShader.inl"
#include "shaders/ShadowMapping.inl"
#include "shaders/BillboardShader.inl"
#include "shaders/TreeShader.inl"
#include "shaders/MinimapShader.inl"
#include "shaders/TransitionShader.inl"
#include "shaders/CompositeShader.inl"
#include "shaders/WireframeShader.inl"
#include "shaders/BeaconShader.inl"
#include "shaders/CloudShader.inl"
#include "shaders/WaterShader.inl"
#include "shaders/PostProcess.inl"
#include "shaders/Glass.inl"
#include "shaders/Blur.inl"
#include "shaders/LensFlare.inl"
#include "shaders/EmissiveShader.inl"
#include "shaders/ShopItems.inl"
#include "shaders/Hole.inl"
#include "shaders/Hologram.inl"
#include "shaders/TerrainMaterials.inl"
#include "shaders/Weather.inl"

    //colour is normal colour with dark shadow
    const std::array BannerStrings =
    {
        cro::String("If you can read this\nyou're flying too close"),
        cro::String("Buy Pentworth's\nIndispensible Lube"),
        cro::String("Will You Marry Me?"),
        cro::String("Cats are better than Dogs"),
        cro::String("I can see my house from here"),
        cro::String("You Can't Go Wrong\nWith a 50ft Dong."),
        cro::String("Harding's Balls"),
        cro::String("Claire: Have you seen my keys?\nThey're not where I left them"),
        cro::String("To truly find yourself you must\nplay hide and seek alone."),
        cro::String("There is no angry way to say\nBUBBLES"),
        cro::String("404"),
        cro::String("Remember to wash\nyour balls thoroughly.")
    };

    //make this static so throughout the duration of the game we
    //cycle without repetition (until we reach the end)
    static std::int32_t BannerIndex = cro::Util::Random::value(0, static_cast<std::int32_t>(BannerStrings.size()) - 1);
}

void GolfState::loadAssets()
{
    BannerIndex = (BannerIndex + 1) % BannerStrings.size();

    std::string skyboxPath = "assets/golf/images/skybox/billiards/trophy.ccm";
    //std::string skyboxPath = "assets/golf/courses/course_10/cmap/01/d/1/cmap.ccm";

    if (m_sharedData.nightTime)
    {
        //skyboxPath = "assets/golf/images/skybox/night/sky.ccm";
        m_lightVolumeDefinition.loadFromFile("assets/golf/models/light_sphere.cmt");
    }
    if (m_reflectionMap.loadFromFile(skyboxPath))
    {
        m_reflectionMap.generateMipMaps();
    }

    loadMaterials();

    loadSprites();

    loadModels();

    loadMap();

    //registerWindow([&]() 
    //    {
    //        ImGui::Begin("Sun");

    //        /*static float c[3] = { 1.f };

    //        if (ImGui::ColorPicker3("Sun", c))
    //        {
    //            cro::Colour col(c[0], c[1], c[2], 1.f);
    //            m_skyScene.getSunlight().getComponent<cro::Sunlight>().setColour(col);
    //            m_gameScene.getSunlight().getComponent<cro::Sunlight>().setColour(col);

    //        }
    //        ImGui::Text("R %3.2f, G %3.2f, B %3.2f", c[0], c[1], c[2]);*/

    //        static float multiplier = 10.f;
    //        if (ImGui::SliderFloat("Multiplier", &multiplier, 10.f, 250.f))
    //        {
    //            //m_gameScene.getSunlight().getComponent<cro::Sunlight>().setColour(cro::Colour::White * multiplier);
    //            m_gameScene.getSystem<cro::ModelRenderer>()->setLightMultiplier(multiplier);
    //        }


    //        ImGui::End();
    //    });
}

void GolfState::loadMap()
{
    //used when parsing holes
    const auto addCrowd = [&](HoleData& holeData, glm::vec3 position, glm::vec3 lookAt, float rotation, std::int32_t crowdIdx)
        {
            constexpr auto MapOrigin = glm::vec3(MapSize.x / 2.f, 0.f, -static_cast<float>(MapSize.y) / 2.f);
            static std::int32_t seed = 0;

            struct CrowdContext final
            {
                std::array<float, 2u> start = {};
                std::array<float, 2u> end = {};
                float density = 1.f;
                constexpr CrowdContext(std::array<float, 2u> s, std::array<float, 2u> e, float d)
                    : start(s), end(e), density(d) {}

                float checkpoint(const std::array<float, 2u>& point) const
                {
                    const float p = ((point[0] * point[0]) / (end[0] * end[0]))
                                + ((point[1] * point[1]) / (end[1] * end[1]));

                    return p;
                }
            };
            constexpr std::array<CrowdContext, 4u> Contexts =
            {
                CrowdContext({ -8.f, -1.5f }, { 8.f, 1.5f }, 1.75f),
                CrowdContext({ -8.f, -1.5f }, { 8.f, 1.5f }, 0.75f),
                CrowdContext({ -16.f, -3.5f }, { 16.f, 3.5f }, 0.75f),
                CrowdContext({ -18.f, -5.5f }, { 18.f, 5.5f }, 0.85f)
            };

            const auto dist = pd::PoissonDiskSampling(Contexts[crowdIdx].density, Contexts[crowdIdx].start, Contexts[crowdIdx].end, 30, seed++);

            rotation *= cro::Util::Const::degToRad;

            //used by terrain builder to create instanced geom
            const glm::mat4 rotMat = glm::rotate(glm::mat4(1.f), rotation, cro::Transform::Y_AXIS);

            for (const auto& p : dist)
            {
                if (Contexts[crowdIdx].checkpoint(p) < 1.f)
                {
                    auto offset = glm::vec3(rotMat * glm::vec4(p[0], 0.f, p[1], 1.f));
                    offset.x += 0.3f + (static_cast<float>(cro::Util::Random::value(2, 5)) / 100.f);
                    offset.z += static_cast<float>(cro::Util::Random::value(-10, 10)) / 100.f;

                    auto r = rotation + cro::Util::Random::value(-0.25f, 0.25f);

                    auto tx = glm::translate(glm::mat4(1.f), position - MapOrigin);
                    tx = glm::translate(tx, offset);

                    auto lookDir = lookAt - (glm::vec3(tx[3]) + MapOrigin);
                    const float len = glm::length2(lookDir);
                    if (len < 1600.f)
                    {
                        r = std::atan2(-lookDir.z, lookDir.x);
                        r += (90.f * cro::Util::Const::degToRad);
                    }
                    tx = glm::rotate(tx, r, cro::Transform::Y_AXIS);                      


                    float scale = static_cast<float>(cro::Util::Random::value(95, 110)) / 100.f;
                    tx = glm::scale(tx, glm::vec3(scale));

                    holeData.crowdPositions[crowdIdx].push_back(tx);
                }
            }
        };

    //presets for lighting volume
    std::unordered_map<std::string, LightData> lightPresets;
    if (m_sharedData.nightTime)
    {
        auto files = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + u8"assets/golf/lights");
        for (const auto& file : files)
        {
            auto ext = cro::FileSystem::getFileExtension(file);
            if (ext == ".lgt")
            {
                LightData preset;

                cro::ConfigFile cfg;
                cfg.loadFromFile("assets/golf/lights/" + file);
                const auto& props = cfg.getProperties();
                for (const auto& prop : props)
                {
                    const auto& name = prop.getName();
                    if (name == "colour")
                    {
                        preset.colour = prop.getValue<cro::Colour>();
                    }
                    else if (name == "radius")
                    {
                        preset.radius = prop.getValue<float>();
                    }
                    else if (name == "animation")
                    {
                        preset.animation = prop.getValue<std::string>();
                    }
                    else if (name == "lens_flare")
                    {
                        preset.lensFlare = prop.getValue<bool>();
                    }
                }

                if (preset.radius > 0.01f)
                {
                    lightPresets.insert(std::make_pair(file.substr(0, file.find(ext)), preset));
                }
            }
        }
    }

    //load the map data
    bool error = false;
    bool hasSpectators = false;
    const auto mapDir = m_sharedData.mapDirectory.toAnsiString();

    const auto installPaths = Content::getInstallPaths();

    std::string mapPath;
    for (const auto& dir : installPaths)
    {
        mapPath = dir + ConstVal::MapPath + mapDir;
        if (cro::FileSystem::directoryExists(cro::FileSystem::getResourcePath() + mapPath))
        {
            break;
        }
    }
    mapPath += +"/course.data";

    bool isUser = false;
    if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + mapPath))
    {
        auto coursePath = cro::App::getPreferencePath() + ConstVal::UserMapPath;
        if (!cro::FileSystem::directoryExists(coursePath))
        {
            cro::FileSystem::createDirectory(coursePath);
        }

        mapPath = cro::App::getPreferencePath() + ConstVal::UserMapPath + mapDir + "/course.data";
        isUser = true;

        if (!cro::FileSystem::fileExists(mapPath))
        {
            LOG("Course file doesn't exist", cro::Logger::Type::Error);
            error = true;
        }
    }

    cro::ConfigFile courseFile;
    if (!courseFile.loadFromFile(mapPath, !isUser))
    {
        error = true;
    }

    if (auto* title = courseFile.findProperty("title"); title)
    {
        m_courseTitle = title->getValue<cro::String>();
    }

    std::string skyboxPath;

    ThemeSettings theme;
    std::vector<std::string> holeStrings;
    const auto& props = courseFile.getProperties();
    for (const auto& prop : props)
    {
        const auto& name = prop.getName();
        if (name == "hole"
            && holeStrings.size() < MaxHoles)
        {
            holeStrings.push_back(prop.getValue<std::string>());
        }
        else if (name == "skybox")
        {
            skyboxPath = prop.getValue<std::string>();

            //if set to night check for night path (appended with _n)
            if (m_sharedData.nightTime)
            {
                auto ext = cro::FileSystem::getFileExtension(skyboxPath);
                auto nightPath = skyboxPath.substr(0, skyboxPath.find(ext)) + "_n" + ext;
                if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + nightPath))
                {
                    skyboxPath = nightPath;
                }
                m_skyScene.getSunlight().getComponent<cro::Sunlight>().setColour(SkyNight);
                m_gameScene.getSunlight().getComponent<cro::Sunlight>().setColour(SkyNight);
            }
        }
        else if (name == "shrubbery")
        {
            cro::ConfigFile shrubbery;
            if (shrubbery.loadFromFile(prop.getValue<std::string>()))
            {
                const auto& shrubProps = shrubbery.getProperties();
                for (const auto& sp : shrubProps)
                {
                    const auto& shrubName = sp.getName();
                    if (shrubName == "model")
                    {
                        theme.billboardModel = sp.getValue<std::string>();
                    }
                    else if (shrubName == "sprite")
                    {
                        theme.billboardSprites = sp.getValue<std::string>();
                    }
                    else if (shrubName == "treeset")
                    {
                        Treeset ts;
                        if (theme.treesets.size() < ThemeSettings::MaxTreeSets
                            && ts.loadFromFile(sp.getValue<std::string>()))
                        {
                            theme.treesets.push_back(ts);
                        }
                    }
                    else if (shrubName == "grass")
                    {
                        theme.grassColour = sp.getValue<cro::Colour>();
                    }
                    else if (shrubName == "grass_tint")
                    {
                        theme.grassTint = sp.getValue<cro::Colour>();
                    }
                }
            }
        }
        else if (name == "audio")
        {
            auto audioPath = prop.getValue<std::string>();

            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + audioPath))
            {
                m_audioPath = audioPath;
            }
        }
        else if (name == "instance_model")
        {
            theme.instancePath = prop.getValue<std::string>();
        }
    }

    //use old sprites if user so wishes
    if (m_sharedData.treeQuality == SharedStateData::TreeQuality::Classic)
    {
        std::string classicModel;
        std::string classicSprites;

        //assume we have the correct file extension, because it's an invalid path anyway if not.
        classicModel = theme.billboardModel.substr(0, theme.billboardModel.find_last_of('.')) + "_low.cmt";
        classicSprites = theme.billboardSprites.substr(0, theme.billboardSprites.find_last_of('.')) + "_low.spt";

        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + classicModel))
        {
            classicModel.clear();
        }
        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + classicSprites))
        {
            classicSprites.clear();
        }

        if (!classicModel.empty() && !classicSprites.empty())
        {
            theme.billboardModel = classicModel;
            theme.billboardSprites = classicSprites;
        }
    }

    SkyboxMaterials materials;
    materials.horizon = m_materialIDs[MaterialID::Horizon];
    materials.horizonSun = m_materialIDs[MaterialID::HorizonSun];
    materials.skinned = m_materialIDs[MaterialID::CelTexturedSkinned];
    materials.glass = m_materialIDs[MaterialID::Glass];

    auto cloudRing = loadSkybox(skyboxPath, m_skyScene, m_resources, materials);
    if (cloudRing.isValid()
        && cloudRing.hasComponent<cro::Model>())
    {
        m_resources.shaders.loadFromString(ShaderID::CloudRing, CloudOverheadVertex, CloudOverheadFragment, "#define REFLECTION\n#define POINT_LIGHT\n");
        auto& shader = m_resources.shaders.get(ShaderID::CloudRing);

        auto matID = m_resources.materials.add(shader);
        auto material = m_resources.materials.get(matID);
        material.setProperty("u_skyColourTop", m_skyScene.getSkyboxColours().top);
        material.setProperty("u_skyColourBottom", m_skyScene.getSkyboxColours().middle);
        cloudRing.getComponent<cro::Model>().setMaterial(0, material);
    }

    
    if (!materials.showWater)
    {
        //the water isn't loaded yet, so we have to send via callback to delay until the scene is updated
        auto e = m_gameScene.createEntity();
        e.addComponent<cro::Callback>().active = true;
        e.getComponent<cro::Callback>().function =
            [&](cro::Entity f, float)
            {
                m_waterEnt.getComponent<cro::Model>().setFacing(cro::Model::Facing::Back);
                f.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(f);
            };

    }
    if (!m_sharedData.nightTime)
    {
        m_skyScene.getSunlight().getComponent<cro::Sunlight>().setColour(materials.sunColour);
        m_gameScene.getSunlight().getComponent<cro::Sunlight>().setColour(materials.sunColour);
    }


    if (materials.requestLensFlare
        && m_sharedData.weatherType == WeatherType::Clear)
    {
        m_lensFlare.sunPos = materials.sunPos;

        //this is just used to signal the UI creation that we
        //have a shader and we want to use it
        if (m_resources.shaders.loadFromString(ShaderID::LensFlare, cro::RenderSystem2D::getDefaultVertexShader(), LensFlareFrag, "#define TEXTURED\n"))
        {
            m_materialIDs[MaterialID::LensFlare] = ShaderID::LensFlare;

            const auto& shader = m_resources.shaders.get(ShaderID::LensFlare);
            m_lensFlare.shaderID = shader.getGLHandle();
            m_lensFlare.positionUniform = shader.getUniformID("u_sourcePosition");
        }
    }

    if (m_sharedData.nightTime
        && m_resources.shaders.loadFromString(ShaderID::PointFlare, cro::RenderSystem2D::getDefaultVertexShader(), PointFlareFrag, "#define TEXTURED\n"))
    {
        //again, not a proper material, juts a signal we loaded successfully
        m_materialIDs[MaterialID::PointFlare] = ShaderID::PointFlare;
    }

    if (m_sharedData.nightTime)
    {
        auto skyDark = SkyBottom.getVec4() * SkyNight.getVec4();
        auto colours = m_skyScene.getSkyboxColours();
        colours.bottom = skyDark;
        m_skyScene.setSkyboxColours(colours);

        if (m_sharedData.weatherType == WeatherType::Clear)
        {
            createFireworks();
            createRoids();
        }
    }

#ifdef CRO_DEBUG_
    auto& colours = m_skyScene.getSkyboxColours();
    //topSky = colours.top.getVec4();
    //bottomSky = colours.middle.getVec4();
#endif

    if (theme.billboardModel.empty()
        || !cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + theme.billboardModel))
    {
        LogE << "Missing or invalid billboard model definition" << std::endl;
        error = true;
    }
    if (theme.billboardSprites.empty()
        || !cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + theme.billboardSprites))
    {
        LogE << "Missing or invalid billboard sprite sheet" << std::endl;
        error = true;
    }
    if (holeStrings.empty())
    {
        LOG("No hole files in course data", cro::Logger::Type::Error);
        error = true;
    }

    //check the rules and truncate hole list
    //if requested - 1 front holes, 1 back holes
    if (m_sharedData.holeCount == 1)
    {
        auto size = std::max(std::size_t(1), holeStrings.size() / 2);
        holeStrings.resize(size);
    }
    else if (m_sharedData.holeCount == 2)
    {
        auto start = holeStrings.size() / 2;
        std::vector<std::string> newStrings(holeStrings.begin() + start, holeStrings.end());
        holeStrings.swap(newStrings);
    }

    //and reverse if rules request
    if (m_sharedData.reverseCourse)
    {
        std::reverse(holeStrings.begin(), holeStrings.end());
    }


    cro::ConfigFile holeCfg;
    cro::ModelDefinition modelDef(m_resources);
    std::string prevHoleString;
    cro::Entity prevHoleEntity;
    std::vector<LightData> prevLightData;
    std::vector<cro::Entity> prevProps;
    std::vector<cro::Entity> prevParticles;
    std::vector<cro::Entity> prevAudio;
    std::vector<cro::Entity> leaderboardProps;
    std::int32_t holeModelCount = 0; //use this to get a guestimate of how many holes per model there are to adjust the camera offset
    cro::ModelDefinition md(m_resources);

    cro::AudioScape propAudio;
    propAudio.loadFromFile("assets/golf/sound/props.xas", m_resources.audio);

    cro::Image currentMap(true);

    for (const auto& hole : holeStrings)
    {
        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + hole))
        {
            LOG("Hole file is missing", cro::Logger::Type::Error);
            error = true;
        }

        if (!holeCfg.loadFromFile(hole))
        {
            LOG("Failed opening hole file", cro::Logger::Type::Error);
            error = true;
        }

        static constexpr std::int32_t MaxProps = 6;
        std::int32_t propCount = 0;
        auto& holeData = m_holeData.emplace_back();
        bool duplicate = false;

        std::vector<std::string> includeFiles;

        const auto& holeProps = holeCfg.getProperties();
        for (const auto& holeProp : holeProps)
        {
            const auto& name = holeProp.getName();
            if (name == "map")
            {
                auto path = holeProp.getValue<std::string>();
                if (!currentMap.loadFromFile(path)
                    || currentMap.getFormat() != cro::ImageFormat::RGBA)
                {
                    LogE << path << ": image file not RGBA" << std::endl;
                    error = true;
                }
                holeData.mapPath = holeProp.getValue<std::string>();
                propCount++;
            }
            else if (name == "pin")
            {
                //TODO not sure how we ensure these are sane values?
                holeData.pin = holeProp.getValue<glm::vec3>();
                holeData.pin.x = glm::clamp(holeData.pin.x, 0.f, MapSizeFloat.x);
                holeData.pin.z = glm::clamp(holeData.pin.z, -MapSizeFloat.y, 0.f);
                propCount++;
            }
            else if (name == "tee")
            {
                holeData.tee = holeProp.getValue<glm::vec3>();
                holeData.tee.x = glm::clamp(holeData.tee.x, 0.f, MapSizeFloat.x);
                holeData.tee.z = glm::clamp(holeData.tee.z, -MapSizeFloat.y, 0.f);
                propCount++;
            }
            else if (name == "target")
            {
                holeData.target = holeProp.getValue<glm::vec3>();
                if (glm::length2(holeData.target) > 0)
                {
                    propCount++;
                }
            }
            else if (name == "subtarget")
            {
                holeData.subtarget = holeProp.getValue<glm::vec3>();
            }
            else if (name == "par")
            {
                holeData.par = holeProp.getValue<std::int32_t>();
                if (holeData.par < 1 || holeData.par > 10)
                {
                    LOG("Invalid PAR value", cro::Logger::Type::Error);
                    error = true;
                }
                propCount++;
            }
            else if (name == "model")
            {
                auto modelPath = holeProp.getValue<std::string>();

                if (modelPath != prevHoleString)
                {
                    //attept to load model
                    if (modelDef.loadFromFile(modelPath))
                    {
                        holeData.modelPath = modelPath;

                        holeData.modelEntity = m_gameScene.createEntity();
                        holeData.modelEntity.addComponent<cro::Transform>();
                        holeData.modelEntity.addComponent<cro::Callback>();
                        modelDef.createModel(holeData.modelEntity);
                        holeData.modelEntity.getComponent<cro::Model>().setHidden(true);
                        for (auto m = 0u; m < holeData.modelEntity.getComponent<cro::Model>().getMeshData().submeshCount; ++m)
                        {
                            if (!holeData.modelEntity.getComponent<cro::Model>().getMaterialData(cro::Mesh::IndexData::Pass::Final, m).customShader)
                            {
                                auto material = m_resources.materials.get(m_materialIDs[MaterialID::Course]);
                                applyMaterialData(modelDef, material, m);
                                holeData.modelEntity.getComponent<cro::Model>().setMaterial(m, material);
                            }
                        }
                        propCount++;

                        //allow the model to scale about its own centre, not world
                        auto offset = holeData.modelEntity.getComponent<cro::Model>().getMeshData().boundingBox.getCentre();
                        offset.y = 0.f;
                        holeData.modelEntity.getComponent<cro::Transform>().setPosition(offset);
                        holeData.modelEntity.getComponent<cro::Transform>().setOrigin(offset);


                        prevHoleString = modelPath;
                        prevHoleEntity = holeData.modelEntity;

                        holeModelCount++;

                        //duplicate for rendering on minimap
                        m_minimapModels.emplace_back() = m_mapScene.createEntity();
                        m_minimapModels.back().addComponent<cro::Transform>();
                        modelDef.createModel(m_minimapModels.back());
                        m_minimapModels.back().getComponent<cro::Model>().setHidden(true);
                        
                        for (auto m = 0u; m < m_minimapModels.back().getComponent<cro::Model>().getMeshData().submeshCount; ++m)
                        {
                            if (!holeData.modelEntity.getComponent<cro::Model>().getMaterialData(cro::Mesh::IndexData::Pass::Final, m).customShader)
                            {
                                auto material = m_resources.materials.get(m_materialIDs[MaterialID::Minimap]);
                                applyMaterialData(modelDef, material, m);
                                m_minimapModels.back().getComponent<cro::Model>().setMaterial(m, material);
                            }
                        }
                    }
                    else
                    {
                        LOG("Failed loading model file", cro::Logger::Type::Error);
                        error = true;
                    }
                }
                else
                {
                    //duplicate the hole by copying the previous model entity
                    holeData.modelPath = prevHoleString;
                    holeData.modelEntity = prevHoleEntity;
                    duplicate = true;
                    propCount++;

                    m_minimapModels.push_back(m_minimapModels.back());
                }
            }
            else if (name == "include")
            {
                includeFiles.push_back(holeProp.getValue<std::string>());
            }
        }

        if (propCount != MaxProps)
        {
            LOG("Missing hole property", cro::Logger::Type::Error);
            error = true;
        }
        else
        {
            if (!duplicate) //this hole wasn't a duplicate of the previous
            {
                const auto parseLightData = [&](const cro::ConfigObject& obj, cro::Entity parent = {})
                    {
                        if (m_sharedData.nightTime)
                        {
                            auto& lightData = holeData.lightData.emplace_back();
                            std::string preset;

                            const auto& lightProps = obj.getProperties();
                            for (const auto& lightProp : lightProps)
                            {
                                const auto& propName = lightProp.getName();
                                if (propName == "radius")
                                {
                                    lightData.radius = std::clamp(lightProp.getValue<float>(), 0.1f, 20.f);
                                }
                                else if (propName == "colour")
                                {
                                    lightData.colour = lightProp.getValue<cro::Colour>();
                                }
                                else if (propName == "position")
                                {
                                    lightData.position = lightProp.getValue<glm::vec3>();
                                    lightData.position.y += 0.01f;
                                }
                                else if (propName == "animation")
                                {
                                    auto str = lightProp.getValue<std::string>();
                                    auto len = std::min(std::size_t(20), str.length());
                                    lightData.animation = str.substr(0, len);
                                }
                                else if (propName == "preset")
                                {
                                    preset = lightProp.getValue<std::string>();
                                }
                            }

                            if (!preset.empty() && lightPresets.count(preset) != 0)
                            {
                                const auto& p = lightPresets.at(preset);
                                //presets take precedence, except for animation
                                lightData.colour = p.colour;
                                lightData.radius = p.radius;
                                lightData.lensFlare = p.lensFlare;

                                if (lightData.animation.empty())
                                {
                                    lightData.animation = p.animation;
                                }
                            }
                            lightData.parent = parent;
                        }
                    };

                //look for prop models (are optional and can fail to load no problem)
                const auto parseProps = [&](const std::vector<cro::ConfigObject>& propObjs)
                    {
                        for (const auto& obj : propObjs)
                        {
                            const auto& name = obj.getName();
                            if (name == "prop")
                            {
                                const auto& modelProps = obj.getProperties();
                                glm::vec3 position(0.f);
                                float rotation = 0.f;
                                glm::vec3 scale(1.f);
                                std::string path;

                                std::vector<glm::vec3> curve;
                                bool loopCurve = true;
                                float loopDelay = 4.f;
                                float loopSpeed = 6.f;
                                float radiusMultiplier = 1.f; //hack because models with a wake eg boats push the bounding radius too far

                                std::string particlePath;
                                std::string emitterName;

                                const cro::ConfigObject* childLight = nullptr;

                                for (const auto& modelProp : modelProps)
                                {
                                    auto propName = modelProp.getName();
                                    if (propName == "position")
                                    {
                                        position = modelProp.getValue<glm::vec3>();
                                    }
                                    else if (propName == "model")
                                    {
                                        path = modelProp.getValue<std::string>();

                                        if (m_sharedData.nightTime)
                                        {
                                            //see if there's a specific model
                                            auto ext = cro::FileSystem::getFileExtension(path);
                                            if (!ext.empty())
                                            {
                                                auto nightPath = path.substr(0, path.find(ext)) + "_night" + ext;
                                                if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + nightPath))
                                                {
                                                    path = nightPath;
                                                }
                                            }
                                        }
                                    }
                                    else if (propName == "rotation")
                                    {
                                        rotation = modelProp.getValue<float>();
                                    }
                                    else if (propName == "scale")
                                    {
                                        scale = modelProp.getValue<glm::vec3>();
                                    }
                                    else if (propName == "particles")
                                    {
                                        particlePath = modelProp.getValue<std::string>();
                                    }
                                    else if (propName == "emitter")
                                    {
                                        emitterName = modelProp.getValue<std::string>();
                                    }
                                }

                                const auto& modelObjs = obj.getObjects();
                                for (const auto& o : modelObjs)
                                {
                                    if (o.getName() == "path")
                                    {
                                        const auto& points = o.getProperties();
                                        for (const auto& p : points)
                                        {
                                            if (p.getName() == "point")
                                            {
                                                curve.push_back(p.getValue<glm::vec3>());
                                            }
                                            else if (p.getName() == "loop")
                                            {
                                                loopCurve = p.getValue<bool>();
                                            }
                                            else if (p.getName() == "delay")
                                            {
                                                loopDelay = std::max(0.f, p.getValue<float>());
                                            }
                                            else if (p.getName() == "speed")
                                            {
                                                loopSpeed = std::max(0.f, p.getValue<float>());
                                            }
                                        }

                                        //break;
                                    }
                                    else if (o.getName() == "light"
                                        && !childLight) //only add the first one
                                    {
                                        childLight = &o;
                                    }
                                }

                                if (!path.empty() && Social::isValid(path)
                                    && cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + path))
                                {
                                    if (modelDef.loadFromFile(path))
                                    {
                                        auto ent = m_gameScene.createEntity();
                                        ent.addComponent<cro::Transform>().setPosition(position);
                                        ent.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation * cro::Util::Const::degToRad);
                                        ent.getComponent<cro::Transform>().setScale(scale);
                                        modelDef.createModel(ent);
                                        if (modelDef.hasSkeleton())
                                        {
                                            for (auto i = 0u; i < modelDef.getMaterialCount(); ++i)
                                            {
                                                auto texMatID = MaterialID::CelTexturedSkinned;

                                                if (modelDef.getMaterial(i)->properties.count("u_maskMap") != 0)
                                                {
                                                    texMatID = MaterialID::CelTexturedSkinnedMasked;
                                                }

                                                auto texturedMat = m_resources.materials.get(m_materialIDs[texMatID]);
                                                applyMaterialData(modelDef, texturedMat, i);
                                                ent.getComponent<cro::Model>().setMaterial(i, texturedMat);
                                            }

                                            auto& skel = ent.getComponent<cro::Skeleton>();
                                            if (!skel.getAnimations().empty())
                                            {
                                                //this is the default behaviour
                                                const auto& anims = skel.getAnimations();
                                                if (anims.size() == 1)
                                                {
                                                    //ent.getComponent<cro::Skeleton>().play(0); // don't play this until unhidden
                                                    skel.getAnimations()[0].looped = true;
                                                    skel.setMaxInterpolationDistance(100.f);
                                                }
                                                //however spectator models need fancier animation
                                                //control... and probably a smaller interp distance
                                                else
                                                {
                                                    //TODO we could improve this by disabling when hidden?
                                                    ent.addComponent<cro::Callback>().active = true;
                                                    ent.getComponent<cro::Callback>().function = SpectatorCallback(anims);
                                                    ent.getComponent<cro::Callback>().setUserData<bool>(false);
                                                    ent.addComponent<cro::CommandTarget>().ID = CommandID::Spectator;

                                                    skel.setMaxInterpolationDistance(80.f);
                                                }
                                            }
                                        }
                                        else
                                        {
                                            bool useWind = ((ent.getComponent<cro::Model>().getMeshData().attributeFlags & cro::VertexProperty::Colour) != 0);
                                            for (auto i = 0u; i < modelDef.getMaterialCount(); ++i)
                                            {
                                                if (!ent.getComponent<cro::Model>().getMaterialData(cro::Mesh::IndexData::Pass::Final, i).customShader)
                                                {
                                                    auto texMatID = useWind ? MaterialID::CelTextured : MaterialID::CelTexturedNoWind;

                                                    if (modelDef.hasTag(i, "glass"))
                                                    {
                                                        texMatID = MaterialID::Glass;
                                                    }

                                                    else if (modelDef.hasTag(i, "wake"))
                                                    {
                                                        texMatID = MaterialID::Wake;
                                                        radiusMultiplier = 0.5f;
                                                    }

                                                    else if (modelDef.getMaterial(i)->properties.count("u_maskMap") != 0)
                                                    {
                                                        texMatID = useWind ? MaterialID::CelTexturedMasked : MaterialID::CelTexturedMaskedNoWind;
                                                    }
                                                    auto texturedMat = m_resources.materials.get(m_materialIDs[texMatID]);

                                                    //if this is a wake material we need to set the animation speed
                                                    //based on the speed of the path if the model has one.
                                                    if (texMatID == MaterialID::Wake &&
                                                        !curve.empty())
                                                    {
                                                        texturedMat.setProperty("u_speed", loopSpeed / 4.f/*std::clamp(loopSpeed, 0.f, 1.f)*/);
                                                    }

                                                    applyMaterialData(modelDef, texturedMat, i);
                                                    ent.getComponent<cro::Model>().setMaterial(i, texturedMat);

                                                    // only do this if we have vertex animation, else the default will suffice
                                                    if (useWind)
                                                    {
                                                        auto shadowMat = m_resources.materials.get(m_materialIDs[MaterialID::ShadowMap]);
                                                        applyMaterialData(modelDef, shadowMat);
                                                        ent.getComponent<cro::Model>().setShadowMaterial(i, shadowMat);
                                                    }
                                                }
                                            }
                                        }
                                        ent.getComponent<cro::Model>().setHidden(true);
                                        ent.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));

                                        holeData.modelEntity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                                        holeData.propEntities.push_back(ent);

                                        //special case for leaderboard model, cos, y'know
                                        if (cro::FileSystem::getFileName(path) == "leaderboard.cmt")
                                        {
                                            leaderboardProps.push_back(ent);
                                        }
                                        else if (cro::FileSystem::getFileName(path).find("rotating_billboard") != std::string::npos)
                                        {
                                            if (modelDef.hasSkeleton())
                                            {
                                                //animation callback
                                                ent.addComponent<cro::Callback>().setUserData<float>(15.f);
                                                ent.getComponent<cro::Callback>().function =
                                                    [](cro::Entity e, float dt)
                                                    {
                                                        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                                                        currTime -= dt;
                                                        if (currTime < 0.f)
                                                        {
                                                            currTime += 15.f;
                                                            e.getComponent<cro::Skeleton>().play(0, 1.f, 0.1f, false);
                                                            e.getComponent<cro::Callback>().active = false;

                                                            e.getComponent<cro::AudioEmitter>().play();
                                                        }
                                                    };
                                                ent.addComponent<cro::AudioEmitter>() = propAudio.getEmitter("billboard");

                                                if (m_billboardVideo.loadFromFile("assets/golf/video/hardings.mpg"))
                                                {
                                                    m_billboardVideo.play();
                                                    m_billboardVideo.setLooped(true);
                                                    ent.getComponent<cro::Model>().setMaterialProperty(2, "u_diffuseMap", cro::TextureID(m_billboardVideo.getTexture()));
                                                }
#ifdef USE_GNS
                                                initBillboardLeagueTexture();
                                                if (m_billboardLeagueTexture.available())
                                                {
                                                    ent.getComponent<cro::Model>().setMaterialProperty(3, "u_diffuseMap", cro::TextureID(m_billboardLeagueTexture.getTexture()));
                                                }
#endif
                                            }
                                        }

                                        //add path if it exists
                                        if (curve.size() > 3)
                                        {
                                            //we need to slow the rotation of big models such as the blimp
                                            const float turnSpeed = std::min(6.f / ((ent.getComponent<cro::Model>().getBoundingSphere().radius * radiusMultiplier) + 0.001f), 2.f);
                                            //LogI << cro::FileSystem::getFileName(path) << " needs model updating" << std::endl;
                                            
                                            if (loopCurve)
                                            {
                                                curve.back() = curve.front();
                                            }

                                            ent.addComponent<PropFollower>().path = curve;
                                            ent.getComponent<PropFollower>().loop = loopCurve;
                                            ent.getComponent<PropFollower>().idleTime = loopDelay;
                                            ent.getComponent<PropFollower>().moveSpeed = loopSpeed;
                                            ent.getComponent<PropFollower>().turnSpeed = turnSpeed;
                                            ent.getComponent<cro::Transform>().setPosition(curve[0]);
                                        }

                                        //add child particles if they exist
                                        if (!particlePath.empty())
                                        {
                                            cro::EmitterSettings settings;
                                            if (settings.loadFromFile(particlePath, m_resources.textures))
                                            {
                                                auto pEnt = m_gameScene.createEntity();
                                                pEnt.addComponent<cro::Transform>();
                                                pEnt.addComponent<cro::ParticleEmitter>().settings = settings;
                                                pEnt.getComponent<cro::ParticleEmitter>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));
                                                pEnt.addComponent<cro::CommandTarget>().ID = CommandID::ParticleEmitter;
                                                ent.getComponent<cro::Transform>().addChild(pEnt.getComponent<cro::Transform>());
                                                holeData.particleEntities.push_back(pEnt);
                                            }
                                        }

                                        //and child light
                                        if (childLight)
                                        {
                                            parseLightData(*childLight, ent);
                                        }

                                        //and child audio
                                        if (propAudio.hasEmitter(emitterName))
                                        {
                                            struct AudioCallbackData final
                                            {
                                                glm::vec3 prevPos = glm::vec3(0.f);
                                                float fadeAmount = 0.f;
                                                float currentVolume = 0.f;
                                            };

                                            auto audioEnt = m_gameScene.createEntity();
                                            audioEnt.addComponent<cro::Transform>();
                                            audioEnt.addComponent<cro::AudioEmitter>() = propAudio.getEmitter(emitterName);
                                            auto baseVolume = audioEnt.getComponent<cro::AudioEmitter>().getVolume();
                                            audioEnt.addComponent<cro::CommandTarget>().ID = CommandID::AudioEmitter;
                                            audioEnt.addComponent<cro::Callback>().setUserData<AudioCallbackData>();

                                            if (ent.hasComponent<PropFollower>())
                                            {
                                                audioEnt.getComponent<cro::Callback>().function =
                                                    [&, ent, baseVolume](cro::Entity e, float dt)
                                                    {
                                                        auto& [prevPos, fadeAmount, currentVolume] = e.getComponent<cro::Callback>().getUserData<AudioCallbackData>();
                                                        auto pos = ent.getComponent<cro::Transform>().getPosition();
                                                        auto velocity = (pos - prevPos) * 60.f; //frame time
                                                        prevPos = pos;
                                                        e.getComponent<cro::AudioEmitter>().setVelocity(velocity);

                                                        const float speed = ent.getComponent<PropFollower>().moveSpeed + 0.001f; //prevent div0
                                                        float pitch = smoothstep(0.005f, 0.6f, std::min(1.f, glm::length2(velocity) / (speed * speed)));
                                                        e.getComponent<cro::AudioEmitter>().setPitch(pitch);

                                                        //fades in when callback first started
                                                        fadeAmount = std::min(1.f, fadeAmount + dt);

                                                        //rather than just jump to volume, we move towards it for a
                                                        //smoother fade
                                                        auto targetVolume = (baseVolume * 0.1f) + (pitch * (baseVolume * 0.9f));
                                                        auto diff = targetVolume - currentVolume;
                                                        if (std::abs(diff) > 0.001f)
                                                        {
                                                            currentVolume += (diff * dt);
                                                        }
                                                        else
                                                        {
                                                            currentVolume = targetVolume;
                                                        }

                                                        e.getComponent<cro::AudioEmitter>().setVolume(currentVolume * fadeAmount);
                                                    };
                                            }
                                            else
                                            {
                                                //add a dummy function which will still be updated on hole end to remove this ent
                                                audioEnt.getComponent<cro::Callback>().function = [](cro::Entity, float) {};
                                            }
                                            ent.getComponent<cro::Transform>().addChild(audioEnt.getComponent<cro::Transform>());
                                            holeData.audioEntities.push_back(audioEnt);
                                        }

                                        //add headlights to carts
                                        if (m_sharedData.nightTime
                                            && path.find("cart0") != std::string::npos)
                                        {
                                            cro::ModelDefinition headlights(m_resources);
                                            if (headlights.loadFromFile("assets/golf/models/menu/headlights.cmt"))
                                            {
                                                auto lampEnt = m_gameScene.createEntity();
                                                lampEnt.addComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
                                                headlights.createModel(lampEnt);
                                                lampEnt.addComponent<cro::Callback>().active = true;
                                                lampEnt.getComponent<cro::Callback>().function =
                                                    [&,ent](cro::Entity e, float)
                                                    {
                                                        e.getComponent<cro::Model>().setHidden(ent.getComponent<cro::Model>().isHidden());
                                                        if (ent.destroyed())
                                                        {
                                                            m_gameScene.destroyEntity(e);
                                                        }
                                                    };
                                                ent.getComponent<cro::Transform>().addChild(lampEnt.getComponent<cro::Transform>());
                                            }
                                        }
                                    }
                                }
                            }
                            else if (name == "particles")
                            {
                                const auto& particleProps = obj.getProperties();
                                glm::vec3 position(0.f);
                                std::string path;

                                for (auto particleProp : particleProps)
                                {
                                    auto propName = particleProp.getName();
                                    if (propName == "path")
                                    {
                                        path = particleProp.getValue<std::string>();
                                    }
                                    else if (propName == "position")
                                    {
                                        position = particleProp.getValue<glm::vec3>();
                                    }
                                }

                                if (!path.empty())
                                {
                                    cro::EmitterSettings settings;
                                    if (settings.loadFromFile(path, m_resources.textures))
                                    {
                                        auto ent = m_gameScene.createEntity();
                                        ent.addComponent<cro::Transform>().setPosition(position);
                                        ent.addComponent<cro::ParticleEmitter>().settings = settings;
                                        ent.getComponent<cro::ParticleEmitter>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));
                                        ent.addComponent<cro::CommandTarget>().ID = CommandID::ParticleEmitter;
                                        holeData.particleEntities.push_back(ent);
                                        holeData.modelEntity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                                    }
                                }
                            }
                            else if (name == "crowd")
                            {
                                const auto& modelProps = obj.getProperties();
                                glm::vec3 position(0.f);
                                float rotation = 0.f;
                                glm::vec3 lookAt = holeData.pin;
                                std::int32_t minDensity = 0;

                                for (const auto& modelProp : modelProps)
                                {
                                    auto propName = modelProp.getName();
                                    if (propName == "position")
                                    {
                                        position = modelProp.getValue<glm::vec3>();
                                    }
                                    else if (propName == "rotation")
                                    {
                                        rotation = modelProp.getValue<float>();
                                    }
                                    else if (propName == "lookat")
                                    {
                                        lookAt = modelProp.getValue<glm::vec3>();
                                    }
                                    else if (propName == "min_density")
                                    {
                                        minDensity = std::clamp(modelProp.getValue<std::int32_t>(), 0, CrowdDensityCount - 1);
                                    }
                                }

                                std::vector<glm::vec3> curve;
                                const auto& modelObjs = obj.getObjects();
                                for (const auto& o : modelObjs)
                                {
                                    if (o.getName() == "path")
                                    {
                                        const auto& points = o.getProperties();
                                        for (const auto& p : points)
                                        {
                                            if (p.getName() == "point")
                                            {
                                                curve.push_back(p.getValue<glm::vec3>());
                                            }
                                        }

                                        break;
                                    }
                                }

                                if (curve.size() < 4)
                                {
                                    for (auto i = 0; i < CrowdDensityCount - 1; ++i) //leave the last set of transforms empty, to hide the crowd
                                    {
                                        if (minDensity <= /*m_sharedData.crowdDensity*/i)
                                        {
                                            addCrowd(holeData, position, lookAt, rotation, i);
                                        }
                                    }
                                }
                                else
                                {
                                    auto& spline = holeData.crowdCurves.emplace_back();
                                    for (auto p : curve)
                                    {
                                        spline.addPoint(p);
                                    }
                                    hasSpectators = true;
                                }
                            }
                            else if (name == "speaker")
                            {
                                std::string emitterName;
                                glm::vec3 position = glm::vec3(0.f);

                                const auto& speakerProps = obj.getProperties();
                                for (const auto& speakerProp : speakerProps)
                                {
                                    const auto& propName = speakerProp.getName();
                                    if (propName == "emitter")
                                    {
                                        emitterName = speakerProp.getValue<std::string>();
                                    }
                                    else if (propName == "position")
                                    {
                                        position = speakerProp.getValue<glm::vec3>();
                                    }
                                }

                                if (!emitterName.empty() &&
                                    propAudio.hasEmitter(emitterName))
                                {
                                    auto emitterEnt = m_gameScene.createEntity();
                                    emitterEnt.addComponent<cro::Transform>().setPosition(position);
                                    emitterEnt.addComponent<cro::AudioEmitter>() = propAudio.getEmitter(emitterName);
                                    float baseVol = emitterEnt.getComponent<cro::AudioEmitter>().getVolume();
                                    emitterEnt.getComponent<cro::AudioEmitter>().setVolume(0.f);
                                    emitterEnt.addComponent<cro::Callback>().function =
                                        [baseVol](cro::Entity e, float dt)
                                        {
                                            auto vol = e.getComponent<cro::AudioEmitter>().getVolume();
                                            vol = std::min(baseVol, vol + dt);
                                            e.getComponent<cro::AudioEmitter>().setVolume(vol);

                                            if (vol == baseVol)
                                            {
                                                e.getComponent<cro::Callback>().active = false;
                                            }
                                        };
                                    holeData.audioEntities.push_back(emitterEnt);
                                }
                            }
                            else if (name == "light")
                            {
                                parseLightData(obj);
                                //if (m_sharedData.nightTime)
                                //{
                                //    auto& lightData = holeData.lightData.emplace_back();
                                //    std::string preset;

                                //    const auto& lightProps = obj.getProperties();
                                //    for (const auto& lightProp : lightProps)
                                //    {
                                //        const auto& propName = lightProp.getName();
                                //        if (propName == "radius")
                                //        {
                                //            lightData.radius = std::clamp(lightProp.getValue<float>(), 0.1f, 20.f);
                                //        }
                                //        else if (propName == "colour")
                                //        {
                                //            lightData.colour = lightProp.getValue<cro::Colour>();
                                //        }
                                //        else if (propName == "position")
                                //        {
                                //            lightData.position = lightProp.getValue<glm::vec3>();
                                //            lightData.position.y += 0.01f;
                                //        }
                                //        else if (propName == "animation")
                                //        {
                                //            auto str = lightProp.getValue<std::string>();
                                //            auto len = std::min(std::size_t(20), str.length());
                                //            lightData.animation = str.substr(0, len);
                                //        }
                                //        else if (propName == "preset")
                                //        {
                                //            preset = lightProp.getValue<std::string>();
                                //        }
                                //    }

                                //    if (!preset.empty() && lightPresets.count(preset) != 0)
                                //    {
                                //        const auto& p = lightPresets.at(preset);
                                //        //presets take precedence, except for animation
                                //        lightData.colour = p.colour;
                                //        lightData.radius = p.radius;
                                //        if (lightData.animation.empty())
                                //        {
                                //            lightData.animation = p.animation;
                                //        }
                                //    }
                                //}
                            }
                        }
                    };
                parseProps(holeCfg.getObjects());

                for (const auto& include : includeFiles)
                {
                    cro::ConfigFile includeCfg;
                    if (includeCfg.loadFromFile(include))
                    {
                        parseProps(includeCfg.getObjects());
                    }
                }

                prevProps = holeData.propEntities;
                prevParticles = holeData.particleEntities;
                prevAudio = holeData.audioEntities;
                prevLightData = holeData.lightData;
            }
            else
            {
                holeData.propEntities = prevProps;
                holeData.particleEntities = prevParticles;
                holeData.audioEntities = prevAudio;
                holeData.lightData = prevLightData; //TODO is this necessary - we're creating the entities on the fly
            }

            //cos you know someone is dying to try and break the game :P
            if (holeData.pin != holeData.tee)
            {
                holeData.distanceToPin = glm::length(holeData.pin - holeData.tee);
            }
        }
    }

    //add the dynamically updated model to any leaderboard props
    if (!leaderboardProps.empty())
    {
        if (md.loadFromFile("assets/golf/models/leaderboard_panel.cmt"))
        {
            for (auto lb : leaderboardProps)
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>();
                md.createModel(entity);

                auto material = m_resources.materials.get(m_materialIDs[MaterialID::Leaderboard]);
                material.setProperty("u_subrect", glm::vec4(0.f, 0.5f, 1.f, 0.5f));
                entity.getComponent<cro::Model>().setMaterial(0, material);
                m_leaderboardTexture.addTarget(entity);

                //updates the texture rect depending on hole number
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<std::size_t>(m_currentHole);
                entity.getComponent<cro::Callback>().function =
                    [&, lb](cro::Entity e, float)
                    {
                        //the leaderboard might get tidies up on hole change
                        //so remove this ent if that's the case
                        if (lb.destroyed())
                        {
                            e.getComponent<cro::Callback>().active = false;
                            m_gameScene.destroyEntity(e);
                        }
                        else
                        {
                            auto currentHole = e.getComponent<cro::Callback>().getUserData<std::size_t>();
                            if (currentHole != m_currentHole)
                            {
                                if (m_currentHole > 8)
                                {
                                    e.getComponent<cro::Model>().setMaterialProperty(0, "u_subrect", glm::vec4(0.f, 0.f, 1.f, 0.5f));
                                }
                            }
                            currentHole = m_currentHole;
                            e.getComponent<cro::Model>().setHidden(lb.getComponent<cro::Model>().isHidden());
                        }
                    };
                entity.getComponent<cro::Model>().setHidden(true);

                lb.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            }
        }
    }


    //remove holes which failed to load - TODO should we delete partially loaded props here?
    m_holeData.erase(std::remove_if(m_holeData.begin(), m_holeData.end(),
        [](const HoleData& hd)
        {
            return !hd.modelEntity.isValid();
        }), m_holeData.end());

    //if we're running a short round, crop the number of holes
    if (m_sharedData.scoreType == ScoreType::ShortRound
        && m_courseIndex != -1)
    {
        switch (m_sharedData.holeCount)
        {
        default:
        case 0:
        {
            auto size = std::min(m_holeData.size(), std::size_t(12));
            m_holeData.resize(size);
        }
        break;
        case 1:
        case 2:
        {
            auto size = std::min(m_holeData.size(), std::size_t(6));
            m_holeData.resize(size);
        }
        break;
        }
    }


    auto* greenShader = &m_resources.shaders.get(ShaderID::CourseGreen); //greens use either this or the gridShader for their material
    auto* gridShader = &m_resources.shaders.get(ShaderID::CourseGrid); //used below when testing for putt holes.

    //check the crowd positions on every hole and set the height
    for (auto& hole : m_holeData)
    {
        m_collisionMesh.updateCollisionMesh(hole.modelEntity.getComponent<cro::Model>().getMeshData());

        for (auto& positions : hole.crowdPositions)
        {
            //remove spectators on the fairway or green
            positions.erase(std::remove_if(positions.begin(), positions.end(),
                [&](const glm::mat4& m)
                {
                    glm::vec3 pos = m[3];
                    pos.x += MapSize.x / 2;
                    pos.z -= MapSize.y / 2;

                    auto result = m_collisionMesh.getTerrain(pos);
                    return (result.terrain != TerrainID::Rough && result.terrain != TerrainID::Scrub && result.terrain != TerrainID::Stone) || !result.wasRayHit || result.height <= 0;
                }),
                positions.end());


            //make sure remaining positions are on the ground plane
            for (auto& m : positions)
            {
                glm::vec3 pos = m[3];
                pos.x += MapSize.x / 2;
                pos.z -= MapSize.y / 2;

                auto result = m_collisionMesh.getTerrain(pos);
                m[3][1] = result.height;
            }


            //remove spectators which intersect the bounding sphere of props
            for (auto pe : hole.propEntities)
            {
                auto sphere = pe.getComponent<cro::Model>().getBoundingSphere();
                sphere.centre += pe.getComponent<cro::Transform>().getPosition();

                positions.erase(std::remove_if(positions.begin(), positions.end(),
                    [sphere](const glm::mat4& m)
                    {
                        glm::vec3 pos = m[3];
                        pos.x += MapSize.x / 2;
                        pos.z -= MapSize.y / 2;

                        return sphere.contains(pos);
                    }),
                    positions.end());
            }

            std::shuffle(positions.begin(), positions.end(), cro::Util::Random::rndEngine);
        }

        for (auto& c : hole.crowdCurves)
        {
            for (auto& p : c.getPoints())
            {
                auto result = m_collisionMesh.getTerrain(p);
                p.y = result.height;
            }
        }

        //make sure the hole/target/tee position matches the terrain
        auto result = m_collisionMesh.getTerrain(hole.pin);
        hole.pin.y = result.height;

        result = m_collisionMesh.getTerrain(hole.target);
        hole.target.y = result.height;

        result = m_collisionMesh.getTerrain(hole.tee);
        hole.tee.y = result.height;



        //while we're here check if this is a putting
        //course by looking to see if the tee is on the green
        hole.puttFromTee = m_collisionMesh.getTerrain(hole.tee).terrain == TerrainID::Green;

        //update the green material with grid shader
        auto& model = hole.modelEntity.getComponent<cro::Model>();
        auto matCount = model.getMeshData().submeshCount;
        for (auto i = 0u; i < matCount; ++i)
        {
            auto mat = model.getMaterialData(cro::Mesh::IndexData::Final, i);
            if (mat.name == "green")
            {
                if (hole.puttFromTee)
                {
                    mat.setShader(*gridShader);
                }
                else
                {
                    mat.setShader(*greenShader);
                }
                model.setMaterial(i, mat);
            }
        }


        switch (m_sharedData.scoreType)
        {
        default: break;
        case ScoreType::MultiTarget:
            if (!hole.puttFromTee)
            {
                hole.par++;
            }
            break;
        }
    }


    if (error)
    {
        m_sharedData.errorMessage = "Failed to load course data\nSee console for more information";
        requestStackPush(StateID::Error);
    }
    else
    {
        m_holeToModelRatio = static_cast<float>(holeModelCount) / m_holeData.size();

        if (hasSpectators)
        {
            loadSpectators();
        }

        m_sharedData.minimapData.mapCentre = m_holeData[0].modelEntity.getComponent<cro::Model>().getMeshData().boundingBox.getCentre();
    }

    m_terrainBuilder.create(m_resources, m_gameScene, theme);

    //terrain builder will have loaded some shaders from which we need to capture some uniforms
    auto* shader = &m_resources.shaders.get(ShaderID::Terrain);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    shader = &m_resources.shaders.get(ShaderID::Slope);
    m_windBuffer.addShader(*shader);

    //only create overhead clouds if the skybox
    //requested we create any (and created the cloud ring
    if (cloudRing.isValid())
    {
        createClouds();
    }

    //reserve the slots for each hole score
    for (auto& client : m_sharedData.connectionData)
    {
        for (auto& player : client.playerData)
        {
            player.score = 0;
            player.holeScores.clear();
            player.holeScores.resize(holeStrings.size());
            std::fill(player.holeScores.begin(), player.holeScores.end(), 0);

            player.holeComplete.clear();
            player.holeComplete.resize(holeStrings.size());
            std::fill(player.holeComplete.begin(), player.holeComplete.end(), false);

            player.distanceScores.clear();
            player.distanceScores.resize(holeStrings.size());
            std::fill(player.distanceScores.begin(), player.distanceScores.end(), 0.f);
        }
    }

    for (auto& data : m_sharedData.timeStats)
    {
        data.totalTime = 0.f;
        data.holeTimes.clear();
        data.holeTimes.resize(holeStrings.size());
        std::fill(data.holeTimes.begin(), data.holeTimes.end(), 0.f);
    }


    const auto applySaveData = [&](std::uint64_t& holeIndex, std::vector<std::uint8_t>& scores, std::int32_t& mulliganCount)
        {
            m_currentHole = std::min(holeStrings.size() - 1, std::size_t(holeIndex));
            m_terrainBuilder.applyHoleIndex(m_currentHole);

            auto& player = m_sharedData.connectionData[0].playerData[0];
            player.holeScores.swap(scores);

            for (auto i = 0u; i < m_currentHole; ++i)
            {
                player.holeComplete[i] = true;

                //look at previous holes and see if we need to take on the crowd positions
                if (m_holeData[i].modelEntity == m_holeData[m_currentHole].modelEntity)
                {
                    if (m_holeData[m_currentHole].crowdPositions[0].empty())
                    {
                        for (auto j = 0u; j < m_holeData[i].crowdPositions.size(); ++j)
                        {
                            m_holeData[i].crowdPositions[j].swap(m_holeData[m_currentHole].crowdPositions[j]);
                        }
                    }

                    if (m_holeData[m_currentHole].crowdCurves.empty())
                    {
                        m_holeData[i].crowdCurves.swap(m_holeData[m_currentHole].crowdCurves);
                    }
                }
            }

            m_resumedFromSave = true;
            if (m_currentHole == 0)
            {
                mulliganCount = 1;
            }
        };
    
    
    //if this is a career game see if we had a round in progress
    if (m_sharedData.gameMode == GameMode::FreePlay)
    {
        Social::setLeaderboardsEnabled(true);
    }
    else
    {
        //always disable these in career
        Social::setLeaderboardsEnabled(false);

        const auto scoreSize = m_sharedData.connectionData[0].playerData[0].holeScores.size();

        std::uint64_t h = 0;
        std::vector<std::uint8_t> scores(scoreSize);
        std::fill(scores.begin(), scores.end(), 0);
        std::int32_t mulliganCount = 0;

        if (m_sharedData.leagueRoundID == LeagueRoundID::Club)
        {
            std::fill(scores.begin(), scores.end(), 0);

            //tournament
            for (auto i = 0u; i < scores.size(); ++i)
            {
                scores[i] = m_sharedData.tournaments[m_sharedData.activeTournament].scores[i];
                if (scores[i] != 0)
                {
                    h++;
                }
                else
                {
                    break;
                }
            }

            mulliganCount = m_sharedData.tournaments[m_sharedData.activeTournament].mulliganCount;

            if (h != 0)
            {
                applySaveData(h, scores, mulliganCount);
            }
        }
        else
        {
            //league
            if (Progress::read(m_sharedData.leagueRoundID, h, scores, mulliganCount))
            {
                if (h != 0)
                {
                    applySaveData(h, scores, mulliganCount);

                    //this might be an upgrade from the old league system
                    //so we need to fill in the in-progress scores
                    std::vector<std::int32_t> parVals;
                    for (auto i = 0u; i < m_currentHole; ++i)
                    {
                        parVals.push_back(m_holeData[i].par);
                        Career::instance(m_sharedData).getLeagueTables()[m_sharedData.leagueRoundID - LeagueRoundID::RoundOne].retrofitHoleScores(parVals);
                    }
                }
            }
        }
        m_mulliganCount = mulliganCount;
    }

    initAudio(theme.treesets.size() > 2, cloudRing.isValid());
}

void GolfState::loadMaterials()
{
    std::string wobble;
    if (m_sharedData.vertexSnap)
    {
        wobble = "#define WOBBLE\n";
    }

    if (m_sharedData.shadowQuality == SharedStateData::ShadowQuality::Classic)
    {
        wobble += "#define CLASSIC_SHADOWS\n";
    }

    const std::string FadeDistance = "#define FAR_DISTANCE " + std::to_string(CameraFarPlane) + "\n";
    const std::string FadeDistanceHQ = "#define FAR_DISTANCE " + std::to_string(CameraFarPlane *0.8f) + "\n"; //fade closer for HQ trees before they are culled

    //load materials
    std::fill(m_materialIDs.begin(), m_materialIDs.end(), -1);

    for (const auto& [name, str] : IncludeMappings)
    {
        m_resources.shaders.addInclude(name, str);
    }

    static const std::string MapSizeString = "const vec2 MapSize = vec2(" + std::to_string(MapSize.x) + ".0, " + std::to_string(MapSize.y) + ".0); ";
    m_resources.shaders.addInclude("MAP_SIZE", MapSizeString.c_str());




    //special prop materials
    if (m_resources.shaders.loadFromString(ShaderID::Lava,
        cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::Unlit), LavaFragV2, "#define TEXTURED\n"))
    {
        m_resources.shaders.mapStringID("lava", ShaderID::Lava);
        auto* shader = &m_resources.shaders.get(ShaderID::Lava);
        m_windBuffer.addShader(*shader);
    }

    if (m_resources.shaders.loadFromString(ShaderID::LavaFall,
        cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::Unlit), LavaFallFrag, "#define TEXTURED\n"))
    {
        m_resources.shaders.mapStringID("lavafall", ShaderID::LavaFall);
        auto* shader = &m_resources.shaders.get(ShaderID::LavaFall);
        m_windBuffer.addShader(*shader);
    }

    if (m_resources.shaders.loadFromString(ShaderID::Hologram,
        cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::Unlit), HoloFrag, "#define TEXTURED\n#define RIMMING\n"))
    {
        m_resources.shaders.mapStringID("holo_shader", ShaderID::Hologram);
        auto* shader = &m_resources.shaders.get(ShaderID::Hologram);
        m_windBuffer.addShader(*shader);
    }

    if (m_resources.shaders.loadFromString(ShaderID::Umbrella, CelVertexShader, UmbrellaFrag,
        "#define DITHERED\n#define INSTANCING\n#define VERTEX_COLOURED\n#define TERRAIN_CLIP\n" + wobble))
    {
        m_resources.shaders.mapStringID("umbrella", ShaderID::Umbrella);
        auto* shader = &m_resources.shaders.get(ShaderID::Umbrella);
        m_resolutionBuffer.addShader(*shader);
    }




    //cel shaded material
    m_resources.shaders.loadFromString(ShaderID::Cel, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define DITHERED\n#define TERRAIN_CLIP\n#define BALL_COLOUR\n" + wobble);
    auto* shader = &m_resources.shaders.get(ShaderID::Cel);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Cel] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::Cel]).setProperty("u_ballColour", cro::Colour::White);

    m_resources.shaders.loadFromString(ShaderID::CelSkinned, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define DITHERED\n#define SKINNED\n#define TERRAIN_CLIP\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelSkinned);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelSkinned] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Hole, HoleVertex, HoleFragment);
    shader = &m_resources.shaders.get(ShaderID::Hole);
    m_materialIDs[MaterialID::Hole] = m_resources.materials.add(*shader);
    
    m_resources.shaders.loadFromString(ShaderID::Flag, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define SKINNED\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Flag);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Flag] = m_resources.materials.add(*shader);

    //we always create this because it's also used on clubs etc even at night
    m_resources.shaders.loadFromString(ShaderID::Ball, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define BALL_COLOUR\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Ball);
    m_scaleBuffer.addShader(*shader); //hmm I forget why balls need these UBOs
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Ball] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::Ball]).setProperty("u_ballColour", cro::Colour::White);

    m_resources.shaders.loadFromString(ShaderID::BallBumped, cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::VertexLit), ShopFragment, 
        "#define NO_SUN_COLOUR\n#define VERTEX_COLOUR\n#define BALL_COLOUR\n#define BUMP\n#define TEXTURED\n");
    shader = &m_resources.shaders.get(ShaderID::BallBumped);
    //m_scaleBuffer.addShader(*shader);
    //m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::BallBumped] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::BallBumped]).setProperty("u_ballColour", cro::Colour::White);


    m_resources.shaders.loadFromString(ShaderID::BallSkinned, CelVertexShader, CelFragmentShader, "#define SKINNED\n#define VERTEX_COLOURED\n#define BALL_COLOUR\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::BallSkinned);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::BallSkinned] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::BallSkinned]).setProperty("u_ballColour", cro::Colour::White);
    m_resources.materials.get(m_materialIDs[MaterialID::BallSkinned]).doubleSided = true;

    if (m_sharedData.nightTime)
    {
        m_resources.shaders.loadFromString(ShaderID::BallNight, GlowVertex, GlowFragment);
        shader = &m_resources.shaders.get(ShaderID::BallNight);
        m_materialIDs[MaterialID::BallNight] = m_resources.materials.add(*shader);
        m_resources.materials.get(m_materialIDs[MaterialID::BallNight]).setProperty("u_ballColour", cro::Colour::White);

        m_resources.shaders.loadFromString(ShaderID::BallNightSkinned, GlowVertex, GlowFragment, "#define SKINNED\n");
        shader = &m_resources.shaders.get(ShaderID::BallNightSkinned);
        m_materialIDs[MaterialID::BallNightSkinned] = m_resources.materials.add(*shader);
        m_resources.materials.get(m_materialIDs[MaterialID::BallNightSkinned]).setProperty("u_ballColour", cro::Colour::White);
        m_resources.materials.get(m_materialIDs[MaterialID::BallNightSkinned]).doubleSided = true;

        m_resources.shaders.loadFromString(ShaderID::Emissive, CelVertexShader, EmissiveFragment, "#define VERTEX_COLOURED\n" + wobble);
        shader = &m_resources.shaders.get(ShaderID::Emissive);
        m_materialIDs[MaterialID::Emissive] = m_resources.materials.add(*shader);
    }



    m_resources.shaders.loadFromString(ShaderID::Trophy, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define REFLECTIONS\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Trophy);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Trophy] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::Trophy]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap.getGLHandle()));

    auto& noiseTex = m_resources.textures.get("assets/golf/images/wind.png");
    noiseTex.setRepeated(true);
    noiseTex.setSmooth(true);
    m_resources.shaders.loadFromString(ShaderID::CelTextured, CelVertexShader, CelFragmentShader, "#define WIND_WARP\n#define TEXTURED\n#define DITHERED\n#define SUBRECT\n#define TERRAIN_CLIP\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTextured);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTextured] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]).setProperty("u_noiseTexture", noiseTex);


    //this is only used on prop models, in case they are emissive or reflective
    m_resources.shaders.loadFromString(ShaderID::CelTexturedMasked, CelVertexShader, CelFragmentShader, "#define WIND_WARP\n#define TEXTURED\n#define DITHERED\n#define SUBRECT\n#define MASK_MAP\n#define TERRAIN_CLIP\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTexturedMasked);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTexturedMasked] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedMasked]).setProperty("u_noiseTexture", noiseTex);
    m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedMasked]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap));

    //disable wind/vert animation on models with no colour channel
    //saves on probably significant amount of vertex processing...
    m_resources.shaders.loadFromString(ShaderID::CelTexturedNoWind, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define DITHERED\n#define SUBRECT\n#define TERRAIN_CLIP\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTexturedNoWind);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTexturedNoWind] = m_resources.materials.add(*shader);


    //this is only used on prop models, in case they are emissive or reflective
    /*std::string rxShadow;
    if (m_sharedData.hqShadows)
    {
        rxShadow = "#define RX_SHADOWS\n";
    }*/
    m_resources.shaders.loadFromString(ShaderID::CelTexturedMaskedNoWind, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define DITHERED\n#define SUBRECT\n#define MASK_MAP\n#define TERRAIN_CLIP\n" + wobble/* + rxShadow*/);
    shader = &m_resources.shaders.get(ShaderID::CelTexturedMaskedNoWind);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTexturedMaskedNoWind] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedMaskedNoWind]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap));


    //custom shadow map so shadows move with wind too...
    m_resources.shaders.loadFromString(ShaderID::ShadowMap, ShadowVertex, ShadowFragment, "#define DITHERED\n#define WIND_WARP\n#define ALPHA_CLIP\n");
    shader = &m_resources.shaders.get(ShaderID::ShadowMap);
    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::ShadowMap] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::ShadowMap]).setProperty("u_noiseTexture", noiseTex);

    m_resources.shaders.loadFromString(ShaderID::BillboardShadow, BillboardVertexShader, ShadowFragment, "#define DITHERED\n#define SHADOW_MAPPING\n#define ALPHA_CLIP\n" + FadeDistance);
    shader = &m_resources.shaders.get(ShaderID::BillboardShadow);
    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::Leaderboard, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define DITHERED\n#define SUBRECT\n#define TERRAIN_CLIP\n");
    shader = &m_resources.shaders.get(ShaderID::Leaderboard);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Leaderboard] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::CelTexturedSkinned, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define DITHERED\n#define SKINNED\n#define SUBRECT\n#define TERRAIN_CLIP\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTexturedSkinned);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTexturedSkinned] = m_resources.materials.add(*shader);


    //again, on props only
    m_resources.shaders.loadFromString(ShaderID::CelTexturedSkinnedMasked, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define DITHERED\n#define SKINNED\n#define SUBRECT\n#define MASK_MAP\n#define TERRAIN_CLIP\n" + wobble /*+ rxShadow*/);
    shader = &m_resources.shaders.get(ShaderID::CelTexturedSkinnedMasked);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTexturedSkinnedMasked] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedSkinnedMasked]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap));


    //sigh we need a special case for ball washer so that it doesn't fade...
    m_resources.shaders.loadFromString(ShaderID::BallWasher, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define SKINNED\n#define SUBRECT\n#define MASK_MAP\n#define TERRAIN_CLIP\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::BallWasher);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::BallWasher] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::BallWasher]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap));


    m_resources.shaders.loadFromString(ShaderID::Glass, cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::VertexLit), GlassFragment);
    shader = &m_resources.shaders.get(ShaderID::Glass);
    m_materialIDs[MaterialID::Glass] = m_resources.materials.add(*shader);
    auto& glassMat = m_resources.materials.get(m_materialIDs[MaterialID::Glass]);
    glassMat.setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap));
    glassMat.doubleSided = true;
    glassMat.blendMode = cro::Material::BlendMode::Alpha;


    m_resources.shaders.loadFromString(ShaderID::HairGlass,
        cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::VertexLit), GlassFragment, "#define USER_COLOUR\n");
    shader = &m_resources.shaders.get(ShaderID::HairGlass);
    m_materialIDs[MaterialID::HairGlass] = m_resources.materials.add(*shader);
    auto& glassHairMat = m_resources.materials.get(m_materialIDs[MaterialID::HairGlass]);
    glassHairMat.setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap));
    glassHairMat.doubleSided = true;
    glassHairMat.blendMode = cro::Material::BlendMode::Alpha;


    m_resources.shaders.loadFromString(ShaderID::Wake, cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::Unlit), WakeFragment, "#define TEXTURED\n");
    shader = &m_resources.shaders.get(ShaderID::Wake);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Wake] = m_resources.materials.add(*shader);
    auto& wakeMat = m_resources.materials.get(m_materialIDs[MaterialID::Wake]);
    wakeMat.setProperty("u_texture", m_resources.textures.get("assets/golf/images/wake.png"));
    wakeMat.setProperty("u_speed", 0.f); //default to zero so if the prop has no path the wake isn't visible
    wakeMat.doubleSided = true;
    wakeMat.blendMode = cro::Material::BlendMode::Alpha;



    m_resources.shaders.loadFromString(ShaderID::Player, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define SKINNED\n#define MASK_MAP\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Player);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Player] = m_resources.materials.add(*shader);

    cro::Image defaultMask;
    defaultMask.create(2, 2, cro::Colour::Black);
    m_defaultMaskMap.loadFromImage(defaultMask);
    m_resources.materials.get(m_materialIDs[MaterialID::Player]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap.getGLHandle()));
    m_resources.materials.get(m_materialIDs[MaterialID::Player]).setProperty("u_maskMap", m_defaultMaskMap);

    
    //hair
    m_resources.shaders.loadFromString(ShaderID::Hair, CelVertexShader, CelFragmentShader, "#define USER_COLOUR\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Hair);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Hair] = m_resources.materials.add(*shader);


    m_resources.shaders.loadFromString(ShaderID::HairReflect, CelVertexShader, CelFragmentShader, "#define USER_COLOUR\n#define REFLECTIONS\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::HairReflect);
    m_materialIDs[MaterialID::HairReflect] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::HairReflect]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap.getGLHandle()));




    std::string targetDefines = (m_sharedData.scoreType == ScoreType::MultiTarget || Social::getMonth() == 2) ? "#define MULTI_TARGET\n" : "";// "#define SHOW_CASCADES\n";

    m_resources.shaders.loadFromString(ShaderID::Course, CelVertexShader, CelFragmentShader, "#define TERRAIN\n#define COMP_SHADE\n#define COLOUR_LEVELS 5.0\n#define TEXTURED\n#define RX_SHADOWS\n#define TERRAIN_CLIP\n" + wobble + targetDefines);
    shader = &m_resources.shaders.get(ShaderID::Course);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Course] = m_resources.materials.add(*shader);
    if (!targetDefines.empty())
    {
        m_targetShader.shaderID = shader->getGLHandle();
        m_targetShader.vpUniform = shader->getUniformID("u_targetViewProjectionMatrix");
        m_targetShader.update();
    }
    auto& shaleTex = m_resources.textures.get("assets/golf/images/shale.png", true);
    shaleTex.setRepeated(true);
    m_resources.materials.get(m_materialIDs[MaterialID::Course]).setProperty("u_angleTex", shaleTex);
    m_resources.materials.get(m_materialIDs[MaterialID::Course]).addCustomSetting(GL_CLIP_DISTANCE1);

    m_resources.shaders.loadFromString(ShaderID::MinimapModel, CelVertexShader, CelFragmentShader, "#define TERRAIN\n#define COMP_SHADE\n#define COLOUR_LEVELS 5.0\n#define TEXTURED\n" + targetDefines);
    shader = &m_resources.shaders.get(ShaderID::MinimapModel);
    m_materialIDs[MaterialID::Minimap] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::Minimap]).setProperty("u_angleTex", shaleTex);


    //m_ballShadows.shaders[0].shader = shader->getGLHandle();
    //m_ballShadows.shaders[0].uniform = shader->getUniformID("u_ballPosition");

    m_resources.shaders.loadFromString(ShaderID::CourseGreen, CelVertexShader, CelFragmentShader, "#define HOLE_HEIGHT\n#define TERRAIN\n#define COMP_SHADE\n#define COLOUR_LEVELS 5.0\n#define TEXTURED\n#define RX_SHADOWS\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CourseGreen);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);

    m_gridShaders[0].shaderID = shader->getGLHandle();
    m_gridShaders[0].transparency = shader->getUniformID("u_transparency");
    m_gridShaders[0].holeHeight = shader->getUniformID("u_holePosition");

    //m_ballShadows.shaders[1].shader = shader->getGLHandle();
    //m_ballShadows.shaders[1].uniform = shader->getUniformID("u_ballPosition");

    m_resources.shaders.loadFromString(ShaderID::CourseGrid, CelVertexShader, CelFragmentShader, "#define HOLE_HEIGHT\n#define TEXTURED\n#define RX_SHADOWS\n#define CONTOUR\n" + wobble + targetDefines);
    shader = &m_resources.shaders.get(ShaderID::CourseGrid);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    if (!targetDefines.empty())
    {
        m_targetShader.shaderID = shader->getGLHandle();
        m_targetShader.vpUniform = shader->getUniformID("u_targetViewProjectionMatrix");
        m_targetShader.update();
    }

    m_gridShaders[1].shaderID = shader->getGLHandle();
    m_gridShaders[1].transparency = shader->getUniformID("u_transparency");
    m_gridShaders[1].holeHeight = shader->getUniformID("u_holePosition");

    if (m_sharedData.nightTime)
    {
        m_resources.shaders.loadFromString(ShaderID::Billboard, BillboardVertexShader, BillboardFragmentShader, "#define USE_MRT\n" + FadeDistance);
    }
    else
    {
        m_resources.shaders.loadFromString(ShaderID::Billboard, BillboardVertexShader, BillboardFragmentShader, FadeDistance);
    }
    shader = &m_resources.shaders.get(ShaderID::Billboard);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Billboard] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::Billboard]).addCustomSetting(GL_CLIP_DISTANCE1);


    //shaders used by terrain
    m_resources.shaders.loadFromString(ShaderID::CelTexturedInstanced, CelVertexShader, CelFragmentShader, "#define WIND_WARP\n#define TEXTURED\n#define DITHERED\n#define INSTANCING\n#define TERRAIN_CLIP\n");
    shader = &m_resources.shaders.get(ShaderID::CelTexturedInstanced);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::ShadowMapInstanced, ShadowVertex, ShadowFragment, "#define DITHERED\n#define WIND_WARP\n#define ALPHA_CLIP\n#define INSTANCING\n");
    shader = &m_resources.shaders.get(ShaderID::ShadowMapInstanced);
    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::Crowd, CelVertexShader, CelFragmentShader, "#define DITHERED\n#define INSTANCING\n#define VATS\n#define TEXTURED\n#define TERRAIN_CLIP\n");
    shader = &m_resources.shaders.get(ShaderID::Crowd);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::CrowdShadow, ShadowVertex, ShadowFragment, "#define DITHERED\n#define INSTANCING\n#define VATS\n");
    m_resolutionBuffer.addShader(m_resources.shaders.get(ShaderID::CrowdShadow));

    m_resources.shaders.loadFromString(ShaderID::CrowdArray, CelVertexShader, CelFragmentShader, "#define DITHERED\n#define INSTANCING\n#define VATS\n#define TEXTURED\n#define ARRAY_MAPPING\n#define TERRAIN_CLIP\n");
    shader = &m_resources.shaders.get(ShaderID::CrowdArray);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::CrowdShadowArray, ShadowVertex, ShadowFragment, "#define DITHERED\n#define INSTANCING\n#define VATS\n#define ARRAY_MAPPING\n");
    m_resolutionBuffer.addShader(m_resources.shaders.get(ShaderID::CrowdShadowArray));


    //HQ tree shaders - wasted if the whole game is LQ, but we want to be able to swap mid-game...
    std::string mrt;
    if (m_sharedData.nightTime)
    {
        mrt = "#define USE_MRT\n";
    }

    m_resources.shaders.loadFromString(ShaderID::TreesetBranch, BranchVertex, BranchFragment, "#define ALPHA_CLIP\n#define INSTANCING\n" + wobble + mrt + FadeDistanceHQ);
    shader = &m_resources.shaders.get(ShaderID::TreesetBranch);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::TreesetLeaf, BushVertex, /*BushGeom,*/ BushFragment, "#define POINTS\n#define INSTANCING\n#define HQ\n" + wobble + mrt + FadeDistanceHQ);
    shader = &m_resources.shaders.get(ShaderID::TreesetLeaf);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);


    m_resources.shaders.loadFromString(ShaderID::TreesetShadow, ShadowVertex, ShadowFragment, "#define INSTANCING\n#define TREE_WARP\n#define ALPHA_CLIP\n" + wobble + FadeDistanceHQ);
    shader = &m_resources.shaders.get(ShaderID::TreesetShadow);
    m_windBuffer.addShader(*shader);

    std::string alphaClip;
    if (m_sharedData.shadowQuality)
    {
        alphaClip = "#define ALPHA_CLIP\n";
    }
    m_resources.shaders.loadFromString(ShaderID::TreesetLeafShadow, ShadowVertex, /*ShadowGeom,*/ ShadowFragment, "#define POINTS\n#define INSTANCING\n#define LEAF_SIZE\n" + alphaClip + wobble + FadeDistanceHQ);
    shader = &m_resources.shaders.get(ShaderID::TreesetLeafShadow);
    m_windBuffer.addShader(*shader);



    //scanline transition
    m_resources.shaders.loadFromString(ShaderID::Transition, MinimapVertex, ScanlineTransition);

    //noise effect
    m_resources.shaders.loadFromString(ShaderID::Noise, MinimapVertex, NoiseFragment);
    shader = &m_resources.shaders.get(ShaderID::Noise);
    m_scaleBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);

    m_postProcesses[PostID::Noise].shader = shader;




    //light blur (also dof blur)
    //if (m_sharedData.nightTime)
    {
        m_resources.shaders.loadFromString(ShaderID::Blur, BlurVert, BlurFrag);
        shader = &m_resources.shaders.get(ShaderID::Blur);
        m_resolutionBuffer.addShader(*shader);
    }


    //fog
    std::string defines;
    if (Social::isGreyscale())
    {
        defines = "#define DESAT 1.0\n";
    }
    if (m_sharedData.nightTime)
    {
        //defines += "#define LIGHT_COLOUR\n#define USE_AA\n";
        defines += "#define LIGHT_COLOUR\n";
    }
    m_resources.shaders.loadFromString(ShaderID::Composite, CompositeVert, CompositeFrag, "#define ZFAR 320.0\n" + defines);
    shader = &m_resources.shaders.get(ShaderID::Composite);
    m_postProcesses[PostID::Composite].shader = shader;
    //depth uniform is set after creating the UI once we know the render texture is created

    defines += "#define DOF\n";
    m_resources.shaders.loadFromString(ShaderID::CompositeDOF, CompositeVert, CompositeFrag, "#define ZFAR 320.0\n" + defines);
    shader = &m_resources.shaders.get(ShaderID::CompositeDOF);
    m_postProcesses[PostID::CompositeDOF].shader = shader;


    //wireframe
    m_resources.shaders.loadFromString(ShaderID::Wireframe, WireframeVertex, WireframeFragment, "#define USE_MRT\n");
    shader = &m_resources.shaders.get(ShaderID::Wireframe);
    m_materialIDs[MaterialID::WireFrame] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]).blendMode = cro::Material::BlendMode::Alpha;
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::WireframeCulled, WireframeVertex, WireframeFragment, "#define CULLED\n#define USE_MRT\n");
    shader = &m_resources.shaders.get(ShaderID::WireframeCulled);
    m_materialIDs[MaterialID::WireFrameCulled] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::WireFrameCulled]).blendMode = cro::Material::BlendMode::Alpha;
    m_resolutionBuffer.addShader(*shader);


    m_resources.shaders.loadFromString(ShaderID::WireframeCulledPoint, WireframeVertex, WireframeFragment, "#define CULLED\n#define USE_MRT\n#define POINT_RADIUS\n");
    shader = &m_resources.shaders.get(ShaderID::WireframeCulledPoint);
    m_materialIDs[MaterialID::WireFrameCulledPoint] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::WireFrameCulledPoint]).blendMode = cro::Material::BlendMode::Alpha;
    m_resolutionBuffer.addShader(*shader);
    m_scaleBuffer.addShader(*shader);


    m_resources.shaders.loadFromString(ShaderID::BallTrail, WireframeVertex, WireframeFragment, "#define HUE\n#define USE_MRT\n");
    shader = &m_resources.shaders.get(ShaderID::BallTrail);
    m_materialIDs[MaterialID::BallTrail] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::BallTrail]).blendMode = cro::Material::BlendMode::Additive;
    m_resources.materials.get(m_materialIDs[MaterialID::BallTrail]).setProperty("u_colourRotation", m_sharedData.beaconColour);
    m_resolutionBuffer.addShader(*shader);


    //minimap - green overhead
    if (m_sharedData.weatherType == WeatherType::Rain
        || m_sharedData.weatherType == WeatherType::Showers)
    {
        defines += "#define RAIN\n";
    }
    m_resources.shaders.loadFromString(ShaderID::Minimap, MinimapVertex, MinimapFragment, defines);
    shader = &m_resources.shaders.get(ShaderID::Minimap);
    m_scaleBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);

    //minimap - course view
    m_resources.shaders.loadFromString(ShaderID::MinimapView, MinimapViewVertex, MinimapViewFragment);
    shader = &m_resources.shaders.get(ShaderID::MinimapView);
    m_minimapZoom.shaderID = shader->getGLHandle();
    m_minimapZoom.matrixUniformID = shader->getUniformID("u_coordMatrix");

    //water - this is if we ever get the rain splash pattern working
    //std::string waterDefines;
    //if (m_sharedData.weatherType == WeatherType::Rain
    //    || m_sharedData.weatherType == WeatherType::Showers)
    //{
    //    waterDefines = "#define RAIN\n";
    //}

    /*static const std::string DepthConsts = "\nconst float ColCount = " + std::to_string(m_depthMap.getGridCount().x) 
        + ".0;\nconst float MetresPerTexture = "+ std::to_string(m_depthMap.getMetresPerTile())
        + ".0;\nconst float MaxTiles = "
        + std::to_string(m_depthMap.getTileCount() - 1) + ".0;\n";
    m_resources.shaders.addInclude("DEPTH_CONSTS", DepthConsts.c_str());*/

    m_resources.shaders.loadFromString(ShaderID::Water, WaterVertex, WaterFragment, "#define NO_DEPTH\n#define USE_MRT\n"/* + waterDefines*/);
    shader = &m_resources.shaders.get(ShaderID::Water);
    m_scaleBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Water] = m_resources.materials.add(*shader);
    //if (!waterDefines.empty())
    //{
    //    auto& waterTex = m_resources.textures.get("assets/golf/images/rain_water.png");
    //    m_resources.materials.get(m_materialIDs[MaterialID::Water]).setProperty("u_rainTexture", waterTex);
    //    m_resources.materials.get(m_materialIDs[MaterialID::Water]).setProperty("u_rainAmount", 1.f);
    //}

 

    //this version is affected by the sunlight colour of the scene
    m_resources.shaders.loadFromString(ShaderID::HorizonSun, HorizonVert, HorizonFrag, "#define SUNLIGHT\n");
    shader = &m_resources.shaders.get(ShaderID::HorizonSun);
    m_materialIDs[MaterialID::HorizonSun] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Horizon, HorizonVert, HorizonFrag);
    shader = &m_resources.shaders.get(ShaderID::Horizon);
    m_materialIDs[MaterialID::Horizon] = m_resources.materials.add(*shader);

    //mmmm... bacon
    m_resources.shaders.loadFromString(ShaderID::Beacon, BeaconVertex, BeaconFragment, "#define TEXTURED\n#define USE_MRT\n");
    m_materialIDs[MaterialID::Beacon] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Beacon));

    const std::string intensity = m_sharedData.nightTime ? "#define INTENSITY 0.6\n" : "#define INTENSITY 0.3\n";
    m_resources.shaders.loadFromString(ShaderID::Target, cro::ModelRenderer::getDefaultVertexShader(cro::ModelRenderer::VertexShaderID::Unlit),
        BeaconFragment, intensity + "#define USE_MRT\n#define VERTEX_COLOUR\n");
    m_materialIDs[MaterialID::Target] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Target));
    m_resources.materials.get(m_materialIDs[MaterialID::Target]).blendMode = cro::Material::BlendMode::Additive;
    m_resources.materials.get(m_materialIDs[MaterialID::Target]).doubleSided = true;
}

void GolfState::loadSprites()
{
    //UI stuffs
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_resources.textures);
    m_sprites[SpriteID::PowerBar] = spriteSheet.getSprite("power_bar_wide");
    m_sprites[SpriteID::PowerBar10] = spriteSheet.getSprite("power_bar_wide_10");
    m_sprites[SpriteID::PowerBarInner] = spriteSheet.getSprite("power_bar_inner_wide");
    m_sprites[SpriteID::PowerBarInnerHC] = spriteSheet.getSprite("power_bar_inner_wide_hc");
    m_sprites[SpriteID::HookBar] = spriteSheet.getSprite("hook_bar");

    m_sprites[SpriteID::PowerBarDouble] = spriteSheet.getSprite("power_bar_double");
    m_sprites[SpriteID::PowerBarDouble10] = spriteSheet.getSprite("power_bar_double_10");
    m_sprites[SpriteID::PowerBarDoubleInner] = spriteSheet.getSprite("power_bar_inner_double");
    m_sprites[SpriteID::PowerBarDoubleInnerHC] = spriteSheet.getSprite("power_bar_inner_double_hc");
    m_sprites[SpriteID::HookBarDouble] = spriteSheet.getSprite("hook_bar_double");

    //most of these are loaded once or not at all so not really sure why
    //we keep these hanging around in an array ike this
    m_sprites[SpriteID::SlopeStrength] = spriteSheet.getSprite("slope_indicator");
    m_sprites[SpriteID::BallSpeed] = spriteSheet.getSprite("ball_speed");
    m_sprites[SpriteID::MapFlag] = spriteSheet.getSprite("flag03");
    m_sprites[SpriteID::MapTarget] = spriteSheet.getSprite("multitarget");
    m_sprites[SpriteID::MiniFlag] = spriteSheet.getSprite("putt_flag");
    m_sprites[SpriteID::MiniMapFlag] = spriteSheet.getSprite("map_flag");
    m_sprites[SpriteID::MiniFlagLarge] = spriteSheet.getSprite("putt_flag_large");
    m_sprites[SpriteID::WindIndicator] = spriteSheet.getSprite("wind_dir");
    m_sprites[SpriteID::WindSpeed] = spriteSheet.getSprite("wind_speed");
    m_sprites[SpriteID::WindSpeedBg] = spriteSheet.getSprite("wind_text_bg");
    m_sprites[SpriteID::Thinking] = spriteSheet.getSprite("thinking");
    m_sprites[SpriteID::Sleeping] = spriteSheet.getSprite("sleeping");
    m_sprites[SpriteID::Typing] = spriteSheet.getSprite("typing");
    m_sprites[SpriteID::Freecam] = spriteSheet.getSprite("camera_icon");
    m_sprites[SpriteID::MessageBoard] = spriteSheet.getSprite("message_board");
    m_sprites[SpriteID::Bunker] = spriteSheet.getSprite("bunker");
    m_sprites[SpriteID::Foul] = spriteSheet.getSprite("foul");
    m_sprites[SpriteID::SpinBg] = spriteSheet.getSprite("spin_bg");
    m_sprites[SpriteID::SpinFg] = spriteSheet.getSprite("spin_fg");
    m_sprites[SpriteID::BirdieLeft] = spriteSheet.getSprite("birdie_left");
    m_sprites[SpriteID::BirdieRight] = spriteSheet.getSprite("birdie_right");
    m_sprites[SpriteID::EagleLeft] = spriteSheet.getSprite("eagle_left");
    m_sprites[SpriteID::EagleRight] = spriteSheet.getSprite("eagle_right");
    m_sprites[SpriteID::AlbatrossLeft] = spriteSheet.getSprite("albatross_left");
    m_sprites[SpriteID::AlbatrossRight] = spriteSheet.getSprite("albatross_right");
    m_sprites[SpriteID::Hio] = spriteSheet.getSprite("hio");

    spriteSheet.loadFromFile("assets/golf/sprites/bounce.spt", m_resources.textures);
    m_sprites[SpriteID::BounceAnim] = spriteSheet.getSprite("bounce");


    spriteSheet.loadFromFile("assets/golf/sprites/emotes.spt", m_resources.textures);
    m_sprites[SpriteID::EmoteHappy] = spriteSheet.getSprite("happy_small");
    m_sprites[SpriteID::EmoteGrumpy] = spriteSheet.getSprite("grumpy_small");
    m_sprites[SpriteID::EmoteLaugh] = spriteSheet.getSprite("laughing_small");
    m_sprites[SpriteID::EmoteSad] = spriteSheet.getSprite("sad_small");
    m_sprites[SpriteID::EmotePulse] = spriteSheet.getSprite("pulse");
}

void GolfState::loadModels()
{
    /*
    This deals with loading the models for avatars/hair/balls
    props and course models are parsed in loadMap();
    */


    //model definitions
    for (auto& md : m_modelDefs)
    {
        md = std::make_unique<cro::ModelDefinition>(m_resources);
    }
    m_modelDefs[ModelID::BallShadow]->loadFromFile("assets/golf/models/ball_shadow.cmt");
    m_modelDefs[ModelID::BullsEye]->loadFromFile("assets/golf/models/target.cmt"); //TODO we can only load this if challenge month or game mode requires
    m_modelDefs[ModelID::PlayerFallBack]->loadFromFile("assets/golf/models/avatars/default.cmt");

    //ball models - the menu should never have let us get this far if it found no ball files
    for (const auto& info : m_sharedData.ballInfo)
    {
        std::unique_ptr<cro::ModelDefinition> def = std::make_unique<cro::ModelDefinition>(m_resources);
        m_ballModels.insert(std::make_pair(info.uid, std::move(def)));
    }


    //load audio from avatar info
    std::unordered_map<std::uint32_t, std::string> audioPaths;
    //list all available audio and put into map
    const auto processPath =
        [&](const std::string& path)
        {
            auto audioFiles = cro::FileSystem::listFiles(path);
            for (const auto& file : audioFiles)
            {
                if (cro::FileSystem::getFileExtension(file) == ".xas")
                {
                    auto fullPath = path + file;
                    cro::AudioScape as;
                    as.loadFromFile(fullPath, m_resources.audio);

                    const auto uid = as.getUID();
                    if (uid != 0
                        && audioPaths.count(uid) == 0)
                    {
                        audioPaths.insert(std::make_pair(uid, fullPath));
                    }
                }
            }
        };
    std::string baseAudioPath = "assets/golf/sound/avatars/";
    processPath(baseAudioPath);
    baseAudioPath = Content::getUserContentPath(Content::UserContent::Voice);
    const auto voiceDirs = cro::FileSystem::listDirectories(baseAudioPath);
    for (const auto& dir : voiceDirs)
    {
        processPath(baseAudioPath + dir + "/");
    }

    auto defaultAudio = m_gameScene.getDirector<GolfSoundDirector>()->addAudioScape("assets/golf/sound/avatars/default.xas", m_resources.audio);


    //copy into active player slots
    const auto indexFromSkinID = [&](std::uint32_t skinID, bool& isRandom)->std::size_t
        {
            auto result = std::find_if(m_sharedData.avatarInfo.begin(), m_sharedData.avatarInfo.end(),
                [skinID](const SharedStateData::AvatarInfo& ai)
                {
                    return skinID == ai.uid;
                });

            if (result != m_sharedData.avatarInfo.end())
            {
                isRandom = false;
                return std::distance(m_sharedData.avatarInfo.begin(), result);
            }
            isRandom = true;
            return cro::Util::Random::value(0u, m_sharedData.avatarInfo.size() - 1);
        };

    const auto indexFromHairID = [&](std::uint32_t hairID)
        {
            if (auto hair = std::find_if(m_sharedData.hairInfo.begin(), m_sharedData.hairInfo.end(),
                [hairID](const SharedStateData::HairInfo& h) {return h.uid == hairID; });
                hair != m_sharedData.hairInfo.end())
            {
                return static_cast<std::int32_t>(std::distance(m_sharedData.hairInfo.begin(), hair));
            }

            return 0;// static_cast<std::int32_t>(cro::Util::Random::value(0u, m_sharedData.hairInfo.size() - 1));
        };

    cro::ModelDefinition animations(m_resources);
    animations.loadFromFile("assets/golf/models/avatars/animations.cmt");

    cro::ModelDefinition defaultAnims(m_resources);
    defaultAnims.loadFromFile("assets/golf/models/avatars/player_zero.cmt");

    //player avatars
    cro::ModelDefinition md(m_resources);
    for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
    {
        for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
        {
            auto skinID = m_sharedData.connectionData[i].playerData[j].skinID;
            bool isRandom = false;
            const auto avatarIndex = indexFromSkinID(skinID, isRandom);

            //if this returned a random index because the skinID wasn't found, correct the skinID
            skinID = m_sharedData.avatarInfo[avatarIndex].uid;

            const auto voiceID = m_sharedData.connectionData[i].playerData[j].voiceID;
            std::size_t playerVoiceIndex = defaultAudio;
            if (audioPaths.count(voiceID) != 0)
            {
                playerVoiceIndex = m_gameScene.getDirector<GolfSoundDirector>()->addAudioScape(audioPaths.at(voiceID), m_resources.audio);

#ifdef USE_GNS
                //track stats if this is a workshop item
                if (i == m_sharedData.localConnectionData.connectionID)
                {
                    auto temp = cro::FileSystem::getFilePath(audioPaths.at(voiceID));
                    temp.pop_back(); //remove trailing '/'

                    if (temp.back() == 'w')
                    {
                        std::uint64_t workshopID = 0;
                        if (auto res = temp.find_last_of('/'); res != std::string::npos)
                        {
                            try
                            {
                                const auto d = temp.substr(res + 1, temp.length() - 1);
                                workshopID = std::stoull(d);
                            }
                            catch (...)
                            {
                                LogW << "could not get workshop ID for " << temp << std::endl;
                            }
                        }

                        if (workshopID != 0)
                        {
                            m_modelStats.push_back(workshopID);
                        }
                    }
                }
#endif
            }
            else
            {
                if (!isRandom)
                {
                    playerVoiceIndex = m_gameScene.getDirector<GolfSoundDirector>()->addAudioScape(m_sharedData.avatarInfo[avatarIndex].audioscape, m_resources.audio);
                }
            }

            const float pitch = 1.f + (static_cast<float>(m_sharedData.connectionData[i].playerData[j].voicePitch) / VoicePitchDivisor);
            m_gameScene.getDirector<GolfSoundDirector>()->setPlayerIndex(i, j, static_cast<std::int32_t>(playerVoiceIndex), pitch);
            m_avatars[i][j].flipped = m_sharedData.connectionData[i].playerData[j].flipped;

            //player avatar model
            //TODO we might want error checking here, but the model files
            //should have been validated by the menu state.
            if (md.loadFromFile(m_sharedData.avatarInfo[avatarIndex].modelPath))
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>();
                entity.addComponent<cro::Callback>().setUserData<PlayerCallbackData>();
                entity.getComponent<cro::Callback>().function =
                    [&](cro::Entity e, float dt)
                    {
                        auto& [direction, scale] = e.getComponent<cro::Callback>().getUserData<PlayerCallbackData>();
                        const auto xScale = e.getComponent<cro::Transform>().getScale().x; //might be flipped

                        if (direction == 0)
                        {
                            scale = std::min(1.f, scale + (dt * 3.f));
                            if (scale == 1)
                            {
                                direction = 1;
                                e.getComponent<cro::Callback>().active = false;
                            }
                        }
                        else
                        {
                            scale = std::max(0.f, scale - (dt * 3.f));

                            if (scale == 0)
                            {
                                direction = 0;
                                e.getComponent<cro::Callback>().active = false;
                                e.getComponent<cro::Model>().setHidden(true);

                                //seems counter-intuitive to search the avatar here
                                //but we have a, uh.. 'handy' handle (to the hands)
                                if (m_activeAvatar->hands)
                                {
                                    //we have to free this up else the model might
                                    //become attached to two avatars...
                                    m_activeAvatar->hands->setModel({});
                                }
                            }
                        }
                        auto yScale = cro::Util::Easing::easeOutBack(scale);
                        e.getComponent<cro::Transform>().setScale(glm::vec3(xScale, yScale, yScale));
                    };
                
                auto material = m_resources.materials.get(m_materialIDs[MaterialID::Player]);
                if (isRandom)
                {
                    //to make sure there's a consistent experience when a model is unavailable
                    //eg we're not subbed to a workshop item, load a default model.
                    m_modelDefs[ModelID::PlayerFallBack]->createModel(entity);
                    applyMaterialData(*m_modelDefs[ModelID::PlayerFallBack], material);
                    entity.getComponent<cro::Model>().setMaterial(0, material);
                }
                else
                {
                    md.createModel(entity);
                    applyMaterialData(md, material); //apply mask map if it exists
                    material.setProperty("u_diffuseMap", m_sharedData.avatarTextures[i][j]);
                    entity.getComponent<cro::Model>().setMaterial(0, material);
                }

                if (m_avatars[i][j].flipped)
                {
                    entity.getComponent<cro::Transform>().setScale({ -1.f, 0.f, 0.f });
                    entity.getComponent<cro::Model>().setFacing(cro::Model::Facing::Back);
                }
                else
                {
                    entity.getComponent<cro::Transform>().setScale({ 1.f, 0.f, 0.f });
                }

                //this should assert in debug, however oor IDs will just be ignored
                //in release so this is the safest way to handle missing animations
                std::fill(m_avatars[i][j].animationIDs.begin(), m_avatars[i][j].animationIDs.end(), AnimationID::Invalid);
                if (entity.hasComponent<cro::Skeleton>())
                {
                    auto& skel = entity.getComponent<cro::Skeleton>();
                    auto defaultAttachment = -1;

                    if (defaultAnims.hasSkeleton())
                    {
                        defaultAttachment = defaultAnims.getSkeleton().getAttachmentIndex("hands");
                        for (auto s = 0u; s < defaultAnims.getSkeleton().getAnimations().size(); ++s)
                        {
                            //hmm this is a bit kludgy, but the models have different celebrate/disappoint
                            //animations and we probably want to keep these for a bit of variation. This
                            //also means we still have to embed these anims in workshop models where we
                            //could have otherwise saved some file-size by not importing anims into the model.
                            const auto& anim = defaultAnims.getSkeleton().getAnimations()[s];
                            if (anim.name != "celebrate"
                                && anim.name != "disappointment"
                                && anim.name != "impatient")
                            {
                                skel.addAnimation(defaultAnims.getSkeleton(), s);
                            }
                        }
                    }

                    if (animations.hasSkeleton())
                    {
                        for (auto s = 0u; s < animations.getSkeleton().getAnimations().size(); ++s)
                        {
                            skel.addAnimation(animations.getSkeleton(), s);
                        }
                    }

                    const auto& anims = skel.getAnimations();
                    for (auto k = 0u; k < std::min(anims.size(), static_cast<std::size_t>(AnimationID::Count)); ++k)
                    {
                        if (anims[k].name == "idle")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Idle] = k;
                            skel.play(k);
                        }
                        else if (anims[k].name == "drive")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Swing] = k;
                        }
                        else if (anims[k].name == "chip")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Chip] = k;
                        }
                        else if (anims[k].name == "chip_idle")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::ChipIdle] = k;
                        }
                        else if (anims[k].name == "to_chip")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::ToChip] = k;
                        }
                        else if (anims[k].name == "from_chip")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::FromChip] = k;
                        }
                        else if (anims[k].name == "putt")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Putt] = k;
                        }
                        else if (anims[k].name == "celebrate")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Celebrate] = k;
                        }
                        else if (anims[k].name == "disappointment")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Disappoint] = k;
                        }
                        else if (anims[k].name == "impatient")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Impatient] = k;
                        }
                    }

                    //and attachment for hair/hats
                    auto id = skel.getAttachmentIndex("head");
                    if (id > -1)
                    {
                        const auto& pd = m_sharedData.connectionData[i].playerData[j];

                        const auto createHeadEnt = [&](std::int32_t colourKey, std::int32_t transformIndexOffset)
                            {
                                auto ent = m_gameScene.createEntity();
                                ent.addComponent<cro::Transform>();
                                md.createModel(ent);

                                //set material and colour
                                material = m_resources.materials.get(m_materialIDs[MaterialID::Hair]);
                                applyMaterialData(md, material); //applies double sidedness

                                const auto hairColour = pc::Palette[pd.avatarFlags[colourKey]];
                                material.setProperty("u_hairColour", hairColour);
                                ent.getComponent<cro::Model>().setMaterial(0, material);
                                ent.getComponent<cro::Model>().setRenderFlags(~RenderFlags::CubeMap);

                                if (md.getMaterialCount() == 2)
                                {
                                    material = md.hasTag(1, "glass") ? m_resources.materials.get(m_materialIDs[MaterialID::HairGlass])
                                        : m_resources.materials.get(m_materialIDs[MaterialID::HairReflect]);
                                    applyMaterialData(md, material, 1);
                                    material.setProperty("u_hairColour", hairColour);
                                    ent.getComponent<cro::Model>().setMaterial(1, material);
                                }

                                //apply any profile specific transforms
                                const auto rot = pd.headwearOffsets[PlayerData::HeadwearOffset::HairRot + transformIndexOffset] * cro::Util::Const::PI;
                                ent.getComponent<cro::Transform>().setPosition(pd.headwearOffsets[PlayerData::HeadwearOffset::HairTx + transformIndexOffset]);
                                ent.getComponent<cro::Transform>().setRotation(cro::Transform::Z_AXIS, rot.z);
                                ent.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rot.y);
                                ent.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, rot.x);
                                ent.getComponent<cro::Transform>().setScale(pd.headwearOffsets[PlayerData::HeadwearOffset::HairScale + transformIndexOffset]);

                                if (m_avatars[i][j].flipped)
                                {
                                    ent.getComponent<cro::Model>().setFacing(cro::Model::Facing::Back);
                                }

                                ent.addComponent<cro::Callback>().active = true;
                                ent.getComponent<cro::Callback>().function =
                                    [entity](cro::Entity e, float)
                                    {
                                        e.getComponent<cro::Model>().setHidden(entity.getComponent<cro::Model>().isHidden());
                                    };

                                return ent;
                            };


                        //do this first so we duplicate a blank attachment if needs be
                        auto hatID = indexFromHairID(m_sharedData.connectionData[i].playerData[j].hatID);
                        if (hatID != 0
                            && md.loadFromFile(m_sharedData.hairInfo[hatID].modelPath))
                        {
                            auto hatEnt = createHeadEnt(pc::ColourKey::Hat, PlayerData::HeadwearOffset::HatTx);

                            auto hatAttachment = entity.getComponent<cro::Skeleton>().getAttachments()[id];
                            auto hatAtID = entity.getComponent<cro::Skeleton>().addAttachment(hatAttachment);
                            skel.getAttachments()[hatAtID].setModel(hatEnt);

                            //play time tracking
                            if (m_sharedData.hairInfo[hatID].workshopID
                                && i == m_sharedData.localConnectionData.connectionID)
                            {
                                m_modelStats.push_back(m_sharedData.hairInfo[hatID].workshopID);
                            }
                        }


                        //look to see if we have a hair model to attach
                        auto hairID = indexFromHairID(pd.hairID);

                        if (hairID != 0
                            && md.loadFromFile(m_sharedData.hairInfo[hairID].modelPath))
                        {
                            auto hairEnt = createHeadEnt(pc::ColourKey::Hair, 0);
                            skel.getAttachments()[id].setModel(hairEnt);

                            if (m_sharedData.hairInfo[hairID].workshopID
                                && i == m_sharedData.localConnectionData.connectionID)
                            {
                                m_modelStats.push_back(m_sharedData.hairInfo[hairID].workshopID);
                            }
                        }
                    }

                    //find attachment points for club model
                    id = skel.getAttachmentIndex("hands");
                    if (id > -1)
                    {
                        //if we're using a single set of anims we need to copy the hand attachment to match
                        if (defaultAttachment != -1)
                        {
                            skel.getAttachments()[id] = defaultAnims.getSkeleton().getAttachments()[defaultAttachment];
                        }
                        m_avatars[i][j].hands = &skel.getAttachments()[id];
                    }
                }

                entity.getComponent<cro::Model>().setHidden(true);
                entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::CubeMap);
                m_avatars[i][j].model = entity;

                auto& avRot = entity.addComponent<AvatarRotation>();
                avRot.avatarFlipped = m_avatars[i][j].flipped;
                avRot.groundRotation = std::bind(&GolfState::getGroundRotation, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

                if (m_sharedData.avatarInfo[avatarIndex].workshopID
                    && i == m_sharedData.localConnectionData.connectionID)
                {
                    m_modelStats.push_back(m_sharedData.avatarInfo[avatarIndex].workshopID);
                }
            }
        }
    }
    //m_activeAvatar = &m_avatars[0][0]; //DON'T DO THIS! WE MUST BE NULL WHEN THE MAP LOADS


    //club models

    //collect all search paths for club models
    std::unordered_map<std::uint32_t, std::string> clubPaths;
    const auto processClubPath = 
        [&](const std::string& path)
        {
            const std::string fileName = "/list.cst";
            cro::ConfigFile cfg;
            if (cfg.loadFromFile(path + fileName, false)) //resource path was already added
            {
                //TODO we need to do full validation, eg models exist here
                if (const auto* uid = cfg.findProperty("uid");
                    uid != nullptr)
                {
                    const auto id = uid->getValue<std::uint32_t>();
                    if (clubPaths.count(id) == 0)
                    {
                        clubPaths.insert(std::make_pair(id, path + fileName));
                    }
                }
            }
        };

    const auto ContentDirs = Content::getInstallPaths();
    for (const auto& c : ContentDirs)
    {
        const auto basePath = cro::FileSystem::getResourcePath() + c + "clubs/";
        const auto clubsets = cro::FileSystem::listDirectories(basePath);

        for (const auto& s : clubsets)
        {
            processClubPath(basePath + s);
        }
    }

    //workshop clubs
    const auto basePath = Content::getUserContentPath(Content::UserContent::Clubs);
    auto clubsets = cro::FileSystem::listDirectories(basePath);

    //remove dirs from this list if it's not from the workshop (rather crudely)
    clubsets.erase(std::remove_if(clubsets.begin(), clubsets.end(), [](const std::string& s) {return s.back() != 'w'; }), clubsets.end());

    if (clubsets.size() > ConstVal::MaxClubsets)
    {
        clubsets.resize(ConstVal::MaxClubsets);
        LogW << "Installed clubsets have been truncated to the maximum 64!" << std::endl;
    }

    for (const auto& s : clubsets)
    {
        processClubPath(basePath + s);
    }


    const auto loadClubModel = 
        [&](std::uint32_t clubID)
        {
            if (m_clubModels.count(clubID) == 0)
            {
                m_clubModels.insert(std::make_pair(clubID, ClubModels()));
                auto& models = m_clubModels.at(clubID);

                if (models.loadFromFile(clubPaths.at(clubID), m_resources, m_gameScene))
                {
                    std::int32_t i = 0;
                    for (auto entity : models.models)
                    {
                        entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::CubeMap));

                        const auto matCount = entity.getComponent<cro::Model>().getMeshData().submeshCount;

                        for (auto j = 0u; j < matCount; ++j)
                        {
                            auto matID = MaterialID::Ball;

                            switch (j)
                            {
                            default: 
                                if (m_sharedData.nightTime
                                    && models.materialIDs[i][j] == ClubModels::Emissive)
                                {
                                    matID = MaterialID::BallNight;
                                    //matID = MaterialID::Emissive;
                                }
                                break;
                            case 1:
                                if (m_sharedData.nightTime)
                                {
                                    matID = models.materialIDs[i][j] == ClubModels::Emissive ? 
                                        MaterialID::BallNight : MaterialID::Trophy;
                                        //MaterialID::Emissive : MaterialID::Trophy;
                                }
                                else
                                {
                                    matID = MaterialID::Trophy;
                                }
                                break;
                            }
                            auto material = m_resources.materials.get(m_materialIDs[matID]);
                            entity.getComponent<cro::Model>().setMaterial(j, material);

                        }
                        i++;

                        entity.addComponent<cro::Callback>().active = true;
                        entity.getComponent<cro::Callback>().function =
                            [&](cro::Entity e, float)
                            {
                                if (m_activeAvatar)
                                {
                                    bool hidden = !(!m_activeAvatar->model.getComponent<cro::Model>().isHidden() &&
                                        m_activeAvatar->hands->getModel() == e);

                                    e.getComponent<cro::Model>().setHidden(hidden);
                                }
                            };
                    }
                }
            }        
        };


    //for each profile in the game load the club models if not already loaded
    for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
    {
        for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
        {
            const auto clubID = m_sharedData.connectionData[i].playerData[j].clubID;
            if (clubPaths.count(clubID) == 0)
            {
                //profile has invalid model set, so use fallback
                m_avatars[i][j].clubModelID = 0;
            }
            else
            {
                m_avatars[i][j].clubModelID = clubID;
                loadClubModel(clubID);

#ifdef USE_GNS
                if (i == m_sharedData.localConnectionData.connectionID)
                {
                    if (m_clubModels.count(clubID) != 0
                        && m_clubModels.at(clubID).workshopID != 0)
                    {
                        m_modelStats.push_back(m_clubModels.at(clubID).workshopID);
                    }
                }
#endif
            }
        }
    }

    //if we didn't load the default set of clubs for some reason
    //create a fallback model so we still have something to reference
    loadClubModel(0);

    for (auto& [_,models] : m_clubModels)
    {
        if (models.models.empty())
        {
            models.models.push_back(m_gameScene.createEntity());
            createFallbackModel(models.models.back(), m_resources);
        }
    }



#ifdef USE_GNS
    if (!m_modelStats.empty())
    {
        Social::beginStats(m_modelStats);
    }
#endif



    //ball resources - ball is rendered as a single point
    //at a distance, and as a model when closer
    //glCheck(glPointSize(BallPointSize)); - this is set in resize callback based on the buffer resolution/pixel scale
    m_ballResources.materialID = m_materialIDs[MaterialID::WireFrameCulledPoint];
    m_ballResources.ballMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));
    m_ballResources.shadowMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));

    auto* meshData = &m_resources.meshes.getMesh(m_ballResources.ballMeshID);
    std::vector<float> verts =
    {
        0.f, 0.f, 0.f,   1.f, 1.f, 1.f, 1.f
    };
    std::vector<std::uint32_t> indices =
    {
        0
    };

    meshData->vertexCount = 1;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = 1;
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    meshData = &m_resources.meshes.getMesh(m_ballResources.shadowMeshID);
    verts =
    {
        0.f, 0.f, 0.f,    0.f, 0.f, 0.f, 0.25f,
    };
    meshData->vertexCount = 1;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = 1;
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    /*auto& trail = m_ballTrails.emplace_back(std::make_unique<BallTrail>());
    trail->create(m_gameScene, m_resources, m_materialIDs[MaterialID::BallTrail]);
    trail->setUseBeaconColour(m_sharedData.trailBeaconColour);*/
}

void GolfState::loadSpectators()
{
    cro::ModelDefinition md(m_resources);
    const std::array modelPaths =
    {
        "assets/golf/models/spectators/01.cmt",
        "assets/golf/models/spectators/02.cmt",
        "assets/golf/models/spectators/03.cmt",
        "assets/golf/models/spectators/04.cmt"
    };

    const bool brolly = m_sharedData.weatherType == WeatherType::Rain
        || m_sharedData.weatherType == WeatherType::Showers;

    cro::ModelDefinition brollyDef(m_resources);
    cro::ModelDefinition brollyAnim(m_resources);
    std::size_t colourIdx = 0;
    constexpr std::array ColourIndices = { 1,6,8,15,17,25 };
    if (brolly)
    {
        brollyDef.loadFromFile("assets/golf/models/spectators/umbrella_attachment.cmt");
        brollyAnim.loadFromFile("assets/golf/models/spectators/wet_animation.cmt");
    }

    for (auto i = 0; i < 2; ++i)
    {
        for (const auto& path : modelPaths)
        {
            if (md.loadFromFile(path))
            {
                for (auto j = 0; j < 3; ++j)
                {
                    auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>();
                    md.createModel(entity);
                    entity.getComponent<cro::Model>().setHidden(true);
                    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));

                    if (md.hasSkeleton())
                    {
                        auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedSkinned]);
                        applyMaterialData(md, material);

                        glm::vec4 rect((1.f / 3.f) * j, 0.f, (1.f / 3.f), 1.f);
                        material.setProperty("u_subrect", rect);
                        entity.getComponent<cro::Model>().setMaterial(0, material);

                        auto& skel = entity.getComponent<cro::Skeleton>();
                        std::int32_t attachmentIdx = -1;

                        if (brollyAnim.isLoaded()
                            && brollyAnim.hasSkeleton())
                        {
                            const auto& srcSkel = brollyAnim.getSkeleton();
                            for (auto k = 0u; k < srcSkel.getAnimations().size(); ++k)
                            {
                                skel.addAnimation(srcSkel, k);
                            }

                            if (attachmentIdx = srcSkel.getAttachmentIndex("brolly"); attachmentIdx != -1)
                            {
                                skel.addAttachment(srcSkel.getAttachments()[attachmentIdx]);

                                //update with new index as we can't really assume they'll be the same (although probably are)
                                attachmentIdx = skel.getAttachmentIndex("brolly");
                            }
                        }

                        if (!skel.getAnimations().empty())
                        {
                            auto& spectator = entity.addComponent<Spectator>();
                            for (auto k = 0u; k < skel.getAnimations().size(); ++k)
                            {
                                if (brolly)
                                {
                                    if (skel.getAnimations()[k].name == "Walk_brolly") //ugh this is mis-named in the file
                                    {
                                        spectator.anims[Spectator::AnimID::Walk] = k;
                                    }
                                    else if (skel.getAnimations()[k].name == "Idle_Brolly")
                                    {
                                        spectator.anims[Spectator::AnimID::Idle] = k;
                                    }
                                }
                                else
                                {
                                    if (skel.getAnimations()[k].name == "Walk")
                                    {
                                        spectator.anims[Spectator::AnimID::Walk] = k;
                                    }
                                    else if (skel.getAnimations()[k].name == "Idle")
                                    {
                                        spectator.anims[Spectator::AnimID::Idle] = k;
                                    }
                                }
                            }
                            skel.setMaxInterpolationDistance(30.f);

                            //add brolly if attachment point exists
                            if (attachmentIdx != -1)
                            {
                                auto material = m_resources.materials.get(m_materialIDs[MaterialID::Cel]);
                                material.doubleSided = true;

                                material.setProperty("u_ballColour", CD32::Colours[ColourIndices[colourIdx % ColourIndices.size()]]);
                                colourIdx++;

                                auto childEnt = m_gameScene.createEntity();
                                childEnt.addComponent<cro::Transform>();
                                brollyDef.createModel(childEnt);

                                childEnt.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));
                                childEnt.getComponent<cro::Model>().setMaterial(0, material);

                                childEnt.addComponent<cro::Callback>().active = true;
                                childEnt.getComponent<cro::Callback>().function =
                                    [entity](cro::Entity e, float)
                                    {
                                        e.getComponent<cro::Model>().setHidden(entity.getComponent<cro::Model>().isHidden());
                                    };
                                skel.getAttachments()[attachmentIdx].setModel(childEnt);
                            }
                        }
                    }

                    if (m_sharedData.crowdDensity == (CrowdDensityCount - 1))
                    {
                        //set to zero crowd
                        entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                    }

                    m_spectatorModels.push_back(entity);
                }
            }
        }
    }

    std::shuffle(m_spectatorModels.begin(), m_spectatorModels.end(), cro::Util::Random::rndEngine);
}

void GolfState::initAudio(bool loadTrees, bool loadPlane)
{
    if (cro::AudioMixer::hasAudioRenderer())
    {
        if (m_sharedData.nightTime)
        {
            auto ext = cro::FileSystem::getFileExtension(m_audioPath);
            auto nightPath = m_audioPath.substr(0, m_audioPath.find(ext)) + "_n" + ext;

            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + nightPath))
            {
                m_audioPath = nightPath;
            }
        }


        //6 evenly spaced points with ambient audio
        auto envOffset = glm::vec2(MapSize) / 3.f;
        cro::AudioScape as;
        if (as.loadFromFile(m_audioPath, m_resources.audio))
        {
            std::array emitterNames =
            {
                std::string("01"),
                std::string("02"),
                std::string("03"),
                std::string("04"),
                std::string("05"),
                std::string("06"),
                std::string("03"),
                std::string("04"),
            };

            for (auto i = 0; i < 2; ++i)
            {
                for (auto j = 0; j < 2; ++j)
                {
                    static constexpr float height = 4.f;
                    glm::vec3 position(envOffset.x * (i + 1), height, -envOffset.y * (j + 1));

                    auto idx = i * 2 + j;

                    if (as.hasEmitter(emitterNames[idx + 4]))
                    {
                        auto entity = m_gameScene.createEntity();
                        entity.addComponent<cro::Transform>().setPosition(position);
                        entity.addComponent<cro::AudioEmitter>() = as.getEmitter(emitterNames[idx + 4]);
                        entity.getComponent<cro::AudioEmitter>().play();
                        entity.getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(5.f));
                    }

                    position = { i * MapSize.x, height, -static_cast<float>(MapSize.y) * j };

                    if (as.hasEmitter(emitterNames[idx]))
                    {
                        auto entity = m_gameScene.createEntity();
                        entity.addComponent<cro::Transform>().setPosition(position);
                        entity.addComponent<cro::AudioEmitter>() = as.getEmitter(emitterNames[idx]);
                        entity.getComponent<cro::AudioEmitter>().play();
                    }
                }
            }

            //random incidental audio
            if (as.hasEmitter("incidental01")
                && as.hasEmitter("incidental02"))
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::AudioEmitter>() = as.getEmitter("incidental01");
                entity.getComponent<cro::AudioEmitter>().setLooped(false);
                auto plane01 = entity;

                entity = m_gameScene.createEntity();
                entity.addComponent<cro::AudioEmitter>() = as.getEmitter("incidental02");
                entity.getComponent<cro::AudioEmitter>().setLooped(false);
                auto plane02 = entity;

                //we'll shoehorn the plane in here. won't make much sense
                //if the audioscape has different audio but hey...
                cro::ModelDefinition md(m_resources);
                cro::Entity planeEnt;
                if (loadPlane &&
                    md.loadFromFile("assets/golf/models/plane.cmt"))
                {
                    static constexpr glm::vec3 Start(-32.f, 60.f, 20.f);
                    static constexpr glm::vec3 End(352.f, 60.f, -220.f);

                    entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>().setPosition(Start);
                    entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 32.f * cro::Util::Const::degToRad);
                    entity.getComponent<cro::Transform>().setScale({ 0.01f, 0.01f, 0.01f });
                    md.createModel(entity);

                    entity.addComponent<cro::Callback>().function =
                        [](cro::Entity e, float dt)
                        {
                            static constexpr float Speed = 10.f;
                            const float MaxLen = glm::length2((Start - End) / 2.f);

                            auto& tx = e.getComponent<cro::Transform>();
                            auto dir = glm::normalize(tx.getRightVector()); //scaling means this isn't normalised :/
                            tx.move(dir * Speed * dt);

                            float currLen = glm::length2((Start + ((Start + End) / 2.f)) - tx.getPosition());
                            float scale = std::max(1.f - (currLen / MaxLen), 0.001f); //can't scale to 0 because it breaks normalizing the right vector above
                            tx.setScale({ scale, scale, scale });

                            if (tx.getPosition().x > End.x)
                            {
                                tx.setPosition(Start);
                                e.getComponent<cro::Callback>().active = false;
                            }
                        };

                    auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
                    applyMaterialData(md, material);

                    //TODO we should be reading the texture size from the model...
                    const auto* m = md.getMaterial(0);
                    if (m->properties.count("u_diffuseMap"))
                    {
                        static constexpr std::uint32_t TexSize = 512;
                        m_planeTexture.create(TexSize, TexSize, false);

                        const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
                        const auto tex = cro::TextureID(m->properties.at("u_diffuseMap").second.textureID);
                        updateBannerTexture(font, tex, m_planeTexture, BannerStrings[BannerIndex]);

                        material.setProperty("u_diffuseMap", m_planeTexture.getTexture());
                    }
                    entity.getComponent<cro::Model>().setMaterial(0, material);


                    //engine
                    entity.addComponent<cro::AudioEmitter>(); //always needs one in case audio doesn't exist
                    if (as.hasEmitter("plane"))
                    {
                        entity.getComponent<cro::AudioEmitter>() = as.getEmitter("plane");
                        entity.getComponent<cro::AudioEmitter>().setLooped(false);
                    }

                    planeEnt = entity;
                }

                struct AudioData final
                {
                    float currentTime = 0.f;
                    float timeout = static_cast<float>(cro::Util::Random::value(32, 64));
                    cro::Entity activeEnt;
                };

                entity = m_gameScene.createEntity();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<AudioData>();
                entity.getComponent<cro::Callback>().function =
                    [plane01, plane02, planeEnt](cro::Entity e, float dt) mutable
                    {
                        auto& [currTime, timeOut, activeEnt] = e.getComponent<cro::Callback>().getUserData<AudioData>();

                        if (!activeEnt.isValid()
                            || activeEnt.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
                        {
                            currTime += dt;

                            if (currTime > timeOut)
                            {
                                currTime = 0.f;
                                timeOut = static_cast<float>(cro::Util::Random::value(120, 240));

                                auto id = cro::Util::Random::value(0, 2);
                                if (id == 0)
                                {
                                    //fly the plane
                                    if (planeEnt.isValid())
                                    {
                                        planeEnt.getComponent<cro::Callback>().active = true;
                                        planeEnt.getComponent<cro::AudioEmitter>().play();
                                        activeEnt = planeEnt;
                                    }
                                }
                                else
                                {
                                    auto ent = (id == 1) ? plane01 : plane02;
                                    if (ent.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
                                    {
                                        ent.getComponent<cro::AudioEmitter>().play();
                                        activeEnt = ent;
                                    }
                                }
                            }
                        }
                    };
            }

            //put the new hole music on the cam for accessibilty
            //this is done *before* m_cameras is updated 
            if (as.hasEmitter("music"))
            {
                m_gameScene.getActiveCamera().addComponent<cro::AudioEmitter>() = as.getEmitter("music");
                m_gameScene.getActiveCamera().getComponent<cro::AudioEmitter>().setLooped(false);
            }
        }
        else
        {
            //still needs an emitter to stop crash playing non-loaded music
            m_gameScene.getActiveCamera().addComponent<cro::AudioEmitter>();
            LogE << "Invalid AudioScape file was found" << std::endl;
        }

        //TODO this is all the same as the driving range, so we can wrap this up in a free func

        registerCommand("list_tracks", [&](const std::string&)
            {
                const auto& trackList = m_sharedData.playlist.getTrackList();

                if (!trackList.empty())
                {
                    for (const auto& str : trackList)
                    {
                        cro::Console::print(str);
                    }
                }
                else
                {
                    cro::Console::print("No music loaded");
                }
            });

        if (!m_sharedData.playlist.getTrackList().empty())
        {
            auto gameMusic = m_gameScene.getActiveCamera();

            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::AudioEmitter>(m_musicStream).setMixerChannel(MixerChannel::UserMusic);
            entity.getComponent<cro::AudioEmitter>().setVolume(0.6f);
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, gameMusic](cro::Entity e, float dt)
                {
                    //set the mixer channel to inverse valaue of main music channel
                    //while the incidental music is playing
                    if (gameMusic.isValid())
                    {
                        //fade out if the menu music is playing, ie in a transition
                        const float target = gameMusic.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Playing ? 1.f - std::ceil(cro::AudioMixer::getVolume(MixerChannel::Music)) : 1.f;
                        float vol = cro::AudioMixer::getPrefadeVolume(MixerChannel::UserMusic);
                        if (target < vol)
                        {
                            vol = std::max(0.f, vol - (dt * 2.f));
                        }
                        else
                        {
                            vol = std::min(1.f, vol + dt);
                        }
                        cro::AudioMixer::setPrefadeVolume(vol, MixerChannel::UserMusic);
                    }


                    //check the current music state and pause when volume is low else play the
                    //next track when we stop playing.
                    const float currVol = cro::AudioMixer::getVolume(MixerChannel::UserMusic);
                    auto state = e.getComponent<cro::AudioEmitter>().getState();

                    if (state == cro::AudioEmitter::State::Playing)
                    {
                        if (currVol < MinMusicVolume)
                        {
                            e.getComponent<cro::AudioEmitter>().pause();
                        }
                    }
                    else if ((state == cro::AudioEmitter::State::Paused
                        && currVol > MinMusicVolume) || state == cro::AudioEmitter::State::Stopped)
                    {
                        e.getComponent<cro::AudioEmitter>().play();
                    }


                    if (e.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Playing)
                    {
                        std::int32_t samples = 0;
                        const auto* data = m_sharedData.playlist.getData(samples);
                        m_musicStream.updateBuffer(data, samples);
                    }
                };
        }



        if (loadTrees)
        {
            const std::array<std::string, 3u> paths =
            {
                "assets/golf/sound/ambience/trees01.ogg",
                "assets/golf/sound/ambience/trees03.ogg",
                "assets/golf/sound/ambience/trees02.ogg"
            };

            /*const std::array positions =
            {
                glm::vec3(80.f, 4.f, -66.f),
                glm::vec3(240.f, 4.f, -66.f),
                glm::vec3(160.f, 4.f, -66.f),
                glm::vec3(240.f, 4.f, -123.f),
                glm::vec3(160.f, 4.f, -123.f),
                glm::vec3(80.f, 4.f, -123.f)
            };

            auto callback = [&](cro::Entity e, float)
            {
                float amount = std::min(1.f, m_windUpdate.currentWindSpeed);
                float pitch = 0.5f + (0.8f * amount);
                float volume = 0.05f + (0.3f * amount);

                e.getComponent<cro::AudioEmitter>().setPitch(pitch);
                e.getComponent<cro::AudioEmitter>().setVolume(volume);
            };

            //this works but... meh
            for (auto i = 0u; i < paths.size(); ++i)
            {
                if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + paths[i]))
                {
                    for (auto j = 0u; j < 2u; ++j)
                    {
                        auto id = m_resources.audio.load(paths[i], true);

                        auto entity = m_gameScene.createEntity();
                        entity.addComponent<cro::Transform>().setPosition(positions[i + (j * paths.size())]);
                        entity.addComponent<cro::AudioEmitter>().setSource(m_resources.audio.get(id));
                        entity.getComponent<cro::AudioEmitter>().setVolume(0.f);
                        entity.getComponent<cro::AudioEmitter>().setLooped(true);
                        entity.getComponent<cro::AudioEmitter>().setRolloff(0.f);
                        entity.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                        entity.getComponent<cro::AudioEmitter>().play();

                        entity.addComponent<cro::Callback>().active = true;
                        entity.getComponent<cro::Callback>().function = callback;
                    }
                }
            }*/
        }


        //fades in the audio
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(0.f);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
            {
                auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
                progress = std::min(1.f, progress + (dt / 5.f));

                cro::AudioMixer::setPrefadeVolume(cro::Util::Easing::easeOutQuad(progress), MixerChannel::Effects);
                cro::AudioMixer::setPrefadeVolume(cro::Util::Easing::easeOutQuad(progress), MixerChannel::Environment);
                cro::AudioMixer::setPrefadeVolume(cro::Util::Easing::easeOutQuad(progress), MixerChannel::UserMusic);

                if (progress == 1)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_gameScene.destroyEntity(e);
                }
            };

    }
}

void GolfState::updateFlagTexture(bool reloadTexture)
{
    if (!m_flagTexture.available())
    {
        //LogI << "Flag Texture is unavailable for custom flags" << std::endl;
        if (cro::FileSystem::fileExists(m_sharedData.flagPath))
        {
            m_flagTexture.create(FlagTextureSize.x, FlagTextureSize.y, false);
            //LogI << "Found " << m_sharedData.flagPath << " - creating flag texture..." << std::endl;
        }
        else
        {
            //LogI << m_sharedData.flagPath << ": file not found." << std::endl;
            return;
        }
    }

    if (reloadTexture)
    {
        if (!m_resources.textures.loaded(TextureID::Flag))
        {
            m_resources.textures.load(TextureID::Flag, m_sharedData.flagPath);
            /*if (m_resources.textures.load(TextureID::Flag, m_sharedData.flagPath))
                LogI << "loaded flag texture from " << m_sharedData.flagPath << std::endl;
            else
                LogI << "failed loading flag texture " << m_sharedData.flagPath << std::endl;*/
        }
        else
        {
            //overwrite existing to recycle the handle.
            m_resources.textures.get(TextureID::Flag).loadFromFile(m_sharedData.flagPath);
            /*if (m_resources.textures.get(TextureID::Flag).loadFromFile(m_sharedData.flagPath))
                LogI << "reloaded flag texture from " << m_sharedData.flagPath << std::endl;
            else
                LogI << "failed reloading flag texture " << m_sharedData.flagPath << std::endl;*/
        }
        m_flagQuad.setTexture(m_resources.textures.get(TextureID::Flag));

        //just saves doing this every time we update the text
        m_flagText.setFont(m_sharedData.sharedResources->fonts.get(FontID::UI));
        m_flagText.setAlignment(cro::SimpleText::Alignment::Centre);
        m_flagText.setPosition({ 160.f, 80.f });
        m_flagText.setScale(glm::vec2(12.f));
        m_flagText.setCharacterSize(UITextSize);
    }

    m_flagTexture.clear(cro::Colour::Magenta);
    m_flagQuad.draw();

    if (m_sharedData.flagText)
    {
        m_flagText.setFillColour(m_sharedData.flagText == 1 ? CD32::Colours[CD32::Black] : CD32::Colours[CD32::BeigeLight]);
        m_flagText.setString(std::to_string(holeNumberFromIndex()));
        m_flagText.draw();
    }

    m_flagTexture.display();


    cro::TextureID tid(m_flagTexture.getTexture());

    cro::Command cmd;
    cmd.targetFlags = CommandID::Flag;
    cmd.action = [tid](cro::Entity e, float)
        {
            e.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", tid);
            //LogI << "Set flag texture on model" << std::endl;
        };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

#ifdef USE_GNS
void GolfState::initBillboardLeagueTexture()
{
    if (!m_billboardLeagueTexture.available())
    {
        /*registerWindow([&]() 
            {
                ImGui::Begin("Text");
                ImGui::Image(m_billboardLeagueTexture.getTexture(), {320.f, 240.f}, {0.f, 1.f}, {1.f, 0.f});
                ImGui::End();
            });*/

        m_billboardLeagueTexture.create(640, 480, false);
        m_billboardLeagueTexture.setSmooth(true);
        m_billboardLeagueTexture.clear(TextNormalColour);

        cro::SimpleText text(m_sharedData.sharedResources->fonts.get(FontID::UI));
        text.setFillColour(TextNormalColour);
        text.setCharacterSize(UITextSize * 4);
        text.setPosition({ 320.f, 444.f });
        text.setString("Last Month's\nLeague Champions");
        text.setAlignment(cro::SimpleText::Alignment::Centre);
        text.setVerticalSpacing(-2.f);
        text.draw(); //hacks around the font bug where the texture is messed up the first time a char size is used
        text.setFillColour(LeaderboardTextDark);
        text.draw();

        text.setAlignment(cro::SimpleText::Alignment::Left);
        text.setPosition({ 18.f, 378.f });
        text.setCharacterSize(UITextSize * 2);

        cro::SimpleQuad av;
        av.setPosition({ 24.f, 358.f });
        cro::Texture avTex;

        constexpr float VerticalSpacing = -76.f; //-135.f;
        constexpr float BGTop = 408.f;
        constexpr float BGWidth = 640.f;

        constexpr auto c = CD32::Colours[CD32::BeigeMid];
        cro::SimpleVertexArray arr;
        arr.setPrimitiveType(GL_TRIANGLES);
        arr.setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f, BGTop), c),
                cro::Vertex2D(glm::vec2(0.f, BGTop + VerticalSpacing), c),
                cro::Vertex2D(glm::vec2(BGWidth, BGTop), c),
                
                cro::Vertex2D(glm::vec2(BGWidth, BGTop), c),
                cro::Vertex2D(glm::vec2(0.f, BGTop + VerticalSpacing), c),
                cro::Vertex2D(glm::vec2(BGWidth, BGTop + VerticalSpacing), c),


                cro::Vertex2D(glm::vec2(0.f, BGTop + (VerticalSpacing * 2.f)), c),
                cro::Vertex2D(glm::vec2(0.f, BGTop + (VerticalSpacing * 3.f)), c),
                cro::Vertex2D(glm::vec2(BGWidth, BGTop + (VerticalSpacing * 2.f)), c),

                cro::Vertex2D(glm::vec2(BGWidth, BGTop + (VerticalSpacing * 2.f)), c),
                cro::Vertex2D(glm::vec2(0.f, BGTop + (VerticalSpacing * 3.f)), c),
                cro::Vertex2D(glm::vec2(BGWidth, BGTop + (VerticalSpacing * 3.f)), c),


                cro::Vertex2D(glm::vec2(0.f, BGTop + (VerticalSpacing * 4.f)), c),
                cro::Vertex2D(glm::vec2(0.f, BGTop + (VerticalSpacing * 5.f)), c),
                cro::Vertex2D(glm::vec2(BGWidth, BGTop + (VerticalSpacing * 4.f)), c),

                cro::Vertex2D(glm::vec2(BGWidth, BGTop + (VerticalSpacing * 4.f)), c),
                cro::Vertex2D(glm::vec2(0.f, BGTop + (VerticalSpacing * 5.f)), c),
                cro::Vertex2D(glm::vec2(BGWidth, BGTop + (VerticalSpacing * 5.f)), c),
            });
        arr.draw();

        const auto& prevWinners = Social::getPreviousLeague();
        for (const auto& [name, score, handle] : prevWinners)
        {
            if (handle != 0)
            {
                auto img = Social::getUserIcon(handle);
                avTex.loadFromImage(img);
                av.setTexture(avTex);
                av.setScale({ 0.25f, 0.25f });
                av.draw();
                av.move({ 0.f, VerticalSpacing });

                cro::String s("     ");
                s += name;
                s += "\n\n\n" + std::to_string(score);
                text.setString(s);
                text.setFillColour(TextNormalColour);
                text.draw();
                text.setFillColour(LeaderboardTextDark);
                text.draw();
                text.move({ 0.f, VerticalSpacing });
            }
        }

        m_billboardLeagueTexture.display();
    }
}
#endif

void GolfState::TargetShader::update()
{
    static const auto RotMat = glm::toMat4(glm::rotate(cro::Transform::QUAT_IDENTITY, -cro::Util::Const::PI / 2.f, cro::Transform::X_AXIS));

    auto s = size + TargetShader::Epsilon;

    projMat = glm::ortho(-s, s, -s, s, -20.f, 20.f);
    viewMat = glm::translate(RotMat, -position );

    glCheck(glUseProgram(shaderID));
    glCheck(glUniformMatrix4fv(vpUniform, 1, GL_FALSE, glm::value_ptr(projMat * viewMat)));
}