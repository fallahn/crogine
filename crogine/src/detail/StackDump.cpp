/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2024
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


#ifdef _MSC_VER
#include <Windows.h>
#endif

#include "ust.hpp"

#include <crogine/detail/StackDump.hpp>

#include <iostream>
#include <ctime>

using namespace cro;

void StackDump::dump(int type)
{
    //hmm apparently we can get a few hundred reports so..
    static int reportCount = 0;

    if (reportCount)
    {
        return;
    }

//#ifdef _MSC_VER
    auto t = std::time(nullptr);
    auto* tm = std::localtime(&t);

    std::string str = std::to_string(tm->tm_year + 1900);
    str += "-" + std::to_string(tm->tm_mon + 1);
    str += "-" + std::to_string(tm->tm_mday);
    str += "-" + std::to_string(tm->tm_hour + 1);
    str += "-" + std::to_string(tm->tm_min);
    str += "-" + std::to_string(tm->tm_sec);
    //auto str = std::to_string(t);

    std::ofstream file("stack_dump_" + str + ".txt");
    switch (type)
    {
    case StackDump::Generic:
    default: break;
    case StackDump::ABRT:
        reportCount++;
        file << "ABORT\n";
        break;
    case StackDump::FPE:
        reportCount++;
        file << "Floating Point Exception\n";
        break;
    case StackDump::ILL:
        reportCount++;
        file << "Illegal op\n";
        break;
    case StackDump::SEG:
        reportCount++;
        file << "Segment Fault\n";
        break;
    }
    file << "Call Stack:\n" << ust::generate() << std::endl;
//#endif
}