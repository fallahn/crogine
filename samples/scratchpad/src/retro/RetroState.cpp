/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

#include "RetroState.hpp"

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

#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Random.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
#include "RetroShaders.inl"

    const std::string FlareFrag =
R"(
uniform sampler2D u_texture;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

void main()
{
    vec2 offset = ((v_colour.gb * 2.0) - 1.0) * 0.01;
    float r = TEXTURE(u_texture, v_texCoord + offset).r;
    float g = TEXTURE(u_texture, v_texCoord).r;
    float b = TEXTURE(u_texture, v_texCoord - offset).r;

    FRAG_OUT.rgb =  vec3(r,g,b) * v_colour.r;
    FRAG_OUT.a = 1.0;
}

)";

    const std::string PTZVertex = 
        R"(
        ATTRIBUTE vec2 a_position;
        ATTRIBUTE vec2 a_texCoord0;
        ATTRIBUTE vec4 a_colour;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;

        uniform mat4 u_coordMatrix = mat4(1.0);

        VARYING_OUT vec2 v_texCoord0;
        VARYING_OUT vec2 v_texCoord1;
        VARYING_OUT vec4 v_colour;

        void main()
        {
            gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
            v_texCoord0 = (u_coordMatrix * vec4(a_texCoord0, 0.0, 1.0)).xy;
            v_texCoord1 = a_texCoord0;
            v_colour = a_colour;
        })";
    const std::string PTZFragment =
        R"(
        OUTPUT
        uniform sampler2D u_texture;

        VARYING_IN vec2 v_texCoord0;
        VARYING_IN vec2 v_texCoord1;
        VARYING_IN vec4 v_colour;

        const float RadiusOuter = (0.48 * 0.48);
        const float RadiusInner = (0.4 * 0.4);

        void main()
        {
            FRAG_OUT = TEXTURE(u_texture, v_texCoord0) * v_colour;

            vec2 pos = v_texCoord1 - vec2(0.5);
            float len2 = dot(pos, pos);
            FRAG_OUT.a *= 1.0 - smoothstep(RadiusInner, RadiusOuter, len2);
        })";

    struct RetroShader final
    {
        enum
        {
            Ball,
            Cloud,
            Flare,
            PTZ,
        };
    };

    struct ShaderProperties final
    {
        std::uint32_t ID = 0;
        std::int32_t palette = -1;
    }shaderProperties;

    struct PTZ final
    {
        glm::vec2 pan = glm::vec2(0.f);
        float tilt = 0.f;
        float zoom = 1.f;

        std::uint32_t handle = 0;
        std::int32_t uniform = -1;
        glm::mat4 tx = glm::mat4(1.f);

        void updateShader(glm::vec2 texSize)
        {
            if (glm::length2(texSize) == 0)
            {
                return;
            }

            auto pos = pan / texSize;

            static constexpr glm::vec3 centre(0.5f, 0.5f, 0.f);
            const float aspect = texSize.x / texSize.y;

            glm::mat4 matrix(1.f);
            matrix = glm::translate(matrix, glm::vec3(pos.x, pos.y, 0.f));
            matrix = glm::scale(matrix, glm::vec3(1.f / aspect, 1.f, 1.f));
            matrix = glm::rotate(matrix, -tilt, cro::Transform::Z_AXIS);
            matrix = glm::scale(matrix, glm::vec3(aspect, 1.f, 1.f));
            matrix = glm::scale(matrix, glm::vec3(1.f / zoom));

            matrix = glm::translate(matrix, -centre);
            tx = glm::inverse(matrix);

            glUseProgram(handle);
            glUniformMatrix4fv(uniform, 1, GL_FALSE, &matrix[0][0]);
        }

        glm::vec2 toMapCoords(glm::vec3 worldCoord, glm::vec2 texSize)
        {
            if (glm::length2(texSize) == 0)
            {
                return glm::vec2(0.f);
            }

            glm::vec2 mapCoord(worldCoord.x, -worldCoord.z);
            mapCoord /= texSize;
            mapCoord = glm::vec2(tx * glm::vec4(mapCoord, 0.0, 1.0));
            return (mapCoord * texSize);
        }
    }ptz;

    struct RetroMaterial final
    {
        enum
        {
            Ball,
            Cloud,

            Count
        };
    };

    std::array<std::int32_t, RetroMaterial::Count> MaterialIDs = {};
}

RetroState::RetroState(cro::StateStack& stack, cro::State::Context context)
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
bool RetroState::handleEvent(const cro::Event& evt)
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

void RetroState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            float scale = std::round(static_cast<float>(data.data0) / m_renderTexture.getSize().x);
            m_quad.setScale(glm::vec2(scale));
        }
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool RetroState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    //needs to be AFTER scene has updated light source, but before UIscene updates
    m_lensFlare.update(m_gameScene.getActiveCamera().getComponent<cro::Camera>());
    m_uiScene.simulate(dt);


    return true;
}

void RetroState::render()
{
    //m_renderTexture.clear();
    m_gameScene.render();
    //m_renderTexture.display();

    //m_quad.draw();
    m_uiScene.render();
}

//private
void RetroState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void RetroState::loadAssets()
{
    //monkey material
    m_paletteTexture.loadFromFile("assets/retro/palette01.png");
    m_paletteTexture.setRepeated(false);

    m_resources.shaders.loadFromString(RetroShader::Ball, ShaderVertex, ShaderFragment);
    auto* shader = &m_resources.shaders.get(RetroShader::Ball);

    shaderProperties.ID = shader->getGLHandle();
    shaderProperties.palette = shader->getUniformID("u_palette");

    MaterialIDs[RetroMaterial::Ball] = m_resources.materials.add(*shader);


    //cloud material
    m_resources.shaders.loadFromString(RetroShader::Cloud, CloudVertex, CloudFragment);
    shader = &m_resources.shaders.get(RetroShader::Cloud);
    MaterialIDs[RetroMaterial::Cloud] = m_resources.materials.add(*shader);


    //lensflare
    m_resources.shaders.loadFromString(RetroShader::Flare, FlareVertex, FlareFragment);


    m_flareShader.loadFromString(cro::RenderSystem2D::getDefaultVertexShader(), FlareFrag, "#define TEXTURED\n");

    //map zoom
    if (m_resources.shaders.loadFromString(RetroShader::PTZ, PTZVertex, PTZFragment))
    {
        const auto& shader = m_resources.shaders.get(RetroShader::PTZ);
        ptz.handle = shader.getGLHandle();
        ptz.uniform = shader.getUniformID("u_coordMatrix");
    }

    //output quad (scales up for chonky pixels)
    m_renderTexture.create(640, 480);
    m_quad.setTexture(m_renderTexture.getTexture());
}

void RetroState::createScene()
{
    m_gameScene.enableSkybox();
    m_gameScene.setSkyboxColours(cro::Colour(0.2f,0.31f,0.612f,1.f), cro::Colour(0.937f, 0.678f, 0.612f, 1.f), cro::Colour(0.706f, 0.851f, 0.804f, 1.f));
    //m_gameScene.setSkyboxColours(cro::Colour(0.2f,0.31f,0.612f,1.f), cro::Colour(0.858f, 0.686f, 0.467f, 1.f), cro::Colour(0.663f, 0.729f, 0.753f, 1.f));
    //m_gameScene.setSkyboxColours(cro::Colour(0.2f,0.31f,0.612f,1.f), cro::Colour(1.f, 0.973f, 0.882f, 1.f), cro::Colour(0.723f, 0.847f, 0.792f, 1.f));

    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/retro/head.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -6.f });
        md.createModel(entity);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
            e.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, dt / 2.f);
        };

        auto material = m_resources.materials.get(MaterialIDs[RetroMaterial::Ball]);
        material.setProperty("u_palette", m_paletteTexture);
        entity.getComponent<cro::Model>().setMaterial(0, material);
    }

    const auto createCloud = [&](glm::vec3 pos)
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(pos);
        md.createModel(entity);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt / 40.f);
            //e.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, dt / 2.f);
        };

        auto material = m_resources.materials.get(MaterialIDs[RetroMaterial::Cloud]);
        material.setProperty("u_skyColourTop", m_gameScene.getSkyboxColours().top);
        material.setProperty("u_skyColourBottom", m_gameScene.getSkyboxColours().middle);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        return entity;
    };

    if (md.loadFromFile("assets/retro/cloud.cmt"))
    {
        auto entity = createCloud(glm::vec3(-2.f, 4.f, -10.f));
        entity.getComponent<cro::Transform>().setScale(glm::vec3(2.f));
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().move({ -0.5f * dt, 0.f, 0.f });
            if (e.getComponent<cro::Transform>().getPosition().x < -14.f)
            {
                e.getComponent<cro::Transform>().move({ 28.f, 0.f, 0.f });
            }
        };
    }


    if (md.loadFromFile("assets/models/sphere.cmt"))
    {
        m_lensFlare.lightSource = m_gameScene.createEntity();
        m_lensFlare.lightSource.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -10.f });
        m_lensFlare.lightSource.getComponent<cro::Transform>().setScale(glm::vec3(2.f));
        md.createModel(m_lensFlare.lightSource);
        m_lensFlare.lightSource.addComponent<cro::Callback>().active = true;
        m_lensFlare.lightSource.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
            {
                auto pos = e.getComponent<cro::Transform>().getPosition();
                pos.x += 5.f * dt;
                if (pos.x > 10.f)
                {
                    pos.x -= 20.f;
                }
                pos.y = std::sin(pos.x/2.f) * 2.f;

                e.getComponent<cro::Transform>().setPosition(pos);
            };
    }


    //this is called when the window is resized to automatically update the camera's matrices/viewport
    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&RetroState::updateView, this, std::placeholders::_1);
}

void RetroState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Lens Flare"))
            {
                auto pos = m_lensFlare.lightSource.getComponent<cro::Transform>().getPosition();
                ImGui::Text("Light Source World: %3.2f, %3.2f, %3.2f", pos.x, pos.y, pos.z);
                ImGui::Text("Light Source NDC: %3.2f, %3.2f", m_lensFlare.ndc.x, m_lensFlare.ndc.y);
                
                auto v = m_lensFlare.visible ? "True" : "False";
                ImGui::Text("Light Source Visible: %s", v);

                v = m_lensFlare.occluded ? "True" : "False";
                ImGui::Text("Light Source Occluded: %s", v);

                ImGui::Text("Point Count %lu", m_lensFlare.points.size());

                const float l = glm::length2(glm::vec2(m_lensFlare.ndc));
                ImGui::Text("Lens Length: %3.2f", l);
            }
            ImGui::End();

            if (ImGui::Begin("Window of happiness"))
            {
                ImGui::Image(m_paletteTexture, { 256.f, 32.f }, { 0.f, 1.f }, { 1.f, 0.f });

                if (ImGui::Button("Swap Palette"))
                {
                    static std::size_t pathIndex = 0;
                    pathIndex = (pathIndex + 1) % 2;

                    std::array<std::string, 2u> paths =
                    {
                        "assets/retro/palette01.png",
                        "assets/retro/palette02.png"
                    };

                    m_paletteTexture.loadFromFile(paths[pathIndex]);
                }
            }
            ImGui::End();        
        });

    auto& t = m_resources.textures.get("assets/retro/preview.png");
    t.setRepeated(true);
    ptz.pan = t.getSize() / 2u;

    static glm::vec2 markerPos(0.f);
    static glm::vec3 worldPos(230.f, 40.f, -120.f);

    registerWindow([&]()
        {
            if (ImGui::Begin("Window of Joy"))
            {
                if (ImGui::SliderFloat2("Pan", &ptz.pan[0], -640.f, 640.f))
                {
                    ptz.updateShader(t.getSize());
                }

                if (ImGui::SliderFloat("Tilt", &ptz.tilt, -cro::Util::Const::PI, cro::Util::Const::PI))
                {
                    ptz.updateShader(t.getSize());
                }

                if (ImGui::SliderFloat("Zoom", &ptz.zoom, 0.5f, 2.f))
                {
                    ptz.updateShader(t.getSize());
                }

                ImGui::SliderFloat("World X", &worldPos.x, 0.f, 320.f);
                ImGui::SameLine();
                ImGui::SliderFloat("World Z", &worldPos.z, 0.f, -200.f);

                ImGui::Text("Marker %3.3f, %3.3f", markerPos.x, markerPos.y);

                static bool enabled = true;
                if (ImGui::Button("Random"))
                {
                    if (enabled)
                    {
                        enabled = false;

                        struct CallbackData final
                        {
                            glm::vec2 target = glm::vec2(0.f);
                            float rotation = 0.f;
                            float zoom = 1.f;

                            float currentTime = 0.f;

                            PTZ start;
                        }data;

                        data.start = ptz;
                        data.target.x = static_cast<float>(cro::Util::Random::value(0, 320));
                        data.target.y = static_cast<float>(cro::Util::Random::value(0, 240));
                        auto rotation = cro::Util::Random::value(-cro::Util::Const::PI, cro::Util::Const::PI);
                        data.rotation = cro::Util::Maths::shortestRotation(ptz.tilt, rotation);
                        data.zoom = cro::Util::Random::value(0.5f, 2.f);

                        auto entity = m_uiScene.createEntity();
                        entity.addComponent<cro::Callback>().active = true;
                        entity.getComponent<cro::Callback>().setUserData<CallbackData>(data);
                        entity.getComponent<cro::Callback>().function =
                            [&](cro::Entity e, float dt)
                        {
                            auto& d = e.getComponent<cro::Callback>().getUserData<CallbackData>();
                            d.currentTime = std::min(1.f, d.currentTime + (dt * 2.f));

                            ptz.pan = glm::mix(d.start.pan, d.target, d.currentTime);
                            ptz.tilt = d.start.tilt + (d.rotation * d.currentTime);
                            ptz.zoom = glm::mix(d.start.zoom, d.zoom, d.currentTime);
                            ptz.updateShader(t.getSize());

                            if (d.currentTime == 1)
                            {
                                e.getComponent<cro::Callback>().active = false;
                                m_uiScene.destroyEntity(e);
                                enabled = true;
                            }
                        };
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset"))
                {
                    ptz.pan = { 320.f, 200.f };
                    ptz.tilt = 0.f;
                    ptz.zoom = 1.f;
                    ptz.updateShader(t.getSize());
                }
            }
            ImGui::End();
        });


    auto& texture = m_resources.textures.get("assets/images/lens_flare.png");
    texture.setRepeated(false);
    m_lensFlare.flareEntity = m_uiScene.createEntity();
    m_lensFlare.flareEntity.addComponent<cro::Transform>();
    m_lensFlare.flareEntity.addComponent<cro::Drawable2D>().setBlendMode(cro::Material::BlendMode::Additive);
    m_lensFlare.flareEntity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_lensFlare.flareEntity.getComponent<cro::Drawable2D>().setTexture(&texture);
    m_lensFlare.flareEntity.getComponent<cro::Drawable2D>().setShader(&m_flareShader);
    



    //PTZ tex - texture is loaded above so ImGui can capture it
    auto entity = m_uiScene.createEntity();    
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.5f));
    entity.getComponent<cro::Transform>().setPosition({ 80.f, 40.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(RetroShader::PTZ));
    entity.addComponent<cro::Sprite>(t);
    auto spriteEnt = entity;
    ptz.updateShader(t.getSize());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ 0.f, 0.f, -0.1f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-5.f, 5.f), cro::Colour::Green),
            cro::Vertex2D(glm::vec2(-5.f), cro::Colour::Green),
            cro::Vertex2D(glm::vec2(5.f), cro::Colour::Green),
            cro::Vertex2D(glm::vec2(5.f, -5.f), cro::Colour::Green)
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, spriteEnt](cro::Entity e, float)
    {
        auto position = ptz.toMapCoords(worldPos / spriteEnt.getComponent<cro::Transform>().getScale().x, t.getSize());
        markerPos = position;
        e.getComponent<cro::Transform>().setPosition(position);
    };
    spriteEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void RetroState::updateView(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    auto windowSize = size;
    /*size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;*/

    //90 deg in x (glm expects fov in y)
    cam3D.setPerspective(50.6f * cro::Util::Const::degToRad, /*16.f / 9.f*/size.x/size.y, 0.1f, 140.f);
    /*cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;*/
    cam3D.viewport = { 0.f, 0.f, 1.f, 1.f };

    float quadScale = std::floor(windowSize.y / m_renderTexture.getSize().y);
    m_quad.setScale(glm::vec2(quadScale));
    m_quad.setPosition((windowSize - (glm::vec2(m_renderTexture.getSize()) * quadScale)) / 2.f);


    //update the UI camera to match the new screen size
    //TODO DON'T DO THIS. Each camera wants its own specific
    //resize callback else the default one will override it.
    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = {0.f, 0.f, 1.f, 1.f};

    cam2D.setOrthographic(0.f, windowSize.x, 0.f, windowSize.y, -10.f, 10.f);
}

void RetroState::LensFlare::update(const cro::Camera& cam)
{
    const auto ndcVisible = [](glm::vec2 p)
        {
            return p.x >= -1.f && p.x <= 1.f && p.y >= -1.f && p.y <= 1.f;
        };

    ndc = cam.getActivePass().viewProjectionMatrix * glm::vec4(lightSource.getComponent<cro::Transform>().getPosition(), 1.f);

    visible = (ndc.w > 0); //hm this only accounts for far clip....
    occluded = false;

    points.clear();
    verts.clear();

    if (visible)
    {
        //mini optimisation would be to div after
        //converting to screenPos - but we're using
        //this to print debug output with imgui.
        ndc.x /= ndc.w;
        ndc.y /= ndc.w;

        glm::vec2 screenPos(ndc);

        if (!ndcVisible(screenPos))
        {
            //TODO check depth map for occlusion
            occluded = true;
        }

        if (!occluded)
        {
            const glm::vec2 Stride = screenPos / flareCount;
            do
            {
                points.push_back(screenPos);
                screenPos -= Stride;
            } while (ndcVisible(screenPos) && points.size() < MaxPoints);


            //why don't we just do this in the above loop?
            constexpr glm::vec2 QuadSize(64.f, 64.f); //TODO scale to screen size
            glm::vec2 OutputSize(cro::App::getWindow().getSize() / 2u);

            //use length of screenPos to calc brightness / set vert colour
            const float Brightness = ((1.f - std::min(1.f, glm::length2(glm::vec2(ndc)))) * 0.4f) + 0.1f;

            cro::Colour c = cro::Colour(Brightness, 1.f, 1.f, 1.f);
            std::int32_t i = 0;
            for (const auto& point : points)
            {
                const auto basePos = (point * OutputSize) + OutputSize;
                const float uWidth = (1.f / MaxPoints);
                const float u = uWidth * i;

                //the shader uses the NDC position to calc abberation
                c.setGreen(std::clamp((point.x + 1.f) / 2.f, 0.f, 1.f));
                c.setBlue(std::clamp((point.y + 1.f) / 2.f, 0.f, 1.f));

                verts.emplace_back(glm::vec2(basePos.x - QuadSize.x, basePos.y + QuadSize.y), glm::vec2(u, 1.f), c);
                verts.emplace_back(basePos - QuadSize, glm::vec2(u, 0.f), c);
                verts.emplace_back(basePos + QuadSize, glm::vec2(u + uWidth, 1.f), c);

                verts.emplace_back(basePos + QuadSize, glm::vec2(u + uWidth, 1.f), c);
                verts.emplace_back(basePos - QuadSize, glm::vec2(u, 0.f), c);
                verts.emplace_back(glm::vec2(basePos.x + QuadSize.x, basePos.y - QuadSize.y), glm::vec2(u+ uWidth, 0.f), c);
                i++;
            }
        }
    }

    flareEntity.getComponent<cro::Drawable2D>().setVertexData(verts);
}