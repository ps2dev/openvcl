#include "test_harness.h"

#include "../../src/VuModuloFeasibilitySolver.h"

using namespace vcl;

TEST_CASE("Modulo feasibility: finds simple schedule")
{
    std::vector<VuModuloFeasibilityNode> nodes( 2 );
    nodes[0].pipe = 1; nodes[0].lo = 0; nodes[0].hi = 1;
    nodes[1].pipe = 2; nodes[1].lo = 0; nodes[1].hi = 2;
    VuModuloFeasibilityEdge edge;
    edge.from = 0; edge.to = 1; edge.latency = 1;
    std::vector<VuModuloFeasibilityEdge> edges( 1, edge );

    const VuModuloFeasibilityResult result = solveVuModuloFeasibility( 2, nodes, edges, 100 );
    REQUIRE( result.status == VuModuloFeasibilityResult::SAT );
    REQUIRE( result.slots.size() == 2u );
    REQUIRE( result.slots[1] >= result.slots[0] + 1u );
}

TEST_CASE("Modulo feasibility: rejects resource collision")
{
    std::vector<VuModuloFeasibilityNode> nodes( 2 );
    nodes[0].pipe = 2; nodes[0].lo = 0; nodes[0].hi = 0;
    nodes[1].pipe = 2; nodes[1].lo = 0; nodes[1].hi = 0;
    std::vector<VuModuloFeasibilityEdge> edges;

    const VuModuloFeasibilityResult result = solveVuModuloFeasibility( 1, nodes, edges, 100 );
    REQUIRE( result.status == VuModuloFeasibilityResult::UNSAT );
}

TEST_CASE("Modulo feasibility: enforces carried self edge")
{
    std::vector<VuModuloFeasibilityNode> nodes( 1 );
    nodes[0].pipe = 1; nodes[0].lo = 0; nodes[0].hi = 2;
    VuModuloFeasibilityEdge edge;
    edge.from = 0; edge.to = 0; edge.distance = 1; edge.latency = 2;
    std::vector<VuModuloFeasibilityEdge> edges( 1, edge );

    const VuModuloFeasibilityResult result = solveVuModuloFeasibility( 1, nodes, edges, 100 );
    REQUIRE( result.status == VuModuloFeasibilityResult::UNSAT );
}

