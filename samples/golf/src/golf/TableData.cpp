/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

#include "TableData.hpp"

#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/FileSystem.hpp>

const std::array<std::string, CollisionID::Count> CollisionID::Labels =
{
    "Table", "Cushion", "Ball", "Pocket"
};

const std::array<std::string, TableData::Rules::Count> TableData::RuleStrings =
{
    "8_ball", "9_ball", "bar_billiards", "snooker", "void"
};

bool TableData::loadFromFile(const std::string& path)
{
    cro::ConfigFile tableConfig;
    if (tableConfig.loadFromFile(path))
    {
        name = cro::FileSystem::getFileName(path);
        name = name.substr(0, name.find_last_of('.'));

        const auto& props = tableConfig.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "model")
            {
                viewModel = p.getValue<std::string>();
            }
            else if (name == "collision")
            {
                collisionModel = p.getValue<std::string>();
            }
            else if (name == "ruleset")
            {
                std::string ruleString = p.getValue<std::string>();
                auto result = std::find_if(TableData::RuleStrings.begin(), TableData::RuleStrings.end(), [&ruleString](const std::string& s) {return s == ruleString; });
                if (result != TableData::RuleStrings.end())
                {
                    rules = static_cast<TableData::Rules>(std::distance(TableData::RuleStrings.begin(), result));
                }
                else
                {
                    LogE << "No valid rule set data found" << std::endl;
                }
            }
            else if (name == "ball_skin")
            {
                std::string skin = p.getValue<std::string>();
                if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + skin))
                {
                    ballSkins.push_back(skin);
                }
            }
            else if (name == "table_skin")
            {
                std::string skin = p.getValue<std::string>();
                if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + skin))
                {
                    tableSkins.push_back(skin);
                }
            }
        }

        if (viewModel.empty() || cro::FileSystem::getFileExtension(viewModel) != ".cmt")
        {
            LogE << "Invalid path to table view model" << std::endl;
        }

        if (collisionModel.empty() || cro::FileSystem::getFileExtension(collisionModel) != ".cmb")
        {
            LogE << "Invalid path to table collision model" << std::endl;
        }

        //note that the cfg file gives x/y position
        //which should be converted to x/-z
        const auto& objs = tableConfig.getObjects();
        for (const auto& obj : objs)
        {
            const auto& name = obj.getName();
            if (name == "pocket")
            {
                auto& pocket = pockets.emplace_back();

                const auto& pocketProps = obj.getProperties();
                for (const auto& prop : pocketProps)
                {
                    const auto& propName = prop.getName();
                    if (propName == "position")
                    {
                        pocket.position = prop.getValue<glm::vec2>();
                    }
                    else if (propName == "value")
                    {
                        pocket.value = prop.getValue<std::int32_t>();
                    }
                    else if (propName == "radius")
                    {
                        pocket.radius = prop.getValue<float>();
                    }
                }
            }
            else if (name == "spawn")
            {
                glm::vec2 size = glm::vec2(0.f);
                glm::vec2 position = glm::vec2(0.f);

                const auto& spawnProps = obj.getProperties();
                for (const auto& prop : spawnProps)
                {
                    const auto& sName = prop.getName();
                    if (sName == "position")
                    {
                        position = prop.getValue<glm::vec2>();
                    }
                    else if (sName == "size")
                    {
                        size = prop.getValue<glm::vec2>();
                    }
                }

                spawnArea.left = position.x - size.x;
                spawnArea.bottom = position.y - size.y;
                spawnArea.width = size.x * 2.f;
                spawnArea.height = size.y * 2.f;
            }
        }


        //load the default table skin
        if (!viewModel.empty())
        {
            cro::ConfigFile vm;
            vm.loadFromFile(viewModel);
            if (auto* skin = vm.findProperty("diffuse"); skin != nullptr)
            {
                auto skinPath = skin->getValue<std::string>();
                if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + skinPath))
                {
                    tableSkins.push_back(skinPath);
                }
            }
        }

        return !viewModel.empty() && !collisionModel.empty()
            && !pockets.empty() && rules != TableData::Void
            && spawnArea.width > 0 && spawnArea.height > 0
            && !tableSkins.empty() && !ballSkins.empty();
    }
    return false;
}
