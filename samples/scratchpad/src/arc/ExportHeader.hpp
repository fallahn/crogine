#pragma once

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

)";


static inline const std::string ArrayData = 
R"(static constexpr std::array<ModifierGroup, 3u> LevelModifiers =
{
    ModifierGroup(pA, fA),
    ModifierGroup(pB, fB),
    ModifierGroup(pC, fC)
};
)";


static inline void writeHeader(const std::array<ModifierGroup, 3>& data)
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

        //iterate over data
        std::int32_t i = 0;
        for (const auto& group : data)
        {
            outFile << "static constexpr std::array<ShotModifier, ClubID::Count> " << PunchStrings[i] << " = \n{\n";
            for (const auto& mod : group.punchModifier)
            {
                outFile << "    ShotModifier("<< mod.angle << "f, " << mod.powerMultiplier << "f, " << mod.targetMultiplier << "f),\n";
            }

            outFile << "}\n\nstatic constexpr std::array<ShotModifier, ClubID::Count> " << FlopStrings[i++] << " = \n{\n";
            for (const auto& mod : group.flopModifier)
            {
                outFile << "    ShotModifier(" << mod.angle << "f, " << mod.powerMultiplier << "f, " << mod.targetMultiplier << ")f,\n";
            }
            outFile << "}\n\n";
        }

        outFile << ArrayData;
    }
}
