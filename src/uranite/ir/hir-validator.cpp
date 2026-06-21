
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

#include "uranite/ir/hir-validator.hpp"

namespace uranite::ir::hir {
	
	HIRValidator::HIRValidator( diagnostic::Engine& diagnosticEngine )
		: diagnosticEngine( diagnosticEngine ) {
	}
	
	bool HIRValidator::validate( const HIRModule& hirModule ) {
		this->collectedErrors.clear();
		
		for( const std::shared_ptr<HIRFunctionDefinition>& functionDefinition : hirModule.functionDefinitions ) {
			if( functionDefinition == nullptr ) {
				this->addError( "null function definition in module", hirModule.sourceLocation );
				continue;
			}
			this->validateFunctionDefinition( *functionDefinition );
		}
		
		for( const std::shared_ptr<HIRClassDefinition>& classDefinition : hirModule.classDefinitions ) {
			if( classDefinition == nullptr ) {
				this->addError( "null class definition in module", hirModule.sourceLocation );
				continue;
			}
			this->validateClassDefinition( *classDefinition );
		}
		
		for( const std::shared_ptr<HIRStructDefinition>& structDefinition : hirModule.structDefinitions ) {
			if( structDefinition == nullptr ) {
				this->addError( "null struct definition in module", hirModule.sourceLocation );
				continue;
			}
			this->validateStructDefinition( *structDefinition );
		}
		
		for( const std::shared_ptr<HIREnumDefinition>& enumDefinition : hirModule.enumDefinitions ) {
			if( enumDefinition == nullptr ) {
				this->addError( "null enum definition in module", hirModule.sourceLocation );
				continue;
			}
			this->validateEnumDefinition( *enumDefinition );
		}
		
		for( const std::shared_ptr<HIRInterfaceDefinition>& interfaceDefinition : hirModule.interfaceDefinitions ) {
			if( interfaceDefinition == nullptr ) {
				this->addError( "null interface definition in module", hirModule.sourceLocation );
				continue;
			}
			this->validateInterfaceDefinition( *interfaceDefinition );
		}
		
		for( const std::shared_ptr<HIRExternFunctionDeclaration>& externDeclaration : hirModule.externFunctionDeclarations ) {
			if( externDeclaration == nullptr ) {
				this->addError( "null extern declaration in module", hirModule.sourceLocation );
				continue;
			}
			if( externDeclaration->functionName.empty() ) {
				this->addError( "extern function has empty name", externDeclaration->sourceLocation );
			}
		}
		
		return this->collectedErrors.empty();
	}
	
	const std::vector<std::string>& HIRValidator::validationErrors() const {
		return this->collectedErrors;
	}
	
	void HIRValidator::validateFunctionDefinition( const HIRFunctionDefinition& functionDefinition ) {
		if( functionDefinition.functionName.empty() ) {
			this->addError( "function definition has empty name", functionDefinition.sourceLocation );
		}
		
		for( const HIRParameterDescriptor& parameter : functionDefinition.parameterDescriptors ) {
			if( parameter.parameterName.empty() && parameter.isSelfParameter == false ) {
				this->addError(
					fmt::format( "parameter in function '{}' has empty name", functionDefinition.functionName ),
					functionDefinition.sourceLocation
				);
			}
		}
		
		if( functionDefinition.functionBody != nullptr ) {
			this->validateBlock( *functionDefinition.functionBody );
		}
		else if( functionDefinition.isAbstractMethod == false &&
				 functionDefinition.isNativeMethod == false ) {
			// Non-abstract, non-native function without body is suspicious but not always an error
			// (interface method signatures have no body)
		}
	}
	
	void HIRValidator::validateClassDefinition( const HIRClassDefinition& classDefinition ) {
		if( classDefinition.className.empty() ) {
			this->addError( "class definition has empty name", classDefinition.sourceLocation );
		}
		
		for( const HIRFieldDescriptor& field : classDefinition.fieldDescriptors ) {
			if( field.fieldName.empty() ) {
				this->addError(
					fmt::format( "field in class '{}' has empty name", classDefinition.className ),
					classDefinition.sourceLocation
				);
			}
		}
		
		for( const std::shared_ptr<HIRFunctionDefinition>& method : classDefinition.methodDefinitions ) {
			if( method == nullptr ) {
				this->addError(
					fmt::format( "null method in class '{}'", classDefinition.className ),
					classDefinition.sourceLocation
				);
				continue;
			}
			this->validateFunctionDefinition( *method );
		}
	}
	
	void HIRValidator::validateStructDefinition( const HIRStructDefinition& structDefinition ) {
		if( structDefinition.structName.empty() ) {
			this->addError( "struct definition has empty name", structDefinition.sourceLocation );
		}
		
		for( const HIRFieldDescriptor& field : structDefinition.fieldDescriptors ) {
			if( field.fieldName.empty() ) {
				this->addError(
					fmt::format( "field in struct '{}' has empty name", structDefinition.structName ),
					structDefinition.sourceLocation
				);
			}
		}
		
		for( const std::shared_ptr<HIRFunctionDefinition>& method : structDefinition.methodDefinitions ) {
			if( method == nullptr ) {
				this->addError(
					fmt::format( "null method in struct '{}'", structDefinition.structName ),
					structDefinition.sourceLocation
				);
				continue;
			}
			this->validateFunctionDefinition( *method );
		}
	}
	
	void HIRValidator::validateEnumDefinition( const HIREnumDefinition& enumDefinition ) {
		if( enumDefinition.enumName.empty() ) {
			this->addError( "enum definition has empty name", enumDefinition.sourceLocation );
		}
		
		for( const HIREnumVariantDescriptor& variant : enumDefinition.variantDescriptors ) {
			if( variant.variantName.empty() ) {
				this->addError(
					fmt::format( "variant in enum '{}' has empty name", enumDefinition.enumName ),
					enumDefinition.sourceLocation
				);
			}
		}
		
		for( const std::shared_ptr<HIRFunctionDefinition>& method : enumDefinition.methodDefinitions ) {
			if( method == nullptr ) {
				this->addError(
					fmt::format( "null method in enum '{}'", enumDefinition.enumName ),
					enumDefinition.sourceLocation
				);
				continue;
			}
			this->validateFunctionDefinition( *method );
		}
	}
	
	void HIRValidator::validateInterfaceDefinition( const HIRInterfaceDefinition& interfaceDefinition ) {
		if( interfaceDefinition.interfaceName.empty() ) {
			this->addError( "interface definition has empty name", interfaceDefinition.sourceLocation );
		}
		
		for( const std::shared_ptr<HIRFunctionDefinition>& method : interfaceDefinition.methodDefinitions ) {
			if( method == nullptr ) {
				this->addError(
					fmt::format( "null method in interface '{}'", interfaceDefinition.interfaceName ),
					interfaceDefinition.sourceLocation
				);
				continue;
			}
			this->validateFunctionDefinition( *method );
		}
	}
	
	void HIRValidator::validateBlock( const HIRBlock& block ) {
		for( const HIRNodeSharedPointer& statement : block.blockStatements ) {
			if( statement == nullptr ) {
				this->addError( "null statement in block", block.sourceLocation );
				continue;
			}
			this->validateNode( statement );
		}
	}
	
	void HIRValidator::validateNode( const HIRNodeSharedPointer& node ) {
		if( node == nullptr ) {
			return;
		}
		
		switch( node->nodeKind ) {
			case HIRNodeKind::Block: {
				HIRBlock& block = static_cast<HIRBlock&>( *node );
				this->validateBlock( block );
				break;
			}
			
			case HIRNodeKind::VariableBinding: {
				HIRVariableBinding& binding = static_cast<HIRVariableBinding&>( *node );
				if( binding.variableName.empty() ) {
					this->addError( "variable binding has empty name", binding.sourceLocation );
				}
				if( binding.initializerExpression != nullptr ) {
					this->validateExpression( binding.initializerExpression );
				}
				break;
			}
			
			case HIRNodeKind::Assignment: {
				HIRAssignment& assignment = static_cast<HIRAssignment&>( *node );
				if( assignment.targetExpression == nullptr ) {
					this->addError( "assignment has null target", assignment.sourceLocation );
				}
				else {
					this->validateExpression( assignment.targetExpression );
				}
				if( assignment.valueExpression == nullptr ) {
					this->addError( "assignment has null value", assignment.sourceLocation );
				}
				else {
					this->validateExpression( assignment.valueExpression );
				}
				break;
			}
			
			case HIRNodeKind::Return: {
				HIRReturn& returnNode = static_cast<HIRReturn&>( *node );
				if( returnNode.returnValueExpression != nullptr ) {
					this->validateExpression( returnNode.returnValueExpression );
				}
				break;
			}
			
			case HIRNodeKind::If: {
				HIRIf& ifNode = static_cast<HIRIf&>( *node );
				if( ifNode.branchCondition == nullptr ) {
					this->addError( "if statement has null condition", ifNode.sourceLocation );
				}
				else {
					this->validateExpression( ifNode.branchCondition );
				}
				if( ifNode.thenBranch == nullptr ) {
					this->addError( "if statement has null then-branch", ifNode.sourceLocation );
				}
				else {
					this->validateBlock( *ifNode.thenBranch );
				}
				if( ifNode.elseBranch != nullptr ) {
					this->validateNode( ifNode.elseBranch );
				}
				break;
			}
			
			case HIRNodeKind::Loop: {
				HIRLoop& loop = static_cast<HIRLoop&>( *node );
				if( loop.loopCondition != nullptr ) {
					this->validateExpression( loop.loopCondition );
				}
				if( loop.loopBody == nullptr ) {
					this->addError( "loop has null body", loop.sourceLocation );
				}
				else {
					this->validateBlock( *loop.loopBody );
				}
				if( loop.loopInitializer != nullptr ) {
					this->validateExpression( loop.loopInitializer );
				}
				if( loop.loopUpdate != nullptr ) {
					this->validateNode( loop.loopUpdate );
				}
				if( loop.isIteratorLoop && loop.iterableExpression == nullptr ) {
					this->addError( "iterator loop has null iterable expression", loop.sourceLocation );
				}
				break;
			}
			
			case HIRNodeKind::Match: {
				HIRMatch& matchNode = static_cast<HIRMatch&>( *node );
				if( matchNode.matchSubject == nullptr ) {
					this->addError( "match statement has null subject", matchNode.sourceLocation );
				}
				else {
					this->validateExpression( matchNode.matchSubject );
				}
				for( const HIRMatchArm& arm : matchNode.matchArms ) {
					if( arm.armBody != nullptr ) {
						this->validateBlock( *arm.armBody );
					}
					if( arm.armPattern != nullptr ) {
						this->validateExpression( arm.armPattern );
					}
				}
				break;
			}
			
			case HIRNodeKind::Switch: {
				HIRSwitch& switchNode = static_cast<HIRSwitch&>( *node );
				if( switchNode.switchSubject == nullptr ) {
					this->addError( "switch statement has null subject", switchNode.sourceLocation );
				}
				else {
					this->validateExpression( switchNode.switchSubject );
				}
				for( const HIRSwitchCase& switchCase : switchNode.switchCases ) {
					if( switchCase.caseBody != nullptr ) {
						this->validateBlock( *switchCase.caseBody );
					}
				}
				break;
			}
			
			case HIRNodeKind::Defer: {
				HIRDefer& deferNode = static_cast<HIRDefer&>( *node );
				if( deferNode.deferredStatement == nullptr ) {
					this->addError( "defer has null deferred statement", deferNode.sourceLocation );
				}
				else {
					this->validateNode( deferNode.deferredStatement );
				}
				break;
			}
			
			case HIRNodeKind::Delete: {
				HIRDelete& deleteNode = static_cast<HIRDelete&>( *node );
				if( deleteNode.targetExpression == nullptr ) {
					this->addError( "delete has null target", deleteNode.sourceLocation );
				}
				else {
					this->validateExpression( deleteNode.targetExpression );
				}
				break;
			}
			
			case HIRNodeKind::Throw: {
				HIRThrow& throwNode = static_cast<HIRThrow&>( *node );
				if( throwNode.thrownExpression == nullptr ) {
					this->addError( "throw has null expression", throwNode.sourceLocation );
				}
				else {
					this->validateExpression( throwNode.thrownExpression );
				}
				break;
			}
			
			case HIRNodeKind::TryCatch: {
				HIRTryCatch& tryCatchNode = static_cast<HIRTryCatch&>( *node );
				if( tryCatchNode.tryBody == nullptr ) {
					this->addError( "try-catch has null try body", tryCatchNode.sourceLocation );
				}
				else {
					this->validateBlock( *tryCatchNode.tryBody );
				}
				for( const HIRExceptionHandler& handler : tryCatchNode.exceptionHandlers ) {
					if( handler.handlerBody != nullptr ) {
						this->validateBlock( *handler.handlerBody );
					}
				}
				if( tryCatchNode.finallyBody != nullptr ) {
					this->validateBlock( *tryCatchNode.finallyBody );
				}
				break;
			}
			
			case HIRNodeKind::UnsafeBlock: {
				HIRUnsafeBlock& unsafeBlock = static_cast<HIRUnsafeBlock&>( *node );
				if( unsafeBlock.unsafeBody != nullptr ) {
					this->validateBlock( *unsafeBlock.unsafeBody );
				}
				break;
			}
			
			case HIRNodeKind::ExpressionStatement: {
				HIRExpressionStatement& exprStatement = static_cast<HIRExpressionStatement&>( *node );
				if( exprStatement.expression == nullptr ) {
					this->addError( "expression statement has null expression", exprStatement.sourceLocation );
				}
				else {
					this->validateExpression( exprStatement.expression );
				}
				break;
			}
			
			case HIRNodeKind::Break:
			case HIRNodeKind::Continue:
			case HIRNodeKind::Pass:
			case HIRNodeKind::InlineAssembly:
			case HIRNodeKind::Drop:
				break;
			
			default:
				break;
		}
	}
	
	void HIRValidator::validateExpression( const HIRNodeSharedPointer& expression ) {
		if( expression == nullptr ) {
			return;
		}
		
		switch( expression->nodeKind ) {
			case HIRNodeKind::BinaryOperation: {
				HIRBinaryOperation& binaryOp = static_cast<HIRBinaryOperation&>( *expression );
				if( binaryOp.leftOperand == nullptr ) {
					this->addError( "binary operation has null left operand", binaryOp.sourceLocation );
				}
				else {
					this->validateExpression( binaryOp.leftOperand );
				}
				if( binaryOp.rightOperand == nullptr ) {
					this->addError( "binary operation has null right operand", binaryOp.sourceLocation );
				}
				else {
					this->validateExpression( binaryOp.rightOperand );
				}
				break;
			}
			
			case HIRNodeKind::UnaryOperation: {
				HIRUnaryOperation& unaryOp = static_cast<HIRUnaryOperation&>( *expression );
				if( unaryOp.operandExpression == nullptr ) {
					this->addError( "unary operation has null operand", unaryOp.sourceLocation );
				}
				else {
					this->validateExpression( unaryOp.operandExpression );
				}
				break;
			}
			
			case HIRNodeKind::FunctionCall: {
				HIRFunctionCall& functionCall = static_cast<HIRFunctionCall&>( *expression );
				if( functionCall.calleeExpression == nullptr ) {
					this->addError( "function call has null callee", functionCall.sourceLocation );
				}
				else {
					this->validateExpression( functionCall.calleeExpression );
				}
				for( const HIRNodeSharedPointer& argument : functionCall.callArguments ) {
					this->validateExpression( argument );
				}
				break;
			}
			
			case HIRNodeKind::MethodCall: {
				HIRMethodCall& methodCall = static_cast<HIRMethodCall&>( *expression );
				if( methodCall.receiverObject == nullptr ) {
					this->addError( "method call has null receiver", methodCall.sourceLocation );
				}
				else {
					this->validateExpression( methodCall.receiverObject );
				}
				if( methodCall.methodName.empty() ) {
					this->addError( "method call has empty method name", methodCall.sourceLocation );
				}
				for( const HIRNodeSharedPointer& argument : methodCall.callArguments ) {
					this->validateExpression( argument );
				}
				break;
			}
			
			case HIRNodeKind::FieldAccess: {
				HIRFieldAccess& fieldAccess = static_cast<HIRFieldAccess&>( *expression );
				if( fieldAccess.objectExpression == nullptr ) {
					this->addError( "field access has null object", fieldAccess.sourceLocation );
				}
				else {
					this->validateExpression( fieldAccess.objectExpression );
				}
				if( fieldAccess.fieldName.empty() ) {
					this->addError( "field access has empty field name", fieldAccess.sourceLocation );
				}
				break;
			}
			
			case HIRNodeKind::IndexAccess: {
				HIRIndexAccess& indexAccess = static_cast<HIRIndexAccess&>( *expression );
				if( indexAccess.objectExpression == nullptr ) {
					this->addError( "index access has null object", indexAccess.sourceLocation );
				}
				else {
					this->validateExpression( indexAccess.objectExpression );
				}
				if( indexAccess.indexExpression == nullptr ) {
					this->addError( "index access has null index", indexAccess.sourceLocation );
				}
				else {
					this->validateExpression( indexAccess.indexExpression );
				}
				break;
			}
			
			case HIRNodeKind::Construct: {
				HIRConstruct& construct = static_cast<HIRConstruct&>( *expression );
				for( const std::pair<std::string, HIRNodeSharedPointer>& field : construct.constructorFields ) {
					if( field.second != nullptr ) {
						this->validateExpression( field.second );
					}
				}
				break;
			}
			
			case HIRNodeKind::Cast: {
				HIRCast& castNode = static_cast<HIRCast&>( *expression );
				if( castNode.sourceExpression == nullptr ) {
					this->addError( "cast has null source expression", castNode.sourceLocation );
				}
				else {
					this->validateExpression( castNode.sourceExpression );
				}
				break;
			}
			
			case HIRNodeKind::Lambda: {
				HIRLambda& lambda = static_cast<HIRLambda&>( *expression );
				if( lambda.lambdaBody != nullptr ) {
					this->validateBlock( *lambda.lambdaBody );
				}
				break;
			}
			
			case HIRNodeKind::Reference:
			case HIRNodeKind::Dereference:
			case HIRNodeKind::MoveTransfer:
			case HIRNodeKind::AddressOf:
			case HIRNodeKind::Await:
			case HIRNodeKind::Yield:
			case HIRNodeKind::IntegerLiteral:
			case HIRNodeKind::FloatLiteral:
			case HIRNodeKind::BooleanLiteral:
			case HIRNodeKind::StringLiteral:
			case HIRNodeKind::CharLiteral:
			case HIRNodeKind::NoneLiteral:
			case HIRNodeKind::Identifier:
			case HIRNodeKind::SelfReference:
			case HIRNodeKind::SuperReference:
			case HIRNodeKind::TypeReference:
			case HIRNodeKind::ArrayLiteral:
			case HIRNodeKind::TupleLiteral:
			case HIRNodeKind::RangeExpression:
			case HIRNodeKind::MatchExpression:
			case HIRNodeKind::InstanceOf:
			case HIRNodeKind::SubclassOf:
			case HIRNodeKind::Comprehension:
				break;
			
			default:
				break;
		}
	}
	
	void HIRValidator::addError( const std::string& errorMessage, const lookup::SourceSharedPointer& sourceLocation ) {
		std::string fullMessage = errorMessage;
		if( sourceLocation != nullptr && sourceLocation->location != nullptr ) {
			fullMessage = fmt::format( "{}:{}:{}: {}", sourceLocation->filename, sourceLocation->location->line, sourceLocation->location->column, errorMessage );
		}
		this->collectedErrors.push_back( fullMessage );
	}

} // namespace uranite::ir::hir
