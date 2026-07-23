
//
// @author hxAri (hxari)
// @create 13-06-2026
// @update 2026-06-17 20:03
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include <algorithm>

#include "uranite/ir/mir/analyzer.hpp"

namespace uranite::ir::mir {
	
	void MIRLivenessAnalyzer::analyze( MIRFunctionDefinition& functionDefinition ) {
		this->buildPredecessorSuccessorEdges( functionDefinition );
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock != nullptr ) {
				this->computeLocalSets( *basicBlock );
			}
		}
		bool changed = true;
		while( changed ) {
			changed = this->propagateBackward( functionDefinition );
		}
	}
	
	void MIRLivenessAnalyzer::computeLocalSets( MIRBasicBlock& basicBlock ) {
		basicBlock.definedVariables.clear();
		basicBlock.usedVariables.clear();
		for( const MIRInstruction& instruction : basicBlock.blockInstructions ) {
			for( MIRVariableIdentifier sourceOperand : instruction.sourceOperands ) {
				if( sourceOperand != INVALID_VARIABLE_IDENTIFIER &&
					basicBlock.definedVariables.count( sourceOperand ) == 0 ) {
					basicBlock.usedVariables.insert( sourceOperand );
				}
			}
			for( const std::pair<MIRBlockIdentifier, MIRVariableIdentifier>& phiEntry : instruction.phiIncomingValues ) {
				if( phiEntry.second != INVALID_VARIABLE_IDENTIFIER &&
					basicBlock.definedVariables.count( phiEntry.second ) == 0 ) {
					basicBlock.usedVariables.insert( phiEntry.second );
				}
			}
			if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
				basicBlock.definedVariables.insert( instruction.destinationVariable );
			}
		}
	}
	
	void MIRLivenessAnalyzer::buildPredecessorSuccessorEdges( MIRFunctionDefinition& functionDefinition ) {
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock != nullptr ) {
				basicBlock->predecessorBlocks.clear();
				basicBlock->successorBlocks.clear();
			}
		}
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock == nullptr || basicBlock->blockInstructions.empty() ) {
				continue;
			}
			const MIRInstruction& lastInstruction = basicBlock->blockInstructions.back();
			auto addEdge = [&]( MIRBlockIdentifier targetIdentifier ) {
				if( targetIdentifier == INVALID_BLOCK_IDENTIFIER ||
					targetIdentifier >= functionDefinition.controlFlowBlocks.size() ||
					functionDefinition.controlFlowBlocks[targetIdentifier] == nullptr ) {
					return;
				}
				basicBlock->successorBlocks.push_back( targetIdentifier );
				functionDefinition.controlFlowBlocks[targetIdentifier]->predecessorBlocks.push_back(
					basicBlock->blockIdentifier
				);
			};
			switch( lastInstruction.instructionKind ) {
				case MIRInstructionKind::BranchConditional:
					addEdge( lastInstruction.trueBranchTarget );
					addEdge( lastInstruction.falseBranchTarget );
					break;
				case MIRInstructionKind::JumpUnconditional:
					addEdge( lastInstruction.trueBranchTarget );
					break;
				case MIRInstructionKind::SwitchBranch:
					for( const std::pair<int64_t, MIRBlockIdentifier>& switchTarget : lastInstruction.switchBranchTargets ) {
						addEdge( switchTarget.second );
					}
					addEdge( lastInstruction.defaultSwitchTarget );
					break;
				case MIRInstructionKind::InvokeFunction:
					addEdge( lastInstruction.trueBranchTarget );
					addEdge( lastInstruction.landingPadTarget );
					break;
				case MIRInstructionKind::ThrowException:
					addEdge( lastInstruction.landingPadTarget );
					break;
				case MIRInstructionKind::ReturnValue:
				case MIRInstructionKind::Unreachable:
					break;
				default:
					break;
			}
		}
	}
	
	bool MIRLivenessAnalyzer::propagateBackward( MIRFunctionDefinition& functionDefinition ) {
		bool changed = false;
		for( int blockIndex = static_cast<int>( functionDefinition.controlFlowBlocks.size() ) - 1;
			 blockIndex >= 0; blockIndex-- ) {
			std::shared_ptr<MIRBasicBlock>& basicBlock = functionDefinition.controlFlowBlocks[blockIndex];
			if( basicBlock == nullptr ) {
				continue;
			}
			std::unordered_set<MIRVariableIdentifier> newLiveOut;
			for( MIRBlockIdentifier successorIdentifier : basicBlock->successorBlocks ) {
				if( successorIdentifier < functionDefinition.controlFlowBlocks.size() ) {
					const std::shared_ptr<MIRBasicBlock>& successorBlock =
						functionDefinition.controlFlowBlocks[successorIdentifier];
					if( successorBlock != nullptr ) {
						newLiveOut.insert(
							successorBlock->liveVariablesAtEntry.begin(),
							successorBlock->liveVariablesAtEntry.end()
						);
					}
				}
			}
			std::unordered_set<MIRVariableIdentifier> newLiveIn = basicBlock->usedVariables;
			for( MIRVariableIdentifier liveOutVariable : newLiveOut ) {
				if( basicBlock->definedVariables.count( liveOutVariable ) == 0 ) {
					newLiveIn.insert( liveOutVariable );
				}
			}
			if( newLiveIn != basicBlock->liveVariablesAtEntry ||
				newLiveOut != basicBlock->liveVariablesAtExit ) {
				changed = true;
				basicBlock->liveVariablesAtEntry = std::move( newLiveIn );
				basicBlock->liveVariablesAtExit = std::move( newLiveOut );
			}
		}
		return changed;
	}
	
} // namespace uranite::ir::mir
