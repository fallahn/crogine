/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/ecs/Entity.hpp>

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

#include <array>

namespace cro
{
    /*!
    \brief PhysicsShapes are attached to PhysicsObjects to
    define their shape within the world.
    */
    struct PhysicsShape final
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
        glm::vec3 extent = glm::vec3(0.f);

        /*!
        \brief Optional offset relative to parent object.
        This is applied to shapes which are attached to a
        Physics Object which has more than one shape attached
        to it.
        */
        glm::vec3 position = glm::vec3(0.f);

        /*!
        \brief Optional rotation of shape.
        This is only applied to shapes which are atatched to an object
        with more than one Physics shape. The shape is rotated around 
        the PhysicsShape position
        */
        glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
    };
    
    /*!
    \brief Physics object components are used by both CollisionSystems
    and PhysicsSystems.
    Physics objects may be composed of up to 10 shapes defined by a 
    PhysicsShape struct. Units are, by default, in kilos and metres.
    */
    class PhysicsObject final
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

        /*!
        \brief Assigns the group ID of thes object;
        Used for collision filtering this should be a bitmask
        containing flags of up to 32 groups to which this object
        belongs. Other objects which contain a collision flag
        for one of the corresponding groups will collide with
        this object.
        Once the entity to which this component belongs is added
        to a Physics or Collision system changing this value
        has no effect.
        */
        void setCollisionGroups(std::int32_t groups) { m_collisionGroups = groups; }

        /*!
        \brief A bitmask of collision flags.
        Each flag in this mask indicates the group ID of objects
        with which this object will collide. EG a value of 5
        will cause the object to collide with objects in groups
        0x4 and 0x1.
        Once the entity to which this component belongs is added
        to a Physics or Collision system changing this value has
        no effect
        */
        void setCollisionFlags(std::int32_t flags) { m_collisionFlags = flags; }

        /*!
        \brief Returns the collision group mask for this component
        */
        std::int32_t getCollisionGroups() const { return m_collisionGroups; }

        /*!
        \brief Returns the number of objects colliding with this one.
        Check this first before attempting to read getCollisionIDs();
        */
        std::size_t getCollisionCount() const { return m_collisionCount; }

        /*!
        \brief Returns a reference to an array of entity IDs whose 
        PhysicsObjects are colliding with this one.
        Use getCollisionCount() to first check the number of collisions
        before attempting to read the array, as old data may exist.
        */
        const std::array<std::size_t, 100u>& getCollisionIDs() const { return m_collisionIDs; }

        //up to 4 points which make the overlapping area
        //of a collision manifold
        struct ManifoldPoint final
        {
            glm::vec3 worldPointA = glm::vec3(0.f);
            glm::vec3 worldPointB = glm::vec3(0.f);
            float distance = 0.f;
        };

        struct Manifold final
        {
            std::size_t pointCount = 0;
            std::array<ManifoldPoint, 4u> points;
        };

        /*!
        \brief Returns a reference to the collision manifold array.
        As with collision IDs the size of this should be first checked
        with getCollisionCount to avoid reading inaccurate data.
        */
        const std::array<Manifold, 100u>& getManifolds() const { return m_manifolds; }

    private:
        float m_mass;
        float m_density;

        std::int32_t m_collisionGroups;
        std::int32_t m_collisionFlags;

        std::array<PhysicsShape, 10u> m_shapes;
        std::size_t m_shapeCount;

        static constexpr std::size_t MaxCollisions = 100;
        std::array<std::size_t, MaxCollisions> m_collisionIDs = {};
        std::array<Manifold, MaxCollisions> m_manifolds = {};
        std::size_t m_collisionCount;
        
        friend class CollisionSystem;
    };
}