/*
 * RegisterAllocator.cpp
 *
 * Copyright (C) 2004 Jesper Svennevid, Daniel Collin
 *
 * Licensed under the AFL v2.0. See the file LICENSE included with this
 * distribution for licensing terms.
 *
 */


#include "RegisterAllocator.h"
#include "BranchState.h"
#include "Error.h"
#include "VuTokenResourceAccess.h"

#include <iostream>
#include <iomanip>
#include <set>
#include <vector>
#include <stdlib.h>
#include <assert.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace vcl
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RegisterAllocator::RegisterAllocator()
{
	unsigned int i;

	for( i = 0; i < sizeof(m_floats)/sizeof(Register); i++ )
	{
		std::stringstream s;
		s << "VF" << std::setw(2) << std::setfill('0') << i;
		m_floats[i].setName( s.str() );
	}

	for( i = 0; i < sizeof(m_integers)/sizeof(Register); i++ )
	{
		std::stringstream s;
		s << "VI" << std::setw(2) << std::setfill('0') << i;
		m_integers[i].setName( s.str() );
	}

	m_dynamicThreshold = 16;
	m_showRegisterInfo = false;
	m_currState = OUTSIDE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RegisterAllocator::~RegisterAllocator()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RegisterAllocator::setAvailableFloats( unsigned int floats )
{
	for( unsigned int i = 1; i < 32; i++ )
		m_floats[i].setAvailable( ( floats & (1<<i) ) != 0 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RegisterAllocator::setAvailableIntegers( unsigned int integers  )
{
	for( unsigned int i = 1; i < 16; i++ )
		m_integers[i].setAvailable( ( integers & (1<<i) ) != 0 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RegisterAllocator::process( std::list<Token>& tokens )
{
	BranchState* branchState = NULL;

	if( !collectLabels( tokens.begin(), tokens.end() ) )
		return false;

	for( std::list<Token>::iterator i = tokens.begin(), next = tokens.begin(); i != tokens.end(); i = next )
	{
		next++;

		switch( state() )
		{
			case OUTSIDE:
			{
				if( !(*i).operand() )
					continue;

				if( !((*i).operand()->flags() & Operand::PREPROCESSOR) )
				{
					Error::Display( Error( "Code not allowed outside entry point", *i ) );
					return false;
				}

				if( "--enter" == (*i).operand()->name() )
				{
					next = i;

					branchState = new BranchState( *this );
					assert( branchState );

					setState( ENTER );
				}
				if( ("--exit" == (*i).operand()->name()) || ("--exitm" == (*i).operand()->name()) )
				{
					next = i;
					setState( EXIT );
				}
				else
				{
				}
			}
			break;

			case ENTER:
			{
				if( !(*i).operand() )
					continue;

				if( (*i).operand()->unit() != Operand::ENTER )
				{
					Error::Display( Error( "Invalid operand inside --enter/--endenter block", *i ) );
					delete branchState;
					return false;
				}

				(*i).setFlags( (*i).flags() | Token::IGNORED );

				if( "in_vf" == (*i).operand()->name() )
				{
					if( (*i).arguments().empty() )
					{
						Error::Display( Error( "Missing argument to in_vf", *i ) );
						return false;
					}
					const Token::Argument& arg = *((*i).arguments().begin());
					branchState->setFloatInput( arg.immediate(), arg.regNumber() );
				}
				else if( "in_vi" == (*i).operand()->name() )
				{
					if( (*i).arguments().empty() )
					{
						Error::Display( Error( "Missing argument to in_vi", *i ) );
						return false;
					}
					const Token::Argument& arg = *((*i).arguments().begin());
					branchState->setIntegerInput( arg.immediate(), arg.regNumber() );
				}
				else if( "in_hw_acc" == (*i).operand()->name() )
					branchState->writeAccumulator( Token::X|Token::Y|Token::Z|Token::W );
				else if( "in_hw_i" == (*i).operand()->name() )
					branchState->writeI();
				else if( "in_hw_p" == (*i).operand()->name() )
					branchState->writeP();
				else if( "in_hw_q" == (*i).operand()->name() )
					branchState->writeQ();
				else if( "in_hw_r" == (*i).operand()->name() )
					branchState->writeR();
				else if( "--endenter" == (*i).operand()->name() )
				{
					m_states.push_back( branchState );
					setState( CODE );
				}
			}
			break;

			case CODE:
			{
				if( !branchState )
				{
					Error::Display( Error( "Internal error: missing branch state before CODE block" ) );
					return false;
				}
				// locate end of codeblock
				for( ; next != tokens.end(); next++ )
				{
					if( !(*next).operand() )
						continue;

					if( !((*next).operand()->flags() & Operand::PREPROCESSOR) )
						continue;

					if( ((*next).operand()->unit() == Operand::EXIT) || ((*next).operand()->unit() == Operand::ENTER) )
						break;

					if( !processCommonDirective( (*next) ) )
						return false;
				}

				branchState->setCurrent( i );
				branchState->setExitPoint( next );
				branchState->pushTraces( &*i, true );

				setState( EXIT );
			}
			break;

			case EXIT:
			{
				if( !(*i).operand() )
					continue;

				if( (*i).operand()->unit() != Operand::EXIT )
				{
					Error::Display( Error( "Invalid operand inside --exit/--endexit block", *i ) );
					return false;
				}

				(*i).setFlags( (*i).flags() | Token::IGNORED );

				if( "out_vf" == (*i).operand()->name() )
				{
					if( (*i).arguments().empty() )
					{
						Error::Display( Error( "Missing argument to out_vf", *i ) );
						return false;
					}
					const Token::Argument& arg = *((*i).arguments().begin());

					if( branchState )
						branchState->setFloatOutput( arg.immediate(), arg.regNumber() );
				}
				else if( "out_vi" == (*i).operand()->name() )
				{
					if( (*i).arguments().empty() )
					{
						Error::Display( Error( "Missing argument to out_vi", *i ) );
						return false;
					}
					const Token::Argument& arg = *((*i).arguments().begin());
					if( branchState )
						branchState->setIntegerOutput( arg.immediate(), arg.regNumber() );
				}
				else if( "--endexit" == (*i).operand()->name() )
				{
					setState( OUTSIDE );
				}
			}
			break;
		}

		if( !processCommonDirective( (*i) ) )
			return false;
	}

	// one entry state per code block so far; record where each block starts
	// before processing moves the states on
	std::vector< std::pair< std::list<Token>::iterator, BranchState* > > blocks;

	std::list<BranchState*>::iterator j;
	for( j = m_states.begin(); j != m_states.end(); j++ )
	{
		if( *j )
			blocks.push_back( std::make_pair( (*j)->current(), *j ) );
	}

	for( std::vector< std::pair< std::list<Token>::iterator, BranchState* > >::iterator b = blocks.begin(); b != blocks.end(); ++b )
		collectLanesLiveAfterWrites( b->first, b->second->exitPoint(), b->second );

	for( j = m_states.begin(); j != m_states.end(); j++ )
	{
		if( !(*j) )
		{
			Error::Display( Error( "Internal error: null branch state before processing" ) );
			return false;
		}
		if( !processBranchState( *j, tokens.end() ) )
			return false;
	}

	for( std::vector< std::pair< std::list<Token>::iterator, BranchState* > >::iterator b = blocks.begin(); b != blocks.end(); ++b )
	{
		if( !completeLiveRanges( b->first, b->second->exitPoint(), b->second ) )
			return false;
	}

	collectLiteralRegisterUsage( tokens );
	extendContinuationLiveRanges( tokens );
	extendLoopDirectiveLiveRanges( tokens );
	extendMultiQStageLiveRanges( tokens );

	if( m_aliases.size() > 0 )
	{
		Alias* unallocated = NULL;
		if( !processAliases( unallocated ) )
		{
			reportUnallocatedAlias( tokens, unallocated );
			return false;
		}
	}

	while( !m_states.empty() )
	{
		delete m_states.back();
		m_states.pop_back();
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RegisterAllocator::processBranchState( BranchState* state, std::list<Token>::iterator end )
{
	std::list<Token>::iterator curr, next;
	for( ; state->current() != end; state->setCurrent( curr ) )
	{
		curr = state->current();
		Token& token = *curr;
		curr++;

		if( !token.operand() )
			continue;

		// if we've reached a new entry or exit-point, abort branch
		if( (token.operand()->unit() == Operand::EXIT) || (token.operand()->unit() == Operand::ENTER) )
			break;

		token.setFlags( token.flags() | Token::PROCESSED );

		for( std::list<Token::Argument>::reverse_iterator i = token.arguments().rbegin(); i != token.arguments().rend(); i++ )
		{
			switch( (*i).type() )
			{
				case Token::Argument::FLOAT_REGISTER:
				{
					if( (*i).flags() & Token::Argument::WRITE )
						state->writeFloat( (*i) );
					else
					{
						if( !state->readFloat( (*i) ) )
						{
							Error::Display( Error( "Read-attempt from uninitialized float register", token, *i ) );
							return false;
						}
					}
				}
				break;

				case Token::Argument::INTEGER_REGISTER:
				{
					if( (*i).flags() & Token::Argument::WRITE )
						state->writeInteger( (*i) );
					else
					{
						if( !state->readInteger( (*i) ) )
						{
							Error::Display( Error( "Read-attempt from uninitialized integer register", token, *i ) );
							return false;
						}
					}
				}
				break;

				case Token::Argument::ACCUMULATOR:
				{
					if( (*i).flags() & Token::Argument::WRITE )
						state->writeAccumulator( (*i).fields() );
					else
					{
						if( !state->readAccumulator( (*i).fields() ) )
						{
							Error::Display( Error( "Read-attempt from uninitialized accumulator", token, *i ) );
							return false;
						}
					}
				}
				break;

				case Token::Argument::Q:
				{
					if( (*i).flags() & Token::Argument::WRITE )
						state->writeQ();
					else
					{
						if( !state->readQ() )
						{
							Error::Display( Error( "Read-attempt from uninitialized Q register", token, *i ) );
							return false;
						}
					}
				}
				break;

				case Token::Argument::P:
				{
					if( (*i).flags() & Token::Argument::WRITE )
						state->writeP();
					else
					{
						if( !state->readP() )
						{
							Error::Display( Error( "Read-attempt from uninitialized P register", token, *i ) );
							return false;
						}
					}
				}
				break;

				case Token::Argument::R:
				{
					if( (*i).flags() & Token::Argument::WRITE )
						state->writeR();
					else
					{
						if( !state->readR() )
						{
							Error::Display( Error( "Read-attempt from uninitialized R register", token, *i ) );
							return false;
						}
					}
				}
				break;

				case Token::Argument::I:
				{
					if( (*i).flags() & Token::Argument::WRITE )
						state->writeI();
					else
					{
						if( !state->readI() )
						{
							Error::Display( Error( "Read-attempt from uninitialized I register", token, *i ) );
							return false;
						}
					}
				}
				break;

				case Token::Argument::IMMEDIATE:
				{
					if( token.operand()->flags() & Operand::IWRITE )
					{
						// special case for LOI
						state->writeI();
					}
				}
				break;

				default: break;
			}
		}

		if( token.operand()->unit() == Operand::BRU )
		{
			// branch instruction

			std::list<Token::Argument>::const_iterator branchDest = token.arguments().end();
			std::list<Token::Argument>::const_iterator branchStore = token.arguments().end();

			std::list<Token::Argument>::const_iterator i;
			for( i = token.arguments().begin(); i != token.arguments().end(); i++ )
			{
				if( (*i).flags() & Token::Argument::BRANCH )
					branchDest = i;

				if( (*i).flags() & Token::Argument::ADDRESS )
					branchStore = i;
			}

			if( token.arguments().end() == branchDest )
			{
				Error::Display( Error( "Invalid branch", token ) );
				return false;
			}

			if( token.arguments().end() != branchStore )
				state->storeAddress( *branchStore );

			std::list<Token>::iterator target;

			// the path runs up to the token after the branch, or to the branch
			// itself when it ends the program
			Token& after = (curr != end) ? *curr : token;

			if( token.operand()->flags() & Operand::DYNAMIC )
			{
				if( !state->address( *branchDest, target, m_labels ) )
					continue;

				unsigned int targetLine = (*target).lineNumber();
				unsigned int currentLine = after.lineNumber();
				bool isBackwardBranch = targetLine < currentLine;

				if( state->isBranchTaken( &*target ) )
				{
					if( isBackwardBranch )
					{
						// Backward branch (loop back-edge) already processed - extend ranges again and stop
						state->extendLiveRanges( targetLine, currentLine );
					}
					break;
				}

				// First time seeing this branch - extend ranges now if it's a backward branch
				if( isBackwardBranch )
				{
					state->extendLiveRanges( targetLine, currentLine );
				}

				if( !updateDynamicTracker( &*state->current() ) )
					break;

				state->storeBranch( &*target );

				BranchState* newState = new BranchState( *state );
				assert( newState );

				newState->pushTraces( &after, false );
				newState->pushTraces( &*target, true );
				newState->setCurrent( target );

				m_states.push_back( newState );
			}
			else
			{
				if( !state->address( *branchDest, target, m_labels ) )
					break;

				if( state->isBranchTaken( &*target ) )
				{
					// Backward branch (loop back-edge) - extend live ranges to cover loop body
					unsigned int targetLine = (*target).lineNumber();
					unsigned int currentLine = after.lineNumber();
					if( targetLine < currentLine )
						state->extendLiveRanges( targetLine, currentLine );
					break;
				}

				state->storeBranch( &*target );

				state->pushTraces( &after, false );
				state->pushTraces( &*target, true );

				curr = target;
			}
		}
	}

	if( state->current() == state->exitPoint() )
	{
		if( !state->applyRegisterOutputs() )
		{
			Error::Display( Error( "Output register conflict", *(state->exitPoint()) ) );
			return false;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{
	// A read or write of a named VF or VI register in a code block.
	struct VariableAccess
	{
		unsigned int node;				// the token's position in the block
		unsigned int lanes;				// the fields of a VF access; 1 for VI
		Dependency* dependency;		// NULL where processBranchState never got to
	};

	struct VariableAccesses
	{
		std::vector<VariableAccess> reads;
		std::vector<VariableAccess> writes;
	};

	typedef std::pair<Alias::Type, std::string> Variable;

	// A read, the writes that reach it, and the block's tokens in between.
	struct ReachedRead
	{
		std::string name;
		Dependency* read;
		std::vector<Dependency*> writes;
		std::vector<unsigned int> live;
	};

	bool sameRegister( const Token::Argument& a, const Token::Argument& b )
	{
		if( a.content() != b.content() )
			return false;
		if( a.content() == Token::Argument::ALIAS )
			return a.alias() == b.alias();
		return a.regNumber() == b.regNumber();
	}

	void reportRegisterConflict( const std::string& name, const Register* a, const Register* b, const Token& token )
	{
		Error::Display( Error( "Register conflict: " + name + " has to be in both " + a->name() + " and " + b->name(), token ) );
	}

	bool joinWebs( Dependency* use, Dependency* def, const std::string& name )
	{
		Alias* useAlias = use->alias();
		Alias* defAlias = def->alias();
		if( useAlias == defAlias )
			return true;

		if( useAlias->allocatedRegister() && defAlias->allocatedRegister() && (useAlias->allocatedRegister() != defAlias->allocatedRegister()) )
		{
			reportRegisterConflict( name, useAlias->allocatedRegister(), defAlias->allocatedRegister(), use->token() );
			return false;
		}

		use->depend( def );
		return true;
	}

	// A code block's tokens and the control flow between them: fall-through,
	// branches to labels, and jr/jalr to the instruction after each bal/jalr
	// that sets their register.
	struct BlockFlow
	{
		std::vector< std::list<Token>::iterator > nodes;
		std::vector< std::vector<unsigned int> > successors;
		std::vector< std::vector<unsigned int> > predecessors;
		bool fallsOffEnd;		// the last token runs on into the block's exit
	};

	void buildBlockFlow( std::list<Token>::iterator begin, std::list<Token>::iterator end, const std::map< std::string, std::list<Token>::iterator >& labels, BlockFlow& flow )
	{
		std::map< const Token*, unsigned int > index;
		for( std::list<Token>::iterator t = begin; t != end; ++t )
		{
			index[ &*t ] = static_cast<unsigned int>( flow.nodes.size() );
			flow.nodes.push_back( t );
		}
		const unsigned int count = static_cast<unsigned int>( flow.nodes.size() );
		flow.successors.assign( count, std::vector<unsigned int>() );
		flow.predecessors.assign( count, std::vector<unsigned int>() );
		flow.fallsOffEnd = false;

		std::vector< std::pair<const Token::Argument*, unsigned int> > returns;
		for( unsigned int n = 0; n + 1 < count; ++n )
		{
			const Token& token = *flow.nodes[n];
			if( !token.operand() || token.operand()->unit() != Operand::BRU )
				continue;
			for( std::list<Token::Argument>::const_iterator a = token.arguments().begin(); a != token.arguments().end(); ++a )
			{
				if( (*a).flags() & Token::Argument::ADDRESS )
					returns.push_back( std::make_pair( &*a, n + 1 ) );
			}
		}

		for( unsigned int n = 0; n < count; ++n )
		{
			const Token& token = *flow.nodes[n];
			bool fallsThrough = true;

			if( token.operand() && token.operand()->unit() == Operand::BRU )
			{
				fallsThrough = (token.operand()->flags() & Operand::DYNAMIC) != 0;

				for( std::list<Token::Argument>::const_iterator a = token.arguments().begin(); a != token.arguments().end(); ++a )
				{
					if( !((*a).flags() & Token::Argument::BRANCH) )
						continue;

					if( (*a).type() == Token::Argument::IMMEDIATE )
					{
						std::map< std::string, std::list<Token>::iterator >::const_iterator label = labels.find( (*a).immediate() );
						if( label == labels.end() )
							continue;
						std::map< const Token*, unsigned int >::const_iterator target = index.find( &*label->second );
						if( target != index.end() )
							flow.successors[n].push_back( target->second );
					}
					else if( (*a).type() == Token::Argument::INTEGER_REGISTER )
					{
						for( unsigned int r = 0; r < returns.size(); ++r )
						{
							if( sameRegister( *returns[r].first, *a ) )
								flow.successors[n].push_back( returns[r].second );
						}
					}
				}
			}

			if( fallsThrough )
			{
				if( n + 1 < count )
					flow.successors[n].push_back( n + 1 );
				else
					flow.fallsOffEnd = true;
			}
		}

		for( unsigned int n = 0; n < count; ++n )
		{
			for( unsigned int s = 0; s < flow.successors[n].size(); ++s )
				flow.predecessors[ flow.successors[n][s] ].push_back( n );
		}
	}

	// The lanes a named register access touches: the fields of a VF access,
	// 1 for VI. A broadcast named by the opcode (maddaw, mulz) leaves its
	// operand without fields; it reads the token's broadcast lane.
	unsigned int accessLanes( const Token& token, const Token::Argument& argument )
	{
		if( argument.type() != Token::Argument::FLOAT_REGISTER )
			return 1;

		unsigned int lanes = argument.fields();
		if( !lanes && (argument.flags() & Token::Argument::BROADCAST) )
			lanes = token.broadcast();
		return lanes ? lanes : (Token::X|Token::Y|Token::Z|Token::W);
	}

	// The lanes of one VF variable each token reads and writes, and the writes
	// that leave some of its lanes as they were.
	struct VariableLanes
	{
		std::vector<unsigned int> read;
		std::vector<unsigned int> written;
		std::vector< std::pair<unsigned int, const Token::Argument*> > partialWrites;
	};
}

void RegisterAllocator::collectLanesLiveAfterWrites( std::list<Token>::iterator begin, std::list<Token>::iterator end, BranchState* entry )
{
	BlockFlow flow;
	buildBlockFlow( begin, end, m_labels, flow );
	const unsigned int count = static_cast<unsigned int>( flow.nodes.size() );
	const unsigned int all = Token::X|Token::Y|Token::Z|Token::W;

	std::map< std::string, VariableLanes > variables;
	for( unsigned int n = 0; n < count; ++n )
	{
		const Token& token = *flow.nodes[n];
		for( std::list<Token::Argument>::const_iterator a = token.arguments().begin(); a != token.arguments().end(); ++a )
		{
			if( ((*a).content() != Token::Argument::ALIAS) || ((*a).type() != Token::Argument::FLOAT_REGISTER) )
				continue;

			VariableLanes& variable = variables[ (*a).alias() ];
			if( variable.read.empty() )
			{
				variable.read.assign( count, 0 );
				variable.written.assign( count, 0 );
			}

			const unsigned int lanes = accessLanes( token, *a );
			if( (*a).flags() & Token::Argument::WRITE )
			{
				variable.written[n] |= lanes;
				if( lanes != all )
					variable.partialWrites.push_back( std::make_pair( n, &*a ) );
			}
			else
				variable.read[n] |= lanes;
		}
	}

	for( std::map< std::string, VariableLanes >::const_iterator v = variables.begin(); v != variables.end(); ++v )
	{
		const VariableLanes& variable = v->second;
		if( variable.partialWrites.empty() )
			continue;

		// the lanes read later on some path before they are written again; the
		// block's exit reads every lane of its outputs

		const unsigned int atExit = entry->isFloatOutput( v->first ) ? all : 0;
		std::vector<unsigned int> liveIn( count, 0 );
		std::vector<unsigned int> liveOut( count, 0 );
		bool changed = true;
		while( changed )
		{
			changed = false;
			for( unsigned int n = count; n-- > 0; )
			{
				unsigned int out = (flow.fallsOffEnd && (n + 1 == count)) ? atExit : 0;
				for( unsigned int s = 0; s < flow.successors[n].size(); ++s )
					out |= liveIn[ flow.successors[n][s] ];
				liveOut[n] = out;

				const unsigned int in = variable.read[n] | (out & ~variable.written[n]);
				if( in != liveIn[n] )
				{
					liveIn[n] = in;
					changed = true;
				}
			}
		}

		for( unsigned int w = 0; w < variable.partialWrites.size(); ++w )
			m_lanesLiveAfter[ variable.partialWrites[w].second ] = liveOut[ variable.partialWrites[w].first ];
	}
}

unsigned int RegisterAllocator::lanesLiveAfter( const Token::Argument& write ) const
{
	std::map< const Token::Argument*, unsigned int >::const_iterator i = m_lanesLiveAfter.find( &write );
	return (i == m_lanesLiveAfter.end()) ? (Token::X|Token::Y|Token::Z|Token::W) : i->second;
}

bool RegisterAllocator::completeLiveRanges( std::list<Token>::iterator begin, std::list<Token>::iterator end, BranchState* entry )
{
	BlockFlow flow;
	buildBlockFlow( begin, end, m_labels, flow );
	const std::vector< std::list<Token>::iterator >& nodes = flow.nodes;
	const std::vector< std::vector<unsigned int> >& successors = flow.successors;
	const std::vector< std::vector<unsigned int> >& predecessors = flow.predecessors;
	const unsigned int count = static_cast<unsigned int>( nodes.size() );
	if( !count )
		return true;

	// every read and write of a named register, by variable

	std::map< Variable, VariableAccesses > variables;
	for( unsigned int n = 0; n < count; ++n )
	{
		for( std::list<Token::Argument>::const_iterator a = nodes[n]->arguments().begin(); a != nodes[n]->arguments().end(); ++a )
		{
			if( (*a).content() != Token::Argument::ALIAS )
				continue;
			if( ((*a).type() != Token::Argument::FLOAT_REGISTER) && ((*a).type() != Token::Argument::INTEGER_REGISTER) )
				continue;

			const bool isFloat = ((*a).type() == Token::Argument::FLOAT_REGISTER);

			VariableAccess access;
			access.node = n;
			access.lanes = accessLanes( *nodes[n], *a );
			access.dependency = (*a).dependency();

			VariableAccesses& accesses = variables[ Variable( isFloat ? Alias::FLOAT : Alias::INTEGER, (*a).alias() ) ];
			if( (*a).flags() & Token::Argument::WRITE )
				accesses.writes.push_back( access );
			else if( access.dependency )
				accesses.reads.push_back( access );
		}
	}

	std::vector<ReachedRead> reached;
	std::map< Variable, std::vector<Dependency*> > inputReads;

	for( std::map< Variable, VariableAccesses >::const_iterator v = variables.begin(); v != variables.end(); ++v )
	{
		const std::vector<VariableAccess>& writes = v->second.writes;
		const bool isInput = (entry->inputRegister( v->first.first, v->first.second ) != NULL);

		// where a write, or the block entry for an input, may have set the variable

		std::vector<bool> set( count, false );
		std::vector<unsigned int> work;
		for( unsigned int w = 0; w < writes.size(); ++w )
			work.push_back( writes[w].node );
		if( isInput )
			work.push_back( 0 );
		while( !work.empty() )
		{
			const unsigned int n = work.back();
			work.pop_back();
			if( set[n] )
				continue;
			set[n] = true;
			for( unsigned int s = 0; s < successors[n].size(); ++s )
				work.push_back( successors[n][s] );
		}

		for( std::vector<VariableAccess>::const_iterator r = v->second.reads.begin(); r != v->second.reads.end(); ++r )
		{
			// back from the read until writes cover the lanes it reads

			std::vector<unsigned int> followed( count, 0 );
			std::vector<bool> reaching( writes.size(), false );
			bool reachesEntry = ((*r).node == 0);

			std::vector< std::pair<unsigned int, unsigned int> > back;
			for( unsigned int p = 0; p < predecessors[(*r).node].size(); ++p )
				back.push_back( std::make_pair( predecessors[(*r).node][p], (*r).lanes ) );

			while( !back.empty() )
			{
				const unsigned int n = back.back().first;
				unsigned int lanes = back.back().second & ~followed[n];
				back.pop_back();
				if( !lanes )
					continue;
				followed[n] |= lanes;

				unsigned int written = 0;
				for( unsigned int w = 0; w < writes.size(); ++w )
				{
					if( (writes[w].node == n) && (writes[w].lanes & lanes) )
					{
						reaching[w] = true;
						written |= writes[w].lanes;
					}
				}
				lanes &= ~written;
				if( !lanes )
					continue;

				if( n == 0 )
					reachesEntry = true;
				for( unsigned int p = 0; p < predecessors[n].size(); ++p )
					back.push_back( std::make_pair( predecessors[n][p], lanes ) );
			}

			ReachedRead read;
			read.name = v->first.second;
			read.read = (*r).dependency;
			for( unsigned int w = 0; w < writes.size(); ++w )
			{
				if( reaching[w] && writes[w].dependency )
					read.writes.push_back( writes[w].dependency );
			}
			// a path from the entry without a write leaves the variable unset
			// there: that stretch is not part of its range
			for( unsigned int n = 0; n < count; ++n )
			{
				if( (n == (*r).node) || (followed[n] && set[n]) )
					read.live.push_back( n );
			}
			reached.push_back( read );

			if( reachesEntry && isInput )
				inputReads[ v->first ].push_back( (*r).dependency );
		}
	}

	// join each read with the writes that reach it, and the reads of an input
	// that see its value from the entry

	for( std::vector<ReachedRead>::const_iterator r = reached.begin(); r != reached.end(); ++r )
	{
		for( std::vector<Dependency*>::const_iterator w = (*r).writes.begin(); w != (*r).writes.end(); ++w )
		{
			if( !joinWebs( (*r).read, *w, (*r).name ) )
				return false;
		}
	}

	for( std::map< Variable, std::vector<Dependency*> >::const_iterator i = inputReads.begin(); i != inputReads.end(); ++i )
	{
		const std::vector<Dependency*>& reads = i->second;
		for( unsigned int k = 1; k < reads.size(); ++k )
		{
			if( !joinWebs( reads[0], reads[k], i->first.second ) )
				return false;
		}

		const Register* input = entry->inputRegister( i->first.first, i->first.second );
		Alias* alias = reads[0]->alias();
		if( !alias->allocatedRegister() )
			alias->setAllocatedRegister( input );
		else if( alias->allocatedRegister() != input )
		{
			reportRegisterConflict( i->first.second, alias->allocatedRegister(), input, reads[0]->token() );
			return false;
		}
	}

	// after the joins, each read's alias is live from the writes to the read

	for( std::vector<ReachedRead>::const_iterator r = reached.begin(); r != reached.end(); ++r )
	{
		Alias* alias = (*r).read->alias();
		const std::vector<unsigned int>& live = (*r).live;
		for( unsigned int k = 0; k < live.size(); ++k )
		{
			unsigned int start = nodes[ live[k] ]->lineNumber();
			unsigned int stop = start;

			// no other token sits between two live tokens in a row
			if( (k + 1 < live.size()) && (live[k + 1] == live[k] + 1) )
			{
				const unsigned int next = nodes[ live[k + 1] ]->lineNumber();
				if( next < start )
					start = next;
				else
					stop = next;
			}

			alias->addRange( start, stop );
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RegisterAllocator::collectLabels( std::list<Token>::iterator start, std::list<Token>::iterator end )
{
	m_labels.clear();

	for( std::list<Token>::iterator i = start; i != end; i++ )
	{
		if( (*i).label().length() )
		{
			std::map<std::string,std::list<Token>::iterator>::iterator j = m_labels.find( (*i).label() );

			if( j != m_labels.end() )
			{
				Error::Display( Error( "Duplicate label '" + (*i).label() + "'", *i ) );
				return false;
			}

			m_labels[ (*i).label() ] = i;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RegisterAllocator::processAliases( Alias*& unallocated )
{
	// allocate registers that do not overlap usage

	// Count total aliases by type
	int totalFloatAliases = 0;
	int totalIntAliases = 0;
	int preallocatedFloats = 0;
	int preallocatedInts = 0;

	for( AliasMap::iterator i = m_aliases.begin(); i != m_aliases.end(); ++i )
	{
		if( i->first->type() == Alias::FLOAT )
		{
			totalFloatAliases++;
			if( i->first->allocatedRegister() )
				preallocatedFloats++;
		}
		else
		{
			totalIntAliases++;
			if( i->first->allocatedRegister() )
				preallocatedInts++;
		}
	}

	// Count available registers
	int availableFloats = 0;
	int availableInts = 0;
	for( int i = 0; i < 32; i++ )
		if( m_floats[i].available() )
			availableFloats++;
	for( int i = 0; i < 16; i++ )
		if( m_integers[i].available() )
			availableInts++;

	if( m_showRegisterInfo )
	{
		std::cerr << "\n=== Register Allocation Debug ===" << std::endl;
		std::cerr << "Float registers: " << availableFloats << " available, "
		          << (totalFloatAliases - preallocatedFloats) << " needed (total aliases: "
		          << totalFloatAliases << ", preallocated: " << preallocatedFloats << ")" << std::endl;
		std::cerr << "Integer registers: " << availableInts << " available, "
		          << (totalIntAliases - preallocatedInts) << " needed (total aliases: "
		          << totalIntAliases << ", preallocated: " << preallocatedInts << ")" << std::endl;

		// Print all alias ranges for debugging
		std::cerr << "\n=== Alias Ranges ===" << std::endl;
		for( AliasMap::iterator i = m_aliases.begin(); i != m_aliases.end(); ++i )
		{
			Alias* alias = i->first;
			std::cerr << (alias->type() == Alias::FLOAT ? "FLOAT" : "INT") << ": ";
			if( alias->allocatedRegister() )
				std::cerr << "[prealloc: " << alias->allocatedRegister()->name() << "] ";
			alias->printRanges( std::cerr );
			std::cerr << std::endl;
		}
		std::cerr << "====================\n" << std::endl;
	}

	// Pre-pass: allocate every two-address chain ATOMICALLY (root +
	// all its known successors) before any singleton aliases run the
	// main loop.  Treating chain members one-at-a-time isn't enough,
	// because an unrelated singleton alias whose live range fits in
	// the same register can snipe it before the successor reaches the
	// main loop — leaving the successor with no choice but a different
	// register, which is exactly the bug we're trying to prevent (the
	// `isubiu` dest then disagrees with its source, the loop counter
	// never decrements, and xgkick never fires).
	//
	// For each unallocated root (no sameNamePredecessor), find every
	// alias whose chain root is this root, then pick a register that
	// no non-chain allocated alias intersects on ANY chain member's
	// range.  Assign that register to all chain members at once.
	for( AliasMap::iterator i = m_aliases.begin(); i != m_aliases.end(); ++i )
	{
		Alias* root = i->first;
		if( root->allocatedRegister() )
			continue;
		if( root->sameNamePredecessor() )
			continue;

		// Collect chain members: walk every alias's predecessor chain
		// (with a hard hop cap as a cycle defence) and gather the ones
		// that terminate at `root`.  This is O(N²) but N is small.
		std::vector<Alias*> chain;
		chain.push_back( root );
		for( AliasMap::iterator k = m_aliases.begin(); k != m_aliases.end(); ++k )
		{
			Alias* other = k->first;
			if( other == root ) continue;
			if( other->type() != root->type() ) continue;
			Alias* p = other;
			for( int hop = 0; hop < 32 && p->sameNamePredecessor(); ++hop )
				p = p->sameNamePredecessor();
			if( p == root )
				chain.push_back( other );
		}

		unsigned int maxLimit = root->type() == Alias::FLOAT ? 32 : 16;
		for( unsigned int j = 0; j < maxLimit; ++j )
		{
			const Register* candidate = (root->type() == Alias::FLOAT) ? &m_floats[j] : &m_integers[j];
			if( !candidate->available() )
				continue;

			// `candidate` must not collide with any already-allocated
			// non-chain alias on ANY chain member's live range.
			bool conflict = false;
			for( AliasMap::iterator k = m_aliases.begin(); !conflict && k != m_aliases.end(); ++k )
			{
				Alias* src = k->first;
				if( !src->allocatedRegister() ) continue;
				if( src->type() != root->type() ) continue;
				if( src->allocatedRegister() != candidate ) continue;
				// Chain members don't conflict with each other on the
				// shared register — that's the whole point.
				bool inChain = false;
				for( unsigned int m = 0; m < chain.size(); ++m )
					if( chain[m] == src ) { inChain = true; break; }
				if( inChain ) continue;
				for( unsigned int m = 0; m < chain.size(); ++m )
				{
					if( chain[m]->intersects( src ) )
					{
						conflict = true;
						break;
					}
				}
			}

			if( !conflict )
			{
				for( unsigned int m = 0; m < chain.size(); ++m )
					chain[m]->setAllocatedRegister( candidate );
				break;
			}
		}
	}

	for( AliasMap::iterator i = m_aliases.begin(); i != m_aliases.end(); ++i )
	{
		Alias* dest = i->first;

		// preallocated register
		if( dest->allocatedRegister() )
			continue;

		unsigned int maxLimit = dest->type() == Alias::FLOAT ? 32 : 16;

		// Two-address coalescing: if `dest` was created by a write that
		// reuses an existing source-level alias name (e.g. the dest of
		// `isubiu x, x, 1`), try the predecessor's register FIRST.  The
		// predecessor may itself be unallocated yet; walk the chain to
		// find the earliest allocated ancestor.  Falls through to the
		// regular j=0..maxLimit scan if no predecessor / preferred reg
		// is available or it conflicts.
		const Register* preferred = NULL;
		// Walk to the root, with a hard cap to defeat any residual cycle
		// the branch-state analysis might have introduced via loop re-
		// processing.  16 is well above any plausible chain depth in
		// real shaders.
		Alias* chainRoot = dest;
		for( int hop = 0; hop < 16 && chainRoot->sameNamePredecessor(); ++hop )
			chainRoot = chainRoot->sameNamePredecessor();
		if( chainRoot != dest && chainRoot->allocatedRegister() )
			preferred = chainRoot->allocatedRegister();
		if( preferred )
		{
			// Run the normal conflict check against `preferred`.  If it
			// holds, assign immediately and skip the j-loop.
			bool conflict = false;
			for( AliasMap::iterator k = m_aliases.begin(); k != m_aliases.end(); ++k )
			{
				Alias* src = k->first;
				if( !src->allocatedRegister() ) continue;
				if( src->type() != dest->type() ) continue;
				if( src->allocatedRegister() != preferred ) continue;
				if( src == dest ) continue;
				// Same-name predecessor on the chain doesn't count as a
				// conflict: that's the whole point of coalescing.
				bool isAncestor = false;
				for( Alias* p = dest->sameNamePredecessor(); p; p = p->sameNamePredecessor() )
				{
					if( p == src ) { isAncestor = true; break; }
				}
				if( isAncestor ) continue;
				if( !dest->intersects( src ) ) continue;
				conflict = true;
				break;
			}
			if( !conflict )
			{
				dest->setAllocatedRegister( preferred );
				continue;
			}
		}

		for( unsigned int j = 0; j < maxLimit; ++j )
		{
			const Register* candidate = (dest->type() == Alias::FLOAT) ? &m_floats[j] : &m_integers[j];

			if( !candidate->available() )
				continue;

			for( AliasMap::iterator k = m_aliases.begin(); k != m_aliases.end(); ++k )
			{
				Alias* src = k->first;

				if( !src->allocatedRegister() )
					continue;

				if( src->type() != dest->type() )
					continue;

				if( src->allocatedRegister() != candidate )
					continue;

				if( !dest->intersects( src ) )
					continue;

				candidate = NULL;
				break;
			}

			if( candidate )
			{
				dest->setAllocatedRegister( candidate );
				break;
			}
		}

		if( !dest->allocatedRegister() )
		{
			if( m_showRegisterInfo )
			{
				std::cerr << "Failed to allocate " << (dest->type() == Alias::FLOAT ? "FLOAT" : "INTEGER")
				          << " register for alias" << std::endl;
			}
			unallocated = dest;
			return false;
		}
		//assert( dest->allocatedRegister() );
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RegisterAllocator::reportUnallocatedAlias( const std::list<Token>& tokens, const Alias* alias ) const
{
	if( !alias )
	{
		Error::Display( Error( "Register allocation ran out of registers" ) );
		return;
	}

	// the registers .init_vf/.init_vi left to the allocator

	const bool isFloat = (alias->type() == Alias::FLOAT);
	unsigned int available = 0;
	for( unsigned int i = 0; i < (isFloat ? 32u : 16u); ++i )
	{
		if( (isFloat ? m_floats[i] : m_integers[i]).available() )
			++available;
	}

	std::stringstream message;
	message << "Register allocation ran out of " << (isFloat ? "VF" : "VI")
	        << " registers (" << available << " available)";

	// name the variable, where it first appears

	for( std::list<Token>::const_iterator i = tokens.begin(); i != tokens.end(); ++i )
	{
		for( std::list<Token::Argument>::const_iterator a = (*i).arguments().begin(); a != (*i).arguments().end(); ++a )
		{
			if( ((*a).content() == Token::Argument::ALIAS) && (*a).dependency()
			    && ((*a).dependency()->alias() == alias) )
			{
				message << " for " << (*a).alias();
				Error::Display( Error( message.str(), *i, *a ) );
				return;
			}
		}
	}

	Error::Display( Error( message.str() ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{
	void extendContinuationBlockLiveRanges( std::list<Token>::iterator blockStart, std::list<Token>::iterator blockEnd )
	{
		for( std::list<Token>::iterator cont = blockStart; cont != blockEnd; ++cont )
		{
			if( !cont->operand() || cont->operand()->name() != "--cont" )
				continue;

			const unsigned int contLine = cont->lineNumber();
			unsigned int blockEndLine = contLine;
			std::set<Alias*> writtenBeforeCont;
			std::set<Alias*> liveAcrossCont;

			for( std::list<Token>::iterator t = blockStart; t != blockEnd; ++t )
			{
				if( t->lineNumber() > blockEndLine )
					blockEndLine = t->lineNumber();

				for( std::list<Token::Argument>::const_iterator a = t->arguments().begin(); a != t->arguments().end(); ++a )
				{
					if( a->content() != Token::Argument::ALIAS || !a->dependency() || !a->dependency()->alias() )
						continue;

					Alias* alias = a->dependency()->alias();
					if( t->lineNumber() < contLine && (a->flags() & Token::Argument::WRITE) )
						writtenBeforeCont.insert( alias );
					else if( t->lineNumber() > contLine
					         && !(a->flags() & Token::Argument::WRITE)
					         && writtenBeforeCont.find( alias ) != writtenBeforeCont.end() )
						liveAcrossCont.insert( alias );
				}
			}

			for( std::set<Alias*>::iterator a = liveAcrossCont.begin(); a != liveAcrossCont.end(); ++a )
				(*a)->addRange( contLine, blockEndLine );
		}
	}
}

void RegisterAllocator::extendContinuationLiveRanges( std::list<Token>& tokens )
{
	std::list<Token>::iterator blockStart = tokens.end();
	for( std::list<Token>::iterator i = tokens.begin(); i != tokens.end(); ++i )
	{
		if( !i->operand() )
			continue;

		if( i->operand()->name() == "--endenter" )
		{
			blockStart = i;
			++blockStart;
			continue;
		}

		const bool startsNewBlock = (i->operand()->unit() == Operand::ENTER);
		const bool startsExitBlock = (i->operand()->unit() == Operand::EXIT);
		if( blockStart == tokens.end() || (!startsNewBlock && !startsExitBlock) )
			continue;

		std::list<Token>::iterator blockEnd = i;
		extendContinuationBlockLiveRanges( blockStart, blockEnd );

		blockStart = tokens.end();
		if( startsNewBlock )
		{
			blockStart = i;
			++blockStart;
		}
	}

	if( blockStart != tokens.end() )
		extendContinuationBlockLiveRanges( blockStart, tokens.end() );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RegisterAllocator::loopTargetHasLoopDirective( std::list<Token>::iterator target, std::list<Token>::iterator end ) const
{
	if( target == end )
		return false;

	if( target->operand() && target->operand()->name() == "--LoopCS" )
		return true;

	std::list<Token>::iterator next = target;
	++next;
	if( next != end && next->operand() && next->operand()->name() == "--LoopCS" )
		return true;

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RegisterAllocator::extendLoopDirectiveRange( std::list<Token>& tokens, unsigned int loopStart, unsigned int loopEnd )
{
	std::set<Alias*> aliases;
	for( std::list<Token>::iterator t = tokens.begin(); t != tokens.end(); ++t )
	{
		if( t->lineNumber() < loopStart || t->lineNumber() > loopEnd )
			continue;

		for( std::list<Token::Argument>::const_iterator a = t->arguments().begin(); a != t->arguments().end(); ++a )
		{
			if( a->content() != Token::Argument::ALIAS || !a->dependency() || !a->dependency()->alias() )
				continue;
			Alias* alias = a->dependency()->alias();
			if( alias->type() == Alias::FLOAT )
				aliases.insert( alias );
		}
	}

	unsigned int availableFloats = 0;
	for( unsigned int i = 0; i < 32; ++i )
	{
		if( m_floats[i].available() )
			++availableFloats;
	}

	std::set<Alias*> overlappingAliases;
	for( AliasMap::iterator i = m_aliases.begin(); i != m_aliases.end(); ++i )
	{
		Alias* alias = i->first;
		if( alias->type() != Alias::FLOAT )
			continue;
		if( aliases.find( alias ) != aliases.end() || alias->hasRangeOverlapping( loopStart, loopEnd ) )
			overlappingAliases.insert( alias );
	}

	if( overlappingAliases.size() > availableFloats )
		return;

	for( std::set<Alias*>::iterator a = aliases.begin(); a != aliases.end(); ++a )
		(*a)->addRange( loopStart, loopEnd );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RegisterAllocator::extendLoopDirectiveLiveRanges( std::list<Token>& tokens )
{
	for( std::list<Token>::iterator branch = tokens.begin(); branch != tokens.end(); ++branch )
	{
		if( !branch->operand() || branch->operand()->unit() != Operand::BRU )
			continue;

		std::list<Token::Argument>::const_iterator branchDest = branch->arguments().end();
		for( std::list<Token::Argument>::const_iterator a = branch->arguments().begin(); a != branch->arguments().end(); ++a )
		{
			if( a->flags() & Token::Argument::BRANCH )
			{
				branchDest = a;
				break;
			}
		}
		if( branchDest == branch->arguments().end() || branchDest->type() != Token::Argument::IMMEDIATE )
			continue;

		std::map< std::string, std::list<Token>::iterator >::iterator label = m_labels.find( branchDest->immediate() );
		if( label == m_labels.end() )
			continue;

		std::list<Token>::iterator target = label->second;
		if( target->lineNumber() >= branch->lineNumber() )
			continue;
		if( !loopTargetHasLoopDirective( target, tokens.end() ) )
			continue;

		extendLoopDirectiveRange( tokens, target->lineNumber(), branch->lineNumber() );
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{
	struct MultiQStage
	{
		MultiQStage()
		{
			producerLine = 0;
		}

		unsigned int producerLine;
		std::vector<unsigned int> consumerLines;
	};

	bool tokenTouchesQ( const Token& token, bool& readsQ, bool& writesQ )
	{
		readsQ = false;
		writesQ = false;
		for( std::list<Token::Argument>::const_iterator a = token.arguments().begin();
		     a != token.arguments().end(); ++a )
		{
			if( a->type() != Token::Argument::Q )
				continue;
			if( a->flags() & Token::Argument::WRITE )
				writesQ = true;
			else
				readsQ = true;
		}
		return readsQ || writesQ;
	}

	void collectFloatAliasesFromToken( const Token& token, std::set<Alias*>& aliases )
	{
		for( std::list<Token::Argument>::const_iterator a = token.arguments().begin();
		     a != token.arguments().end(); ++a )
		{
			if( a->type() != Token::Argument::FLOAT_REGISTER )
				continue;
			if( a->content() != Token::Argument::ALIAS || !a->dependency() || !a->dependency()->alias() )
				continue;
			Alias* alias = a->dependency()->alias();
			if( alias->type() == Alias::FLOAT )
				aliases.insert( alias );
		}
	}

	bool stringListContains( const std::list<std::string>& values, const std::string& value )
	{
		for( std::list<std::string>::const_iterator i = values.begin(); i != values.end(); ++i )
		{
			if( *i == value )
				return true;
		}
		return false;
	}

	bool stringListsIntersect( const std::list<std::string>& a, const std::list<std::string>& b )
	{
		for( std::list<std::string>::const_iterator i = a.begin(); i != a.end(); ++i )
		{
			if( stringListContains( b, *i ) )
				return true;
		}
		return false;
	}

	void addUniqueStrings( std::list<std::string>& dest, const std::list<std::string>& src )
	{
		for( std::list<std::string>::const_iterator i = src.begin(); i != src.end(); ++i )
		{
			if( !stringListContains( dest, *i ) )
				dest.push_back( *i );
		}
	}

	void addAliasRange( const std::set<Alias*>& aliases, unsigned int beginLine, unsigned int endLine )
	{
		if( beginLine > endLine )
			return;
		for( std::set<Alias*>::const_iterator a = aliases.begin(); a != aliases.end(); ++a )
			(*a)->addRange( beginLine, endLine );
	}

	void collectMultiQStages( std::list<Token>& tokens,
	                          unsigned int loopStart,
	                          unsigned int loopEnd,
	                          std::vector<MultiQStage>& stages,
	                          unsigned int& qReads,
	                          unsigned int& qWrites )
	{
		qReads = 0;
		qWrites = 0;
		for( std::list<Token>::iterator t = tokens.begin(); t != tokens.end(); ++t )
		{
			if( t->lineNumber() < loopStart || t->lineNumber() > loopEnd )
				continue;

			bool readsQ = false;
			bool writesQ = false;
			if( !tokenTouchesQ( *t, readsQ, writesQ ) )
				continue;

			if( writesQ )
			{
				MultiQStage stage;
				stage.producerLine = t->lineNumber();
				stages.push_back( stage );
				++qWrites;
			}

			if( readsQ )
			{
				++qReads;
				if( !stages.empty() )
					stages.back().consumerLines.push_back( t->lineNumber() );
			}
		}
	}

	void collectMultiQProducerDependencySliceAliases( std::list<Token>& tokens,
	                                                  unsigned int beginLine,
	                                                  unsigned int producerLine,
	                                                  std::set<Alias*>& aliases )
	{
		std::vector<Token*> rangeTokens;
		for( std::list<Token>::iterator t = tokens.begin(); t != tokens.end(); ++t )
		{
			if( t->lineNumber() < beginLine || t->lineNumber() > producerLine )
				continue;
			rangeTokens.push_back( &*t );
		}
		if( rangeTokens.empty() )
			return;

		VuTokenResourceAccess producerAccess;
		if( !buildVuTokenResourceAccess( *rangeTokens.back(), producerAccess ) )
			return;

		std::list<std::string> neededRegisters = producerAccess.registerReads;
		unsigned int neededResources = producerAccess.implicitReads;
		collectFloatAliasesFromToken( *rangeTokens.back(), aliases );

		for( unsigned int reverse = static_cast<unsigned int>( rangeTokens.size() ); reverse > 0; --reverse )
		{
			Token& token = *rangeTokens[reverse - 1];
			VuTokenResourceAccess access;
			if( !buildVuTokenResourceAccess( token, access ) )
				continue;

			const bool feedsNeededRegister = stringListsIntersect( access.registerWrites, neededRegisters );
			const bool feedsNeededResource = (access.implicitWrites & neededResources) != 0;
			if( !feedsNeededRegister && !feedsNeededResource )
				continue;

			collectFloatAliasesFromToken( token, aliases );
			addUniqueStrings( neededRegisters, access.registerReads );
			neededResources |= access.implicitReads;
		}
	}

	void extendAdjacentMultiQStageAliases( std::list<Token>& tokens,
	                                       const std::vector<MultiQStage>& stages,
	                                       unsigned int loopEnd )
	{
		if( stages.size() < 2 )
			return;

		for( unsigned int stage = 1; stage < stages.size(); ++stage )
		{
			if( stages[stage - 1].consumerLines.empty() )
				continue;

			const unsigned int previousProducerLine = stages[stage - 1].producerLine;
			const unsigned int previousLastConsumerLine = stages[stage - 1].consumerLines.back();
			const unsigned int currentProducerLine = stages[stage].producerLine;
			if( previousProducerLine == 0
			    || currentProducerLine == 0
			    || previousLastConsumerLine >= currentProducerLine )
				continue;

			std::set<Alias*> aliases;
			collectMultiQProducerDependencySliceAliases( tokens,
			                                             previousLastConsumerLine + 1,
			                                             currentProducerLine,
			                                             aliases );
			addAliasRange( aliases, previousProducerLine, currentProducerLine );
		}
		(void)loopEnd;
	}
}

void RegisterAllocator::extendMultiQStageRange( std::list<Token>& tokens, unsigned int loopStart, unsigned int loopEnd )
{
	std::set<Alias*> qStageAliases;
	std::vector<MultiQStage> stages;
	unsigned int qReads = 0;
	unsigned int qWrites = 0;

	collectMultiQStages( tokens, loopStart, loopEnd, stages, qReads, qWrites );
	if( qWrites > 1 && qReads > 0 )
		extendAdjacentMultiQStageAliases( tokens, stages, loopEnd );

	for( std::list<Token>::iterator t = tokens.begin(); t != tokens.end(); ++t )
	{
		if( t->lineNumber() < loopStart || t->lineNumber() > loopEnd )
			continue;

		bool readsQ = false;
		bool writesQ = false;
		if( !tokenTouchesQ( *t, readsQ, writesQ ) )
			continue;

		collectFloatAliasesFromToken( *t, qStageAliases );
	}

	if( qWrites <= 1 || qReads == 0 || qStageAliases.empty() )
		return;

	unsigned int availableFloats = 0;
	for( unsigned int i = 0; i < 32; ++i )
	{
		if( m_floats[i].available() )
			++availableFloats;
	}

	std::set<Alias*> overlappingAliases;
	for( AliasMap::iterator i = m_aliases.begin(); i != m_aliases.end(); ++i )
	{
		Alias* alias = i->first;
		if( alias->type() != Alias::FLOAT )
			continue;
		if( qStageAliases.find( alias ) != qStageAliases.end() || alias->hasRangeOverlapping( loopStart, loopEnd ) )
			overlappingAliases.insert( alias );
	}

	if( overlappingAliases.size() > availableFloats )
		return;

	for( std::set<Alias*>::iterator a = qStageAliases.begin(); a != qStageAliases.end(); ++a )
		(*a)->addRange( loopStart, loopEnd );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RegisterAllocator::extendMultiQStageLiveRanges( std::list<Token>& tokens )
{
	for( std::list<Token>::iterator branch = tokens.begin(); branch != tokens.end(); ++branch )
	{
		if( !branch->operand() || branch->operand()->unit() != Operand::BRU )
			continue;

		std::list<Token::Argument>::const_iterator branchDest = branch->arguments().end();
		for( std::list<Token::Argument>::const_iterator a = branch->arguments().begin(); a != branch->arguments().end(); ++a )
		{
			if( a->flags() & Token::Argument::BRANCH )
			{
				branchDest = a;
				break;
			}
		}
		if( branchDest == branch->arguments().end() || branchDest->type() != Token::Argument::IMMEDIATE )
			continue;

		std::map< std::string, std::list<Token>::iterator >::iterator label = m_labels.find( branchDest->immediate() );
		if( label == m_labels.end() )
			continue;

		std::list<Token>::iterator target = label->second;
		if( target->lineNumber() >= branch->lineNumber() )
			continue;
		if( !loopTargetHasLoopDirective( target, tokens.end() ) )
			continue;

		extendMultiQStageRange( tokens, target->lineNumber(), branch->lineNumber() );
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RegisterAllocator::collectLiteralRegisterUsage( std::list<Token>& tokens )
{
	// One synthetic alias per (type, register) pair, with one range per
	// usage line.  Inserted into m_aliases pre-allocated to its physical
	// register so the existing conflict check in processAliases naturally
	// keeps user aliases off it.
	Alias* floats[32];
	Alias* integers[16];
	for( unsigned int i = 0; i < 32; i++ ) floats[i]   = NULL;
	for( unsigned int i = 0; i < 16; i++ ) integers[i] = NULL;

	for( std::list<Token>::iterator it = tokens.begin(); it != tokens.end(); ++it )
	{
		if( !it->operand() )
			continue;
		// Preprocessor directives (--enter, in_vf, .name, etc.) don't emit
		// hardware ops — their register references shouldn't pin physical
		// registers across the whole program.
		if( it->operand()->flags() & Operand::PREPROCESSOR )
			continue;
		if( it->flags() & Token::IGNORED )
			continue;

		const unsigned int line = it->lineNumber();

		for( std::list<Token::Argument>::const_iterator a = it->arguments().begin(); a != it->arguments().end(); ++a )
		{
			if( a->content() != Token::Argument::REGISTER )
				continue;

			if( a->type() == Token::Argument::FLOAT_REGISTER )
			{
				// Float side intentionally disabled — adding synthetic
				// VF aliases (even just for vf00, the zero register)
				// shifts the allocator's choices and ends up splitting
				// `vert_xform` between two VF sets across the [E]
				// halt boundary (load into VF05..VF08, use in
				// xform_loop as VF01..VF04 = garbage).  The integer-
				// side fix below is the one that matters for the
				// originally-motivating `do_clipping` / VI01 bug.
				(void)a;
				continue;
			}
			else if( a->type() == Token::Argument::INTEGER_REGISTER )
			{
				int r = a->regNumber();
				if( r < 0 || r >= 16 )
					continue;
				if( !integers[r] )
				{
					integers[r] = new Alias( Alias::INTEGER );
					integers[r]->setAllocatedRegister( &m_integers[r] );
					m_aliases[ integers[r] ] = integers[r];
				}
				integers[r]->addRange( line, line );
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Alias* RegisterAllocator::obtainAlias( Alias::Type type )
{
	Alias* alias = new Alias( type );

	m_aliases[ alias ] = alias;

	return alias;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RegisterAllocator::releaseAlias( Alias* alias )
{
	AliasMap::iterator i = m_aliases.find( alias );
	assert( i != m_aliases.end() );

	// Any alias whose same-name predecessor chain points at `alias` is
	// about to hold a dangling pointer; clear those edges so processAliases
	// doesn't walk into freed memory.  (Without this guard openvcl segfaults
	// when branch-state merges release one half of a previously-recorded
	// two-address pair.)
	for( AliasMap::iterator k = m_aliases.begin(); k != m_aliases.end(); ++k )
	{
		Alias* other = k->first;
		if( other == alias )
			continue;
		if( other->sameNamePredecessor() == alias )
			other->setSameNamePredecessor( NULL );
	}

	m_aliases.erase( i );

	delete alias;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RegisterAllocator::processCommonDirective( Token& token )
{
	if( !token.operand() )
		return true;

	if( token.operand()->name() == ".name" )
	{
		m_name = token.arguments().begin()->immediate();
		token.setFlags( token.flags() | Token::IGNORED );
		return true;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RegisterAllocator::updateDynamicTracker( const Token* src )
{
	std::map< const Token*, unsigned int >::iterator i = m_dynamicTracker.find( src );

	if( m_dynamicTracker.end() == i )
	{
		m_dynamicTracker[ src ] = 1;
		return true;
	}

	if( i->second > m_dynamicThreshold )
		return false;

	m_dynamicTracker[ src ] = i->second+1;
	return true;
}


}
