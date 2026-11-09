// Regression tests: a block's outputs leave in the registers its --exit
// names.
//
// The register allocator walks the program path by path.  The path that
// takes a conditional branch is a copy of the state before it, and the copy
// was made without the block's outputs, so a value assigned before the
// branch never got its out_vf register on that path.  And an output's live
// range ended at its last access instead of at the exit, so a temporary
// could take its register before the program ended.

#include "test_harness.h"
#include "openvcl_runner.h"

#include <sstream>
#include <string>
#include <vector>

namespace
{
    // fOut is assigned before the branch, and again on the path that does
    // not take it.
    const char* kBranchedOutput =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmExitOutputs\n"
        "\t--enter\n"
        "\tin_vi iFlag(vi01)\n"
        "\tin_vf fA(vf01)\n"
        "\t--endenter\n"
        "\tmove fOut, fA\n"
        "\tibeq iFlag, vi00, lDone\n"
        "\tadd fOut, fA, fA\n"
        "lDone:\n"
        "\t--exit\n"
        "\tout_vf fOut(vf05)\n"
        "\t--endexit\n";

    // fT is needed after fOut's last access, while fOut waits for the exit.
    const char* kOutputBeforeTemporary =
        "\t.init_vf vf01, vf05, vf06\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmExitOutputs\n"
        "\t--enter\n"
        "\tin_vf fA(vf01)\n"
        "\t--endenter\n"
        "\tadd fOut, fA, fA\n"
        "\tadd fT, fA, fA\n"
        "\tsq fT, 0(vi00)\n"
        "\t--exit\n"
        "\tout_vf fOut(vf05)\n"
        "\t--endexit\n";

    const char* kIntegerOutputBeforeTemporary =
        "\t.init_vf_all\n"
        "\t.init_vi vi01, vi02, vi03\n"
        "\t.syntax new\n"
        "\t.name vsmExitOutputs\n"
        "\t--enter\n"
        "\tin_vi iA(vi01)\n"
        "\t--endenter\n"
        "\tiaddiu iOut, iA, 1\n"
        "\tiaddiu iT, iA, 2\n"
        "\tisw.x iT, 0(vi00)\n"
        "\t--exit\n"
        "\tout_vi iOut(vi02)\n"
        "\t--endexit\n";

    // fA goes from the entry to the exit untouched, in VF01.
    const char* kPassedThrough =
        "\t.init_vf vf01, vf02\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmExitOutputs\n"
        "\t--enter\n"
        "\tin_vf fA(vf01)\n"
        "\t--endenter\n"
        "\tlq fT, 0(vi00)\n"
        "\tsq fT, 1(vi00)\n"
        "\t--exit\n"
        "\tout_vf fA(vf01)\n"
        "\t--endexit\n";

    // fA enters in VF01 and is wanted in VF05 at the exit; nothing moves it.
    const char* kMismatchedRegisters =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmExitOutputs\n"
        "\t--enter\n"
        "\tin_vf fA(vf01)\n"
        "\t--endenter\n"
        "\tlq fT, 0(vi00)\n"
        "\tsq fT, 1(vi00)\n"
        "\t--exit\n"
        "\tout_vf fA(vf05)\n"
        "\t--endexit\n";

    ::test::RunResult compile(const char* source)
    {
        return ::test::run_openvcl(std::vector<std::string>(), source);
    }

    // The emitted instructions, upper and lower halves apart: code lines are
    // indented by 20 spaces, and padding separates the two halves.
    std::vector<std::string> instructions(const std::string& vsm)
    {
        std::vector<std::string> result;
        std::stringstream stream(vsm);
        std::string line;
        while (std::getline(stream, line))
        {
            if (line.compare(0, 20, std::string(20, ' ')) != 0)
                continue;
            std::string::size_type start = line.find_first_not_of(' ');
            while (start != std::string::npos)
            {
                const std::string::size_type end = line.find("  ", start);
                result.push_back(line.substr(start, end == std::string::npos ? std::string::npos : end - start));
                start = (end == std::string::npos) ? end : line.find_first_not_of(' ', end);
            }
        }
        return result;
    }

    // The first instruction with this opcode.
    std::string find(const std::vector<std::string>& code, const std::string& opcode)
    {
        for (std::vector<std::string>::const_iterator i = code.begin(); i != code.end(); ++i)
        {
            if (i->compare(0, opcode.size() + 1, opcode + " ") == 0)
                return *i;
        }
        return std::string();
    }

    bool contains(const std::vector<std::string>& code, const std::string& instruction)
    {
        for (std::vector<std::string>::const_iterator i = code.begin(); i != code.end(); ++i)
        {
            if (*i == instruction)
                return true;
        }
        return false;
    }

    // The register an instruction names first: what it writes, or what a
    // store stores.
    std::string firstRegister(const std::string& instruction)
    {
        const std::string::size_type space = instruction.find(' ');
        return space == std::string::npos ? std::string() : instruction.substr(space + 1, 4);
    }
}

TEST_CASE("openvcl: an output assigned before a taken branch gets its register")
{
    ::test::RunResult result = compile(kBranchedOutput);
    REQUIRE(result.exit_code == 0);

    const std::vector<std::string> code = instructions(result.stdout_data);
    CHECK(contains(code, "move VF05, VF01"));
    CHECK(contains(code, "add.xyzw VF05, VF01, VF01"));
}

TEST_CASE("openvcl: an output keeps its register until the exit")
{
    ::test::RunResult result = compile(kOutputBeforeTemporary);
    REQUIRE(result.exit_code == 0);

    std::vector<std::string> code = instructions(result.stdout_data);
    CHECK(contains(code, "add.xyzw VF05, VF01, VF01"));
    CHECK(firstRegister(find(code, "sq")) == "VF06");

    result = compile(kIntegerOutputBeforeTemporary);
    REQUIRE(result.exit_code == 0);

    code = instructions(result.stdout_data);
    CHECK(contains(code, "iaddiu VI02, VI01, 1"));
    CHECK(firstRegister(find(code, "isw.x")) == "VI03");
}

TEST_CASE("openvcl: an input passed through to an output keeps its register")
{
    ::test::RunResult result = compile(kPassedThrough);
    REQUIRE(result.exit_code == 0);

    const std::vector<std::string> code = instructions(result.stdout_data);
    CHECK(firstRegister(find(code, "lq")) == "VF02");
    CHECK(firstRegister(find(code, "sq")) == "VF02");
}

TEST_CASE("openvcl: an output that wants an input's value in another register is an error")
{
    ::test::RunResult result = compile(kMismatchedRegisters);
    CHECK(result.exit_code != 0);
    CHECK(result.stderr_data.find("error: Register conflict: fA has to be in both VF01 and VF05") != std::string::npos);
}
