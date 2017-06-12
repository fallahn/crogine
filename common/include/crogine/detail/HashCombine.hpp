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

#ifndef CRO_HASH_COMBINE_HPP_
#define CRO_HASH_COMBINE_HPP_

//as seen on https://stackoverflow.com/a/19195373/6740859

#include <crogine/Config.hpp>

namespace cro
{
    template <typename T>
    inline void hash_combine(std::size_t& s, const T& v)
    {
        static_assert(std::is_pod<T>::value, "Not a POD");
        std::hash<T> h;
        s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
    }

    //allows hashing of arbitrary POD structs
    //via template specialisation
    template <class T>
    struct StructHash;
}

#endif //CRO_HASH_COMBINE_HPP_