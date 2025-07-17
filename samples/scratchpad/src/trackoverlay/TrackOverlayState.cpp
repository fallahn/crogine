//Auto-generated source file for Scratchpad Stub 01/07/2024, 11:14:05

#include "TrackoverlayState.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/Font.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Easings.hpp>

#include <fstream>

namespace
{
    constexpr glm::vec2 ViewSize(1920.f, 220.f);
    constexpr glm::vec2 BannerSize(ViewSize.x, 180.f);
    constexpr glm::vec2 ThumbSize(ViewSize.y, ViewSize.y);

    constexpr glm::uvec2 TexSize(ViewSize.y, ViewSize.y);

    struct FontID final
    {
        enum
        {
            Default,
            Title,
            Artist,

            Count
        };
    };

    constexpr float BannerDepth = -0.5f;
    constexpr float TextDepth = 0.f;
    constexpr float ImageDepth = 0.5f;

    constexpr std::int32_t MaxFontSize = 100;
    constexpr std::int32_t MinFontSize = 10;

    constexpr glm::vec3 TextPosition(ThumbSize.x + 80.f, ThumbSize.y - 40.f, TextDepth);
    constexpr float TransitionSpeed = 1.f; //seconds

    const cro::Colour BannerColour = cro::Colour::Transparent;// cro::Colour(0.f, 0.f, 0.f, 0.3f);

    const std::string ThumbVertex = 
R"(
uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;

ATTRIBUTE vec2 a_position;
ATTRIBUTE MED vec2 a_texCoord0;
ATTRIBUTE LOW vec4 a_colour;

VARYING_OUT LOW vec4 v_colour;
VARYING_OUT MED vec2 v_texCoord;
VARYING_OUT vec3 v_worldPos;

void main()
{
    vec4 worldPos = u_worldMatrix * vec4(a_position, 0.0, 1.0);
    gl_Position = u_viewProjectionMatrix * worldPos;
    
    v_colour = a_colour;
    v_texCoord = a_texCoord0;
    v_worldPos = worldPos.xyz;
})";

    const std::string ThumbFrag = 
R"(
OUTPUT

uniform sampler2DArray u_texture;
uniform float u_index = 0.0;
uniform vec3 u_surfaceNormal = vec3(0.0, 0.0, 1.0);

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;
VARYING_IN vec3 v_worldPos;

const vec3 LightDir = vec3(0.57735, 0.57735, 0.57735);
const vec3 CamPos = vec3(90.0, 90.0, 400.0);
const float SpecularStrength = 0.5;

void main()
{
    vec4 colour = texture(u_texture, vec3(v_texCoord, u_index)) * v_colour;

    vec3 viewDir = normalize(CamPos - v_worldPos);
    vec3 reflectDir = reflect(normalize(-LightDir), u_surfaceNormal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    float specular = SpecularStrength * spec;
    colour.rgb += specular;

    FRAG_OUT = colour;
})";
}

TrackOverlayState::TrackOverlayState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_currentIndex  (0),
    m_showSettings  (false)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });

    cro::App::getInstance().setClearColour(cro::Colour(0.f, 1.f, 0.f, 0.f));
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
}

//public
bool TrackOverlayState::handleEvent(const cro::Event& evt)
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
        case SDLK_SPACE:
            nextTrack();
            break;
        case SDLK_F2:
            m_showSettings = !m_showSettings;
            if (!m_showSettings)
            {
                writeSettings();
            }
            break;
        }
    }
    else if(evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonRightShoulder:
            nextTrack();
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void TrackOverlayState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool TrackOverlayState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void TrackOverlayState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void TrackOverlayState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::AudioSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void TrackOverlayState::loadAssets()
{
    readSettings();

    m_resources.fonts.load(FontID::Default, "assets/fonts/VeraMono.ttf");
    m_resources.fonts.load(FontID::Artist, m_settings.artistFont);
    m_resources.fonts.load(FontID::Title, m_settings.titleFont);
   
    m_fallbackImage.create(TexSize.x, TexSize.y, cro::Colour::Black);
    
    const float halfSize = static_cast<float>(TexSize.x / 2);
    const glm::vec2 centre(halfSize);
    for (auto i = 0u; i < (TexSize.x * TexSize.y); ++i)
    {
        auto x = i % TexSize.x;
        auto y = i / TexSize.y;

        glm::vec2 position(x, y);
        const float dist = glm::length(position - centre);
        const float alpha = (1.f - glm::smoothstep(halfSize - 3.5f, halfSize - 1.5f, dist)) * 255.f;
        const auto colour = std::uint8_t((1.f - glm::smoothstep(halfSize - 73.f, halfSize - 71.f, dist)) * 255.f);

        cro::Colour c(colour, colour, colour, std::uint8_t(alpha));
        m_fallbackImage.setPixel(x, y, c);
    }

    m_textures.create(TexSize.x, TexSize.y);
    m_textures.insertLayer(m_fallbackImage, 0);

    if (m_thumbShader.loadFromString(ThumbVertex, ThumbFrag))
    {
        m_shaderHandle.id = m_thumbShader.getGLHandle();
        m_shaderHandle.indexUniform = m_thumbShader.getUniformID("u_index");
        m_shaderHandle.normalUniform = m_thumbShader.getUniformID("u_surfaceNormal");
    }

    loadAlbumDirectory();
}

void TrackOverlayState::createScene()
{
    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.8f, 2.f });
}

void TrackOverlayState::createUI()
{
    //banner
    constexpr float Padding = (ViewSize.y - BannerSize.y) / 2.f;
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, BannerDepth });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, BannerSize.y + Padding), BannerColour),
            cro::Vertex2D(glm::vec2(0.f, Padding), BannerColour),
            cro::Vertex2D(glm::vec2(BannerSize.x, BannerSize.y + Padding), BannerColour),
            cro::Vertex2D(glm::vec2(BannerSize.x, Padding), BannerColour)
        }    
    );

    //thumbnail
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 40.f + (ThumbSize.x / 2.f), 0.f, ImageDepth});
    entity.getComponent<cro::Transform>().setOrigin({ ThumbSize.x / 2.f, 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, ThumbSize.y), glm::vec2(0.f, 1.f)),
            cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f)),
            cro::Vertex2D(ThumbSize, glm::vec2(1.f)),
            cro::Vertex2D(glm::vec2(ThumbSize.x, 0.f), glm::vec2(1.f, 0.f))
        }
    );
    entity.getComponent<cro::Drawable2D>().setShader(&m_thumbShader);
    entity.getComponent<cro::Drawable2D>().setTexture(cro::TextureID(m_textures), TexSize);
    m_displayEnts.thumb = entity;

    cro::String title("No Tracks Loaded");
    cro::String artist("Buns.");

    if (!m_textStrings.empty())
    {
        title = m_textStrings[0].first;
        artist = m_textStrings[0].second;
    }

    //title text
    const auto& titleFont = m_resources.fonts.get(FontID::Title);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(TextPosition);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(titleFont).setString(title);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::White);
    entity.getComponent<cro::Text>().setCharacterSize(m_settings.titleFontSize);

    struct CallbackData final
    {
        float width = 0.f;
        std::int32_t state = 0; //0 out 1 in
        CallbackData(float w, std::int32_t s)
            : width(w), state(s) {}
    };

    entity.addComponent<cro::Callback>().setUserData<CallbackData>(cro::Text::getLocalBounds(entity).width, 0);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& [width, state] = e.getComponent<cro::Callback>().getUserData<CallbackData>();
            const float Speed = (dt * (ViewSize.x * TransitionSpeed) * 2.f);

            float rotation = 0.f;
            glUseProgram(m_shaderHandle.id);

            e.getComponent<cro::Transform>().move({ -Speed, 0.f });
            const auto xPos = e.getComponent<cro::Transform>().getPosition().x;
            if (state == 0)
            {
                const float imageScale = 1.f - ((xPos - TextPosition.x) / -width);
                rotation = -imageScale;
                m_displayEnts.thumb.getComponent<cro::Transform>().setScale({ cro::Util::Easing::easeOutSine(imageScale), 1.f });

                if (xPos < -width)
                {
                    e.getComponent<cro::Transform>().move({ width + ViewSize.x, 0.f });

                    const auto& [title, artist] = m_textStrings[m_currentIndex];
                    m_displayEnts.artistText.getComponent<cro::Text>().setString(artist);
                    m_displayEnts.titleText.getComponent<cro::Text>().setString(title);
                    //TODO check size and scale to fit if needed

                    //update the image index
                    glUniform1f(m_shaderHandle.indexUniform, static_cast<float>(m_currentIndex));

                    width = cro::Text::getLocalBounds(e).width;
                    state = 1;
                }
            }
            else
            {
                const float imageScale = 1.f - ((xPos - TextPosition.x) / (ViewSize.x - TextPosition.x));
                rotation = imageScale;
                m_displayEnts.thumb.getComponent<cro::Transform>().setScale({ cro::Util::Easing::easeInSine(imageScale), 1.f });

                if (xPos < TextPosition.x)
                {
                    m_displayEnts.thumb.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    e.getComponent<cro::Transform>().setPosition(TextPosition);
                    state = 0;
                    e.getComponent<cro::Callback>().active = 0;
                }
            }

            glm::quat q = glm::rotate(cro::Transform::QUAT_IDENTITY, rotation * (cro::Util::Const::PI / 2.f), cro::Transform::Y_AXIS);
            glm::vec3 normal = q * cro::Transform::Z_AXIS;
            glUniform3f(m_shaderHandle.normalUniform, normal.x, normal.y, normal.z);
        };

    auto titleText = entity;
    m_displayEnts.titleText = entity;

    //artist text
    const auto& artistFont = m_resources.fonts.get(FontID::Artist);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -80.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(artistFont).setString(artist);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::White);
    entity.getComponent<cro::Text>().setCharacterSize(m_settings.artistFontSize);
    titleText.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_displayEnts.artistText = entity;


    registerWindow([&]()
        {
            if (m_showSettings)
            {
                if (ImGui::Begin("Settings"), nullptr, ImGuiWindowFlags_NoTitleBar)
                {
                    ImGui::Text("Title Font: %s", cro::FileSystem::getFileName(m_settings.titleFont).c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("Choose##0"))
                    {
                        auto path = cro::FileSystem::openFileDialogue("c:/windows/fonts/Arial.ttf");
                        if (!path.empty())
                        {
                            m_settings.titleFont = path;
                            m_resources.fonts.get(FontID::Title).loadFromFile(path);
                            m_displayEnts.titleText.getComponent<cro::Text>().setFont(m_resources.fonts.get(FontID::Title));
                        }
                    }

                    if (ImGui::InputInt("Size##0", &m_settings.titleFontSize))
                    {
                        m_settings.titleFontSize = std::clamp(m_settings.titleFontSize, MinFontSize, MaxFontSize);
                        m_displayEnts.titleText.getComponent<cro::Text>().setCharacterSize(m_settings.titleFontSize);
                    }

                    ImGui::Text("Artist Font: %s", cro::FileSystem::getFileName(m_settings.artistFont).c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("Choose##1"))
                    {
                        auto path = cro::FileSystem::openFileDialogue("c:/windows/fonts/Arial.ttf");
                        if (!path.empty())
                        {
                            m_settings.artistFont = path;
                            m_resources.fonts.get(FontID::Artist).loadFromFile(path);
                            m_displayEnts.artistText.getComponent<cro::Text>().setFont(m_resources.fonts.get(FontID::Artist));
                        }
                    }

                    if (ImGui::InputInt("Size##1", &m_settings.artistFontSize))
                    {
                        m_settings.artistFontSize = std::clamp(m_settings.artistFontSize, MinFontSize, MaxFontSize);
                        m_displayEnts.artistText.getComponent<cro::Text>().setCharacterSize(m_settings.artistFontSize);
                    }

                    ImGui::Text("Current Dir: %s", m_settings.albumDirectory.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("Choose##2"))
                    {
                        auto path = cro::FileSystem::openFolderDialogue();
                        if (!path.empty())
                        {
                            std::replace(path.begin(), path.end(), '\\', '/');
                            if (path.back() == '/')
                            {
                                path.pop_back();
                            }
                            m_settings.albumDirectory = path;
                            loadAlbumDirectory();
                        }
                    }

                    if (ImGui::Button("Write Timestamps"))
                    {
                        std::ofstream file(m_settings.albumDirectory + "/yt.txt");
                        for (const auto& [title, artist] : m_textStrings)
                        {
                            const auto utf0 = title.toUtf8();
                            const auto utf1 = artist.toUtf8();

                            file << "00:00 " << utf0.c_str() << " - " << utf1.c_str() << "\n";
                        }
                    }


                    if (ImGui::Button("Close"))
                    {
                        writeSettings();
                        m_showSettings = false;
                    }
                }
                ImGui::End();
            }        
        });


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());

        cam.viewport = {0.f, 0.f, 1.f, (size.x / ViewSize.x) * (ViewSize.y / size.y)};
        //cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);
        cam.setOrthographic(0.f, ViewSize.x, 0.f, ViewSize.y, -1.f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}

void TrackOverlayState::loadAlbumDirectory()
{
    m_currentIndex = 0;
    m_textStrings.clear();

    std::uint32_t index = 0;
    cro::Image thumbImage(true);

    cro::ConfigFile cfg;
    if (cfg.loadFromFile(m_settings.albumDirectory + "/tracklist.cfg"))
    {
        const auto& obs = cfg.getObjects();
        for (const auto& ob : obs)
        {
            cro::String title;
            cro::String artist;
            std::string image;

            const auto& props = ob.getProperties();
            for (const auto& prop : props)
            {
                if (prop.getName() == "title")
                {
                    title = prop.getValue<cro::String>();
                }
                else if (prop.getName() == "artist")
                {
                    artist = prop.getValue<cro::String>();
                }
                else if (prop.getName() == "thumb")
                {
                    image = prop.getValue<std::string>();
                }
            }

            if (!title.empty() && !artist.empty())
            {
                m_textStrings.emplace_back(std::make_pair(title, artist));

                //load image and insert at index
                if (!image.empty())
                {
                    if (thumbImage.loadFromFile(m_settings.albumDirectory + "/" + image))
                    {
                        if (thumbImage.getSize().x != TexSize.x
                            || thumbImage.getSize().y != TexSize.y)
                        {
                            thumbImage.resize(TexSize);
                        }

                        if (!m_textures.insertLayer(thumbImage, index))
                        {
                            m_textures.insertLayer(m_fallbackImage, index);
                        }
                    }
                    else
                    {
                        m_textures.insertLayer(m_fallbackImage, index);
                    }
                }

                index++;

                if (index == MaxTracks)
                {
                    break;
                }
            }
        }
    }
}

void TrackOverlayState::readSettings()
{
    cro::ConfigFile cfg;
    if (cfg.loadFromFile("settings.cfg"))
    {
        for (const auto& prop : cfg.getProperties())
        {
            if (prop.getName() == "title_font")
            {
                m_settings.titleFont = prop.getValue<std::string>();
            }
            else if (prop.getName() == "title_font_size")
            {
                m_settings.titleFontSize = std::clamp(prop.getValue<std::int32_t>(), MinFontSize, MaxFontSize);
            }
            else if (prop.getName() == "artist_font")
            {
                m_settings.artistFont = prop.getValue<std::string>();
            }
            else if (prop.getName() == "artist_font_size")
            {
                m_settings.artistFontSize = std::clamp(prop.getValue<std::int32_t>(), MinFontSize, MaxFontSize);
            }
            else if (prop.getName() == "directory")
            {
                m_settings.albumDirectory = prop.getValue<std::string>();
            }
        }
    }

    if (m_settings.titleFont.empty())
    {
        m_settings.titleFont = "assets/fonts/VeraMono.ttf";
    }

    if (m_settings.artistFont.empty())
    {
        m_settings.artistFont = "assets/fonts/VeraMono.ttf";
    }

    if (m_settings.albumDirectory.empty())
    {
        m_settings.albumDirectory = "assets";
    }
}

void TrackOverlayState::writeSettings() const
{
    cro::ConfigFile cfg;
    cfg.addProperty("title_font").setValue(m_settings.titleFont);
    cfg.addProperty("title_font_size").setValue(m_settings.titleFontSize);
    cfg.addProperty("artist_font").setValue(m_settings.artistFont);
    cfg.addProperty("artist_font_size").setValue(m_settings.artistFontSize);
    cfg.addProperty("directory").setValue(m_settings.albumDirectory);
    cfg.save("settings.cfg");
}

void TrackOverlayState::nextTrack()
{
    if (!m_textStrings.empty() &&
        !m_displayEnts.titleText.getComponent<cro::Callback>().active)
    {
        m_displayEnts.titleText.getComponent<cro::Callback>().active = true;


        m_currentIndex = (m_currentIndex + 1) % m_textStrings.size();
    }
}