/*-----------------------------------------------------------------------

Matt Marchant 2023
http://trederia.blogspot.com

crogine application - Zlib license.

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

//Auto-generated source file for Scratchpad Stub 05/01/2023, 21:01:55

#include "AnimBlendState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/Texture.hpp>

namespace
{

}

AnimBlendState::AnimBlendState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
bool AnimBlendState::handleEvent(const cro::Event& evt)
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
            requestStackClear();
            requestStackPush(0);
            break;

        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void AnimBlendState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool AnimBlendState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void AnimBlendState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void AnimBlendState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void AnimBlendState::loadAssets()
{
    
}

void AnimBlendState::createScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/models/01.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.2f);
        md.createModel(entity);

        if (entity.hasComponent<cro::Skeleton>())
        {
            auto animCount = entity.getComponent<cro::Skeleton>().getAnimations().size();

            //this makes some suppositions about the model, but we're
            //here for a reason...
            if (animCount > 2)
            {
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(2.f);
                entity.getComponent<cro::Callback>().function =
                    [&, animCount](cro::Entity e, float dt)
                {
                    auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                    currTime -= dt * m_gameScene.getSystem<cro::SkeletalAnimator>()->getPlaybackRate();

                    if (currTime < 0)
                    {
                        e.getComponent<cro::Skeleton>().play(cro::Util::Random::value(0, 1) ? 0 : (animCount - 1), 1.f, 0.5f);
                        currTime = static_cast<float>(cro::Util::Random::value(8, 16));
                    }

                    if (e.getComponent<cro::Skeleton>().getState() == cro::Skeleton::Stopped)
                    {
                        e.getComponent<cro::Skeleton>().play(1, 1.f, 0.5f);
                    }
                };
            }
            entity.getComponent<cro::Skeleton>().play(1, 1.f);
        }

        m_modelEntity = entity;


        const std::array pos =
        {
            glm::vec3(1.f, 0.f, 0.f),
            glm::vec3(1.f, 0.f, -1.f),
            glm::vec3(-1.f, 0.f, -1.f),
        };

        for (auto i = 0u; i < 3u; ++i)
        {
            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(pos[i]);
            entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, cro::Util:: Const::PI + (i * (cro::Util::Const::PI / 2.f)));
            md.createModel(entity);
            if (entity.hasComponent<cro::Skeleton>())
            {
                entity.getComponent<cro::Skeleton>().play(1);
                entity.addComponent<cro::Callback>() = m_modelEntity.getComponent<cro::Callback>();
            }
        }
    }

    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.8f, 1.f });
}

void AnimBlendState::createUI()
{
    registerWindow([&]()
        {
            if (ImGui::Begin("League"))
            {
                const auto& entries = m_league.getTable();
                for (const auto& e : entries)
                {
                    ImGui::Text("Skill: %d - Curve: %d - Score: %d - Name: %d", e.skill, e.curve, e.currentScore, e.nameIndex);
                }
                ImGui::Text("Iteratation: %d", m_league.getCurrentIteration());
                ImGui::SameLine();
                if (ImGui::Button("Iterate"))
                {
                    m_league.iterate();
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset"))
                {
                    m_league.reset();
                }

                if (ImGui::Button("Run 10 Seasons"))
                {
                    for (auto i = 0; i < 10; ++i)
                    {
                        for (auto j = 0; j < League::MaxIterations; ++j)
                        {
                            m_league.iterate();
                        }
                    }
                    LogI << "Done!" << std::endl;
                }
            }
            ImGui::End();

            if (m_modelEntity.isValid())
            {
                if (ImGui::Begin("Controls"))
                {
                    if (m_modelEntity.hasComponent<cro::Skeleton>())
                    {
                        static bool step = false;
                        if (step)
                        {
                            if (ImGui::Button("Play Mode"))
                            {
                                step = false;
                                m_gameScene.setSystemActive<cro::SkeletalAnimator>(true);
                                m_modelEntity.getComponent<cro::Callback>().active = true;
                                m_modelEntity.getComponent<cro::Skeleton>().play(0, 1.f, 0.5f);
                            }

                            ImGui::Separator();
                            static float stepRate = 1.f / 60.f;
                            ImGui::SliderFloat("Step Rate", &stepRate, 1.f / 60.f, 0.2f);
                            if (ImGui::Button("Step"))
                            {
                                m_gameScene.getSystem<cro::SkeletalAnimator>()->process(stepRate);
                            }
                        }
                        else
                        {
                            if (ImGui::Button("Step Mode"))
                            {
                                step = true;
                                m_gameScene.setSystemActive<cro::SkeletalAnimator>(false);
                                m_modelEntity.getComponent<cro::Callback>().active = false;
                            }
                        }

                        ImGui::NewLine();
                        bool interp = m_modelEntity.getComponent<cro::Skeleton>().getInterpolationEnabled();
                        if (ImGui::Checkbox("Interpolation", &interp))
                        {
                            m_modelEntity.getComponent<cro::Skeleton>().setInterpolationEnabled(interp);
                        }
                        m_gameScene.getSystem<cro::SkeletalAnimator>()->debugUI();
                    }
                    else
                    {
                        ImGui::Text("No Skeleton In Model");
                    }
                }
                ImGui::End();
            }
        });

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