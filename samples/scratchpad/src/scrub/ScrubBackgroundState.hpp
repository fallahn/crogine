#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>

class ScrubBackgroundState final : public cro::State
{
public:
    ScrubBackgroundState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::ScrubBackground; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

};
