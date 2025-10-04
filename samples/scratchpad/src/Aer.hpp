#pragma once

#include <crogine/gui/GuiClient.hpp>

#include <ctime>
#include <string>
#include <vector>

struct ValueItem final
{
    float value = 0.f;
    std::time_t timestamp = 0;

    std::string description;

    ValueItem() = default;
    ValueItem(float v, std::time_t t)
        : value(v), timestamp(t) { }
};

class Aer final : public cro::GuiClient
{
public:
    Aer();

    void setVisible(bool b) { m_visible = b; }
    bool getVisible() const { return m_visible; }

private:
    bool m_visible;

    std::vector<ValueItem> m_rates;
    std::vector<float> m_dailyRates;

    std::vector<ValueItem> m_payments;
    std::vector<float> m_runningTotal;
    float m_currentTotal;

    float m_interest;
    float m_avgInterestRate;

    tm m_rateDate;
    tm m_paymentDate;
    tm m_taxYear;

    void refreshRates();
    void refreshPayments();

    void updateInterest();


    void read(const std::string& path);
    void write(const std::string& path);
};