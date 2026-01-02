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
#include "MessageIDs.hpp"

#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/gui/Gui.hpp>

using namespace cl;

void GolfState::createMinimapCamera()
{
    auto mapCam = m_mapScene.createEntity();
    mapCam.addComponent<cro::Transform>().setPosition({ static_cast<float>(MapSize.x) / 2.f, MinimapZoom::CamHeight, -static_cast<float>(MapSize.y) / 2.f });
    mapCam.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    auto& miniCam = mapCam.addComponent<cro::Camera>();
    miniCam.setOrthographic(-MapSizeFloat.x / 2.f, MapSizeFloat.x / 2.f, -MapSizeFloat.y / 2.f, MapSizeFloat.y / 2.f, 1.f, 40.f);
    miniCam.viewport = { 0.f, 0.f, 1.f, 1.f };

    m_minimapZoom.camera = mapCam;
    m_mapScene.setActiveCamera(mapCam);


    m_minimapZoom.sceneTexture.create(MapSize.x*2, MapSize.y*2, true, false/*, 2*/);
    m_minimapZoom.sceneTexture.setSmooth(true);

    m_sharedData.minimapData.mapScene = &m_mapScene;

    //registerWindow([&]() 
    //    {
    //        ImGui::Begin("Minimap");
    //        ImGui::Image(m_minimapZoom.sceneTexture.getTexture(), { MapSizeFloat.x, MapSizeFloat.y }, { 0.f, 1.f }, { 1.f, 0.f });
    //        ImGui::Text("Zoom %3.2f, %3.2f", m_minimapZoom.zoom, 1.f / m_minimapZoom.zoom);
    //        /*static float r = 0.f;
    //        if (ImGui::SliderFloat("X", &r, -3.f, 3.f))
    //        {
    //            m_minimapZoom.camera.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, r);
    //        }*/

    //        ImGui::End();
    //    });
}

void GolfState::updateMinimapTexture()
{
    //if (m_sharedData.scoreType == ScoreType::MultiTarget)
    //{
    //    const auto* shader = &m_resources.shaders.get(ShaderID::MinimapModel);
    //    m_targetShader.shaderID = shader->getGLHandle();
    //    m_targetShader.vpUniform = shader->getUniformID("u_targetViewProjectionMatrix");

    //    m_targetShader.size = 5.f; //we don't actually know what size has been chosen, so this is a rough average
    //    if (m_holeData[m_currentHole].puttFromTee)
    //    {
    //        m_targetShader.size *= 0.032f;
    //    }
    //    m_targetShader.position = m_holeData[m_currentHole].target;
    //    m_targetShader.update();
    //}

    //16 pass for 4x4 smaller renders
    /*glm::vec2 viewSize(MapSize);
    auto& miniCam = m_mapCam.getComponent<cro::Camera>();
    miniCam.setOrthographic(-viewSize.x / 8.f, viewSize.x / 8.f, -viewSize.y / 8.f, viewSize.y / 8.f, -0.1f, 60.f);
    miniCam.viewport = { 0.f, 0.f, 1.f/4.f, 1.f/4.f };*/

    if (m_currentHole > 0)
    {
        m_minimapModels[m_currentHole - 1].getComponent<cro::Model>().setHidden(true);
    }

    m_minimapModels[m_currentHole].getComponent<cro::Model>().setHidden(false);
    m_mapScene.getSystem<cro::CameraSystem>()->process(0.f); //updates the visibility of the models
    m_mapScene.getSystem<cro::ModelRenderer>()->process(0.f);


    //auto vCount = m_mapScene.getSystem<cro::ModelRenderer>()->getVisibleCount(m_mapCam.getComponent<cro::Camera>().getDrawListIndex());
    //LogI << "Visible count: " << vCount << std::endl;

    //auto entCount = m_mapScene.getSystem<cro::ModelRenderer>()->getEntities().size();
    //LogI << "Entity count: " << entCount << std::endl;


    cro::Colour c = cro::Colour::Transparent;
    //cro::Colour c(std::uint8_t(39), 56, 153);


    m_minimapTrail.getComponent<cro::Drawable2D>().getVertexData().clear();

    auto oldCam = m_mapScene.setActiveCamera(m_minimapZoom.camera2D);

    m_mapTextureMRT.clear(c);
    m_mapScene.render();
    //m_minimapModels[m_currentHole].getComponent<cro::Model>().setHidden(true);
    m_mapTextureMRT.display();

    m_mapScene.setActiveCamera(oldCam);

    ////m_mapTextureMRT.setBorderColour(c);



    //this triggers a map refresh so don't set it until
    //we know the texture is up to date.
    m_sharedData.minimapData.holeNumber = m_currentHole;

    retargetMinimap(true);

    auto* msg = postMessage<SceneEvent>(MessageID::SceneMessage);
    msg->type = SceneEvent::MinimapUpdated;

    /*if (m_sharedData.scoreType == ScoreType::MultiTarget)
    {
        m_targetShader.size = 0.f;
        m_targetShader.update();
    }*/
}

void GolfState::updateMiniMap()
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::MiniMap;
    cmd.action = [&](cro::Entity en, float)
        {
            //trigger animation - this does the actual render
            en.getComponent<cro::Callback>().active = true;
            //m_mapCam.getComponent<cro::Camera>().active = true;
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void GolfState::retargetMinimap(bool reset)
{
    if (m_minimapZoom.activeAnimation.isValid())
    {
        //remove existing animation
        m_minimapZoom.activeAnimation.getComponent<cro::Callback>().active = false;
        m_uiScene.destroyEntity(m_minimapZoom.activeAnimation);
        m_minimapZoom.activeAnimation = {};
    }
    struct MapZoomData final
    {
        struct
        {
            glm::vec2 pan = glm::vec2(0.f);
            float tilt = 0.f;
            float zoom = 1.f;
        }start, end;

        float progress = 0.f;
    }target;

    target.start.pan = m_minimapZoom.pan;
    target.start.tilt = m_minimapZoom.tilt;
    target.start.zoom = m_minimapZoom.zoom;

    static constexpr float MinZoom = 0.5f;
    static constexpr float MaxZoom = 32.f;

    if (reset)
    {
        //create a default view around the bounds of the hole model
        target.end.tilt = 0.f; //TODO this could be wound several times past TAU and should be only fmod this value

        auto bb = m_holeData[m_currentHole].modelEntity.getComponent<cro::Model>().getAABB();
        auto centre = bb.getCentre();
        target.end.pan = glm::vec2(centre.x, -centre.z) * m_minimapZoom.mapScale;

        auto xZoom = std::clamp(static_cast<float>(MiniMapSize.x) / ((bb[1].x - bb[0].x) * 1.6f), MinZoom, MaxZoom);
        auto zZoom = std::clamp(static_cast<float>(MiniMapSize.y) / ((bb[1].z - bb[0].z) * 1.6f), MinZoom, MaxZoom);
        target.end.zoom = xZoom > zZoom ? xZoom : zZoom;
    }
    else
    {
        bool isMultiTarget = (m_sharedData.scoreType == ScoreType::MultiTarget
            && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].targetHit);

        //find vec between player and flag
        const auto pin = isMultiTarget ? m_holeData[m_currentHole].target : m_holeData[m_currentHole].pin;
        const auto player = m_currentPlayer.position;

        //rotate minimap so flag is at top
        glm::vec2 dir(pin.x - player.x, -pin.z + player.z);
        auto rotation = std::atan2(-dir.y, dir.x) + cro::Util::Const::PI;
        target.end.tilt = m_minimapZoom.tilt + cro::Util::Maths::shortestRotation(m_minimapZoom.tilt, rotation);


        target.end.pan = glm::vec2(player.x, -player.z);

        //if we have a tight dogleg, such as on the mini putt
        //check if the primary target is in between and shift
        //towards it to better centre the hole
        const auto targ = findTargetPos(player);
        glm::vec2 targDir(targ.x - player.x, -targ.z + player.z);
        const auto dirNorm = glm::normalize(dir);
        const auto d = glm::dot(dirNorm, glm::normalize(targDir));
        const auto l2 = glm::length2(targDir);
        if (!isMultiTarget &&
            (d > 0 && d < 0.8f && l2 > 2.25f && l2 < glm::length2(dir)))
        {
            auto p = dir / 2.f;

            //find the distance to the target point and offset by perpendicular amount
            const glm::vec2 perpNormal(dirNorm.y, -dirNorm.x);
            const glm::vec2 targPos(targ.x, -targ.z);
            const float offset = /*std::abs*/((targPos.x - target.end.pan.x) * dirNorm.y - (targPos.y - target.end.pan.y) * dirNorm.x) / 2.f;
            p += perpNormal * offset;
            target.end.pan += p;
        }
        else
        {
            //centre view between player and flag
            target.end.pan += (dir / 2.f);
        }
        //(pan is in texture coords hum)
        target.end.pan *= m_minimapZoom.mapScale;

        //get distance between flag and player and expand by 1.4 (about 3m around a putting hole)
        //TODO this should be fixed 3m - as a percentage it's HUGE on big maps when fully zoomed
        const float viewLength = std::max(glm::length(dir), m_inputParser.getEstimatedDistance()) * 1.45f; //remember this is world coords

        //scale zoom on long edge of map by box length and clamp to 32x
        target.end.zoom = std::clamp(static_cast<float>(MiniMapSize.x) / viewLength, MinZoom, MaxZoom);
    }

    //create a temp ent to interp between start and end values
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<MapZoomData>(target);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<MapZoomData>();

            //const auto speed = 0.4f + (0.7f * (1.f - std::clamp(glm::length2(data.start.pan - data.end.pan) / (100.f * 100.f), 0.f, 1.f)));
            const auto speed = 0.4f + (0.7f * (1.f - std::clamp(glm::length(data.start.pan - data.end.pan) / 100.f, 0.f, 1.f)));
            data.progress = std::min(1.f, data.progress + (dt * speed));

            m_minimapZoom.pan = glm::mix(data.start.pan, data.end.pan, cro::Util::Easing::easeOutExpo(data.progress));
            m_minimapZoom.tilt = glm::mix(data.start.tilt, data.end.tilt, cro::Util::Easing::easeInOutBack(data.progress));
            m_minimapZoom.zoom = glm::mix(data.start.zoom, data.end.zoom, cro::Util::Easing::easeOutBack(data.progress));
            m_minimapZoom.updateShader();

            if (data.progress == 1)
            {
                m_minimapZoom.activeAnimation = {};
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
        };
    m_minimapZoom.activeAnimation = entity;
}



//minimap zoom struct
void MinimapZoom::updateShader()
{
    //TODO rename this once which switch from the shader
    //to the 3D camera completely
    CRO_ASSERT(glm::length2(textureSize) != 0, "");

    //the inverse matrix is calculated so we can convert
    //world positions to map coords in toMapCoords()
    static constexpr glm::vec3 centre(0.5f, 0.5f, 0.f);
    const auto pos = pan / textureSize;
    const float aspect = textureSize.x / textureSize.y;

    glm::mat4 matrix(1.f);
    matrix = glm::translate(matrix, glm::vec3(pos.x, pos.y, 0.f));
    matrix = glm::scale(matrix, glm::vec3(1.f / aspect, 1.f, 1.f));
    matrix = glm::rotate(matrix, -tilt, cro::Transform::Z_AXIS);
    matrix = glm::scale(matrix, glm::vec3(aspect, 1.f, 1.f));
    matrix = glm::scale(matrix, glm::vec3(1.f / zoom));
    matrix = glm::scale(matrix, glm::vec3(MapSizeRatio, 1.f));
    matrix = glm::translate(matrix, -centre);
    invTx = glm::inverse(matrix);

    //glUseProgram(shaderID);
    //glUniformMatrix4fv(matrixUniformID, 1, GL_FALSE, &matrix[0][0]);


    //update the 3D camera 
    auto& tx = camera.getComponent<cro::Transform>();
    const auto pos3D = pan / mapScale; //TODO once large tex is removed pan no longer needs to scale
    tx.setPosition({pos3D.x, CamHeight, -pos3D.y});
    tx.setRotation(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    tx.rotate(cro::Transform::Z_AXIS, -tilt);

    auto& cam = camera.getComponent<cro::Camera>();
    const glm::vec2 viewSize = ((MapSizeFloat / zoom) / 2.f) * MapSizeRatio; //MapSizeRatio scales the UI size of the minimap to the texture
    cam.setOrthographic(-viewSize.x, viewSize.x, -viewSize.y, viewSize.y, 1.f, 40.f);
    
    
    //problem is: the perspective mis-aligns icons on the minimap, the UI viewport of
    //the minimap has a different aspect to the texture, and something else I don't remember.
    /*constexpr float ratio = MapSizeFloat.x / MapSizeFloat.y;
    const auto fov = std::atan((((MapSizeFloat.y / 2.f) / zoom) * MapSizeRatio.y) / CamHeight) * 2.f;
    cam.setPerspective(fov, ratio, 10.f, 40.f);*/
}

glm::vec2 MinimapZoom::toMapCoords(glm::vec3 worldCoord) const
{
    CRO_ASSERT(glm::length2(textureSize) != 0, "");

    glm::vec2 mapCoord = glm::vec2(worldCoord.x, -worldCoord.z) * mapScale;
    mapCoord /= textureSize;
    mapCoord = glm::vec2(invTx * glm::vec4(mapCoord, 0.0, 1.0));
    return (mapCoord * textureSize);
}