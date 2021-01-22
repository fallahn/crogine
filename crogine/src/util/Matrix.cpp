//Modified from the glm source for decomposing matrices.
//see glm/gtx/matrix_decompose.inl

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>
#include <crogine/detail/glm/gtc/constants.hpp>
#include <crogine/detail/glm/gtc/epsilon.hpp>

#include <crogine/util/Matrix.hpp>

namespace cro
{
	namespace Detail
	{
		glm::vec3 combine(const glm::vec3& a, const glm::vec3& b, float aScale, float bScale)
		{
			return (a * aScale) + (b * bScale);
		}

		glm::vec3 scale(const glm::vec3& v, float length)
		{
			return  v * length / glm::length(v);
		}
	}

	namespace Util:: Matrix
	{
		bool decompose(const glm::mat4& matrix, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale)
		{
			glm::mat4 localMatrix(matrix);

			//normalize the matrix.
			if (glm::epsilonEqual(localMatrix[3][3], 0.f, glm::epsilon<float>()))
			{
				return false;
			}

			for (auto i = 0; i < 4; ++i)
			{
				for (auto j = 0; j < 4; ++j)
				{
					localMatrix[i][j] /= localMatrix[3][3];
				}
			}

			// Next take care of translation (easy).
			translation = localMatrix[3];
			localMatrix[3] = glm::vec4(0, 0, 0, localMatrix[3].w);

			glm::vec3 Row[3], Pdum3;

			// Now get scale and shear.
			for (auto i = 0; i < 3; ++i)
			{
				for (auto j = 0; j < 3; ++j)
				{
					Row[i][j] = localMatrix[i][j];
				}
			}

			// Compute X scale factor and normalize first row.
			scale.x = length(Row[0]);// v3Length(Row[0]);

			Row[0] = Detail::scale(Row[0], 1.f);

			// Compute XY shear factor and make 2nd row orthogonal to 1st.
			//Skew.z = dot(Row[0], Row[1]);
			//Row[1] = Detail::combine(Row[1], Row[0], static_cast<float>(1), -Skew.z);

			// Now, compute Y scale and normalize 2nd row.
			scale.y = length(Row[1]);
			//Row[1] = Detail::scale(Row[1], static_cast<float>(1));
			//Skew.z /= scale.y;

			// Compute XZ and YZ shears, orthogonalize 3rd row.
			/*Skew.y = glm::dot(Row[0], Row[2]);
			Row[2] = Detail::combine(Row[2], Row[0], static_cast<float>(1), -Skew.y);
			Skew.x = glm::dot(Row[1], Row[2]);
			Row[2] = Detail::combine(Row[2], Row[1], static_cast<float>(1), -Skew.x);*/

			// Next, get Z scale and normalize 3rd row.
			scale.z = length(Row[2]);
			//Row[2] = Detail::scale(Row[2], static_cast<float>(1));
			//Skew.y /= scale.z;
			//Skew.x /= scale.z;

			// At this point, the matrix (in rows[]) is orthonormal.
			// Check for a coordinate system flip.  If the determinant
			// is -1, then negate the matrix and the scaling factors.
			Pdum3 = cross(Row[1], Row[2]); // v3Cross(row[1], row[2], Pdum3);
			if (dot(Row[0], Pdum3) < 0)
			{
				for (auto i = 0; i < 3; i++)
				{
					scale[i] *= -1.f;
					Row[i] *= -1.f;
				}
			}


			//rotation
			int i, j, k = 0;
			float root, trace = Row[0].x + Row[1].y + Row[2].z;
			if (trace > 0)
			{
				root = glm::sqrt(trace + 1.f);
				rotation.w = 0.5f * root;
				root = 0.5f / root;
				rotation.x = root * (Row[1].z - Row[2].y);
				rotation.y = root * (Row[2].x - Row[0].z);
				rotation.z = root * (Row[0].y - Row[1].x);
			} // End if > 0
			else
			{
				static int Next[3] = { 1, 2, 0 };
				i = 0;
				if (Row[1].y > Row[0].x) i = 1;
				if (Row[2].z > Row[i][i]) i = 2;
				j = Next[i];
				k = Next[j];

				root = glm::sqrt(Row[i][i] - Row[j][j] - Row[k][k] + 1.f);

				rotation[i] = 0.5f * root;
				root = 0.5f / root;
				rotation[j] = root * (Row[i][j] + Row[j][i]);
				rotation[k] = root * (Row[i][k] + Row[k][i]);
				rotation.w = root * (Row[j][k] - Row[k][j]);
			} // End if <= 0

			rotation = glm::conjugate(rotation);
			return true;
		}
	}
}
