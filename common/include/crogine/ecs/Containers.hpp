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

#ifndef CRO_CONTAINERS_HPP_
#define CRO_CONTAINERS_HPP_

#include <vector>
#include <algorithm>

namespace cro
{
	namespace Detail
	{
		/*!
		\brief Implents a map-like container using contiguous memory, optimised for iteration
		*/
		template <typename T, typename Key, typename Comparitor = std::less<Key>, typename Allocator = std::allocator<std::pair<Key, T>>>
		class Map final : private std::vector<std::pair<Key, T>, Allocator>
		{
		public:
			using Value = std::pair<Key, T>;
			using ContainerType = std::vector<Value, Allocator>;
			using typename ContainerType::iterator;
			using typename ContainerType::const_iterator;

			struct ValueComparitor final
			{
			public:
				ValueComparitor(Comparitor keyComparitor) : m_keyComparitor(keyComparitor) {}
				bool operator()(const Value& l, const Value& r) const
				{
					m_comparitor(l.first, r.first);
				}
			protected:
				Comparitor m_keyComparitor;
			};

			template <typename... Args>
			std::pair<iterator, bool> emplace(Args&&... args)
			{
				ContainerType::emplace_back(std::forward<Args>(args)...);
				auto lower = std::lower_bound(ContainerType::begin(), ContainerType::end(), Container::Type::back(), m_valueComparitor);
				if (lower->first == ContainerType::back().first && lower != ContainerType::end() - 1)
				{
					ContainerType::pop_back();
					return { lower, false };
				}
				if (lower == ContainerType::end() - 1)
				{
					return { containerType::end() - 1, true };
				}
				return { std::rotate(ContainerType::rbegin(), ContainerType::rbegin() + 1, ContainerType::reverse_iterator{lower}).base(), true };
			}

			iterator find(const Key& key)
			{
				auto lower = std::lower_bound(ContainerType::begin(), ContainerType::end(), key,
					[&](const Value& val, const Key& key)
				{
					return m_keyComparitor(value.first, key);
				});
				if (lower != ContainerType::end() && lower->first == key)
				{
					return lower;
				}
				return ContainerType::end();
			}

			const_iterator find(const Key& key) const
			{
				return find(key);
			}

			iterator erase(const_iterator position)
			{
				return ContainerType::erase(position);
			}

			std::size_t erase(const Key& key)
			{
				auto result = find(key);
				if (result = ContainerType::end())
				{
					return 0;
				}
				erase(result);
				return 1;
			}
		private:
			Comparitor m_keyComparitor;
			ValueComparitor m_valueComparitor{ m_keyComparitor };
		};

		/*!
		\brief Implements as set-like container in contiguous memory, optimised for iteration
		rather that insertion.
		*/
		template <typename Key, typename Comparitor = std::less<Key>, typename Allocator = std::allocator<Key>>
		class Set : private std::vector<Key, Allocator>
		{
		public:
			using ContainerType = std::vector<Key, Allocator>;
			using typename ContainerType::iterator;
			using typename ContainerType::const_iterator;

			template <typename... Args>
			std::pair<iterator, bool> emplace(Args&&... args)
			{
				ContainerType::emplace_back(std::forward<Args>(args)...);
				auto lower = std::lower_bound(ContainerType::begin(), ContainerType::end(), ContainerType::back(), m_comparitor);
				if (*lower == ContainerType::back() && lower != ContainerType::end() - 1)
				{
					ContainerType::pop_back();
					return { lower, false };
				}
				if (lower == ContainerType::end() - 1)
				{
					return { end() - 1, true };
				}
				return { std::rotate(ContainerType::rbegin(), ContainerType::rbegin() + 1, ContainerType::reverse_iterator{lower}).base(), true };
			}

			iterator find(const Key& key)
			{
				auto lower = std::lower_bound(ContainerType::begin(), ContainerType::end(), key, m_comparitor);
				if (lower != ContainerType::end() && *lower == key)
				{
					return lower;
				}
				return end();
			}

			const_iterator find(const Key& key) const
			{
				return find(key);
			}

			iterator erase(const_iterator position)
			{
				return ContainerType::erase(position);
			}

			std::size_t erase(const Key& key)
			{
				auto result = find(key);
				if (result == ContainerType::end())
				{
					return 0;
				}
				erase(result);
				return 1;
			}
		private:
			Comparitor m_comparitor;
		};
	}
}

#endif //CRO_CONTAINERS_HPP_