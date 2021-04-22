/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "GameState.hpp"
#include "GameConsts.hpp"

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/GlobalConsts.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>

namespace
{
    const glm::vec2 PuntBarSize = { 250.f, 20.f };
    const glm::vec2 PuntBarOffset = { 20.f, 20.f };
}

void GameState::createUI()
{
    //draws the split screen lines. The geometry
    //is calculated by the camera view update callback
    cro::Entity entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);

    m_splitScreenNode = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>(); //the game scene camera callback will also update this
    m_uiScene.setActiveCamera(entity);
}

void GameState::updatePlayerUI()
{
    for (auto i = 0u; i < m_cameras.size(); ++i)
    {
        if (!m_playerUIs[i].puntBar.isValid())
        {
            //create the entities
            m_playerUIs[i].puntBar = m_uiScene.createEntity();
            m_playerUIs[i].puntBar.addComponent<cro::Transform>();
            m_playerUIs[i].puntBar.addComponent<cro::Drawable2D>();
        }

        //update the geometry
        auto playerIndex = m_cameras[i].getComponent<std::uint8_t>();
        m_playerUIs[i].puntBar.getComponent<cro::Drawable2D>().getVertexData() =
        {
            cro::Vertex2D(glm::vec2(0.f, PuntBarSize.y), PlayerColours[playerIndex]),
            cro::Vertex2D(glm::vec2(0.f), PlayerColours[playerIndex]),
            cro::Vertex2D(PuntBarSize, PlayerColours[playerIndex]),
            cro::Vertex2D(glm::vec2(PuntBarSize.x, 0.f), PlayerColours[playerIndex])
        };
        m_playerUIs[i].puntBar.getComponent<cro::Drawable2D>().updateLocalBounds();
        m_playerUIs[i].puntBar.getComponent<cro::Transform>().setPosition(
            glm::vec2(PuntBarOffset.x + ((cro::DefaultSceneSize.x / 2) * (i % 2)),
                (((1 - (i / 2)) * (cro::DefaultSceneSize.y / 2)) * (m_cameras.size() / 3)) + PuntBarOffset.y));
    }
}