// Regression tests: values live around a loop keep their registers.
//
// The register allocator walks the program path by path, and stops a path
// at a branch the path has already taken.  A loop's second trip was never
// walked past its first inner branch, so a value written in one trip and
// read after that branch in the next lost its register there, and a later
// temporary could take it.  Values loaded before a --cont loop and read
// after an inner branch lost theirs the same way, which Tyra's programs do.
//
// A write to some lanes of a variable joined the variable's earlier value
// whenever another lane had been written before, even when that lane was
// written again before any read.  Values rewritten lane by lane on every
// trip, like Tyra's output colours, stayed live for the whole program.

#include "test_harness.h"
#include "openvcl_runner.h"

#include <sstream>
#include <string>
#include <vector>

namespace
{
    // iPtr is read after the inner branch, then advanced for the next trip.
    const char* kLoopPointer =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmLoopLiveness\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu iPtr, vi00, 100\n"
        "\tiaddiu iCount, vi00, 10\n"
        "lLoop:\n"
        "\tilw.x iCond, 0(iCount)\n"
        "\tibeq iCond, vi00, lSkip\n"
        "\tiaddiu iTemp, vi00, 5\n"
        "\tisw.x iTemp, 1(vi00)\n"
        "lSkip:\n"
        "\tisw.x iCount, 0(iPtr)\n"
        "\tiaddiu iPtr, iPtr, 1\n"
        "\tiaddiu iT2, vi00, 7\n"
        "\tiaddiu iT3, iT2, 1\n"
        "\tisw.x iT3, 2(vi00)\n"
        "\tiaddi iCount, iCount, -1\n"
        "\tibne iCount, vi00, lLoop\n"
        "\t--exit\n"
        "\t--endexit\n";

    // fTag is loaded once; each call continued after --cont runs from begin
    // again, and stores fTag after the inner branch.
    const char* kContinuedLoop =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmLoopLiveness\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tlq fTag, 0(vi00)\n"
        "\tilw.x iMode, 1(vi00)\n"
        "begin:\n"
        "\txtop iBuf\n"
        "\tiblez iMode, lColor\n"
        "\tlq fC, 0(iBuf)\n"
        "\tb lStore\n"
        "lColor:\n"
        "\tlq fC, 1(iBuf)\n"
        "lStore:\n"
        "\tsq fTag, 2(iBuf)\n"
        "\tlq fA, 3(iBuf)\n"
        "\tadd fB, fA, fC\n"
        "\tsq fB, 4(iBuf)\n"
        "\txgkick iBuf\n"
        "\t--barrier\n"
        "\t--cont\n"
        "\tb begin\n"
        "\t--exit\n"
        "\t--endexit\n";

    // iVal is set in the first loop and read in every trip of the second.
    const char* kSiblingLoops =
        "\t.init_vi vi01, vi02, vi03, vi04\n"
        "\t.init_vf_all\n"
        "\t.syntax new\n"
        "\t.name vsmLoopLiveness\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu iCount, vi00, 3\n"
        "lA:\n"
        "\tiaddiu iVal, iCount, 10\n"
        "\tiaddi iCount, iCount, -1\n"
        "\tibne iCount, vi00, lA\n"
        "\tiaddiu iCount2, vi00, 3\n"
        "lB:\n"
        "\tilw.x iCond, 0(iCount2)\n"
        "\tibeq iCond, vi00, lBSkip\n"
        "\tiaddiu iTmp, vi00, 1\n"
        "\tisw.x iTmp, 0(vi00)\n"
        "lBSkip:\n"
        "\tisw.x iVal, 1(vi00)\n"
        "\tiaddiu iTmp2, vi00, 5\n"
        "\tiaddiu iTmp3, iTmp2, 5\n"
        "\tisw.x iTmp3, 2(vi00)\n"
        "\tiaddi iCount2, iCount2, -1\n"
        "\tibne iCount2, vi00, lB\n"
        "\t--exit\n"
        "\t--endexit\n";

    // mulw reads only the w lane of fT, which mul.w has just written: fT is
    // not live around the loop, and three VF registers are enough.
    const char* kBroadcastLane =
        "\t.init_vf vf01, vf02, vf03\n"
        "\t.init_vi_all\n"
        "\t.name vsmLoopLiveness\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu iN, vi00, 4\n"
        "\tlq fA, 0(vi00)\n"
        "lLoop:\n"
        "\tlq fB, 1(vi00)\n"
        "\tmul.w fT, fB, fB\n"
        "\tmulw.xyz fC, fA, fT\n"
        "\tsq.xyz fC, 2(vi00)\n"
        "\tlq fD, 3(vi00)\n"
        "\tadd fE, fD, fA\n"
        "\tsq fE, 4(vi00)\n"
        "\tiaddi iN, iN, -1\n"
        "\tibne iN, vi00, lLoop\n"
        "\t--exit\n"
        "\t--endexit\n";

    // Each trip writes fC's xyz and then its w before storing it, so the last
    // trip's fC is dead by then, and four VF registers are enough.
    const char* kPartialWrites =
        "\t.init_vf vf01, vf02, vf03, vf04\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmLoopLiveness\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu iN, vi00, 4\n"
        "\tlq fA, 0(vi00)\n"
        "lLoop:\n"
        "\tlq fB, 1(vi00)\n"
        "\tadd.xyz fC, fA, fB\n"
        "\tadd.w fC, fB, fB\n"
        "\tsq fC, 2(vi00)\n"
        "\tlq fD, 3(vi00)\n"
        "\tadd fE, fD, fA\n"
        "\tmul fF, fE, fD\n"
        "\tsq fF, 4(vi00)\n"
        "\tiaddi iN, iN, -1\n"
        "\tibne iN, vi00, lLoop\n"
        "\t--exit\n"
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

    // The first instruction with this opcode that contains `part`.
    std::string find(const std::vector<std::string>& code, const std::string& opcode, const std::string& part)
    {
        for (std::vector<std::string>::const_iterator i = code.begin(); i != code.end(); ++i)
        {
            if (i->compare(0, opcode.size() + 1, opcode + " ") == 0 && i->find(part) != std::string::npos)
                return *i;
        }
        return std::string();
    }

    // The register an instruction names first: what it writes, or what a
    // store stores.
    std::string firstRegister(const std::string& instruction)
    {
        const std::string::size_type space = instruction.find(' ');
        return space == std::string::npos ? std::string() : instruction.substr(space + 1, 4);
    }

    // The register of an address operand, as in 0(VI01).
    std::string addressRegister(const std::string& instruction)
    {
        const std::string::size_type open = instruction.rfind('(');
        return open == std::string::npos ? std::string() : instruction.substr(open + 1, 4);
    }

    std::vector<std::string> mentioning(const std::vector<std::string>& code, const std::string& reg)
    {
        std::vector<std::string> result;
        for (std::vector<std::string>::const_iterator i = code.begin(); i != code.end(); ++i)
        {
            if (i->find(reg) != std::string::npos)
                result.push_back(*i);
        }
        return result;
    }
}

TEST_CASE("openvcl: a pointer carried around a loop keeps its register past an inner branch")
{
    ::test::RunResult result = compile(kLoopPointer);
    REQUIRE(result.exit_code == 0);
    const std::vector<std::string> code = instructions(result.stdout_data);

    // isw.x iCount, 0(iPtr), and iaddiu iT2, vi00, 7
    const std::string pointer = addressRegister(find(code, "isw.x", ", 0("));
    const std::string temporary = firstRegister(find(code, "iaddiu", ", VI00, 7"));
    REQUIRE(pointer.size() == 4);
    REQUIRE(temporary.size() == 4);

    CHECK(temporary != pointer);
    CHECK(!find(code, "iaddiu", pointer + ", " + pointer + ", 1").empty());
}

TEST_CASE("openvcl: a value loaded before a --cont loop keeps its register through the loop")
{
    ::test::RunResult result = compile(kContinuedLoop);
    REQUIRE(result.exit_code == 0);
    const std::vector<std::string> code = instructions(result.stdout_data);

    // only lq fTag and sq fTag name its register
    const std::string tag = firstRegister(find(code, "lq", ", 0(VI00)"));
    REQUIRE(tag.size() == 4);

    const std::vector<std::string> uses = mentioning(code, tag);
    REQUIRE(uses.size() == 2);
    CHECK(uses[0].compare(0, 3, "lq ") == 0);
    CHECK(uses[1].compare(0, 3, "sq ") == 0);
}

TEST_CASE("openvcl: a value set in one loop keeps its register through a sibling loop")
{
    ::test::RunResult result = compile(kSiblingLoops);
    REQUIRE(result.exit_code == 0);
    const std::vector<std::string> code = instructions(result.stdout_data);

    // only iaddiu iVal and isw.x iVal name its register
    const std::string value = firstRegister(find(code, "iaddiu", ", 10"));
    REQUIRE(value.size() == 4);

    const std::vector<std::string> uses = mentioning(code, value);
    REQUIRE(uses.size() == 2);
    CHECK(uses[0].compare(0, 7, "iaddiu ") == 0);
    CHECK(uses[1].compare(0, 6, "isw.x ") == 0);
}

TEST_CASE("openvcl: a broadcast operand keeps only its lane live")
{
    ::test::RunResult result = compile(kBroadcastLane);
    CHECK(result.exit_code == 0);
}

TEST_CASE("openvcl: a partial write starts a new value when the lanes it keeps are dead")
{
    ::test::RunResult result = compile(kPartialWrites);
    CHECK(result.exit_code == 0);
}
