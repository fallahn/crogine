#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>
#include <vector>

namespace cro::Util::Maths
{
    /*
    Generate a Catmull-Rom spline from a given set of 3D points
    //based on https://www.codeproject.com/Articles/30838/Overhauser-Catmull-Rom-Splines-for-Camera-Animatio
    */
    class CRO_EXPORT_API Spline final
    {
    public:
        /*!
        * \brief Constructor
        */
        Spline();

        /*!
        \brief Add a point to the spline
        \param Point in 3D space
        */
        void addPoint(const glm::vec3& point);

        /*!
        \brief Returns a 3D point on the spline at the given
        normalised coordinate.
        \param t Value between 0 and 1, representing the normalised
        position along the spline.
        \returns 3D point on the spline
        */
        glm::vec3 getInterpolatedPoint(float t) const;

        /*!
        \brief Returns an approximated orientation at the
        given point along the curve
        */
        glm::quat getInterpolatedOrientation(float t) const;

        /*!
        \brief Returns the number of points added to the spline
        */
        std::size_t getPointCount() const;

        /*!
        \brief Returns the control point at the given index
        */
        glm::vec3 getPointAt(std::size_t idx) const;

        /*!
        \brief Returns the current number of control points
        */
        std::size_t getSize() const { return m_points.size(); }

        /*!
        \brief Returns an APPROXIMATE length of the spline
        created by taking multiple samples along the path
        and measuring the distance between each.
        */
        float getLength() const;

    private:
        std::vector<glm::vec3> m_points;
        float m_dt;

        mutable bool m_dirtyLength;
        mutable float m_length;

        //method for computing the Catmull-Rom parametric equation
        //given a time (t) and a vector quadruple (p1,p2,p3,p4).
        glm::vec3 eq(float t, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& p4) const;
    };
}