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

#include "DistanceField.hpp"

#include <cmath>
#include <climits>
#include <cstring>
#include <algorithm>

using namespace cro;
using namespace cro::Detail;

namespace
{
    const float INF = 1e20;
    inline int32 square(int32 x)
    {
        return x * x;
    }
}

std::vector<uint8> DistanceField::toDF(const SDL_Surface* input)
{
    std::size_t size = input->pitch * input->h;
    std::vector<float> floatData(size);
    uint8* pixels = static_cast<uint8*>(input->pixels);
    for (auto i = 0u; i < size; ++i)
    {
        floatData[i] = (pixels[i] > 240) ? 0.f : INF;
    }

    twoD(floatData, input->pitch, input->h);

    for (auto& f : floatData)
    {
        f = std::sqrt(f);
    }

    return toBytes(floatData);
}

//private
void DistanceField::twoD(std::vector<float>& floatData, int32 width, int32 height)
{
    std::vector<float> f(std::max(width, height));

    for (auto x = 0; x < width; x++) 
    {
        for (auto y = 0; y < height; y++)
        {
            f[y] = floatData[y * width + x];
        }
        auto d = oneD(f, height);

        for (auto y = 0; y < height; y++)
        {
            floatData[y * width + x] = d[y];
        }
    }

    for (auto y = 0; y < height; y++)
    {
        for (auto x = 0; x < width; x++)
        {
            f[x] = floatData[y * width + x];
        }
        auto d = oneD(f, width);

        for (auto x = 0; x < width; x++)
        {
            floatData[y * width + x] = d[x];
        }
    }
}

std::vector<float> DistanceField::oneD(const std::vector<float>& floatData, std::size_t size)
{
    std::vector<float> d(size);
    std::vector<int32> v(size);
    std::vector<float> z(size + 1);

    int32 k = 0;
    v[0] = 0;
    z[0] = -INF;
    z[1] = INF;

    for (auto q = 1u; q <= size - 1; q++)
    {
        float s = ((floatData[q] + square(q)) - (floatData[v[k]] + square(v[k]))) / (2 * q - 2 * v[k]);
        while (s <= z[k]) 
        {
            k--;
            s = ((floatData[q] + square(q)) - (floatData[v[k]] + square(v[k]))) / (2 * q - 2 * v[k]);
        }
        k++;
        v[k] = q;
        z[k] = s;
        z[k + 1] = +INF;
    }

    k = 0;
    for (auto q = 0u; q <= size - 1; q++) 
    {
        while (z[k + 1] < q)
            k++;
        d[q] = square(q - v[k]) + floatData[v[k]];
    }

    return d;
}

std::vector<uint8> DistanceField::toBytes(const std::vector<float>& floatData)
{
    float min, max;
    min = max = floatData[0];
    for (auto f : floatData)
    {
        if (f < min) min = f;
        if (f > max) max = f;
    }

    std::vector<uint8> retVal(floatData.size());
    std::memset(retVal.data(), 0, retVal.size());
    if (min == max)
    {
        return retVal;
    }

    float scale = UCHAR_MAX / (max - min);
    for (auto i = 0u; i < retVal.size(); ++i)
    {
        uint8 val = static_cast<uint8>(255.f - ((floatData[i] - min) * scale));
        retVal[i] = val;
    }

    return retVal;
}
