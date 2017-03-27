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

#include <crogine/graphics/Shader.hpp>

#include "../glad/glad.h"
#include "../glad/GLCheck.hpp"

using namespace cro;

Shader::Shader()
    : m_handle(0)
{

}

Shader::~Shader()
{
    if (m_handle)
    {
        glCheck(glDeleteProgram(m_handle));
    }
}

//public
bool Shader::loadFromFile(const std::string& vertex, const std::string& fragment)
{
    return loadFromString(parseFile(vertex), parseFile(fragment));
}

bool Shader::loadFromString(const std::string& vertex, const std::string& fragment)
{
    if (m_handle)
    {
        //remove existing program
        glCheck(glDeleteProgram(m_handle));
    }    
    
    //compile vert shader
    GLuint vertID = glCreateShader(GL_VERTEX_SHADER);
    
    auto srcPtr = vertex.c_str();
    glCheck(glShaderSource(vertID, 1, &srcPtr, nullptr));
    glCheck(glCompileShader(vertID));

    GLint result = GL_FALSE;
    int resultLength = 0;

    glCheck(glGetShaderiv(vertID, GL_COMPILE_STATUS, &result));
    glCheck(glGetShaderiv(vertID, GL_INFO_LOG_LENGTH, &resultLength));
    if (result == GL_FALSE)
    {
        //failed compilation
        std::string str;
        str.resize(resultLength + 1);
        glCheck(glGetShaderInfoLog(vertID, resultLength, nullptr, &str[0]));
        Logger::log("Failed compiling vertex shader: " + std::to_string(result) + ", " + str, Logger::Type::Error);

        glCheck(glDeleteShader(vertID));
        return false;
    }
    
    //compile frag shader
    GLuint fragID = glCreateShader(GL_FRAGMENT_SHADER);
    srcPtr = fragment.c_str();
    glCheck(glShaderSource(fragID, 1, &srcPtr, nullptr));
    glCheck(glCompileShader(fragID));

    result = GL_FALSE;
    resultLength = 0;

    glCheck(glGetShaderiv(fragID, GL_COMPILE_STATUS, &result));
    glCheck(glGetShaderiv(fragID, GL_INFO_LOG_LENGTH, &resultLength));
    if (result == GL_FALSE)
    {
        std::string str;
        str.resize(resultLength + 1);
        glCheck(glGetShaderInfoLog(fragID, resultLength, nullptr, &str[0]));
        Logger::log("Failed compiling fragment shader: " + std::to_string(result) + ", " + str, Logger::Type::Error);

        glCheck(glDeleteShader(vertID));
        glCheck(glDeleteShader(fragID));
        return false;
    }

    //link shaders to program
    m_handle = glCreateProgram();
    if (m_handle)
    {
        glCheck(glAttachShader(m_handle, vertID));
        glCheck(glAttachShader(m_handle, fragID));
        glCheck(glLinkProgram(m_handle));

        result = GL_FALSE;
        resultLength = 0;
        glCheck(glGetProgramiv(m_handle, GL_LINK_STATUS, &result));
        glCheck(glGetProgramiv(m_handle, GL_INFO_LOG_LENGTH, &resultLength));
        if (result == GL_FALSE)
        {
            std::string str;
            str.resize(resultLength + 1);
            glCheck(glGetProgramInfoLog(m_handle, resultLength, nullptr, &str[0]));
            Logger::log("Failed to link shader program: " + std::to_string(result) + ", " + str, Logger::Type::Error);

            glCheck(glDetachShader(m_handle, vertID));
            glCheck(glDetachShader(m_handle, fragID));

            glCheck(glDeleteShader(vertID));
            glCheck(glDeleteShader(fragID));
            glCheck(glDeleteProgram(m_handle));
            m_handle = 0;
            return false;
        }
        else
        {
            //tidy
            glCheck(glDetachShader(m_handle, vertID));
            glCheck(glDetachShader(m_handle, fragID));

            glCheck(glDeleteShader(vertID));
            glCheck(glDeleteShader(fragID));
            return true;
        }
    }

    return false;
}

uint32 Shader::getGLHandle() const
{
    return m_handle;
}

//private
std::string Shader::parseFile(const std::string& file)
{
    std::string retVal;
    retVal.reserve(1000);

    //open file and verify

    //read line by line - 
    //if line starts with #include increase inclusion depth
    //if depth under limit append parseFile(include)
    //else append line

    //close file

    return {};
}