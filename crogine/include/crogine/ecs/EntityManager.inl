/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

template <typename T>
void EntityManager::addComponent(Entity entity, T component)
{
    auto componentID = m_componentManager.getID<T>();
    auto entID = entity.getIndex();

    auto& pool = getPool<T>();

    pool.insert(entID, std::move(component));
    m_componentMasks[entID].set(componentID);
}

template <typename T, typename... Args>
T& EntityManager::addComponent(Entity entity, Args&&... args)
{
    T component(std::forward<Args>(args)...);
    addComponent<T>(entity, std::move(component));
    return getComponent<T>(entity);
}

template <typename T>
bool EntityManager::hasComponent(Entity entity) const
{
    const auto componentID = m_componentManager.getID<T>();
    const auto entityID = entity.getIndex();

    CRO_ASSERT(entityID < m_componentMasks.size(), "Entity index out of range");
    return m_componentMasks[entityID].test(componentID);
}

template <typename T>
T& EntityManager::getComponent(Entity entity)
{
    const auto componentID = m_componentManager.getID<T>();
    const auto entityID = entity.getIndex();

    CRO_ASSERT(hasComponent<T>(entity), "Component does not exist!");


    CRO_ASSERT(componentID < m_componentPools.size(), "Component index out of range");
    auto* pool = (dynamic_cast<Detail::ComponentPool<T>*>(m_componentPools[componentID].get()));

    CRO_ASSERT(entityID < pool->size(), "Entity index out of range");
    return pool->at(entityID);
}

template <typename T>
Detail::ComponentPool<T>& EntityManager::getPool()
{
    const auto componentID = m_componentManager.getID<T>();

    if (!m_componentPools[componentID])
    {
        m_componentPools[componentID] = std::make_unique<Detail::ComponentPool<T>>(m_initialPoolSize);
    }

    return *(dynamic_cast<Detail::ComponentPool<T>*>(m_componentPools[componentID].get()));
}