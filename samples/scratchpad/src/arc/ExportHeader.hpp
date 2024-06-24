#pragma once

#include <crogine/util/Constants.hpp>

#include <fstream>
#include <string>
#include <array>

struct ClubID final
{
    enum
    {
        Driver, ThreeWood, FiveWood,
        FourIron, FiveIron, SixIron,
        SevenIron, EightIron, NineIron,
        PitchWedge, GapWedge, SandWedge,
        Putter,

        Count
    };
};

struct Stat final
{
    constexpr Stat() = default;
    constexpr Stat(float p, float t) : power(p), target(t) {}
    float power = 0.f; //impulse strength
    float target = 0.f; //distance target
};

struct ClubStat final
{
    constexpr ClubStat(Stat a, Stat b, Stat c)
        : stats() {
        stats = { a, b, c };
    }
    std::array<Stat, 3> stats = {};
};

struct Club final
{
    std::int32_t id = 0;
    std::string name;
    float angle = 0.f;

    Club(std::int32_t i, const std::string& n, float a)
        : id(i), name(n), angle(a * cro::Util::Const::degToRad) {}
};

struct ShotModifier final
{
    float angle = 0.f;
    float powerMultiplier = 1.f;
    float targetMultiplier = 1.f;
};

struct ModifierGroup final
{
    const std::array<ShotModifier, ClubID::Count> DefaultModifier = {}; //leaves the default shot
    std::array<ShotModifier, ClubID::Count> punchModifier = {};
    std::array<ShotModifier, ClubID::Count> flopModifier = {};
};


static inline const std::string StructData = 
R"(struct ShotModifier final
{
    float angle = 0.f;
    float powerMultiplier = 1.f;
    float targetMultiplier = 1.f;
    ShotModifier() = default;
    constexpr ShotModifier(float a, float p, float t)
        : angle(a), powerMultiplier(p), targetMultiplier(t){}
};

struct ModifierGroup final
{
    const std::array<ShotModifier, ClubID::Count> DefaultModifier = {}; //leaves the default shot
    std::array<ShotModifier, ClubID::Count> punchModifier = {};
    std::array<ShotModifier, ClubID::Count> flopModifier = {};

    ModifierGroup() = default;
    constexpr ModifierGroup(const std::array<ShotModifier, ClubID::Count>& p, const std::array<ShotModifier, ClubID::Count>& f)
        : punchModifier(p), flopModifier(f) {}
};

)";


static inline const std::string ArrayData = 
R"(static constexpr std::array<ModifierGroup, 3u> LevelModifiers =
{
    ModifierGroup(pA, fA),
    ModifierGroup(pB, fB),
    ModifierGroup(pC, fC)
};
)";

static inline constexpr std::array<float, ClubID::Count> SideSpin =
{ 0.3f, 0.35f, 0.45f, 0.45f, 0.5f, 0.55f, 0.6f, 0.75f, 0.8f, 0.05f, 0.05f, 0.05f, 0.f };

static inline constexpr std::array<float, ClubID::Count> TopSpin =
{ 0.5f, 0.55f, 0.55f, 0.78f, 0.78f, 0.8f, 0.8f, 0.85f, 0.85f, 0.9f, 0.93f, 0.95f, 0.f };

static inline const std::array<std::string, ClubID::Count> ClubStrings =
{
    "Club(ClubID::Driver,    \"Driver \", ",
    "Club(ClubID::ThreeWood, \"3 Wood \", ",
    "Club(ClubID::FiveWood,  \"5 Wood \", ",


    "Club(ClubID::FourIron,  \"4 Iron \", ",
    "Club(ClubID::FiveIron,  \"5 Iron \", ",
    "Club(ClubID::SixIron,   \"6 Iron \", ",
    "Club(ClubID::SevenIron, \"7 Iron \", ",
    "Club(ClubID::EightIron, \"8 Iron \", ",
    "Club(ClubID::NineIron,  \"9 Iron \", ",


    "Club(ClubID::PitchWedge, \"Pitch Wedge \", ",
    "Club(ClubID::GapWedge,   \"Gap Wedge \",   ",
    "Club(ClubID::SandWedge,  \"Sand Wedge \",  ",
    "Club(ClubID::Putter,     \"Putter \",      "
};



static inline void writeHeader(const std::array<ModifierGroup, 3>& data,
                                const std::array<Club, ClubID::Count>& clubs,
                                const std::array<ClubStat, ClubID::Count>& clubStats)
{
    static const std::array<std::string, 3u> PunchStrings =
    {
        "pA", "pB", "pC"
    };
    static const std::array<std::string, 3u> FlopStrings =
    {
        "fA", "fB", "fC"
    };

    std::ofstream outFile("ClubModifiers.hpp");
    if (outFile.is_open() && outFile.good())
    {
        outFile << "#include \"Clubs.hpp\"\n\n";
        outFile << StructData;

        outFile.precision(3);
        outFile.setf(std::ios::fixed);

        //flop / punch modifiers

        //iterate over data
        std::int32_t i = 0;
        for (const auto& group : data)
        {
            outFile << "static constexpr std::array<ShotModifier, ClubID::Count> " << PunchStrings[i] << " = \n{\n";
            for (const auto& mod : group.punchModifier)
            {
                outFile << "    ShotModifier("<< mod.angle << "f, " << mod.powerMultiplier << "f, " << mod.targetMultiplier << "f),\n";
            }

            outFile << "};\n\nstatic constexpr std::array<ShotModifier, ClubID::Count> " << FlopStrings[i++] << " = \n{\n";
            for (const auto& mod : group.flopModifier)
            {
                outFile << "    ShotModifier(" << mod.angle << "f, " << mod.powerMultiplier << "f, " << mod.targetMultiplier << "f),\n";
            }
            outFile << "};\n\n";
        }

        outFile << ArrayData;
        outFile << "/*\n";

        //club set data
        i = 0;
        outFile << "\n\nstatic inline const std::array<Club, ClubID::Count> Clubs =\n{";
        for (const auto& club : clubs)
        {
            outFile << "\n    " << ClubStrings[i] << (clubs[i].angle * cro::Util::Const::radToDeg) << "f, " << SideSpin[i] << "f, " << TopSpin[i] << "f),";
            i++;
        }
        outFile << "\n};";

        //clubstats
        i = 0;
        outFile << "\n\nstatic inline constexpr std::array<ClubStat, ClubID::Count> ClubStats = \n{";
        for (const auto& stat : clubStats)
        {
            outFile << "\n    ClubStat({ " << clubStats[i].stats[0].power << "f, " << clubStats[i].stats[0].target << "f }, ";
            outFile << "{ " << clubStats[i].stats[1].power << "f, " << clubStats[i].stats[1].target << "f }, ";
            outFile << "{ " << clubStats[i].stats[2].power << "f, " << clubStats[i].stats[2].target << "f }),";

            i++;
        }
        outFile << "\n};\n*/";
    }
}
