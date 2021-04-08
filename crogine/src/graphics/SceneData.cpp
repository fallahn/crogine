/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <crogine/graphics/SceneData.hpp>

using namespace cro;

SceneData::SceneData()
{
    //model object
    SceneData::ComponentParser parser;
    parser.componentName = "models";
    parser.objectName = "model";
    parser.parseFunc = 
        [](const cro::ConfigFile& obj, std::any& dest)
    {
        SceneData::Model modelData;

        const auto& props = obj.getProperties();
        for (const auto& prop : props)
        {
            const auto& name = prop.getName();
            if (name == "src")
            {
                modelData.src = prop.getValue<std::string>();
            }
            else if (name == "id")
            {
                modelData.id = prop.getValue<std::int32_t>();
            }
        }

        if (modelData.src.empty())
        {
            //TODO print something more useful such as which file this is
            LogW << "Model data object with no src parameter" << std::endl;
            return;
        }

        if (modelData.id == -1)
        {
            LogW << "Model data with no ID found" << std::endl;
            return;
        }

        std::any_cast<std::vector<SceneData::Model>&>(dest).push_back(modelData);
    };
    registerComponentType<SceneData::Model>(parser);


    //rect object
    parser.componentName = "rects";
    parser.objectName = "rect";
    parser.parseFunc =
        [](const cro::ConfigFile& obj, std::any& dest)
    {
        Rect rect;

        const auto& props = obj.getProperties();
        for (const auto& prop : props)
        {
            const auto& name = prop.getName();
            if (name == "id")
            {
                rect.id = prop.getValue<std::int32_t>();
            }
            else if (name == "size")
            {
                rect.rect = prop.getValue<FloatRect>();
            }
            else if (name == "depth")
            {
                rect.depth = prop.getValue<float>();
            }
            else if (name == "depth_axis")
            {
                auto axis = prop.getValue<std::int32_t>();
                if (axis > -1 && axis < 3)
                {
                    rect.depthAxis = static_cast<Rect::Axis>(axis);
                }
                else
                {
                    LogW << axis << ": invalid rect axis value" << std::endl;
                }
            }
        }

        if (rect.id == -1)
        {
            LogW << "rect with missing ID" << std::endl;
            return;
        }

        std::any_cast<std::vector<Rect>&>(dest).push_back(rect);
    };
    registerComponentType<SceneData::Rect>(parser);


    //node object
    parser.componentName = "nodes";
    parser.objectName = "node";
    parser.parseFunc =
        [&](const cro::ConfigFile& obj, std::any& dest)
    {
        static constexpr std::int32_t MaxDepth = 12;
        std::int32_t depth = 0;

        std::function<SceneData::Node(const cro::ConfigObject&)> getNode = [&](const cro::ConfigObject& o)
        {
            static constexpr std::int32_t MaxComponents = 12;
            std::int32_t componentCount = 0;

            SceneData::Node node;

            const auto& props = o.getProperties();
            for (const auto& prop : props)
            {
                const auto& name = prop.getName();
                if (name == "position")
                {
                    node.position = prop.getValue<glm::vec3>();
                }
                else if (name == "rotation")
                {
                    auto rot = prop.getValue<glm::vec3>();
                    rot = glm::radians(rot);
                    node.orientation = glm::quat(rot);
                }
                else if (name == "scale")
                {
                    node.scale = prop.getValue<glm::vec3>();
                }
                else
                {
                    //search component types.
                    if (componentCount++ < MaxComponents)
                    {
                        auto parser = std::find_if(m_parsers.begin(), m_parsers.end(),
                            [name](const auto& p)
                            {
                                return p.second.objectName == name;
                            });

                        if (parser != m_parsers.end())
                        {
                            node.components.emplace_back(std::make_pair(parser->first, prop.getValue<std::int32_t>()));
                        }
                    }
                }
            }

            if (depth++ < MaxDepth)
            {
                const auto& subObjs = o.getObjects();
                for (const auto& sub : subObjs)
                {
                    const auto& name = sub.getName();
                    if (name == "node")
                    {
                        node.children.push_back(getNode(sub));
                    }
                }
            }

            return node;
        };

        std::any_cast<std::vector<SceneData::Node>&>(dest).push_back(getNode(obj));
    };
    registerComponentType<SceneData::Node>(parser);
}

//public
bool SceneData::loadFromFile(const std::string& path)
{
    cro::ConfigFile file;
    if (file.loadFromFile(path))
    {
        //parse sky properties
        m_properties = {};
        const auto& sceneProps = file.getProperties();
        for (const auto& prop : sceneProps)
        {
            const auto& propName = prop.getName();
            if (propName == "sky_colour")
            {
                m_properties.skyColour = prop.getValue<cro::Colour>();
            }
            else if (propName == "sky_direction")
            {
                m_properties.skyDirection = prop.getValue<glm::vec3>();
            }
            else if (propName == "cubemap")
            {
                m_properties.cubemap = prop.getValue<std::string>();
            }
        }

        const auto& objs = file.getObjects();
        for (const auto& obj : objs)
        {
            //see if we have a parser for this object
            const auto& componentName = obj.getName();
            auto parser = std::find_if(m_parsers.begin(), m_parsers.end(),
                [&componentName](const auto& p)
                {
                    return p.second.componentName == componentName;
                });

            //and parse it if we do
            if (parser != m_parsers.end())
            {
                const auto& [cName, objName, parseFunc] = parser->second;

                const auto& componentObjs = obj.getObjects();
                for (const auto& componentData : componentObjs)
                {
                    if (componentData.getName() == objName)
                    {
                        //store component
                        parseFunc(componentData, m_components[parser->first]);
                    }
                }
            }

            //TODO run a post process callback to remove eg duplicate IDs
        }

        return true;
    }
    return false;
}