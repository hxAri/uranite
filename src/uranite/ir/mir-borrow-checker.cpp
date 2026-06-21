
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

#include "uranite/ir/mir-borrow-checker.hpp"

#include <fmt/format.h>

namespace uranite::ir::mir {
	
	MIRBorrowChecker::MIRBorrowChecker( diagnostic::Engine& diagnosticEngine )
		: diagnosticEngine( diagnosticEngine ) {
	}
	
	void MIRBorrowChecker::check( MIRFunctionDefinition& functionDefinition ) {
		this->detectedViolations = 0;
		this->propagateOwnershipStates( functionDefinition );
	}
	
	int MIRBorrowChecker::violationCount() const {
		return this->detectedViolations;
	}
	
	void MIRBorrowChecker::propagateOwnershipStates( MIRFunctionDefinition& functionDefinition ) {
		// Per-block entry ownership states
		std::unordered_map<MIRBlockIdentifier, OwnershipStateMap> blockEntryStates;
		
		// Initialize entry block: parameters are Owned, all others Uninitialized
		OwnershipStateMap entryState;
		for( MIRVariableIdentifier parameterVariable : functionDefinition.parameterVariableIdentifiers ) {
			entryState[parameterVariable] = BorrowOwnershipState::Owned;
		}
		blockEntryStates[functionDefinition.entryBlockIdentifier] = entryState;
		
		// Fixed-point iteration
		bool changed = true;
		int iterationCount = 0;
		int maxIterations = static_cast<int>( functionDefinition.controlFlowBlocks.size() ) * 4 + 10;
		
		while( changed && iterationCount < maxIterations ) {
			changed = false;
			iterationCount++;
			
			for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
				if( basicBlock == nullptr ) {
					continue;
				}
				
				// Merge predecessor states
				OwnershipStateMap mergedEntryState;
				if( basicBlock->blockIdentifier == functionDefinition.entryBlockIdentifier ) {
					mergedEntryState = blockEntryStates[basicBlock->blockIdentifier];
				}
				else if( basicBlock->predecessorBlocks.empty() == false ) {
					std::vector<OwnershipStateMap> predecessorMaps;
					for( MIRBlockIdentifier predecessorIdentifier : basicBlock->predecessorBlocks ) {
						if( blockEntryStates.count( predecessorIdentifier ) > 0 ) {
							// Compute exit state by replaying instructions on entry state
							OwnershipStateMap predecessorExitState = blockEntryStates[predecessorIdentifier];
							std::shared_ptr<MIRBasicBlock>& predecessorBlock =
								functionDefinition.controlFlowBlocks[predecessorIdentifier];
							if( predecessorBlock != nullptr ) {
								for( const MIRInstruction& instruction : predecessorBlock->blockInstructions ) {
									this->checkInstruction( instruction, predecessorExitState, functionDefinition );
								}
							}
							predecessorMaps.push_back( std::move( predecessorExitState ) );
						}
					}
					if( predecessorMaps.empty() == false ) {
						mergedEntryState = this->mergeOwnershipMaps( predecessorMaps );
					}
				}
				
				// Check for change
				if( blockEntryStates.count( basicBlock->blockIdentifier ) == 0 ||
					blockEntryStates[basicBlock->blockIdentifier] != mergedEntryState ) {
					blockEntryStates[basicBlock->blockIdentifier] = mergedEntryState;
					changed = true;
				}
			}
		}
		
		// Final pass: check all instructions with converged states
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock == nullptr ) {
				continue;
			}
			
			OwnershipStateMap currentState;
			if( blockEntryStates.count( basicBlock->blockIdentifier ) > 0 ) {
				currentState = blockEntryStates[basicBlock->blockIdentifier];
			}
			
			for( const MIRInstruction& instruction : basicBlock->blockInstructions ) {
				this->checkInstruction( instruction, currentState, functionDefinition );
			}
		}
	}
	
	void MIRBorrowChecker::checkInstruction(
		const MIRInstruction& instruction,
		OwnershipStateMap& currentStates,
		const MIRFunctionDefinition& functionDefinition
	) {
		// Check source operands for use-after-move
		for( MIRVariableIdentifier sourceOperand : instruction.sourceOperands ) {
			if( sourceOperand == INVALID_VARIABLE_IDENTIFIER ) {
				continue;
			}
			
			if( currentStates.count( sourceOperand ) > 0 ) {
				BorrowOwnershipState operandState = currentStates[sourceOperand];
				
				if( operandState == BorrowOwnershipState::Moved ) {
					std::string variableName = "v" + std::to_string( sourceOperand );
					if( functionDefinition.variableDescriptorTable.count( sourceOperand ) > 0 ) {
						variableName = functionDefinition.variableDescriptorTable.at( sourceOperand ).variableName;
					}
					this->reportViolation(
						fmt::format( "use of moved variable '{}'", variableName ),
						instruction.sourceLocation
					);
				}
			}
		}
		
		// Update state based on instruction kind
		switch( instruction.instructionKind ) {
			case MIRInstructionKind::MoveValue: {
				// Source is moved
				if( instruction.sourceOperands.empty() == false ) {
					MIRVariableIdentifier sourceVariable = instruction.sourceOperands[0];
					if( sourceVariable != INVALID_VARIABLE_IDENTIFIER ) {
						currentStates[sourceVariable] = BorrowOwnershipState::Moved;
					}
				}
				// Destination is now owned
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					currentStates[instruction.destinationVariable] = BorrowOwnershipState::Owned;
				}
				break;
			}
			
			case MIRInstructionKind::CopyValue:
			case MIRInstructionKind::AllocateLocal:
			case MIRInstructionKind::ConstantInteger:
			case MIRInstructionKind::ConstantFloat:
			case MIRInstructionKind::ConstantBoolean:
			case MIRInstructionKind::ConstantString:
			case MIRInstructionKind::ConstantChar:
			case MIRInstructionKind::ConstantNone:
			case MIRInstructionKind::ConstructObject:
			case MIRInstructionKind::CallFunction:
			case MIRInstructionKind::LoadVariable:
			case MIRInstructionKind::ComputeFieldAddress:
			case MIRInstructionKind::ComputeIndexAddress:
			case MIRInstructionKind::CastType:
			case MIRInstructionKind::AddressOf:
			case MIRInstructionKind::HeapAllocate: {
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					currentStates[instruction.destinationVariable] = BorrowOwnershipState::Owned;
				}
				break;
			}
			
			case MIRInstructionKind::TakeReference: {
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					currentStates[instruction.destinationVariable] = BorrowOwnershipState::Borrowed;
				}
				if( instruction.sourceOperands.empty() == false ) {
					MIRVariableIdentifier sourceVariable = instruction.sourceOperands[0];
					if( sourceVariable != INVALID_VARIABLE_IDENTIFIER ) {
						currentStates[sourceVariable] = BorrowOwnershipState::Borrowed;
					}
				}
				break;
			}
			
			case MIRInstructionKind::DropValue:
			case MIRInstructionKind::HeapFree: {
				if( instruction.sourceOperands.empty() == false ) {
					MIRVariableIdentifier sourceVariable = instruction.sourceOperands[0];
					if( sourceVariable != INVALID_VARIABLE_IDENTIFIER ) {
						if( currentStates.count( sourceVariable ) > 0 &&
							currentStates[sourceVariable] == BorrowOwnershipState::Moved ) {
							std::string variableName = "v" + std::to_string( sourceVariable );
							if( functionDefinition.variableDescriptorTable.count( sourceVariable ) > 0 ) {
								variableName = functionDefinition.variableDescriptorTable.at( sourceVariable ).variableName;
							}
							this->reportViolation(
								fmt::format( "double-free: dropping already moved variable '{}'", variableName ),
								instruction.sourceLocation
							);
						}
						currentStates[sourceVariable] = BorrowOwnershipState::Moved;
					}
				}
				break;
			}
			
			case MIRInstructionKind::StoreVariable: {
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					currentStates[instruction.destinationVariable] = BorrowOwnershipState::Owned;
				}
				break;
			}
			
			default: {
				// Arithmetic, comparison, logical, branch, etc. — destination is Owned if produced
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					currentStates[instruction.destinationVariable] = BorrowOwnershipState::Owned;
				}
				break;
			}
		}
	}
	
	OwnershipStateMap MIRBorrowChecker::mergeOwnershipMaps(
		const std::vector<OwnershipStateMap>& predecessorStates
	) {
		if( predecessorStates.empty() ) {
			return {};
		}
		if( predecessorStates.size() == 1 ) {
			return predecessorStates[0];
		}
		
		OwnershipStateMap merged = predecessorStates[0];
		
		for( size_t mapIndex = 1; mapIndex < predecessorStates.size(); mapIndex++ ) {
			const OwnershipStateMap& otherMap = predecessorStates[mapIndex];
			
			// Add variables only in other map
			for( const std::pair<const MIRVariableIdentifier, BorrowOwnershipState>& entry : otherMap ) {
				if( merged.count( entry.first ) == 0 ) {
					merged[entry.first] = entry.second;
				}
				else {
					// Conservative merge: Moved wins over Owned
					BorrowOwnershipState existingState = merged[entry.first];
					BorrowOwnershipState incomingState = entry.second;
					
					if( existingState == BorrowOwnershipState::Moved ||
						incomingState == BorrowOwnershipState::Moved ) {
						merged[entry.first] = BorrowOwnershipState::Moved;
					}
					else if( existingState == BorrowOwnershipState::Uninitialized ||
							 incomingState == BorrowOwnershipState::Uninitialized ) {
						merged[entry.first] = BorrowOwnershipState::Uninitialized;
					}
					else if( existingState == BorrowOwnershipState::MutablyBorrowed ||
							 incomingState == BorrowOwnershipState::MutablyBorrowed ) {
						merged[entry.first] = BorrowOwnershipState::MutablyBorrowed;
					}
					else if( existingState == BorrowOwnershipState::Borrowed ||
							 incomingState == BorrowOwnershipState::Borrowed ) {
						merged[entry.first] = BorrowOwnershipState::Borrowed;
					}
				}
			}
		}
		
		return merged;
	}
	
	void MIRBorrowChecker::reportViolation(
		const std::string& message,
		const lookup::SourceSharedPointer& sourceLocation
	) {
		this->detectedViolations++;
		if( sourceLocation != nullptr && sourceLocation->location != nullptr ) {
			this->diagnosticEngine.error(
				sourceLocation,
				fmt::format( "borrow check: {}", message )
			);
		}
	}

} // namespace uranite::ir::mir
