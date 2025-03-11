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

template <>
inline std::string ConfigProperty::getValue<std::string>() const
{
#ifdef OLD_PARSER
    return m_value;
#else
    return m_utf8Values.empty() ? std::string("null") : std::string(m_utf8Values[0].begin(), m_utf8Values[0].end());
#endif
}

template<>
inline cro::String ConfigProperty::getValue<cro::String>() const
{
#ifdef OLD_PARSER
    if (!m_isStringValue)
    {
        return m_value;
    }
    return cro::String::fromUtf8(m_value.begin(), m_value.end());
#else
    return m_utf8Values.empty() ? cro::String("null") : cro::String::fromUtf8(m_utf8Values[0].begin(), m_utf8Values[0].end());
#endif
}

template <>
inline std::int32_t ConfigProperty::getValue<std::int32_t>() const
{
#ifdef OLD_PARSER
    std::int32_t retVal;
    std::istringstream is(m_value);
    if (is >> retVal) return retVal;
    return 0;
#else
    return m_floatValues.empty() ? 0 : static_cast<std::int32_t>(m_floatValues[0]);
#endif
}

template <>
inline std::uint32_t ConfigProperty::getValue<std::uint32_t>() const
{
#ifdef OLD_PARSER
    std::uint32_t retVal;
    std::istringstream is(m_value);
    if (is >> retVal) return retVal;
    return 0u;
#else
    return m_floatValues.empty() ? 0u : static_cast<std::uint32_t>(m_floatValues[0]);
#endif
}

template <>
inline float ConfigProperty::getValue<float>() const
{
#ifdef OLD_PARSER
    float retVal;
    std::istringstream is(m_value);
    if (is >> retVal) return retVal;
    return 0.f;
#else
    return m_floatValues.empty() ? 0.f : static_cast<float>(m_floatValues[0]);
#endif
}

template <>
inline bool ConfigProperty::getValue<bool>() const
{
#ifdef OLD_PARSER
    return (m_value == "true");
#else
    return m_boolValue;
#endif
}

template <>
inline glm::vec2 ConfigProperty::getValue<glm::vec2>() const
{
#ifdef OLD_PARSER
    auto values = valueAsArray();
#else
    const auto& values = m_floatValues;
#endif

    glm::vec2 retval(0.f); //loop allows for values to be the wrong size
    for (auto i = 0u; i < values.size() && i < 2u; ++i)
    {
        retval[i] = values[i];
    }

    return retval;
}

template <>
inline glm::vec3 ConfigProperty::getValue<glm::vec3>() const
{
#ifdef OLD_PARSER
    auto values = valueAsArray();
#else
    const auto& values = m_floatValues;
#endif

    glm::vec3 retval(0.f);
    for (auto i = 0u; i < values.size() && i < 3u; ++i)
    {
        retval[i] = values[i];
    }

    return retval;
}

template <>
inline glm::vec4 ConfigProperty::getValue<glm::vec4>() const
{
#ifdef OLD_PARSER
    auto values = valueAsArray();
#else
    const auto& values = m_floatValues;
#endif

    glm::vec4 retval(0.f);
    for (auto i = 0u; i < values.size() && i < 4u; ++i)
    {
        retval[i] = values[i];
    }

    return retval;
}

template <>
inline FloatRect ConfigProperty::getValue<FloatRect>() const
{
    //ensures the array is in range and that we use the correct source regardless of parser define
    auto values = getValue<glm::vec4>();
    return { values[0], values[1], values[2], values[3] };
}

template <>
inline Colour ConfigProperty::getValue<Colour>() const
{
    auto values = getValue<glm::vec4>();//ensures the array is in range
    auto clamp = [](float v)
    {
        return std::max(0.f, std::min(1.f, v));
    };
    return cro::Colour(clamp(values[0]), clamp(values[1]), clamp(values[2]), clamp(values[3]));
}