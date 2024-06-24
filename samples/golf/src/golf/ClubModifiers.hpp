#include "Clubs.hpp"

struct ShotModifier final
{
    float angle = 0.f;
    float powerMultiplier = 1.f;
    float targetMultiplier = 1.f;
    ShotModifier(float a, float p, float t)
        : angle(a), powerMultiplier(p), targetMultiplier(t){}
};

struct ModifierGroup final
{
    const std::array<ShotModifier, ClubID::Count> DefaultModifier = {}; //leaves the default shot
    std::array<ShotModifier, ClubID::Count> punchModifier = {};
    std::array<ShotModifier, ClubID::Count> flopModifier = {};

    ModifierGroup() = default;
    ModifierGroup(const std::array<ShotModifier, ClubID::Count>& p, const std::array<ShotModifier, ClubID::Count>& f)
        : punchModifier(p), flopModifier(f) {}
};

static constexpr std::array<ShotModifier, ClubID::Count> pA = 
{
    ShotModifier(-0.287f, 1.439f, 1.048f),
    ShotModifier(-0.259f, 1.286f, 1.061f),
    ShotModifier(-0.297f, 1.307f, 1.068f),
    ShotModifier(-0.358f, 1.353f, 1.075f),
    ShotModifier(-0.351f, 1.353f, 1.082f),
    ShotModifier(-0.343f, 1.353f, 1.088f),
    ShotModifier(-0.282f, 1.279f, 1.095f),
    ShotModifier(-0.290f, 1.292f, 1.109f),
    ShotModifier(-0.290f, 1.313f, 1.122f),
    ShotModifier(-0.503f, 1.129f, 1.143f),
    ShotModifier(-0.686f, 1.217f, 1.333f),
    ShotModifier(-0.686f, 1.306f, 1.500f),
    ShotModifier(0.000f, 1.000f, 1.000f),
}

static constexpr std::array<ShotModifier, ClubID::Count> fA = 
{
    ShotModifier(0.294f, 0.879f, 0.910f),
    ShotModifier(0.244f, 0.898f, 0.891f),
    ShotModifier(0.274f, 0.911f, 0.871f),
    ShotModifier(0.343f, 0.918f, 0.789f),
    ShotModifier(0.344f, 0.905f, 0.776f),
    ShotModifier(0.351f, 0.884f, 0.755f),
    ShotModifier(0.328f, 0.857f, 0.725f),
    ShotModifier(0.343f, 0.850f, 0.701f),
    ShotModifier(0.373f, 0.837f, 0.674f),
    ShotModifier(0.359f, 0.966f, 0.429f),
    ShotModifier(0.305f, 1.095f, 0.504f),
    ShotModifier(0.313f, 1.000f, 0.511f),
    ShotModifier(0.000f, 1.000f, 1.000f),
}

static constexpr std::array<ShotModifier, ClubID::Count> pB = 
{
    ShotModifier(-0.245f, 1.333f, 1.041f),
    ShotModifier(-0.229f, 1.238f, 1.054f),
    ShotModifier(-0.305f, 1.313f, 1.068f),
    ShotModifier(-0.267f, 1.211f, 1.068f),
    ShotModifier(-0.305f, 1.265f, 1.075f),
    ShotModifier(-0.275f, 1.251f, 1.082f),
    ShotModifier(-0.328f, 1.347f, 1.088f),
    ShotModifier(-0.290f, 1.292f, 1.095f),
    ShotModifier(-0.305f, 1.326f, 1.102f),
    ShotModifier(-0.412f, 1.054f, 1.136f),
    ShotModifier(-0.496f, 1.075f, 1.313f),
    ShotModifier(-0.686f, 1.306f, 1.500f),
    ShotModifier(0.000f, 1.000f, 1.000f),
}

static constexpr std::array<ShotModifier, ClubID::Count> fB = 
{
    ShotModifier(0.350f, 0.885f, 0.918f),
    ShotModifier(0.282f, 0.912f, 0.898f),
    ShotModifier(0.275f, 0.912f, 0.877f),
    ShotModifier(0.328f, 0.919f, 0.803f),
    ShotModifier(0.358f, 0.912f, 0.789f),
    ShotModifier(0.351f, 0.898f, 0.776f),
    ShotModifier(0.351f, 0.877f, 0.752f),
    ShotModifier(0.274f, 0.850f, 0.728f),
    ShotModifier(0.328f, 0.844f, 0.708f),
    ShotModifier(0.374f, 1.041f, 0.470f),
    ShotModifier(0.320f, 1.143f, 0.504f),
    ShotModifier(0.313f, 1.000f, 0.511f),
    ShotModifier(0.000f, 1.000f, 1.000f),
}

static constexpr std::array<ShotModifier, ClubID::Count> pC = 
{
    ShotModifier(-0.236f, 1.327f, 1.075f),
    ShotModifier(-0.274f, 1.306f, 1.041f),
    ShotModifier(-0.274f, 1.265f, 1.061f),
    ShotModifier(-0.298f, 1.244f, 1.068f),
    ShotModifier(-0.351f, 1.347f, 1.068f),
    ShotModifier(-0.343f, 1.360f, 1.075f),
    ShotModifier(-0.328f, 1.346f, 1.082f),
    ShotModifier(-0.336f, 1.373f, 1.088f),
    ShotModifier(-0.289f, 1.285f, 1.095f),
    ShotModifier(-0.419f, 1.061f, 1.136f),
    ShotModifier(-0.511f, 1.061f, 1.285f),
    ShotModifier(-0.686f, 1.306f, 1.500f),
    ShotModifier(0.000f, 1.000f, 1.000f),
}

static constexpr std::array<ShotModifier, ClubID::Count> fC = 
{
    ShotModifier(0.351f, 0.891f, 0.925f),
    ShotModifier(0.290f, 0.912f, 0.912f),
    ShotModifier(0.298f, 0.932f, 0.891f),
    ShotModifier(0.259f, 0.905f, 0.817f),
    ShotModifier(0.312f, 0.912f, 0.803f),
    ShotModifier(0.335f, 0.898f, 0.789f),
    ShotModifier(0.344f, 0.891f, 0.776f),
    ShotModifier(0.320f, 0.871f, 0.755f),
    ShotModifier(0.343f, 0.857f, 0.728f),
    ShotModifier(0.374f, 1.075f, 0.504f),
    ShotModifier(0.320f, 1.129f, 0.490f),
    ShotModifier(0.313f, 1.000f, 0.511f),
    ShotModifier(0.000f, 1.000f, 1.000f),
}

static constexpr std::array<ModifierGroup, 3u> LevelModifiers =
{
    ModifierGroup(pA, fA),
    ModifierGroup(pB, fB),
    ModifierGroup(pC, fC)
};


const std::array<Club, ClubID::Count> Clubs =
{
    Club(ClubID::Driver,    "Driver ", 28.992f),
    Club(ClubID::ThreeWood, "3 Wood ", 32.315f),
    Club(ClubID::FiveWood,  "5 Wood ", 34.721f),
    Club(ClubID::FourIron,  "4 Iron ", 37.586f),
    Club(ClubID::FiveIron,  "5 Iron ", 37.128f),
    Club(ClubID::SixIron,   "6 Iron ", 36.326f),
    Club(ClubID::SevenIron, "7 Iron ", 35.924f),
    Club(ClubID::EightIron, "8 Iron ", 35.924f),
    Club(ClubID::NineIron,  "9 Iron ", 35.523f),
    Club(ClubID::PitchWedge, "Pitch Wedge ", 56.895f),
    Club(ClubID::GapWedge,   "Gap Wedge ",   62.452f),
    Club(ClubID::SandWedge,  "Sand Wedge ",  60.000f),
    Club(ClubID::Putter,     "Putter ",      0.000f),
};

constexpr std::array<ClubStat, ClubID::Count> ClubStats = 
{
    ClubStat({ 47.199f, 220.000f }, { 49.378f, 240.000f }, { 51.398f, 260.000f }),
    ClubStat({ 41.344f, 180.000f }, { 43.534f, 200.000f }, { 45.694f, 220.000f }),
    ClubStat({ 37.172f, 150.000f }, { 38.402f, 160.000f }, { 40.533f, 180.000f }),
    ClubStat({ 35.149f, 140.000f }, { 36.490f, 150.000f }, { 37.690f, 160.000f }),
    ClubStat({ 33.869f, 130.000f }, { 35.440f, 140.000f }, { 36.490f, 150.000f }),
    ClubStat({ 32.850f, 120.000f }, { 34.160f, 130.000f }, { 35.440f, 140.000f }),
    ClubStat({ 31.330f, 110.000f }, { 32.850f, 120.000f }, { 34.160f, 130.000f }),
    ClubStat({ 29.910f, 100.000f }, { 31.330f, 110.000f }, { 32.850f, 120.000f }),
    ClubStat({ 28.400f, 90.000f }, { 29.910f, 100.000f }, { 31.621f, 110.000f }),
    ClubStat({ 25.491f, 70.000f }, { 26.562f, 75.000f }, { 27.460f, 80.000f }),
    ClubStat({ 17.691f, 30.000f }, { 18.239f, 32.000f }, { 19.140f, 35.000f }),
    ClubStat({ 10.300f, 10.000f }, { 10.300f, 10.000f }, { 10.300f, 10.000f }),
    ClubStat({ 9.110f, 10.000f }, { 9.110f, 10.000f }, { 9.110f, 10.000f }),
};