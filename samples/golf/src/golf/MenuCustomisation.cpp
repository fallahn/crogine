/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "MenuState.hpp"
#include "GameConsts.hpp"
#include "MenuConsts.hpp"
#include "CommonConsts.hpp"
#include "spooky2.hpp"
#include "BallSystem.hpp"
#include "CallbackData.hpp"
#include "MenuEnum.inl"

#include <Social.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>

namespace
{
#include "RandNames.hpp"

    std::uint64_t findWorkshopID(const std::string& path)
    {
        //do this with a cro string to try and preserve utf encoding
        cro::String str = cro::String::fromUtf8(path.begin(), path.end());
        auto dirList = cro::Util::String::tokenize(str, "/");

        if (!dirList.empty())
        {
            if (dirList.back().find("w") == dirList.back().size() - 1)
            {
                std::stringstream ss;
                ss << dirList.back().substr(0, dirList.back().size() - 1).toAnsiString();

                std::uint64_t id = 0;
                ss >> id;

                return id;
            }
        }

        return 0;
    };

    SharedStateData::BallInfo readBallCfg(const cro::ConfigFile& cfg)
    {
        SharedStateData::BallInfo retVal;

        const auto& props = cfg.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "model")
            {
                retVal.modelPath = p.getValue<std::string>();
            }
            else if (name == "uid")
            {
                retVal.uid = p.getValue<std::uint32_t>();
            }
            else if (name == "tint")
            {
                retVal.tint = p.getValue<cro::Colour>();
            }
            else if (name == "roll")
            {
                retVal.rollAnimation = p.getValue<bool>();
            }
        }

        return retVal;
    }

    SharedStateData::HairInfo readHairCfg(const cro::ConfigFile& cfg)
    {
        SharedStateData::HairInfo retVal;

        const auto& props = cfg.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "model")
            {
                retVal.modelPath = p.getValue<std::string>();
            }
            else if (name == "uid")
            {
                retVal.uid = p.getValue<std::uint32_t>();
            }
        }

        return retVal;
    }

    void insertInfo(SharedStateData::BallInfo info, std::vector<SharedStateData::BallInfo>& dst, bool relPath)
    {
        bool exists = relPath ?
            cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + info.modelPath) :
            cro::FileSystem::fileExists(info.modelPath);

        if ((!info.modelPath.empty() && exists))
        {
            auto ball = std::find_if(dst.begin(), dst.end(),
                [&info](const SharedStateData::BallInfo& ballPair)
                {
                    return ballPair.uid == info.uid;
                });

            if (ball == dst.end())
            {
                dst.emplace_back(info);
            }
#ifdef CRO_DEBUG_
            else
            {
                LogW << info.modelPath << ": a ball already exists with UID " << info.uid << std::endl;
                LogW << info.uid << " has been ignored" << std::endl;
            }
#endif
        }
    }

    void insertInfo(SharedStateData::HairInfo info, std::vector<SharedStateData::HairInfo>& dst, bool relPath)
    {
        bool exists = relPath ?
            cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + info.modelPath) :
            cro::FileSystem::fileExists(info.modelPath);

        if ((!info.modelPath.empty() && exists))
        {
            auto hair = std::find_if(dst.begin(), dst.end(), [&info](const SharedStateData::HairInfo& h)
                {
                    return h.uid == info.uid;
                });

            if (hair == dst.end())
            {
                dst.emplace_back(info);
            }
#ifdef CRO_DEBUG_
            else
            {
                LogW << "Duplicate hair " << info.uid << " has been skipped for loading" << std::endl;
            }
#endif
        }
    }
}

void MenuState::createBallScene()
{
    if (!cro::FileSystem::directoryExists(Social::getBaseContentPath()))
    {
        cro::FileSystem::createDirectory(Social::getBaseContentPath());
    }

    static constexpr float RootPoint = 100.f;
    static constexpr float BallSpacing = 0.09f;

    auto ballTargetCallback = [](cro::Entity e, float dt)
    {
        auto id = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
        float target = RootPoint + (BallSpacing * id);

        auto pos = e.getComponent<cro::Transform>().getPosition();
        auto diff = target - pos.x;
        pos.x += diff * (dt * 10.f);

        e.getComponent<cro::Transform>().setPosition(pos);
        e.getComponent<cro::Camera>().active = (std::abs(diff) > Ball::Radius * 0.1f);
    };

    auto ballTexCallback = [&](cro::Camera&)
    {
        std::uint32_t samples = m_sharedData.pixelScale ? 0 :
            m_sharedData.antialias ? m_sharedData.multisamples : 0;

        //auto windowSize = static_cast<float>(cro::App::getWindow().getSize().x);

        float windowScale = getViewScale();
        float scale = m_sharedData.pixelScale ? windowScale : 1.f;
        
        auto invScale = static_cast<std::uint32_t>((windowScale + 1.f) - scale);
        auto size = BallPreviewSize * invScale;
        m_ballTexture.create(size.x, size.y, true, false, samples);
    };

    m_ballCam = m_backgroundScene.createEntity();
    m_ballCam.addComponent<cro::Transform>().setPosition({ RootPoint - 10.f, 0.045f, 0.095f });
    m_ballCam.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.03f);
    m_ballCam.addComponent<cro::Camera>().setPerspective(1.f, static_cast<float>(BallPreviewSize.x) / BallPreviewSize.y, 0.001f, 2.f);
    m_ballCam.getComponent<cro::Camera>().resizeCallback = ballTexCallback;
    m_ballCam.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, BallRenderFlags);
    m_ballCam.addComponent<cro::Callback>().active = true;
    m_ballCam.getComponent<cro::Callback>().setUserData<std::int32_t>(0);
    m_ballCam.getComponent<cro::Callback>().function = ballTargetCallback;
    

    ballTexCallback(m_ballCam.getComponent<cro::Camera>());

    auto ballFiles = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + "assets/golf/balls");
    if (ballFiles.empty())
    {
        LogE << "No ball files were found" << std::endl;
    }

    if (ballFiles.size() > ConstVal::MaxBalls)
    {
        ballFiles.resize(ConstVal::MaxBalls);
    }

    m_sharedData.ballInfo.clear();

    //parse the default ball directory
    for (const auto& file : ballFiles)
    {
        cro::ConfigFile cfg;
        if (cro::FileSystem::getFileExtension(file) == ".ball"
            && cfg.loadFromFile("assets/golf/balls/" + file))
        {
            auto info = readBallCfg(cfg);

            //if we didn't find a UID create one from the file name and save it to the cfg
            if (info.uid == 0)
            {
                info.uid = SpookyHash::Hash32(file.data(), file.size(), 0);
                cfg.addProperty("uid").setValue(info.uid);
                cfg.save("assets/golf/balls/" + file);
            }

            insertInfo(info, m_sharedData.ballInfo, true);
        }
    }

    //look in the user directory - only do this if the default dir is OK?
    const auto BallUserPath = Social::getUserContentPath(Social::UserContent::Ball);
    if (cro::FileSystem::directoryExists(BallUserPath))
    {
        auto dirList = cro::FileSystem::listDirectories(BallUserPath);
        if (dirList.size() > ConstVal::MaxBalls)
        {
            dirList.resize(ConstVal::MaxBalls);
        }

        for (const auto& dir : dirList)
        {
            auto path = BallUserPath + dir + "/" ;
            auto files = cro::FileSystem::listFiles(path);

            for (const auto& file : files)
            {
                if (cro::FileSystem::getFileExtension(file) == ".ball")
                {
                    cro::ConfigFile cfg;
                    if (cfg.loadFromFile(path + file, false))
                    {
                        auto info = readBallCfg(cfg);
                        info.modelPath = path + info.modelPath;
                        info.workshopID = findWorkshopID(path);

                        insertInfo(info, m_sharedData.ballInfo, false);
                    }

                    break; //skip the rest of the file list
                }
            }
        }
    }


    //read in the info for unlockable balls - if valid
    //info is unlocked add it to ballModels now so it appears
    //in the preview window
    //else add it to the list afterwards so it can be rendered
    //in game, but not selected by the player
    const std::array SpecialPaths =
    {
        std::string("assets/golf/special/10.spec"),
        std::string("assets/golf/special/20.spec"),
        std::string("assets/golf/special/30.spec"),
        std::string("assets/golf/special/40.spec"),
        std::string("assets/golf/special/50.spec"),
        std::string("assets/golf/special/100.spec"),
    };
    constexpr std::array SpecialLevels =
    {
        10,20,30,40,50,100
    };
    const std::uint32_t level = Social::getLevel();
    std::vector<SharedStateData::BallInfo> delayedEntries;
    for (auto i = 0u; i < SpecialPaths.size(); ++i)
    {
        cro::ConfigFile cfg;
        if (cfg.loadFromFile(SpecialPaths[i]))
        {
            auto info = readBallCfg(cfg);

            //if we didn't find a UID create one from the file name and save it to the cfg
            if (info.uid == 0)
            {
                info.uid = SpookyHash::Hash32(SpecialPaths[i].data(), SpecialPaths[i].size(), 0);
                cfg.addProperty("uid").setValue(info.uid);
                cfg.save(SpecialPaths[i]);
            }

            if (level >= SpecialLevels[i])
            {
                insertInfo(info, m_sharedData.ballInfo, true);
            }
            else
            {
                info.locked = true;
                delayedEntries.push_back(info);
            }
        }
    }


    //load each model for the preview in the player menu
    cro::ModelDefinition ballDef(m_resources);
    m_profileData.shadowDef = std::make_unique<cro::ModelDefinition>(m_resources);
    m_profileData.grassDef = std::make_unique<cro::ModelDefinition>(m_resources);

    auto shadow = m_profileData.shadowDef->loadFromFile("assets/golf/models/ball_shadow.cmt");
    auto grass = m_profileData.grassDef->loadFromFile("assets/golf/models/ball_plane.cmt");

    std::vector<std::uint32_t> invalidBalls;

    for (auto i = 0u; i < m_sharedData.ballInfo.size(); ++i)
    {
        if (ballDef.loadFromFile(m_sharedData.ballInfo[i].modelPath))
        {
            auto entity = m_backgroundScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ (i * BallSpacing) + RootPoint, 0.f, 0.f });

            auto baseEnt = entity;

            entity = m_backgroundScene.createEntity();
            entity.addComponent<cro::Transform>();
            ballDef.createModel(entity);

            //clamp scale of balls in case someone got funny with a large model
            const float scale = std::min(1.f, MaxBallRadius / entity.getComponent<cro::Model>().getBoundingSphere().radius);
            entity.getComponent<cro::Transform>().setScale(glm::vec3(scale));

            //allow for double sided balls.
            auto material = m_resources.materials.get(m_materialIDs[MaterialID::Cel]);
            applyMaterialData(ballDef, material);
            entity.getComponent<cro::Model>().setMaterial(0, material);
            if (entity.getComponent<cro::Model>().getMeshData().submeshCount > 1)
            {
                material = m_resources.materials.get(m_materialIDs[MaterialID::Trophy]);
                applyMaterialData(ballDef, material);
                entity.getComponent<cro::Model>().setMaterial(1, material);
            }
            entity.getComponent<cro::Model>().setRenderFlags(BallRenderFlags);
            baseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            entity.addComponent<cro::Callback>().active = true;
            if (m_sharedData.ballInfo[i].rollAnimation)
            {
                entity.getComponent<cro::Callback>().function =
                    [](cro::Entity e, float dt)
                {
                    e.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -dt * 6.f);
                };
                entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -cro::Util::Const::PI * 0.85f);
                entity.getComponent<cro::Transform>().move({ 0.f, Ball::Radius, 0.f });
                entity.getComponent<cro::Transform>().setOrigin({ 0.f, Ball::Radius, 0.f });
            }
            else
            {
                entity.getComponent<cro::Callback>().function =
                    [](cro::Entity e, float dt)
                {
                    e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
                };
            }

            m_profileData.ballDefs.push_back(ballDef); //store this for faster loading in profile editor
          
            if (shadow)
            {
                entity = m_backgroundScene.createEntity();
                entity.addComponent<cro::Transform>();
                m_profileData.shadowDef->createModel(entity);
                baseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                entity.getComponent<cro::Model>().setRenderFlags(BallRenderFlags);
            }

            if (grass)
            {
                entity = m_backgroundScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ 0.f, -0.001f, 0.f });
                entity.getComponent<cro::Transform>().setScale(glm::vec3(5.f));
                m_profileData.grassDef->createModel(entity);
                baseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                entity.getComponent<cro::Model>().setRenderFlags(BallRenderFlags);
            }
        }
        else
        {
            //probably should remove from the ball models vector so that it's completely vetted
            invalidBalls.push_back(m_sharedData.ballInfo[i].uid);
            i--; //spacing is based on this and we don't want a gap from a bad ball
        }
    }

    //add delayed entries for in-game rendering
    for (const auto& info : delayedEntries)
    {
        insertInfo(info, m_sharedData.ballInfo, true);
    }

    //tidy up bad balls.
    for (auto uid : invalidBalls)
    {
        m_sharedData.ballInfo.erase(std::remove_if(m_sharedData.ballInfo.begin(), m_sharedData.ballInfo.end(),
            [uid](const SharedStateData::BallInfo& ball)
            {
                return ball.uid == uid;
            }), m_sharedData.ballInfo.end());
    }

}

std::int32_t MenuState::indexFromBallID(std::uint32_t ballID)
{
    auto ball = std::find_if(m_sharedData.ballInfo.begin(), m_sharedData.ballInfo.end(),
        [ballID](const SharedStateData::BallInfo& ballPair)
        {
            return ballPair.uid == ballID;
        });

    if (ball != m_sharedData.ballInfo.end())
    {
        return static_cast<std::int32_t>(std::distance(m_sharedData.ballInfo.begin(), ball));
    }

    return 0;
}

void MenuState::updateProfileTextures(std::size_t start, std::size_t count)
{
    if (count == 0)
    {
        //we might have passed in 0 as a param if it was derived from an empty vector
        return;
    }

    CRO_ASSERT(start < m_profileData.playerProfiles.size() && start + count <= m_profileData.playerProfiles.size(), "");

    for (auto i = start; i < start + count; ++i)
    {
        const auto& flags = m_profileData.playerProfiles[i].avatarFlags;
        for (auto j = 0u; j < pc::ColourKey::Count; ++j)
        {
            m_profileTextures[i].setColour(pc::ColourKey::Index(j), flags[j]);
        }

        m_profileTextures[i].apply();
    }
}

void MenuState::parseAvatarDirectory()
{
    m_sharedData.avatarInfo.clear();

    const std::string AvatarPath = "assets/golf/avatars/";
    auto files = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + AvatarPath);
    m_playerAvatars.reserve(files.size());
    processAvatarList(files, AvatarPath);

    //custom avatars
    auto avatarUserDir = Social::getUserContentPath(Social::UserContent::Avatar);
    if (cro::FileSystem::directoryExists(avatarUserDir))
    {
        auto dirs = cro::FileSystem::listDirectories(avatarUserDir);
        dirs.resize(std::min(dirs.size(), std::size_t(24)));//arbitrary limit
        for (const auto& dir : dirs)
        {
            auto resourceDir = avatarUserDir + dir + "/";
            files = cro::FileSystem::listFiles(resourceDir);
            processAvatarList(files, resourceDir, resourceDir);
        }
    }


    //load hair models
    m_sharedData.hairInfo.clear();

    //push an empty model on the front so index 0 is always no hair
    m_sharedData.hairInfo.emplace_back(0, "");
    for (auto& avatar : m_playerAvatars)
    {
        avatar.hairModels.emplace_back();
    }

    const std::string HairPath = "assets/golf/avatars/hair/";
    auto hairFiles = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + HairPath);

    if (hairFiles.size() > ConstVal::MaxHeadwear)
    {
        hairFiles.resize(ConstVal::MaxHeadwear);
    }

    for (const auto& file : hairFiles)
    {
        if (cro::FileSystem::getFileExtension(file) != ".hct")
        {
            continue;
        }

        cro::ConfigFile cfg;
        if (cfg.loadFromFile(HairPath + file))
        {
            auto info = readHairCfg(cfg);
            insertInfo(info, m_sharedData.hairInfo, true);

            //if uid is missing write it to cfg - although this doesn't work on apple bundles
            if (info.uid == 0)
            {
                info.uid = SpookyHash::Hash32(file.data(), file.size(), 0);
                cfg.addProperty("uid").setValue(info.uid);
                cfg.save(HairPath + file);
            }
        }
    }

    const auto userHairPath = Social::getUserContentPath(Social::UserContent::Hair);
    if (cro::FileSystem::directoryExists(userHairPath))
    {
        auto userDirs = cro::FileSystem::listDirectories(userHairPath);

        if (userDirs.size() > ConstVal::MaxHeadwear)
        {
            userDirs.resize(ConstVal::MaxHeadwear);
        }

        for (const auto& userDir : userDirs)
        {
            const auto userPath = userHairPath + userDir + "/";
            hairFiles = cro::FileSystem::listFiles(userPath);

            for (const auto& file : hairFiles)
            {
                if (cro::FileSystem::getFileExtension(file) != ".hct")
                {
                    continue;
                }

                cro::ConfigFile cfg;
                if (cfg.loadFromFile(userPath + file, false))
                {
                    auto info = readHairCfg(cfg);
                    info.modelPath = userPath + info.modelPath;
                    info.workshopID = findWorkshopID(userPath);

                    insertInfo(info, m_sharedData.hairInfo, false);
                    break; //only load one...
                }
            }
        }
    }

    //these are just used in the player preview window
    //not in game.
    if (!m_playerAvatars.empty())
    {
        cro::ModelDefinition md(m_resources);
        for (const auto& info : m_sharedData.hairInfo)
        {
            if (!info.modelPath.empty() && //first entry is 'bald' ie no model
                md.loadFromFile(info.modelPath))
            {
                for (auto& avatar : m_playerAvatars)
                {
                    auto& modelInfo = avatar.hairModels.emplace_back();
                    modelInfo.model = m_avatarScene.createEntity();
                    modelInfo.model.addComponent<cro::Transform>();
                    md.createModel(modelInfo.model);

                    modelInfo.model.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::Hair]));
                    modelInfo.model.getComponent<cro::Model>().setHidden(true);

                    modelInfo.uid = info.uid;
                }

                m_profileData.hairDefs.push_back(md);
            }
        }
    }


    //for every profile create a texture for the preview
    for (auto& profile : m_profileData.playerProfiles)
    {
        if (profile.skinID == 0)
        {
            //use first valid skin
            profile.skinID = m_sharedData.avatarInfo[0].uid;
        }

        //compare against list of unlocked balls and make sure we're in it
        auto ballID = indexFromBallID(profile.ballID);
        if (ballID >= m_profileData.ballDefs.size()
            || ballID == -1)
        {
            profile.ballID = 0;
        }

        m_profileTextures.emplace_back(m_sharedData.avatarInfo[indexFromAvatarID(profile.skinID)].texturePath);

        if (!profile.mugshot.empty())
        {
            m_profileTextures.back().setMugshot(profile.mugshot);
        }
    }
    updateProfileTextures(0, m_profileTextures.size());

    createAvatarScene();
}

void MenuState::processAvatarList(const std::vector<std::string>& fileList, const std::string& searchPath, const std::string resourcePath)
{
    //path strings must include trailing "/"!!
    for (const auto& file : fileList)
    {
        if (cro::FileSystem::getFileExtension(file) == ".avt")
        {
            cro::ConfigFile cfg;
            if (cfg.loadFromFile(searchPath + file))
            {
                SharedStateData::AvatarInfo info;

                const auto& props = cfg.getProperties();
                for (const auto& prop : props)
                {
                    const auto& name = prop.getName();
                    if (name == "model")
                    {
                        info.modelPath = resourcePath + prop.getValue<std::string>();
                        if (!info.modelPath.empty())
                        {
                            cro::ConfigFile modelData;
                            modelData.loadFromFile(info.modelPath);
                            for (const auto& o : modelData.getObjects())
                            {
                                if (o.getName() == "material")
                                {
                                    for (const auto& p : o.getProperties())
                                    {
                                        if (p.getName() == "diffuse")
                                        {
                                            info.texturePath = resourcePath + p.getValue<std::string>();
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else if (name == "audio")
                    {
                        info.audioscape = prop.getValue<std::string>();
                    }
                    else if (name == "uid")
                    {
                        info.uid = prop.getValue<std::uint32_t>();
                    }
                }

                if (!info.modelPath.empty())
                {
                    //TODO we probably want to remove this
                    //and reject files instead as improperly authored
                    if (info.uid == 0)
                    {
                        //create a uid from the file name and save it to the cfg
                        //uses Bob Jenkins' spooky hash
                        info.uid = SpookyHash::Hash32(file.data(), file.size(), 0);
                        cfg.addProperty("uid").setValue(info.uid);
                        cfg.save(searchPath + file);
                    }

                    //check uid doesn't exist
                    auto result = std::find_if(m_sharedData.avatarInfo.begin(), m_sharedData.avatarInfo.end(),
                        [&info](const SharedStateData::AvatarInfo& i)
                        {
                            return info.uid == i.uid;
                        });

                    if (result == m_sharedData.avatarInfo.end())
                    {
                        info.workshopID = findWorkshopID(searchPath);
                        m_sharedData.avatarInfo.push_back(info);
                        m_playerAvatars.emplace_back(info.texturePath);
                    }
                    else
                    {
                        LogW << "Avatar with UID " << info.uid << " already exists. " << info.modelPath << " will be skipped." << std::endl;
                    }
                }
                else
                {
                    LogW << "Skipping " << file << ": missing or corrupt data, or not an avatar." << std::endl;
                }
            }
        }
    }
}

void MenuState::createAvatarScene()
{
    auto avatarTexCallback = [&](cro::Camera& cam)
    {
        //auto windowSize = static_cast<float>(cro::App::getWindow().getSize().x);

        float windowScale = getViewScale();
        float scale = m_sharedData.pixelScale ? windowScale : 1.f;
        auto size = AvatarPreviewSize * static_cast<std::uint32_t>((windowScale + 1.f) - scale);
        m_avatarTexture.create(size.x, size.y);

        /*static constexpr float ratio = static_cast<float>(AvatarPreviewSize.y) / AvatarPreviewSize.x;
        cam.setPerspective(70.f, ratio, 0.1f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };*/
    };

    auto avatarCam = m_avatarScene.createEntity();
    avatarCam.addComponent<cro::Transform>().setPosition({ 0.f, 0.7f, 1.3f });
    //avatarCam.addComponent<cro::Camera>().setPerspective(75.f * cro::Util::Const::degToRad, static_cast<float>(AvatarPreviewSize.x) / AvatarPreviewSize.y, 0.001f, 10.f);

    static constexpr float ratio = static_cast<float>(AvatarPreviewSize.y) / AvatarPreviewSize.x;
    static constexpr float orthoWidth = 0.7f;
    auto orthoSize = glm::vec2(orthoWidth, orthoWidth * ratio);
    avatarCam.addComponent<cro::Camera>().setOrthographic(-orthoSize.x, orthoSize.x, -orthoSize.y, orthoSize.y, 0.001f, 10.f);
    avatarCam.getComponent<cro::Camera>().resizeCallback = avatarTexCallback;
    avatarTexCallback(avatarCam.getComponent<cro::Camera>());

    m_avatarScene.setActiveCamera(avatarCam);

    auto sunEnt = m_avatarScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -40.56f * cro::Util::Const::degToRad);

    //load the preview models
    cro::ModelDefinition clubDef(m_resources);
    clubDef.loadFromFile("assets/golf/models/club_iron.cmt");

    cro::ModelDefinition md(m_resources);
    for (auto i = 0u; i < m_sharedData.avatarInfo.size(); ++i)
    {
        if (md.loadFromFile(m_sharedData.avatarInfo[i].modelPath))
        {
            auto entity = m_avatarScene.createEntity();
            entity.addComponent<cro::Transform>().setOrigin(glm::vec2(-0.34f, 0.f));
            entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
            md.createModel(entity);

            //TODO account for multiple materials? Avatar is set to
            //only update a single image though, so 1 material should
            //be a hard limit.
            auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedSkinned]);
            applyMaterialData(md, material);
            entity.getComponent<cro::Model>().setMaterial(0, material);

            if (entity.hasComponent<cro::Skeleton>())
            {
                auto id = entity.getComponent<cro::Skeleton>().getAttachmentIndex("hands");
                if (id > -1)
                {
                    auto e = m_avatarScene.createEntity();
                    e.addComponent<cro::Transform>();
                    clubDef.createModel(e);

                    material = m_resources.materials.get(m_materialIDs[MaterialID::Cel]);
                    applyMaterialData(clubDef, material);
                    material.doubleSided = true; //we could update front face with parent model but meh
                    e.getComponent<cro::Model>().setMaterial(0, material);

                    entity.getComponent<cro::Skeleton>().getAttachments()[id].setModel(e);
                }
                else
                {
                    //no hands, no good
                    LogE << cro::FileSystem::getFileName(m_sharedData.avatarInfo[i].modelPath) << ": no hands attachment found, avatar not loaded" << std::endl;
                    m_avatarScene.destroyEntity(entity);
                    m_sharedData.avatarInfo[i].modelPath.clear();
                }

                id = entity.getComponent<cro::Skeleton>().getAttachmentIndex("head");
                if (id > -1)
                {
                    //hair is optional so OK if this doesn't exist
                    m_playerAvatars[i].hairAttachment = &entity.getComponent<cro::Skeleton>().getAttachments()[id];
                }

                //TODO fail to load if there's no animations? This shouldn't
                //be game breaking if there are none, it'll just look wrong.
            }
            else
            {
                //no skeleton, no good (see why this works, below)
                LogE << cro::FileSystem::getFileName(m_sharedData.avatarInfo[i].modelPath) << ": no skeleton found, avatar not loaded" << std::endl;
                m_avatarScene.destroyEntity(entity);
                m_sharedData.avatarInfo[i].modelPath.clear();
            }

            m_playerAvatars[i].previewModel = entity;

            m_profileData.avatarDefs.push_back(md);
        }
        else
        {
            //we'll use this to signify failure by clearing
            //the string and using that to erase the entry
            //from the avatar list (below). An empty avatar 
            //list will display an error on the menu and stop
            //the game from being playable.
            m_sharedData.avatarInfo[i].modelPath.clear();
            LogE << m_sharedData.avatarInfo[i].modelPath << ": model not loaded!" << std::endl;
        }
    }

    //remove all info with no valid path
    m_sharedData.avatarInfo.erase(std::remove_if(
        m_sharedData.avatarInfo.begin(),
        m_sharedData.avatarInfo.end(),
        [](const SharedStateData::AvatarInfo& ai)
        {
            return ai.modelPath.empty();
        }),
        m_sharedData.avatarInfo.end());

    //remove all models with no valid preview
    m_playerAvatars.erase(std::remove_if(m_playerAvatars.begin(), m_playerAvatars.end(),
        [](const PlayerAvatar& a)
        {
            return !a.previewModel.isValid();
        }),
        m_playerAvatars.end());

    //if any remain, load the preview audio
    const std::array<std::string, 3u> emitterNames =
    {
        "bunker", "fairway", "green"
    };
    for (auto i = 0u; i < m_playerAvatars.size(); ++i)
    {
        cro::AudioScape as;
        if (!m_sharedData.avatarInfo[i].audioscape.empty() &&
            as.loadFromFile(m_sharedData.avatarInfo[i].audioscape, m_resources.audio))
        {
            for (const auto& name : emitterNames)
            {
                if (as.hasEmitter(name))
                {
                    auto entity = m_uiScene.createEntity();
                    entity.addComponent<cro::Transform>();
                    entity.addComponent<cro::AudioEmitter>() = as.getEmitter(name);
                    entity.getComponent<cro::AudioEmitter>().setLooped(false);
                    m_playerAvatars[i].previewSounds.push_back(entity);
                }
            }
        }

        //we add the animation here as we need to capture the index which may have
        //changed between loading the model and erasing invalid avatars
        auto entity = m_playerAvatars[i].previewModel;
        entity.addComponent<cro::Callback>().setUserData<AvatarAnimCallbackData>();
        entity.getComponent<cro::Callback>().function =
            [&,i](cro::Entity e, float dt) mutable
        {
            const float Speed = dt * 4.f;

            auto& [direction, progress] = e.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>();
            if (direction == 0)
            {
                //grow
                progress = std::min(1.f, progress + Speed);
                if (progress == 1)
                {
                    direction = 1;
                    e.getComponent<cro::Callback>().active = false;
                }
            }
            else
            {
                //shrink
                progress = std::max(0.f, progress - (Speed /** 2.f*/));
                if (progress == 0)
                {
                    direction = 0;
                    e.getComponent<cro::Callback>().active = false;

                    e.getComponent<cro::Model>().setHidden(true);

                    //hide hair model if it's attached
                    if (m_playerAvatars[i].hairAttachment != nullptr
                        && m_playerAvatars[i].hairAttachment->getModel().isValid())
                    {
                        m_playerAvatars[i].hairAttachment->getModel().getComponent<cro::Model>().setHidden(true);
                    }
                }
            }

            auto scale = e.getComponent<cro::Transform>().getScale();
            auto facing = cro::Util::Maths::sgn(scale.x);
            scale = { progress * facing, /*progress*/1.f, progress };
            e.getComponent<cro::Transform>().setScale(scale);
        };
    }
}

std::int32_t MenuState::indexFromAvatarID(std::uint32_t id)
{
    auto avatar = std::find_if(m_sharedData.avatarInfo.begin(), m_sharedData.avatarInfo.end(),
        [id](const SharedStateData::AvatarInfo& info)
        {
            return info.uid == id;
        });

    if (avatar != m_sharedData.avatarInfo.end())
    {
        return static_cast<std::int32_t>(std::distance(m_sharedData.avatarInfo.begin(), avatar));
    }

    return 0;
}

void MenuState::ugcInstalledHandler(std::uint64_t id, std::int32_t type)
{
    //called when UGC such as a ball or hair model is received
    //from a remote player and downloaded locally.
    if (type == Social::UserContent::Ball)
    {
        //these won't appear as selectable until the menu is quit
        //and reloaded, but that's probably OK. They just need to
        //exist in the shared data so the main game can find the
        //models for remote players who have them.
        const auto BallUserPath = Social::getUserContentPath(Social::UserContent::Ball) + std::to_string(id) + "w/";

        //this can happen sometimes when the UGC fails to install (usually linux)
        //so quit so we don't throw
        if (!cro::FileSystem::directoryExists(BallUserPath))
        {
            LogE << "Couldn't find UGC at " << BallUserPath << std::endl;
            return;
        }
        
        auto files = cro::FileSystem::listFiles(BallUserPath);
        LogI << "installed remote ball" << std::endl;
        for (const auto& file : files)
        {
            if (cro::FileSystem::getFileExtension(file) == ".ball")
            {
                cro::ConfigFile cfg;
                if (cfg.loadFromFile(BallUserPath + file, false))
                {
                    auto info = readBallCfg(cfg);
                    info.modelPath = BallUserPath + info.modelPath;
                    info.workshopID = id;

                    insertInfo(info, m_sharedData.ballInfo, false);
                }

                break; //skip the rest of the file list
            }
        }
    }
    else if (type == Social::UserContent::Hair)
    {
        const auto HairUserPath = Social::getUserContentPath(Social::UserContent::Hair) + std::to_string(id) + "w/";

        if (!cro::FileSystem::directoryExists(HairUserPath))
        {
            LogE << "Couldn't find UGC at " << HairUserPath << std::endl;
            return;
        }
        auto files = cro::FileSystem::listFiles(HairUserPath);

        LogI << "installed remote hair" << std::endl;
        for (const auto& file : files)
        {
            if (cro::FileSystem::getFileExtension(file) == ".hct")
            {
                cro::ConfigFile cfg;
                if (cfg.loadFromFile(HairUserPath + file, false))
                {
                    auto info = readHairCfg(cfg);
                    info.modelPath = HairUserPath + info.modelPath;
                    info.workshopID = id;

                    insertInfo(info, m_sharedData.hairInfo, false);
                }

                break; //skip the rest of the file list
            }
        }
    }
    else if (type == Social::UserContent::Avatar)
    {
        //insert into m_sharedData.avatarInfo so GolfState can find it
        const auto& avatarPath = Social::getUserContentPath(Social::UserContent::Avatar) + std::to_string(id) + "w/";

        if (!cro::FileSystem::directoryExists(avatarPath))
        {
            LogE << "Couldn't find UGC at " << avatarPath << std::endl;
            return;
        }

        auto files = cro::FileSystem::listFiles(avatarPath);
        processAvatarList(files, avatarPath, avatarPath);
        LogI << "Installed remote avatar" << std::endl;
        //this just updates all the textures including the newly acquired
        //avatar data - there's room for optimisation here.
        updateLobbyAvatars();
    }
    else
    {
        LogE << "Unknown UGC: " << id << ", " << type << std::endl;
    }
}

std::int32_t MenuState::indexFromHairID(std::uint32_t id)
{
    //assumes all avatars contain some list of models...
    //not sure why we aren't doing this on m_sharedData.hairInfo ? 
    /*auto hair = std::find_if(m_playerAvatars[0].hairModels.begin(), m_playerAvatars[0].hairModels.end(),
        [id](const PlayerAvatar::HairInfo& h)
        {
            return h.uid == id;
        });

    if (hair != m_playerAvatars[0].hairModels.end())
    {
        return static_cast<std::int32_t>(std::distance(m_playerAvatars[0].hairModels.begin(), hair));
    }*/


    auto hair = std::find_if(m_sharedData.hairInfo.begin(), m_sharedData.hairInfo.end(),
        [id](const SharedStateData::HairInfo& h)
        {
            return h.uid == id;
        });

    if (hair != m_sharedData.hairInfo.end())
    {
        return static_cast<std::int32_t>(std::distance(m_sharedData.hairInfo.begin(), hair));
    }

    return 0;
}
