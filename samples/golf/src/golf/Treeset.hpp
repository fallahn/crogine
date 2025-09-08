/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#pragma once

#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <string>
#include <vector>

struct Treeset final
{
    std::string modelPath;
    std::string texturePath;
    glm::vec3 colour = glm::vec3(1.f);
    float randomness = 0.2f;
    float leafSize = 0.2f;
    float colourRotation = 0.25f;

    //indices of sub meshes with branch material
    std::vector<std::uint32_t> branchIndices;

    //indices of sub meshes with leaf material
    std::vector<std::uint32_t> leafIndices;

    bool loadFromFile(const std::string& path)
    {
        modelPath.clear();
        texturePath.clear();
        
        branchIndices.clear();
        leafIndices.clear();


        cro::ConfigFile cfg;
        if (!cfg.loadFromFile(path))
        {
            return false;
        }

        auto workingPath = cro::FileSystem::getFilePath(path);

        const auto& props = cfg.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "model")
            {
                modelPath = workingPath + p.getValue<std::string>();
            }
            else if (name == "texture")
            {
                texturePath = workingPath + p.getValue<std::string>();
            }
            else if (name == "colour")
            {
                colour = p.getValue<glm::vec3>();
            }
            else if (name == "colour_rotation")
            {
                colourRotation = std::clamp(p.getValue<float>(), 0.f, 1.f);
            }
            else if (name == "randomness")
            {
                randomness = std::min(1.f, std::max(0.f, p.getValue<float>()));
            }
            else if (name == "leaf_size")
            {
                leafSize = std::min(1.f, std::max(0.1f, p.getValue<float>()));
            }
            else if (name == "branch_index")
            {
                branchIndices.push_back(p.getValue<std::uint32_t>());
            }
            else if (name == "leaf_index")
            {
                leafIndices.push_back(p.getValue<std::uint32_t>());
            }
        }


        //error checking
        auto fileName = cro::FileSystem::getFileName(path);

        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + modelPath))
        {
            LogE << fileName << ": no file exists at model path" << std::endl;
            return false;
        }

        if (!leafIndices.empty() &&
            !cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + texturePath))
        {
            LogE << fileName << ": no file exists at texture path" << std::endl;
            return false;
        }

        if (branchIndices.empty() && leafIndices.empty())
        {
            LogE << fileName << ": No material indices were specified" << std::endl;
            return false;
        }

        return true;
    }
};
