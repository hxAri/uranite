
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

#include "uranite/ir/mir-lowering.hpp"

#include <fmt/format.h>

namespace uranite::ir::mir {
	
	MIRLowering::MIRLowering( diagnostic::Engine& diagnosticEngine )
		: diagnosticEngine( diagnosticEngine ) {
	}
	
	std::shared_ptr<MIRModuleDefinition> MIRLowering::lower( hir::HIRModule& hirModule ) {
		this->currentModule = std::make_shared<MIRModuleDefinition>();
		this->currentModule->moduleName = hirModule.moduleName;
		
		for( std::shared_ptr<hir::HIRClassDefinition>& classDefinition : hirModule.classDefinitions ) {
			if( classDefinition != nullptr ) {
				this->lowerClassDefinition( *classDefinition );
			}
		}
		
		for( std::shared_ptr<hir::HIRFunctionDefinition>& functionDefinition : hirModule.functionDefinitions ) {
			if( functionDefinition != nullptr ) {
				this->lowerFunctionDefinition( *functionDefinition );
			}
		}
		
		return this->currentModule;
	}
	
	// ===================================================================
	// Declaration Lowering
	// ===================================================================
	
	void MIRLowering::lowerFunctionDefinition( hir::HIRFunctionDefinition& hirFunction ) {
		std::shared_ptr<MIRFunctionDefinition> mirFunction = std::make_shared<MIRFunctionDefinition>();
		mirFunction->functionName = hirFunction.functionName;
		mirFunction->mangledFunctionName = hirFunction.mangledName;
		mirFunction->ownerClassQualifiedName = hirFunction.ownerClassName;
		mirFunction->returnTypeDescriptor = hirFunction.returnTypeDescriptor;
		
		this->currentFunction = mirFunction;
		this->nextInstructionIdentifier = 0;
		this->variableNameMap.clear();
		
		std::shared_ptr<MIRBasicBlock> entryBlock = mirFunction->createBasicBlock( "entry" );
		this->switchToBlock( entryBlock );
		mirFunction->entryBlockIdentifier = entryBlock->blockIdentifier;
		
		for( const hir::HIRParameterDescriptor& parameterDescriptor : hirFunction.parameterDescriptors ) {
			MIRVariableIdentifier parameterVariable = mirFunction->allocateVariable(
				parameterDescriptor.parameterName,
				parameterDescriptor.parameterType,
				parameterDescriptor.isMutableParameter
			);
			mirFunction->variableDescriptorTable[parameterVariable].isParameterVariable = true;
			mirFunction->parameterVariableIdentifiers.push_back( parameterVariable );
			this->variableNameMap[parameterDescriptor.parameterName] = parameterVariable;
		}
		
		if( hirFunction.functionBody != nullptr ) {
			this->lowerBlock( *hirFunction.functionBody );
		}
		
		this->ensureBlockTerminated();

		this->currentModule->functionDefinitions.push_back( std::move( mirFunction ) );
		this->currentFunction = nullptr;
		this->currentBlock = nullptr;
	}
	
	void MIRLowering::lowerClassDefinition( hir::HIRClassDefinition& hirClass ) {
		TypeLayoutDescriptor typeLayout;
		typeLayout.typeQualifiedName = hirClass.classQualifiedName;
		typeLayout.hasVirtualTable = false;
		
		int fieldIndex = 0;
		for( const hir::HIRFieldDescriptor& fieldDescriptor : hirClass.fieldDescriptors ) {
			typeLayout.fieldByteOffsets.push_back( fieldIndex * 8 );
			typeLayout.fieldNames.push_back( fieldDescriptor.fieldName );
			typeLayout.fieldTypes.push_back( fieldDescriptor.fieldType );
			fieldIndex++;
		}
		typeLayout.typeSizeInBytes = fieldIndex * 8;
		typeLayout.typeAlignmentInBytes = 8;
		
		this->currentModule->typeLayoutTable[hirClass.className] = std::move( typeLayout );

		std::string savedClassName = this->currentClassName;
		std::string savedParentClassName = this->currentParentClassName;
		this->currentClassName = hirClass.className;
		this->currentParentClassName = hirClass.parentClassQualifiedName;

		for( std::shared_ptr<hir::HIRFunctionDefinition>& methodDefinition : hirClass.methodDefinitions ) {
			if( methodDefinition != nullptr ) {
				this->lowerFunctionDefinition( *methodDefinition );
			}
		}

		this->currentClassName = savedClassName;
		this->currentParentClassName = savedParentClassName;
	}
	
	// ===================================================================
	// Statement Lowering
	// ===================================================================
	
	void MIRLowering::lowerBlock( hir::HIRBlock& hirBlock ) {
		for( const hir::HIRNodeSharedPointer& statement : hirBlock.blockStatements ) {
			if( statement != nullptr ) {
				this->lowerStatement( statement );
			}
		}
	}
	
	void MIRLowering::lowerStatement( const hir::HIRNodeSharedPointer& hirStatement ) {
		if( hirStatement == nullptr || this->currentBlock == nullptr ) {
			return;
		}
		
		if( this->currentBlock->isTerminated ) {
			return;
		}
		
		switch( hirStatement->nodeKind ) {
			case hir::HIRNodeKind::VariableBinding: {
				hir::HIRVariableBinding& binding = static_cast<hir::HIRVariableBinding&>( *hirStatement );
				MIRVariableIdentifier variableIdentifier = this->currentFunction->allocateVariable(
					binding.variableName,
					binding.variableType,
					binding.isMutableBinding
				);
				this->variableNameMap[binding.variableName] = variableIdentifier;

				MIRInstruction allocateInstruction( MIRInstructionKind::AllocateLocal );
				allocateInstruction.destinationVariable = variableIdentifier;
				allocateInstruction.operandType = binding.variableType;
				allocateInstruction.sourceLocation = binding.sourceLocation;
				this->emitInstruction( allocateInstruction );
				
				if( binding.initializerExpression != nullptr ) {
					MIRVariableIdentifier initializerValue = this->lowerExpression( binding.initializerExpression );
					MIRInstruction storeInstruction( MIRInstructionKind::StoreVariable );
					storeInstruction.destinationVariable = variableIdentifier;
					storeInstruction.sourceOperands.push_back( initializerValue );
					storeInstruction.sourceLocation = binding.sourceLocation;
					this->emitInstruction( storeInstruction );
				}
				break;
			}
			
			case hir::HIRNodeKind::Assignment: {
				hir::HIRAssignment& assignment = static_cast<hir::HIRAssignment&>( *hirStatement );
				MIRVariableIdentifier valueVariable = this->lowerExpression( assignment.valueExpression );
				MIRVariableIdentifier targetVariable = INVALID_VARIABLE_IDENTIFIER;
				if( assignment.targetExpression != nullptr &&
					assignment.targetExpression->nodeKind == hir::HIRNodeKind::Identifier ) {
					hir::HIRIdentifier& targetIdentifier =
						static_cast<hir::HIRIdentifier&>( *assignment.targetExpression );
					std::unordered_map<std::string, MIRVariableIdentifier>::iterator targetLookup =
						this->variableNameMap.find( targetIdentifier.identifierName );
					if( targetLookup != this->variableNameMap.end() ) {
						targetVariable = targetLookup->second;
					}
				}
				else {
					targetVariable = this->lowerExpression( assignment.targetExpression );
				}
				MIRInstruction storeInstruction( MIRInstructionKind::StoreVariable );
				storeInstruction.destinationVariable = targetVariable;
				storeInstruction.sourceOperands.push_back( valueVariable );
				storeInstruction.sourceLocation = assignment.sourceLocation;
				this->emitInstruction( storeInstruction );
				break;
			}
			
			case hir::HIRNodeKind::Return: {
				hir::HIRReturn& returnNode = static_cast<hir::HIRReturn&>( *hirStatement );
				MIRInstruction returnInstruction( MIRInstructionKind::ReturnValue );
				returnInstruction.sourceLocation = returnNode.sourceLocation;
				if( returnNode.returnValueExpression != nullptr ) {
					MIRVariableIdentifier returnValue = this->lowerExpression( returnNode.returnValueExpression );
					returnInstruction.sourceOperands.push_back( returnValue );
				}
				this->emitTerminator( returnInstruction );
				break;
			}
			
			case hir::HIRNodeKind::If: {
				hir::HIRIf& ifNode = static_cast<hir::HIRIf&>( *hirStatement );
				this->lowerIf( ifNode );
				break;
			}
			
			case hir::HIRNodeKind::Loop: {
				hir::HIRLoop& loopNode = static_cast<hir::HIRLoop&>( *hirStatement );
				this->lowerLoop( loopNode );
				break;
			}
			
			case hir::HIRNodeKind::Match: {
				hir::HIRMatch& matchNode = static_cast<hir::HIRMatch&>( *hirStatement );
				this->lowerMatch( matchNode );
				break;
			}
			
			case hir::HIRNodeKind::Switch: {
				hir::HIRSwitch& switchNode = static_cast<hir::HIRSwitch&>( *hirStatement );
				this->lowerSwitch( switchNode );
				break;
			}
			
			case hir::HIRNodeKind::TryCatch: {
				hir::HIRTryCatch& tryCatchNode = static_cast<hir::HIRTryCatch&>( *hirStatement );
				this->lowerTryCatch( tryCatchNode );
				break;
			}
			
			case hir::HIRNodeKind::Defer: {
				hir::HIRDefer& deferNode = static_cast<hir::HIRDefer&>( *hirStatement );
				this->lowerDefer( deferNode );
				break;
			}
			
			case hir::HIRNodeKind::Throw: {
				hir::HIRThrow& throwNode = static_cast<hir::HIRThrow&>( *hirStatement );
				MIRVariableIdentifier thrownValue = this->lowerExpression( throwNode.thrownExpression );
				MIRInstruction throwInstruction( MIRInstructionKind::ThrowException );
				throwInstruction.sourceOperands.push_back( thrownValue );
				throwInstruction.sourceLocation = throwNode.sourceLocation;
				this->emitTerminator( throwInstruction );
				break;
			}
			
			case hir::HIRNodeKind::Delete: {
				hir::HIRDelete& deleteNode = static_cast<hir::HIRDelete&>( *hirStatement );
				MIRVariableIdentifier targetVariable = this->lowerExpression( deleteNode.targetExpression );
				MIRInstruction freeInstruction( MIRInstructionKind::HeapFree );
				freeInstruction.sourceOperands.push_back( targetVariable );
				freeInstruction.sourceLocation = deleteNode.sourceLocation;
				this->emitInstruction( freeInstruction );
				break;
			}
			
			case hir::HIRNodeKind::Break: {
				if( this->loopContextStack.empty() == false ) {
					LoopContext& loopContext = this->loopContextStack.top();
					MIRInstruction jumpInstruction( MIRInstructionKind::JumpUnconditional );
					jumpInstruction.trueBranchTarget = loopContext.exitBlockIdentifier;
					jumpInstruction.sourceLocation = hirStatement->sourceLocation;
					this->emitTerminator( jumpInstruction );
				}
				break;
			}
			
			case hir::HIRNodeKind::Continue: {
				if( this->loopContextStack.empty() == false ) {
					LoopContext& loopContext = this->loopContextStack.top();
					MIRBlockIdentifier continueTarget = loopContext.updateBlockIdentifier;
					if( continueTarget == INVALID_BLOCK_IDENTIFIER ) {
						continueTarget = loopContext.headerBlockIdentifier;
					}
					MIRInstruction jumpInstruction( MIRInstructionKind::JumpUnconditional );
					jumpInstruction.trueBranchTarget = continueTarget;
					jumpInstruction.sourceLocation = hirStatement->sourceLocation;
					this->emitTerminator( jumpInstruction );
				}
				break;
			}
			
			case hir::HIRNodeKind::ExpressionStatement: {
				hir::HIRExpressionStatement& exprStatement = static_cast<hir::HIRExpressionStatement&>( *hirStatement );
				if( exprStatement.expression != nullptr ) {
					this->lowerExpression( exprStatement.expression );
				}
				break;
			}
			
			case hir::HIRNodeKind::InlineAssembly: {
				hir::HIRInlineAssembly& asmNode = static_cast<hir::HIRInlineAssembly&>( *hirStatement );
				MIRInstruction asmInstruction( MIRInstructionKind::InlineAssembly );
				asmInstruction.assemblyTemplate = asmNode.assemblyTemplate;
				asmInstruction.assemblyIsVolatile = asmNode.isVolatile;
				asmInstruction.assemblyClobbers = asmNode.clobberRegisters;
				for( const hir::HIRAsmOperand& outputOperand : asmNode.outputOperands ) {
					asmInstruction.assemblyOutputConstraints.push_back( outputOperand.constraintString );
					if( outputOperand.boundExpression != nullptr ) {
						MIRVariableIdentifier outputVariable = this->lowerExpression( outputOperand.boundExpression );
						asmInstruction.assemblyOutputVariables.push_back( outputVariable );
					}
				}
				for( const hir::HIRAsmOperand& inputOperand : asmNode.inputOperands ) {
					asmInstruction.assemblyInputConstraints.push_back( inputOperand.constraintString );
					if( inputOperand.boundExpression != nullptr ) {
						MIRVariableIdentifier inputVariable = this->lowerExpression( inputOperand.boundExpression );
						asmInstruction.assemblyInputVariables.push_back( inputVariable );
					}
				}
				asmInstruction.sourceLocation = asmNode.sourceLocation;
				this->emitInstruction( asmInstruction );
				break;
			}
			
			case hir::HIRNodeKind::UnsafeBlock: {
				hir::HIRUnsafeBlock& unsafeBlock = static_cast<hir::HIRUnsafeBlock&>( *hirStatement );
				if( unsafeBlock.unsafeBody != nullptr ) {
					this->lowerBlock( *unsafeBlock.unsafeBody );
				}
				break;
			}
			
			case hir::HIRNodeKind::Block: {
				hir::HIRBlock& innerBlock = static_cast<hir::HIRBlock&>( *hirStatement );
				this->lowerBlock( innerBlock );
				break;
			}
			
			case hir::HIRNodeKind::Pass:
			case hir::HIRNodeKind::Drop:
				break;
			
			default:
				break;
		}
	}
	
	// ===================================================================
	// Control Flow Lowering
	// ===================================================================
	
	void MIRLowering::lowerIf( hir::HIRIf& hirIf ) {
		MIRVariableIdentifier conditionVariable = this->lowerExpression( hirIf.branchCondition );
		
		std::shared_ptr<MIRBasicBlock> thenBlock = this->currentFunction->createBasicBlock( "if.then" );
		std::shared_ptr<MIRBasicBlock> mergeBlock = this->currentFunction->createBasicBlock( "if.merge" );
		std::shared_ptr<MIRBasicBlock> elseBlock = nullptr;
		
		if( hirIf.elseBranch != nullptr ) {
			elseBlock = this->currentFunction->createBasicBlock( "if.else" );
		}
		
		MIRInstruction branchInstruction( MIRInstructionKind::BranchConditional );
		branchInstruction.sourceOperands.push_back( conditionVariable );
		branchInstruction.trueBranchTarget = thenBlock->blockIdentifier;
		branchInstruction.falseBranchTarget = ( elseBlock != nullptr ) ? elseBlock->blockIdentifier : mergeBlock->blockIdentifier;
		branchInstruction.sourceLocation = hirIf.sourceLocation;
		this->emitTerminator( branchInstruction );
		
		// Then branch
		this->switchToBlock( thenBlock );
		if( hirIf.thenBranch != nullptr ) {
			this->lowerBlock( *hirIf.thenBranch );
		}
		if( this->currentBlock->isTerminated == false ) {
			MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
			jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
			this->emitTerminator( jumpToMerge );
		}
		
		// Else branch
		if( elseBlock != nullptr && hirIf.elseBranch != nullptr ) {
			this->switchToBlock( elseBlock );
			if( hirIf.elseBranch->nodeKind == hir::HIRNodeKind::If ) {
				hir::HIRIf& nestedIf = static_cast<hir::HIRIf&>( *hirIf.elseBranch );
				this->lowerIf( nestedIf );
			}
			else if( hirIf.elseBranch->nodeKind == hir::HIRNodeKind::Block ) {
				hir::HIRBlock& elseBodyBlock = static_cast<hir::HIRBlock&>( *hirIf.elseBranch );
				this->lowerBlock( elseBodyBlock );
			}
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
				jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
				this->emitTerminator( jumpToMerge );
			}
		}
		
		this->switchToBlock( mergeBlock );
	}
	
	void MIRLowering::lowerLoop( hir::HIRLoop& hirLoop ) {
		// Desugar range-for: for Type var in start..end → init + condition + body + update
		if( hirLoop.isIteratorLoop && hirLoop.iterableExpression != nullptr &&
			hirLoop.iterableExpression->nodeKind == hir::HIRNodeKind::RangeExpression ) {
			hir::HIRRangeExpression& rangeExpression =
				static_cast<hir::HIRRangeExpression&>( *hirLoop.iterableExpression );

			// Allocate loop variable
			semantic::TypeSharedPointer loopVariableType = hirLoop.loopVariableType;
			if( loopVariableType == nullptr ) {
				loopVariableType = rangeExpression.rangeStart != nullptr
					? rangeExpression.rangeStart->resolvedType : nullptr;
			}
			MIRVariableIdentifier loopVariable = this->currentFunction->allocateVariable(
				hirLoop.loopVariableName, loopVariableType, true
			);
			this->variableNameMap[hirLoop.loopVariableName] = loopVariable;

			MIRInstruction allocateInstruction( MIRInstructionKind::AllocateLocal );
			allocateInstruction.destinationVariable = loopVariable;
			allocateInstruction.operandType = loopVariableType;
			allocateInstruction.sourceLocation = hirLoop.sourceLocation;
			this->emitInstruction( allocateInstruction );

			// Store start value
			MIRVariableIdentifier startValue = this->lowerExpression( rangeExpression.rangeStart );
			MIRInstruction storeStart( MIRInstructionKind::StoreVariable );
			storeStart.destinationVariable = loopVariable;
			storeStart.sourceOperands.push_back( startValue );
			this->emitInstruction( storeStart );

			// Lower end value before loop
			MIRVariableIdentifier endValue = this->lowerExpression( rangeExpression.rangeEnd );

			std::shared_ptr<MIRBasicBlock> headerBlock = this->currentFunction->createBasicBlock( "range.header" );
			std::shared_ptr<MIRBasicBlock> bodyBlock = this->currentFunction->createBasicBlock( "range.body" );
			std::shared_ptr<MIRBasicBlock> updateBlock = this->currentFunction->createBasicBlock( "range.update" );
			std::shared_ptr<MIRBasicBlock> exitBlock = this->currentFunction->createBasicBlock( "range.exit" );

			MIRInstruction jumpToHeader( MIRInstructionKind::JumpUnconditional );
			jumpToHeader.trueBranchTarget = headerBlock->blockIdentifier;
			this->emitTerminator( jumpToHeader );

			// Header: loopVar < end (or <= for inclusive)
			this->switchToBlock( headerBlock );
			MIRInstruction loadLoopVar( MIRInstructionKind::LoadVariable );
			loadLoopVar.sourceOperands.push_back( loopVariable );
			loadLoopVar.calledFunctionQualifiedName = hirLoop.loopVariableName;
			loadLoopVar.operandType = loopVariableType;
			MIRVariableIdentifier currentValue = this->currentFunction->allocateVariable(
				"_range_cur", loopVariableType, false
			);
			loadLoopVar.destinationVariable = currentValue;
			this->emitInstruction( loadLoopVar );

			MIRInstructionKind compareKind = rangeExpression.isInclusive
				? MIRInstructionKind::CompareLessEqual
				: MIRInstructionKind::CompareLessThan;
			MIRInstruction compareInstruction( compareKind );
			compareInstruction.sourceOperands.push_back( currentValue );
			compareInstruction.sourceOperands.push_back( endValue );
			MIRVariableIdentifier conditionResult = this->currentFunction->allocateVariable(
				"_range_cond", nullptr, false
			);
			compareInstruction.destinationVariable = conditionResult;
			this->emitInstruction( compareInstruction );

			MIRInstruction branchInstruction( MIRInstructionKind::BranchConditional );
			branchInstruction.sourceOperands.push_back( conditionResult );
			branchInstruction.trueBranchTarget = bodyBlock->blockIdentifier;
			branchInstruction.falseBranchTarget = exitBlock->blockIdentifier;
			this->emitTerminator( branchInstruction );

			// Push loop context
			LoopContext loopContext;
			loopContext.headerBlockIdentifier = headerBlock->blockIdentifier;
			loopContext.exitBlockIdentifier = exitBlock->blockIdentifier;
			loopContext.updateBlockIdentifier = updateBlock->blockIdentifier;
			this->loopContextStack.push( loopContext );

			// Body
			this->switchToBlock( bodyBlock );
			if( hirLoop.loopBody != nullptr ) {
				this->lowerBlock( *hirLoop.loopBody );
			}
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToUpdate( MIRInstructionKind::JumpUnconditional );
				jumpToUpdate.trueBranchTarget = updateBlock->blockIdentifier;
				this->emitTerminator( jumpToUpdate );
			}

			// Update: loopVar = loopVar + 1
			this->switchToBlock( updateBlock );
			MIRInstruction reloadVar( MIRInstructionKind::LoadVariable );
			reloadVar.sourceOperands.push_back( loopVariable );
			reloadVar.calledFunctionQualifiedName = hirLoop.loopVariableName;
			reloadVar.operandType = loopVariableType;
			MIRVariableIdentifier reloadedValue = this->currentFunction->allocateVariable(
				"_range_reload", loopVariableType, false
			);
			reloadVar.destinationVariable = reloadedValue;
			this->emitInstruction( reloadVar );

			MIRInstruction oneConstant( MIRInstructionKind::ConstantInteger );
			oneConstant.integerConstantValue = 1;
			oneConstant.operandType = loopVariableType;
			MIRVariableIdentifier oneValue = this->currentFunction->allocateVariable(
				"_range_step", loopVariableType, false
			);
			oneConstant.destinationVariable = oneValue;
			this->emitInstruction( oneConstant );

			MIRInstruction incrementInstruction( MIRInstructionKind::AddInteger );
			incrementInstruction.sourceOperands.push_back( reloadedValue );
			incrementInstruction.sourceOperands.push_back( oneValue );
			incrementInstruction.operandType = loopVariableType;
			MIRVariableIdentifier incrementedValue = this->currentFunction->allocateVariable(
				"_range_next", loopVariableType, false
			);
			incrementInstruction.destinationVariable = incrementedValue;
			this->emitInstruction( incrementInstruction );

			MIRInstruction storeIncremented( MIRInstructionKind::StoreVariable );
			storeIncremented.destinationVariable = loopVariable;
			storeIncremented.sourceOperands.push_back( incrementedValue );
			this->emitInstruction( storeIncremented );

			MIRInstruction jumpBackToHeader( MIRInstructionKind::JumpUnconditional );
			jumpBackToHeader.trueBranchTarget = headerBlock->blockIdentifier;
			this->emitTerminator( jumpBackToHeader );

			this->loopContextStack.pop();
			this->switchToBlock( exitBlock );
			return;
		}

		std::shared_ptr<MIRBasicBlock> headerBlock = this->currentFunction->createBasicBlock( "loop.header" );
		std::shared_ptr<MIRBasicBlock> bodyBlock = this->currentFunction->createBasicBlock( "loop.body" );
		std::shared_ptr<MIRBasicBlock> exitBlock = this->currentFunction->createBasicBlock( "loop.exit" );
		std::shared_ptr<MIRBasicBlock> updateBlock = nullptr;

		if( hirLoop.loopUpdate != nullptr ) {
			updateBlock = this->currentFunction->createBasicBlock( "loop.update" );
		}

		// Initialize loop variable if present (C-style for)
		if( hirLoop.loopVariableName.empty() == false && hirLoop.loopInitializer != nullptr ) {
			semantic::TypeSharedPointer loopVariableType = hirLoop.loopVariableType;
			if( loopVariableType == nullptr && hirLoop.loopInitializer->resolvedType != nullptr ) {
				loopVariableType = hirLoop.loopInitializer->resolvedType;
			}
			MIRVariableIdentifier loopVariable = this->currentFunction->allocateVariable(
				hirLoop.loopVariableName, loopVariableType, true
			);
			this->variableNameMap[hirLoop.loopVariableName] = loopVariable;

			MIRInstruction allocateInstruction( MIRInstructionKind::AllocateLocal );
			allocateInstruction.destinationVariable = loopVariable;
			allocateInstruction.operandType = loopVariableType;
			allocateInstruction.sourceLocation = hirLoop.sourceLocation;
			this->emitInstruction( allocateInstruction );

			MIRVariableIdentifier initValue = this->lowerExpression( hirLoop.loopInitializer );
			MIRInstruction storeInit( MIRInstructionKind::StoreVariable );
			storeInit.destinationVariable = loopVariable;
			storeInit.sourceOperands.push_back( initValue );
			this->emitInstruction( storeInit );
		}
		else if( hirLoop.loopInitializer != nullptr ) {
			this->lowerExpression( hirLoop.loopInitializer );
		}

		// Jump to header
		MIRInstruction jumpToHeader( MIRInstructionKind::JumpUnconditional );
		jumpToHeader.trueBranchTarget = headerBlock->blockIdentifier;
		this->emitTerminator( jumpToHeader );

		// Header: evaluate condition
		this->switchToBlock( headerBlock );
		if( hirLoop.loopCondition != nullptr ) {
			MIRVariableIdentifier conditionVariable = this->lowerExpression( hirLoop.loopCondition );
			MIRInstruction branchInstruction( MIRInstructionKind::BranchConditional );
			branchInstruction.sourceOperands.push_back( conditionVariable );
			branchInstruction.trueBranchTarget = bodyBlock->blockIdentifier;
			branchInstruction.falseBranchTarget = exitBlock->blockIdentifier;
			branchInstruction.sourceLocation = hirLoop.sourceLocation;
			this->emitTerminator( branchInstruction );
		}
		else {
			// Infinite loop
			MIRInstruction jumpToBody( MIRInstructionKind::JumpUnconditional );
			jumpToBody.trueBranchTarget = bodyBlock->blockIdentifier;
			this->emitTerminator( jumpToBody );
		}

		// Push loop context for break/continue
		LoopContext loopContext;
		loopContext.headerBlockIdentifier = headerBlock->blockIdentifier;
		loopContext.exitBlockIdentifier = exitBlock->blockIdentifier;
		loopContext.updateBlockIdentifier = ( updateBlock != nullptr ) ? updateBlock->blockIdentifier : INVALID_BLOCK_IDENTIFIER;
		this->loopContextStack.push( loopContext );

		// Body
		this->switchToBlock( bodyBlock );
		if( hirLoop.loopBody != nullptr ) {
			this->lowerBlock( *hirLoop.loopBody );
		}
		if( this->currentBlock->isTerminated == false ) {
			MIRBlockIdentifier backEdgeTarget = ( updateBlock != nullptr ) ? updateBlock->blockIdentifier : headerBlock->blockIdentifier;
			MIRInstruction jumpBack( MIRInstructionKind::JumpUnconditional );
			jumpBack.trueBranchTarget = backEdgeTarget;
			this->emitTerminator( jumpBack );
		}

		// Update block (for C-style for loops)
		if( updateBlock != nullptr ) {
			this->switchToBlock( updateBlock );
			if( hirLoop.loopUpdate != nullptr ) {
				this->lowerStatement( hirLoop.loopUpdate );
			}
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToHeaderFromUpdate( MIRInstructionKind::JumpUnconditional );
				jumpToHeaderFromUpdate.trueBranchTarget = headerBlock->blockIdentifier;
				this->emitTerminator( jumpToHeaderFromUpdate );
			}
		}
		
		this->loopContextStack.pop();
		this->switchToBlock( exitBlock );
	}
	
	void MIRLowering::lowerMatch( hir::HIRMatch& hirMatch ) {
		MIRVariableIdentifier subjectVariable = this->lowerExpression( hirMatch.matchSubject );
		std::shared_ptr<MIRBasicBlock> mergeBlock = this->currentFunction->createBasicBlock( "match.merge" );
		
		MIRInstruction switchInstruction( MIRInstructionKind::SwitchBranch );
		switchInstruction.sourceOperands.push_back( subjectVariable );
		switchInstruction.defaultSwitchTarget = mergeBlock->blockIdentifier;
		switchInstruction.sourceLocation = hirMatch.sourceLocation;
		
		std::vector<std::shared_ptr<MIRBasicBlock>> armBlocks;
		for( size_t armIndex = 0; armIndex < hirMatch.matchArms.size(); armIndex++ ) {
			std::shared_ptr<MIRBasicBlock> armBlock = this->currentFunction->createBasicBlock(
				fmt::format( "match.arm.{}", armIndex )
			);
			armBlocks.push_back( armBlock );
			switchInstruction.switchBranchTargets.push_back( { static_cast<int64_t>( armIndex ), armBlock->blockIdentifier } );
		}
		
		this->emitTerminator( switchInstruction );
		
		for( size_t armIndex = 0; armIndex < hirMatch.matchArms.size(); armIndex++ ) {
			this->switchToBlock( armBlocks[armIndex] );
			hir::HIRMatchArm& arm = hirMatch.matchArms[armIndex];
			if( arm.armBody != nullptr ) {
				this->lowerBlock( *arm.armBody );
			}
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
				jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
				this->emitTerminator( jumpToMerge );
			}
		}
		
		this->switchToBlock( mergeBlock );
	}
	
	void MIRLowering::lowerSwitch( hir::HIRSwitch& hirSwitch ) {
		MIRVariableIdentifier subjectVariable = this->lowerExpression( hirSwitch.switchSubject );
		std::shared_ptr<MIRBasicBlock> mergeBlock = this->currentFunction->createBasicBlock( "switch.merge" );
		
		MIRInstruction switchInstruction( MIRInstructionKind::SwitchBranch );
		switchInstruction.sourceOperands.push_back( subjectVariable );
		switchInstruction.defaultSwitchTarget = mergeBlock->blockIdentifier;
		switchInstruction.sourceLocation = hirSwitch.sourceLocation;
		
		std::vector<std::shared_ptr<MIRBasicBlock>> caseBlocks;
		for( size_t caseIndex = 0; caseIndex < hirSwitch.switchCases.size(); caseIndex++ ) {
			std::shared_ptr<MIRBasicBlock> caseBlock = this->currentFunction->createBasicBlock(
				fmt::format( "switch.case.{}", caseIndex )
			);
			caseBlocks.push_back( caseBlock );
			
			hir::HIRSwitchCase& switchCase = hirSwitch.switchCases[caseIndex];
			if( switchCase.isDefaultCase ) {
				switchInstruction.defaultSwitchTarget = caseBlock->blockIdentifier;
			}
			else {
				switchInstruction.switchBranchTargets.push_back( { static_cast<int64_t>( caseIndex ), caseBlock->blockIdentifier } );
			}
		}
		
		this->emitTerminator( switchInstruction );
		
		for( size_t caseIndex = 0; caseIndex < hirSwitch.switchCases.size(); caseIndex++ ) {
			this->switchToBlock( caseBlocks[caseIndex] );
			hir::HIRSwitchCase& switchCase = hirSwitch.switchCases[caseIndex];
			if( switchCase.caseBody != nullptr ) {
				this->lowerBlock( *switchCase.caseBody );
			}
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
				jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
				this->emitTerminator( jumpToMerge );
			}
		}
		
		this->switchToBlock( mergeBlock );
	}
	
	void MIRLowering::lowerTryCatch( hir::HIRTryCatch& hirTryCatch ) {
		std::shared_ptr<MIRBasicBlock> tryBlock = this->currentFunction->createBasicBlock( "try.body" );
		std::shared_ptr<MIRBasicBlock> mergeBlock = this->currentFunction->createBasicBlock( "try.merge" );
		std::shared_ptr<MIRBasicBlock> landingPadBlock = this->currentFunction->createBasicBlock( "try.landing" );
		
		MIRInstruction jumpToTry( MIRInstructionKind::JumpUnconditional );
		jumpToTry.trueBranchTarget = tryBlock->blockIdentifier;
		this->emitTerminator( jumpToTry );
		
		// Try body
		this->switchToBlock( tryBlock );
		if( hirTryCatch.tryBody != nullptr ) {
			this->lowerBlock( *hirTryCatch.tryBody );
		}
		if( this->currentBlock->isTerminated == false ) {
			MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
			jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
			this->emitTerminator( jumpToMerge );
		}
		
		// Landing pad
		this->switchToBlock( landingPadBlock );
		MIRInstruction landingPadInstruction( MIRInstructionKind::LandingPad );
		landingPadInstruction.sourceLocation = hirTryCatch.sourceLocation;
		MIRVariableIdentifier exceptionVariable = this->currentFunction->allocateVariable(
			"_caught_exception", nullptr, false
		);
		landingPadInstruction.destinationVariable = exceptionVariable;
		this->emitInstruction( landingPadInstruction );
		
		// Handler blocks
		for( size_t handlerIndex = 0; handlerIndex < hirTryCatch.exceptionHandlers.size(); handlerIndex++ ) {
			hir::HIRExceptionHandler& handler = hirTryCatch.exceptionHandlers[handlerIndex];
			std::shared_ptr<MIRBasicBlock> handlerBlock = this->currentFunction->createBasicBlock(
				fmt::format( "catch.handler.{}", handlerIndex )
			);
			
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToHandler( MIRInstructionKind::JumpUnconditional );
				jumpToHandler.trueBranchTarget = handlerBlock->blockIdentifier;
				this->emitTerminator( jumpToHandler );
			}
			
			this->switchToBlock( handlerBlock );
			if( handler.exceptionVariableName.empty() == false ) {
				MIRVariableIdentifier handlerVariable = this->currentFunction->allocateVariable(
					handler.exceptionVariableName, nullptr, false
				);
				MIRInstruction copyException( MIRInstructionKind::CopyValue );
				copyException.destinationVariable = handlerVariable;
				copyException.sourceOperands.push_back( exceptionVariable );
				this->emitInstruction( copyException );
			}
			if( handler.handlerBody != nullptr ) {
				this->lowerBlock( *handler.handlerBody );
			}
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
				jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
				this->emitTerminator( jumpToMerge );
			}
		}
		
		// Finally block
		if( hirTryCatch.finallyBody != nullptr ) {
			std::shared_ptr<MIRBasicBlock> finallyBlock = this->currentFunction->createBasicBlock( "finally" );
			this->switchToBlock( finallyBlock );
			this->lowerBlock( *hirTryCatch.finallyBody );
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
				jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
				this->emitTerminator( jumpToMerge );
			}
		}
		
		this->switchToBlock( mergeBlock );
	}
	
	void MIRLowering::lowerDefer( hir::HIRDefer& hirDefer ) {
		MIRInstruction deferPushInstruction( MIRInstructionKind::DeferPush );
		deferPushInstruction.sourceLocation = hirDefer.sourceLocation;
		this->emitInstruction( deferPushInstruction );
	}
	
	// ===================================================================
	// Expression Lowering
	// ===================================================================
	
	MIRVariableIdentifier MIRLowering::lowerExpression( const hir::HIRNodeSharedPointer& hirExpression ) {
		if( hirExpression == nullptr ) {
			return INVALID_VARIABLE_IDENTIFIER;
		}
		
		switch( hirExpression->nodeKind ) {
			case hir::HIRNodeKind::IntegerLiteral: {
				hir::HIRIntegerLiteral& integerLiteral = static_cast<hir::HIRIntegerLiteral&>( *hirExpression );
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantInteger );
				constantInstruction.integerConstantValue = integerLiteral.integerValue;
				constantInstruction.operandType = integerLiteral.resolvedType;
				constantInstruction.sourceLocation = integerLiteral.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_const_int", integerLiteral.resolvedType, false
				);
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			
			case hir::HIRNodeKind::FloatLiteral: {
				hir::HIRFloatLiteral& floatLiteral = static_cast<hir::HIRFloatLiteral&>( *hirExpression );
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantFloat );
				constantInstruction.floatConstantValue = floatLiteral.floatValue;
				constantInstruction.operandType = floatLiteral.resolvedType;
				constantInstruction.sourceLocation = floatLiteral.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_const_float", floatLiteral.resolvedType, false
				);
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			
			case hir::HIRNodeKind::BooleanLiteral: {
				hir::HIRBooleanLiteral& boolLiteral = static_cast<hir::HIRBooleanLiteral&>( *hirExpression );
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantBoolean );
				constantInstruction.booleanConstantValue = boolLiteral.booleanValue;
				constantInstruction.operandType = boolLiteral.resolvedType;
				constantInstruction.sourceLocation = boolLiteral.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_const_bool", boolLiteral.resolvedType, false
				);
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			
			case hir::HIRNodeKind::StringLiteral: {
				hir::HIRStringLiteral& stringLiteral = static_cast<hir::HIRStringLiteral&>( *hirExpression );
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantString );
				constantInstruction.stringConstantValue = stringLiteral.stringValue;
				constantInstruction.operandType = stringLiteral.resolvedType;
				constantInstruction.sourceLocation = stringLiteral.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_const_str", stringLiteral.resolvedType, false
				);
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			
			case hir::HIRNodeKind::CharLiteral: {
				hir::HIRCharLiteral& charLiteral = static_cast<hir::HIRCharLiteral&>( *hirExpression );
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantChar );
				constantInstruction.charConstantValue = charLiteral.charValue;
				constantInstruction.operandType = charLiteral.resolvedType;
				constantInstruction.sourceLocation = charLiteral.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_const_char", charLiteral.resolvedType, false
				);
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			
			case hir::HIRNodeKind::NoneLiteral: {
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantNone );
				constantInstruction.operandType = hirExpression->resolvedType;
				constantInstruction.sourceLocation = hirExpression->sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_const_none", hirExpression->resolvedType, false
				);
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			
			case hir::HIRNodeKind::Identifier: {
				hir::HIRIdentifier& identifier = static_cast<hir::HIRIdentifier&>( *hirExpression );
				MIRInstruction loadInstruction( MIRInstructionKind::LoadVariable );
				loadInstruction.calledFunctionQualifiedName = identifier.identifierName;
				loadInstruction.operandType = identifier.resolvedType;
				loadInstruction.sourceLocation = identifier.sourceLocation;
				std::unordered_map<std::string, MIRVariableIdentifier>::iterator variableLookup =
					this->variableNameMap.find( identifier.identifierName );
				if( variableLookup != this->variableNameMap.end() ) {
					loadInstruction.sourceOperands.push_back( variableLookup->second );
				}
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					identifier.identifierName, identifier.resolvedType, false
				);
				loadInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( loadInstruction );
			}
			
			case hir::HIRNodeKind::SelfReference: {
				MIRInstruction loadInstruction( MIRInstructionKind::LoadVariable );
				loadInstruction.calledFunctionQualifiedName = "self";
				loadInstruction.operandType = hirExpression->resolvedType;
				loadInstruction.sourceLocation = hirExpression->sourceLocation;
				std::unordered_map<std::string, MIRVariableIdentifier>::iterator selfLookup =
					this->variableNameMap.find( "self" );
				if( selfLookup != this->variableNameMap.end() ) {
					loadInstruction.sourceOperands.push_back( selfLookup->second );
				}
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"self", hirExpression->resolvedType, false
				);
				loadInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( loadInstruction );
			}
			
			case hir::HIRNodeKind::BinaryOperation: {
				hir::HIRBinaryOperation& binaryOp = static_cast<hir::HIRBinaryOperation&>( *hirExpression );
				return this->lowerBinaryOperation( binaryOp );
			}
			
			case hir::HIRNodeKind::UnaryOperation: {
				hir::HIRUnaryOperation& unaryOp = static_cast<hir::HIRUnaryOperation&>( *hirExpression );
				return this->lowerUnaryOperation( unaryOp );
			}
			
			case hir::HIRNodeKind::FunctionCall: {
				hir::HIRFunctionCall& callExpression = static_cast<hir::HIRFunctionCall&>( *hirExpression );
				return this->lowerFunctionCall( callExpression );
			}
			
			case hir::HIRNodeKind::MethodCall: {
				hir::HIRMethodCall& methodCall = static_cast<hir::HIRMethodCall&>( *hirExpression );
				return this->lowerMethodCall( methodCall );
			}
			
			case hir::HIRNodeKind::FieldAccess: {
				hir::HIRFieldAccess& fieldAccess = static_cast<hir::HIRFieldAccess&>( *hirExpression );
				return this->lowerFieldAccess( fieldAccess );
			}
			
			case hir::HIRNodeKind::IndexAccess: {
				hir::HIRIndexAccess& indexAccess = static_cast<hir::HIRIndexAccess&>( *hirExpression );
				return this->lowerIndexAccess( indexAccess );
			}
			
			case hir::HIRNodeKind::Construct: {
				hir::HIRConstruct& construct = static_cast<hir::HIRConstruct&>( *hirExpression );
				return this->lowerConstruct( construct );
			}
			
			case hir::HIRNodeKind::Cast: {
				hir::HIRCast& castNode = static_cast<hir::HIRCast&>( *hirExpression );
				MIRVariableIdentifier sourceVariable = this->lowerExpression( castNode.sourceExpression );
				MIRInstruction castInstruction( MIRInstructionKind::CastType );
				castInstruction.sourceOperands.push_back( sourceVariable );
				castInstruction.castTargetType = castNode.targetCastType;
				castInstruction.operandType = castNode.resolvedType;
				castInstruction.sourceLocation = castNode.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_cast", castNode.resolvedType, false
				);
				castInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( castInstruction );
			}
			
			case hir::HIRNodeKind::Reference: {
				hir::HIRReference& refNode = static_cast<hir::HIRReference&>( *hirExpression );
				MIRVariableIdentifier targetVariable = this->lowerExpression( refNode.targetExpression );
				MIRInstruction refInstruction( MIRInstructionKind::TakeReference );
				refInstruction.sourceOperands.push_back( targetVariable );
				refInstruction.operandType = refNode.resolvedType;
				refInstruction.sourceLocation = refNode.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_ref", refNode.resolvedType, false
				);
				refInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( refInstruction );
			}
			
			case hir::HIRNodeKind::Dereference: {
				hir::HIRDereference& derefNode = static_cast<hir::HIRDereference&>( *hirExpression );
				MIRVariableIdentifier targetVariable = this->lowerExpression( derefNode.targetExpression );
				MIRInstruction derefInstruction( MIRInstructionKind::DereferencePointer );
				derefInstruction.sourceOperands.push_back( targetVariable );
				derefInstruction.operandType = derefNode.resolvedType;
				derefInstruction.sourceLocation = derefNode.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_deref", derefNode.resolvedType, false
				);
				derefInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( derefInstruction );
			}
			
			case hir::HIRNodeKind::MoveTransfer: {
				hir::HIRMoveTransfer& moveNode = static_cast<hir::HIRMoveTransfer&>( *hirExpression );
				MIRVariableIdentifier sourceVariable = this->lowerExpression( moveNode.movedExpression );
				MIRInstruction moveInstruction( MIRInstructionKind::MoveValue );
				moveInstruction.sourceOperands.push_back( sourceVariable );
				moveInstruction.operandType = moveNode.resolvedType;
				moveInstruction.sourceLocation = moveNode.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_moved", moveNode.resolvedType, false
				);
				moveInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( moveInstruction );
			}
			
			case hir::HIRNodeKind::AddressOf: {
				hir::HIRAddressOf& addrNode = static_cast<hir::HIRAddressOf&>( *hirExpression );
				MIRVariableIdentifier targetVariable = this->lowerExpression( addrNode.targetExpression );
				MIRInstruction addrInstruction( MIRInstructionKind::AddressOf );
				addrInstruction.sourceOperands.push_back( targetVariable );
				addrInstruction.operandType = addrNode.resolvedType;
				addrInstruction.sourceLocation = addrNode.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_addr", addrNode.resolvedType, false
				);
				addrInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( addrInstruction );
			}
			
			default:
				return INVALID_VARIABLE_IDENTIFIER;
		}
	}
	
	MIRVariableIdentifier MIRLowering::lowerBinaryOperation( hir::HIRBinaryOperation& hirBinaryOp ) {
		MIRVariableIdentifier leftVariable = this->lowerExpression( hirBinaryOp.leftOperand );
		MIRVariableIdentifier rightVariable = this->lowerExpression( hirBinaryOp.rightOperand );
		
		MIRInstructionKind instructionKind = MIRInstructionKind::NoOperation;
		
		bool isFloatOperation = hirBinaryOp.resolvedType != nullptr &&
			( hirBinaryOp.resolvedType->kind == semantic::Type::Kind::Float ||
			  ( hirBinaryOp.resolvedType->kind == semantic::Type::Kind::Class &&
			    ( hirBinaryOp.resolvedType->name == "Float" ||
			      hirBinaryOp.resolvedType->name == "Double" ||
			      hirBinaryOp.resolvedType->name == "F32" ||
			      hirBinaryOp.resolvedType->name == "F64" ) ) );

		switch( hirBinaryOp.operatorKind ) {
			case token::Type::Plus:
				instructionKind = isFloatOperation ? MIRInstructionKind::AddFloat : MIRInstructionKind::AddInteger;
				break;
			case token::Type::Minus:
				instructionKind = isFloatOperation ? MIRInstructionKind::SubtractFloat : MIRInstructionKind::SubtractInteger;
				break;
			case token::Type::Star:
				instructionKind = isFloatOperation ? MIRInstructionKind::MultiplyFloat : MIRInstructionKind::MultiplyInteger;
				break;
			case token::Type::Slash:
				instructionKind = isFloatOperation ? MIRInstructionKind::DivideFloat : MIRInstructionKind::DivideInteger;
				break;
			case token::Type::Percent:          instructionKind = MIRInstructionKind::ModuloInteger; break;
			case token::Type::Power:
				instructionKind = isFloatOperation ? MIRInstructionKind::PowerFloat : MIRInstructionKind::PowerInteger;
				break;
			case token::Type::Ampersand:        instructionKind = MIRInstructionKind::BitwiseAnd; break;
			case token::Type::Pipe:             instructionKind = MIRInstructionKind::BitwiseOr; break;
			case token::Type::Caret:            instructionKind = MIRInstructionKind::BitwiseXor; break;
			case token::Type::ShiftLeft:        instructionKind = MIRInstructionKind::ShiftLeft; break;
			case token::Type::ShiftRight:       instructionKind = MIRInstructionKind::ShiftRight; break;
			case token::Type::Equal:            instructionKind = MIRInstructionKind::CompareEqual; break;
			case token::Type::NotEqual:         instructionKind = MIRInstructionKind::CompareNotEqual; break;
			case token::Type::LessThan:         instructionKind = MIRInstructionKind::CompareLessThan; break;
			case token::Type::GreaterThan:      instructionKind = MIRInstructionKind::CompareGreaterThan; break;
			case token::Type::LessThanEqual:    instructionKind = MIRInstructionKind::CompareLessEqual; break;
			case token::Type::GreaterThanEqual: instructionKind = MIRInstructionKind::CompareGreaterEqual; break;
			case token::Type::KeywordAnd:       instructionKind = MIRInstructionKind::LogicalAnd; break;
			case token::Type::KeywordOr:        instructionKind = MIRInstructionKind::LogicalOr; break;
			default:                            instructionKind = MIRInstructionKind::NoOperation; break;
		}
		
		MIRInstruction binaryInstruction( instructionKind );
		binaryInstruction.sourceOperands.push_back( leftVariable );
		binaryInstruction.sourceOperands.push_back( rightVariable );
		binaryInstruction.operandType = hirBinaryOp.resolvedType;
		binaryInstruction.sourceLocation = hirBinaryOp.sourceLocation;
		
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			"_binop", hirBinaryOp.resolvedType, false
		);
		binaryInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( binaryInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerUnaryOperation( hir::HIRUnaryOperation& hirUnaryOp ) {
		MIRVariableIdentifier operandVariable = this->lowerExpression( hirUnaryOp.operandExpression );
		
		MIRInstructionKind instructionKind = MIRInstructionKind::NoOperation;
		
		switch( hirUnaryOp.operatorKind ) {
			case token::Type::Minus: {
				bool isFloatNegate = hirUnaryOp.resolvedType != nullptr &&
					( hirUnaryOp.resolvedType->kind == semantic::Type::Kind::Float ||
					  ( hirUnaryOp.resolvedType->kind == semantic::Type::Kind::Class &&
					    ( hirUnaryOp.resolvedType->name == "Float" ||
					      hirUnaryOp.resolvedType->name == "Double" ||
					      hirUnaryOp.resolvedType->name == "F32" ||
					      hirUnaryOp.resolvedType->name == "F64" ) ) );
				instructionKind = isFloatNegate
					? MIRInstructionKind::NegateFloat
					: MIRInstructionKind::NegateInteger;
				break;
			}
			case token::Type::Tilde:      instructionKind = MIRInstructionKind::BitwiseNot; break;
			case token::Type::KeywordNot: instructionKind = MIRInstructionKind::LogicalNot; break;
			default:                      instructionKind = MIRInstructionKind::NoOperation; break;
		}
		
		MIRInstruction unaryInstruction( instructionKind );
		unaryInstruction.sourceOperands.push_back( operandVariable );
		unaryInstruction.operandType = hirUnaryOp.resolvedType;
		unaryInstruction.sourceLocation = hirUnaryOp.sourceLocation;
		
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			"_unop", hirUnaryOp.resolvedType, false
		);
		unaryInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( unaryInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerFunctionCall( hir::HIRFunctionCall& hirCall ) {
		std::vector<MIRVariableIdentifier> argumentVariables;
		for( const hir::HIRNodeSharedPointer& argument : hirCall.callArguments ) {
			argumentVariables.push_back( this->lowerExpression( argument ) );
		}
		
		MIRInstruction callInstruction( MIRInstructionKind::CallFunction );
		if( hirCall.calleeExpression != nullptr ) {
			if( hirCall.calleeExpression->nodeKind == hir::HIRNodeKind::Identifier ) {
				hir::HIRIdentifier& calleeIdentifier = static_cast<hir::HIRIdentifier&>( *hirCall.calleeExpression );
				callInstruction.calledFunctionQualifiedName = calleeIdentifier.identifierName;
			}
			else if( hirCall.calleeExpression->nodeKind == hir::HIRNodeKind::SuperReference ) {
				// parent() super-constructor → resolve to parent class constructor name
				if( this->currentParentClassName.empty() == false ) {
					callInstruction.calledFunctionQualifiedName = this->currentParentClassName;
				}
			}
		}
		callInstruction.sourceOperands = std::move( argumentVariables );
		callInstruction.operandType = hirCall.resolvedType;
		callInstruction.sourceLocation = hirCall.sourceLocation;
		
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			"_call", hirCall.resolvedType, false
		);
		callInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( callInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerMethodCall( hir::HIRMethodCall& hirMethodCall ) {
		MIRVariableIdentifier receiverVariable = this->lowerExpression( hirMethodCall.receiverObject );
		
		std::vector<MIRVariableIdentifier> argumentVariables;
		argumentVariables.push_back( receiverVariable );
		for( const hir::HIRNodeSharedPointer& argument : hirMethodCall.callArguments ) {
			argumentVariables.push_back( this->lowerExpression( argument ) );
		}
		
		MIRInstruction callInstruction( MIRInstructionKind::CallFunction );
		std::string ownerClassName;
		if( hirMethodCall.receiverObject != nullptr &&
			hirMethodCall.receiverObject->resolvedType != nullptr ) {
			ownerClassName = hirMethodCall.receiverObject->resolvedType->name;
		}
		if( ownerClassName.empty() == false ) {
			callInstruction.calledFunctionQualifiedName = ownerClassName + "." + hirMethodCall.methodName;
		}
		else {
			callInstruction.calledFunctionQualifiedName = hirMethodCall.methodName;
		}
		callInstruction.sourceOperands = std::move( argumentVariables );
		callInstruction.operandType = hirMethodCall.resolvedType;
		callInstruction.sourceLocation = hirMethodCall.sourceLocation;
		
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			"_mcall", hirMethodCall.resolvedType, false
		);
		callInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( callInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerFieldAccess( hir::HIRFieldAccess& hirFieldAccess ) {
		MIRVariableIdentifier objectVariable = this->lowerExpression( hirFieldAccess.objectExpression );

		int resolvedFieldIndex = hirFieldAccess.fieldLayoutIndex;
		if( resolvedFieldIndex < 0 && hirFieldAccess.objectExpression != nullptr &&
			hirFieldAccess.objectExpression->resolvedType != nullptr ) {
			std::string objectTypeName = hirFieldAccess.objectExpression->resolvedType->name;
			if( this->currentModule->typeLayoutTable.count( objectTypeName ) > 0 ) {
				const TypeLayoutDescriptor& layout = this->currentModule->typeLayoutTable[objectTypeName];
				for( size_t fieldIndex = 0; fieldIndex < layout.fieldNames.size(); fieldIndex++ ) {
					if( layout.fieldNames[fieldIndex] == hirFieldAccess.fieldName ) {
						resolvedFieldIndex = static_cast<int>( fieldIndex );
						break;
					}
				}
			}
		}

		MIRInstruction gepInstruction( MIRInstructionKind::ComputeFieldAddress );
		gepInstruction.sourceOperands.push_back( objectVariable );
		gepInstruction.fieldAccessName = hirFieldAccess.fieldName;
		gepInstruction.fieldLayoutIndex = resolvedFieldIndex;
		gepInstruction.operandType = hirFieldAccess.resolvedType;
		gepInstruction.sourceLocation = hirFieldAccess.sourceLocation;
		
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			fmt::format( "_field_{}", hirFieldAccess.fieldName ), hirFieldAccess.resolvedType, false
		);
		gepInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( gepInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerIndexAccess( hir::HIRIndexAccess& hirIndexAccess ) {
		MIRVariableIdentifier objectVariable = this->lowerExpression( hirIndexAccess.objectExpression );
		MIRVariableIdentifier indexVariable = this->lowerExpression( hirIndexAccess.indexExpression );
		
		MIRInstruction gepInstruction( MIRInstructionKind::ComputeIndexAddress );
		gepInstruction.sourceOperands.push_back( objectVariable );
		gepInstruction.sourceOperands.push_back( indexVariable );
		gepInstruction.operandType = hirIndexAccess.resolvedType;
		gepInstruction.sourceLocation = hirIndexAccess.sourceLocation;
		
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			"_index", hirIndexAccess.resolvedType, false
		);
		gepInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( gepInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerConstruct( hir::HIRConstruct& hirConstruct ) {
		std::vector<MIRVariableIdentifier> fieldVariables;
		for( const std::pair<std::string, hir::HIRNodeSharedPointer>& field : hirConstruct.constructorFields ) {
			fieldVariables.push_back( this->lowerExpression( field.second ) );
		}
		
		MIRInstruction constructInstruction( MIRInstructionKind::ConstructObject );
		constructInstruction.sourceOperands = std::move( fieldVariables );
		constructInstruction.operandType = hirConstruct.constructedType;
		constructInstruction.sourceLocation = hirConstruct.sourceLocation;
		if( hirConstruct.constructedType != nullptr ) {
			constructInstruction.calledFunctionQualifiedName = hirConstruct.constructedType->name;
		}
		
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			"_construct", hirConstruct.constructedType, false
		);
		constructInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( constructInstruction );
	}
	
	// ===================================================================
	// Instruction Emission
	// ===================================================================
	
	MIRVariableIdentifier MIRLowering::emitInstruction( MIRInstruction instruction ) {
		if( this->currentBlock == nullptr || this->currentBlock->isTerminated ) {
			return instruction.destinationVariable;
		}
		instruction.instructionIdentifier = this->nextInstructionIdentifier++;
		MIRVariableIdentifier destinationVariable = instruction.destinationVariable;
		this->currentBlock->blockInstructions.push_back( std::move( instruction ) );
		return destinationVariable;
	}
	
	void MIRLowering::emitTerminator( MIRInstruction terminator ) {
		if( this->currentBlock == nullptr || this->currentBlock->isTerminated ) {
			return;
		}
		terminator.instructionIdentifier = this->nextInstructionIdentifier++;
		this->currentBlock->blockInstructions.push_back( std::move( terminator ) );
		this->currentBlock->isTerminated = true;
	}
	
	void MIRLowering::switchToBlock( std::shared_ptr<MIRBasicBlock> targetBlock ) {
		this->currentBlock = std::move( targetBlock );
	}
	
	void MIRLowering::ensureBlockTerminated() {
		if( this->currentBlock != nullptr && this->currentBlock->isTerminated == false ) {
			MIRInstruction returnInstruction( MIRInstructionKind::ReturnValue );
			this->emitTerminator( returnInstruction );
		}
	}

} // namespace uranite::ir::mir
