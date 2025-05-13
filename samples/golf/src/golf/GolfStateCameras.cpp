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
#include "PacketIDs.hpp"
#include "FpsCameraSystem.hpp"
#include "CommandIDs.hpp"
#include "GameConsts.hpp"
#include "ClientCollisionSystem.hpp"
#include "BallSystem.hpp"
#include "CallbackData.hpp"
#include "MessageIDs.hpp"
#include "TextAnimCallback.hpp"
#include "Clubs.hpp"
#include "LightAnimationSystem.hpp"

#include <crogine/ecs/components/AudioListener.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/LightVolumeSystem.hpp>

#include <crogine/detail/glm/gtx/rotate_vector.hpp>
#include <crogine/util/Maths.hpp>

#include "../ErrorCheck.hpp"

using namespace cl;

namespace
{
    constexpr float DefaultZoomSpeed = 3.f;
    constexpr float PuttZoomSpeed = 20.f;

    //float Speed = 4.f;
    //float ZoomSpeed = 20.f;
}

const cro::Time GolfState::DefaultIdleTime = cro::seconds(180.f);

void GolfState::createCameras()
{
    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("speed"))
    //        {
    //            /*ImGui::SliderFloat("Speed", &Speed, 1.f, 8.f);
    //            ImGui::SliderFloat("Zoom Speed", &ZoomSpeed, 10.f, 50.f);*/
    //            static uint32_t prec = 0;
    //            if (ImGui::Button("Precision"))
    //            {
    //                prec = prec == 0 ? 1 : 0;
    //                m_gameSceneMRTexture.setPrecision(prec);
    //            }
    //        }
    //        ImGui::End();
    //    });

    //update the 3D view - applied on player cam and transition cam
    auto updateView = [&](cro::Camera& cam)
        {
            auto winSize = glm::vec2(cro::App::getWindow().getSize());
            float maxScale = getViewScale();
            float scale = m_sharedData.pixelScale ? maxScale : 1.f;
            auto texSize = winSize / scale;

            //only want to resize the buffer once !!
            if (cam == m_cameras[CameraID::Player].getComponent<cro::Camera>())
            {
                std::uint32_t samples = m_sharedData.pixelScale ? 0 :
                    m_sharedData.antialias ? m_sharedData.multisamples : 0;

                if (m_sharedData.nightTime)
                {
                    m_gameSceneMRTexture.setPrecision(m_sharedData.lightmapQuality);

                    glm::uvec2 usize(texSize);
                    m_sharedData.antialias =
                        m_gameSceneMRTexture.create(usize.x, usize.y, MRTIndex::Count)
                        && m_sharedData.multisamples != 0
                        && !m_sharedData.pixelScale;

                    m_renderTarget.clear = [&](cro::Colour c) { m_gameSceneMRTexture.clear(c); };
                    m_renderTarget.display = std::bind(&cro::MultiRenderTexture::display, &m_gameSceneMRTexture);
                    m_renderTarget.getSize = std::bind(&cro::MultiRenderTexture::getSize, &m_gameSceneMRTexture);

                    m_lightMaps[LightMapID::Scene].create(usize.x, usize.y, false/*, false, samples*/);

                    m_lightBlurTextures[LightMapID::Scene].create(usize.x / 4u, usize.y / 4u, false);
                    m_lightBlurTextures[LightMapID::Scene].setSmooth(true);
                    m_lightBlurQuads[LightMapID::Scene].setTexture(m_gameSceneMRTexture.getTexture(MRTIndex::Light), usize / 4u);
                    m_lightBlurQuads[LightMapID::Scene].setShader(m_resources.shaders.get(ShaderID::Blur));

                    m_focusTexture.create(usize.x / 4, usize.y / 4, false);
                    m_focusQuad.setTexture(m_gameSceneMRTexture.getTexture());
                }
                else
                {
                    cro::RenderTarget::Context ctx;
                    ctx.depthBuffer = true;
#ifdef __APPLE__
                    //*sigh*
                    ctx.depthTexture = false;
#else
                    ctx.depthTexture = true;
#endif
                    ctx.samples = samples;
                    ctx.width = static_cast<std::uint32_t>(texSize.x);
                    ctx.height = static_cast<std::uint32_t>(texSize.y);

                    //ctx.floatingPointStorage = true;

                    m_sharedData.antialias =
                        m_gameSceneTexture.create(ctx)
                        && m_sharedData.multisamples != 0
                        && !m_sharedData.pixelScale;
                    

                    m_renderTarget.clear = std::bind(&cro::RenderTexture::clear, &m_gameSceneTexture, std::placeholders::_1);
                    m_renderTarget.display = std::bind(&cro::RenderTexture::display, &m_gameSceneTexture);
                    m_renderTarget.getSize = std::bind(&cro::RenderTexture::getSize, &m_gameSceneTexture);

                    m_focusTexture.create(ctx.width / 4, ctx.height / 4, false);
                    m_focusQuad.setTexture(m_gameSceneTexture.getTexture());
                }
                m_focusTexture.setSmooth(true);
                m_focusQuad.setScale(glm::vec2(0.25f));
                m_focusQuad.setShader(m_resources.shaders.get(ShaderID::Blur));

                auto invScale = (maxScale + 1.f) - scale;
                glCheck(glPointSize(invScale * BallPointSize));
                glCheck(glLineWidth(invScale));

                m_scaleBuffer.setData(invScale);

                m_resolutionUpdate.resolutionData.resolution = texSize / invScale;
                m_resolutionUpdate.resolutionData.bufferResolution = texSize;
                m_resolutionBuffer.setData(m_resolutionUpdate.resolutionData);


                //this lets the shader scale leaf billboards correctly
                auto targetHeight = texSize.y;
                glUseProgram(m_resources.shaders.get(ShaderID::TreesetLeaf).getGLHandle());
                glUniform1f(m_resources.shaders.get(ShaderID::TreesetLeaf).getUniformID("u_targetHeight"), targetHeight);
            }

            //fetch this explicitly so the transition cam also gets the correct zoom
            float zoom = m_cameras[CameraID::Player].getComponent<CameraFollower::ZoomData>().fov;
            cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad * zoom, texSize.x / texSize.y, 0.1f, CameraFarPlane /** 1.25f*/, m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition(m_holeData[0].pin);
    camEnt.addComponent<CameraFollower::ZoomData>().target = 1.f; //used to set zoom when putting.
    camEnt.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& cam = e.getComponent<cro::Camera>();
            auto& zoom = e.getComponent<CameraFollower::ZoomData>();
            float diff = zoom.target - zoom.fov;

            CRO_ASSERT(zoom.target > 0, "");

            if (std::fabs(diff) > 0.001f)
            {
                zoom.fov += (diff * dt) * e.getComponent<cro::Callback>().getUserData<float>();
            }
            else
            {
                zoom.fov = zoom.target;
                e.getComponent<cro::Callback>().active = false;
            }

            auto fov = m_sharedData.fov * cro::Util::Const::degToRad * zoom.fov;
            cam.setPerspective(fov, cam.getAspectRatio(), 0.1f, CameraFarPlane, m_shadowQuality.cascadeCount);
            m_cameras[CameraID::Transition].getComponent<cro::Camera>().setPerspective(fov, cam.getAspectRatio(), 0.1f, CameraFarPlane, m_shadowQuality.cascadeCount);
        };
    camEnt.getComponent<cro::Callback>().setUserData<float>(DefaultZoomSpeed);

    m_cameras[CameraID::Player] = camEnt;
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.setRenderFlags(cro::Camera::Pass::Reflection, RenderFlags::Reflection);
    cam.setRenderFlags(cro::Camera::Pass::Final, RenderFlags::Main);
    cam.resizeCallback = updateView;
    updateView(cam);

    static constexpr std::uint32_t ReflectionMapSize = 1024u;
    const std::uint32_t ShadowMapSize = m_sharedData.hqShadows ? 4096u : 2048u;

    //used by transition callback to interp camera
    camEnt.addComponent<TargetInfo>().waterPlane = m_waterEnt;
    camEnt.getComponent<TargetInfo>().targetLookAt = m_holeData[0].target;
    cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    cam.reflectionBuffer.setSmooth(true);
    cam.shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    cam.setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    cam.setShadowExpansion(20.f);

    //create an overhead camera
    auto setPerspective = [&](cro::Camera& cam)
        {
            auto vpSize = glm::vec2(cro::App::getWindow().getSize());

            cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, vpSize.x / vpSize.y, 0.1f, CameraFarPlane, m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(DefaultSkycamPosition);
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam) //use explicit callback so we can capture the entity and use it to zoom via CamFollowSystem
        {
            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * camEnt.getComponent<CameraFollower>().zoom.fov,
                vpSize.x / vpSize.y, 0.1f, CameraFarPlane /** 1.25f*/,
                m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };

            //fades billboards with zoom
            if (m_currentCamera == CameraID::Sky)
            {
                m_resolutionUpdate.targetFade = m_currentPlayer.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;
                m_resolutionUpdate.targetFade += (ZoomFadeDistance / 2.f) + (camEnt.getComponent<CameraFollower>().zoom.progress * (ZoomFadeDistance / 2.f));
            }
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize / 2, ShadowMapSize / 2);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Reflection, RenderFlags::Reflection);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlags::Main);
    camEnt.addComponent<cro::CommandTarget>().ID = CommandID::SpectatorCam;
    camEnt.addComponent<CameraFollower>().radius = SkyCamRadius * SkyCamRadius;
    camEnt.getComponent<CameraFollower>().id = CameraID::Sky;
    camEnt.getComponent<CameraFollower>().zoom.target = 0.25f;// 0.1f;
    camEnt.getComponent<CameraFollower>().zoom.speed = SkyCamZoomSpeed;
    camEnt.getComponent<CameraFollower>().targetOffset = { 0.f,0.65f,0.f };
    camEnt.addComponent<cro::AudioListener>();

    //this holds the water plane ent when active
    camEnt.addComponent<TargetInfo>();
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Sky] = camEnt;


    //same as sky cam, but controlled by the active player
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(DefaultSkycamPosition);
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam)
        {
            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * camEnt.getComponent<CameraFollower::ZoomData>().fov,
                vpSize.x / vpSize.y, 0.1f, CameraFarPlane /** 1.25f*/,
                m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };

            //fades billboards with zoom
            if (m_currentCamera == CameraID::Drone)
            {
                m_resolutionUpdate.targetFade = m_currentPlayer.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;
                m_resolutionUpdate.targetFade += (ZoomFadeDistance / 2.f) + (camEnt.getComponent<CameraFollower::ZoomData>().progress * (ZoomFadeDistance / 2.f));
            }
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize / 2, ShadowMapSize / 2);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Reflection, RenderFlags::Reflection);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlags::Main);
    camEnt.addComponent<cro::CommandTarget>().ID = CommandID::DroneCam;
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<CameraFollower::ZoomData>();

    //this holds the water plane ent when active
    camEnt.addComponent<TargetInfo>();
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Drone] = camEnt;



    //and a green camera
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam)
        {
            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * camEnt.getComponent<CameraFollower>().zoom.fov,
                vpSize.x / vpSize.y, 0.1f, CameraFarPlane/* * 1.25f*/,
                m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Reflection, RenderFlags::Reflection);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlags::Main);
    camEnt.addComponent<cro::CommandTarget>().ID = CommandID::SpectatorCam;
    camEnt.addComponent<CameraFollower>().radius = GreenCamRadiusLarge * GreenCamRadiusLarge;
    camEnt.getComponent<CameraFollower>().id = CameraID::Green;
    camEnt.getComponent<CameraFollower>().zoom.speed = GreenCamZoomFast;
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Green] = camEnt;

    //bystander cam (when remote or CPU player is swinging)
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam)
        {
            //this cam has a slightly narrower FOV
            auto zoomFOV = camEnt.getComponent<cro::Callback>().getUserData<CameraFollower::ZoomData>().fov;

            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * zoomFOV * 0.7f,
                vpSize.x / vpSize.y, 0.1f, CameraFarPlane * 0.7f,
                m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowNearDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Reflection, RenderFlags::Reflection);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlags::Main);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();

    CameraFollower::ZoomData zoomData;
    zoomData.speed = 3.f;
    zoomData.target = 0.45f;
    camEnt.addComponent<cro::Callback>().setUserData<CameraFollower::ZoomData>(zoomData);
    camEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            auto& zoom = e.getComponent<cro::Callback>().getUserData<CameraFollower::ZoomData>();

            zoom.progress = std::min(1.f, zoom.progress + (dt * zoom.speed));
            zoom.fov = glm::mix(1.f, zoom.target, cro::Util::Easing::easeInOutQuad(zoom.progress));
            e.getComponent<cro::Camera>().resizeCallback(e.getComponent<cro::Camera>());

            if (zoom.progress == 1)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        };
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Bystander] = camEnt;


    //idle cam when player AFKs
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam)
        {
            //this cam has a slightly narrower FOV
            auto zoomFOV = camEnt.getComponent<cro::Callback>().getUserData<CameraFollower::ZoomData>().fov;

            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * zoomFOV * 0.7f,
                vpSize.x / vpSize.y, 0.1f, CameraFarPlane * 0.7f,
                m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowNearDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Reflection, RenderFlags::Reflection);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlags::Main);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();
    camEnt.addComponent<cro::Callback>().setUserData<CameraFollower::ZoomData>(zoomData); //needed by resize callback, but not used HUM
    camEnt.getComponent<cro::Callback>().active = true;
    camEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            if (e.getComponent<cro::Camera>().active)
            {
                static constexpr glm::vec3 TargetOffset(0.f, 1.f, 0.f);
                auto target = m_currentPlayer.position + TargetOffset;

                static float rads = 0.f;
                rads += (dt * 0.1f);
                glm::vec3 pos(std::cos(rads), 0.f, std::sin(rads));
                pos *= 5.f;
                pos += target;
                pos.y = m_collisionMesh.getTerrain(pos).height + 2.f;

                e.getComponent<cro::Transform>().setPosition(pos);
                e.getComponent<cro::Transform>().setRotation(lookRotation(pos, target));
            }
        };
    setPerspective(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().updateMatrices(camEnt.getComponent<cro::Transform>());
    m_cameras[CameraID::Idle] = camEnt;

    //fly-by cam for transition
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback = updateView;
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(45.f);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Reflection, RenderFlags::Reflection);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlags::Main);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();
    updateView(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Transition] = camEnt;


    //free cam
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam)
        {
            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective(m_sharedData.fov * camEnt.getComponent<FpsCamera>().fov * cro::Util::Const::degToRad,
                vpSize.x / vpSize.y, 0.1f, CameraFarPlane /** 1.25f*/,
                m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(15.f);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Reflection, RenderFlags::Reflection);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlags::Main);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<FpsCamera>();
    camEnt.addComponent<TargetInfo>();
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_freeCam = camEnt;

#ifdef PATH_TRACING
    initBallDebug();
#endif
    addCameraDebugging();


    const auto flightCamCallback =
        [&](cro::Camera& cam)
    {
            //we're using the same texture as the green view
            //so just set the camera properties
            cam.setPerspective(FlightCamFOV * cro::Util::Const::degToRad, 1.f, 0.06f, 200.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    //follows the ball in flight
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback = flightCamCallback;
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlags::FlightCam);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize / 4, ShadowMapSize / 4);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(/*m_shadowQuality.shadowFarDistance*/3.f);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(1.f);

    camEnt.addComponent<cro::Callback>().active = true;
    camEnt.getComponent<cro::Callback>().setUserData<cro::Entity>(); //this is the target ball
    camEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            //static constexpr float FollowFast = 0.009f;
            //static constexpr float FollowSlow = 0.5f;
            static constexpr float MinHeight = 0.08f;

            if (e.getComponent<cro::Camera>().active)
            {
                auto target = e.getComponent<cro::Callback>().getUserData<cro::Entity>();
                if (target.isValid())
                {
                    glm::vec3 dir(0.f);
                    auto targetPos = target.getComponent<cro::Transform>().getPosition();

                    if (target.getComponent<ClientCollider>().state != static_cast<std::uint8_t>(Ball::State::Idle))
                    {
                        dir = e.getComponent<cro::Transform>().getPosition() - targetPos;
                    }
                    else
                    {
                        dir = e.getComponent<cro::Transform>().getPosition() - m_holeData[m_currentHole].pin;
                    }
                    auto pos = targetPos + (glm::normalize(glm::vec3(dir.x, 0.f, dir.z)) * MinFlightCamDistance);
                    pos.y += MinHeight;

                    auto newPos = pos;// cro::Util::Maths::smoothDamp(e.getComponent<cro::Transform>().getPosition(), pos, vel, followSpeed, dt);

                    //clamp above ground
                    auto t = m_collisionMesh.getTerrain(newPos);
                    if (newPos.y < t.height + MinHeight)
                    {
                        newPos.y = t.height + MinHeight;
                    }
                    
                    //if (glm::length2(targetPos - newPos) > (MinFlightCamDistance * MinFlightCamDistance))
                    {
                        e.getComponent<cro::Transform>().setPosition(newPos);

                        auto  rotation = lookRotation(newPos, targetPos + glm::vec3(0.f, 0.04f, 0.f));
                        e.getComponent<cro::Transform>().setRotation(rotation);
                    }
                }
            }
        };
    flightCamCallback(camEnt.getComponent<cro::Camera>());
    m_flightCam = camEnt;



    //set up the skybox cameras so they can be updated with the relative active cams
    m_skyCameras[SkyCam::Main] = m_skyScene.getActiveCamera();
    m_skyCameras[SkyCam::Main].getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Reflection, RenderFlags::Reflection);
    m_skyCameras[SkyCam::Flight] = m_skyScene.createEntity();
    m_skyCameras[SkyCam::Flight].addComponent<cro::Transform>();
    auto& skyCam = m_skyCameras[SkyCam::Flight].addComponent<cro::Camera>();
    skyCam.setPerspective(FlightCamFOV * cro::Util::Const::degToRad, 1.f, 0.1f, 14.f);
    skyCam.viewport = { 0.f, 0.f, 1.f, 1.f };

}

void GolfState::setGreenCamPosition()
{
    auto holePos = m_holeData[m_currentHole].pin;

    if (m_holeData[m_currentHole].puttFromTee)
    {
        auto teePos = m_holeData[m_currentHole].tee;
        auto targetPos = m_holeData[m_currentHole].target;

        if ((glm::length2(m_currentPlayer.position - teePos) <
            glm::length2(m_currentPlayer.position - holePos))
            &&
            (glm::length2(m_currentPlayer.position - targetPos) >
                /*glm::length2(m_currentPlayer.position - holePos)*/16.f))
        {
            holePos = targetPos;
        }
    }

    m_cameras[CameraID::Green].getComponent<cro::Transform>().setPosition({ holePos.x, GreenCamHeight, holePos.z });

    float heightOffset = GreenCamHeight;

    if (m_holeData[m_currentHole].puttFromTee)
    {
        auto direction = holePos - m_holeData[m_currentHole].tee;
        direction = glm::normalize(direction) * 2.f;
        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = 3.5f * 3.5f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 1.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.5f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = 0.8f;
        heightOffset *= 0.18f;
    }
    else if (auto direction = holePos - m_currentPlayer.position; m_currentPlayer.terrain == TerrainID::Green)
    {
        direction = glm::normalize(direction) * 4.2f;

        auto r = (cro::Util::Random::value(0, 1) * 2) - 1;
        direction = glm::rotate(direction, 0.5f * static_cast<float>(r), glm::vec3(0.f, 1.f, 0.f));

        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = GreenCamRadiusSmall * GreenCamRadiusSmall;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 1.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.3f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = GreenCamZoomSlow;

        heightOffset *= 0.15f;
    }

    else if ((glm::length2(direction) < (50.f * 50.f)))
    {
        //player is within larger radius
        direction = glm::normalize(direction) * 5.5f;

        auto r = (cro::Util::Random::value(0, 1) * 2) - 1;
        direction = glm::rotate(direction, 0.5f * static_cast<float>(r), glm::vec3(0.f, 1.f, 0.f));

        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = GreenCamRadiusMedium * GreenCamRadiusMedium;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 9.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.5f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = GreenCamZoomSlow / 2.f;

        heightOffset *= 0.15f;
    }

    else
    {
        direction = glm::normalize(direction) * 15.f;

        auto r = (cro::Util::Random::value(0, 1) * 2) - 1;
        direction = glm::rotate(direction, 0.35f * static_cast<float>(r), glm::vec3(0.f, 1.f, 0.f));

        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = GreenCamRadiusLarge * GreenCamRadiusLarge;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 16.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.25f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = GreenCamZoomFast;
    }

    //double check terrain height and make sure camera is always above
    auto result = m_collisionMesh.getTerrain(m_cameras[CameraID::Green].getComponent<cro::Transform>().getPosition());
    result.height = std::max(result.height, holePos.y);
    result.height += heightOffset;

    auto pos = m_cameras[CameraID::Green].getComponent<cro::Transform>().getPosition();
    pos.y = result.height;

    m_cameras[CameraID::Green].getComponent<cro::Transform>().setRotation(lookRotation(pos, m_holeData[m_currentHole].pin));
    m_cameras[CameraID::Green].getComponent<cro::Transform>().setPosition(pos);
}

void GolfState::setActiveCamera(std::int32_t camID)
{
    m_sharedData.minimapData.active = (camID == CameraID::Player && !m_holeData[m_currentHole].puttFromTee);

    if (m_photoMode)
    {
        return;
    }

    CRO_ASSERT(camID >= 0 && camID < CameraID::Count, "");

    if (m_cameras[camID].isValid()
        && camID != m_currentCamera)
    {
        if (camID != CameraID::Player
            && (camID < m_currentCamera))
        {
            //don't switch back to the previous camera
            //ie if we're on the green cam don't switch
            //back to sky
            return;
        }

        //reset existing zoom
        if (m_cameras[m_currentCamera].hasComponent<CameraFollower>())
        {
            m_cameras[m_currentCamera].getComponent<CameraFollower>().reset(m_cameras[m_currentCamera]);
        }

        //set the water plane ent on the active camera
        if (m_cameras[m_currentCamera].getComponent<TargetInfo>().waterPlane.isValid())
        {
            m_cameras[m_currentCamera].getComponent<TargetInfo>().waterPlane = {};
        }
        m_cameras[m_currentCamera].getComponent<cro::Camera>().active = false;


        if (camID == CameraID::Player)
        {
            auto target = m_currentPlayer.position;
            m_waterEnt.getComponent<cro::Callback>().setUserData<glm::vec3>(target.x, WaterLevel, target.z);
        }

        //set scene camera
        m_gameScene.setActiveCamera(m_cameras[camID]);
        if (camID != CameraID::Sky
            && camID != CameraID::Drone)
        {
            m_gameScene.setActiveListener(m_cameras[camID]);

            //reset the fade distance which may have been modified by a zooming follower.
            m_resolutionUpdate.targetFade = m_currentPlayer.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;
            //m_resolutionUpdate.resolutionData.nearFadeDistance = m_resolutionUpdate.targetFade - 0.01f;
        }
        m_currentCamera = camID;

        m_cameras[m_currentCamera].getComponent<TargetInfo>().waterPlane = m_waterEnt;
        m_cameras[m_currentCamera].getComponent<cro::Camera>().active = true;

        if (m_cameras[m_currentCamera].hasComponent<CameraFollower>())
        {
            m_cameras[m_currentCamera].getComponent<CameraFollower>().reset(m_cameras[m_currentCamera]);
            m_cameras[m_currentCamera].getComponent<CameraFollower>().currentFollowTime = CameraFollower::MinFollowTime;
        }
        auto* post = m_cameras[m_currentCamera].getComponent<TargetInfo>().postProcess;
        m_courseEnt.getComponent<cro::Drawable2D>().setShader(post->shader);
        for (const auto& [name, val] : post->uniforms)
        {
            m_courseEnt.getComponent<cro::Drawable2D>().bindUniform(name, val);
        }

        //setUIHidden(m_currentCamera == CameraID::Sky);
    }
}

void GolfState::updateCameraHeight(float movement)
{
    //lets the player move the camera up and down when putting
    if (m_currentCamera == CameraID::Player
        && (m_currentPlayer.terrain == TerrainID::Green
            || getClub() == ClubID::Putter))
    {
        auto camPos = m_cameras[CameraID::Player].getComponent<cro::Transform>().getPosition();

        static constexpr float DistanceIncrease = 5.f;
        float distanceToHole = glm::length(m_holeData[m_currentHole].pin - camPos);
        float heightMultiplier = std::clamp(distanceToHole - DistanceIncrease, 0.f, DistanceIncrease);

        const auto MaxOffset = m_cameras[CameraID::Player].getComponent<TargetInfo>().targetHeight + 0.2f;
        const auto TargetHeight = MaxOffset + m_collisionMesh.getTerrain(camPos).height;

        camPos.y = std::clamp(camPos.y + movement, TargetHeight - (MaxOffset * 0.5f), TargetHeight + (MaxOffset * 0.6f) + (heightMultiplier / DistanceIncrease));


        //correct for any target offset that may have been added in transition
        auto& lookAtOffset = m_cameras[CameraID::Player].getComponent<TargetInfo>().finalLookAtOffset;
        auto movement = lookAtOffset * (1.f / 30.f);
        lookAtOffset += movement;

        auto& lookAt = m_cameras[CameraID::Player].getComponent<TargetInfo>().finalLookAt;
        lookAt -= movement;

        m_cameras[CameraID::Player].getComponent<cro::Transform>().setPosition(camPos);
        m_cameras[CameraID::Player].getComponent<cro::Transform>().setRotation(lookRotation(camPos, lookAt));
    }
}

void GolfState::toggleFreeCam()
{
    if (!m_photoMode)
    {
        //only switch if we're the active player and the input is active
        if (/*!m_groupIdle &&*/
            (!m_inputParser.getActive() || m_currentCamera != CameraID::Player
            || m_emoteWheel.currentScale != 0)
            || m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].isCPU)
        {
            return;
        }
    }


    m_photoMode = !m_photoMode;
    if (m_photoMode)
    {
        m_defaultCam = m_gameScene.setActiveCamera(m_freeCam);
        m_defaultCam.getComponent<cro::Camera>().active = false;
        m_defaultCam.getComponent<TargetInfo>().waterPlane = {};

        m_gameScene.setActiveListener(m_freeCam);


        const auto pos = m_defaultCam.getComponent<cro::Transform>().getWorldPosition();
        const auto rot = m_defaultCam.getComponent<cro::Transform>().getWorldRotation();
        const auto fov = m_defaultCam.getComponent<CameraFollower::ZoomData>().fov;
        m_freeCam.getComponent<FpsCamera>().startTransition(pos, rot, fov);

        m_freeCam.getComponent<cro::Transform>().setPosition(pos);
        m_freeCam.getComponent<cro::Transform>().setRotation(rot);
        m_freeCam.getComponent<cro::Camera>().active = true;
        m_freeCam.getComponent<cro::Camera>().resizeCallback(m_freeCam.getComponent<cro::Camera>());
        
        m_freeCam.getComponent<TargetInfo>().waterPlane = m_waterEnt;
        //TODO store the target of the water ent so we can restore it?

        //reduce fade distance
        m_resolutionUpdate.targetFade = 0.2f;

        setUIHidden(true);

        //hide stroke indicator
        cro::Command cmd;
        cmd.targetFlags = CommandID::StrokeArc | CommandID::StrokeIndicator;
        cmd.action = [](cro::Entity e, float) {e.getComponent<cro::Model>().setHidden(true); };
        m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        m_gameScene.setSystemActive<FpsCameraSystem>(m_photoMode);
        m_gameScene.getSystem<FpsCameraSystem>()->process(0.f);

        //m_waterEnt.getComponent<cro::Callback>().active = false;

        const inv::Loadout* l = nullptr;
        if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID)
        {
            l = &m_sharedProfiles.playerProfiles[m_sharedData.profileIndices[m_currentPlayer.player]].loadout;
        }
        m_inputParser.setActive(!m_photoMode && m_restoreInput, m_currentPlayer.terrain, l);
        cro::App::getWindow().setMouseCaptured(true);


        m_freecamMenuEnt.getComponent<cro::Callback>().active = true; //this does the show/hide animation
        enableDOF(m_useDOF);

        for (auto i = 0; i < 4; ++i)
        {
            cro::GameController::applyDSTriggerEffect(i, cro::GameController::DSTriggerBoth, cro::GameController::DSEffect::createFeedback(0, 1));
        }
    }
    else
    {
        const auto pos = m_freeCam.getComponent<cro::Transform>().getWorldPosition();
        const auto rot = m_freeCam.getComponent<cro::Transform>().getWorldRotation();
        m_freeCam.getComponent<FpsCamera>().endTransition(pos, rot);

        m_freeCam.getComponent<FpsCamera>().transition.completionCallback =
            [&]()
            {
                m_gameScene.setActiveCamera(m_defaultCam);
                m_gameScene.setActiveListener(m_defaultCam);

                m_defaultCam.getComponent<cro::Camera>().active = true;
                m_defaultCam.getComponent<TargetInfo>().waterPlane = m_waterEnt;

                m_freeCam.getComponent<cro::Camera>().active = false;
                m_freeCam.getComponent<TargetInfo>().waterPlane = {};

                //restore fade distance
                m_resolutionUpdate.targetFade = m_currentPlayer.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;

                //unhide UI
                setUIHidden(false);

                //and stroke indicator
                cro::Command cmd;
                cmd.targetFlags = CommandID::StrokeIndicator;
                cmd.action = [&](cro::Entity e, float)
                    {
                        auto localPlayer = m_currentPlayer.client == m_sharedData.clientConnection.connectionID;
                        e.getComponent<cro::Model>().setHidden(!(localPlayer && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU));
                    };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                cmd.targetFlags = CommandID::StrokeArc;
                cmd.action = [&](cro::Entity e, float)
                    {
                        //don't show the arc if we're switching to putt view
                        auto localPlayer = m_currentPlayer.client == m_sharedData.clientConnection.connectionID;
                        e.getComponent<cro::Model>().setHidden(!(localPlayer 
                            && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU
                            && !m_puttViewState.isPuttView));
                    };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


                m_gameScene.setSystemActive<FpsCameraSystem>(false);

                m_waterEnt.getComponent<cro::Callback>().active = true;

                const inv::Loadout* l = nullptr;
                if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID)
                {
                    l = &m_sharedProfiles.playerProfiles[m_sharedData.profileIndices[m_currentPlayer.player]].loadout;
                }
                m_inputParser.setActive(!m_photoMode && m_restoreInput, m_currentPlayer.terrain, l);
                cro::App::getWindow().setMouseCaptured(false);

                m_freecamMenuEnt.getComponent<cro::Callback>().active = false; //this does the show/hide animation

            };
        enableDOF(false);

        for (auto i = 0; i < 4; ++i)
        {
            cro::GameController::applyDSTriggerEffect(i, cro::GameController::DSTriggerBoth, cro::GameController::DSEffect::createWeapon(0, 1, 2));
        }
    }

    Activity a;
    a.client = m_sharedData.clientConnection.connectionID;
    a.type = m_photoMode ? Activity::FreecamStart : Activity::FreecamEnd;
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::Activity, a, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void GolfState::enableDOF(bool enable)
{
    if (enable)
    {
        //use DOF shader
        m_freeCam.getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::CompositeDOF];
    }
    else
    {
        //regular composite
        m_freeCam.getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Composite];
    }

    m_courseEnt.getComponent<cro::Drawable2D>().setShader(m_freeCam.getComponent<TargetInfo>().postProcess->shader);
    for (const auto& [n, v] : m_freeCam.getComponent<TargetInfo>().postProcess->uniforms)
    {
        m_courseEnt.getComponent<cro::Drawable2D>().bindUniform(n, v);
    }
}

void GolfState::applyShadowQuality()
{
    m_shadowQuality.update(m_sharedData.hqShadows);

    m_gameScene.getSystem<cro::ShadowMapRenderer>()->setRenderInterval(m_sharedData.hqShadows ? 2 : 3);

    auto applyShadowSettings =
        [&](cro::Camera& cam, std::int32_t camID)
        {
            std::uint32_t shadowMapSize = m_sharedData.hqShadows ? 3072u : 1024u;
            if (camID == CameraID::Sky)
            {
                shadowMapSize /= 2;
            }

            float camDistance = m_shadowQuality.shadowFarDistance;
            if ((camID == CameraID::Player
                && m_currentPlayer.terrain == TerrainID::Green)
                || camID == CameraID::Bystander)
            {
                camDistance = m_shadowQuality.shadowNearDistance;
            }

            cam.shadowMapBuffer.create(shadowMapSize, shadowMapSize);
            cam.setMaxShadowDistance(camDistance);
            cam.setBlurPassCount(1);

            cam.setPerspective(cam.getFOV(), cam.getAspectRatio(), cam.getNearPlane(), cam.getFarPlane(), m_shadowQuality.cascadeCount);
        };

    for (auto i = 0; i < static_cast<std::int32_t>(m_cameras.size()); ++i)
    {
        applyShadowSettings(m_cameras[i].getComponent<cro::Camera>(), i);
    }

#ifdef CRO_DEBUG_
    applyShadowSettings(m_freeCam.getComponent<cro::Camera>(), -1);
#endif // CRO_DEBUG_

}

void GolfState::updateSkybox(float dt)
{
    auto activeCam = m_photoMode ? m_freeCam : m_cameras[m_currentCamera];

    const auto& srcCam = activeCam.getComponent<cro::Camera>();
    auto& dstCam = m_skyCameras[SkyCam::Main].getComponent<cro::Camera>();

    //we want the sky zoom to be less profound, to emulate distance
    const auto baseFov = m_sharedData.fov * cro::Util::Const::degToRad;
    const auto ratio = srcCam.getFOV() / baseFov;
    float diff = 1.f - ratio;
    diff -= (diff / 32.f);

    dstCam.viewport = srcCam.viewport;
    dstCam.setPerspective(baseFov * (1.f - diff), srcCam.getAspectRatio(), 0.01f, 14.f);

    static constexpr float HeightScale = 128.f;

    m_skyCameras[SkyCam::Main].getComponent<cro::Transform>().setRotation(activeCam.getComponent<cro::Transform>().getWorldRotation());
    auto pos = activeCam.getComponent<cro::Transform>().getWorldPosition();
    pos.x = 0.f;
    pos.y /= HeightScale;
    pos.z = 0.f;
    m_skyCameras[SkyCam::Main].getComponent<cro::Transform>().setPosition(pos);

    if (m_flightCam.getComponent<cro::Camera>().active)
    {
        m_skyCameras[SkyCam::Flight].getComponent<cro::Transform>().setRotation(m_flightCam.getComponent<cro::Transform>().getWorldRotation());
        pos = m_flightCam.getComponent<cro::Transform>().getWorldPosition();
        pos.x = 0.f;
        pos.y /= HeightScale;
        pos.z = 0.f;
        m_skyCameras[SkyCam::Flight].getComponent<cro::Transform>().setPosition(pos);
    }

    //and make sure the skybox is up to date too, so there's
    //no lag between camera orientation.
    m_skyScene.simulate(dt);
}

void GolfState::resetIdle()
{
    m_idleTimer.restart();
    m_idleTime = DefaultIdleTime / 3.f;

    if (m_currentCamera == CameraID::Idle)
    {
        setActiveCamera(CameraID::Player);
        m_inputParser.setSuspended(false);

        cro::Command cmd;
        cmd.targetFlags = CommandID::StrokeArc | CommandID::StrokeIndicator;
        cmd.action = [&](cro::Entity e, float)
            {
                auto localPlayer = m_currentPlayer.client == m_sharedData.clientConnection.connectionID;
                e.getComponent<cro::Model>().setHidden(!(localPlayer && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU));
            };
        m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        //resets the drone which may have drifted while player idled.
        if (m_drone.isValid())
        {
            auto& data = m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>();
            data.target.getComponent<cro::Transform>().setPosition(data.resetPosition);
        }

        Activity a;
        a.client = m_sharedData.clientConnection.connectionID;
        a.type = Activity::PlayerIdleEnd;
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::Activity, a, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
}

void GolfState::togglePuttingView(bool putt)
{
    /*
    NOTE this is only used when manually switching to a putter
    if it is currently available. Landing on the green will
    automatically set the camera to the putt position via
    createTransition()
    */

    if (m_puttViewState.isBusy ||
        m_puttViewState.isPuttView == putt ||
        !m_puttViewState.isEnabled)
    {
        return;
    }

    m_puttViewState.isBusy = true;
    m_puttViewState.isPuttView = putt;


    //set the target zoom on the player camera
    float zoom = 1.f;
    if (putt)
    {
        const float dist = 1.f - std::min(1.f, glm::length(m_currentPlayer.position - m_holeData[m_currentHole].pin));
        zoom = m_holeData[m_currentHole].puttFromTee ? (PuttingZoom * (1.f - (0.56f * dist))) : GolfZoom;

        //reduce the zoom within the final metre
        float diff = 1.f - zoom;
        zoom += diff * dist;
    }

    m_cameras[CameraID::Player].getComponent<CameraFollower::ZoomData>().target = zoom;
    m_cameras[CameraID::Player].getComponent<cro::Callback>().active = true;
    m_cameras[CameraID::Player].getComponent<cro::Callback>().getUserData<float>() = PuttZoomSpeed;


    //display the putting grid
    cro::Command cmd;
    cmd.targetFlags = CommandID::SlopeIndicator;
    cmd.action = [putt](cro::Entity e, float)
        {
            if (putt)
            {
                e.getComponent<cro::Model>().setHidden(!putt);
                e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 0;
            }
            else
            {
                e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 1;
            }
            e.getComponent<cro::Callback>().active = true;
        };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    //hmm sometimes the pointer is reset before we can
    //hide the model, so let's just hide ALL the models
    if (putt)
    {
        for (auto i = 0u; i < ConstVal::MaxClients; ++i)
        {
            for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
            {
                auto scale = m_avatars[i][j].model.getComponent<cro::Transform>().getScale();
                scale.y = scale.z = 0.f; //don't mutate x... it controls which way they're facing
                m_avatars[i][j].model.getComponent<cro::Transform>().setScale(scale);
                m_avatars[i][j].model.getComponent<cro::Model>().setHidden(true);
                m_avatars[i][j].model.getComponent<cro::Callback>().getUserData<PlayerCallbackData>().scale = 0;
            }
        }
    }



    //hide player avatar
    if (m_activeAvatar)
    {
        //check distance and animate 
        if (putt)
        {
            auto scale = m_activeAvatar->model.getComponent<cro::Transform>().getScale();
            scale.y = scale.z = 0.f;
            m_activeAvatar->model.getComponent<cro::Transform>().setScale(scale);
            m_activeAvatar->model.getComponent<cro::Model>().setHidden(true);
            m_activeAvatar->model.getComponent<cro::Callback>().getUserData<PlayerCallbackData>().scale = 0;
        }
        else
        {
            //the hide callback will have removed the club model
            const auto& models = m_clubModels.at(m_activeAvatar->clubModelID);

            m_activeAvatar->hands->setModel(models.models[models.indices[getClub()]]);
            m_activeAvatar->hands->getModel().getComponent<cro::Model>().setFacing(m_activeAvatar->model.getComponent<cro::Model>().getFacing());

            m_activeAvatar->model.getComponent<cro::Callback>().getUserData<PlayerCallbackData>().direction = 0;
            m_activeAvatar->model.getComponent<cro::Callback>().active = true;
            m_activeAvatar->model.getComponent<cro::Model>().setHidden(false);
        }

    }


    //set up the camera target
    auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();
    if (putt)
    {
        targetInfo.targetHeight = CameraPuttHeight;
        targetInfo.targetHeight *= CameraTeeMultiplier;
        targetInfo.targetOffset = CameraPuttOffset;
    }
    else
    {
        targetInfo.targetHeight = CameraStrokeHeight;
        targetInfo.targetOffset = CameraStrokeOffset;
    }

    //if we have a sub-target see if that should be active
    //TODO we should have already determined this in createTransition, 
    //so we should store it somewhere and use  that instead
    auto activeTarget = findTargetPos(m_currentPlayer.position);


    auto targetDir = activeTarget - m_currentPlayer.position;
    auto pinDir = m_holeData[m_currentHole].pin - m_currentPlayer.position;
    targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;

    //always look at the target in mult-target mode and target not yet hit
    if (m_sharedData.scoreType == ScoreType::MultiTarget
        && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].targetHit)
    {
        targetInfo.targetLookAt = m_holeData[m_currentHole].target;
    }
    else
    {
        //if both the pin and the target are in front of the player
        if (glm::dot(glm::normalize(targetDir), glm::normalize(pinDir)) > 0.4)
        {
            //set the target depending on how close it is
            auto pinDist = glm::length2(pinDir);
            auto targetDist = glm::length2(targetDir);
            if (pinDist < targetDist)
            {
                //always target pin if its closer
                targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
            }
            else
            {
                //target the pin if the target is too close
                //TODO this is really to do with whether or not we're putting
                //when this check happens, but it's unlikely to have
                //a target on the green in other cases.
                const float MinDist = m_holeData[m_currentHole].puttFromTee ? 9.f : 2500.f;
                if (targetDist < MinDist) //remember this in len2
                {
                    targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
                }
                else
                {
                    targetInfo.targetLookAt = activeTarget;
                }
            }
        }
        else
        {
            //else set the pin as the target
            targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
        }
    }

    
    //if this the local client disable the input while playing the animation
    //and update the stroke arc if visible
    if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID)
    {
        m_inputParser.setSuspended(true);

        cro::Command cmd;
        cmd.targetFlags = CommandID::StrokeIndicator;
        cmd.action = [&, putt](cro::Entity e, float)
            {
                //fudgy way of changing the render type when putting
                if (putt)
                {
                    e.getComponent<cro::Model>().getMeshData().indexData[0].primitiveType = GL_TRIANGLES;
                }
                else
                {
                    e.getComponent<cro::Model>().getMeshData().indexData[0].primitiveType = GL_LINE_STRIP;
                }
            };
        m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        cmd.targetFlags = CommandID::StrokeArc;
        cmd.action = [&, putt](cro::Entity e, float)
            {
                e.getComponent<cro::Model>().setHidden(putt);
            };
        m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    }


    //creates an entity which calls setCamPosition() in an
    //interpolated manner until we reach the dest,
    //at which point we update the ent destroys itself

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();
            auto& progress = e.getComponent<cro::Callback>().getUserData<float>();

            if (progress == 1)
            {
                //we're there
                targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;
                targetInfo.startHeight = targetInfo.targetHeight;
                targetInfo.startOffset = targetInfo.targetOffset;

                m_cameras[CameraID::Player].getComponent<cro::Callback>().getUserData<float>() = DefaultZoomSpeed;
                
                if (!m_groupIdle &&
                    m_currentPlayer.client == m_sharedData.localConnectionData.connectionID)
                {
                    m_inputParser.setSuspended(false);
                }
                m_puttViewState.isBusy = false;

                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
            else
            {
                static constexpr float Speed = 4.f;
                progress = std::min(1.f, progress + (dt * Speed));

                const auto percent = cro::Util::Easing::easeOutQuint(progress);
                targetInfo.currentLookAt = targetInfo.prevLookAt + ((targetInfo.targetLookAt - targetInfo.prevLookAt) * percent);

                auto height = targetInfo.targetHeight - targetInfo.startHeight;
                auto offset = targetInfo.targetOffset - targetInfo.startOffset;


                setCameraPosition(m_currentPlayer.position,
                    targetInfo.startHeight + (height * percent),
                    targetInfo.startOffset + (offset * percent));
            }
        };
}

void GolfState::setCameraTarget(const ActivePlayer& playerData)
{
    auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();
    if (playerData.terrain == TerrainID::Green)
    {
        targetInfo.targetHeight = CameraPuttHeight;
        //if (!m_holeData[m_currentHole].puttFromTee)
        {
            targetInfo.targetHeight *= CameraTeeMultiplier;
        }
        targetInfo.targetOffset = CameraPuttOffset;
    }
    else
    {
        targetInfo.targetHeight = CameraStrokeHeight;
        targetInfo.targetOffset = CameraStrokeOffset;
    }

    //if we have a sub-target see if that should be active
    auto activeTarget = findTargetPos(playerData.position);


    auto targetDir = activeTarget - playerData.position;
    auto pinDir = m_holeData[m_currentHole].pin - playerData.position;
    targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;

    //always look at the target in mult-target mode and target not yet hit
    if (m_sharedData.scoreType == ScoreType::MultiTarget
        && !m_sharedData.connectionData[playerData.client].playerData[playerData.player].targetHit)
    {
        targetInfo.targetLookAt = m_holeData[m_currentHole].target;
    }
    else
    {
        //if both the pin and the target are in front of the player
        if (glm::dot(glm::normalize(targetDir), glm::normalize(pinDir)) > 0.4)
        {
            //set the target depending on how close it is
            auto pinDist = glm::length2(pinDir);
            auto targetDist = glm::length2(targetDir);
            if (pinDist < targetDist)
            {
                //always target pin if its closer
                targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
            }
            else
            {
                //target the pin if the target is too close
                //TODO this is really to do with whether or not we're putting
                //when this check happens, but it's unlikely to have
                //a target on the green in other cases.
                const float MinDist = m_holeData[m_currentHole].puttFromTee ? 9.f : 2500.f;
                if (targetDist < MinDist) //remember this in len2
                {
                    targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
                }
                else
                {
                    targetInfo.targetLookAt = activeTarget;
                }
            }
        }
        else
        {
            //else set the pin as the target
            targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
        }
    }
}

void GolfState::createTransition(const ActivePlayer& playerData, bool setNextPlayer)
{
    //float targetDistance = glm::length2(playerData.position - m_currentPlayer.position);
    m_spectateGhost.getComponent<cro::Callback>().getUserData<GhostCallbackData>().direction = GhostCallbackData::Out;

    //set the target zoom on the player camera
    float zoom = 1.f;
    if (playerData.terrain == TerrainID::Green)
    {
        const float dist = 1.f - std::min(1.f, glm::length(playerData.position - m_holeData[m_currentHole].pin));
        zoom = m_holeData[m_currentHole].puttFromTee ? (PuttingZoom * (1.f - (0.56f * dist))) : GolfZoom;

        //reduce the zoom within the final metre
        float diff = 1.f - zoom;
        zoom += diff * dist;
    }

    m_cameras[CameraID::Player].getComponent<CameraFollower::ZoomData>().target = zoom;
    m_cameras[CameraID::Player].getComponent<cro::Callback>().active = true;

    if (setNextPlayer)
    {
        //hide player avatar
        if (m_activeAvatar)
        {
            //check distance and animate 
            if (playerData.terrain == TerrainID::Green)
            {
                auto scale = m_activeAvatar->model.getComponent<cro::Transform>().getScale();
                scale.y = 0.f;
                scale.z = 0.f;
                m_activeAvatar->model.getComponent<cro::Transform>().setScale(scale);
                m_activeAvatar->model.getComponent<cro::Model>().setHidden(true);

                if (m_activeAvatar->hands)
                {
                    //we have to free this up alse the model might
                    //become attached to two avatars...
                    m_activeAvatar->hands->setModel({});
                }
            }
            else
            {
                m_activeAvatar->model.getComponent<cro::Callback>().getUserData<PlayerCallbackData>().direction = 1;
                m_activeAvatar->model.getComponent<cro::Callback>().active = true;
            }
        }


        //hide hud
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::Root;
        cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>().first = 1;
                e.getComponent<cro::Callback>().active = true;
            };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        cmd.targetFlags = CommandID::UI::PlayerName;
        cmd.action =
            [&](cro::Entity e, float)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f)); //also hides attached icon
                auto& data = e.getComponent<cro::Callback>().getUserData<TextCallbackData>();
                data.string = " ";
                e.getComponent<cro::Callback>().active = true;
            };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        /*cmd.targetFlags = CommandID::UI::PlayerIcon;
        cmd.action =
            [&](cro::Entity e, float)
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);*/

    }

    //creates an entity which calls setCamPosition() in an
    //interpolated manner until we reach the dest,
    //at which point we update the active player and
    //the ent destroys itself
    auto startPos = setNextPlayer ? m_currentPlayer.position : m_lastSpectatePosition;
    if (startPos == playerData.position)
    {
        //we're already there
        auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();
        targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;
        targetInfo.startHeight = targetInfo.targetHeight;
        targetInfo.startOffset = targetInfo.targetOffset;

        if (setNextPlayer)
        {
            requestNextPlayer(playerData);
        }
        m_lastSpectatePosition = playerData.position;
        setGhostPosition(playerData.position);
    }
    else
    {
        //set up the camera target
        setCameraTarget(playerData);

        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(startPos);
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<ActivePlayer>(playerData);
        entity.getComponent<cro::Callback>().function =
            [&, startPos, setNextPlayer](cro::Entity e, float dt)
        {
            const auto& playerData = e.getComponent<cro::Callback>().getUserData<ActivePlayer>();

            auto currPos = e.getComponent<cro::Transform>().getPosition();
            auto travel = playerData.position - currPos;
            auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();

            auto targetDir = targetInfo.currentLookAt - currPos;
            m_camRotation = std::atan2(-targetDir.z, targetDir.x);

            float minTravel = playerData.terrain == TerrainID::Green ? 0.000001f : 0.005f;
            if (glm::length2(travel) < minTravel)
            {
                //we're there
                targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;
                targetInfo.startHeight = targetInfo.targetHeight;
                targetInfo.startOffset = targetInfo.targetOffset;

                //hmm the final result is not always the same as the flyby - so snapping this here
                //can cause a jump in view

                //setCameraPosition(playerData.position, targetInfo.targetHeight, targetInfo.targetOffset);
                if (setNextPlayer)
                {
                    requestNextPlayer(playerData);
                }
                m_lastSpectatePosition = playerData.position;
                setGhostPosition(playerData.position);

                m_gameScene.getActiveListener().getComponent<cro::AudioListener>().setVelocity(glm::vec3(0.f));

                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
            else
            {
                const auto totalDist = glm::length(playerData.position - startPos);
                //if (std::isnan(totalDist)) throw std::length_error("pos & start pos are the same");
                //if (totalDist == 0) throw std::length_error("this will cause div0");
                const auto currentDist = glm::length(travel);
                //if (std::isnan(currentDist)) throw;
                const auto percent = 1.f - (currentDist / totalDist);

                targetInfo.currentLookAt = targetInfo.prevLookAt + ((targetInfo.targetLookAt - targetInfo.prevLookAt) * percent);

                auto height = targetInfo.targetHeight - targetInfo.startHeight;
                auto offset = targetInfo.targetOffset - targetInfo.startOffset;

                static constexpr float Speed = 4.f;
                e.getComponent<cro::Transform>().move(travel * Speed * dt);
                setCameraPosition(e.getComponent<cro::Transform>().getPosition(),
                    targetInfo.startHeight + (height * percent),
                    targetInfo.startOffset + (offset * percent));

                m_gameScene.getActiveListener().getComponent<cro::AudioListener>().setVelocity(travel * Speed);
            }
        };
    }
}

void GolfState::startFlyBy()
{
    m_idleTimer.restart();
    m_idleTime = cro::seconds(90.f);

    //reset the zoom if not putting from tee
    m_cameras[CameraID::Player].getComponent<CameraFollower::ZoomData>().target = m_holeData[m_currentHole].puttFromTee ? PuttingZoom : 1.f;
    m_cameras[CameraID::Player].getComponent<cro::Callback>().active = true;
    m_cameras[CameraID::Player].getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);


    //static for lambda capture
    static constexpr float MoveSpeed = 50.f; //metres per sec
    static constexpr float MaxHoleDistance = 275.f; //this scales the move speed based on the tee-pin distance
    float SpeedMultiplier = (0.25f + ((m_holeData[m_currentHole].distanceToPin / MaxHoleDistance) * 0.75f));
    float heightMultiplier = 1.f;

    //only slow down if current and previous were putters - in cases of custom courses
    bool previousPutt = (m_currentHole > 0) ? m_holeData[m_currentHole - 1].puttFromTee : m_holeData[m_currentHole].puttFromTee;
    if (m_holeData[m_currentHole].puttFromTee)
    {
        if (previousPutt)
        {
            SpeedMultiplier /= 3.f;
        }
        heightMultiplier = 0.35f;
    }

    struct FlyByTarget final
    {
        std::function<float(float)> ease = std::bind(&cro::Util::Easing::easeInOutQuad, std::placeholders::_1);
        std::int32_t currentTarget = 0;
        float progress = 0.f;
        std::array<glm::mat4, 4u> targets = {};
        std::array<float, 4u> speeds = {};
    }targetData;

    targetData.targets[0] = m_cameras[CameraID::Player].getComponent<cro::Transform>().getLocalTransform();


    static constexpr glm::vec3 BaseOffset(10.f, 5.f, 0.f);
    const auto& holeData = m_holeData[m_currentHole];

    //calc offset based on direction of initial target to tee
    glm::vec3 dir = holeData.tee - holeData.pin;
    float rotation = std::atan2(-dir.z, dir.x);
    glm::quat q = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), rotation, cro::Transform::Y_AXIS);
    glm::vec3 offset = q * BaseOffset;
    offset.y *= heightMultiplier;

    //set initial transform to look at pin from offset position
    auto transform = glm::inverse(glm::lookAt(offset + holeData.pin, holeData.pin, cro::Transform::Y_AXIS));
    targetData.targets[1] = transform;

    float moveDist = glm::length(glm::vec3(targetData.targets[0][3]) - glm::vec3(targetData.targets[1][3]));
    targetData.speeds[0] = moveDist / MoveSpeed;
    targetData.speeds[0] *= 1.f / SpeedMultiplier;

    //translate the transform to look at target point or half way point if not set
    constexpr float MinTargetMoveDistance = 100.f;
    auto diff = holeData.target - holeData.pin;
    if (glm::length2(diff) < MinTargetMoveDistance)
    {
        diff = (holeData.tee - holeData.pin) / 2.f;
    }
    diff.y = 10.f * SpeedMultiplier;


    transform[3] += glm::vec4(diff, 0.f);
    targetData.targets[2] = transform;

    moveDist = glm::length(glm::vec3(targetData.targets[1][3]) - glm::vec3(targetData.targets[2][3]));
    auto moveSpeed = MoveSpeed;
    if (!m_holeData[m_currentHole].puttFromTee)
    {
        moveSpeed *= std::min(1.f, (moveDist / (MinTargetMoveDistance / 2.f)));
    }
    targetData.speeds[1] = moveDist / moveSpeed;
    targetData.speeds[1] *= 1.f / SpeedMultiplier;

    //the final transform is set to what should be the same as the initial player view
    //this is actually more complicated than it seems, so the callback interrogates the
    //player camera when it needs to.

    //set to initial position
    m_cameras[CameraID::Transition].getComponent<cro::Transform>().setLocalTransform(targetData.targets[0]);


    //interp the transform targets
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<FlyByTarget>(targetData);
    entity.getComponent<cro::Callback>().function =
        [&, SpeedMultiplier](cro::Entity e, float dt)
        {
            //keep the balls hidden during transition
            cro::Command c;
            c.targetFlags = CommandID::Ball;
            c.action = [](cro::Entity e, float)
                {
                    e.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                    e.getComponent<cro::Callback>().active = false;
                };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(c);

            auto& data = e.getComponent<cro::Callback>().getUserData<FlyByTarget>();
            data.progress = /*std::min*/(data.progress + (dt / data.speeds[data.currentTarget])/*, 1.f*/);

            auto& camTx = m_cameras[CameraID::Transition].getComponent<cro::Transform>();

            //find out 'lookat' point as it would appear on the water plane and set the water there
            glm::vec3 intersection(0.f);
            if (planeIntersect(camTx.getLocalTransform(), intersection))
            {
                intersection.y = WaterLevel;
                m_cameras[CameraID::Transition].getComponent<TargetInfo>().waterPlane = m_waterEnt; //TODO this doesn't actually update the parent if already attached somewhere else...
                m_cameras[CameraID::Transition].getComponent<TargetInfo>().waterPlane.getComponent<cro::Transform>().setPosition(intersection);
            }

            if (data.progress >= 1)
            {
                data.progress -= 1.f;
                data.currentTarget++;
                //camTx.setLocalTransform(data.targets[data.currentTarget]);

                switch (data.currentTarget)
                {
                default: break;
                case 2:
                    //hope the player cam finished...
                    //which it hasn't on smaller holes, and annoying.
                    //not game-breaking. but annoying.
                {
                    data.targets[3] = m_cameras[CameraID::Player].getComponent<cro::Transform>().getLocalTransform();
                    //data.ease = std::bind(&cro::Util::Easing::easeInSine, std::placeholders::_1);
                    float moveDist = glm::length(glm::vec3(data.targets[2][3]) - glm::vec3(data.targets[3][3]));
                    data.speeds[2] = moveDist / MoveSpeed;
                    data.speeds[2] *= 1.f / SpeedMultiplier;

                    //play the transition music
                    if (m_sharedData.gameMode == GameMode::Tutorial
                        && cro::AudioMixer::getVolume(MixerChannel::UserMusic) < 0.01f
                        && cro::AudioMixer::getVolume(MixerChannel::Music) != 0)
                    {
                        m_cameras[CameraID::Player].getComponent<cro::AudioEmitter>().play();
                    }
                    //else we'll play it when the score board shows, below
                }
                break;
                case 3:
                    //we're done here
                    camTx.setLocalTransform(data.targets[data.currentTarget]);

                    m_gameScene.getSystem<CameraFollowSystem>()->resetCamera();
                    setActiveCamera(CameraID::Player);
                    {
                        if (m_sharedData.gameMode == GameMode::Tutorial)
                        {
                            auto* msg = cro::App::getInstance().getMessageBus().post<SceneEvent>(MessageID::SceneMessage);
                            msg->type = SceneEvent::TransitionComplete;
                        }
                        else
                        {
                            showScoreboard(true);
                            m_newHole = true;

                            if (cro::AudioMixer::getVolume(MixerChannel::UserMusic) < 0.01f
                                && cro::AudioMixer::getVolume(MixerChannel::Music) != 0)
                            {
                                m_cameras[CameraID::Player].getComponent<cro::AudioEmitter>().play();
                            }

                            //delayed ent just to show the score board for a while
                            auto de = m_gameScene.createEntity();
                            de.addComponent<cro::Callback>().active = true;
                            de.getComponent<cro::Callback>().setUserData<float>(0.2f);
                            de.getComponent<cro::Callback>().function =
                                [&](cro::Entity ent, float dt)
                                {
                                    auto& currTime = ent.getComponent<cro::Callback>().getUserData<float>();
                                    currTime -= dt;
                                    if (currTime < 0)
                                    {
                                        auto* msg = cro::App::getInstance().getMessageBus().post<SceneEvent>(MessageID::SceneMessage);
                                        msg->type = SceneEvent::TransitionComplete;

                                        ent.getComponent<cro::Callback>().active = false;
                                        m_gameScene.destroyEntity(ent);
                                    }
                                };
                        }
                        e.getComponent<cro::Callback>().active = false;
                        m_gameScene.destroyEntity(e);
                    }
                    break;
                }
            }

            if (data.currentTarget < 3)
            {
                auto rot = glm::slerp(glm::quat_cast(data.targets[data.currentTarget]), glm::quat_cast(data.targets[data.currentTarget + 1]), data.progress);
                camTx.setRotation(rot);

                auto pos = interpolate(glm::vec3(data.targets[data.currentTarget][3]), glm::vec3(data.targets[data.currentTarget + 1][3]), data.ease(data.progress));
                camTx.setPosition(pos);
            }
        };


    setActiveCamera(CameraID::Transition);


    //hide the minimap ball
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::MiniBall;
    cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide the mini flag
    cmd.targetFlags = CommandID::UI::MiniFlag;
    cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide hud
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>().first = 1;
            e.getComponent<cro::Callback>().active = true;
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //show wait message
    cmd.targetFlags = CommandID::UI::WaitMessage;
    cmd.action =
        [&](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide player
    if (m_activeAvatar)
    {
        auto scale = m_activeAvatar->model.getComponent<cro::Transform>().getScale();
        scale.y = 0.f;
        scale.z = 0.f;
        m_activeAvatar->model.getComponent<cro::Transform>().setScale(scale);
        m_activeAvatar->model.getComponent<cro::Model>().setHidden(true);

        if (m_activeAvatar->hands)
        {
            //we have to free this up alse the model might
            //become attached to two avatars...
            m_activeAvatar->hands->setModel({});
        }
    }

    //hide stroke indicator
    cmd.targetFlags = CommandID::StrokeIndicator | CommandID::StrokeArc;
    cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Model>().setHidden(true);
            e.getComponent<cro::Callback>().active = false;
        };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide ball models so they aren't seen floating mid-air
    //this is moved so it's done continuously by the fly-by callback
    //cmd.targetFlags = CommandID::Ball;
    //cmd.action = [](cro::Entity e, float)
    //    {
    //        e.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    //    };
    //m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    auto* msg = postMessage<SceneEvent>(MessageID::SceneMessage);
    msg->type = SceneEvent::TransitionStart;
    msg->data = static_cast<std::int32_t>(m_currentHole);
}

void GolfState::setCameraPosition(glm::vec3 position, float height, float viewOffset)
{
    static constexpr float MinDist = 6.f;
    static constexpr float MaxDist = 270.f;
    static constexpr float DistDiff = MaxDist - MinDist;
    float heightMultiplier = 1.f; //goes to -1.f at max dist

    auto camEnt = m_cameras[CameraID::Player];
    auto& targetInfo = camEnt.getComponent<TargetInfo>();
    auto target = targetInfo.currentLookAt - position;

    auto dist = glm::length(target);
    float distNorm = std::min(1.f, (dist - MinDist) / DistDiff);
    heightMultiplier -= (2.f * distNorm);

    target *= 1.f - ((1.f - 0.08f) * distNorm);
    target += position;

    auto result = m_collisionMesh.getTerrain(position);

    camEnt.getComponent<cro::Transform>().setPosition({ position.x, result.height + height, position.z });


    auto currentCamTarget = glm::vec3(target.x, result.height + (height * heightMultiplier), target.z);
    camEnt.getComponent<TargetInfo>().finalLookAt = currentCamTarget;

    auto oldPos = camEnt.getComponent<cro::Transform>().getPosition();
    camEnt.getComponent<cro::Transform>().setRotation(lookRotation(oldPos, currentCamTarget));

    auto offset = -camEnt.getComponent<cro::Transform>().getForwardVector();
    camEnt.getComponent<cro::Transform>().move(offset * viewOffset);

    //clamp above ground height and hole radius
    auto newPos = camEnt.getComponent<cro::Transform>().getPosition();

    static constexpr float MinRad = 0.6f + CameraPuttOffset;
    static constexpr float MinRadSqr = MinRad * MinRad;

    auto holeDir = m_holeData[m_currentHole].pin - newPos;
    auto holeDist = glm::length2(holeDir);
    /*if (holeDist < MinRadSqr)
    {
        auto len = std::sqrt(holeDist);
        auto move = MinRad - len;
        holeDir /= len;
        holeDir *= move;
        newPos -= holeDir;
    }*/

    //lower height as we get closer to hole
    heightMultiplier = std::min(1.f, std::max(0.f, holeDist / MinRadSqr));
    //if (!m_holeData[m_currentHole].puttFromTee)
    {
        heightMultiplier *= CameraTeeMultiplier;
    }

    auto groundHeight = std::max(m_collisionMesh.getTerrain(newPos).height, WaterLevel);
    newPos.y = std::max(groundHeight + (CameraPuttHeight * heightMultiplier), newPos.y);

    camEnt.getComponent<cro::Transform>().setPosition(newPos);

    //hmm this stops the putt cam jumping when the position has been offset
    //however the look at point is no longer the hole, so the rotation
    //is weird and we need to interpolate back through the offset
    camEnt.getComponent<TargetInfo>().finalLookAt += newPos - oldPos;
    camEnt.getComponent<TargetInfo>().finalLookAtOffset = newPos - oldPos;

    //also updated by camera follower...
    if (targetInfo.waterPlane.isValid())
    {
        targetInfo.waterPlane.getComponent<cro::Callback>().setUserData<glm::vec3>(target.x, WaterLevel, target.z);
    }
}

void GolfState::sendFreecamToTarget()
{
    //hack to test if transition is already active to prevent
    //multiple button presses, while keeping vars local...
    static cro::Entity entity;

    if (entity.isValid())
    {
        return;
    }

    if (m_photoMode
        && getClub() != ClubID::Putter
        && m_currentPlayer.terrain != TerrainID::Green) 
    {
        glm::vec3 targetLookAt = glm::vec3(0.f);
        glm::vec3 targetPos = glm::vec3(0.f);

        const auto estimatedDist = m_inputParser.getEstimatedDistance();
        auto pinDir = m_holeData[m_currentHole].pin - m_currentPlayer.position;

        //look at the hole if it's closer
        if (const auto len2 = glm::length2(pinDir); len2 < (estimatedDist * estimatedDist))
        {
            const auto len = std::sqrt(len2);

            const glm::vec3 dirVec = pinDir / len;

            targetLookAt = m_holeData[m_currentHole].pin;
            targetLookAt.y = m_collisionMesh.getTerrain(targetLookAt).height + (GreenCamHeight / 3.f);

            targetPos = (dirVec * std::max(len * 0.75f, 1.f)) + m_currentPlayer.position;
            targetPos.y = m_collisionMesh.getTerrain(targetPos).height + (GreenCamHeight * 0.6f);
        }
        else
        {
            const glm::vec3 dirVec = glm::rotate(cro::Transform::QUAT_IDENTITY, m_inputParser.getYaw() + (cro::Util::Const::PI / 2.f), cro::Transform::Y_AXIS) * cro::Transform::Z_AXIS;

            targetLookAt = (dirVec * estimatedDist) + m_currentPlayer.position;
            targetLookAt.y = m_collisionMesh.getTerrain(targetLookAt).height + (GreenCamHeight / 2.f);

            targetPos = (dirVec * std::max(estimatedDist * 0.95f, 1.f)) + m_currentPlayer.position;
            targetPos.y = m_collisionMesh.getTerrain(targetPos).height + GreenCamHeight;
        }


        const auto lookAt = glm::lookAt(targetPos, targetLookAt, cro::Transform::Y_AXIS);
        
        glm::vec3 a, b;
        glm::quat targetRot;
        cro::Util::Matrix::decompose(lookAt, a, targetRot, b);

        const auto startPos = m_freeCam.getComponent<cro::Transform>().getPosition();
        const auto startRot = m_freeCam.getComponent<cro::Transform>().getRotation();

        struct MoveData final
        {
            float currentTime = 0.f;
            const float MaxTime = 0.25f;
        };
        m_gameScene.setSystemActive<FpsCameraSystem>(false);


        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<MoveData>();
        entity.getComponent<cro::Callback>().function =
            [&, startPos, startRot, targetPos, targetRot](cro::Entity e, float dt)
            {
                auto& [ct, MaxTime] = e.getComponent<cro::Callback>().getUserData<MoveData>();
                ct += dt;
                
                const float amt = cro::Util::Easing::easeOutQuint(std::min(ct / MaxTime, 1.f));
                m_freeCam.getComponent<cro::Transform>().setRotation(glm::slerp(startRot, targetRot, amt));
                m_freeCam.getComponent<cro::Transform>().setPosition(glm::mix(startPos, targetPos, amt));
                
                if (amt == 1)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_gameScene.destroyEntity(e);

                    m_gameScene.setSystemActive<FpsCameraSystem>(true);
                    m_freeCam.getComponent<FpsCamera>().correctPitch(targetRot);
                }
            };
    }
}

void GolfState::setGhostPosition(glm::vec3 pos)
{
    auto res = m_collisionMesh.getTerrain(pos);

    if (m_groupIdle && 
        (res.terrain == TerrainID::Rough || res.terrain == TerrainID::Fairway))
    {
        m_spectateGhost.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_camRotation + (cro::Util::Const::PI / 2.f));
        
        const auto goodPos = 
            [&](glm::vec3& p)
            {
                res = m_collisionMesh.getTerrain(p);
                p.y = res.height;
                return (res.terrain == TerrainID::Fairway
                    || res.terrain == TerrainID::Rough
                    || res.terrain == TerrainID::Stone);
            };

        static constexpr auto offset = glm::vec3(-1.6f, 0.f, 0.2f);
        auto move = m_spectateGhost.getComponent<cro::Transform>().getRotation() * offset;

        auto newPos = pos + move;
        bool posIsGood = false;
        if (posIsGood = goodPos(newPos); !posIsGood)
        {
            //try flipping the x axis if the first pos is no good
            move = offset;
            move.x *= -1.f;
            move = m_spectateGhost.getComponent<cro::Transform>().getRotation() * move;
            newPos = pos + move;
            posIsGood = goodPos(newPos);
        }        
        
        if (posIsGood)
        {
            m_spectateGhost.getComponent<cro::Transform>().setPosition(newPos);
            m_spectateGhost.getComponent<cro::Model>().setMaterialProperty(0, "u_rimColour", getBeaconColour(m_sharedData.beaconColour) * glm::vec4(glm::vec3(0.8f), 1.f));
            m_spectateGhost.getComponent<cro::Callback>().getUserData<GhostCallbackData>().direction = GhostCallbackData::In;
            m_spectateGhost.getComponent<cro::Callback>().active = true;
        }
    }
}

void GolfState::spectateGroup(std::uint8_t group)
{
    const cro::Time MinSpectateTime = cro::seconds(4.f);

    //update the current index to next player
    if (m_groupPlayerPositions[group].client != m_sharedData.localConnectionData.connectionID //trying to spectate ourself causes a NaN in the camera transform
        && m_groupPlayerPositions[group].client != 255
        && m_spectateTimer.elapsed() > MinSpectateTime)
    {
        auto& pPos = m_groupPlayerPositions[group];

        //check we're at least 3m from the pin
        static constexpr float MinCamDist = 1.6f;
        static constexpr float MinCamDistSqr = MinCamDist * MinCamDist;
        auto dir = pPos.position - m_holeData[m_currentHole].pin;
        auto len = glm::length2(dir);
        if (len < MinCamDistSqr)
        {
            pPos.position = m_holeData[m_currentHole].pin + (dir * (MinCamDist / std::sqrt(len)));
        }
        else
        {
            auto dir = pPos.position - m_currentPlayer.position;
            auto len = glm::length2(dir);

            if (len < MinCamDistSqr)
            {
                pPos.position = m_currentPlayer.position + (dir * (MinCamDist / std::sqrt(len)));
            }
        }

        setCameraTarget(pPos);
        createTransition(pPos, false);

        retargetMinimap(true);

        //hm, doesn't really work
        //m_gameScene.getSystem<CameraFollowSystem>()->setTargetGroup(group);
        //m_gameScene.setSystemActive<CameraFollowSystem>(true);

        m_spectateTimer.restart();
        m_idleCameraIndex = group;

        m_gameScene.getSystem<CameraFollowSystem>()->setTargetGroup(group);
        m_gameScene.setSystemActive<CameraFollowSystem>(true);

        //set the UI name to the name of the person we're viewing
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::PlayerName;
        cmd.action =
            [&](cro::Entity e, float)
            {
                auto& data = e.getComponent<cro::Callback>().getUserData<TextCallbackData>();
                data.string = m_sharedData.connectionData[pPos.client].playerData[pPos.player].name;
                data.client = pPos.client;
                data.player = pPos.player;
                e.getComponent<cro::Callback>().active = true;
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    }
}

void GolfState::updateLensFlare(cro::Entity e, float)
{
    if (!m_sharedData.useLensFlare)
    {
        e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        return;
    }
    e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

    auto ndc = m_skyScene.getActiveCamera().getComponent<cro::Camera>().getActivePass().viewProjectionMatrix * glm::vec4(m_lensFlare.sunPos, 1.f);

    bool visible = (ndc.w > 0);
    bool occluded = false;

    auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
    verts.clear();

    static constexpr std::int32_t FlareCount = 3; //set the initial stride from sun to screen centre - total is more than this
    static constexpr std::int32_t MaxPoints = 6; //max number of quads - may be few if point count is clipped by screen

    if (visible)
    {
        glm::vec2 screenPos(ndc);
        screenPos /= ndc.w;
        occluded = !ndcVisible(screenPos);

        if (!occluded)
        {
            const glm::vec2 OutputSize(cro::App::getWindow().getSize() / 2u);
            const glm::vec2 QuadSize(48.f * m_viewScale.x);

            auto depthUV = screenPos + glm::vec2(1.f);
            depthUV /= 2.f;

            glUseProgram(m_lensFlare.shaderID);
            glUniform2f(m_lensFlare.positionUniform, depthUV.x, depthUV.y);

            //use length of screenPos to calc brightness / set vert colour
            const float Brightness = (cro::Util::Easing::easeOutCubic(1.f - std::min(1.f, glm::length(screenPos))) * 0.15f) + 0.05f;
            cro::Colour c = cro::Colour(Brightness * 0.2f, 1.f, 1.f, 1.f);
            std::int32_t i = 0;

            //create the quads
            const glm::vec2 Stride = screenPos / FlareCount;
            do
            {
                screenPos -= Stride;

                auto point = screenPos;
                const auto basePos = (point * OutputSize) + OutputSize;
                constexpr float uWidth = (1.f / MaxPoints);
                const float u = uWidth * i;

                auto r = c.getRed();
                r *= (((1.f - std::min(1.f, glm::length2(glm::vec2(screenPos)))) * 0.8f) + 0.2f);
                c.setRed(r);


                //store the centre point of the quad texture so we can scale it in the shader
                //for an abberation effect
                c.setGreen(std::clamp(u + (uWidth / 2.f), 0.f, 1.f));
                c.setBlue(0.5f);

                verts.emplace_back(glm::vec2(basePos.x - QuadSize.x, basePos.y + QuadSize.y), glm::vec2(u, 1.f), c);
                verts.emplace_back(basePos - QuadSize, glm::vec2(u, 0.f), c);
                verts.emplace_back(basePos + QuadSize, glm::vec2(u + uWidth, 1.f), c);

                verts.emplace_back(basePos + QuadSize, glm::vec2(u + uWidth, 1.f), c);
                verts.emplace_back(basePos - QuadSize, glm::vec2(u, 0.f), c);
                verts.emplace_back(glm::vec2(basePos.x + QuadSize.x, basePos.y - QuadSize.y), glm::vec2(u + uWidth, 0.f), c);
                i++;

            } while (ndcVisible(screenPos) && i < MaxPoints);

            if (!verts.empty())
            {
                e.getComponent<cro::Drawable2D>().updateLocalBounds();
            }

            //make sure we're still visible in free cam mode/hidden UI
            float depth = m_courseEnt.getComponent<cro::Transform>().getOrigin().z;
            e.getComponent<cro::Transform>().setOrigin({ 0.f, 0.f, depth });
        }
    }
}

void GolfState::updatePointFlares(cro::Entity e, float)
{
    if (!m_sharedData.useLensFlare)
    {
        e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        return;
    }
    e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));


    const auto texSize = glm::vec2(e.getComponent<cro::Drawable2D>().getTexture()->getSize());

    std::vector<cro::Vertex2D> verts;

    const auto addQuad = [&](glm::vec2 scale, glm::vec2 position, cro::Colour c)
        {
            const auto size = (texSize * scale) / 2.f;

            verts.emplace_back(position + glm::vec2(-size.x, size.y), glm::vec2(0.f, 1.f), c);
            verts.emplace_back(position -size, glm::vec2(0.f), c);
            verts.emplace_back(position + size, glm::vec2(1.f), c);

            verts.emplace_back(position + size, glm::vec2(1.f), c);
            verts.emplace_back(position - size, glm::vec2(0.f), c);
            verts.emplace_back(position + glm::vec2(size.x, -size.y), glm::vec2(1.f, 0.f), c);
        };
    
    const auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    const auto& visibleLights = 
        m_gameScene.getSystem<cro::LightVolumeSystem>()->getDrawList(cam.getDrawListIndex());
    
    const glm::vec2 OutputSize(cro::App::getWindow().getSize() / 2u);
    const auto camPosition = m_gameScene.getActiveCamera().getComponent<cro::Transform>().getWorldPosition();

    for (auto light : visibleLights)
    {
        //this is a bit hacky, but we don't want to add flares to lights
        //that simulate flames, so we'll just skip any with an animation
        if (light.hasComponent<LightAnimation>())
        {
            continue;
        }

        static constexpr float MaxLightDist = 50.f;
        static constexpr float MaxLightDistSqr = MaxLightDist * MaxLightDist;

        const auto lightPos = light.getComponent<cro::Transform>().getWorldPosition();
        if (glm::length2(lightPos - camPosition) > MaxLightDistSqr)
        {
            //skip lights further than max dist
            continue;
        }

        auto ndc = cam.getPass(cro::Camera::Pass::Final).viewProjectionMatrix * glm::vec4(lightPos, 1.f);
        
        bool visible = (ndc.w > 0);
        bool occluded = false;

        if (visible)
        {
            glm::vec2 screenPos(ndc);
            screenPos /= ndc.w;
            occluded = !ndcVisible(screenPos);

            if (!occluded)
            {
                auto depthUV = screenPos + glm::vec2(1.f);
                depthUV /= 2.f;

                const float Brightness = (cro::Util::Easing::easeOutCubic(1.f - std::min(1.f, glm::length(screenPos))) * 0.4f) + 0.1f;

                cro::Colour c(depthUV.x, depthUV.y, ndc.z / ndc.w, Brightness);
                const auto pos = (screenPos * OutputSize) + OutputSize;
                const float Scale = (1.f - (glm::length(lightPos - camPosition) / MaxLightDist));// *light.getComponent<cro::Transform>().getWorldScale().x;

                addQuad(glm::vec2(Scale) * m_viewScale.x, pos, c);
            }
        }
    }

    e.getComponent<cro::Drawable2D>().setVertexData(verts);

    //make sure we're still visible in free cam mode/hidden UI
    float depth = m_courseEnt.getComponent<cro::Transform>().getOrigin().z;
    e.getComponent<cro::Transform>().setOrigin({ 0.f, 0.f, depth });
}

void GolfState::setIdleGroup(std::uint8_t group)
{
    m_groupIdle = true;
    /*m_gameScene.getSystem<CameraFollowSystem>()->setTargetGroup(group);
    m_gameScene.setSystemActive<CameraFollowSystem>(true);*/

    //hide the player model
    if (m_activeAvatar)
    {
        m_activeAvatar->model.getComponent<cro::Callback>().getUserData<PlayerCallbackData>().direction = 1;
        m_activeAvatar->model.getComponent<cro::Callback>().active = true;
    }

    resetIdle();

    //and the power bar
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>().first = 1;
            e.getComponent<cro::Callback>().active = true;
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    spectateGroup(group);
}