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

#include <string>

class VatFile final
{
public:
    VatFile();

    bool loadFromFile(const std::string&);

    const std::string& getModelPath() const { return m_modelPath; }
    const std::string& getPositionPath() const { return m_positionPath; }
    const std::string& getNormalPath() const { return m_normalPath; }
    const std::string& getTangentPath() const { return m_tangentPath; }

    bool hasTangents() const { return !m_tangentPath.empty(); }

private:

    float m_frameRate;
    std::int32_t m_frameCount;
    std::int32_t m_frameLoop;

    std::string m_modelPath;
    std::string m_positionPath;
    std::string m_normalPath;
    std::string m_tangentPath;

    void reset();

    friend struct VatAnimation;
};