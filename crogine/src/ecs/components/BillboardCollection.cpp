/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2026
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

#include <crogine/ecs/components/BillboardCollection.hpp>

using namespace cro;

BillboardCollection::BillboardCollection()
    : m_chunkSize       (0),
    m_dirty             (false),
    m_chunkCount        (0u),
    m_chunkCoordStart   (0.f)
{
    m_drawLists.reserve(10);
    m_cameraCallbacks.reserve(10);
}

//public
void BillboardCollection::addBillboard(Billboard billboard)
{
    m_billboards.push_back(billboard);
    m_dirty = true;
}

void BillboardCollection::setBillboards(std::vector<Billboard>& billboards)
{
    m_billboards.swap(billboards);
    m_dirty = true;
}

void BillboardCollection::setChunkSize(std::uint32_t size)
{
    m_chunkSize = std::clamp(size, 0u, 100u);
    m_dirty = true;
}

void BillboardCollection::setDrawlistCallback(Entity camera, const std::function<void(Entity, Entity)>& callback)
{
    m_pendingCallbacks.emplace_back(camera, callback);
}

//private