/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "../golf/ChunkVisSystem.hpp"
#include "../golf/CollisionMesh.hpp"

#include <array>
#include <string>
#include <future>

//offline processing for generating chunked positions for grass

/*
File format: 
int[64] Sizes - size of each chunk array to read, in bytes, starting after header
glm::mat4[Sizes[i]] - raw matrix data to load into instances, little endian
//TODO - do we want to pre-process the normal mats too? Need to perf test
*/

class GrassProcessor final
{
public:
    using Header = std::array<std::int32_t, 64>;
    
    GrassProcessor();

    //launches processing
    void begin(const std::string& path);

    //wait for threads to finish and then writes file
    //returns true if job is complete
    bool process();

private:
    std::int32_t m_processTotal;
    std::array<std::future<std::vector<glm::mat4>>, ChunkVisSystem::ChunkCount> m_processResults = {};

    std::array<std::vector<glm::mat4>, ChunkVisSystem::ChunkCount> m_transformData = {};

    std::size_t m_currentPath;
    std::vector<std::string> m_modelPaths;

    CollisionMesh m_collisionMesh;

    void queueJob();
};