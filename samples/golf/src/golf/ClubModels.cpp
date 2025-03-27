/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "ClubModels.hpp"
#include "GameConsts.hpp"

#include <crogine/core/ConfigFile.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

bool ClubModels::loadFromFile(const std::string& path, cro::ResourceCollection& resources, cro::Scene& scene)
{
    //hmm if this vector is not empty should we be destroying all the entities first?
    models.clear();
    std::fill(indices.begin(), indices.end(), 0);


    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path))
    {
        //load the models first so we can clamp the indices
        auto rootPath = cro::FileSystem::getFilePath(path);
        cro::ModelDefinition md(resources);

        const auto& objs = cfg.getObjects();
        for (const auto& o : objs)
        {
            if (o.getName() == "models")
            {
                const auto& props = o.getProperties();
                for (const auto& p : props)
                {
                    if (p.getName() == "path")
                    {
                        const auto modelPath = rootPath + p.getValue<std::string>();
                        if (md.loadFromFile(modelPath))
                        {
                            auto entity = scene.createEntity();
                            entity.addComponent<cro::Transform>();
                            md.createModel(entity);

                            //TODO we'll apply the models on returning from this func when
                            //we decide if we need a fallback model or not - so just track
                            //the ID of the requested materials

                            auto& v = materialIDs.emplace_back();
                            const auto matCount = entity.getComponent<cro::Model>().getMeshData().submeshCount;

                            for (auto i = 0u; i < matCount; ++i)
                            {
                                v.push_back(md.hasTag(i, "illum") ? Emissive : Flat);
                            }
                            models.push_back(entity);
                        }
                    }
                }
            }
        }


        if (models.empty())
        {
            //no point continuing - we'll let the game load a fallback model
            return false;
        }


        const std::int32_t ModelCount = static_cast<std::int32_t>(models.size()) - 1;

        const auto& props = cfg.getProperties();
        for (const auto& p : props)
        {
            const auto& pName = p.getName();
            if (pName == "uid")
            {
                uid = p.getValue<std::uint32_t>();
            }
            else if (pName == "name")
            {
                name = p.getValue<std::string>();
            }
            else if (pName == "driver")
            {
                indices[ClubID::Driver] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
            else if (pName == "three_wood")
            {
                indices[ClubID::ThreeWood] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
            else if (pName == "five_wood")
            {
                indices[ClubID::FiveWood] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
            else if (pName == "four_iron")
            {
                indices[ClubID::FourIron] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
            else if (pName == "five_iron")
            {
                indices[ClubID::FiveIron] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
            else if (pName == "six_iron")
            {
                indices[ClubID::SixIron] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
            else if (pName == "seven_iron")
            {
                indices[ClubID::SevenIron] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
            else if (pName == "eight_iron")
            {
                indices[ClubID::EightIron] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
            else if (pName == "nine_iron")
            {
                indices[ClubID::NineIron] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
            else if (pName == "pitch_wedge")
            {
                indices[ClubID::PitchWedge] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
            else if (pName == "gap_wedge")
            {
                indices[ClubID::GapWedge] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
            else if (pName == "sand_wedge")
            {
                indices[ClubID::SandWedge] = std::clamp(p.getValue<std::int32_t>(), 0, ModelCount);
            }
        }

#ifdef USE_GNS
        //check to see if we can get a workshop ID from the path
        if (rootPath.back() == '/')
        {
            rootPath.pop_back();
        }

        if (rootPath.back() == 'w')
        {
            //LogI << "Processing: " << rootPath << std::endl;
            if (auto res = rootPath.find_last_of('/'); res != std::string::npos)
            {
                try
                {
                    const auto d = rootPath.substr(res + 1, rootPath.length() - 1);
                    workshopID = std::stoull(d);
                    //LogI << "Got workshop ID of " << workshopID << std::endl;
                }
                catch (...)
                {
                    LogW << "could not get workshop ID for " << rootPath << std::endl;
                }
            }
        }
        /*else
        {
            LogI << "Rootpath: " << rootPath << std::endl;
        }*/
#endif


        return true;
    }
    return false;
}