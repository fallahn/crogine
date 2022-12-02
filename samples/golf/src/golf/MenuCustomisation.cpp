/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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
#include "spooky2.hpp"
#include "BallSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/util/Random.hpp>

namespace
{
#include "RandNames.hpp"
}

void MenuState::createBallScene()
{
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

    for (auto i = 0u; i < m_ballThumbCams.size(); ++i)
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ RootPoint, 0.045f, 0.095f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.03f);
        entity.addComponent<cro::Camera>().setPerspective(0.89f, 2.f, 0.001f, 2.f);
        entity.getComponent<cro::Camera>().viewport = { (i % 2) * 0.5f, (i / 2) * 0.5f, 0.5f, 0.5f };
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<std::int32_t>(0);
        entity.getComponent<cro::Callback>().function = ballTargetCallback;
        m_ballThumbCams[i] = entity;
    }


    auto ballTexCallback = [&](cro::Camera&)
    {
        std::uint32_t samples = m_sharedData.pixelScale ? 0 :
            m_sharedData.antialias ? m_sharedData.multisamples : 0;

        auto vpSize = calcVPSize().y;
        auto windowSize = static_cast<float>(cro::App::getWindow().getSize().y);

        float windowScale = std::floor(windowSize / vpSize);
        float scale = m_sharedData.pixelScale ? windowScale : 1.f;
        
        auto invScale = static_cast<std::uint32_t>((windowScale + 1.f) - scale);
        auto size = BallPreviewSize * invScale;
        m_ballTexture.create(size, size, true, false, samples);

        size = BallThumbSize * invScale;
        m_ballThumbTexture.create(size * 4, size * 2, true, false, samples);
    };

    m_ballCam = m_backgroundScene.createEntity();
    m_ballCam.addComponent<cro::Transform>().setPosition({ RootPoint, 0.045f, 0.095f });
    m_ballCam.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.03f);
    m_ballCam.addComponent<cro::Camera>().setPerspective(1.f, 1.f, 0.001f, 2.f);
    m_ballCam.getComponent<cro::Camera>().resizeCallback = ballTexCallback;
    m_ballCam.addComponent<cro::Callback>().active = true;
    m_ballCam.getComponent<cro::Callback>().setUserData<std::int32_t>(0);
    m_ballCam.getComponent<cro::Callback>().function = ballTargetCallback;


    ballTexCallback(m_ballCam.getComponent<cro::Camera>());

    auto ballFiles = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + "assets/golf/balls");
    if (ballFiles.empty())
    {
        LogE << "No ball files were found" << std::endl;
    }

    m_sharedData.ballModels.clear();

    for (const auto& file : ballFiles)
    {
        cro::ConfigFile cfg;
        if (cro::FileSystem::getFileExtension(file) == ".ball"
            && cfg.loadFromFile("assets/golf/balls/" + file))
        {
            std::uint32_t uid = 0;// SpookyHash::Hash32(file.data(), file.size(), 0);
            std::string modelPath;
            cro::Colour colour = cro::Colour::White;

            const auto& props = cfg.getProperties();
            for (const auto& p : props)
            {
                const auto& name = p.getName();
                if (name == "model")
                {
                    modelPath = p.getValue<std::string>();
                }
                else if (name == "uid")
                {
                    uid = p.getValue<std::uint32_t>();
                }
                else if (name == "tint")
                {
                    colour = p.getValue<cro::Colour>();
                }
            }

            //if we didn't find a UID create one from the file name and save it to the cfg
            if (uid == 0)
            {
                uid = SpookyHash::Hash32(file.data(), file.size(), 0);
                cfg.addProperty("uid").setValue(uid);
                cfg.save("assets/golf/balls/" + file);
            }

            if (/*uid > -1
                &&*/ (!modelPath.empty() && cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + modelPath)))
            {
                auto ball = std::find_if(m_sharedData.ballModels.begin(), m_sharedData.ballModels.end(),
                    [uid](const SharedStateData::BallInfo& ballPair)
                    {
                        return ballPair.uid == uid;
                    });

                if (ball == m_sharedData.ballModels.end())
                {
                    m_sharedData.ballModels.emplace_back(colour, uid, modelPath);
                }
                else
                {
                    LogE << file << ": a ball already exists with UID " << uid << std::endl;
                }
            }
        }
    }

    cro::ModelDefinition ballDef(m_resources);

    cro::ModelDefinition shadowDef(m_resources);
    auto shadow = shadowDef.loadFromFile("assets/golf/models/ball_shadow.cmt");

    for (auto i = 0u; i < m_sharedData.ballModels.size(); ++i)
    {
        if (ballDef.loadFromFile(m_sharedData.ballModels[i].modelPath))
        {
            auto entity = m_backgroundScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ (i * BallSpacing) + RootPoint, 0.f, 0.f });
            ballDef.createModel(entity);

            //allow for double sided balls.
            auto material = m_resources.materials.get(m_materialIDs[MaterialID::Cel]);
            applyMaterialData(ballDef, material);
            entity.getComponent<cro::Model>().setMaterial(0, material);

            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, /*0.3f **/ dt);
            };

            if (shadow)
            {
                auto ballEnt = entity;
                entity = m_backgroundScene.createEntity();
                entity.addComponent<cro::Transform>();
                shadowDef.createModel(entity);
                ballEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            }
        }
    }
}

std::int32_t MenuState::indexFromBallID(std::uint32_t ballID)
{
    auto ball = std::find_if(m_sharedData.ballModels.begin(), m_sharedData.ballModels.end(),
        [ballID](const SharedStateData::BallInfo& ballPair)
        {
            return ballPair.uid == ballID;
        });

    if (ball != m_sharedData.ballModels.end())
    {
        return static_cast<std::int32_t>(std::distance(m_sharedData.ballModels.begin(), ball));
    }

    return -1;
}

void MenuState::parseAvatarDirectory()
{
    m_sharedData.avatarInfo.clear();

    const std::string AvatarPath = "assets/golf/avatars/";

    auto files = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + AvatarPath);
    m_playerAvatars.reserve(files.size());

    for (const auto& file : files)
    {
        if (cro::FileSystem::getFileExtension(file) == ".avt")
        {
            cro::ConfigFile cfg;
            if (cfg.loadFromFile(AvatarPath + file))
            {
                SharedStateData::AvatarInfo info;
                std::string texturePath;

                const auto& props = cfg.getProperties();
                for (const auto& prop : props)
                {
                    const auto& name = prop.getName();
                    if (name == "model")
                    {
                        info.modelPath = prop.getValue<std::string>();
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
                                            texturePath = p.getValue<std::string>();
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
                    if (info.uid == 0)
                    {
                        //create a uid from the file name and save it to the cfg
                        //uses Bob Jenkins' spooky hash
                        info.uid = SpookyHash::Hash32(file.data(), file.size(), 0);
                        cfg.addProperty("uid").setValue(info.uid);
                        cfg.save(AvatarPath + file);
                    }

                    //check uid doesn't exist
                    auto result = std::find_if(m_sharedData.avatarInfo.begin(), m_sharedData.avatarInfo.end(),
                        [&info](const SharedStateData::AvatarInfo& i)
                        {
                            return info.uid == i.uid;
                        });

                    if (result == m_sharedData.avatarInfo.end())
                    {
                        m_sharedData.avatarInfo.push_back(info);
                        m_playerAvatars.emplace_back(texturePath);
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


    //load hair models
    m_sharedData.hairInfo.clear();

    //push an empty model on the front so index 0 is always no hair
    m_sharedData.hairInfo.emplace_back(0, "");
    for (auto& avatar : m_playerAvatars)
    {
        avatar.hairModels.emplace_back();
    }

    const std::string HairPath = "assets/golf/avatars/hair/";
    const auto hairFiles = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + HairPath);

    cro::ModelDefinition md(m_resources);
    for (const auto& file : hairFiles)
    {
        if (cro::FileSystem::getFileExtension(file) != ".hct")
        {
            continue;
        }

        cro::ConfigFile cfg;
        if (cfg.loadFromFile(HairPath + file))
        {
            std::string modelPath;
            std::uint32_t uid = 0;

            const auto& props = cfg.getProperties();
            for (const auto& p : props)
            {
                const auto& name = p.getName();
                if (name == "model")
                {
                    auto model = p.getValue<std::string>();
                    if (cro::FileSystem::getFileExtension(model) == ".cmt"
                        && cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + model))
                    {
                        modelPath = model;
                    }
                }
                else if (name == "uid")
                {
                    uid = p.getValue<std::uint32_t>();
                }
            }

            if (md.loadFromFile(modelPath))
            {
                //if uid is missing write it to cfg
                if (uid == 0)
                {
                    uid = SpookyHash::Hash32(file.data(), file.size(), 0);
                    cfg.addProperty("uid").setValue(uid);
                    cfg.save(HairPath + file);
                }
                m_sharedData.hairInfo.emplace_back(uid, modelPath);

                for (auto& avatar : m_playerAvatars)
                {
                    auto& info = avatar.hairModels.emplace_back();
                    info.model = m_avatarScene.createEntity();
                    info.model.addComponent<cro::Transform>();
                    md.createModel(info.model);

                    info.model.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::Hair]));
                    info.model.getComponent<cro::Model>().setHidden(true);

                    info.uid = uid;
                }
            }
        }
    }


    if (!m_playerAvatars.empty())
    {
        for (auto i = 0u; i < ConnectionData::MaxPlayers; ++i)
        {
            m_avatarIndices[i] = indexFromAvatarID(m_sharedData.localConnectionData.playerData[i].skinID);
            m_hairIndices[i] = indexFromHairID(m_sharedData.localConnectionData.playerData[i].hairID);
        }
    }

    createAvatarScene();
}

void MenuState::createAvatarScene()
{
    auto avatarTexCallback = [&](cro::Camera&)
    {
        auto vpSize = calcVPSize().y;
        auto windowSize = static_cast<float>(cro::App::getWindow().getSize().y);

        float windowScale = std::floor(windowSize / vpSize);
        float scale = m_sharedData.pixelScale ? windowScale : 1.f;
        auto size = AvatarPreviewSize * static_cast<std::uint32_t>((windowScale + 1.f) - scale);
        m_avatarTexture.create(size.x, size.y);
    };

    auto avatarCam = m_avatarScene.createEntity();
    avatarCam.addComponent<cro::Transform>().setPosition({ 0.f, 0.649f, 1.3f });
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
            entity.getComponent<cro::Model>().setHidden(true);

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

void MenuState::applyAvatarColours(std::size_t playerIndex)
{
    auto avatarIndex = m_avatarIndices[playerIndex];

    const auto& flags = m_sharedData.localConnectionData.playerData[playerIndex].avatarFlags;
    m_playerAvatars[avatarIndex].setColour(pc::ColourKey::Bottom, flags[0]);
    m_playerAvatars[avatarIndex].setColour(pc::ColourKey::Top, flags[1]);
    m_playerAvatars[avatarIndex].setColour(pc::ColourKey::Skin, flags[2]);
    m_playerAvatars[avatarIndex].setColour(pc::ColourKey::Hair, flags[3]);

    m_playerAvatars[avatarIndex].setTarget(m_sharedData.avatarTextures[0][playerIndex]);
    m_playerAvatars[avatarIndex].apply();
}

void MenuState::updateThumb(std::size_t index)
{
    setPreviewModel(index);

    //we have to make sure model data is updated correctly before each
    //draw call as this func might be called in a loop to update multiple
    //avatars outside of the main update function.
    m_avatarScene.simulate(0.f);

    m_resolutionBuffer.bind(0);

    m_avatarThumbs[index].clear(cro::Colour::Transparent);
    //m_avatarThumbs[index].clear(cro::Colour::Magenta);
    m_avatarScene.render();
    m_avatarThumbs[index].display();
}

void MenuState::setPreviewModel(std::size_t playerIndex)
{
    auto index = m_avatarIndices[playerIndex];
    auto flipped = m_sharedData.localConnectionData.playerData[playerIndex].flipped;

    //hmm this would be quicker if we just tracked the active model...
    //in fact it might be contributing to the slow down when entering main lobby.
    for (auto i = 0u; i < m_playerAvatars.size(); ++i)
    {
        if (m_playerAvatars[i].previewModel.isValid()
            && m_playerAvatars[i].previewModel.hasComponent<cro::Model>())
        {
            m_playerAvatars[i].previewModel.getComponent<cro::Model>().setHidden(i != index);
            m_playerAvatars[i].previewModel.getComponent<cro::Transform>().setScale(glm::vec3(0.f));

            if (i == index)
            {
                if (flipped)
                {
                    m_playerAvatars[i].previewModel.getComponent<cro::Transform>().setScale({ -1.f, 1.f, 1.f });
                    m_playerAvatars[i].previewModel.getComponent<cro::Model>().setFacing(cro::Model::Facing::Back);
                }
                else
                {
                    m_playerAvatars[i].previewModel.getComponent<cro::Transform>().setScale({ 1.f, 1.f, 1.f });
                    m_playerAvatars[i].previewModel.getComponent<cro::Model>().setFacing(cro::Model::Facing::Front);
                }
                auto texID = cro::TextureID(m_sharedData.avatarTextures[0][playerIndex].getGLHandle());
                m_playerAvatars[i].previewModel.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", texID);

                //check to see if we have a hair model and apply its properties
                if (m_playerAvatars[i].hairAttachment != nullptr)
                {
                    if (m_playerAvatars[i].hairAttachment->getModel().isValid())
                    {
                        m_playerAvatars[i].hairAttachment->getModel().getComponent<cro::Model>().setHidden(true);
                        m_playerAvatars[i].hairAttachment->getModel().getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                    }

                    auto hairIndex = m_hairIndices[playerIndex];
                    m_playerAvatars[i].hairAttachment->setModel(m_playerAvatars[i].hairModels[hairIndex].model);


                    if (m_playerAvatars[i].hairModels[hairIndex].model.isValid())
                    {
                        m_playerAvatars[i].hairAttachment->getModel().getComponent<cro::Transform>().setScale(glm::vec3(1.f));
                        m_playerAvatars[i].hairModels[hairIndex].model.getComponent<cro::Model>().setHidden(false);
                        m_playerAvatars[i].hairModels[hairIndex].model.getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", m_playerAvatars[i].getColour(pc::ColourKey::Hair).first);
                        //m_playerAvatars[i].hairModels[hairIndex].model.getComponent<cro::Model>().setMaterialProperty(0, "u_darkColour", m_playerAvatars[i].getColour(pc::ColourKey::Hair).second);
                    }
                }
            }
        }
    }
}

std::int32_t MenuState::indexFromHairID(std::uint32_t id)
{
    //assumes all avatars contain some list of models...
    auto hair = std::find_if(m_playerAvatars[0].hairModels.begin(), m_playerAvatars[0].hairModels.end(),
        [id](const PlayerAvatar::HairInfo& h)
        {
            return h.uid == id;
        });

    if (hair != m_playerAvatars[0].hairModels.end())
    {
        return static_cast<std::int32_t>(std::distance(m_playerAvatars[0].hairModels.begin(), hair));
    }

    return 0;
}