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

#include "SSAOState.hpp"
#include "../StateIDs.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
#include "BlurShaders.inl"

    struct MRTChannel final
    {
        static constexpr std::int32_t Colour = 0;
        static constexpr std::int32_t Normal = 1;
        static constexpr std::int32_t Position = 2;
    };

    struct ShaderID final
    {
        enum
        {
            MipmapRender,
            Blur,
            SSAO,
            SSAOBlur,
            SSAOBlend
        };
    };
    constexpr std::int32_t KernelSize = 64;

    const std::string SSAOFrag = R"(

    uniform sampler2D u_texture; //position
    uniform sampler2D u_normalMap;
    uniform sampler2D u_noiseMap;
    uniform vec3 u_sampleKernel[KernelSize];

    uniform mat4 u_sceneProjectionMatrix;

    VARYING_IN vec2 v_texCoord;
    
    OUTPUT

    const vec2 NoiseSize = vec2(4.0);
    const float Radius = 0.5;

    void main()
    {
        vec2 noiseScale = textureSize(u_texture, 0) / NoiseSize;
        vec3 position = TEXTURE(u_texture, v_texCoord).xyz;
        vec3 normal = normalize(TEXTURE(u_normalMap, v_texCoord).rgb);
        vec3 randomiser = TEXTURE(u_noiseMap, v_texCoord * noiseScale).xyz;
        randomiser.xy *= 2.0;
        randomiser -= 1.0;
        randomiser = normalize(randomiser);

        vec3 tan = normalize(randomiser - normal * dot(randomiser, normal));
        vec3 bitan = cross(normal, tan);
        mat3 tbn = mat3(tan, bitan, normal);

        float ao = 0.0;
        for (int i = 0; i < KernelSize; ++i)
        {
            vec3 samplePos = tbn * u_sampleKernel[i];
            samplePos = position + samplePos * Radius;

            vec4 offset = vec4(samplePos, 1.0);
            offset = u_sceneProjectionMatrix * offset;
            offset.xyz /= offset.w;
            offset.xyz *= 0.5;
            offset.xyz += 0.5;

            float depth = TEXTURE(u_texture, offset.xy).z;
            float rangeClamp = smoothstep(0.0, 1.0, Radius / abs(position.z - depth));

            ao += (depth > samplePos.z ? 1.0 : 0.0) * rangeClamp;
        }

        ao = 1.0 - (ao / KernelSize);

        FRAG_OUT = vec4(vec3(ao), 1.0);
    })";

    //note this specifically takes advantage
    //of the randomising noise texture in the SSAO shader
    const std::string SSAOBlur = R"(
    OUTPUT

    uniform sampler2D u_texture;

    VARYING_IN vec2 v_texCoord;

    void main()
    {
        vec2 texel = 1.0 / textureSize(u_texture, 0);

        float colour = 0.0;
        for (int y = -2; y < 2; ++y)
        {
            for (int x = -2; x < 2; ++x)
            {
                vec2 offset = vec2(x, y) * texel;
                colour += TEXTURE(u_texture, v_texCoord + offset).r;
            }
        }
        FRAG_OUT = vec4(vec3(colour / (4.0 * 4.0)), 1.0);
    })";

    //TODO this is a bit of a fudge, as really it should be
    //a contribution to the lighting calc, but this has already been done..
    const std::string SSAOBlend = R"(
    OUTPUT

    uniform sampler2D u_texture;
    uniform sampler2D u_aoMap;

    VARYING_IN vec2 v_texCoord;

    void main()
    {
        FRAG_OUT = vec4(TEXTURE(u_texture, v_texCoord).rgb * TEXTURE(u_aoMap, v_texCoord).r, 1.0);
    })";
}

SSAOState::SSAOState(cro::StateStack& stack, cro::State::Context context)
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
bool SSAOState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
            requestStackClear();
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void SSAOState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool SSAOState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void SSAOState::render()
{
    m_renderBuffer.clear();
    m_gameScene.render();
    m_renderBuffer.display();

    m_colourQuad.draw();
    m_normalQuad.draw();
    m_positionQuad.draw();
    m_depthQuad.draw();

    m_ssaoBuffer.clear(cro::Colour::CornflowerBlue);
    updateSSAOData();
    m_ssaoBuffer.display();

    m_ssaoQuad.draw();
    m_ssaoBlurQuad.draw();


    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, m_ssaoBuffer.getTexture().getGLHandle());
    glUseProgram(m_outputData.shader);
    glUniform1i(m_outputData.aoMap, 10);
    m_outputQuad.draw();

    m_uiScene.render();
}

//private
void SSAOState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void SSAOState::loadAssets()
{
    m_environmentMap.loadFromFile("assets/images/hills.hdr");

    //m_resources.shaders.loadFromString(ShaderID::Blur, cro::SimpleDrawable::getDefaultVertexShader(), BlurFragment);

    m_resources.shaders.loadFromString(ShaderID::SSAO, 
        cro::SimpleDrawable::getDefaultVertexShader(), SSAOFrag, "#define KernelSize " + std::to_string(KernelSize) + "\n");
    auto* shader = &m_resources.shaders.get(ShaderID::SSAO);
    m_ssaoData.shader = shader->getGLHandle();
    m_ssaoData.kernel = shader->getUniformID("u_sampleKernel[0]");
    m_ssaoData.noise = shader->getUniformID("u_noiseMap");
    m_ssaoData.normal = shader->getUniformID("u_normalMap");
    m_ssaoData.projectionMatrix = shader->getUniformID("u_sceneProjectionMatrix");

    m_resources.shaders.loadFromString(ShaderID::SSAOBlur, cro::SimpleDrawable::getDefaultVertexShader(), SSAOBlur);
    
    m_resources.shaders.loadFromString(ShaderID::SSAOBlend, cro::SimpleDrawable::getDefaultVertexShader(), SSAOBlend);
    shader = &m_resources.shaders.get(ShaderID::SSAOBlend);
    m_outputData.shader = shader->getGLHandle();
    m_outputData.aoMap = shader->getUniformID("u_aoMap");
}

void SSAOState::createScene()
{
    cro::ModelDefinition md(m_resources, &m_environmentMap);
    if (md.loadFromFile("assets/ssao/cart/cart.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt * 0.5f);
        };
    }

    if (md.loadFromFile("assets/ssao/background/background.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
    }

    auto resizeWindow = [&](cro::Camera& cam)
    {
        auto buffSize = cro::App::getWindow().getSize();
        auto size = glm::vec2(buffSize);

        m_renderBuffer.create(buffSize.x, buffSize.y, 3);
        
        m_colourQuad.setTexture(m_renderBuffer.getTexture(MRTChannel::Colour), buffSize);
        m_colourQuad.setPosition({ 0.f, (size.y / 4.f) * 3.f });
        m_colourQuad.setScale({ 0.25f, 0.25f });

        m_normalQuad.setTexture(m_renderBuffer.getTexture(MRTChannel::Normal), buffSize);
        m_normalQuad.setPosition({ 0.f, size.y / 2.f });
        m_normalQuad.setScale({ 0.25f, 0.25f });

        m_positionQuad.setTexture(m_renderBuffer.getTexture(MRTChannel::Position), buffSize);
        m_positionQuad.setPosition({ 0.f, size.y / 4.f });
        m_positionQuad.setScale({ 0.25f, 0.25f });

        m_depthQuad.setTexture(m_renderBuffer.getDepthTexture(), buffSize);
        m_depthQuad.setScale({ 0.25f, 0.25f });

        buffSize /= 2u;

        m_ssaoBuffer.create(buffSize.x, buffSize.y, false);
        m_ssaoBuffer.setSmooth(true);
        m_ssaoQuad.setTexture(m_ssaoBuffer.getTexture());
        m_ssaoQuad.setPosition({ size.x / 2.f, 0.f });
        m_ssaoQuad.setScale({ 0.5f, 0.5f });

        m_ssaoBlurBuffer.create(buffSize.x, buffSize.y, false);
        m_ssaoBlurBuffer.setSmooth(true);
        m_ssaoBlurQuad.setTexture(m_ssaoBlurBuffer.getTexture());
        m_ssaoBlurQuad.setShader(m_resources.shaders.get(ShaderID::SSAOBlur));
        m_ssaoBlurQuad.setPosition({ size.x / 4.f, 0.f });

        //used for rendering the SSAO data to the SSAO buffer
        m_ssaoBufferQuad.setTexture(m_renderBuffer.getTexture(MRTChannel::Position), buffSize);
        m_ssaoBufferQuad.setShader(m_resources.shaders.get(ShaderID::SSAO));


        m_outputQuad.setTexture(m_renderBuffer.getTexture(MRTChannel::Colour), buffSize * 2u);
        m_outputQuad.setShader(m_resources.shaders.get(ShaderID::SSAOBlend));
        m_outputQuad.setPosition({ size.x / 4.f, size.y / 4.f });
        m_outputQuad.setScale({ 0.75f, 0.75f });

        cam.setPerspective(0.7f, size.x / size.y, 2.1f, 7.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Camera>().resizeCallback = resizeWindow;
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(2048, 2048);
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 2.f, 3.878f });
    camEnt.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -0.254f);
    resizeWindow(camEnt.getComponent<cro::Camera>());

    auto sun = m_gameScene.getSunlight();
    sun.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.3f);
    sun.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.4f);

    generateSSAOData();
}

void SSAOState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Cart of buns"))
            {
                /*static glm::vec3 pos(0.f, 0.5f, 3.f);
                if (ImGui::SliderFloat3("Position", &pos[0], -10.f, 10.f))
                {
                    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition(pos);
                }

                static float rotation = 0.f;
                if (ImGui::SliderFloat("Rotation", &rotation, -2.f, 2.f))
                {
                    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, rotation);
                }*/
                ImGui::Image(m_noiseTexture, { 32.f, 32.f }, { 0.f, 1.f }, { 1.f, 0.f });
            }
            ImGui::End();        
        });
}

void SSAOState::generateSSAOData()
{
    const auto lerp = [](float a, float b, float t)
    {
        return a + t * (b - a);
    };

    for (auto i = 0; i < KernelSize; ++i)
    {
        auto& sample = m_sampleKernel.emplace_back(
            cro::Util::Random::value(-1.f, 1.f),
            cro::Util::Random::value(-1.f, 1.f),
            cro::Util::Random::value(0.f, 1.f)
        );

        sample = glm::normalize(sample);
        sample *= cro::Util::Random::value(0.f, 1.f);

        float scale = static_cast<float>(i) / KernelSize;
        scale = lerp(0.1f, 1.f, scale * scale);
        sample *= scale;
    }

    static constexpr std::int32_t NoiseSize = 16;
    std::vector<std::uint16_t> noiseData;
    for (auto i = 0; i < NoiseSize; ++i)
    {
        noiseData.emplace_back(cro::Util::Random::value(std::numeric_limits<std::uint16_t>::min(), std::numeric_limits<std::uint16_t>::max()));
        noiseData.emplace_back(cro::Util::Random::value(std::numeric_limits<std::uint16_t>::min(), std::numeric_limits<std::uint16_t>::max()));
        noiseData.emplace_back(0);
    }

    m_noiseTexture.create(4, 4, cro::ImageFormat::RGB);
    m_noiseTexture.setRepeated(true);
    m_noiseTexture.update(noiseData.data());
}

void SSAOState::updateSSAOData()
{
    //position texture is automatically set to m_texture via SimpleQuad

    glUseProgram(m_ssaoData.shader);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, m_renderBuffer.getTexture(1).textureID);
    glUniform1i(m_ssaoData.normal, 10);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, m_noiseTexture.getGLHandle());
    glUniform1i(m_ssaoData.noise, 11);
    glUniform3fv(m_ssaoData.kernel, KernelSize, &m_sampleKernel[0][0]);

    const auto& projMat = m_gameScene.getActiveCamera().getComponent<cro::Camera>().getProjectionMatrix();
    //const auto& projMat = m_gameScene.getActiveCamera().getComponent<cro::Camera>().getActivePass().viewProjectionMatrix;
    glUniformMatrix4fv(m_ssaoData.projectionMatrix, 1, GL_FALSE, &projMat[0][0]);

    
    m_ssaoBlurBuffer.clear();
    m_ssaoBufferQuad.draw();
    m_ssaoBlurBuffer.display();


    auto pos = m_ssaoBlurQuad.getPosition();
    m_ssaoBlurQuad.setPosition({ 0.f, 0.f });
    m_ssaoBlurQuad.setScale({ 1.f, 1.f });

    m_ssaoBlurQuad.draw();

    m_ssaoBlurQuad.setPosition(pos);
    m_ssaoBlurQuad.setScale({ 0.5f, 0.5f });

}