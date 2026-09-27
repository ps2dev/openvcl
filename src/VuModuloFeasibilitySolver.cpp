#include "VuModuloFeasibilitySolver.h"

#include <algorithm>
#include <deque>
#include <set>
#include <utility>

namespace vcl
{
namespace
{

struct PairEdge
{
    unsigned int from;
    unsigned int to;
    int bound;
};

typedef std::vector<unsigned int> Domain;
typedef std::vector<Domain> Domains;

class Solver
{
public:
    Solver( unsigned int ii,
            const std::vector<VuModuloFeasibilityNode>& nodes,
            const std::vector<VuModuloFeasibilityEdge>& edges,
            unsigned int maxStates,
            bool useGlobalResourceMatching )
        : m_ii( ii ),
          m_nodes( nodes ),
          m_maxStates( maxStates ),
          m_useGlobalResourceMatching( useGlobalResourceMatching ),
          m_states( 0 ),
          m_revisions( 0 ),
          m_exhausted( false )
    {
        const unsigned int n = static_cast<unsigned int>( nodes.size() );
        m_pairEdges.resize( n * n );
        m_neighbours.resize( n );

        for( unsigned int e = 0; e < edges.size(); ++e )
        {
            if( edges[e].from >= n || edges[e].to >= n ) continue;
            PairEdge edge;
            edge.from = edges[e].from;
            edge.to = edges[e].to;
            edge.bound = static_cast<int>( edges[e].latency )
                       - static_cast<int>( edges[e].distance * ii );
            m_pairEdges[pairIndex( edge.from, edge.to )].push_back( edge );
            if( edge.from != edge.to )
            {
                m_neighbours[edge.from].insert( edge.to );
                m_neighbours[edge.to].insert( edge.from );
            }
        }

        for( unsigned int a = 0; a < n; ++a )
        {
            if( nodes[a].pipe == 0 ) continue;
            for( unsigned int b = a + 1; b < n; ++b )
            {
                if( nodes[a].pipe != nodes[b].pipe ) continue;
                m_neighbours[a].insert( b );
                m_neighbours[b].insert( a );
            }
        }
    }

    VuModuloFeasibilityResult solve()
    {
        VuModuloFeasibilityResult result;
        Domains domains( m_nodes.size() );
        for( unsigned int node = 0; node < m_nodes.size(); ++node )
        {
            if( m_nodes[node].hi < m_nodes[node].lo )
            {
                result.status = VuModuloFeasibilityResult::UNSAT;
                return result;
            }
            for( unsigned int slot = m_nodes[node].lo; slot <= m_nodes[node].hi; ++slot )
            {
                if( compatible( node, slot, node, slot ) )
                    domains[node].push_back( slot );
                if( slot == static_cast<unsigned int>( -1 ) ) break;
            }
            if( domains[node].empty() )
            {
                result.status = VuModuloFeasibilityResult::UNSAT;
                return result;
            }
        }

        std::deque< std::pair<unsigned int, unsigned int> > queue;
        for( unsigned int node = 0; node < m_neighbours.size(); ++node )
            for( std::set<unsigned int>::const_iterator other = m_neighbours[node].begin();
                 other != m_neighbours[node].end(); ++other )
                queue.push_back( std::make_pair( node, *other ) );
        if( !propagate( domains, queue ) )
        {
            result.status = VuModuloFeasibilityResult::UNSAT;
            result.revisions = m_revisions;
            return result;
        }

        std::vector<unsigned int> slots;
        const bool found = search( domains, slots );
        result.states = m_states;
        result.revisions = m_revisions;
        if( found )
        {
            result.status = VuModuloFeasibilityResult::SAT;
            result.slots.swap( slots );
        }
        else if( m_exhausted )
            result.status = VuModuloFeasibilityResult::UNKNOWN;
        else
            result.status = VuModuloFeasibilityResult::UNSAT;
        return result;
    }

private:
    unsigned int pairIndex( unsigned int first, unsigned int second ) const
    {
        if( first > second ) std::swap( first, second );
        return first * static_cast<unsigned int>( m_nodes.size() ) + second;
    }

    bool resourceCompatible( unsigned int first,
                             unsigned int firstSlot,
                             unsigned int second,
                             unsigned int secondSlot ) const
    {
        if( first == second ) return true;
        const VuModuloFeasibilityNode& a = m_nodes[first];
        const VuModuloFeasibilityNode& b = m_nodes[second];
        if( a.pipe == 0 || a.pipe != b.pipe ) return true;
        if( a.pipe == 1 || a.pipe == 2 )
            return ( firstSlot % m_ii ) != ( secondSlot % m_ii );
        if( a.pipe == 3 || a.pipe == 4 )
        {
            for( unsigned int da = 0; da < a.duration; ++da )
                for( unsigned int db = 0; db < b.duration; ++db )
                    if( ( firstSlot + da ) % m_ii == ( secondSlot + db ) % m_ii )
                        return false;
        }
        return true;
    }

    bool compatible( unsigned int first,
                     unsigned int firstSlot,
                     unsigned int second,
                     unsigned int secondSlot ) const
    {
        if( !resourceCompatible( first, firstSlot, second, secondSlot ) )
            return false;
        const std::vector<PairEdge>& pair = m_pairEdges[pairIndex( first, second )];
        for( unsigned int e = 0; e < pair.size(); ++e )
        {
            const int fromSlot = pair[e].from == first
                               ? static_cast<int>( firstSlot )
                               : static_cast<int>( secondSlot );
            const int toSlot = pair[e].to == first
                             ? static_cast<int>( firstSlot )
                             : static_cast<int>( secondSlot );
            if( toSlot - fromSlot < pair[e].bound )
                return false;
        }
        return true;
    }

    bool revise( Domains& domains, unsigned int first, unsigned int second )
    {
        Domain kept;
        kept.reserve( domains[first].size() );
        bool changed = false;
        for( unsigned int a = 0; a < domains[first].size(); ++a )
        {
            bool supported = false;
            for( unsigned int b = 0; b < domains[second].size() && !supported; ++b )
                supported = compatible( first, domains[first][a], second, domains[second][b] );
            if( supported ) kept.push_back( domains[first][a] );
            else
            {
                changed = true;
                ++m_revisions;
            }
        }
        if( changed ) domains[first].swap( kept );
        return changed;
    }

    bool propagate( Domains& domains,
                    std::deque< std::pair<unsigned int, unsigned int> >& queue )
    {
        while( !queue.empty() )
        {
            const unsigned int first = queue.front().first;
            const unsigned int second = queue.front().second;
            queue.pop_front();
            if( !revise( domains, first, second ) ) continue;
            if( domains[first].empty() ) return false;
            for( std::set<unsigned int>::const_iterator other = m_neighbours[first].begin();
                 other != m_neighbours[first].end(); ++other )
                if( *other != second )
                    queue.push_back( std::make_pair( *other, first ) );
        }
        return true;
    }

    bool augmentResourceMatching( unsigned int node,
                                  const Domains& domains,
                                  std::vector<int>& residueOwner,
                                  std::vector<bool>& seenResidues ) const
    {
        std::vector<bool> triedResidues( m_ii, false );
        for( unsigned int v = 0; v < domains[node].size(); ++v )
        {
            const unsigned int residue = domains[node][v] % m_ii;
            if( triedResidues[residue] || seenResidues[residue] ) continue;
            triedResidues[residue] = true;
            seenResidues[residue] = true;
            if( residueOwner[residue] < 0
                || augmentResourceMatching( static_cast<unsigned int>( residueOwner[residue] ),
                                            domains,
                                            residueOwner,
                                            seenResidues ) )
            {
                residueOwner[residue] = static_cast<int>( node );
                return true;
            }
        }
        return false;
    }

    bool resourceMatchingFeasible( const Domains& domains, unsigned int pipe ) const
    {
        std::vector<unsigned int> pipeNodes;
        for( unsigned int node = 0; node < m_nodes.size(); ++node )
            if( m_nodes[node].pipe == pipe ) pipeNodes.push_back( node );
        if( pipeNodes.size() > m_ii ) return false;

        // Upper/lower instructions consume exactly one modulo residue. Pairwise
        // AC-3 enforces unequal residues two nodes at a time, but cannot detect a
        // Hall-set conflict such as three nodes whose domains contain only two
        // residues. A bipartite matching is a necessary global resource check and
        // therefore only prunes branches that cannot possibly satisfy the MRT.
        std::sort( pipeNodes.begin(), pipeNodes.end(), DomainSizeLess( domains ) );
        std::vector<int> residueOwner( m_ii, -1 );
        for( unsigned int i = 0; i < pipeNodes.size(); ++i )
        {
            std::vector<bool> seenResidues( m_ii, false );
            if( !augmentResourceMatching( pipeNodes[i], domains, residueOwner, seenResidues ) )
                return false;
        }
        return true;
    }

    struct DomainSizeLess
    {
        explicit DomainSizeLess( const Domains& domains ) : domains( domains ) {}
        bool operator()( unsigned int first, unsigned int second ) const
        {
            if( domains[first].size() != domains[second].size() )
                return domains[first].size() < domains[second].size();
            return first < second;
        }
        const Domains& domains;
    };

    bool globalResourceFeasible( const Domains& domains ) const
    {
        return resourceMatchingFeasible( domains, 1u )
            && resourceMatchingFeasible( domains, 2u );
    }

    unsigned int chooseNode( const Domains& domains ) const
    {
        unsigned int best = static_cast<unsigned int>( -1 );
        for( unsigned int node = 0; node < domains.size(); ++node )
        {
            if( domains[node].size() <= 1 ) continue;
            if( best == static_cast<unsigned int>( -1 )
                || domains[node].size() < domains[best].size()
                || ( domains[node].size() == domains[best].size()
                     && m_neighbours[node].size() > m_neighbours[best].size() ) )
                best = node;
        }
        return best;
    }

    struct ValuePressure
    {
        unsigned int slot;
        unsigned int support;
    };

    static bool valuePressureLess( const ValuePressure& a, const ValuePressure& b )
    {
        if( a.support != b.support ) return a.support > b.support;
        return a.slot < b.slot;
    }

    std::vector<unsigned int> orderedValues( const Domains& domains, unsigned int node ) const
    {
        std::vector<ValuePressure> values;
        for( unsigned int v = 0; v < domains[node].size(); ++v )
        {
            ValuePressure pressure;
            pressure.slot = domains[node][v];
            pressure.support = 0;
            for( std::set<unsigned int>::const_iterator other = m_neighbours[node].begin();
                 other != m_neighbours[node].end(); ++other )
                for( unsigned int o = 0; o < domains[*other].size(); ++o )
                    if( compatible( node, pressure.slot, *other, domains[*other][o] ) )
                        ++pressure.support;
            values.push_back( pressure );
        }
        std::sort( values.begin(), values.end(), valuePressureLess );
        std::vector<unsigned int> ordered;
        for( unsigned int v = 0; v < values.size(); ++v ) ordered.push_back( values[v].slot );
        return ordered;
    }

    bool search( const Domains& current, std::vector<unsigned int>& slots )
    {
        if( m_states >= m_maxStates )
        {
            m_exhausted = true;
            return false;
        }
        ++m_states;
        if( m_useGlobalResourceMatching && !globalResourceFeasible( current ) ) return false;
        const unsigned int node = chooseNode( current );
        if( node == static_cast<unsigned int>( -1 ) )
        {
            slots.resize( current.size() );
            for( unsigned int i = 0; i < current.size(); ++i ) slots[i] = current[i][0];
            return true;
        }

        const std::vector<unsigned int> values = orderedValues( current, node );
        for( unsigned int v = 0; v < values.size(); ++v )
        {
            Domains child = current;
            child[node].assign( 1, values[v] );
            std::deque< std::pair<unsigned int, unsigned int> > queue;
            for( std::set<unsigned int>::const_iterator other = m_neighbours[node].begin();
                 other != m_neighbours[node].end(); ++other )
                queue.push_back( std::make_pair( *other, node ) );
            if( propagate( child, queue ) && search( child, slots ) ) return true;
            if( m_exhausted ) return false;
        }
        return false;
    }

    unsigned int m_ii;
    const std::vector<VuModuloFeasibilityNode>& m_nodes;
    unsigned int m_maxStates;
    bool m_useGlobalResourceMatching;
    unsigned int m_states;
    unsigned int m_revisions;
    bool m_exhausted;
    std::vector< std::vector<PairEdge> > m_pairEdges;
    std::vector< std::set<unsigned int> > m_neighbours;
};

} // namespace

VuModuloFeasibilityResult solveVuModuloFeasibility(
    unsigned int initiationInterval,
    const std::vector<VuModuloFeasibilityNode>& nodes,
    const std::vector<VuModuloFeasibilityEdge>& edges,
    unsigned int maxStates,
    bool useGlobalResourceMatching )
{
    if( initiationInterval == 0 || nodes.empty() || maxStates == 0 )
        return VuModuloFeasibilityResult();
    Solver solver( initiationInterval, nodes, edges, maxStates, useGlobalResourceMatching );
    return solver.solve();
}

} // namespace vcl
