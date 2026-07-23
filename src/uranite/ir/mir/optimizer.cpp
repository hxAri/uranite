
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
#include <unordered_set>

#include "uranite/ir/mir/optimizer.hpp"

namespace uranite::ir::mir {
	
	void MIROptimizer::optimize( MIRModuleDefinition& moduleDefinition ) {
		for( std::shared_ptr<MIRFunctionDefinition>& functionDefinition : moduleDefinition.functionDefinitions ) {
			if( functionDefinition != nullptr ) {
				this->optimizeFunction( *functionDefinition );
			}
		}
	}
	
	void MIROptimizer::optimizeFunction( MIRFunctionDefinition& functionDefinition ) {
		bool changed = true;
		int iterationLimit = 20;
		int iterationCount = 0;
		
		while( changed && iterationCount < iterationLimit ) {
			changed = false;
			iterationCount++;
			
			changed |= this->eliminateUnreachableBlocks( functionDefinition );
			changed |= this->foldConstants( functionDefinition );
			changed |= this->propagateCopies( functionDefinition );
			changed |= this->eliminateDeadStores( functionDefinition );
			changed |= this->mergeLinearBlocks( functionDefinition );
		}
	}
	
	int MIROptimizer::removedInstructionCount() const {
		return this->totalRemovedInstructions;
	}
	
	int MIROptimizer::removedBlockCount() const {
		return this->totalRemovedBlocks;
	}
	
	bool MIROptimizer::eliminateDeadStores( MIRFunctionDefinition& functionDefinition ) {
		MIRLivenessAnalyzer livenessAnalysis;
		livenessAnalysis.analyze( functionDefinition );

		bool changed = false;

		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock == nullptr ) {
				continue;
			}

			// Collect variables produced by GEP — stores to these are pointer stores (never dead)
			std::unordered_set<MIRVariableIdentifier> gepResultVariables;
			std::unordered_set<MIRVariableIdentifier> indirectCallTargetVariables;
			for( const MIRInstruction& instruction : basicBlock->blockInstructions ) {
				if( ( instruction.instructionKind == MIRInstructionKind::ComputeFieldAddress ||
					  instruction.instructionKind == MIRInstructionKind::ComputeIndexAddress ) &&
					instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					gepResultVariables.insert( instruction.destinationVariable );
				}
				if( instruction.instructionKind == MIRInstructionKind::CallFunction &&
					instruction.calledFunctionQualifiedName.empty() == false ) {
					for( const std::pair<const MIRVariableIdentifier, MIRVariableDescriptor>& entry :
						functionDefinition.variableDescriptorTable ) {
						if( entry.second.variableName == instruction.calledFunctionQualifiedName ) {
							indirectCallTargetVariables.insert( entry.first );
							break;
						}
					}
				}
			}

			// Walk instructions backward, tracking which variables are read after current position
			std::unordered_set<MIRVariableIdentifier> neededAfter = basicBlock->liveVariablesAtExit;
			std::vector<MIRInstruction> survivingInstructions;
			survivingInstructions.reserve( basicBlock->blockInstructions.size() );
			bool blockChanged = false;

			for( int instructionIndex = static_cast<int>( basicBlock->blockInstructions.size() ) - 1;
				 instructionIndex >= 0; instructionIndex-- ) {

				const MIRInstruction& instruction = basicBlock->blockInstructions[instructionIndex];

				bool isDeadStore = false;

				if( instruction.instructionKind == MIRInstructionKind::StoreVariable &&
					instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER &&
					neededAfter.count( instruction.destinationVariable ) == 0 &&
					gepResultVariables.count( instruction.destinationVariable ) == 0 &&
					indirectCallTargetVariables.count( instruction.destinationVariable ) == 0 ) {
					isDeadStore = true;
				}
				
				if( isDeadStore ) {
					this->totalRemovedInstructions++;
					blockChanged = true;
				}
				else {
					survivingInstructions.push_back( instruction );
				}
				
				// Update neededAfter: remove destination (defined here), add sources (used here)
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					neededAfter.erase( instruction.destinationVariable );
				}
				for( MIRVariableIdentifier sourceOperand : instruction.sourceOperands ) {
					if( sourceOperand != INVALID_VARIABLE_IDENTIFIER ) {
						neededAfter.insert( sourceOperand );
					}
				}
			}
			
			if( blockChanged ) {
				std::reverse( survivingInstructions.begin(), survivingInstructions.end() );
				basicBlock->blockInstructions = std::move( survivingInstructions );
				changed = true;
			}
		}
		
		return changed;
	}
	
	bool MIROptimizer::propagateCopies( MIRFunctionDefinition& functionDefinition ) {
		bool changed = false;
		
		// Build copy map: dest → source for CopyValue and LoadVariable where source is single
		std::unordered_map<MIRVariableIdentifier, MIRVariableIdentifier> copySourceMap;
		
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock == nullptr ) {
				continue;
			}
			
			for( const MIRInstruction& instruction : basicBlock->blockInstructions ) {
				if( instruction.instructionKind == MIRInstructionKind::CopyValue &&
					instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER &&
					instruction.sourceOperands.size() == 1 &&
					instruction.sourceOperands[0] != INVALID_VARIABLE_IDENTIFIER ) {
					copySourceMap[instruction.destinationVariable] = instruction.sourceOperands[0];
				}
			}
		}
		
		if( copySourceMap.empty() ) {
			return false;
		}
		
		// Resolve transitive copies: if a→b→c, then a→c
		for( std::pair<const MIRVariableIdentifier, MIRVariableIdentifier>& copyEntry : copySourceMap ) {
			MIRVariableIdentifier resolvedSource = copyEntry.second;
			std::unordered_set<MIRVariableIdentifier> visited;
			visited.insert( copyEntry.first );
			
			while( copySourceMap.count( resolvedSource ) > 0 &&
				   visited.count( resolvedSource ) == 0 ) {
				visited.insert( resolvedSource );
				resolvedSource = copySourceMap[resolvedSource];
			}
			copyEntry.second = resolvedSource;
		}
		
		// Replace uses of copied variables with their ultimate source
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock == nullptr ) {
				continue;
			}
			
			for( MIRInstruction& instruction : basicBlock->blockInstructions ) {
				for( MIRVariableIdentifier& sourceOperand : instruction.sourceOperands ) {
					if( sourceOperand != INVALID_VARIABLE_IDENTIFIER &&
						copySourceMap.count( sourceOperand ) > 0 ) {
						sourceOperand = copySourceMap[sourceOperand];
						changed = true;
					}
				}
				
				// Phi incoming values
				for( std::pair<MIRBlockIdentifier, MIRVariableIdentifier>& phiEntry : instruction.phiIncomingValues ) {
					if( phiEntry.second != INVALID_VARIABLE_IDENTIFIER &&
						copySourceMap.count( phiEntry.second ) > 0 ) {
						phiEntry.second = copySourceMap[phiEntry.second];
						changed = true;
					}
				}
			}
		}
		
		return changed;
	}
	
	bool MIROptimizer::foldConstants( MIRFunctionDefinition& functionDefinition ) {
		bool changed = false;
		
		// Collect known constant values: variable → (kind, intVal, floatVal, boolVal)
		struct ConstantValue {
			MIRInstructionKind constantKind;
			int64_t integerValue = 0;
			double floatValue = 0.0;
			bool booleanValue = false;
		};
		std::unordered_map<MIRVariableIdentifier, ConstantValue> constantValues;
		
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock == nullptr ) {
				continue;
			}
			
			for( MIRInstruction& instruction : basicBlock->blockInstructions ) {
				// Record constant definitions
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					if( instruction.instructionKind == MIRInstructionKind::ConstantInteger ) {
						ConstantValue constVal;
						constVal.constantKind = MIRInstructionKind::ConstantInteger;
						constVal.integerValue = instruction.integerConstantValue;
						constantValues[instruction.destinationVariable] = constVal;
					}
					else if( instruction.instructionKind == MIRInstructionKind::ConstantFloat ) {
						ConstantValue constVal;
						constVal.constantKind = MIRInstructionKind::ConstantFloat;
						constVal.floatValue = instruction.floatConstantValue;
						constantValues[instruction.destinationVariable] = constVal;
					}
					else if( instruction.instructionKind == MIRInstructionKind::ConstantBoolean ) {
						ConstantValue constVal;
						constVal.constantKind = MIRInstructionKind::ConstantBoolean;
						constVal.booleanValue = instruction.booleanConstantValue;
						constantValues[instruction.destinationVariable] = constVal;
					}
				}
				
				// Try fold binary integer operations
				if( instruction.sourceOperands.size() == 2 ) {
					MIRVariableIdentifier leftOperand = instruction.sourceOperands[0];
					MIRVariableIdentifier rightOperand = instruction.sourceOperands[1];
					
					bool leftIsConstInt = (
						constantValues.count( leftOperand ) > 0 &&
						constantValues[leftOperand].constantKind == MIRInstructionKind::ConstantInteger
					);
					bool rightIsConstInt = (
						constantValues.count( rightOperand ) > 0 &&
						constantValues[rightOperand].constantKind == MIRInstructionKind::ConstantInteger
					);
					
					if( leftIsConstInt && rightIsConstInt &&
						instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
						
						int64_t leftValue = constantValues[leftOperand].integerValue;
						int64_t rightValue = constantValues[rightOperand].integerValue;
						bool folded = false;
						int64_t foldedResult = 0;
						
						switch( instruction.instructionKind ) {
							case MIRInstructionKind::AddInteger:
								foldedResult = leftValue + rightValue;
								folded = true;
								break;
							case MIRInstructionKind::SubtractInteger:
								foldedResult = leftValue - rightValue;
								folded = true;
								break;
							case MIRInstructionKind::MultiplyInteger:
								foldedResult = leftValue * rightValue;
								folded = true;
								break;
							case MIRInstructionKind::DivideInteger:
								if( rightValue != 0 ) {
									foldedResult = leftValue / rightValue;
									folded = true;
								}
								break;
							case MIRInstructionKind::ModuloInteger:
								if( rightValue != 0 ) {
									foldedResult = leftValue % rightValue;
									folded = true;
								}
								break;
							case MIRInstructionKind::BitwiseAnd:
								foldedResult = leftValue & rightValue;
								folded = true;
								break;
							case MIRInstructionKind::BitwiseOr:
								foldedResult = leftValue | rightValue;
								folded = true;
								break;
							case MIRInstructionKind::BitwiseXor:
								foldedResult = leftValue ^ rightValue;
								folded = true;
								break;
							case MIRInstructionKind::ShiftLeft:
								if( rightValue >= 0 && rightValue < 64 ) {
									foldedResult = leftValue << rightValue;
									folded = true;
								}
								break;
							case MIRInstructionKind::ShiftRight:
								if( rightValue >= 0 && rightValue < 64 ) {
									foldedResult = leftValue >> rightValue;
									folded = true;
								}
								break;
							default:
								break;
						}
						
						if( folded ) {
							instruction.instructionKind = MIRInstructionKind::ConstantInteger;
							instruction.integerConstantValue = foldedResult;
							instruction.sourceOperands.clear();
							
							ConstantValue newConst;
							newConst.constantKind = MIRInstructionKind::ConstantInteger;
							newConst.integerValue = foldedResult;
							constantValues[instruction.destinationVariable] = newConst;
							
							changed = true;
						}
					}
					
					// Fold integer comparisons
					if( leftIsConstInt && rightIsConstInt &&
						instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
						
						int64_t leftValue = constantValues[leftOperand].integerValue;
						int64_t rightValue = constantValues[rightOperand].integerValue;
						bool folded = false;
						bool comparisonResult = false;
						
						switch( instruction.instructionKind ) {
							case MIRInstructionKind::CompareEqual:
								comparisonResult = ( leftValue == rightValue );
								folded = true;
								break;
							case MIRInstructionKind::CompareNotEqual:
								comparisonResult = ( leftValue != rightValue );
								folded = true;
								break;
							case MIRInstructionKind::CompareLessThan:
								comparisonResult = ( leftValue < rightValue );
								folded = true;
								break;
							case MIRInstructionKind::CompareGreaterThan:
								comparisonResult = ( leftValue > rightValue );
								folded = true;
								break;
							case MIRInstructionKind::CompareLessEqual:
								comparisonResult = ( leftValue <= rightValue );
								folded = true;
								break;
							case MIRInstructionKind::CompareGreaterEqual:
								comparisonResult = ( leftValue >= rightValue );
								folded = true;
								break;
							default:
								break;
						}
						
						if( folded ) {
							instruction.instructionKind = MIRInstructionKind::ConstantBoolean;
							instruction.booleanConstantValue = comparisonResult;
							instruction.sourceOperands.clear();
							
							ConstantValue newConst;
							newConst.constantKind = MIRInstructionKind::ConstantBoolean;
							newConst.booleanValue = comparisonResult;
							constantValues[instruction.destinationVariable] = newConst;
							
							changed = true;
						}
					}
					
					// Fold float binary operations
					bool leftIsConstFloat = (
						constantValues.count( leftOperand ) > 0 &&
						constantValues[leftOperand].constantKind == MIRInstructionKind::ConstantFloat
					);
					bool rightIsConstFloat = (
						constantValues.count( rightOperand ) > 0 &&
						constantValues[rightOperand].constantKind == MIRInstructionKind::ConstantFloat
					);
					
					if( leftIsConstFloat && rightIsConstFloat &&
						instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
						
						double leftValue = constantValues[leftOperand].floatValue;
						double rightValue = constantValues[rightOperand].floatValue;
						bool folded = false;
						double foldedResult = 0.0;
						
						switch( instruction.instructionKind ) {
							case MIRInstructionKind::AddFloat:
								foldedResult = leftValue + rightValue;
								folded = true;
								break;
							case MIRInstructionKind::SubtractFloat:
								foldedResult = leftValue - rightValue;
								folded = true;
								break;
							case MIRInstructionKind::MultiplyFloat:
								foldedResult = leftValue * rightValue;
								folded = true;
								break;
							case MIRInstructionKind::DivideFloat:
								if( rightValue != 0.0 ) {
									foldedResult = leftValue / rightValue;
									folded = true;
								}
								break;
							default:
								break;
						}
						
						if( folded ) {
							instruction.instructionKind = MIRInstructionKind::ConstantFloat;
							instruction.floatConstantValue = foldedResult;
							instruction.sourceOperands.clear();
							
							ConstantValue newConst;
							newConst.constantKind = MIRInstructionKind::ConstantFloat;
							newConst.floatValue = foldedResult;
							constantValues[instruction.destinationVariable] = newConst;
							
							changed = true;
						}
					}
				}
				
				// Fold unary negate on constants
				if( instruction.sourceOperands.size() == 1 &&
					instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					
					MIRVariableIdentifier operand = instruction.sourceOperands[0];
					
					if( instruction.instructionKind == MIRInstructionKind::NegateInteger &&
						constantValues.count( operand ) > 0 &&
						constantValues[operand].constantKind == MIRInstructionKind::ConstantInteger ) {
						
						instruction.instructionKind = MIRInstructionKind::ConstantInteger;
						instruction.integerConstantValue = -constantValues[operand].integerValue;
						instruction.sourceOperands.clear();
						
						ConstantValue newConst;
						newConst.constantKind = MIRInstructionKind::ConstantInteger;
						newConst.integerValue = instruction.integerConstantValue;
						constantValues[instruction.destinationVariable] = newConst;
						
						changed = true;
					}
					else if( instruction.instructionKind == MIRInstructionKind::NegateFloat &&
							 constantValues.count( operand ) > 0 &&
							 constantValues[operand].constantKind == MIRInstructionKind::ConstantFloat ) {
						
						instruction.instructionKind = MIRInstructionKind::ConstantFloat;
						instruction.floatConstantValue = -constantValues[operand].floatValue;
						instruction.sourceOperands.clear();
						
						ConstantValue newConst;
						newConst.constantKind = MIRInstructionKind::ConstantFloat;
						newConst.floatValue = instruction.floatConstantValue;
						constantValues[instruction.destinationVariable] = newConst;
						
						changed = true;
					}
					else if( instruction.instructionKind == MIRInstructionKind::LogicalNot &&
							 constantValues.count( operand ) > 0 &&
							 constantValues[operand].constantKind == MIRInstructionKind::ConstantBoolean ) {
						
						instruction.instructionKind = MIRInstructionKind::ConstantBoolean;
						instruction.booleanConstantValue = ( constantValues[operand].booleanValue == false );
						instruction.sourceOperands.clear();
						
						ConstantValue newConst;
						newConst.constantKind = MIRInstructionKind::ConstantBoolean;
						newConst.booleanValue = instruction.booleanConstantValue;
						constantValues[instruction.destinationVariable] = newConst;
						
						changed = true;
					}
				}
			}
		}
		
		return changed;
	}
	
	bool MIROptimizer::mergeLinearBlocks( MIRFunctionDefinition& functionDefinition ) {
		MIRLivenessAnalyzer livenessAnalysis;
		livenessAnalysis.analyze( functionDefinition );
		
		bool changed = false;
		
		for( size_t blockIndex = 0; blockIndex < functionDefinition.controlFlowBlocks.size(); blockIndex++ ) {
			std::shared_ptr<MIRBasicBlock>& currentBlock = functionDefinition.controlFlowBlocks[blockIndex];
			if( currentBlock == nullptr || currentBlock->blockInstructions.empty() ) {
				continue;
			}
			
			// Check if last instruction is unconditional jump to a block with single predecessor
			const MIRInstruction& lastInstruction = currentBlock->blockInstructions.back();
			if( lastInstruction.instructionKind != MIRInstructionKind::JumpUnconditional ) {
				continue;
			}
			
			MIRBlockIdentifier targetIdentifier = lastInstruction.trueBranchTarget;
			if( targetIdentifier == INVALID_BLOCK_IDENTIFIER ||
				targetIdentifier >= functionDefinition.controlFlowBlocks.size() ) {
				continue;
			}
			
			std::shared_ptr<MIRBasicBlock>& targetBlock = functionDefinition.controlFlowBlocks[targetIdentifier];
			if( targetBlock == nullptr ) {
				continue;
			}
			
			// Target must have exactly one predecessor (this block)
			if( targetBlock->predecessorBlocks.size() != 1 ) {
				continue;
			}
			
			// Don't merge entry block away
			if( targetIdentifier == functionDefinition.entryBlockIdentifier ) {
				continue;
			}
			
			// Don't self-merge
			if( targetIdentifier == currentBlock->blockIdentifier ) {
				continue;
			}
			
			// Merge: remove jump from current, append target's instructions
			currentBlock->blockInstructions.pop_back();
			currentBlock->blockInstructions.insert(
				currentBlock->blockInstructions.end(),
				targetBlock->blockInstructions.begin(),
				targetBlock->blockInstructions.end()
			);
			currentBlock->isTerminated = targetBlock->isTerminated;
			
			// Update successors: current now has target's successors
			currentBlock->successorBlocks = targetBlock->successorBlocks;
			
			// Update predecessors of target's successors to point to current
			for( MIRBlockIdentifier successorIdentifier : targetBlock->successorBlocks ) {
				if( successorIdentifier < functionDefinition.controlFlowBlocks.size() &&
					functionDefinition.controlFlowBlocks[successorIdentifier] != nullptr ) {
					std::shared_ptr<MIRBasicBlock>& successorBlock =
						functionDefinition.controlFlowBlocks[successorIdentifier];
					for( MIRBlockIdentifier& predecessorIdentifier : successorBlock->predecessorBlocks ) {
						if( predecessorIdentifier == targetIdentifier ) {
							predecessorIdentifier = currentBlock->blockIdentifier;
						}
					}
				}
			}
			
			// Rewrite branch targets in merged instructions that reference target
			for( MIRInstruction& instruction : currentBlock->blockInstructions ) {
				if( instruction.trueBranchTarget == targetIdentifier ) {
					instruction.trueBranchTarget = currentBlock->blockIdentifier;
				}
				if( instruction.falseBranchTarget == targetIdentifier ) {
					instruction.falseBranchTarget = currentBlock->blockIdentifier;
				}
				if( instruction.defaultSwitchTarget == targetIdentifier ) {
					instruction.defaultSwitchTarget = currentBlock->blockIdentifier;
				}
				for( std::pair<int64_t, MIRBlockIdentifier>& switchTarget : instruction.switchBranchTargets ) {
					if( switchTarget.second == targetIdentifier ) {
						switchTarget.second = currentBlock->blockIdentifier;
					}
				}
			}
			
			// Null out merged block
			functionDefinition.controlFlowBlocks[targetIdentifier] = nullptr;
			this->totalRemovedBlocks++;
			changed = true;
			
			// Re-check same block index (might chain-merge)
			blockIndex--;
		}
		
		return changed;
	}
	
	bool MIROptimizer::eliminateUnreachableBlocks( MIRFunctionDefinition& functionDefinition ) {
		if( functionDefinition.controlFlowBlocks.empty() ) {
			return false;
		}
		
		// BFS from entry to find all reachable blocks
		std::unordered_set<MIRBlockIdentifier> reachableBlocks;
		std::vector<MIRBlockIdentifier> worklist;
		worklist.push_back( functionDefinition.entryBlockIdentifier );
		reachableBlocks.insert( functionDefinition.entryBlockIdentifier );
		
		while( worklist.empty() == false ) {
			MIRBlockIdentifier currentIdentifier = worklist.back();
			worklist.pop_back();
			
			if( currentIdentifier >= functionDefinition.controlFlowBlocks.size() ) {
				continue;
			}
			
			std::shared_ptr<MIRBasicBlock>& currentBlock = functionDefinition.controlFlowBlocks[currentIdentifier];
			if( currentBlock == nullptr ) {
				continue;
			}
			
			for( MIRBlockIdentifier successorIdentifier : currentBlock->successorBlocks ) {
				if( reachableBlocks.count( successorIdentifier ) == 0 ) {
					reachableBlocks.insert( successorIdentifier );
					worklist.push_back( successorIdentifier );
				}
			}
		}
		
		bool changed = false;
		
		for( size_t blockIndex = 0; blockIndex < functionDefinition.controlFlowBlocks.size(); blockIndex++ ) {
			MIRBlockIdentifier blockIdentifier = static_cast<MIRBlockIdentifier>( blockIndex );
			
			if( functionDefinition.controlFlowBlocks[blockIndex] != nullptr &&
				reachableBlocks.count( blockIdentifier ) == 0 ) {
				
				// Remove this block from predecessors of its successors
				std::shared_ptr<MIRBasicBlock>& deadBlock = functionDefinition.controlFlowBlocks[blockIndex];
				for( MIRBlockIdentifier successorIdentifier : deadBlock->successorBlocks ) {
					if( successorIdentifier < functionDefinition.controlFlowBlocks.size() &&
						functionDefinition.controlFlowBlocks[successorIdentifier] != nullptr ) {
						std::vector<MIRBlockIdentifier>& predecessors =
							functionDefinition.controlFlowBlocks[successorIdentifier]->predecessorBlocks;
						predecessors.erase(
							std::remove( predecessors.begin(), predecessors.end(), blockIdentifier ),
							predecessors.end()
						);
					}
				}
				
				functionDefinition.controlFlowBlocks[blockIndex] = nullptr;
				this->totalRemovedBlocks++;
				changed = true;
			}
		}
		
		return changed;
	}

} // namespace uranite::ir::mir
