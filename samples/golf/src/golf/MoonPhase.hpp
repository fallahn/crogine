#pragma once

#include <ctime>
#include <string>

//calculates the current moon phase from the reference date
//of 6th Jan 200 from a given time_t
class MoonPhase final
{
public:
    //default ctor
    MoonPhase();

    //construct with the given timestamp
    explicit MoonPhase(std::time_t);

    //normalised, 0.5 is full moon
    float getPhase() const { return m_phase; }
    
    //number of days into current cycle
    float getCycle() const { return m_cycle; }

    //name of the current phase
    const std::string& getPhaseName() const;

    //update the phase to the given epoch time
    void update(std::time_t);

private:
    float m_phase;
    float m_cycle;
    std::size_t m_phaseIndex;
};