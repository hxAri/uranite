
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

#include <fmt/format.h>

#include "uranite/ir/mir/printer.hpp"

namespace uranite::ir::mir {
	
	std::string MIRPrinter::print( const MIRModuleDefinition& mirModule ) {
		this->outputStream.str( "" );
		this->outputStream.clear();
		this->outputStream << "MIRModule \"" << mirModule.moduleName << "\"\n";
		if( mirModule.typeLayoutTable.empty() == false ) {
			this->outputStream << "\n  Type Layouts:\n";
			for( const std::pair<const std::string, TypeLayoutDescriptor>& typeEntry : mirModule.typeLayoutTable ) {
				const TypeLayoutDescriptor& typeLayout = typeEntry.second;
				this->outputStream << "    " << typeEntry.first
					<< " (size=" << typeLayout.typeSizeInBytes
					<< ", align=" << typeLayout.typeAlignmentInBytes
					<< ", vtable=" << ( typeLayout.hasVirtualTable ? "yes" : "no" ) << ")\n";
			}
		}
		for( const std::shared_ptr<MIRFunctionDefinition>& functionDefinition : mirModule.functionDefinitions ) {
			if( functionDefinition != nullptr ) {
				this->printFunction( *functionDefinition );
			}
		}
		return this->outputStream.str();
	}
	
	void MIRPrinter::printFunction( const MIRFunctionDefinition& functionDefinition ) {
		this->outputStream << "\n  Function " << functionDefinition.functionName;
		if( functionDefinition.ownerClassQualifiedName.empty() == false ) {
			this->outputStream << " [" << functionDefinition.ownerClassQualifiedName << "]";
		}
		this->outputStream << " (params=" << functionDefinition.parameterVariableIdentifiers.size()
			<< ", blocks=" << functionDefinition.controlFlowBlocks.size()
			<< ", vars=" << functionDefinition.variableDescriptorTable.size() << ")\n";
		for( const std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock != nullptr ) {
				this->printBasicBlock( *basicBlock );
			}
		}
	}
	
	void MIRPrinter::printBasicBlock( const MIRBasicBlock& basicBlock ) {
		this->outputStream << "    " << basicBlock.blockLabel << " [bb" << basicBlock.blockIdentifier << "]:\n";
		for( const MIRInstruction& instruction : basicBlock.blockInstructions ) {
			this->printInstruction( instruction );
		}
	}
	
	void MIRPrinter::printInstruction( const MIRInstruction& instruction ) {
		this->outputStream << "      ";
		if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
			this->outputStream << "v" << instruction.destinationVariable << " = ";
		}
		this->outputStream << this->instructionKindToString( instruction.instructionKind );
		for( MIRVariableIdentifier sourceOperand : instruction.sourceOperands ) {
			this->outputStream << " v" << sourceOperand;
		}
		switch( instruction.instructionKind ) {
			case MIRInstructionKind::ConstantInteger:
				this->outputStream << " " << instruction.integerConstantValue;
				break;
			case MIRInstructionKind::ConstantFloat:
				this->outputStream << " " << instruction.floatConstantValue;
				break;
			case MIRInstructionKind::ConstantBoolean:
				this->outputStream << " " << ( instruction.booleanConstantValue ? "true" : "false" );
				break;
			case MIRInstructionKind::ConstantString:
				this->outputStream << " \"" << instruction.stringConstantValue << "\"";
				break;
			case MIRInstructionKind::ConstantChar:
				this->outputStream << " '" << instruction.charConstantValue << "'";
				break;
			case MIRInstructionKind::CallFunction:
			case MIRInstructionKind::LoadVariable:
				if( instruction.calledFunctionQualifiedName.empty() == false ) {
					this->outputStream << " @" << instruction.calledFunctionQualifiedName;
				}
				break;
			case MIRInstructionKind::BranchConditional:
				this->outputStream << " -> bb" << instruction.trueBranchTarget << " / bb" << instruction.falseBranchTarget;
				break;
			case MIRInstructionKind::JumpUnconditional:
				this->outputStream << " -> bb" << instruction.trueBranchTarget;
				break;
			case MIRInstructionKind::SwitchBranch:
				this->outputStream << " default=bb" << instruction.defaultSwitchTarget;
				for( const std::pair<int64_t, MIRBlockIdentifier>& switchTarget : instruction.switchBranchTargets ) {
					this->outputStream << " [" << switchTarget.first << "->bb" << switchTarget.second << "]";
				}
				break;
			case MIRInstructionKind::ComputeFieldAddress:
				this->outputStream << " ." << instruction.fieldAccessName;
				break;
			case MIRInstructionKind::InlineAssembly:
				this->outputStream << " \"" << instruction.assemblyTemplate << "\"";
				break;
			default:
				break;
		}
		this->outputStream << "\n";
	}
	
	std::string MIRPrinter::instructionKindToString( MIRInstructionKind instructionKind ) {
		switch( instructionKind ) {
			case MIRInstructionKind::AllocateLocal:        return "alloc";
			case MIRInstructionKind::LoadVariable:         return "load";
			case MIRInstructionKind::StoreVariable:        return "store";
			case MIRInstructionKind::CopyValue:            return "copy";
			case MIRInstructionKind::MoveValue:            return "move";
			case MIRInstructionKind::DropValue:            return "drop";
			case MIRInstructionKind::AddInteger:           return "add.i";
			case MIRInstructionKind::SubtractInteger:      return "sub.i";
			case MIRInstructionKind::MultiplyInteger:      return "mul.i";
			case MIRInstructionKind::DivideInteger:        return "div.i";
			case MIRInstructionKind::ModuloInteger:        return "mod.i";
			case MIRInstructionKind::NegateInteger:        return "neg.i";
			case MIRInstructionKind::PowerInteger:         return "pow.i";
			case MIRInstructionKind::AddFloat:             return "add.f";
			case MIRInstructionKind::SubtractFloat:        return "sub.f";
			case MIRInstructionKind::MultiplyFloat:        return "mul.f";
			case MIRInstructionKind::DivideFloat:          return "div.f";
			case MIRInstructionKind::NegateFloat:          return "neg.f";
			case MIRInstructionKind::PowerFloat:           return "pow.f";
			case MIRInstructionKind::BitwiseAnd:           return "and";
			case MIRInstructionKind::BitwiseOr:            return "or";
			case MIRInstructionKind::BitwiseXor:           return "xor";
			case MIRInstructionKind::BitwiseNot:           return "not";
			case MIRInstructionKind::ShiftLeft:            return "shl";
			case MIRInstructionKind::ShiftRight:           return "shr";
			case MIRInstructionKind::CompareEqual:         return "cmp.eq";
			case MIRInstructionKind::CompareNotEqual:      return "cmp.ne";
			case MIRInstructionKind::CompareLessThan:      return "cmp.lt";
			case MIRInstructionKind::CompareGreaterThan:   return "cmp.gt";
			case MIRInstructionKind::CompareLessEqual:     return "cmp.le";
			case MIRInstructionKind::CompareGreaterEqual:  return "cmp.ge";
			case MIRInstructionKind::LogicalAnd:           return "land";
			case MIRInstructionKind::LogicalOr:            return "lor";
			case MIRInstructionKind::LogicalNot:           return "lnot";
			case MIRInstructionKind::TakeReference:        return "ref";
			case MIRInstructionKind::DereferencePointer:   return "deref";
			case MIRInstructionKind::ComputeFieldAddress:  return "gep.field";
			case MIRInstructionKind::ComputeIndexAddress:  return "gep.index";
			case MIRInstructionKind::HeapAllocate:         return "heap.alloc";
			case MIRInstructionKind::HeapFree:             return "heap.free";
			case MIRInstructionKind::AddressOf:            return "addressof";
			case MIRInstructionKind::CallFunction:         return "call";
			case MIRInstructionKind::CallVirtual:          return "vcall";
			case MIRInstructionKind::ReturnValue:          return "ret";
			case MIRInstructionKind::BranchConditional:    return "br";
			case MIRInstructionKind::JumpUnconditional:    return "jmp";
			case MIRInstructionKind::SwitchBranch:         return "switch";
			case MIRInstructionKind::InvokeFunction:       return "invoke";
			case MIRInstructionKind::Unreachable:          return "unreachable";
			case MIRInstructionKind::ConstructObject:      return "construct";
			case MIRInstructionKind::DestructObject:       return "destruct";
			case MIRInstructionKind::LoadVirtualTable:     return "load.vtable";
			case MIRInstructionKind::ConstantInteger:      return "const.i";
			case MIRInstructionKind::ConstantFloat:        return "const.f";
			case MIRInstructionKind::ConstantBoolean:      return "const.b";
			case MIRInstructionKind::ConstantString:       return "const.s";
			case MIRInstructionKind::ConstantChar:         return "const.c";
			case MIRInstructionKind::ConstantNone:         return "const.none";
			case MIRInstructionKind::CastType:             return "cast";
			case MIRInstructionKind::InstanceOfCheck:      return "instanceof";
			case MIRInstructionKind::PhiNode:              return "phi";
			case MIRInstructionKind::ThrowException:       return "throw";
			case MIRInstructionKind::LandingPad:           return "landingpad";
			case MIRInstructionKind::DeferPush:            return "defer.push";
			case MIRInstructionKind::DeferEmit:            return "defer.emit";
			case MIRInstructionKind::InlineAssembly:       return "asm";
			case MIRInstructionKind::Yield:                return "yield";
			case MIRInstructionKind::NoOperation:          return "nop";
			default:                                       return "unknown";
		}
	}
	
	void MIRPrinter::indent( int indentLevel ) {
		for( int indentCounter = 0; indentCounter < indentLevel; indentCounter++ ) {
			this->outputStream << "  ";
		}
	}
	
} // namespace uranite::ir::mir
