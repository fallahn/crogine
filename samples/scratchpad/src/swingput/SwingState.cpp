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

#include "SwingState.hpp"

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
#include <crogine/util/Wavetable.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
#include "Rain.inl"
#include "Glow.inl"

    float cohesion = 10.f;
    float maxVelocity = 200.f;
    float targetRadius = 50.f;

    const std::string WaterVert =
        R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec3 a_tangent;
    ATTRIBUTE vec3 a_bitangent;
    ATTRIBUTE vec2 a_texCoord0;

    #include WVP_UNIFORMS

    uniform vec4 u_subrect;
    uniform vec4 u_clipPlane;

    VARYING_OUT vec3 v_worldPosition;

    VARYING_OUT vec3 v_tbn[3];
    VARYING_OUT vec2 v_texCoord0;

    void main()
    {
        mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
        vec4 position = a_position;
        vec4 worldPosition = u_worldMatrix * position;

        v_worldPosition = worldPosition.xyz;
        gl_Position = wvp * position;

#if defined(SPHERE)
        v_tbn[0] = normalize(a_position.xyz); //assumes the position is relative to centre and used as cubmap lookup
#else
        vec3 normal = a_normal;
        vec4 tangent = vec4(a_tangent, 0.0);
        vec4 bitangent = vec4(a_bitangent, 0.0);

        v_tbn[0] = normalize(u_normalMatrix * tangent.xyz);
        v_tbn[1] = normalize(u_normalMatrix * bitangent.xyz);
        v_tbn[2] = normalize(u_normalMatrix * normal);

        v_texCoord0 = u_subrect.xy + (a_texCoord0 * u_subrect.zw);

        gl_ClipDistance[0] = dot(worldPosition, u_clipPlane);
#endif
    })";


    const std::string WaterFrag = R"(
    OUTPUT

    uniform sampler2D u_diffuseMap;
    uniform samplerCube u_cubeMap;
    uniform vec3 u_cameraWorldPosition;
    uniform float u_time;

    VARYING_IN vec2 v_texCoord0;
    VARYING_IN vec3 v_worldPosition;
    VARYING_IN vec3 v_tbn[3]; //v_tbn[2] contains normal, remember

    const float RidgeCount = 300.0;
    const float MaxAngle = 0.05;

    void main()
    {
        vec4 colour = TEXTURE(u_diffuseMap, v_texCoord0);

        vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);
        vec3 normal = v_tbn[2];

        float fresnel = dot(reflect(-eyeDirection, normal), normal);
        const float bias = 0.8;
        fresnel = 1.0 - ((fresnel * (1.0 - bias)) + bias);


        //for ease this is calculated in tangent space, hence the need for bitan matrix
        float r = sin(v_texCoord0.x * RidgeCount);
        float cycle = sin((u_time * 5.0) + v_texCoord0.x);
        r *= cycle;

        float s = sin(v_texCoord0.y * 10.0) * cos(u_time * 10.0);
        r *= s;
        r *= MaxAngle;

        vec3 noiseNormal = normalize(vec3(sin(r) * 0.5 + 0.5, 0.5, cos(r)));

        noiseNormal = noiseNormal * 2.0 - 1.0;
        noiseNormal = normalize(v_tbn[0] * noiseNormal.r + v_tbn[1] * noiseNormal.g + v_tbn[2] * noiseNormal.b);



        vec3 R = reflect(-eyeDirection, noiseNormal);
        vec3 reflectionColour = TEXTURE_CUBE(u_cubeMap, R).rgb;

        colour.rgb = mix(colour.rgb, reflectionColour, fresnel);
        //colour.rgb += reflectionColour * fresnel;

//colour.rgb = vec3(r + 1.0 * 0.5);
//colour.rgb = noiseNormal;
//colour.rgb = vec3(fresnel);
//colour.rgb = reflectionColour;
        FRAG_OUT = colour;
    })";


    const std::string SphereFrag = 
    R"(
    OUTPUT

    uniform samplerCubeArray u_cubeMap;
    //uniform samplerCube u_cubeMap;

    VARYING_IN vec3 v_tbn[3];

    void main()
    {
        FRAG_OUT = texture(u_cubeMap, vec4(normalize(v_tbn[0]), 0.0));
        //FRAG_OUT = texture(u_cubeMap, normalize(v_tbn[0]));
    })";


    struct ShaderID final
    {
        enum
        {
            Water = 1,
            Sphere,

            Rain,
            Glow
        };
    };

    struct MaterialID final
    {
        enum
        {
            Water,
            Sphere,

            Glow
        };
    };

    std::int32_t timeUniform = -1;
    std::uint32_t shaderID = 0;
}

SwingState::SwingState(cro::StateStack& stack, cro::State::Context context)
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

    loadSettings();
}

SwingState::~SwingState()
{
    saveSettings();
}

//public
bool SwingState::handleEvent(const cro::Event& evt)
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
    else if (evt.type == SDL_MOUSEMOTION)
    {
        if (evt.motion.state & SDL_BUTTON_LMASK)
        {
            auto pos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(glm::vec2(evt.motion.x, evt.motion.y));
            m_target.setPosition(pos);
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            auto pos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(glm::vec2(evt.button.x, evt.button.y));
            m_target.setPosition(pos);
        }
    }

    //m_inputParser.handleEvent(evt);

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void SwingState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool SwingState::simulate(float dt)
{
    static float accumulator = 0.f;
    accumulator += dt;

    glUseProgram(shaderID);
    glUniform1f(timeUniform, accumulator);


    static constexpr float FrameTime = 1.f / 10.f;
    static float animTime = 0.f;
    animTime += dt;

    static constexpr std::int32_t MaxFrames = 24;
    static std::int32_t frameID = 0;

    while (animTime > FrameTime)
    {
        frameID = (frameID + 1) % MaxFrames;

        auto row = 5 - (frameID / 4);
        auto col = frameID % 4;

        glm::vec4 rect =
        {
            (1.f / 4.f) * col,
            (1.f / 6.f) * row,
            (1.f / 4.f), (1.f / 6.f)
        };

        glUseProgram(m_rainShader.shaderID);
        glUniform4f(m_rainShader.rectUniform, rect.x, rect.y, rect.z, rect.w);

        animTime -= FrameTime;
    }


    m_inputParser.process(dt);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void SwingState::render()
{
    m_gameScene.render();
    m_uiScene.render();

    auto oldCam = m_gameScene.setActiveCamera(m_ballCam);
    m_ballTexture.clear(cro::Colour(0.05f, 0.05f, 0.05f, 1.f));
    m_gameScene.render();
    m_ballTexture.display();
    m_gameScene.setActiveCamera(oldCam);

    glUseProgram(m_rainShader.shaderID);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, m_rainShader.textureID);
    glUniform1i(m_rainShader.rainUniform, 11);


    m_rainQuad.draw();
    m_ballQuad.draw();

    m_target.draw();
    m_follower.draw();
}

//private
void SwingState::addSystems()
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

void SwingState::loadAssets()
{
    m_cubemap.loadFromFile("assets/images/1/cmap.ccm");
    m_resources.shaders.loadFromString(ShaderID::Water, WaterVert, WaterFrag);
    auto& shader = m_resources.shaders.get(ShaderID::Water);
    shaderID = shader.getGLHandle();
    timeUniform = shader.getUniformID("u_time");
    m_resources.materials.add(MaterialID::Water, shader);


    m_resources.shaders.loadFromString(ShaderID::Sphere, WaterVert, SphereFrag, "#define SPHERE\n");
    m_resources.materials.add(MaterialID::Sphere, m_resources.shaders.get(ShaderID::Sphere));


    std::vector<std::string> cubemapPaths =
    {
        "assets/images/0/cmap.ccm",
        "assets/images/1/cmap.ccm",
        "assets/images/2/cmap.ccm"
    };
    m_cubemapArray.loadFromFiles(cubemapPaths);

    m_target.setVertexData(
        {
            cro::Vertex2D(glm::vec2(-10.f, 10.f), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(-10.f, -10.f), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(10.f, 10.f), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(10.f, -10.f), cro::Colour::Blue)
        }
    );

    m_follower.setVertexData(
        {
            cro::Vertex2D(glm::vec2(-10.f, 10.f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(-10.f, -10.f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f, 10.f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f, -10.f), cro::Colour::Red)
        }
    );

    auto& tex = m_resources.textures.get("assets/images/face.png");
    tex.setSmooth(true);
    m_rainQuad.setTexture(tex);
    m_rainQuad.setScale(glm::vec2(4.f));

    m_resources.shaders.loadFromString(ShaderID::Rain, RainVert, RainFrag);
    auto& rainShader = m_resources.shaders.get(ShaderID::Rain);
    m_rainQuad.setShader(rainShader);

    auto& rainTex = m_resources.textures.get("assets/golf/images/rain_sheet.png");
    rainTex.setSmooth(true);

    m_rainShader.shaderID = rainShader.getGLHandle();
    m_rainShader.rainUniform = rainShader.getUniformID("u_rainMap");
    m_rainShader.rectUniform = rainShader.getUniformID("u_subrect");
    m_rainShader.textureID = rainTex.getGLHandle();


    m_ballTexture.create(512, 512);
    m_ballQuad.setTexture(m_ballTexture.getTexture());

    m_resources.shaders.loadFromString(ShaderID::Glow, GlowVertex, GlowFragment);
    m_resources.materials.add(MaterialID::Glow, m_resources.shaders.get(ShaderID::Glow));
}

void SwingState::createScene()
{
    //this is called when the window is resized to automatically update the camera's matrices/viewport
    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&SwingState::updateView, this, std::placeholders::_1);

    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 1.8f, 2.5f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.5f);

    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/models/plane.cmt");

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        static const std::vector<float> WaveTable = cro::Util::Wavetable::sine(0.1f);
        static std::size_t idx = 0;

        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, WaveTable[idx] * 0.5f);

        idx = (idx + 1) % WaveTable.size();
    };

    auto material = m_resources.materials.get(MaterialID::Water);
    material.doubleSided = true;
    auto* m = md.getMaterial(0);
    if (m->properties.count("u_diffuseMap"))
    {
        material.setProperty("u_diffuseMap", cro::TextureID(m->properties.at("u_diffuseMap").second.textureID));
    }
    if (m->properties.count("u_subrect"))
    {
        const float* v = m->properties.at("u_subrect").second.vecValue;
        glm::vec4 subrect(v[0], v[1], v[2], v[3]);
        material.setProperty("u_subrect", subrect);
    }
    material.setProperty("u_cubeMap", cro::CubemapID(m_cubemap));
    material.animation = m->animation;
    entity.getComponent<cro::Model>().setMaterial(0, material);


    //sphere preview
    md.loadFromFile("assets/models/sphere_1m.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -1.f, 0.5f, 0.f });
    md.createModel(entity);

    material = m_resources.materials.get(MaterialID::Sphere);
    material.setProperty("u_cubeMap", cro::CubemapID(m_cubemapArray));
    entity.getComponent<cro::Model>().setMaterial(0, material);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = [](cro::Entity e, float dt) {e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt); };


    //entity uses callback system to process logic of follower
    struct FollowerData final
    {
        glm::vec2 velocity = glm::vec2(0.f);
    };

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<FollowerData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt) mutable
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<FollowerData>();

        auto diff = (m_target.getPosition() - m_follower.getPosition());

        //coherance
        data.velocity += diff * cohesion;

        //velocity matching is redundant in cases where target is stopped - because we'll just stop following


        //clamp velocity
        if (auto len2 = glm::length2(data.velocity); len2 > (maxVelocity * maxVelocity))
        {
            data.velocity = (data.velocity / glm::sqrt(len2)) * maxVelocity;
        }

        //reduce speed as we get within radius of target
        float multiplier = std::clamp(glm::length2(diff) / (targetRadius * targetRadius), 0.f, 1.f);

        //make sure not to mutate the velocity by the multiplier
        m_follower.move(data.velocity * multiplier * dt);
    };


    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.32f);
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.2f);



    //balls.
    const std::array BallPaths =
    {
        std::string("assets/golf/models/balls/ball_pot.cmt"),
        std::string("assets/golf/models/balls/ball_nine.cmt"),
        std::string("assets/golf/models/balls/ball_pineapple.cmt"),
        std::string("assets/golf/models/balls/ball02.cmt"),
        std::string("assets/golf/models/balls/ball03.cmt"),
    };

    auto nub = m_gameScene.createEntity();
    nub.addComponent<cro::Transform>().setPosition({ 1.5f, 0.f, 0.f });
    nub.addComponent<cro::Callback>().active = true;
    nub.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.25f * dt);
        };

    static constexpr float Radius = 0.1f;
    static constexpr float Arc = cro::Util::Const::TAU / BallPaths.size();
    for (auto i = 0u; i < BallPaths.size(); ++i)
    {
        if (md.loadFromFile(BallPaths[i]))
        {
            auto pos = glm::vec3(glm::cos(i * Arc), 0.f, glm::sin(i * Arc));

            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(pos * Radius);
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
                {
                    e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.1f * dt);
                };

            md.createModel(entity);
            entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(MaterialID::Glow));
            nub.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }

    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(nub.getComponent<cro::Transform>().getPosition() + glm::vec3(0.f, 0.02f, 0.f));
    auto& ballCam = camEnt.addComponent<cro::Camera>();
    ballCam.viewport = { 0.f, 0.f, 1.f, 1.f };
    ballCam.setPerspective(75.f * cro::Util::Const::degToRad, 1.f, 0.01f, 0.2f);
    m_ballCam = camEnt;
    //nub.getComponent<cro::Transform>().addChild(camEnt.getComponent<cro::Transform>());
}

void SwingState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Follower"))
            {
                auto pos = m_target.getPosition();
                ImGui::Text("Target: %3.1f, %3.1f", pos.x, pos.y);

                pos = m_follower.getPosition();
                ImGui::Text("Follower: %3.1f, %3.1f", pos.x, pos.y);

                ImGui::Separator();
                ImGui::SliderFloat("Cohesion", &cohesion, 10.f, 40.f);
                ImGui::SliderFloat("Max Velocity", &maxVelocity, 200.f, 500.f);
                ImGui::SliderFloat("Target Radius", &targetRadius, 50.f, 100.f);
            }
            ImGui::End();   
        });


    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 320.f, 240.f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
    entity.getComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-10.f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(50.f), cro::Colour::Blue) ,
            cro::Vertex2D(glm::vec2(50.f), cro::Colour::Green) ,
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        auto start = m_inputParser.getBackPoint();
        auto active = m_inputParser.getActivePoint();
        auto end = m_inputParser.getFrontPoint();

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        verts[0].position = start;
        verts[1].position = active;
        verts[2].position = end;
    };


    constexpr float Size = 100.f;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 600.f, 240.f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-10.f, -Size), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f,  -Size), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(-10.f,  -0.5f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f,  -0.5f), cro::Colour::Red),

            cro::Vertex2D(glm::vec2(-10.f,  -0.5f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(10.f,  -0.5f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(-10.f,  0.5f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(10.f,  0.5f), cro::Colour::White),

            cro::Vertex2D(glm::vec2(-10.f,  0.5f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f,  0.5f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(-10.f, Size), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(10.f,  Size), cro::Colour::Red),


            cro::Vertex2D(glm::vec2(-10.f, -Size), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(10.f,  -Size), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(-10.f, 0.f), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(10.f,  0.f), cro::Colour::Blue),

        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        float height = verts[14].position.y;
        float targetAlpha = 0.f;

        if (m_inputParser.active())
        {
            height = m_inputParser.getActivePoint().y;
            targetAlpha = 1.f;
        }

        auto& currentAlpha = e.getComponent<cro::Callback>().getUserData<float>();
        const float InSpeed = dt * 6.f;
        const float OutSpeed = m_inputParser.getPower() == 0 ? InSpeed : dt * 0.5f;
        if (currentAlpha < targetAlpha)
        {
            currentAlpha = std::min(1.f, currentAlpha + InSpeed);
        }
        else
        {
            currentAlpha = std::max(0.f, currentAlpha - OutSpeed);
        }

        for (auto& v : verts)
        {
            v.colour.setAlpha(currentAlpha);
        }
        verts[14].position.y = height;
        verts[15].position.y = height;
    };

    auto updateUI = [](cro::Camera& cam)
    {
        //glm::vec2 size(cro::App::getWindow().getSize());
        //cam.setOrthographic(0.f, size.x, 0.f, size.y, -1.f, 2.f);
        cam.setOrthographic(0.f, 720.f, 0.f, 480.f, -1.f, 2.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    m_uiScene.getActiveCamera().getComponent<cro::Camera>().resizeCallback = updateUI;
    updateUI(m_uiScene.getActiveCamera().getComponent<cro::Camera>());
}

void SwingState::loadSettings()
{

}

void SwingState::saveSettings()
{

}

void SwingState::updateView(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
   
    const auto windowSize = size;
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    //90 deg in x (glm expects fov in y)
    cam3D.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 140.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    m_rainQuad.setPosition(windowSize - glm::vec2(576.f, 280.f));
    m_ballQuad.setPosition(windowSize - glm::vec2(576.f, 824.f));
}