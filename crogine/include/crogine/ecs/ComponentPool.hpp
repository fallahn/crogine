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

#pragma once

#include <crogine/detail/Detail.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/detail/NoResize.hpp>

#include <vector>
#include <numeric>

namespace cro
{
    namespace Detail
    {
        class Pool
        {
        public:
            static constexpr auto nullindex = std::numeric_limits<std::uint32_t>::max();

            explicit Pool(const std::string& name)
                : m_freeIndex(0), m_indexMap(Detail::MinFreeIDs), m_indexPool(Detail::MinFreeIDs), m_name(name)
            {
                std::fill(m_indexMap.begin(), m_indexMap.end(), nullindex);
                std::iota(m_indexPool.begin(), m_indexPool.end(), 0);
            }
            
            virtual ~Pool() = default;
            virtual void clear() = 0;
            virtual void reset(std::uint32_t) = 0;

            std::size_t maxSize() const
            {
                return m_indexPool.size();
            }

            std::size_t used() const
            {
                return m_freeIndex;
            }

            const std::string& getName() const
            {
                return m_name;
            }

        protected:
            std::size_t m_freeIndex;
            std::vector<std::uint32_t> m_indexMap;
            std::vector<std::uint32_t> m_indexPool;

            std::string m_name;
        };

        /*!
        \brief memory pooling for components

        Note: components which inherit NoResize or have no copy assignment
        (eg move-only types such as Model or AudioEmitter) reserve MinFreeID
        slots in a pool, to prevent resizing and invalidating components.
        */
        template <class T>
        class ComponentPool final : public Pool
        {
        public:
            explicit ComponentPool(std::size_t size = 128) 
                : Pool  (typeid(T).name()),
                m_pool  (size)
            {
                if constexpr (!std::is_copy_assignable_v<T>
                    || std::is_base_of_v<NonResizeable, T>)
                {
                    m_pool.reserve(MaxPoolSize<T>::value);
                    LOG("Reserved maximum pool size of " + std::to_string(MaxPoolSize<T>::value) + " for " + std::string(typeid(T).name()), cro::Logger::Type::Info);
                }
            }

            T& at(std::size_t idx)
            {
                return m_pool.at(m_indexMap[idx]);
                //return m_pool.at(idx);
            }
            const T& at(std::size_t idx) const
            {
                return m_pool.at(m_indexMap[idx]);
                //return m_pool.at(idx);
            }

            void insert(std::uint32_t idx, T component)
            {
                /*if (idx >= size())
                {
                    resize(std::min(static_cast<std::uint32_t>(Detail::MinFreeIDs), idx + 128));
                }*/
                //m_pool[idx] = std::move(component);
                
                
                
                m_indexMap[idx] = m_indexPool[m_freeIndex];
                m_freeIndex++;

                const auto mappedIdx = m_indexMap[idx];
                if (mappedIdx >= size())
                {
                    resize(std::min(static_cast<std::uint32_t>(Detail::MinFreeIDs), mappedIdx + 128));
                }

                m_pool[mappedIdx] = std::move(component);
            }

            void reset(std::uint32_t idx) override
            {
                /*if (idx < m_pool.size())
                {
                    m_pool[idx] = T();
                }*/
                
                
                
                const auto mappedIdx = m_indexMap[idx];
                
                if (mappedIdx != nullindex)
                {
                    m_pool[mappedIdx] = T();
                    
                    m_freeIndex--;
                    m_indexPool[m_freeIndex] = mappedIdx;
                    m_indexMap[idx] = nullindex;
                }
            }

            void clear() override
            {
                m_pool.clear();
                m_freeIndex = 0;

                std::fill(m_indexMap.begin(), m_indexMap.end(), nullindex);
                std::iota(m_indexPool.begin(), m_indexPool.end(), 0);
            }

            bool empty() const
            {
                return m_pool.empty();
            }

            std::size_t size() const
            {
                return m_pool.size();
            }

        private:
            std::vector<T> m_pool;

            void resize(std::size_t size)
            {
                assert(size <= MaxPoolSize<T>::value);
                if (size > m_pool.size())
                {
                    m_pool.resize(size);

                    //just surpress this if we're of a type which reserves
                    //max size, else the message is confusing
                    if constexpr (std::is_copy_assignable_v<T>
                        || !std::is_base_of_v<NonResizeable, T>)
                    {
                        LOG("Warning component pool " + std::string(typeid(T).name()) + " has been resized to " + std::to_string(m_pool.size()) + " - existing component references may be invalidated", cro::Logger::Type::Warning);
                    }
                }
            }
        };
    }
}