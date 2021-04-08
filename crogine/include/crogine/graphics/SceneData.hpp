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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/graphics/Colour.hpp>

#include <crogine/detail/glm/gtc/quaternion.hpp>

#include <string>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <any>

namespace cro
{
    /*!
    \brief Parses (or attempts to parse) a cro::ConfigFile containing Scene data
    in the following format:

    scene <id>
    The root object of the file should be named scene with an optional id. It can contain
    the following properties:
    sky_colour - a 4 component vector containing the initial colour of the sky light
    sky_direction - a 3 component vector contaiing the initial direction of the sky light
    cubemap - a string containing the relative path to a cubemap file with which to render the sky

    The scene should also contain one or more objects, each of which contain one or more
    objects to describe a set of asset types to be loaded:
    
    An object named models. This object then contains one or
    more model objects, which should be loaded by crogine. The model object has 2 properties:

    models
    {
        model <id>
        {
            src = "relative/path/to/model.cmt"
            id = 0
        }

        model <id>
        {
            src = "relative/path/to/other_model.cmt"
            id = 1
        }
    }

    If either properties are missing the object will be skipped. If an object has already been
    loaded with the given id then the object will be skipped.

    An object named rects, containing one or more 2D rectangles, which are axis-aligned.
    
    rects
    {
        rect
        {
            id = 1          //unique ID. If this ID has be loaded already then the object is skipped
            size = 0,0,12,4 //size in FloatRect format: left,bottom, width, height
            depth = 0       //depth on the depth axis in world units
            depth_axis = 2  //the axis perpendicular to the plane, with x,y,z mapped to 0,1,2 repectively
        }
    }


    Once all assets have been listed the scene graph describes the layout of the scene's entities.
    The scene graph is contained within the nodes object, similarly to asset objects. However,
    Objects contained within another object are parented to it, which mean that any nested node objects
    have their transform multiplied by their parents. In other words the transform properties of a node 
    object should be described in local space.

    Each node should have at least a position described as a 3 component vector. They may also have
    a rotation in euler angles (in degrees), and a scale, both or which are also 3 component vectors.

    For a node to have any real meaning they should also have at least one property describing one
    of the assets described by the asset objects:


    nodes
    {
        node player
        {
            position = 10, 0 ,10
            rotation = 0, 90, 0
            model = 1

            node gun
            {
                position = 0.5, 0, 0.1
                model = 2
            }
        }

        node wall
        {
            position = 0, 0, -10
            model = 0
            rect = 3
        }
    }

    Note that the player node uses a model with id of 1, and the wall node usees both a model id of 0
    and a rect with an id of 3.

    */
    class CRO_EXPORT_API SceneData final
    {
    public:

        /*!
        \brief Struct representing a Model object in a SceneData file
        */
        struct Model final
        {
            std::string src;
            std::int32_t id = -1;
        };

        /*!
        \brief Struct representing the Rect object in a SceneData file
        */
        struct Rect final
        {
            enum Axis
            {
                X,Y,Z
            };
            FloatRect rect;
            float depth = 0.f;
            Axis depthAxis = Z;
            std::int32_t id = -1;
        };

        /*!
        \brief Struct representing a Node object in a SceneData file
        */
        struct Node final
        {
            glm::quat orientation = glm::quat(1.f, 0.f, 0.f,0.f);
            glm::vec3 position = glm::vec3(0.f);
            glm::vec3 scale = glm::vec3(1.f);

            std::vector<Node> children;

            /*!
            \brief Lists the types and IDs of components attached to this node.
            Use it to look up the attachments with getComponent()
            //TODO why does type_index cause this to get mangled?
            */
            std::vector<std::pair<std::type_index, std::int32_t>> components;
        };

        /*!
        \brief Default Constructor
        */
        SceneData();

        /*!
        \brief Attempts to load the file at the given path
        \returns true if successful, else false.
        */
        bool loadFromFile(const std::string& path);

        /*!
        \brief Contains SceneData properties such as sky light colour/direction
        */
        struct Properties final
        {
            Colour skyColour = glm::vec4(1.f);
            glm::vec3 skyDirection = glm::vec3(0.f, -1.f, 0.f);
            std::string cubemap;
        };
        const Properties& getProperties() const { return m_properties; }


        /*!
        \brief Parses component information from a SceneData file
        */
        struct ComponentParser final
        {
            //!< A string containing the name of the object which holds all further objects for the new type, eg 'models'
            std::string componentName; 
            //!< A string containing the object name as it appears in the holding object in the SceneData file, eg 'model'
            std::string objectName; 
            /*!
            Parsing function which reads the given ConfigObject with a name of objectName and provides a destination for the data.
            Destination will be any_cast<std::vector<T>&>() where T is the type this parser is registered with (note the REFERENCE ;)
            */
            std::function<void(const cro::ConfigObject&, std::any&)> parseFunc; 
        };

        /*!
        \brief Registers a data struct for parsing component
        data from the SceneData
        \tparam T Typename for the data struct used to parse the component data
        \param parser An instance of a ComponentParser used to identify and parse
        component data for type T
        */
        template <typename T>
        void registerComponentType(const ComponentParser& parser)
        {
            CRO_ASSERT(!parser.componentName.empty(), "Must have a component name");
            CRO_ASSERT(!parser.objectName.empty(), "Must have and objectt name");
            CRO_ASSERT(parser.parseFunc, "Must have a parsing function");

            std::type_index index = typeid(T);

            CRO_ASSERT(m_parsers.count(index) == 0, "A parser for this type already exists");
            m_parsers.insert(std::make_pair(index, parser));
            m_components.insert(std::make_pair(index, std::make_any<std::vector<T>>()));
        }

        /*
        \brief Returns a vector of any parsed components of the given type
        if that type has been registered with registerComponentType()
        */
        template <typename T>
        const std::vector<T>& getComponentData() const
        {
            std::type_index index = typeid(T);

            CRO_ASSERT(m_components.count(index) != 0, "This type has not been registered");
            return std::any_cast<const std::vector<T>&>(m_components.at(index));
        }


        /*!
        \brief Returns a copy of the component data if it exists
        \param type Type index of the component read from a node's component list
        \param id ID of the component to retrieve
        */


    private:

        Properties m_properties;

        std::unordered_map<std::type_index, ComponentParser> m_parsers;
        std::unordered_map<std::type_index, std::any> m_components;
    };
}
