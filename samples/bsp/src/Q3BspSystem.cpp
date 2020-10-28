/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "Q3BspSystem.hpp"

#include <crogine/core/FileSystem.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/graphics/Image.hpp>

Q3BspSystem::Q3BspSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(Q3BspSystem))
{

}

//public
void Q3BspSystem::process(float)
{

}

void Q3BspSystem::updateDrawList(cro::Entity)
{

}

void Q3BspSystem::render(cro::Entity, const cro::RenderTarget&)
{

}

bool Q3BspSystem::loadMap(const std::string& mapPath)
{
    LOG("TODO clear existing data before loading new map", cro::Logger::Type::Info);
    m_faces.clear();

    auto path = cro::FileSystem::getResourcePath() + mapPath;

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "rb");
    if (file.file)
    {
        //validate size
        SDL_RWseek(file.file, 0, RW_SEEK_END);
        auto fileSize = SDL_RWtell(file.file);

        if (fileSize < sizeof(Q3::Header))
        {
            LogE << mapPath << ": error reading file, size too small" << std::endl;
            return false;
        }

        SDL_RWseek(file.file, 0, RW_SEEK_SET);

        //validate header
        Q3::Header header;

        SDL_RWread(file.file, &header, sizeof(header), 1);
        std::string id(header.id, 4);
        if (id != "IBSP")
        {
            LogE << mapPath << ": Incorrect header id. Should be IBSP, got " << id << std::endl;
            return false;
        }
        if (header.version != 46)
        {
            LogE << mapPath << ": Incorrect version id. Should be 46, got " << header.version << std::endl;
            return false;
        }

        //read lump info
        std::vector<Q3::Lump> lumpInfo(Q3::Lumps::MaxLumps);
        SDL_RWread(file.file, lumpInfo.data(), sizeof(Q3::Lump) * Q3::Lumps::MaxLumps, 1);

        //so we can track where the lump data actually starts
        auto lumpStart = SDL_RWtell(file.file);

        //parse the vertex list
        std::vector<Q3::Vertex> vertices;
        parseLump(vertices, file.file, lumpStart, lumpInfo[Q3::Vertices]);

        //this is stored as member data as it's used by the PVS during rendering
        parseLump(m_faces, file.file, lumpStart, lumpInfo[Q3::Faces]);

        //parse the index list
        std::vector<std::int32_t> indices;
        parseLump(indices, file.file, lumpStart, lumpInfo[Q3::Indices]);

        //TODO load texture/material info

        //load the lightmap data
        std::uint32_t lightmapCount = lumpInfo[Q3::Lightmaps].length / sizeof(Q3::Lightmap);
        SDL_RWseek(file.file, lumpStart + lumpInfo[Q3::Lightmaps].offset, RW_SEEK_SET);
        buildLightmaps(file.file, lightmapCount);

        return true;
    }

    return false;
}

//private
void Q3BspSystem::buildLightmaps(SDL_RWops* file, std::uint32_t count)
{
    std::vector<std::uint8_t> buffer(128 * 128 * 3);

    auto adjustGamma = [&buffer]()
    {
        const float amount = 8.f;

        for (auto i = 0u; i < buffer.size(); ++i)
        {
            float scale = 1.f;
            float temp = 0.f;

            float currentByte = static_cast<float>(buffer[i]);
            currentByte *= amount / 255.f;

            //clamp to max value
            if (currentByte > 1.f && (temp = (1.f / currentByte)) < scale) scale = temp;

            currentByte *= scale * 255.f;
            buffer[i] = static_cast<std::uint8_t>(currentByte);
        }
    };

    for (auto i = 0u; i < count; ++i)
    {
        SDL_RWread(file, buffer.data(), buffer.size(), 1);
        adjustGamma();
        //TODO flip vertically?

        auto& texture = m_lightmaps.emplace_back();
        texture.create(128, 128, cro::ImageFormat::RGB);
        texture.update(buffer.data());
        texture.setSmooth(true);

        /*cro::Image img;
        img.loadFromMemory(buffer.data(), 128, 128, cro::ImageFormat::RGB);
        img.write("img_" + std::to_string(i) + ".png");*/
    }

    //add a blank lightmap at the end of the array so faces with no
    //lightmap can be assigned this in the shader, as the shader still
    //requires the lightmap uniform be set.
    std::fill(buffer.begin(), buffer.end(), 255);
    auto& texture = m_lightmaps.emplace_back();
    texture.create(128, 128, cro::ImageFormat::RGB);
    texture.update(buffer.data());
}