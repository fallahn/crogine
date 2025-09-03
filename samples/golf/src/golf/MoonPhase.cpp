#include "MoonPhase.hpp"

#include <array>
#include <cmath>

namespace
{
    //reference cycle offset in days, 6th Jan 2000
    constexpr double SynodicOffset = 2451550.26;
    //period of moon cycle in days.
    constexpr double SynodicPeriod = 29.530588853;

    const std::array<std::string, 8u> PhaseNames =
    {
        "New", "Waxing Crescent", "First Quarter", "Waxing Gibbous",
        "Full", "Waning Gibbous", "Last Quarter", "Waning Crescent"
    };

    double toJulian(std::time_t t)
    {
        return static_cast<double>(t) / 86400.0 + 2440587.5;
    }
}

MoonPhase::MoonPhase()
    : m_phase       (0.f),
    m_cycle         (0.f),
    m_phaseIndex    (0)
{

}

MoonPhase::MoonPhase(std::time_t t)
    : MoonPhase()
{
    update(t);
}

//public
void MoonPhase::update(std::time_t t)
{
    const auto date = toJulian(t);

    auto phase = (date - SynodicOffset) / SynodicPeriod;
    phase -= std::floor(phase);
    m_phase = static_cast<float>(phase);

    m_cycle = static_cast<float>(phase * SynodicPeriod);

    m_phaseIndex = static_cast<std::size_t>(phase * 8.0 + 0.5) % PhaseNames.size();
}

const std::string& MoonPhase::getPhaseName() const
{
    return PhaseNames[m_phaseIndex];
}