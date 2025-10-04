#define IMGUI_DATEPICKER_YEAR_MIN 2020
#define IMGUI_DATEPICKER_YEAR_MAX 2040

#include "Aer.hpp"

#include <crogine/gui/Gui.hpp>

#include <sstream>

namespace
{
    static constexpr auto OneDay = 24 * 60 * 60;
}

Aer::Aer()
    : m_visible         (true),
    m_currentTotal      (0.f),
    m_avgInterestRate   (0.f),
    m_interest          (0.f)
{
    std::memset(&m_taxYear, 0, sizeof(tm));

    m_taxYear.tm_mon = 3;
    m_taxYear.tm_mday = 6;
    m_taxYear.tm_year = 124;
    m_taxYear.tm_hour = 1;
    m_taxYear.tm_isdst = 0;

    //1712365200
    m_paymentDate = m_rateDate = m_taxYear;

    registerWindow(
        [&]()
        {
            if (m_visible)
            {
                ImGui::SetNextWindowSize({ 600.f, 400.f });
                if (ImGui::Begin("AER Calculator"))
                {
                    ImGui::BeginChild("Rates", { 280.f, 300.f }, ImGuiChildFlags_Border);
                    ImGui::TextUnformatted("Rate");

                    static auto rateIdx = 0u;
                    if (ImGui::BeginListBox("##rates_list", {264.f, 0.f}))
                    {
                        for (auto i = 0u; i < m_rates.size(); ++i)
                        {
                            const bool selected = rateIdx == i;
                            if (ImGui::Selectable(m_rates[i].description.c_str(), selected))
                            {
                                rateIdx = i;
                            }

                            if (selected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }

                        ImGui::EndListBox();
                    }

                    if (ImGui::DatePicker("##rate_date", m_rateDate, true, 10.f))
                    {
                        
                    }
                    static float rateVal = 2.f;
                    ImGui::SetNextItemWidth(264.f);
                    ImGui::InputFloat("##rate_value", &rateVal);

                    ImGui::NewLine();
                    if (ImGui::Button("Add"))
                    {
                        const auto timestamp = std::mktime(&m_rateDate);

                        //only add this if timestamp is unique
                        if (std::find_if(m_rates.begin(), m_rates.end(), 
                            [timestamp](const ValueItem& r) {return r.timestamp == timestamp;}) == m_rates.end())
                        {
                            auto& r = m_rates.emplace_back(rateVal, timestamp);
                            refreshRates();
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Remove"))
                    {
                        if (rateIdx < m_rates.size())
                        {
                            m_rates.erase(m_rates.begin() + rateIdx);
                            if (!m_rates.empty())
                            {
                                rateIdx %= m_rates.size();
                            }
                            else
                            {
                                rateIdx = 0;
                            }

                            refreshRates();
                        }
                    }

                    ImGui::Text("Avg rate: %3.3f", m_avgInterestRate);

                    ImGui::EndChild();
                    ImGui::SameLine();
                    ImGui::BeginChild("Payments", { 280.f, 300.f }, ImGuiChildFlags_Border);

                    ImGui::TextUnformatted("Payment");

                    static auto payIdx = 0u;
                    if (ImGui::BeginListBox("##payment_list", { 264.f, 0.f }))
                    {
                        for (auto i = 0u; i < m_payments.size(); ++i)
                        {
                            const bool selected = payIdx == i;
                            if (ImGui::Selectable(m_payments[i].description.c_str(), selected))
                            {
                                payIdx = i;
                            }

                            if (selected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }

                        ImGui::EndListBox();
                    }

                    if (ImGui::DatePicker("##payment_date", m_paymentDate, true, 10.f))
                    {

                    }
                    static float paymentVal = 0.f;
                    ImGui::SetNextItemWidth(264.f);
                    ImGui::InputFloat("##payment_value", &paymentVal);

                    ImGui::NewLine();
                    if (ImGui::Button("Add"))
                    {
                        const auto timestamp = std::mktime(&m_paymentDate);
                        m_payments.emplace_back(paymentVal, timestamp);

                        refreshPayments();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Remove"))
                    {
                        if (payIdx < m_payments.size())
                        {
                            m_payments.erase(m_payments.begin() + payIdx);
                            if (!m_rates.empty())
                            {
                                payIdx %= m_rates.size();
                            }
                            else
                            {
                                payIdx = 0;
                            }

                            refreshPayments();
                        }
                    }

                    //huh? dollar sign works but pound sign is rendered as a ?
                    ImGui::Text("Total: $%3.2f", m_currentTotal);
                    ImGui::SameLine();
                    ImGui::Text("Interest: $%3.2f", m_interest);

                    ImGui::EndChild();

                    ImGui::PushStyleColor(ImGuiCol_Text, { 1.f,0.f,0.f,1.f });
                    ImGui::Text("THIS IS SET TO 2024 TAX YEAR!");
                    ImGui::PopStyleColor();
                }
                ImGui::End();
            }
        });
}

//public


//private
void Aer::refreshRates()
{
    //update descriptions TODO we don't need to do this for every rate!
    //just any that doesn't yet have a description
    for (auto& rate : m_rates)
    {
        std::stringstream ss;
        ss << "Rate: " << rate.value << "%, " << std::put_time(std::localtime(&rate.timestamp), "%d-%m-%Y");
        rate.description = ss.str();
    }
    //sort by date
    std::sort(m_rates.begin(), m_rates.end(), [](const ValueItem& a, const ValueItem& b) {return a.timestamp < b.timestamp; });


    //recalculate the daily rate for the year
    m_dailyRates.clear();
    m_avgInterestRate = 0.f;

    if (!m_rates.empty())
    {
        auto currDate = m_rates[0].timestamp;
        auto index = 0;

        for (auto i = 0; i < 365; ++i)
        {
            m_dailyRates.push_back(m_rates[index].value / 365.f);
            currDate += OneDay;

            m_avgInterestRate += m_dailyRates.back();

            if (index < (static_cast<std::int32_t>(m_rates.size()) - 1))
            {
                while (index < (static_cast<std::int32_t>(m_rates.size()) - 1)
                    && std::localtime(&currDate)->tm_yday == std::localtime(&m_rates[index + 1].timestamp)->tm_yday)
                {
                    index++;
                }
            }
        }
    }
//TODO verify this at least gives the same answer as the other version

    updateInterest();
}

void Aer::refreshPayments()
{
    for (auto& payment : m_payments)
    {
        std::stringstream ss;
        ss << "Payment: $" << payment.value << ", " << std::put_time(std::localtime(&payment.timestamp), "%d-%m-%Y");
        payment.description = ss.str();
    }

    m_runningTotal.clear();
    m_currentTotal = 0.f;

    if (!m_payments.empty())
    {
        //TODO ask for the beginning of the current tax year
        auto currDate = std::mktime(&m_taxYear);
        auto index = -1;

        for (auto i = 0; i < 365; ++i)
        {
            m_runningTotal.push_back(m_currentTotal); //we'll be 0 from the very beginning because the first payment is at the end of the month
            currDate += OneDay;

            if (index < (static_cast<std::int32_t>(m_payments.size()) - 1))
            {
                const auto currDay = std::localtime(&currDate)->tm_yday;
                const auto paymentDay = std::localtime(&m_payments[index + 1].timestamp)->tm_yday;

                while (index < (static_cast<std::int32_t>(m_payments.size()) - 1)
                    && currDay == paymentDay)
                {
                    index++;
                    m_currentTotal += m_payments[index].value;
                }
            }
        }
    }

    updateInterest();
}

void Aer::updateInterest()
{
    m_interest = 0.f;

    if (m_runningTotal.size() == m_dailyRates.size())
    {
        for (auto i = 0u; i < m_runningTotal.size(); ++i)
        {
            const auto val = (m_runningTotal[i] / 100.f) * m_dailyRates[i];
            m_interest += val;
        }
    }
    /*else
    {
        LogE << "Runing total size: " << m_runningTotal.size() << ", Daily rates size: " << m_dailyRates.size() << std::endl;
    }*/
}

void Aer::read(const std::string& path)
{

}

void Aer::write(const std::string& path)
{

}