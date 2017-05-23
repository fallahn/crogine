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

#include <crogine/graphics/IQMBuilder.hpp>
#include "../detail/GLCheck.hpp"

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstring>

using namespace cro;

namespace
{
    namespace Iqm
    {
#include "Iqm.inl"
    }
}

void loadVertexData(const Iqm::Header& header, char* data, const std::string& strings, cro::Mesh::Data& out);
void loadAnimationData(const Iqm::Header& header, char* data, const std::string& strings, cro::Mesh::Data& out);

IqmBuilder::IqmBuilder(const std::string& path)
    : m_path    (path),
    m_file      (nullptr)
{

}

IqmBuilder::~IqmBuilder()
{
    if (m_file)
    {
        SDL_RWclose(m_file);
    }
}

//private
cro::Mesh::Data IqmBuilder::build() const
{
    cro::Mesh::Data returnData;
    returnData.primitiveType = GL_TRIANGLES;
    
    m_file = SDL_RWFromFile(m_path.c_str(), "rb");
    if (m_file)
    {
        //do some file checks
        auto fileSize = SDL_RWsize(m_file);
        if (fileSize < sizeof(Iqm::Header))
        {
            Logger::log(m_path + ": Invalid file size", Logger::Type::Error);
            SDL_RWclose(m_file);
            m_file = nullptr;
            return {};
        }

        std::vector<char> fileData(fileSize);
        auto readCount = SDL_RWread(m_file, fileData.data(), fileSize, 1);
        SDL_RWclose(m_file);
        m_file = nullptr;

        if (readCount == 0)
        {
            Logger::log(m_path + ": failed to read data.", Logger::Type::Error);
            return {};
        }
        
        //start parsing - begin by checking header
        char* filePtr = fileData.data();
        Iqm::Header header;
        Iqm::readHeader(filePtr, header);

        if (std::string(header.magic) != Iqm::IQM_MAGIC)
        {
            Logger::log("Not an IQM file, header returned " + std::string(header.magic) + " instead of " + Iqm::IQM_MAGIC, Logger::Type::Error);
            return {};
        }

        if (header.version != Iqm::IQM_VERSION)
        {
            Logger::log("Wrong IQM version, found " + std::to_string(header.version) + " expected " + std::to_string(Iqm::IQM_VERSION), Logger::Type::Error);
            return {};
        }

        if (header.filesize > (16 << 20))
        {
            Logger::log("IQM file greater than 16mb, file not loaded", Logger::Type::Error);
            return {};
        }

        //read string data
        std::string strings;
        strings.resize(header.textCount);
        std::memcpy(&strings[0], fileData.data() + header.textOffset, header.textCount);

        //check for vertex data
        if (header.vertexCount > 0)
        {
            loadVertexData(header, fileData.data(), strings, returnData);
        }
        else
        {
            Logger::log("No vertex data was found in " + m_path + ", this file may contain only animation data", Logger::Type::Warning);
        }
        loadAnimationData(header, fileData.data(), strings, returnData);
    }
    else
    {
        Logger::log("Failed opening " + m_path, Logger::Type::Error);
        return {};
    }
    return returnData;
}

void loadVertexData(const Iqm::Header& header, char* data, const std::string& strings, cro::Mesh::Data& out)
{
    const auto dataStart = data;

    data += header.varrayOffset;
    //iqm files keep each attribute in their own array
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> tangents; //tans are 4 component - the w contains the sign of the cross product for the bitan
    std::vector<float> texCoords;
    std::vector<uint8> blendIndices;
    std::vector<uint8> blendWeights;
    std::vector<uint8> colours;


    for (auto i = 0u; i < header.varrayCount; ++i)
    {
        Iqm::VertexArray vertArray;
        data = Iqm::readVertexArray(data, vertArray);

        switch (vertArray.type)
        {
        default: break;
        case Iqm::POSITION:
            positions.resize(header.vertexCount * Iqm::positionSize);
            std::memcpy(positions.data(), dataStart + vertArray.offset, sizeof(float) * positions.size());
            out.attributes[Mesh::Position] = Iqm::positionSize;
            break;
        case Iqm::NORMAL:
            normals.resize(header.vertexCount * Iqm::normalSize);
            std::memcpy(normals.data(), dataStart + vertArray.offset, sizeof(float) * normals.size());
            out.attributes[Mesh::Normal] = Iqm::normalSize;
            break;
        case Iqm::TANGENT:
            tangents.resize(header.vertexCount * (Iqm::normalSize + 1));
            std::memcpy(tangents.data(), dataStart + vertArray.offset, sizeof(float) * tangents.size());
            out.attributes[Mesh::Tangent] = Iqm::normalSize;
            break;
        case Iqm::TEXCOORD:
            texCoords.resize(header.vertexCount * Iqm::uvSize);
            std::memcpy(texCoords.data(), dataStart + vertArray.offset, sizeof(float) * texCoords.size());
            out.attributes[Mesh::UV0] = Iqm::uvSize;
            break;
        case Iqm::BLENDINDICES:
            blendIndices.resize(header.vertexCount * Iqm::blendIndexSize);
            std::memcpy(blendIndices.data(), dataStart + vertArray.offset, blendIndices.size());
            out.attributes[Mesh::BlendIndices] = Iqm::blendIndexSize;
            break;
        case Iqm::BLENDWEIGHTS:
            blendWeights.resize(header.vertexCount * Iqm::blendWeightSize);
            std::memcpy(blendWeights.data(), dataStart + vertArray.offset, blendWeights.size());
            out.attributes[Mesh::BlendWeights] = Iqm::blendWeightSize;
            break;
        case Iqm::COLOUR:
            colours.resize(header.vertexCount * Iqm::colourSize);
            std::memcpy(colours.data(), dataStart + vertArray.offset, colours.size());
            out.attributes[Mesh::Colour] = Iqm::colourSize;
            break;
        }

        //parse the index arrays. Do this first in case
        //we decide to calculate our own tangent data
        //which will be interleaved in  the VBO
        std::vector<Iqm::Triangle> triangles;
        bool buildTangents = tangents.empty();
        char* meshData = dataStart + header.meshOffset;
        for (auto i = 0u; i < header.meshCount; ++i)
        {
            Iqm::Mesh mesh;
            meshData = Iqm::readMesh(meshData, mesh);

            //TODO optionally read material data from string

            Iqm::Triangle triangle;
            char* triangleData = dataStart + header.triangleOffset + (mesh.firstTriangle * sizeof(triangle.vertex));
            std::vector<uint16> indices;

            for (auto j = 0; j < mesh.triangleCount; ++j)
            {
                std::memcpy(triangle.vertex, triangleData, sizeof(triangle.vertex));
                triangleData += sizeof(triangle.vertex);
                //IQM triangles are wound clockwise - this is a kludge to make things draw but it makes models appear 'inside out'!
                indices.push_back(triangle.vertex[2]);
                indices.push_back(triangle.vertex[1]);
                indices.push_back(triangle.vertex[0]);

                //cache the triangle if we need to build tangents
                if (buildTangents)
                {
                    triangles.push_back(triangle);
                }
            }

            out.indexData[i].format = GL_UNSIGNED_INT;
            out.indexData[i].primitiveType = out.primitiveType;
            out.indexData[i].indexCount = static_cast<uint32>(indices.size());

            glCheck(glGenBuffers(1, &out.indexData[i].ibo));
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.indexData[i].ibo));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, out.indexData[i].indexCount * sizeof(uint16), indices.data(), GL_STATIC_DRAW));
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
        }


        //calc bitangents
        std::vector<float> pureTangents; //4th component removed
        std::vector<float> bitangents;
        if (!buildTangents)
        {
            //use cross product of model data
        }
        else
        {
            //calc from UV coords and face normals
        }


        //interleave data and create VBO
        //also measure bounds if bounds data missing
    }

    //use bounds data from file if available
    if (header.boundsOffset > 0)
    {

    }
    else
    {

    }
}

void loadAnimationData(const Iqm::Header& header, char* data, const std::string& strings, cro::Mesh::Data& out)
{
    //TODO - this.
    //TODO warn if bone count > 64 as this is the limit on mobile devices (or even lower!)
}