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
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

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

void BillboardCollection::setDrawListCallback(Entity camera, const std::function<void(Entity, Entity)>& callback)
{
    m_pendingCallbacks.emplace_back(camera, callback);
}

//private


//-----default camera callback
DefaultBillboardCallback::DefaultBillboardCallback(float minRad, float maxRad)
    : m_minDist2(std::min(minRad* minRad, maxRad* maxRad)),
    m_maxDist2(std::max(minRad* minRad, maxRad* maxRad))
{

}

void DefaultBillboardCallback::operator()(Entity camera, Entity parent)
{
    //put the camera transform into vertex local space
    const auto inverseTx = glm::inverse(parent.getComponent<cro::Transform>().getWorldTransform());
    const auto camTx = inverseTx * camera.getComponent<cro::Transform>().getWorldTransform();

    //calc AABB of frustum - use this to only test the chunks which fall into its grid area
    glm::vec3 minPos(std::numeric_limits<float>::max());
    glm::vec3 maxPos(std::numeric_limits<float>::lowest());
    const auto& corners = camera.getComponent<Camera>().getFrustumCorners();

    for (const auto& c : corners)
    {
        const auto p = camTx * c;
        minPos.x = std::min(minPos.x, p.x);
        minPos.y = std::min(minPos.y, p.y);
        minPos.z = std::min(minPos.z, p.z);

        maxPos.x = std::max(maxPos.x, p.x);
        maxPos.y = std::max(maxPos.y, p.y);
        maxPos.z = std::max(maxPos.z, p.z);
    }

    auto& bbc = parent.getComponent<cro::BillboardCollection>();
    const auto BoundsPadding = glm::vec3(bbc.getChunkSize());
    minPos -= BoundsPadding;
    maxPos += BoundsPadding;

    const auto coordStart = bbc.getChunkCoordStart();
    const auto relPos = glm::vec2(minPos.x, minPos.z) - coordStart;
    const auto xPos = static_cast<std::int32_t>(std::floor(relPos.x / bbc.getChunkSize()));
    const auto yPos = static_cast<std::int32_t>(std::floor(relPos.y / bbc.getChunkSize()));

    const auto GridSizeX = static_cast<std::int32_t>(std::floor((maxPos.x - minPos.x) / bbc.getChunkSize()));
    const auto startX = std::max(0, xPos);
    const auto endX = std::min(bbc.getChunkCount().x, std::uint32_t(xPos + GridSizeX));

    const auto GridSizeY = static_cast<std::int32_t>(std::floor((maxPos.z - minPos.z) / bbc.getChunkSize()));
    const auto startY = std::max(0, yPos);
    const auto endY = std::min(bbc.getChunkCount().y, std::uint32_t(yPos + GridSizeY));

    auto& drawList = bbc.getDrawList(camera.getComponent<Camera>().getDrawListIndex());
    drawList.clear();

    const auto& mesh = parent.getComponent<Model>().getMeshData();
    const auto testPos = glm::vec3(camTx[3]);

    for (auto y = startY; y < endY; ++y)
    {
        for (auto x = startX; x < endX; ++x)
        {
            const auto idx = (y * bbc.getChunkCount().x) + x;
            const auto& chunk = bbc.getChunkIndices()[idx];

            const glm::vec3 centre =
                glm::vec3(coordStart.x + (x * bbc.getChunkSize()),
                    chunk.meanHeight,
                    coordStart.y + (y * bbc.getChunkSize())) + (BoundsPadding / 2.f);

            const auto dir = centre - testPos;
            if (const auto len2 = glm::length2(dir); len2 < m_maxDist2 && len2 > m_minDist2)
            {
                //const float hFOV = std::tan(cam.getFOV() / 2.f) * cam.getAspectRatio();
                const float hFOV = 0.5f; //nice idea but a) assumes horizontal is widest axis b) doesn't account for AABB overlap
                //if (const auto d = glm::dot(glm::normalize(dir), cro::Util::Matrix::getForwardVector(camTx)); d > hFOV)
                {
                    drawList.addChunk(mesh, chunk);
                }
            }
        }
    }
}