/*-----------------------------------------------------------------------

Matt Marchant 2020
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

/*
Much of this is derived from http://www.mralligator.com/q3/

However some data regarding the Face struct is incorrect (or I misinterpreted it)
and corrections are noted by the struct definition, below.

*/

#include <cstdint>

namespace Q3
{
    struct Vec2f final
    {
        float x = 0.f;
        float y = 0.f;
    };

    struct Vec3f final
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;

        void operator /= (float r)
        {
            x /= r;
            y /= r;
            z /= r;
        }

        Vec3f operator + (Vec3f r) const
        {
            Vec3f retVal;
            retVal.x = x + r.x;
            retVal.y = y + r.y;
            retVal.z = z + r.z;

            return retVal;
        }
    };

    struct Vec3i final
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
    };

    struct Header final
    {
        //should be IBSP
        char id[4];
        std::int32_t version = 0; //46 for Q3
    };


    struct Lump final
    {
        std::int32_t offset = 0;
        std::int32_t length = 0;
    };


    struct Vertex final
    {
        Vec3f position;
        Vec2f uv0;
        Vec2f uv1; //shadow map coord
        Vec3f normal;
        std::uint8_t colour[4]; //RGBA

        //operators are used when tesselating patches
        //TODO how do we set the normal/colour of the return value?
        Vertex operator + (const Vertex& r) const
        {
            Vertex retVal;
            retVal.position = position + r.position;
            retVal.normal = normal + r.normal;
            retVal.normal /= 2.f;

            retVal.uv0.x = uv0.x + r.uv0.x;
            retVal.uv0.y = uv0.y + r.uv0.y;

            retVal.uv1.x = uv1.x + r.uv1.x;
            retVal.uv1.y = uv1.y + r.uv1.y;

            return retVal;
        }

        Vertex operator * (float r) const
        {
            Vertex retVal;
            retVal.position.x = position.x * r;
            retVal.position.y = position.y * r;
            retVal.position.z = position.z * r;

            retVal.uv0.x = uv0.x * r;
            retVal.uv0.y = uv0.y * r;

            retVal.uv1.x = uv1.x * r;
            retVal.uv1.y = uv1.y * r;

            return retVal;
        }
    };

    enum FaceType
    {
        Poly = 1,
        Patch,
        Mesh,
        Billboard
    };

    /*
    Some of the specs for this outlined here are wrong: http://www.mralligator.com/q3/
    See the comments below for correction.

    To correctly calculate the indices for a face do something like 

    for (auto i = 0; i < face.meshVertCount; ++i)
    {
        indexData.push_back(face.firstVertIndex + m_indices[face.firstMeshIndex + i]);
    }

    */

    struct Face final
    {
        std::int32_t materialID = 0;         //index in the material lookup array
        std::int32_t effect = 0;             //index for effect, -1 for no effect
        std::int32_t type = 0;               //1=poly, 2=patch, 3=mesh, 4=billboard
        std::int32_t firstVertIndex = 0;     //index to the first vert in this face - directly indexes the vertex array, for both types 1 & 3
        std::int32_t vertexCount = 0;        //number of vertices in the face - not necessarily the same as number of indices as vertices are shared
        std::int32_t firstMeshIndex = 0;     //start index in the indices array for the face if it is type 1 or 3, the value of which is ADDED to firstVertIndex
        std::int32_t meshIndexCount = 0;     //number of indices in this face if it is type 1 or 3
        std::int32_t lightmapID = 0;         //index in the texture array for the light map texture
        std::int32_t lightmapCorner[2];      //face's lightmap corner
        std::int32_t lightmapSize[2];        //lightmap dimensions
        Vec3f lightmapPosition;              //origin of lightmap
        Vec3f lightmapVectors[2];            //coordinates of lightmap S and T vectors
        Vec3f normal;                        //face normal
        std::int32_t size[2];                //bezier patch dimensions
    };

    //referred to as a texture in the docs but we'll actually be loading materials
    struct Texture final
    {
        std::int8_t name[64];
        std::int32_t surfaceFlags;
        std::int32_t contentFlags;
    };

    enum Lumps //lump list, specific to BSP type
    {
        Entities,
        Textures,
        Planes,
        Nodes,
        Leaves,
        LeafFaces,
        LeafBrushes,
        Models,
        Brushes,
        BrushSides,
        Vertices,
        Indices,
        Shaders,
        Faces,
        Lightmaps,
        LightVolumes,
        VisData,

        MaxLumps
    };

    struct Lightmap final
    {
        char imageBits[128][128][3];   // The RGB data in a 128x128 image
    };

    struct Node final
    {
        std::int32_t planeIndex = 0; //index in plane array
        std::int32_t frontIndex = 0; //child index for front node
        std::int32_t rearIndex = 0;  //child index for rear node;
        Vec3i bbMin;                 //bounding box minimum position
        Vec3i bbMax;                 //bounding box maximum position
    };

    struct Leaf final
    {
        std::int32_t cluster = 0;    //vis cluster to which the leaf belongs
        std::int32_t area = 0;       //area portal to which leaf belongs
        Vec3i bbMin;                 //bounding box minimum position
        Vec3i bbMax;                 //bounding box maximum position
        std::int32_t firstFace = 0;  //index of the first face in the face array
        std::int32_t faceCount = 0;  //number of faces in this leaf
        std::int32_t firstBrush = 0; //first brush in the brush array
        std::int32_t brushCount = 0; //number of brushes in this leaf
    };

    struct Plane final
    {
        Vec3f normal;
        float distance = 0.f;
    };

    struct VisiData final
    {
        std::int32_t clusterCount = 0;
        std::int32_t bytesPerCluster = 0;
        std::int8_t* bitsetArray = nullptr;
    };
}