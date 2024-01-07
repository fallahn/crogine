//Auto-generated source file for Scratchpad Stub 28/11/2023, 13:22:27

#include "InteriorMappingState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/BinaryMeshBuilder.hpp>
#include <crogine/graphics/ImageArray.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtx/euler_angles.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>

namespace ds
{
    struct Trigger final
    {
        enum
        {
            Left, Right,

            Count
        };
    };

    struct TriggerMask final
    {
        static constexpr std::uint8_t Left  = 0x1;
        static constexpr std::uint8_t Right = 0x2;
        static constexpr std::uint8_t Both  = Left | Right;
    };

    struct Mode final
    {
        enum
        {
            Off,
            Feedback,
            Weapon,
            Vibrate,
            MultiPositionFeedback,
            SlopFeedback,
            MultiPositionVibrate
        };
    };

    //sets all values to zero
    struct ParamReset final
    {
        std::uint8_t padding[48] = { 0 };
    };

    //feedback parameters
    //trigger pushes back against the player
    //with stiffness applied at the provided position
    struct ParamFeedback final
    {
        std::uint8_t position    =   0; //0-9 point at which push back starts to happen
        std::uint8_t strength    =   0; //0-8 strength of push back
        std::uint8_t padding[46] = { 0 };
    };

    //weapon parameters
    //make the trigger act like a gun trigger
    struct ParamWeapon final
    {
        std::uint8_t startPosition =   0; //2-7 start of trigger stiffness
        std::uint8_t endPosition   =   0; //startPosition + (1-8) end of trigger stiffness
        std::uint8_t strength      =   0; //0-8 strength of trigger push back
        std::uint8_t padding[45]   = { 0 };
    };

    //vibration parameters
    struct ParamVibrate final
    {
        std::uint8_t position    =   0; //0-9 position of trigger to start vibration
        std::uint8_t strength    =   0; //0-8 stength of vibration
        std::uint8_t frequency   =   0; //0-255 vibration frequency in hertz
        std::uint8_t padding[45] = { 0 };
    };

    static constexpr std::int32_t NumFeedbackPositions = 10;

    //set the strength of feedback at positions 0-NumFeedbackPositions
    struct ParamMultiPosFeedback final
    {
        std::uint8_t strengths[NumFeedbackPositions] = { 0 };
        std::uint8_t padding[38]                     = { 0 };
    };


    //parameters for slope feedback
    //slope feedback has 3 points at which the feedback strength changes
    struct ParamSlopeFeedback final
    {
        std::uint8_t startPosition =   0; //0-endPosition position at which strength starts changing
        std::uint8_t endPosition   =   0; //startPosition + (1-9) positions at which strength stops changing
        std::uint8_t startStrength =   0; //0-8 strength at start position
        std::uint8_t endStrength   =   0; //0-8 strength at end position
        std::uint8_t padding[44]   = { 0 };
    };

    //paramters for vibration with multiple points
    struct ParamMultiPosVibrate final
    {
        std::uint8_t frequency                       =   0; //0-255 frequency of vibration
        std::uint8_t strengths[NumFeedbackPositions] = { 0 }; //0-8 strength at each position
        std::uint8_t padding[37]                     = { 0 };
    };

    //union of all param types
    union EffectParams
    {
        ParamReset reset = {};
        ParamFeedback feedback;
        ParamWeapon weapon;
        ParamVibrate vibrate;
        ParamMultiPosFeedback multiPositionFeedback;
        ParamSlopeFeedback slopeFeedback;
        ParamMultiPosVibrate multiPositionVibrate;
    };

    //effect struct for mode and params
    struct Effect final
    {
        std::uint8_t mode = 0;
        const std::uint8_t Padding[4] = { 0 };
        EffectParams parameters;
    };
    

    //effect packet to send to the controller
    struct EffectPacket final
    {
        std::uint8_t triggerMask = 0; //set to TriggerMask::Left, Right or Both
        const std::uint8_t Padding[9] = { 0 };

        Effect effects[Trigger::Count] = {};
    };
}


typedef struct
{
    Uint8 ucEnableBits1;              /* 0 */
    Uint8 ucEnableBits2;              /* 1 */
    Uint8 ucRumbleRight;              /* 2 */
    Uint8 ucRumbleLeft;               /* 3 */
    Uint8 ucHeadphoneVolume;          /* 4 */
    Uint8 ucSpeakerVolume;            /* 5 */
    Uint8 ucMicrophoneVolume;         /* 6 */
    Uint8 ucAudioEnableBits;          /* 7 */
    Uint8 ucMicLightMode;             /* 8 */
    Uint8 ucAudioMuteBits;            /* 9 */
    Uint8 rgucRightTriggerEffect[11]; /* 10 */
    Uint8 rgucLeftTriggerEffect[11];  /* 21 */
    Uint8 rgucUnknown1[6];            /* 32 */
    Uint8 ucLedFlags;                 /* 38 */
    Uint8 rgucUnknown2[2];            /* 39 */
    Uint8 ucLedAnim;                  /* 41 */
    Uint8 ucLedBrightness;            /* 42 */
    Uint8 ucPadLights;                /* 43 */
    Uint8 ucLedRed;                   /* 44 */
    Uint8 ucLedGreen;                 /* 45 */
    Uint8 ucLedBlue;                  /* 46 */
} DS5EffectsState_t;

std::int32_t trigger_effect = 0;
static void CyclePS5TriggerEffect(SDL_GameController* gameController)
{
    DS5EffectsState_t state;

    /*
    * Mode
    * 
    */

    Uint8 effects[3][11] = {
        /* Clear trigger effect */
        { 0x05, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        /* Constant resistance across entire trigger pull */
        { 0x01, 0, 110, 0, 0, 0, 0, 0, 0, 0, 0 },
        /* Resistance and vibration when trigger is pulled */
        { 0x06, 15, 63, 128, 0, 0, 0, 0, 0, 0, 0 },
    };

    trigger_effect = (trigger_effect + 1) % SDL_arraysize(effects);

    SDL_zero(state);
    state.ucEnableBits1 |= (0x04 | 0x08); /* Modify right and left trigger effect respectively */
    SDL_memcpy(state.rgucRightTriggerEffect, effects[trigger_effect], sizeof(effects[trigger_effect]));
    SDL_memcpy(state.rgucLeftTriggerEffect, effects[trigger_effect], sizeof(effects[trigger_effect]));
    SDL_GameControllerSendEffect(gameController, &state, sizeof(state));
}



namespace
{
#include "IMShader.inl"
#include "ProjectionShader.inl"

    struct ShaderID final
    {
        enum
        {
            IntMapping,
            Projection,

            Count
        };
    };
    std::array<std::uint32_t, ShaderID::Count> shaderIDs = {};

    struct MaterialID final
    {
        enum
        {
            InMapping = 1,
            Projection
        };
    };

    struct ShaderUniforms final
    {
        std::uint32_t shaderID = 0;
        std::int32_t uv = -1;
        std::int32_t roomSize = -1;
        std::int32_t backplane = -1;
    }shaderUniforms;
}

InteriorMappingState::InteriorMappingState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        /*loadAssets();
        createScene();*/

        loadCullingAssets();
        createCullingScene();
        createUI();
    });

    registerWindow([]()
        {
            if (ImGui::Begin("Controller"))
            {
                if (cro::GameController::hasPSLayout(0))
                {
                    if (ImGui::Button("Apply Effect"))
                    {
                        if (auto* controller = SDL_GameControllerFromInstanceID(0); controller)
                        {
                            //ds::EffectPacket packet;
                            //packet.triggerMask = 0x04 | 0x08;// ds::TriggerMask::Left;
                            //packet.effects[ds::Trigger::Left].mode = ds::Mode::Vibrate;
                            //packet.effects[ds::Trigger::Left].parameters.vibrate.frequency = 20;
                            //packet.effects[ds::Trigger::Left].parameters.vibrate.position = 5;
                            //packet.effects[ds::Trigger::Left].parameters.vibrate.strength = 8;

                            //assert(SDL_GameControllerGetType(controller) == SDL_CONTROLLER_TYPE_PS5);

                            //auto result = SDL_GameControllerSendEffect(controller, &packet, sizeof(ds::EffectPacket));
                            //if (result == -1)
                            //{
                            //    LogI << "Controller doesn't support effect" << std::endl;
                            //}
                            CyclePS5TriggerEffect(controller);
                        }
                        else
                        {
                            LogI << "Controller is nullptr" << std::endl;
                        }
                    }
                }
                else
                {
                    ImGui::Text("No PS controller detected");
                }
            }
            ImGui::End();
        });
}

//public
bool InteriorMappingState::handleEvent(const cro::Event& evt)
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

    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            cro::App::getWindow().setMouseCaptured(true);
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            cro::App::getWindow().setMouseCaptured(false);
            break;
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        static constexpr float PixelsPerRad = 380.f; //obvs this doesn't scale with window size...

        if (cro::App::getWindow().getMouseCaptured())
        {
            auto& tx = m_gameScene.getActiveCamera().getComponent<cro::Transform>();

            const float xMotion = static_cast<float>(evt.motion.xrel) / PixelsPerRad;
            const float yMotion = static_cast<float>(evt.motion.yrel) / PixelsPerRad;
            const auto upVec = glm::inverse(glm::toMat3(tx.getRotation())) * cro::Transform::Y_AXIS;

            tx.rotate(upVec, -xMotion);
            tx.rotate(cro::Transform::X_AXIS, -yMotion);
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void InteriorMappingState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool InteriorMappingState::simulate(float dt)
{
    glm::vec3 motion(0.f);
    if (cro::Keyboard::isKeyPressed(SDLK_w))
    {
        motion.z += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_s))
    {
        motion.z -= 1.f;
    }

    if (cro::Keyboard::isKeyPressed(SDLK_d))
    {
        motion.x += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_a))
    {
        motion.x -= 1.f;
    }

    if (cro::Keyboard::isKeyPressed(SDLK_SPACE))
    {
        motion.y += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_LCTRL))
    {
        motion.y -= 1.f;
    }

    if (auto l2 = glm::length2(motion); l2 != 0)
    {
        static constexpr float Speed = 50.f;
        auto& tx = m_gameScene.getActiveCamera().getComponent<cro::Transform>();

        motion /= std::sqrt(l2);
        motion *= Speed * dt;

        const auto fwd = tx.getForwardVector();
        tx.move(fwd * motion.z);

        const auto rgt = tx.getRightVector();
        tx.move(rgt * motion.x);

        const auto up = tx.getUpVector();
        tx.move(up * motion.y);
    }

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);


    const auto* system = m_gameScene.getSystem<ChunkVisSystem>();
    static std::int32_t oldIdx = 0;
    auto idx = system->getIndex();

    if (idx &&
        idx != oldIdx)
    {
        const auto& indices = system->getIndexList();
        const auto chunkSize = system->getChunkSize();

        std::vector<const std::vector<glm::mat4>*> transforms;
        std::vector<const std::vector<glm::mat3>*> normals;

        m_cullingDebugTexture.clear(cro::Colour::Plum);
        for (auto i : indices)
        {
            //we're assuming if a transform was added the corresponding normal mat exists
            if (!m_cells[i].transforms.empty())
            {
                transforms.push_back(&m_cells[i].transforms);
                normals.push_back(&m_cells[i].normals);
            }

            auto x = i % ChunkVisSystem::ColCount;
            auto y = i / ChunkVisSystem::ColCount;

            m_cullingDebugVerts.setPosition({ x * chunkSize.x, y * chunkSize.y });
            m_cullingDebugVerts.draw();
        }
        m_cullingDebugTexture.display();

        if (!transforms.empty())
        {
            m_entities[EntityID::InstancedCulled].getComponent<cro::Model>().updateInstanceTransforms(transforms, normals);
        }
    }
    oldIdx = idx;

    return true;
}

void InteriorMappingState::render()
{
    m_profileTimer.begin();
    m_gameScene.render();
    m_profileTimer.end();

    m_uiScene.render();
}

//private
void InteriorMappingState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<ChunkVisSystem>(mb, glm::vec2(320.f, 200.f));
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void InteriorMappingState::loadAssets()
{
    m_resources.shaders.loadFromString(ShaderID::IntMapping, IMVertex, IMFragment2);
    auto* shader = &m_resources.shaders.get(ShaderID::IntMapping);
    
    auto& tex = m_resources.textures.get("assets/images/cup.png");
    tex.setRepeated(true);
    m_resources.materials.add(MaterialID::InMapping, *shader);
    m_resources.materials.get(MaterialID::InMapping).setProperty("u_roomTexture", tex);

    //m_cubemap.loadFromFile("assets/images/0/cmap.ccm");
    //m_resources.materials.get(MaterialID::InMapping).setProperty("u_roomTexture", cro::CubemapID(m_cubemap));

    shaderUniforms.shaderID = shader->getGLHandle();
    //shaderUniforms.roomSize = shader.getUniformID("u_roomSize");
    shaderUniforms.uv = shader->getUniformID("u_texCoordScale");
    shaderUniforms.backplane = shader->getUniformID("u_backPlaneScale");


    m_resources.shaders.loadFromString(ShaderID::Projection, ProjectionVertex, ProjectionFragment);
    shader = &m_resources.shaders.get(ShaderID::Projection);
    m_resources.materials.add(MaterialID::Projection, *shader);
}

void InteriorMappingState::createScene()
{
    cro::ModelDefinition md(m_resources);
    //if (md.loadFromFile("assets/models/cylinder_tangents.cmt"))
    //{
    //    auto entity = m_gameScene.createEntity();
    //    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.f });
    //    md.createModel(entity);

    //    glm::mat4 proj = glm::ortho(-0.3f, 0.3f, -0.3f, 0.3f, -5.f, 5.f);
    //    glm::mat4 view = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -1.f, 0.f));

    //    auto material = m_resources.materials.get(MaterialID::Projection);
    //    material.setProperty("u_targetViewProjectionMatrix", proj * view);
    //    entity.getComponent<cro::Model>().setMaterial(0, material);

    //    /*entity.addComponent<cro::Callback>().active = true;
    //    entity.getComponent<cro::Callback>().function =
    //        [](cro::Entity e, float dt)
    //        {
    //            e.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, dt);
    //            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
    //        };*/

    //    registerWindow([entity]() mutable
    //        {
    //            if (ImGui::Begin("Shader"))
    //            {
    //                static glm::vec3 position(0.f, 1.f, 0.f);
    //                static float rotation = 0.f;
    //                static float size = 0.3f;

    //                static auto updateShader = [entity]() mutable
    //                    {
    //                        glm::mat4 proj = glm::ortho(-size, size, -size, size, -5.f, 5.f);
    //                        glm::mat4 view = glm::translate(glm::mat4(1.f), -position);

    //                        auto rot = glm::rotate(cro::Transform::QUAT_IDENTITY, -rotation, cro::Transform::X_AXIS);
    //                        view *= glm::toMat4(rot);

    //                        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_targetViewProjectionMatrix", proj * view);
    //                    };

    //                if (ImGui::SliderFloat("Size##t", &size, 0.1f, 0.6f))
    //                {
    //                    size = std::clamp(size, 0.1f, 0.6f);
    //                    updateShader();
    //                }
    //                if (ImGui::SliderFloat("X##t", &position.x, -1.f, 1.f))
    //                {
    //                    position.x = std::clamp(position.x, -1.f, 1.f);
    //                    updateShader();
    //                }
    //                if (ImGui::SliderFloat("Y##t", &position.y, 0.f, 2.f))
    //                {
    //                    position.y = std::clamp(position.y, 0.f, 2.f);
    //                    updateShader();
    //                }
    //                if (ImGui::SliderFloat("Rotation##t", &rotation, -cro::Util::Const::PI / 2.f, cro::Util::Const::PI))
    //                {
    //                    rotation = std::clamp(rotation, -cro::Util::Const::PI / 2.f, cro::Util::Const::PI / 2.f);
    //                    updateShader();
    //                }
    //            }
    //            ImGui::End();
    //        });
    //}
    
    //if (md.loadFromFile("assets/models/cylinder_tangents.cmt"))
    //{
    //    auto entity = m_gameScene.createEntity();
    //    entity.addComponent<cro::Transform>().setPosition({ -1.5, 0.f, 0.f });
    //    md.createModel(entity);

    //    entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(MaterialID::InMapping));

    //    //TODO refactor this to a sngle func that take the entity as a param
    //    registerWindow([&, entity]() mutable
    //        {
    //            if (ImGui::Begin("Cylinder"))
    //            {
    //                auto pos = entity.getComponent<cro::Transform>().getPosition();

    //                if (ImGui::SliderFloat("X", &pos.x, -3.f, 3.f))
    //                {
    //                    pos.x = std::clamp(pos.x, -3.f, 3.f);
    //                    entity.getComponent<cro::Transform>().setPosition(pos);
    //                }

    //                if (ImGui::SliderFloat("Y", &pos.y, -2.f, 2.f))
    //                {
    //                    pos.y = std::clamp(pos.y, -2.f, 2.f);
    //                    entity.getComponent<cro::Transform>().setPosition(pos);
    //                }

    //                static glm::vec3 rotation = glm::vec3(0.f);
    //                if (ImGui::SliderFloat("Rotation X", &rotation[0], -cro::Util::Const::PI, cro::Util::Const::PI))
    //                {
    //                    rotation.x = std::clamp(rotation[0], -cro::Util::Const::PI, cro::Util::Const::PI);
    //                    entity.getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(rotation)));
    //                }
    //            }
    //            ImGui::End();
    //        });
    //}

    if (md.loadFromFile("assets/models/plane_tangents.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(MaterialID::InMapping));

        registerWindow([&, entity]() mutable
            {
                if (ImGui::Begin("Plane"))
                {
                    ImGui::BeginChild("##0", {320.f, 0.f}, true);
                    auto pos = entity.getComponent<cro::Transform>().getPosition();
                    
                    if (ImGui::SliderFloat("X", &pos.x, -3.f, 3.f))
                    {
                        pos.x = std::clamp(pos.x, -3.f, 3.f);
                        entity.getComponent<cro::Transform>().setPosition(pos);
                    }
                    
                    if (ImGui::SliderFloat("Y", &pos.y, -2.f, 2.f))
                    {
                        pos.y = std::clamp(pos.y, -2.f, 2.f);
                        entity.getComponent<cro::Transform>().setPosition(pos);
                    }

                    static glm::vec3 rotation = glm::vec3(0.f);
                    if (ImGui::SliderFloat("Rotation X", &rotation[0], -cro::Util::Const::PI, cro::Util::Const::PI))
                    {
                        rotation.x = std::clamp(rotation[0], -cro::Util::Const::PI, cro::Util::Const::PI);
                        entity.getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(rotation)));
                    }
                    if (ImGui::SliderFloat("Rotation Y", &rotation[2], -cro::Util::Const::PI, cro::Util::Const::PI))
                    {
                        rotation.z = std::clamp(rotation[2], -cro::Util::Const::PI, cro::Util::Const::PI);
                        entity.getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(rotation)));
                    }
                    if (ImGui::SliderFloat("Rotation Z", &rotation[1], -cro::Util::Const::PI, cro::Util::Const::PI))
                    {
                        rotation.y = std::clamp(rotation[1], -cro::Util::Const::PI, cro::Util::Const::PI);
                        entity.getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(rotation)));
                    }
                    if (ImGui::Button("Reset"))
                    {
                        rotation = glm::vec3(0.f);
                        entity.getComponent<cro::Transform>().setRotation(glm::toQuat(glm::orientate3(rotation)));
                    }

                    static glm::vec3 scale(1.f);
                    if (ImGui::SliderFloat("Scale X", &scale.x, 0.5f, 2.f))
                    {
                        scale.x = std::clamp(scale.x, 0.5f, 2.f);
                        entity.getComponent<cro::Transform>().setScale(scale);
                    }
                    ImGui::EndChild();


                    ImGui::SameLine();


                    ImGui::BeginChild("##1", { 320.f, 0.f }, true);
                    
                    ImGui::Text("TexCoords");
                    static glm::vec2 uvScale(1.f);
                    if (ImGui::SliderFloat("U", &uvScale.x, 0.1f, 3.f))
                    {
                        uvScale.x = std::clamp(uvScale.x, 0.1f, 3.f);
                        glUseProgram(shaderUniforms.shaderID);
                        glUniform2f(shaderUniforms.uv, uvScale.x, uvScale.y);
                    }
                    if (ImGui::SliderFloat("V", &uvScale.y, 0.1f, 3.f))
                    {
                        uvScale.x = std::clamp(uvScale.x, 0.1f, 3.f);
                        glUseProgram(shaderUniforms.shaderID);
                        glUniform2f(shaderUniforms.uv, uvScale.x, uvScale.y);
                    }

                    ImGui::Text("Room Size");
                    static glm::vec3 roomScale(1.f);

                    if (ImGui::SliderFloat("X", &roomScale.x, 0.5f, 3.f))
                    {
                        roomScale.x = std::clamp(roomScale.x, 0.5f, 3.f);
                        glUseProgram(shaderUniforms.shaderID);
                        glUniform3f(shaderUniforms.roomSize, roomScale.x, roomScale.y, roomScale.z);
                    }
                    if (ImGui::SliderFloat("Y", &roomScale.y, 0.5f, 3.f))
                    {
                        roomScale.y = std::clamp(roomScale.y, 0.5f, 3.f);
                        glUseProgram(shaderUniforms.shaderID);
                        glUniform3f(shaderUniforms.roomSize, roomScale.x, roomScale.y, roomScale.z);
                    }
                    if (ImGui::SliderFloat("Z", &roomScale.z, 0.5f, 3.f))
                    {
                        roomScale.z = std::clamp(roomScale.z, 0.5f, 3.f);
                        glUseProgram(shaderUniforms.shaderID);
                        glUniform3f(shaderUniforms.roomSize, roomScale.x, roomScale.y, roomScale.z);
                    }

                    static float bpScale = 0.5f;
                    if (ImGui::SliderFloat("BP Scale", &bpScale, 0.f, 2.f))
                    {
                        bpScale = std::clamp(bpScale, 0.f, 2.f);
                        glUseProgram(shaderUniforms.shaderID);
                        glUniform1f(shaderUniforms.backplane, bpScale);
                    }

                    ImGui::EndChild();
                }
                ImGui::End();
            });
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

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 1.f, 3.5f });
    //m_gameScene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.4f);
}

void InteriorMappingState::loadCullingAssets()
{
    const auto Size = m_gameScene.getSystem<ChunkVisSystem>()->getChunkSize();

    m_cullingDebugVerts.setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, Size.y), cro::Colour::Green),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::Green),
            cro::Vertex2D(Size, cro::Colour::Green),
            cro::Vertex2D(glm::vec2(Size.x, 0.f), cro::Colour::Green)
        }
    );
    m_cullingDebugVerts.setPrimitiveType(GL_TRIANGLE_STRIP);
}

void InteriorMappingState::createCullingScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/models/map_plane.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        m_cullingDebugTexture.create(320, 200, false);
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", cro::TextureID(m_cullingDebugTexture.getTexture()));
    }

    std::vector<glm::mat4> positions;
    cro::ImageArray<std::uint8_t> arr;
    if (arr.loadFromFile("assets/images/instance_map.png"))
    {
        for (auto i = 0u; i < arr.size(); i += arr.getChannels())
        {
            if (arr[i])
            {
                if (cro::Util::Random::value(0, 6) == 0)
                {
                    auto x = (i / arr.getChannels()) % arr.getDimensions().x;
                    auto y = (i / arr.getChannels()) / arr.getDimensions().x;

                    float scale = static_cast<float>(cro::Util::Random::value(5, 40)) / 10.f;

                    auto tx = glm::translate(glm::mat4(1.f), glm::vec3(static_cast<float>(x), 0.f, -static_cast<float>(y)));
                    tx = glm::scale(tx, glm::vec3(scale));

                    positions.push_back(tx);

                    auto norm = glm::inverseTranspose(tx);

                    x /= std::ceil(m_gameScene.getSystem<ChunkVisSystem>()->getChunkSize().x);
                    y /= std::ceil(m_gameScene.getSystem<ChunkVisSystem>()->getChunkSize().y);

                    auto idx = y * ChunkVisSystem::ColCount + x;
                    m_cells[idx].transforms.push_back(tx);
                    m_cells[idx].normals.push_back(norm);
                }
            }
        }
    }

    if (md.loadFromFile("assets/models/cube.cmt", true))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        entity.getComponent<cro::Model>().setInstanceTransforms(positions);
        m_entities[EntityID::Instanced] = entity;
    }

    if (md.loadFromFile("assets/models/cube2.cmt", true))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        entity.getComponent<cro::Model>().setInstanceTransforms(positions);
        entity.getComponent<cro::Model>().setHidden(true);
        m_entities[EntityID::InstancedCulled] = entity;
    }



    auto resize = [](cro::Camera& cam)
        {
            glm::vec2 size(cro::App::getWindow().getSize());
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
            cam.setPerspective(80.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 350.f);
        };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 160.f, 60.f, 30.5f });
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.5f);

    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.4f);
}

void InteriorMappingState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Culling"))
            {
                ImGui::Text("Render Time %3.3f", m_profileTimer.result() * 1000.f);

                bool v = m_entities[EntityID::Instanced].getComponent<cro::Model>().isHidden();
                if (ImGui::Checkbox("Hide Instanced", &v))
                {
                    m_entities[EntityID::Instanced].getComponent<cro::Model>().setHidden(v);
                    m_entities[EntityID::InstancedCulled].getComponent<cro::Model>().setHidden(!v);
                }

                v = !v;
                if (ImGui::Checkbox("Hide Instance Culled", &v))
                {
                    m_entities[EntityID::Instanced].getComponent<cro::Model>().setHidden(!v);
                    m_entities[EntityID::InstancedCulled].getComponent<cro::Model>().setHidden(v);
                }

                const auto* s = m_gameScene.getSystem<ChunkVisSystem>();
                static std::int32_t lastFlags = 0;

                auto idx = s->getIndex();

                ImVec4 c = lastFlags == idx ? ImVec4(0.f, 1.f, 0.f, 1.f) : ImVec4(1.f, 0.f, 0.f, 1.f);
                ImGui::PushStyleColor(ImGuiCol_Text, c);
                ImGui::Text("Flags: %d", idx);
                ImGui::PopStyleColor();
                lastFlags = idx;

#ifdef CRO_DEBUG_
                ImGui::Text("Narrowphase %3.3fms, %d visible", s->getNarrowphaseTime() * 1000.f, s->narrowphaseCount);
#endif
                //ImGui::Image(m_cullingDebugTexture.getTexture(), { 320.f, 200.f }, { 0.f, 1.f }, { 1.f, 0.f });
            }
            ImGui::End();
        
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