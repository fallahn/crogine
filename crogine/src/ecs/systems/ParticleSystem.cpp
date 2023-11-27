/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Matrix.hpp>

#include "../../detail/GLCheck.hpp"

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

#ifdef CRO_DEBUG_
#include <crogine/gui/Gui.hpp>
#endif

#ifdef PLATFORM_DESKTOP
#define ENABLE_POINT_SPRITES glCheck(glEnable(GL_PROGRAM_POINT_SIZE));
#define DISABLE_POINT_SPRITES glCheck(glDisable(GL_PROGRAM_POINT_SIZE));
#else
#define ENABLE_POINT_SPRITES
#define DISABLE_POINT_SPRITES
#endif // PLATFORM_DESKTOP

using namespace cro;

namespace
{
    const std::string vertex = R"(
        ATTRIBUTE vec4 a_position;
        ATTRIBUTE LOW vec4 a_colour;
        ATTRIBUTE MED vec3 a_normal; //this actually stores rotation, scale and current frame if animated

        uniform mat4 u_projection;
        uniform mat4 u_viewProjection;
        uniform LOW float u_viewportHeight;
        uniform LOW float u_particleSize;

#if defined(MOBILE)

#else
        uniform vec4 u_clipPlane;
#endif

        VARYING_OUT LOW vec4 v_colour;
        VARYING_OUT MED mat2 v_rotation;
        VARYING_OUT LOW float v_currentFrame;
        VARYING_OUT HIGH float v_depth;

        void main()
        {
            v_colour = a_colour;

            vec2 rot = vec2(sin(a_normal.x), cos(a_normal.x));
            v_rotation[0] = vec2(rot.y, -rot.x);
            v_rotation[1]= rot;

            v_currentFrame = a_normal.z;

            gl_Position = u_viewProjection * a_position;
            gl_PointSize = u_viewportHeight * u_projection[1][1] / gl_Position.w * u_particleSize * a_normal.y;

            v_depth = gl_Position.z / gl_Position.w;

#if defined (MOBILE)

#else
            gl_ClipDistance[0] = dot(a_position, u_clipPlane);
#endif

        }
    )";

    const std::string fragment = R"(
        uniform sampler2D u_texture;
        uniform float u_frameCount;
        uniform vec2 u_textureSize;
        uniform vec2 u_cameraRange;

#if defined (SUNLIGHT)
        uniform vec4 u_lightColour;
#endif


        VARYING_IN LOW vec4 v_colour;
        VARYING_IN MED mat2 v_rotation;
        VARYING_IN LOW float v_currentFrame;
        VARYING_IN HIGH float v_depth;
        OUTPUT


#if defined (MOBILE)
        float smoothstep(float edge0, float edge1, float x)
        {
            float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        }
#else
//horrible hack to stop particles outputting
//to light buffer in golf

layout (location = 3) out vec4 LIGHT_OUT;

#endif

        float linearise(float z)
        {
            return (2.0 * u_cameraRange.x) / (u_cameraRange.y + u_cameraRange.x - z*(u_cameraRange.y - u_cameraRange.x));
        }

        void main()
        {
            float frameWidth = 1.0 / u_frameCount;
            vec2 coord = vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y);
            coord.x *= frameWidth;
            coord.x += v_currentFrame * frameWidth;
            vec2 centreOffset = vec2((v_currentFrame * frameWidth) + (frameWidth / 2.f), 0.5);

            //convert to texture space
            coord *= u_textureSize;
            centreOffset *= u_textureSize;

            //rotate
            coord = v_rotation * (coord - centreOffset);
            coord += centreOffset;

            //and back to UV space
            coord /= u_textureSize;

            //frag depth should have geometry depth stored as depth writes are disabled
            //TODO currently disabled as this looks really broken with ortho projections
            float geomDepth = linearise(gl_FragCoord.z);
            float diff = clamp(geomDepth - linearise(v_depth), 0.0, 1.0);
            float strength = smoothstep(0.0, 0.05, diff);

            FRAG_OUT = v_colour * TEXTURE(u_texture, coord);// * strength;

        #if defined (SUNLIGHT)
            FRAG_OUT *= u_lightColour;
        #endif

        #if defined (BLEND_ADD)
            FRAG_OUT.rgb *= v_colour.a;
        #endif

        #if defined (BLEND_MULTIPLY)
            FRAG_OUT.rgb += (vec3(1.0) - FRAG_OUT.rgb) * (1.0 - FRAG_OUT.a);
        #endif

#if !defined(MOBILE)
LIGHT_OUT = vec4(vec3(0.0), 1.0);
#endif

        }
    )";

    constexpr std::size_t MaxVertData = ParticleEmitter::MaxParticles * (3 + 4 + 3); //pos, colour, rotation/scale vert attribs
    const std::size_t MaxParticleSystems = 128; //max number of VBOs - must be divisible by min count
    const std::size_t MinParticleSystems = 4; //min amount before resizing - this many added on resize (so don't make too large!!)
    const std::size_t VertexSize = 10 * sizeof(float); //pos, colour, rotation/scale vert attribs


    bool inFrustum(const Frustum& frustum, const ParticleEmitter& emitter)
    {
        bool visible = true;
        std::size_t i = 0;
        while (visible && i < frustum.size())
        {
            visible = (Spatial::intersects(frustum[i++], emitter.getBounds()) != Planar::Back);
        }

        return visible;
    }
}

ParticleSystem::ParticleSystem(MessageBus& mb)
    : System            (mb, typeid(ParticleSystem)),
    m_drawLists         (1),
    m_dataBuffer        (MaxVertData),
    m_vboIDs            (MaxParticleSystems),
    m_vaoIDs            (MaxParticleSystems),
    m_nextBuffer        (0),
    m_bufferCount       (0)
{
    for (auto& vbo : m_vboIDs)
    {
        vbo = 0;
    }

    //required for core profile on desktop
    for (auto& vao : m_vaoIDs)
    {
        vao = 0;
    }

    requireComponent<Transform>();
    requireComponent<ParticleEmitter>();

    const std::array<std::string, ShaderID::Count> Defines =
    {
        "#define SUNLIGHT\n", "#define BLEND_ADD\n", "#define BLEND_MULTIPLY\n"
    };

    for (auto i = 0; i < ShaderID::Count; ++i)
    {
        auto& shader = m_shaders.emplace_back(std::make_unique<Shader>());

        auto& handle = m_shaderHandles[i];

        //ATTRIB MAPPING RELIES ON ALL VARIANTS USING THE SAME VERTEX SHADER
        //so please avoid changing this if you can...
        std::fill(handle.uniformIDs.begin(), handle.uniformIDs.end(), -1);
        if (!shader->loadFromString(vertex, fragment, Defines[i]))
        {
            Logger::log("Failed to compile Particle shader", Logger::Type::Error);
        }
        else
        {
            handle.id = shader->getGLHandle();

            //fetch uniforms.
            const auto& uniforms = shader->getUniformMap();
#ifdef PLATFORM_DESKTOP
            handle.uniformIDs[UniformID::ClipPlane] = uniforms.find("u_clipPlane")->second;
#endif
            handle.uniformIDs[UniformID::Projection] = uniforms.find("u_projection")->second;
            if (i == ShaderID::Alpha)
            {
                handle.uniformIDs[UniformID::LightColour] = uniforms.find("u_lightColour")->second;
            }
            handle.uniformIDs[UniformID::Texture] = uniforms.find("u_texture")->second;
            handle.uniformIDs[UniformID::ViewProjection] = uniforms.find("u_viewProjection")->second;
            handle.uniformIDs[UniformID::Viewport] = uniforms.find("u_viewportHeight")->second;
            handle.uniformIDs[UniformID::ParticleSize] = uniforms.find("u_particleSize")->second;
            handle.uniformIDs[UniformID::TextureSize] = uniforms.find("u_textureSize")->second;
            handle.uniformIDs[UniformID::FrameCount] = uniforms.find("u_frameCount")->second;
            if (uniforms.count("u_cameraRange"))
            {
                handle.uniformIDs[UniformID::CameraRange] = uniforms.find("u_cameraRange")->second;
            }

            //map attributes
            const auto& attribMap = shader->getAttribMap();
            handle.attribData[0].index = attribMap[Mesh::Position];
            handle.attribData[0].attribSize = 3;
            handle.attribData[0].offset = 0;

            handle.attribData[1].index = attribMap[Mesh::Colour];
            handle.attribData[1].attribSize = 4;
            handle.attribData[1].offset = 3 * sizeof(float);

            handle.attribData[2].index = attribMap[Mesh::Normal]; //actually rotation/scale just using the existing naming convention
            handle.attribData[2].attribSize = 3;
            handle.attribData[2].offset = (3 + 4) * sizeof(float);
        }
    }
    cro::Image img;
    img.create(2, 2, cro::Colour::White);
    m_fallbackTexture.loadFromImage(img);
}

ParticleSystem::~ParticleSystem()
{
    //delete VBOs
    for (auto vbo : m_vboIDs)
    {
        if (vbo)
        {
            glCheck(glDeleteBuffers(1, &vbo));
        }
    }
#ifdef PLATFORM_DESKTOP
    for (auto vao : m_vaoIDs)
    {
        glCheck(glDeleteVertexArrays(1, &vao));
    }

#endif
}

//public
void ParticleSystem::updateDrawList(Entity cameraEnt)
{   
    const auto& cam = cameraEnt.getComponent<Camera>();
    auto passCount = cam.reflectionBuffer.available() ? 2 : 1;
    const auto camPos = cameraEnt.getComponent<cro::Transform>().getWorldPosition();
    const auto forwardVec = cro::Util::Matrix::getForwardVector(cameraEnt.getComponent<cro::Transform>().getWorldTransform());

    if (cam.getDrawListIndex() >= m_drawLists.size())
    {
        m_drawLists.resize(cam.getDrawListIndex() + 1);
    }
    auto& drawlist = m_drawLists[cam.getDrawListIndex()];

    for (auto& visible : drawlist)
    {
        visible.clear();
    }

    const auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& emitter = entity.getComponent<ParticleEmitter>();
        const auto emitterDirection = entity.getComponent<cro::Transform>().getWorldPosition() - camPos;

        for (auto i = 0; i < passCount; ++i)
        {
            if ((emitter.m_renderFlags & cam.getPass(i).renderFlags) == 0)
            {
                continue;
            }

            if (glm::dot(forwardVec, emitterDirection) > 0)
            {
                if (!emitter.m_pendingUpdate)
                {
                    emitter.m_pendingUpdate = true;
                    m_potentiallyVisible.push_back(entity);
                }

                const auto& frustum = cam.getPass(i).getFrustum();
                if (emitter.m_nextFreeParticle > 0 && inFrustum(frustum, emitter))
                {
                    drawlist[i].push_back(entity);
                }
            }
        }
    }

    DPRINT("Visible particle Systems", std::to_string(drawlist[0].size()));
}

void ParticleSystem::process(float dt)
{
    /*for (auto& handle : m_shaderHandles)
    {
        handle.boundThisFrame = false;
    }*/

    auto& entities = getEntities();
    for (auto& e : entities/*m_potentiallyVisible*/)
    {
        //check each emitter to see if it should spawn a new particle
        auto& emitter = e.getComponent<ParticleEmitter>();

        emitter.m_prevTimestamp = emitter.m_currentTimestamp;
        emitter.m_currentTimestamp += dt;

        const float rate = (1.f / emitter.settings.emitRate); //TODO this ought to be const when rate itself is set...

        if (/*emitter.m_pendingUpdate &&*/
            emitter.m_running)
        {
            auto& tx = e.getComponent<Transform>();
            glm::quat rotation = glm::quat_cast(tx.getLocalTransform());
            auto worldPos = tx.getWorldPosition();

            emitter.m_emissionTime += dt;

            while (emitter.m_emissionTime > rate)
            {
                //make sure not to update this again unless it gets marked as visible next frame
                emitter.m_pendingUpdate = false;

                //apply fallback texture if one doesn't exist
                //this would be speedier to do once when adding the emitter to the system
                //but the texture may change at runtime.
                if (emitter.settings.textureID == 0)
                {
                    emitter.settings.textureID = m_fallbackTexture.getGLHandle();
                }

                emitter.m_emissionTime -= rate;

                //interpolate position
                emitter.m_emissionTimestamp += rate;

                const float t = (emitter.m_emissionTimestamp - emitter.m_prevTimestamp) / (emitter.m_currentTimestamp - emitter.m_prevTimestamp);
                auto basePosition = glm::mix(emitter.m_previousPosition, worldPos, t);

                static const float epsilon = 0.0001f;
                auto emitCount = emitter.settings.emitCount;
                while (emitCount--)
                {
                    if (emitter.m_nextFreeParticle < emitter.m_particles.size() - 1)
                    {
                        //TODO a lot of this only needs to be calc'd ONCE for every particle in emitCount
                        const auto& settings = emitter.settings;
                        CRO_ASSERT(settings.emitRate > 0, "Emit rate must be grater than 0");
                        CRO_ASSERT(settings.lifetime > 0, "Lifetime must be greater than 0");
                        auto& p = emitter.m_particles[emitter.m_nextFreeParticle];
                        p.colour = settings.colour;
                        p.gravity = settings.gravity;
                        p.lifetime = settings.lifetime + cro::Util::Random::value(-settings.lifetimeVariance, settings.lifetimeVariance + epsilon);
                        //p.lifetime -= (emitter.m_currentTimestamp - emitter.m_emissionTimestamp);
                        p.maxLifeTime = p.lifetime;
                        //p.lifetime *= 1.f - t;

                        auto randRot = glm::rotate(rotation, Util::Random::value(-settings.spread, (settings.spread + epsilon)) * Util::Const::degToRad, Transform::X_AXIS);
                        randRot = glm::rotate(randRot, Util::Random::value(-settings.spread, (settings.spread + epsilon)) * Util::Const::degToRad, Transform::Z_AXIS);

                        auto worldScale = tx.getWorldScale();

                        p.velocity = randRot * settings.initialVelocity;
                        p.rotation = (settings.randomInitialRotation) ? Util::Random::value(-Util::Const::PI, Util::Const::PI) : 0.f;
                        p.scale = std::abs((worldScale.x + worldScale.y) / 2.f);// 1.f;
                        p.acceleration = settings.acceleration;
                        p.frameID = (settings.useRandomFrame && settings.frameCount > 1) ? cro::Util::Random::value(0, static_cast<std::int32_t>(settings.frameCount) - 1) : 0;
                        p.frameTime = 0.f;
                        p.loopCount = settings.loopCount;

                        //spawn particle in world position
                        //auto basePosition = worldPos + interpolation;// tx.getWorldPosition();
                        p.position = basePosition + (settings.initialVelocity * cro::Util::Random::value(0.001f, 0.007f));

                        //add random radius placement - TODO how to do with a position table? CAN'T HAVE +- 0!!
                        p.position.x += Util::Random::value(-settings.spawnRadius, settings.spawnRadius + epsilon);
                        p.position.y += Util::Random::value(-settings.spawnRadius, settings.spawnRadius + epsilon);
                        p.position.z += Util::Random::value(-settings.spawnRadius, settings.spawnRadius + epsilon);

                        if (emitter.settings.inheritRotation)
                        {
                            /*p.position -= basePosition;
                            p.position = rotation * glm::vec4(p.position, 1.f);
                            p.position += basePosition;*/
                            p.velocity = glm::vec3(tx.getWorldTransform() * glm::vec4(p.velocity, 0.0));
                        }

                        auto offset = settings.spawnOffset;
                        offset *= worldScale;
                        p.position += offset;


                        emitter.m_nextFreeParticle++;
                        if (emitter.m_releaseCount > 0)
                        {
                            emitter.m_releaseCount--;
                        }
                    }
                }
            }
        }

        if (emitter.m_releaseCount == 0)
        {
            emitter.stop();
        }

        //update each particle
        glm::vec3 minBounds(std::numeric_limits<float>::max());
        glm::vec3 maxBounds(0.f);

        float framerate = 1.f / emitter.settings.framerate;
        for (auto i = 0u; i < emitter.m_nextFreeParticle; ++i)
        {
            auto& p = emitter.m_particles[i];

            p.velocity *= p.acceleration;
            p.velocity += p.gravity * dt;
            for (auto f : emitter.settings.forces)
            {
                p.velocity += f * dt;
            }
            p.position += p.velocity * dt;            
           
            p.lifetime -= dt;
            p.colour.setAlpha(std::min(1.f, std::max(p.lifetime / p.maxLifeTime, 0.f)));

            p.rotation += emitter.settings.rotationSpeed * dt;
            p.scale += ((p.scale * emitter.settings.scaleModifier) * dt);

            if (emitter.settings.animate)
            {
                p.frameTime += dt;
                if (p.frameTime > framerate)
                {
                    p.frameID++;
                    if (p.frameID == emitter.settings.frameCount
                        && p.loopCount)
                    {
                        p.loopCount--;
                        p.frameID = 0;
                    }
                    p.frameTime -= framerate;
                }
            }

            //update bounds for culling
            if (p.position.x < minBounds.x) minBounds.x = p.position.x;
            if (p.position.y < minBounds.y) minBounds.y = p.position.y;
            if (p.position.z < minBounds.z) minBounds.z = p.position.z;

            if (p.position.x > maxBounds.x) maxBounds.x = p.position.x;
            if (p.position.y > maxBounds.y) maxBounds.y = p.position.y;
            if (p.position.z > maxBounds.z) maxBounds.z = p.position.z;
        }
        auto dist = (maxBounds - minBounds) / 2.f;
        emitter.m_bounds.centre = dist + minBounds;
        emitter.m_bounds.radius = glm::length(dist);

        //go over again and remove dead particles with pop/swap
        for (auto i = 0u; i < emitter.m_nextFreeParticle; ++i)
        {
            if (emitter.m_particles[i].lifetime < 0
                || ((emitter.m_particles[i].frameID == emitter.settings.frameCount)
                    && (emitter.m_particles[i].loopCount == 0)))
            {
                emitter.m_nextFreeParticle--;
                std::swap(emitter.m_particles[i], emitter.m_particles[emitter.m_nextFreeParticle]);                
            }
        }
        //DPRINT("Next free Particle", std::to_string(emitter.m_nextFreeParticle));

        //TODO sort verts by depth? should be drawing back to front for transparency really.

        //update VBO
        std::size_t idx = 0;
        for (auto i = 0u; i < emitter.m_nextFreeParticle; ++i)
        {
            const auto& p = emitter.m_particles[i];

            //position
            m_dataBuffer[idx++] = p.position.x;
            m_dataBuffer[idx++] = p.position.y;
            m_dataBuffer[idx++] = p.position.z;

            //colour
            m_dataBuffer[idx++] = p.colour.getRed();
            m_dataBuffer[idx++] = p.colour.getGreen();
            m_dataBuffer[idx++] = p.colour.getBlue();
            m_dataBuffer[idx++] = p.colour.getAlpha();

            //rotation/size/animation
            m_dataBuffer[idx++] = p.rotation * Util::Const::degToRad;
            m_dataBuffer[idx++] = p.scale;
            m_dataBuffer[idx++] = static_cast<float>(p.frameID);
        }
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, emitter.m_vbo));
        glCheck(glBufferSubData(GL_ARRAY_BUFFER, 0, idx * sizeof(float), m_dataBuffer.data()));

        emitter.m_previousPosition = e.getComponent<cro::Transform>().getWorldPosition();
    }

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    m_potentiallyVisible.clear();
}

void ParticleSystem::render(Entity camera, const RenderTarget& rt)
{   
    const auto sunlightColour = getScene()->getSunlight().getComponent<cro::Sunlight>().getColour();

    //particles are already in world space so just need viewProj
    const auto& cam = camera.getComponent<Camera>();
    if (cam.getDrawListIndex() < m_drawLists.size())
    {
        const auto& pass = cam.getActivePass();

        glm::vec4 clipPlane = glm::vec4(0.f, 1.f, 0.f, -getScene()->getWaterLevel() + (0.05f * pass.getClipPlaneMultiplier())) * pass.getClipPlaneMultiplier();

        float pointSize = 0.f;
        glCheck(glGetFloatv(GL_POINT_SIZE, &pointSize));

        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glCullFace(pass.getCullFace()));
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        ENABLE_POINT_SPRITES;

        auto vp = applyViewport(cam.viewport, rt);

        //bind shader
        const auto bindShader = [&](std::int32_t index, const ParticleEmitter& emitter)
        {
            auto& handle = m_shaderHandles[index];
            glCheck(glUseProgram(handle.id));

            //set shader uniforms (texture/projection)
            //if (!handle.boundThisFrame)
            {
                glCheck(glUniform4f(handle.uniformIDs[UniformID::ClipPlane], clipPlane.r, clipPlane.g, clipPlane.b, clipPlane.a));
                glCheck(glUniformMatrix4fv(handle.uniformIDs[UniformID::Projection], 1, GL_FALSE, glm::value_ptr(cam.getProjectionMatrix())));
                glCheck(glUniformMatrix4fv(handle.uniformIDs[UniformID::ViewProjection], 1, GL_FALSE, glm::value_ptr(pass.viewProjectionMatrix)));
                glCheck(glUniform1f(handle.uniformIDs[UniformID::Viewport], static_cast<float>(vp.height)));
                glCheck(glUniform1i(handle.uniformIDs[UniformID::Texture], 0));
                glCheck(glUniform2f(handle.uniformIDs[UniformID::CameraRange], cam.getNearPlane(), cam.getFarPlane()));

                //handle.boundThisFrame = true;
            }

            glCheck(glUniform1f(handle.uniformIDs[UniformID::ParticleSize], emitter.settings.size));
            glCheck(glUniform1f(handle.uniformIDs[UniformID::FrameCount], static_cast<float>(emitter.settings.frameCount)));
            glCheck(glUniform2f(handle.uniformIDs[UniformID::TextureSize], emitter.settings.textureSize.x, emitter.settings.textureSize.y));
        };
        glCheck(glActiveTexture(GL_TEXTURE0));



        const auto& entities = m_drawLists[cam.getDrawListIndex()][cam.getActivePassIndex()];
        for (auto entity : entities)
        {
            //it's possible an entity might be destroyed between adding to the 
            //draw list and render time - hum.
            if (entity.destroyed())
            {
                continue;
            }

            const auto& emitter = entity.getComponent<ParticleEmitter>();

            //apply blend mode - this also binds the appropriate shader for current mode
            switch (emitter.settings.blendmode)
            {
            default: break;
            case EmitterSettings::Alpha:
                glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
                bindShader(ShaderID::Alpha, emitter);
                glCheck(glUniform4f(m_shaderHandles[ShaderID::Alpha].uniformIDs[UniformID::LightColour], sunlightColour.getRed(), sunlightColour.getGreen(), sunlightColour.getBlue(), 1.f));
                break;
            case EmitterSettings::Multiply:
                glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));
                bindShader(ShaderID::Multiply, emitter);
                break;
            case EmitterSettings::Add:
                glCheck(glBlendFunc(GL_ONE, GL_ONE));
                bindShader(ShaderID::Add, emitter);
                break;
            }

            //bind emitter texture
            glCheck(glBindTexture(GL_TEXTURE_2D, emitter.settings.textureID));


#ifdef PLATFORM_DESKTOP
            glCheck(glBindVertexArray(emitter.m_vao));
            glCheck(glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(emitter.m_nextFreeParticle)));
#else
            //bind emitter vbo
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, emitter.m_vbo));

            //bind vertex attribs
            for (auto [index, attribSize, offset] : m_shaderHandles[0].attribData)
            {
                glCheck(glEnableVertexAttribArray(index));
                glCheck(glVertexAttribPointer(index, attribSize,
                    GL_FLOAT, GL_FALSE, VertexSize,
                    reinterpret_cast<void*>(static_cast<intptr_t>(offset))));
            }

            //draw
            glCheck(glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(emitter.m_nextFreeParticle)));

            //unbind attribs
            for (auto j = 0u; j < m_shaderHandles[0].attribData.size(); ++j)
            {
                glCheck(glDisableVertexAttribArray(m_shaderHandles[0].attribData[j].index));
            }
#endif //PLATFORM
        }

#ifdef PLATFORM_DESKTOP
        glCheck(glBindVertexArray(0));
#else
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif //PLATFORM

        glCheck(glUseProgram(0));
        glCheck(glBindTexture(GL_TEXTURE_2D, 0));

        restorePreviousViewport();
        glCheck(glDisable(GL_CULL_FACE));
        glCheck(glDisable(GL_BLEND));
        glCheck(glDisable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_TRUE));
        //DISABLE_POINT_SPRITES;

        glCheck(glPointSize(pointSize));
    }
}

//private
void ParticleSystem::onEntityAdded(Entity entity)
{    
    //check VBO count and increase if needed
    if (m_nextBuffer == m_bufferCount)
    {
        for (auto i = 0u; i < MinParticleSystems; ++i)
        {
            allocateBuffer();
        }
    }

    auto pos = entity.getComponent<cro::Transform>().getWorldPosition();
    entity.getComponent<cro::ParticleEmitter>().m_previousPosition = pos;

    entity.getComponent<ParticleEmitter>().m_vbo = m_vboIDs[m_nextBuffer];
    entity.getComponent<ParticleEmitter>().m_vao = m_vaoIDs[m_nextBuffer];
    m_nextBuffer++;
}

void ParticleSystem::onEntityRemoved(Entity entity)
{
    auto vboID = entity.getComponent<ParticleEmitter>().m_vbo;
    auto vaoID = entity.getComponent<ParticleEmitter>().m_vao;
    
    //update available VBOs
    std::size_t idx = 0;
    while (m_vboIDs[idx] != vboID) { idx++; }

    std::swap(m_vboIDs[idx], m_vboIDs[m_nextBuffer]);

    //and vaos
    idx = 0;
    while (m_vaoIDs[idx] != vaoID) { idx++; }

    std::swap(m_vaoIDs[idx], m_vaoIDs[m_nextBuffer]);

    m_nextBuffer--;
}

void ParticleSystem::allocateBuffer()
{
    CRO_ASSERT(m_bufferCount < m_vboIDs.size(), "Max Buffers Reached!");
    glCheck(glGenBuffers(1, &m_vboIDs[m_bufferCount]));

#ifdef PLATFORM_DESKTOP
    glCheck(glGenVertexArrays(1, &m_vaoIDs[m_bufferCount]));
    glCheck(glBindVertexArray(m_vaoIDs[m_bufferCount]));
#endif //PLATFORM

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vboIDs[m_bufferCount]));
    glCheck(glBufferData(GL_ARRAY_BUFFER, MaxVertData * sizeof(float), nullptr, GL_DYNAMIC_DRAW));

#ifdef PLATFORM_DESKTOP
    //HMMMMMMM this only works because all the shaders use the same vertex shader
    for(auto [index, attribSize, offset] : m_shaderHandles[0].attribData)
    {
        glCheck(glEnableVertexAttribArray(index));
        glCheck(glVertexAttribPointer(index, attribSize,
            GL_FLOAT, GL_FALSE, VertexSize,
            reinterpret_cast<void*>(static_cast<intptr_t>(offset))));
    }

    glCheck(glBindVertexArray(0));
#endif //PLATFORM

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    m_bufferCount++;
}
