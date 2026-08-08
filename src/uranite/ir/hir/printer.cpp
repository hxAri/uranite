
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

#include "uranite/ir/hir/printer.hpp"

namespace uranite::ir::hir {
	
	std::string HIRPrinter::print( const HIRModule& hirModule ) {
		this->outputStream.str( "" );
		this->outputStream << "HIRModule \"" << hirModule.moduleName << "\"" << std::endl;
		for( const std::shared_ptr<HIRExternFunctionDeclaration>& externDeclaration : hirModule.externFunctionDeclarations ) {
			if( externDeclaration != nullptr ) {
				this->indent( 1 );
				this->outputStream << "ExternFunction \"" << externDeclaration->functionName << "\" link=\"" << externDeclaration->linkageName << "\"" << std::endl;
			}
		}
		for( const std::shared_ptr<HIRConstantDefinition>& constantDefinition : hirModule.constantDefinitions ) {
			if( constantDefinition != nullptr ) {
				this->indent( 1 );
				this->outputStream << "Constant \"" << constantDefinition->constantName << "\"" << std::endl;
			}
		}
		for( const std::shared_ptr<HIRGlobalVariableDefinition>& globalVariable : hirModule.globalVariableDefinitions ) {
			if( globalVariable != nullptr ) {
				this->indent( 1 );
				this->outputStream << "GlobalVariable \"" << globalVariable->variableName << "\"" << std::endl;
			}
		}
		for( const std::shared_ptr<HIREnumDefinition>& enumDefinition : hirModule.enumDefinitions ) {
			if( enumDefinition != nullptr ) {
				this->printEnumDefinition( *enumDefinition, 1 );
			}
		}
		for( const std::shared_ptr<HIRInterfaceDefinition>& interfaceDefinition : hirModule.interfaceDefinitions ) {
			if( interfaceDefinition != nullptr ) {
				this->printInterfaceDefinition( *interfaceDefinition, 1 );
			}
		}
		for( const std::shared_ptr<HIRStructDefinition>& structDefinition : hirModule.structDefinitions ) {
			if( structDefinition != nullptr ) {
				this->printStructDefinition( *structDefinition, 1 );
			}
		}
		for( const std::shared_ptr<HIRClassDefinition>& classDefinition : hirModule.classDefinitions ) {
			if( classDefinition != nullptr ) {
				this->printClassDefinition( *classDefinition, 1 );
			}
		}
		for( const std::shared_ptr<HIRFunctionDefinition>& functionDefinition : hirModule.functionDefinitions ) {
			if( functionDefinition != nullptr ) {
				this->printFunctionDefinition( *functionDefinition, 1 );
			}
		}
		return this->outputStream.str();
	}
	
	void HIRPrinter::printFunctionDefinition( const HIRFunctionDefinition& function, int indentLevel ) {
		this->indent( indentLevel );
		this->outputStream << "HIRFunctionDefinition \"" << function.functionName << "\"";
		if( function.ownerClassName.empty() == false ) {
			this->outputStream << " owner=\"" << function.ownerClassName << "\"";
		}
		if( function.isAsyncFunction ) {
			this->outputStream << " async";
		}
		if( function.isStaticMethod ) {
			this->outputStream << " static";
		}
		if( function.isVirtualMethod ) {
			this->outputStream << " virtual";
		}
		this->outputStream << std::endl;
		for( const HIRParameterDescriptor& parameter : function.parameterDescriptors ) {
			this->indent( indentLevel + 1 );
			this->outputStream << "Parameter \"" << parameter.parameterName << "\"";
			if( parameter.isSelfParameter ) {
				this->outputStream << " self";
			}
			if( parameter.isVariadicParameter ) {
				this->outputStream << " variadic";
			}
			this->outputStream << std::endl;
		}
		if( function.functionBody != nullptr ) {
			this->printBlock( *function.functionBody, indentLevel + 1 );
		}
	}
	
	void HIRPrinter::printClassDefinition( const HIRClassDefinition& classDefinition, int indentLevel ) {
		this->indent( indentLevel );
		this->outputStream << "HIRClassDefinition \"" << classDefinition.className << "\"";
		if( classDefinition.isAbstractClass ) {
			this->outputStream << " abstract";
		}
		if( classDefinition.isFinalClass ) {
			this->outputStream << " final";
		}
		this->outputStream << std::endl;
		for( const HIRFieldDescriptor& field : classDefinition.fieldDescriptors ) {
			this->indent( indentLevel + 1 );
			this->outputStream << "Field \"" << field.fieldName << "\" index=" << field.fieldIndex << std::endl;
		}
		for( const std::shared_ptr<HIRFunctionDefinition>& method : classDefinition.methodDefinitions ) {
			if( method != nullptr ) {
				this->printFunctionDefinition( *method, indentLevel + 1 );
			}
		}
	}
	
	void HIRPrinter::printStructDefinition( const HIRStructDefinition& structDefinition, int indentLevel ) {
		this->indent( indentLevel );
		this->outputStream << "HIRStructDefinition \"" << structDefinition.structName << "\"" << std::endl;
		for( const HIRFieldDescriptor& field : structDefinition.fieldDescriptors ) {
			this->indent( indentLevel + 1 );
			this->outputStream << "Field \"" << field.fieldName << "\" index=" << field.fieldIndex << std::endl;
		}
		for( const std::shared_ptr<HIRFunctionDefinition>& method : structDefinition.methodDefinitions ) {
			if( method != nullptr ) {
				this->printFunctionDefinition( *method, indentLevel + 1 );
			}
		}
	}
	
	void HIRPrinter::printEnumDefinition( const HIREnumDefinition& enumDefinition, int indentLevel ) {
		this->indent( indentLevel );
		this->outputStream << "HIREnumDefinition \"" << enumDefinition.enumName << "\"" << std::endl;
		for( const HIREnumVariantDescriptor& variant : enumDefinition.variantDescriptors ) {
			this->indent( indentLevel + 1 );
			this->outputStream << "Variant \"" << variant.variantName << "\"" << std::endl;
		}
	}
	
	void HIRPrinter::printInterfaceDefinition( const HIRInterfaceDefinition& interfaceDefinition, int indentLevel ) {
		this->indent( indentLevel );
		this->outputStream << "HIRInterfaceDefinition \"" << interfaceDefinition.interfaceName << "\"" << std::endl;
		for( const std::shared_ptr<HIRFunctionDefinition>& method : interfaceDefinition.methodDefinitions ) {
			if( method != nullptr ) {
				this->printFunctionDefinition( *method, indentLevel + 1 );
			}
		}
	}
	
	void HIRPrinter::printBlock( const HIRBlock& block, int indentLevel ) {
		this->indent( indentLevel );
		this->outputStream << "Block" << std::endl;
		for( const HIRNodeSharedPointer& statement : block.blockStatements ) {
			this->printNode( statement, indentLevel + 1 );
		}
	}
	
	void HIRPrinter::printNode( const HIRNodeSharedPointer& node, int indentLevel ) {
		if( node == nullptr ) {
			return;
		}
		this->indent( indentLevel );
		this->outputStream << this->nodeKindToString( node->nodeKind );
		switch( node->nodeKind ) {
			case HIRNodeKind::VariableBinding: {
				HIRVariableBinding& variable = static_cast<HIRVariableBinding&>( *node );
				this->outputStream << " \"" << variable.variableName << "\"";
				if( variable.isMutableBinding ) {
					this->outputStream << " mutable";
				}
				this->outputStream << std::endl;
				if( variable.initializerExpression != nullptr ) {
					this->printNode( variable.initializerExpression, indentLevel + 1 );
				}
				break;
			}
			case HIRNodeKind::Assignment: {
				HIRAssignment& assignment = static_cast<HIRAssignment&>( *node );
				this->outputStream << std::endl;
				this->printNode( assignment.targetExpression, indentLevel + 1 );
				this->printNode( assignment.valueExpression, indentLevel + 1 );
				break;
			}
			case HIRNodeKind::Return: {
				HIRReturn& returnNode = static_cast<HIRReturn&>( *node );
				this->outputStream << std::endl;
				if( returnNode.returnValueExpression != nullptr ) {
					this->printNode( returnNode.returnValueExpression, indentLevel + 1 );
				}
				break;
			}
			case HIRNodeKind::If: {
				HIRIf& ifNode = static_cast<HIRIf&>( *node );
				this->outputStream << std::endl;
				this->printNode( ifNode.branchCondition, indentLevel + 1 );
				if( ifNode.thenBranch != nullptr ) {
					this->printBlock( *ifNode.thenBranch, indentLevel + 1 );
				}
				if( ifNode.elseBranch != nullptr ) {
					this->printNode( ifNode.elseBranch, indentLevel + 1 );
				}
				break;
			}
			case HIRNodeKind::Loop: {
				HIRLoop& loop = static_cast<HIRLoop&>( *node );
				if( loop.loopVariableName.empty() == false ) {
					this->outputStream << " var=\"" << loop.loopVariableName << "\"";
				}
				if( loop.isIteratorLoop ) {
					this->outputStream << " iterator";
				}
				this->outputStream << std::endl;
				if( loop.loopCondition != nullptr ) {
					this->indent( indentLevel + 1 );
					this->outputStream << "Condition:" << std::endl;
					this->printNode( loop.loopCondition, indentLevel + 2 );
				}
				if( loop.loopBody != nullptr ) {
					this->printBlock( *loop.loopBody, indentLevel + 1 );
				}
				break;
			}
			case HIRNodeKind::Block: {
				HIRBlock& block = static_cast<HIRBlock&>( *node );
				this->outputStream << std::endl;
				for( const HIRNodeSharedPointer& statement : block.blockStatements ) {
					this->printNode( statement, indentLevel + 1 );
				}
				break;
			}
			case HIRNodeKind::IntegerLiteral: {
				HIRIntegerLiteral& literal = static_cast<HIRIntegerLiteral&>( *node );
				this->outputStream << " " << literal.integerValue << std::endl;
				break;
			}
			case HIRNodeKind::FloatLiteral: {
				HIRFloatLiteral& literal = static_cast<HIRFloatLiteral&>( *node );
				this->outputStream << " " << literal.floatValue << std::endl;
				break;
			}
			case HIRNodeKind::BooleanLiteral: {
				HIRBooleanLiteral& literal = static_cast<HIRBooleanLiteral&>( *node );
				this->outputStream << " " << ( literal.booleanValue ? "true" : "false" ) << std::endl;
				break;
			}
			case HIRNodeKind::StringLiteral: {
				HIRStringLiteral& literal = static_cast<HIRStringLiteral&>( *node );
				this->outputStream << " \"" << literal.stringValue << "\"" << std::endl;
				break;
			}
			case HIRNodeKind::Identifier: {
				HIRIdentifier& identifier = static_cast<HIRIdentifier&>( *node );
				this->outputStream << " \"" << identifier.identifierName << "\"" << std::endl;
				break;
			}
			case HIRNodeKind::BinaryOperation: {
				HIRBinaryOperation& binaryOperation = static_cast<HIRBinaryOperation&>( *node );
				this->outputStream << " op=" << static_cast<int>( binaryOperation.operatorKind ) << std::endl;
				this->printNode( binaryOperation.leftOperand, indentLevel + 1 );
				this->printNode( binaryOperation.rightOperand, indentLevel + 1 );
				break;
			}
			case HIRNodeKind::UnaryOperation: {
				HIRUnaryOperation& unaryOperation = static_cast<HIRUnaryOperation&>( *node );
				this->outputStream << " op=" << static_cast<int>( unaryOperation.operatorKind );
				this->outputStream << ( unaryOperation.isPrefixOperator ? " prefix" : " postfix" ) << std::endl;
				this->printNode( unaryOperation.operandExpression, indentLevel + 1 );
				break;
			}
			case HIRNodeKind::FunctionCall: {
				HIRFunctionCall& functionCall = static_cast<HIRFunctionCall&>( *node );
				this->outputStream << " args=" << functionCall.callArguments.size() << std::endl;
				this->printNode( functionCall.calleeExpression, indentLevel + 1 );
				for( const HIRNodeSharedPointer& argument : functionCall.callArguments ) {
					this->printNode( argument, indentLevel + 1 );
				}
				break;
			}
			case HIRNodeKind::MethodCall: {
				HIRMethodCall& methodCall = static_cast<HIRMethodCall&>( *node );
				this->outputStream << " \"" << methodCall.methodName << "\" args=" << methodCall.callArguments.size() << std::endl;
				this->printNode( methodCall.receiverObject, indentLevel + 1 );
				for( const HIRNodeSharedPointer& argument : methodCall.callArguments ) {
					this->printNode( argument, indentLevel + 1 );
				}
				break;
			}
			case HIRNodeKind::FieldAccess: {
				HIRFieldAccess& fieldAccess = static_cast<HIRFieldAccess&>( *node );
				this->outputStream << " \"" << fieldAccess.fieldName << "\"" << std::endl;
				this->printNode( fieldAccess.objectExpression, indentLevel + 1 );
				break;
			}
			case HIRNodeKind::Construct: {
				HIRConstruct& construct = static_cast<HIRConstruct&>( *node );
				this->outputStream << " fields=" << construct.constructorFields.size() << std::endl;
				break;
			}
			default:
				this->outputStream << std::endl;
				break;
		}
	}
	
	void HIRPrinter::indent( int indentLevel ) {
		for( int levelIndex = 0; levelIndex < indentLevel; levelIndex++ ) {
			this->outputStream << "  ";
		}
	}
	
	std::string HIRPrinter::nodeKindToString( HIRNodeKind nodeKind ) {
		switch( nodeKind ) {
			case HIRNodeKind::Module: return "Module";
			case HIRNodeKind::FunctionDefinition: return "FunctionDefinition";
			case HIRNodeKind::ClassDefinition: return "ClassDefinition";
			case HIRNodeKind::StructDefinition: return "StructDefinition";
			case HIRNodeKind::EnumDefinition: return "EnumDefinition";
			case HIRNodeKind::InterfaceDefinition: return "InterfaceDefinition";
			case HIRNodeKind::GlobalVariableDefinition: return "GlobalVariableDefinition";
			case HIRNodeKind::ExternFunctionDeclaration: return "ExternFunctionDeclaration";
			case HIRNodeKind::ConstantDefinition: return "ConstantDefinition";
			case HIRNodeKind::Block: return "Block";
			case HIRNodeKind::VariableBinding: return "VariableBinding";
			case HIRNodeKind::Assignment: return "Assignment";
			case HIRNodeKind::Return: return "Return";
			case HIRNodeKind::If: return "If";
			case HIRNodeKind::Match: return "Match";
			case HIRNodeKind::Loop: return "Loop";
			case HIRNodeKind::Break: return "Break";
			case HIRNodeKind::Continue: return "Continue";
			case HIRNodeKind::Defer: return "Defer";
			case HIRNodeKind::Delete: return "Delete";
			case HIRNodeKind::Throw: return "Throw";
			case HIRNodeKind::TryCatch: return "TryCatch";
			case HIRNodeKind::InlineAssembly: return "InlineAssembly";
			case HIRNodeKind::UnsafeBlock: return "UnsafeBlock";
			case HIRNodeKind::ExpressionStatement: return "ExpressionStatement";
			case HIRNodeKind::Pass: return "Pass";
			case HIRNodeKind::Switch: return "Switch";
			case HIRNodeKind::Drop: return "Drop";
			case HIRNodeKind::IntegerLiteral: return "IntegerLiteral";
			case HIRNodeKind::FloatLiteral: return "FloatLiteral";
			case HIRNodeKind::BooleanLiteral: return "BooleanLiteral";
			case HIRNodeKind::StringLiteral: return "StringLiteral";
			case HIRNodeKind::CharLiteral: return "CharLiteral";
			case HIRNodeKind::NoneLiteral: return "NoneLiteral";
			case HIRNodeKind::Identifier: return "Identifier";
			case HIRNodeKind::SelfReference: return "SelfReference";
			case HIRNodeKind::ParentReference: return "ParentReference";
			case HIRNodeKind::BinaryOperation: return "BinaryOperation";
			case HIRNodeKind::UnaryOperation: return "UnaryOperation";
			case HIRNodeKind::FunctionCall: return "FunctionCall";
			case HIRNodeKind::MethodCall: return "MethodCall";
			case HIRNodeKind::FieldAccess: return "FieldAccess";
			case HIRNodeKind::IndexAccess: return "IndexAccess";
			case HIRNodeKind::Construct: return "Construct";
			case HIRNodeKind::Cast: return "Cast";
			case HIRNodeKind::Reference: return "Reference";
			case HIRNodeKind::Dereference: return "Dereference";
			case HIRNodeKind::MoveTransfer: return "MoveTransfer";
			case HIRNodeKind::AddressOf: return "AddressOf";
			case HIRNodeKind::Lambda: return "Lambda";
			case HIRNodeKind::Await: return "Await";
			case HIRNodeKind::Yield: return "Yield";
			case HIRNodeKind::MatchExpression: return "MatchExpression";
			case HIRNodeKind::ArrayLiteral: return "ArrayLiteral";
			case HIRNodeKind::TupleLiteral: return "TupleLiteral";
			case HIRNodeKind::TypeReference: return "TypeReference";
			case HIRNodeKind::InstanceOf: return "InstanceOf";
			case HIRNodeKind::SubclassOf: return "SubclassOf";
			case HIRNodeKind::RangeExpression: return "RangeExpression";
			case HIRNodeKind::Comprehension: return "Comprehension";
		}
		return "Unknown";
	}
	
} // namespace uranite::ir::hir
