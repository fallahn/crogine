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

#pragma once

#include "Q3Bsp.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>
#include <crogine/graphics/Texture.hpp>

class Q3BspSystem final : public cro::System, public cro::Renderable
{
public:
    explicit Q3BspSystem(cro::MessageBus&);

    void process(float) override;
    void updateDrawList(cro::Entity camera) override;
    void render(cro::Entity camera, const cro::RenderTarget& target) override;


    bool loadMap(const std::string&);

private:

    std::vector<Q3::Face> m_faces;
    std::vector<cro::Texture> m_lightmaps;

    void buildLightmaps(SDL_RWops* file, std::uint32_t count);

    template <typename T>
    void parseLump(std::vector<T>& dest, SDL_RWops* file, std::int64_t offsetStart, Q3::Lump lumpInfo)
    {
        auto lumpCount = lumpInfo.length / sizeof(T);
        dest.resize(lumpCount);
        SDL_RWseek(file, offsetStart + lumpInfo.offset, RW_SEEK_SET);
        SDL_RWread(file, dest.data(), sizeof(T), lumpCount);
    }
};