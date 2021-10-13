/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <crogine/core/Log.hpp>
#include <crogine/util/Network.hpp>

#ifdef _WIN32
#include <iphlpapi.h>

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdio.h>
#endif

#ifdef _WIN32
std::vector<std::string> cro::Util::Net::getLocalAddresses()
{
    std::vector<std::string> retVal;

    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = nullptr;

    UINT i = 0;

    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    pAdapterInfo = (IP_ADAPTER_INFO*)MALLOC(sizeof(IP_ADAPTER_INFO));

    if (pAdapterInfo == nullptr)
    {
        LogE << "Error allocating memory for adapter info" << std::endl;
        return retVal;
    }

    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) 
    {
        FREE(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)MALLOC(ulOutBufLen);
        if (pAdapterInfo == nullptr) 
        {
            LogE << "Error allocating memory needed to call GetAdaptersinfo" << std::endl;
            return retVal;
        }
    }

    DWORD dwRetVal = 0;
    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR)
    {
        pAdapter = pAdapterInfo;
        while (pAdapter)
        {
            if (pAdapter->Type == MIB_IF_TYPE_ETHERNET)
            {
                retVal.emplace_back(pAdapter->IpAddressList.IpAddress.String);
                IP_ADDR_STRING* add = pAdapter->IpAddressList.Next;
                while (add)
                {
                    retVal.emplace_back(add->IpAddress.String);
                    add = add->Next;
                }
            }
            pAdapter = pAdapter->Next;
        }
    }

    if (pAdapterInfo)
    {
        FREE(pAdapterInfo);
    }

    return retVal;
}
#else
std::vector<std::string> cro::Util::Net::getLocalAddresses()
{
    std::vector<std::string> retVal;

    struct ifaddrs* ifap, * ifa;
    struct sockaddr_in* sa;
    char* addr = nullptr;

    getifaddrs(&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr
            && ifa->ifa_addr->sa_family == AF_INET)
        {
            sa = (struct sockaddr_in*)ifa->ifa_addr;
            addr = inet_ntoa(sa->sin_addr);
            retVal.emplace_back(addr);
        }
    }

    freeifaddrs(ifap);

    return retVal;
}
#endif