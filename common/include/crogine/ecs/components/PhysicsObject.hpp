/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef CRO_PHYSICS_OBJECT_HPP_
#define CRO_PHYSICS_OBJECT_HPP_

#include <crogine/Config.hpp>

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>

namespace cro
{
    /*!
    \brief PhysicsShapes are attached to PhysicsObjects to
    define their shape within the world.
    */
    struct CRO_EXPORT_API PhysicsShape final
    {
        /*
        \brief The type of the shape
        */
        enum class Type
        {
            Capsule, Cone, Cylinder,
            Box, Hull, Sphere, Compound, Null
        }type = Type::Null;

        /*!
        \brief Applies to Capsule, Cone and Sphere shapes
        */
        float radius = 0.f;

        /*!
        \brief Applies to Capsule and Cone Shapes.
        For capsule shapes this is the distance between the centre
        points of the two radial caps.
        */
        float length = 0.f;

        /*!
        brief The orientation of Capsule, Cone and Cylinder shapes.
        */
        enum class Orientation
        {
            X, Y, Z
        }orientation = Orientation::Y;
        
        /*!
        \brief The half extent of Box and Cylinder shapes.
        ie This is the distance from the centre of the box
        to the first corner, or half the length of a Cylinder
        and its radius
        */
        glm::vec3 extent;

        /*!
        \brief Optional offset relative to parent object.
        This is applied to shapes which are attached to a
        Physics Object which has more than one shape attached
        to it.
        */
        glm::vec3 position;

        /*!
        \brief Optional rotation of shape.
        This is only applied to shapes which are atatched to an object
        with more than one Physics shape. The shape is rotated around 
        the PhysicsShape position
        */
        glm::quat rotation;
    };
    
    /*!
    \brief Physics object components are used by both CollisionSystems
    and PhysicsSystems.
    Physics objects may be composed of up to 10 shapes defined by a 
    PhysicsShape struct. Units are, by default, in kilos and metres.
    */
    class CRO_EXPORT_API PhysicsObject final
    {
    public:
        /*!
        \brief Constructor.
        \param mass The mass of the object. This only applicable
        to objects added to a scene containing a PhysicsSystem
        \param density The object's density. As with mass values,
        density only affects PhysicsObjects which are added to a 
        scene with a PhysicsSystem
        */
        PhysicsObject(float mass = 0.f, float density = 0.f);

        /*!
        \brief Adds a shape to the PhysicsObject.
        Up to 10 shapes can be added to a single object.
        \returns true if successful, else false if the maximum
        number of shapes has already been reached.
        */
        bool addShape(const PhysicsShape&);

    private:
        float m_mass;
        float m_density;
        std::array<PhysicsShape, 10> m_shapes;
        std::size_t m_shapeCount;

        friend class CollisionSystem;
    };
}

#endif //CRO_PHYSICS_OBJECT_HPP_