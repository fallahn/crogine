/*-----------------------------------------------------------------------

Matt Marchant 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include <MonthlyChallenge.hpp>
#include <Social.hpp>
#include <Achievements.hpp>
#include <StoredValue.hpp>

#include <crogine/core/SysTime.hpp>

#include <bitset>

namespace
{
    std::array<StoredValue, 12u> StoredValues =
    {
        StoredValue("m00"),
        StoredValue("m01"),
        StoredValue("m02"),
        StoredValue("m03"),
        StoredValue("m04"),
        StoredValue("m05"),
        StoredValue("m06"),
        StoredValue("m07"),
        StoredValue("m08"),
        StoredValue("m09"),
        StoredValue("m10"),
        StoredValue("m11"),
    };

    StoredValue challengeFlags("flg");
    constexpr std::int32_t ChallengeFlagsComplete = 0xfff;

    constexpr std::array<std::int32_t, 12u> MonthDays =
    {
        31,28,31,30,31,30,31,31,30,31,30,31
    };

    const cro::Time MinRepeatTime = cro::seconds(0.5f);
}


MonthlyChallenge::MonthlyChallenge()
    : m_month   (-1),
    m_day       (0),
    m_leapYear  (false)
{
    //fetch the current month
    auto ts = static_cast<std::time_t>(cro::SysTime::epoch());
    auto td = std::localtime(&ts);

    m_month = td->tm_mon;
    m_day = td->tm_mday;

    auto year = 1900 + td->tm_year;
    m_leapYear = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
}

//public
void MonthlyChallenge::updateChallenge(std::int32_t id, std::int32_t value)
{
    if (m_repeatClock.elapsed() < MinRepeatTime)
    {
        return;
    }
    m_repeatClock.restart();

    if (m_month != -1 && id == m_month
        && Achievements::getActive())
    {
        //make sure we're not already complete else the completion
        //message will get raised mutliple times...
        auto progress = getProgress();
        if (progress.value != progress.target)
        {

            if (m_challenges[id].type == Challenge::Counter)
            {
                m_challenges[id].value = std::min(m_challenges[id].value + 1, m_challenges[id].targetValue);

                auto* msg = cro::App::postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
                msg->type = Social::SocialEvent::MonthlyProgress;
                msg->level = m_challenges[id].value;
                msg->reason = m_challenges[id].targetValue;
                msg->challengeID = id;
            }
            else if (m_challenges[id].type == Challenge::Flag
                && value < 32)
            {
                auto oldVal = m_challenges[id].value;
                m_challenges[id].value |= (1 << value);

                if (oldVal != m_challenges[id].value)
                {
                    std::bitset<sizeof(Challenge::value) * 8> value(m_challenges[id].value);
                    std::bitset<sizeof(Challenge::targetValue) * 8> target(m_challenges[id].targetValue);

                    auto* msg = cro::App::postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
                    msg->type = Social::SocialEvent::MonthlyProgress;
                    msg->level = static_cast<std::int32_t>(value.count());
                    msg->reason = static_cast<std::int32_t>(target.count());
                    msg->challengeID = id;
                }
            }

            //write stat to storage
            StoredValues[m_month].value = static_cast<std::int32_t>(m_challenges[id].value);
            StoredValues[m_month].write();
        }
    }
}

void MonthlyChallenge::refresh()
{
    if (m_month > -1 && m_month < ChallengeID::Count)
    {
        for (auto i = 0; i < ChallengeID::Count; ++i)
        {
            if (i == m_month)
            {
                //fetch current value
                StoredValues[i].read();
                m_challenges[i].value = StoredValues[i].value;

                //update the completion flags here (this makes sure to retroactively apply at least this month's, if needed)
                if (m_challenges[i].value == m_challenges[i].targetValue)
                {
                    challengeFlags.read();
                    std::int32_t flags = challengeFlags.value;

                    flags |= (1 << i);
                    challengeFlags.value = flags;
                    challengeFlags.write();

                    if (flags == ChallengeFlagsComplete)
                    {
                        //award achievement
                    }
                    else
                    {
                        //display progress TODO hmmm do we want to do this on EVERY refresh?
                        std::bitset<sizeof(std::int32_t) * 8> bitflags(flags);
                        LogI << "Completed " << bitflags.count() << " challenges" << std::endl;
                    }
                }
            }
            else
            {
                //reset stat
                StoredValues[i].value = 0;
                StoredValues[i].write();
            }
        }
    }
}

MonthlyChallenge::Progress MonthlyChallenge::getProgress() const
{
    Progress retVal;
    if (m_month != -1)
    {
        if (m_challenges[m_month].type == Challenge::Counter)
        {
            retVal.target = m_challenges[m_month].targetValue;
            retVal.value = m_challenges[m_month].value;
        }
        else
        {
            std::bitset<sizeof(Challenge::value) * 8> value(m_challenges[m_month].value);
            std::bitset<sizeof(Challenge::targetValue) * 8> target(m_challenges[m_month].targetValue);

            retVal.value = static_cast<std::int32_t>(value.count());
            retVal.target = static_cast<std::int32_t>(target.count());
            retVal.flags = m_challenges[m_month].value;
        }

        retVal.index = m_month;
    }

    return retVal;
}

cro::String MonthlyChallenge::getProgressString() const
{
    cro::String ret;

    if (m_month != -1)
    {
        auto progress = getProgress();
        auto monthDays = MonthDays[m_month];
        if (m_month == 2 && m_leapYear)
        {
            monthDays++;
        }

        const auto remain = monthDays - m_day;


        ret = ChallengeDescriptions[m_month] + "\n";
        if (progress.value != progress.target)
        {
            if (m_challenges[m_month].type == Challenge::Counter)
            {
                ret += "Progress: " + std::to_string(progress.value) + "/" + std::to_string(progress.target);
            }
            else
            {
                ret += " "; //leave empty so we can display the 'check' boxes
            }

            if (remain == 0)
            {
                ret += "\nFinal Day!";
            }
            else
            {
                ret += "\n" + std::to_string(remain) + " more days remaining";
            }
        }
        else
        {
            ret += "Completed! New challenge in " + std::to_string(remain + 1) + " days";
        }
    }

    return ret;
}