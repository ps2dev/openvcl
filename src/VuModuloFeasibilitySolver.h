#ifndef OPENVCL_VU_MODULO_FEASIBILITY_SOLVER_H
#define OPENVCL_VU_MODULO_FEASIBILITY_SOLVER_H

#include <vector>

namespace vcl
{

struct VuModuloFeasibilityNode
{
    unsigned int pipe;
    unsigned int duration;
    unsigned int lo;
    unsigned int hi;

    VuModuloFeasibilityNode()
        : pipe( 0 ), duration( 1 ), lo( 0 ), hi( 0 ) {}
};

struct VuModuloFeasibilityEdge
{
    unsigned int from;
    unsigned int to;
    unsigned int distance;
    unsigned int latency;

    VuModuloFeasibilityEdge()
        : from( 0 ), to( 0 ), distance( 0 ), latency( 0 ) {}
};

struct VuModuloFeasibilityResult
{
    enum Status
    {
        SAT,
        UNSAT,
        UNKNOWN
    };

    Status status;
    std::vector<unsigned int> slots;
    unsigned int states;
    unsigned int revisions;

    VuModuloFeasibilityResult()
        : status( UNKNOWN ), states( 0 ), revisions( 0 ) {}
};

VuModuloFeasibilityResult solveVuModuloFeasibility(
    unsigned int initiationInterval,
    const std::vector<VuModuloFeasibilityNode>& nodes,
    const std::vector<VuModuloFeasibilityEdge>& edges,
    unsigned int maxStates );

} // namespace vcl

#endif
