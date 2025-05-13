/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "GcState.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/audio/AudioMixer.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Easings.hpp>

namespace
{

}

GCState::GCState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_processReturn (true),
    m_cameraIndex   (0)
{
    addSystems();
    loadAssets();
    createScene();
    createUI();

    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("Window"))
    //        {
    //            auto pos = m_gameScene.getActiveCamera().getComponent<cro::Transform>().getPosition();
    //            if (ImGui::DragFloat3("Pos", &pos[0], 0.1f))
    //            {
    //                m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition(pos);
    //            }

    //            auto t = m_gameScene.getActiveCamera().getComponent<cro::Camera>().shadowMapBuffer.getTexture(0);
    //            ImGui::Image(t, { 256.f, 256.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //        }
    //        ImGui::End();
    //    });
}

//public
bool GCState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            quitState();
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonB:
        case cro::GameController::ButtonBack:
        case cro::GameController::ButtonStart:
            quitState();
            break;
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void GCState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Pushed
            && data.id == StateID::GC)
        {
            m_music.play();


            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity, float)
                {
                    if (m_music.getStatus() == cro::MusicPlayer::Status::Stopped)
                    {
                        quitState();
                    }
                };            
        }
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool GCState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return m_processReturn;
}

void GCState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void GCState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::ParticleSystem>(mb);

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void GCState::loadAssets()
{
    m_environmentMap.loadFromFile("assets/images/hills.hdr");
    m_music.loadFromFile("assets/golf/sound/stage.ogg");
    m_music.setVolume(0.f);

    m_gameScene.setSkyboxColours(cro::Colour::Black, cro::Colour::Black, cro::Colour::Black);
    m_gameScene.enableSkybox();
}

void GCState::createScene()
{
    cro::ModelDefinition md(m_resources, &m_environmentMap);
    if (md.loadFromFile("assets/golf/models/billboard.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -2.f, 0.f, 0.f });
        md.createModel(entity);
        entity.getComponent<cro::Skeleton>().play(0);
    }

    if (md.loadFromFile("assets/golf/models/wheelie_animated.cmt"))
    {
        float x = -2.7f;
        constexpr float Spacing = 1.3f;
        constexpr std::int32_t BinCount = 5;

        for (auto i = 0; i < BinCount; ++i)
        {
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({x, 0.f, 2.f});
            md.createModel(entity);
            entity.getComponent<cro::Skeleton>().play(0);

            x += Spacing;
        }
    }

    if (md.loadFromFile("assets/golf/models/stage.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
    }

    if (md.loadFromFile("assets/golf/models/phone.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -4.8f, 0.f, 0.3f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 2.f);
        entity.getComponent<cro::Transform>().setScale(glm::vec3(0.6667f));
        md.createModel(entity);
    }


    auto dancerRoot = m_gameScene.createEntity();
    dancerRoot.addComponent<cro::Transform>().setPosition({ 4.f, 0.f, 1.2f });
    dancerRoot.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.8f);

    if (md.loadFromFile("assets/golf/models/cc01.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -0.75f, 0.f, 0.f });
        md.createModel(entity);
        if (md.hasSkeleton())
        {
            entity.getComponent<cro::Skeleton>().play(0);
        }
        dancerRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }
    if (md.loadFromFile("assets/golf/models/cc02.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.75f, 0.f, 0.f });
        md.createModel(entity);
        if (md.hasSkeleton())
        {
            entity.getComponent<cro::Skeleton>().play(0);
        }
        dancerRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }


    cro::EmitterSettings particles;
    particles.loadFromFile("assets/golf/particles/hio.cps", m_resources.textures);
    const std::array Positions =
    {
        glm::vec3(-3.2f, 0.f, 0.2f),
        glm::vec3(3.2f, 0.f, 0.2f),

        glm::vec3(-3.f, 0.f, 3.8f),
        glm::vec3(3.f, 0.f, 3.8f),
    };

    for (auto p : Positions)
    {
        static constexpr float RotationAmount = cro::Util::Const::PI / 8.f;
        float rotation = -RotationAmount;

        for (auto i = 0; i < 3; ++i)
        {
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(p);
            entity.getComponent<cro::Transform>().setRotation(cro::Transform::Z_AXIS, rotation);
            entity.addComponent<cro::ParticleEmitter>().settings = particles;
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<float>(5.f);
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
                {
                    auto& t = e.getComponent<cro::Callback>().getUserData<float>();
                    t -= dt;

                    if (t < 0)
                    {
                        t += 8.f;
                        e.getComponent<cro::ParticleEmitter>().start();
                    }
                };

            rotation += RotationAmount;
        }
    }


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 15.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    cam.shadowMapBuffer.create(2048, 2048);
    cam.setBlurPassCount(1);
    resize(cam);

    static constexpr float CamMin = 1.f;
    static constexpr float CamMax = 7.f;

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 2.1f, CamMin });
    m_gameScene.getActiveCamera().addComponent<cro::Callback>().active = true;
    m_gameScene.getActiveCamera().getComponent<cro::Callback>().setUserData<float>(0.f);
    m_gameScene.getActiveCamera().getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
        progress = std::min(1.f, (dt * 0.1667f) + progress);

        auto pos = e.getComponent<cro::Transform>().getPosition();
        pos.z = ((CamMax - CamMin) * cro::Util::Easing::easeOutQuad(progress)) + CamMin;
        e.getComponent<cro::Transform>().setPosition(pos);

        if (progress == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            setNextCam();
        }
    };


    for (auto i = 0; i < CameraID::Count; ++i)
    {
        //TODO read shadow map size from settings
        const std::uint32_t ShadowSize = ShadowMapHigh;

        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Camera>().resizeCallback = resize;
        entity.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowSize, ShadowSize);
        entity.getComponent<cro::Camera>().active = false;
        resize(entity.getComponent<cro::Camera>());
        m_cameras[i] = entity;
    }
    
    //set up each cam's unique behaviour
    auto axisEnt = m_gameScene.createEntity();
    axisEnt.addComponent<cro::Transform>().addChild(m_cameras[CameraID::Rotate].getComponent<cro::Transform>());
    m_cameras[CameraID::Rotate].getComponent<cro::Transform>().setPosition({ 0.f, 2.1f, 7.f });
    m_cameras[CameraID::Rotate].addComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(0.5f, 0);
    m_cameras[CameraID::Rotate].getComponent<cro::Callback>().function =
        [&, axisEnt](cro::Entity e, float dt) mutable
    {
        const float Speed = dt * 0.166667f;

        auto& [progress, direction] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
        if (direction == 0)
        {
            progress = std::min(1.f, progress + Speed);
            if (progress == 1)
            {
                direction = 1;
            }
        }
        else
        {
            progress = std::max(0.f, progress - Speed);
            if (progress == 0)
            {
                direction = 0;

                e.getComponent<cro::Callback>().active = false;
                setNextCam();
            }
        }        

        constexpr float StartAngle = cro::Util::Const::PI * 0.4f;
        constexpr float MaxRotation = cro::Util::Const::PI - (StartAngle * 2.f);
        float rotation = (MaxRotation * cro::Util::Easing::easeInOutQuad(progress)) -(MaxRotation / 2.f);
        axisEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
    };

    m_cameras[CameraID::Pass].getComponent<cro::Transform>().setPosition({ 3.f, 1.1f, 3.f });
    m_cameras[CameraID::Pass].addComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(1.f, 1);
    m_cameras[CameraID::Pass].getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        const float Speed = dt * 0.166667f;
        constexpr float XOffset = 3.f;

        auto& [progress, direction] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
        if (direction == 0)
        {
            progress = std::min(1.f, progress + Speed);
            if (progress == 1)
            {
                direction = 1;
                e.getComponent<cro::Callback>().active = false;
                setNextCam();
            }
        }
        else
        {
            progress = std::max(0.f, progress - Speed);
            if (progress == 0)
            {
                direction = 0;

                e.getComponent<cro::Callback>().active = false;
                setNextCam();
            }
        }
    
        //update position
        float offset = (progress * 2.f) - 1.f;
        offset *= XOffset;

        auto pos = e.getComponent<cro::Transform>().getPosition();
        pos.x = offset;
        e.getComponent<cro::Transform>().setPosition(pos);
    };

    m_cameras[CameraID::Static].getComponent<cro::Transform>().setPosition({ -3.f, 0.3f, 3.5f });
    m_cameras[CameraID::Static].getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.7f);
    m_cameras[CameraID::Static].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, 0.4f);
    m_cameras[CameraID::Static].addComponent<cro::Callback>().setUserData<float>(5.f);
    m_cameras[CameraID::Static].getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;

        if (currTime < 0.f)
        {
            currTime += 5.f;
            e.getComponent<cro::Callback>().active = false;
            setNextCam();
        }
    };


    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.4f);
}

void GCState::createUI()
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 1.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(1.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(1.f, 0.f), cro::Colour::White),
        });

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(1.f, 0);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        const float Speed = dt * 0.4f;
        
        auto& [progress, direction] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
        if (direction == 0)
        {
            //fade out
            progress = std::max(0.f, progress - Speed);
        }
        else
        {
            //fade in
            progress = std::min(1.f, progress + Speed);

            if (progress == 1)
            {
                m_music.stop();
                direction = 0;
                e.getComponent<cro::Callback>().active = false;
                requestStackPop();
            }
        }

        //any time the fade is active update the underlying
        //state to allow it to complete any transition effect
        m_processReturn = progress != 0;

        const float alpha = cro::Util::Easing::easeOutQuint(progress);
        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(alpha);
        }
        
        m_music.setVolume((1.f - progress) * std::max(0.2f, cro::AudioMixer::getVolume(MixerChannel::Music)));

        glm::vec2 size(cro::App::getWindow().getSize());
        e.getComponent<cro::Transform>().setScale(size);
    };
    m_fadeEnt = entity;


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}

void GCState::setNextCam()
{
    m_cameras[m_cameraIndex].getComponent<cro::Callback>().active = true;

    //TODO reset the callback if applicable

    m_gameScene.getActiveCamera().getComponent<cro::Camera>().active = false;
    m_gameScene.setActiveCamera(m_cameras[m_cameraIndex]);
    m_gameScene.getActiveCamera().getComponent<cro::Camera>().active = true;

    m_cameraIndex = (m_cameraIndex + 1) % m_cameras.size();
}

void GCState::quitState()
{
    m_fadeEnt.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 1;
}
