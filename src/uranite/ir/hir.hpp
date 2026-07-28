
//
// @author hxAri (hxari)
// @create 2025-02-24 15:15
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

#ifndef _URANITE_IR_HIR_HPP_
#define _URANITE_IR_HIR_HPP_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "uranite/ast/node.hpp"
#include "uranite/lookup/source.hpp"
#include "uranite/semantic/typeref.hpp"
#include "uranite/token/token.hpp"

namespace uranite::ir::hir {
	
	/** @brief Categorizes every HIR node into exactly one structural role. */
	enum class HIRNodeKind {
		
		// Module-level declarations
		Module,
		FunctionDefinition,
		ClassDefinition,
		StructDefinition,
		EnumDefinition,
		InterfaceDefinition,
		GlobalVariableDefinition,
		ExternFunctionDeclaration,
		ConstantDefinition,
		
		// Statements
		Block,
		VariableBinding,
		Assignment,
		Return,
		If,
		Match,
		Loop,
		Break,
		Continue,
		Defer,
		Delete,
		Throw,
		TryCatch,
		InlineAssembly,
		UnsafeBlock,
		ExpressionStatement,
		Pass,
		Switch,
		Drop,
		
		// Expressions
		IntegerLiteral,
		FloatLiteral,
		BooleanLiteral,
		StringLiteral,
		CharLiteral,
		NoneLiteral,
		Identifier,
		SelfReference,
		SuperReference,
		BinaryOperation,
		UnaryOperation,
		FunctionCall,
		MethodCall,
		FieldAccess,
		IndexAccess,
		Construct,
		Cast,
		Reference,
		Dereference,
		MoveTransfer,
		AddressOf,
		Lambda,
		Await,
		Yield,
		MatchExpression,
		ArrayLiteral,
		TupleLiteral,
		TypeReference,
		InstanceOf,
		SubclassOf,
		RangeExpression,
		Comprehension,
		MapLiteral,
		SetLiteral
	
	};
	
	// Forward declarations
	struct HIRNode;
	struct HIRBlock;
	struct HIRFunctionDefinition;
	struct HIRClassDefinition;
	struct HIRStructDefinition;
	struct HIREnumDefinition;
	struct HIRInterfaceDefinition;
	struct HIRExternFunctionDeclaration;
	
	using HIRNodeSharedPointer = std::shared_ptr<HIRNode>;
	
	/** @brief Root of the HIR node hierarchy; carries resolved type and source location. */
	struct HIRNode {
		
		HIRNodeKind nodeKind;
		semantic::TypeSharedPointer resolvedType;
		lookup::SourceSharedPointer sourceLocation;
		std::string qualifiedScopeName;
		
		HIRNode(
			HIRNodeKind nodeKind,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : nodeKind( nodeKind ),
			resolvedType( std::move( resolvedType ) ),
			sourceLocation( sourceLocation ) {
		}
		
		virtual ~HIRNode() = default;
	
	};
	
	/** @brief Describes a single function/method parameter in HIR. */
	struct HIRParameterDescriptor {
		std::string parameterName;
		semantic::TypeSharedPointer parameterType;
		HIRNodeSharedPointer defaultValueExpression;
		bool isSelfParameter = false;
		bool isVariadicParameter = false;
		bool isMutableParameter = false;
		bool isReferenceParameter = false;
		bool isKeywordParameter = false;
		bool isPropertyParameter = false;
		ast::AccessModifier propertyAccess = ast::AccessModifier::Default;
		bool isReadonlyProperty = false;
	};
	
	/** @brief Describes a single field in a class/struct/interface. */
	struct HIRFieldDescriptor {
		std::string fieldName;
		semantic::TypeSharedPointer fieldType;
		int fieldIndex = -1;
		ast::AccessModifier fieldAccess = ast::AccessModifier::Default;
		bool isStaticField = false;
		bool isFinalField = false;
		bool isReadonlyField = false;
		HIRNodeSharedPointer defaultValueExpression;
	};
	
	/** @brief Describes a generic type parameter. */
	struct HIRGenericParameterDescriptor {
		std::string parameterName;
		std::vector<semantic::TypeSharedPointer> constraintTypes;
		semantic::TypeSharedPointer defaultType;
	};
	
	/** @brief Ordered sequence of statements with optional deferred cleanup actions. */
	struct HIRBlock : HIRNode {
		
		std::vector<HIRNodeSharedPointer> blockStatements;
		std::vector<HIRNodeSharedPointer> deferredCleanupActions;
		
		HIRBlock(
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Block, nullptr, sourceLocation ) {
		}
	
	};
	
	/** @brief Variable declaration with optional initializer. */
	struct HIRVariableBinding : HIRNode {
		
		std::string variableName;
		semantic::TypeSharedPointer variableType;
		HIRNodeSharedPointer initializerExpression;
		bool isMutableBinding = false;
		bool isConstantBinding = false;
		bool isHeapAllocated = false;
		
		HIRVariableBinding(
			const std::string& variableName,
			semantic::TypeSharedPointer variableType,
			HIRNodeSharedPointer initializerExpression,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::VariableBinding, variableType, sourceLocation ),
			variableName( variableName ),
			variableType( std::move( variableType ) ),
			initializerExpression( std::move( initializerExpression ) ) {
		}
	
	};
	
	/** @brief Assignment to an existing target (simple or compound). */
	struct HIRAssignment : HIRNode {
		
		token::Type assignmentOperator;
		HIRNodeSharedPointer targetExpression;
		HIRNodeSharedPointer valueExpression;
		
		HIRAssignment(
			token::Type assignmentOperator,
			HIRNodeSharedPointer targetExpression,
			HIRNodeSharedPointer valueExpression,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Assignment, nullptr, sourceLocation ),
			assignmentOperator( assignmentOperator ),
			targetExpression( std::move( targetExpression ) ),
			valueExpression( std::move( valueExpression ) ) {
		}
	
	};
	
	/** @brief Return statement with optional value expression. */
	struct HIRReturn : HIRNode {
		
		HIRNodeSharedPointer returnValueExpression;
		
		HIRReturn(
			HIRNodeSharedPointer returnValueExpression,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Return, nullptr, sourceLocation ),
			returnValueExpression( std::move( returnValueExpression ) ) {
		}
	
	};
	
	/** @brief Conditional branch; elif chains lowered to nested HIRIf in elseBranch. */
	struct HIRIf : HIRNode {
		
		HIRNodeSharedPointer branchCondition;
		std::shared_ptr<HIRBlock> thenBranch;
		HIRNodeSharedPointer elseBranch;
		
		HIRIf(
			HIRNodeSharedPointer branchCondition,
			std::shared_ptr<HIRBlock> thenBranch,
			HIRNodeSharedPointer elseBranch,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::If, nullptr, sourceLocation ),
			branchCondition( std::move( branchCondition ) ),
			thenBranch( std::move( thenBranch ) ),
			elseBranch( std::move( elseBranch ) ) {
		}
	
	};
	
	/** @brief Single arm of a match statement. */
	struct HIRMatchArm {
		HIRNodeSharedPointer armPattern;
		HIRNodeSharedPointer armGuard;
		std::shared_ptr<HIRBlock> armBody;
	};
	
	/** @brief Pattern match statement (preserved from AST, not desugared). */
	struct HIRMatch : HIRNode {
		
		HIRNodeSharedPointer matchSubject;
		std::vector<HIRMatchArm> matchArms;
		
		HIRMatch(
			HIRNodeSharedPointer matchSubject,
			std::vector<HIRMatchArm> matchArms,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Match, nullptr, sourceLocation ),
			matchSubject( std::move( matchSubject ) ),
			matchArms( std::move( matchArms ) ) {
		}
	
	};
	
	/** @brief Unified loop construct; all for/while/range loops lower to this. */
	struct HIRLoop : HIRNode {
		
		HIRNodeSharedPointer loopInitializer;
		HIRNodeSharedPointer loopCondition;
		std::shared_ptr<HIRBlock> loopBody;
		HIRNodeSharedPointer loopUpdate;
		std::string loopVariableName;
		std::string loopVariableName2;
		semantic::TypeSharedPointer loopVariableType;
		semantic::TypeSharedPointer loopVariableType2;
		bool isIteratorLoop = false;
		HIRNodeSharedPointer iterableExpression;
		
		HIRLoop(
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Loop, nullptr, sourceLocation ) {
		}
	
	};
	
	/** @brief Break statement (loop exit). */
	struct HIRBreak : HIRNode {
		
		HIRBreak(
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Break, nullptr, sourceLocation ) {
		}
	
	};
	
	/** @brief Continue statement (skip to next iteration). */
	struct HIRContinue : HIRNode {
		
		HIRContinue(
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Continue, nullptr, sourceLocation ) {
		}
	
	};
	
	/** @brief Deferred statement, executed at scope exit in LIFO order. */
	struct HIRDefer : HIRNode {
		
		HIRNodeSharedPointer deferredStatement;
		int scopeDepthAtRegistration = 0;
		
		HIRDefer(
			HIRNodeSharedPointer deferredStatement,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Defer, nullptr, sourceLocation ),
			deferredStatement( std::move( deferredStatement ) ) {
		}
	
	};
	
	/** @brief Explicit memory deallocation. */
	struct HIRDelete : HIRNode {
		
		HIRNodeSharedPointer targetExpression;
		
		HIRDelete(
			HIRNodeSharedPointer targetExpression,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Delete, nullptr, sourceLocation ),
			targetExpression( std::move( targetExpression ) ) {
		}
	
	};
	
	/** @brief Raise/throw an exception object. */
	struct HIRThrow : HIRNode {
		
		HIRNodeSharedPointer thrownExpression;
		
		HIRThrow(
			HIRNodeSharedPointer thrownExpression,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Throw, nullptr, sourceLocation ),
			thrownExpression( std::move( thrownExpression ) ) {
		}
	
	};
	
	/** @brief Single exception handler clause. */
	struct HIRExceptionHandler {
		std::string exceptionVariableName;
		std::vector<semantic::TypeSharedPointer> exceptionTypes;
		std::shared_ptr<HIRBlock> handlerBody;
		lookup::SourceSharedPointer sourceLocation;
	};
	
	/** @brief Try-catch-finally block. */
	struct HIRTryCatch : HIRNode {
		
		std::shared_ptr<HIRBlock> tryBody;
		std::vector<HIRExceptionHandler> exceptionHandlers;
		std::shared_ptr<HIRBlock> finallyBody;
		
		HIRTryCatch(
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::TryCatch, nullptr, sourceLocation ) {
		}
	
	};
	
	/** @brief Inline assembly operand (constraint + variable binding). */
	struct HIRAsmOperand {
		std::string constraintString;
		HIRNodeSharedPointer boundExpression;
	};
	
	/** @brief Inline assembly statement. */
	struct HIRInlineAssembly : HIRNode {
		
		bool isVolatile = false;
		std::string assemblyTemplate;
		std::vector<HIRAsmOperand> outputOperands;
		std::vector<HIRAsmOperand> inputOperands;
		std::vector<std::string> clobberRegisters;
		
		HIRInlineAssembly(
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::InlineAssembly, nullptr, sourceLocation ) {
		}
	
	};
	
	/** @brief Unsafe block bypassing safety checks. */
	struct HIRUnsafeBlock : HIRNode {
		
		std::shared_ptr<HIRBlock> unsafeBody;
		
		HIRUnsafeBlock(
			std::shared_ptr<HIRBlock> unsafeBody,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::UnsafeBlock, nullptr, sourceLocation ),
			unsafeBody( std::move( unsafeBody ) ) {
		}
	
	};
	
	/** @brief Expression used as a statement (discards result). */
	struct HIRExpressionStatement : HIRNode {
		
		HIRNodeSharedPointer expression;
		
		HIRExpressionStatement(
			HIRNodeSharedPointer expression,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::ExpressionStatement, nullptr, sourceLocation ),
			expression( std::move( expression ) ) {
		}
	
	};
	
	/** @brief No-op placeholder statement. */
	struct HIRPass : HIRNode {
		
		HIRPass(
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Pass, nullptr, sourceLocation ) {
		}
	
	};
	
	/** @brief Single case of a switch statement. */
	struct HIRSwitchCase {
		HIRNodeSharedPointer casePattern;
		std::shared_ptr<HIRBlock> caseBody;
		bool isDefaultCase = false;
	};
	
	/** @brief Switch statement (multi-way branch on a single expression). */
	struct HIRSwitch : HIRNode {
		
		HIRNodeSharedPointer switchSubject;
		std::vector<HIRSwitchCase> switchCases;
		
		HIRSwitch(
			HIRNodeSharedPointer switchSubject,
			std::vector<HIRSwitchCase> switchCases,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Switch, nullptr, sourceLocation ),
			switchSubject( std::move( switchSubject ) ),
			switchCases( std::move( switchCases ) ) {
		}
	
	};
	
	/** @brief Explicit RAII drop point for a variable. */
	struct HIRDrop : HIRNode {
		
		std::string droppedVariableName;
		
		HIRDrop(
			const std::string& droppedVariableName,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Drop, nullptr, sourceLocation ),
			droppedVariableName( droppedVariableName ) {
		}
	
	};
	
	/** @brief Integer literal expression. */
	struct HIRIntegerLiteral : HIRNode {
		
		int64_t integerValue;
		std::string rawRepresentation;
		
		HIRIntegerLiteral(
			int64_t integerValue,
			const std::string& rawRepresentation,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::IntegerLiteral, std::move( resolvedType ), sourceLocation ),
			integerValue( integerValue ),
			rawRepresentation( rawRepresentation ) {
		}
	
	};
	
	/** @brief Float literal expression. */
	struct HIRFloatLiteral : HIRNode {
		
		double floatValue;
		std::string rawRepresentation;
		
		HIRFloatLiteral(
			double floatValue,
			const std::string& rawRepresentation,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::FloatLiteral, std::move( resolvedType ), sourceLocation ),
			floatValue( floatValue ),
			rawRepresentation( rawRepresentation ) {
		}
	
	};
	
	/** @brief Boolean literal expression. */
	struct HIRBooleanLiteral : HIRNode {
		
		bool booleanValue;
		
		HIRBooleanLiteral(
			bool booleanValue,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::BooleanLiteral, std::move( resolvedType ), sourceLocation ),
			booleanValue( booleanValue ) {
		}
	
	};
	
	/** @brief String literal expression. */
	struct HIRStringLiteral : HIRNode {
		
		std::string stringValue;
		
		HIRStringLiteral(
			const std::string& stringValue,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::StringLiteral, std::move( resolvedType ), sourceLocation ),
			stringValue( stringValue ) {
		}
	
	};
	
	/** @brief Char literal expression. */
	struct HIRCharLiteral : HIRNode {
		
		char charValue;
		
		HIRCharLiteral(
			char charValue,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::CharLiteral, std::move( resolvedType ), sourceLocation ),
			charValue( charValue ) {
		}
	
	};
	
	/** @brief None/null literal expression. */
	struct HIRNoneLiteral : HIRNode {
		
		HIRNoneLiteral(
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::NoneLiteral, std::move( resolvedType ), sourceLocation ) {
		}
	
	};
	
	/** @brief Name reference expression. */
	struct HIRIdentifier : HIRNode {
		
		std::string identifierName;
		
		HIRIdentifier(
			const std::string& identifierName,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Identifier, std::move( resolvedType ), sourceLocation ),
			identifierName( identifierName ) {
		}
	
	};
	
	/** @brief Self/this reference expression. */
	struct HIRSelfReference : HIRNode {
		
		HIRSelfReference(
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::SelfReference, std::move( resolvedType ), sourceLocation ) {
		}
	
	};
	
	/** @brief Super/parent reference expression. */
	struct HIRSuperReference : HIRNode {
		
		HIRSuperReference(
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::SuperReference, std::move( resolvedType ), sourceLocation ) {
		}
	
	};
	
	/** @brief Binary operation expression (arithmetic, comparison, logical, bitwise). */
	struct HIRBinaryOperation : HIRNode {
		
		token::Type operatorKind;
		HIRNodeSharedPointer leftOperand;
		HIRNodeSharedPointer rightOperand;
		
		HIRBinaryOperation(
			token::Type operatorKind,
			HIRNodeSharedPointer leftOperand,
			HIRNodeSharedPointer rightOperand,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::BinaryOperation, std::move( resolvedType ), sourceLocation ),
			operatorKind( operatorKind ),
			leftOperand( std::move( leftOperand ) ),
			rightOperand( std::move( rightOperand ) ) {
		}
	
	};
	
	/** @brief Unary operation expression (prefix or postfix). */
	struct HIRUnaryOperation : HIRNode {
		
		token::Type operatorKind;
		HIRNodeSharedPointer operandExpression;
		bool isPrefixOperator;
		
		HIRUnaryOperation(
			token::Type operatorKind,
			HIRNodeSharedPointer operandExpression,
			bool isPrefixOperator,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::UnaryOperation, std::move( resolvedType ), sourceLocation ),
			operatorKind( operatorKind ),
			operandExpression( std::move( operandExpression ) ),
			isPrefixOperator( isPrefixOperator ) {
		}
	
	};
	
	/** @brief Direct function call expression. */
	struct HIRFunctionCall : HIRNode {
		
		HIRNodeSharedPointer calleeExpression;
		std::vector<HIRNodeSharedPointer> callArguments;
		std::vector<std::pair<std::string, HIRNodeSharedPointer>> keywordArguments;
		std::vector<semantic::TypeSharedPointer> typeArguments;
		
		HIRFunctionCall(
			HIRNodeSharedPointer calleeExpression,
			std::vector<HIRNodeSharedPointer> callArguments,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::FunctionCall, std::move( resolvedType ), sourceLocation ),
			calleeExpression( std::move( calleeExpression ) ),
			callArguments( std::move( callArguments ) ) {
		}
	
	};
	
	/** @brief Method call on a receiver object. */
	struct HIRMethodCall : HIRNode {
		
		HIRNodeSharedPointer receiverObject;
		std::string methodName;
		std::vector<HIRNodeSharedPointer> callArguments;
		std::vector<std::pair<std::string, HIRNodeSharedPointer>> keywordArguments;
		std::vector<semantic::TypeSharedPointer> typeArguments;
		
		HIRMethodCall(
			HIRNodeSharedPointer receiverObject,
			const std::string& methodName,
			std::vector<HIRNodeSharedPointer> callArguments,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::MethodCall, std::move( resolvedType ), sourceLocation ),
			receiverObject( std::move( receiverObject ) ),
			methodName( methodName ),
			callArguments( std::move( callArguments ) ) {
		}
	
	};
	
	/** @brief Field/member access expression (object.member). */
	struct HIRFieldAccess : HIRNode {
		
		HIRNodeSharedPointer objectExpression;
		std::string fieldName;
		int fieldLayoutIndex = -1;
		
		HIRFieldAccess(
			HIRNodeSharedPointer objectExpression,
			const std::string& fieldName,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::FieldAccess, std::move( resolvedType ), sourceLocation ),
			objectExpression( std::move( objectExpression ) ),
			fieldName( fieldName ) {
		}
	
	};
	
	/** @brief Array/dictionary index access expression (object[index]). */
	struct HIRIndexAccess : HIRNode {
		
		HIRNodeSharedPointer objectExpression;
		HIRNodeSharedPointer indexExpression;
		
		HIRIndexAccess(
			HIRNodeSharedPointer objectExpression,
			HIRNodeSharedPointer indexExpression,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::IndexAccess, std::move( resolvedType ), sourceLocation ),
			objectExpression( std::move( objectExpression ) ),
			indexExpression( std::move( indexExpression ) ) {
		}
	
	};
	
	/** @brief Object construction expression (new ClassName(args)). */
	struct HIRConstruct : HIRNode {
		
		semantic::TypeSharedPointer constructedType;
		std::vector<std::pair<std::string, HIRNodeSharedPointer>> constructorFields;
		
		HIRConstruct(
			semantic::TypeSharedPointer constructedType,
			std::vector<std::pair<std::string, HIRNodeSharedPointer>> constructorFields,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Construct, constructedType, sourceLocation ),
			constructedType( std::move( constructedType ) ),
			constructorFields( std::move( constructorFields ) ) {
		}
	
	};
	
	/** @brief Type cast expression. */
	struct HIRCast : HIRNode {
		
		HIRNodeSharedPointer sourceExpression;
		semantic::TypeSharedPointer targetCastType;
		
		HIRCast(
			HIRNodeSharedPointer sourceExpression,
			semantic::TypeSharedPointer targetCastType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Cast, targetCastType, sourceLocation ),
			sourceExpression( std::move( sourceExpression ) ),
			targetCastType( std::move( targetCastType ) ) {
		}
	
	};
	
	/** @brief Take reference (&x) expression. */
	struct HIRReference : HIRNode {
		
		HIRNodeSharedPointer targetExpression;
		
		HIRReference(
			HIRNodeSharedPointer targetExpression,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Reference, std::move( resolvedType ), sourceLocation ),
			targetExpression( std::move( targetExpression ) ) {
		}
	
	};
	
	/** @brief Dereference (*ptr) expression. */
	struct HIRDereference : HIRNode {
		
		HIRNodeSharedPointer targetExpression;
		
		HIRDereference(
			HIRNodeSharedPointer targetExpression,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Dereference, std::move( resolvedType ), sourceLocation ),
			targetExpression( std::move( targetExpression ) ) {
		}
	
	};
	
	/** @brief Ownership transfer (move x) expression. */
	struct HIRMoveTransfer : HIRNode {
		
		HIRNodeSharedPointer movedExpression;
		
		HIRMoveTransfer(
			HIRNodeSharedPointer movedExpression,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::MoveTransfer, std::move( resolvedType ), sourceLocation ),
			movedExpression( std::move( movedExpression ) ) {
		}
	
	};
	
	/** @brief Address-of (addressof x) expression yielding I64. */
	struct HIRAddressOf : HIRNode {
		
		HIRNodeSharedPointer targetExpression;
		
		HIRAddressOf(
			HIRNodeSharedPointer targetExpression,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::AddressOf, std::move( resolvedType ), sourceLocation ),
			targetExpression( std::move( targetExpression ) ) {
		}
	
	};
	
	/** @brief Lambda/anonymous function expression. */
	struct HIRLambda : HIRNode {
		
		std::vector<HIRParameterDescriptor> parameterDescriptors;
		semantic::TypeSharedPointer returnTypeDescriptor;
		std::shared_ptr<HIRBlock> lambdaBody;
		bool isAsyncLambda = false;
		
		HIRLambda(
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Lambda, std::move( resolvedType ), sourceLocation ) {
		}
	
	};
	
	/** @brief Await expression for async task completion. */
	struct HIRAwait : HIRNode {
		
		HIRNodeSharedPointer awaitedExpression;
		
		HIRAwait(
			HIRNodeSharedPointer awaitedExpression,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Await, std::move( resolvedType ), sourceLocation ),
			awaitedExpression( std::move( awaitedExpression ) ) {
		}
	
	};
	
	/** @brief Yield expression for generators. */
	struct HIRYield : HIRNode {
		
		HIRNodeSharedPointer yieldedExpression;
		
		HIRYield(
			HIRNodeSharedPointer yieldedExpression,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Yield, std::move( resolvedType ), sourceLocation ),
			yieldedExpression( std::move( yieldedExpression ) ) {
		}
	
	};
	
	/** @brief Match-as-expression (inline match returning a value). */
	struct HIRMatchExpression : HIRNode {
		
		HIRNodeSharedPointer matchSubject;
		std::vector<HIRNodeSharedPointer> matchPatterns;
		std::vector<HIRNodeSharedPointer> matchValues;
		HIRNodeSharedPointer matchDefaultValue;
		
		HIRMatchExpression(
			HIRNodeSharedPointer matchSubject,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::MatchExpression, std::move( resolvedType ), sourceLocation ),
			matchSubject( std::move( matchSubject ) ) {
		}
	
	};
	
	/** @brief Array literal expression. */
	struct HIRArrayLiteral : HIRNode {
		
		std::vector<HIRNodeSharedPointer> elementExpressions;
		
		HIRArrayLiteral(
			std::vector<HIRNodeSharedPointer> elementExpressions,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::ArrayLiteral, std::move( resolvedType ), sourceLocation ),
			elementExpressions( std::move( elementExpressions ) ) {
		}
	
	};
	
	/** @brief Tuple literal expression. */
	struct HIRTupleLiteral : HIRNode {
		
		std::vector<HIRNodeSharedPointer> elementExpressions;
		
		HIRTupleLiteral(
			std::vector<HIRNodeSharedPointer> elementExpressions,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::TupleLiteral, std::move( resolvedType ), sourceLocation ),
			elementExpressions( std::move( elementExpressions ) ) {
		}
	
	};
	
	/** @brief Type reference used in expression context. */
	struct HIRTypeReference : HIRNode {
		
		std::string referencedTypeName;
		
		HIRTypeReference(
			const std::string& referencedTypeName,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::TypeReference, std::move( resolvedType ), sourceLocation ),
			referencedTypeName( referencedTypeName ) {
		}
	
	};
	
	/** @brief Instanceof type check expression. */
	struct HIRInstanceOf : HIRNode {
		
		HIRNodeSharedPointer checkedExpression;
		semantic::TypeSharedPointer checkedType;
		
		HIRInstanceOf(
			HIRNodeSharedPointer checkedExpression,
			semantic::TypeSharedPointer checkedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::InstanceOf, nullptr, sourceLocation ),
			checkedExpression( std::move( checkedExpression ) ),
			checkedType( std::move( checkedType ) ) {
		}
	
	};
	
	/** @brief Subclass-of type check expression. */
	struct HIRSubclassOf : HIRNode {
		
		semantic::TypeSharedPointer childType;
		semantic::TypeSharedPointer parentType;
		
		HIRSubclassOf(
			semantic::TypeSharedPointer childType,
			semantic::TypeSharedPointer parentType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::SubclassOf, nullptr, sourceLocation ),
			childType( std::move( childType ) ),
			parentType( std::move( parentType ) ) {
		}
	
	};
	
	/** @brief Range expression (start..end or start...end). */
	struct HIRRangeExpression : HIRNode {
		
		HIRNodeSharedPointer rangeStart;
		HIRNodeSharedPointer rangeEnd;
		bool isInclusive;
		
		HIRRangeExpression(
			HIRNodeSharedPointer rangeStart,
			HIRNodeSharedPointer rangeEnd,
			bool isInclusive,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::RangeExpression, std::move( resolvedType ), sourceLocation ),
			rangeStart( std::move( rangeStart ) ),
			rangeEnd( std::move( rangeEnd ) ),
			isInclusive( isInclusive ) {
		}
	
	};
	
	/** @brief List/set/map comprehension expression. */
	struct HIRComprehension : HIRNode {
		
		enum class ComprehensionKind { List, Set, Map };
		
		ComprehensionKind comprehensionKind = ComprehensionKind::List;
		HIRNodeSharedPointer bodyExpression;
		HIRNodeSharedPointer keyExpression;
		HIRNodeSharedPointer conditionExpression;
		HIRNodeSharedPointer iterableExpression;
		std::string iteratorVariableName;
		semantic::TypeSharedPointer iteratorVariableType;
		
		HIRComprehension(
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Comprehension, std::move( resolvedType ), sourceLocation ) {
		}
	
	};
	
	struct HIRMapLiteral : HIRNode {
		
		std::vector<std::pair<HIRNodeSharedPointer, HIRNodeSharedPointer>> entries;
		
		HIRMapLiteral(
			std::vector<std::pair<HIRNodeSharedPointer, HIRNodeSharedPointer>> entries,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::MapLiteral, std::move( resolvedType ), sourceLocation ),
			entries( std::move( entries ) ) {
		}
	
	};
	
	struct HIRSetLiteral : HIRNode {
		
		std::vector<HIRNodeSharedPointer> elementExpressions;
		
		HIRSetLiteral(
			std::vector<HIRNodeSharedPointer> elementExpressions,
			semantic::TypeSharedPointer resolvedType,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::SetLiteral, std::move( resolvedType ), sourceLocation ),
			elementExpressions( std::move( elementExpressions ) ) {
		}
	
	};
	
	/** @brief Function or method definition. */
	struct HIRFunctionDefinition : HIRNode {
		
		std::string functionName;
		std::string mangledName;
		std::string ownerClassName;
		ast::AccessModifier accessModifier = ast::AccessModifier::Default;
		std::vector<HIRParameterDescriptor> parameterDescriptors;
		semantic::TypeSharedPointer returnTypeDescriptor;
		std::shared_ptr<HIRBlock> functionBody;
		std::vector<HIRGenericParameterDescriptor> genericParameters;
		bool isVirtualMethod = false;
		bool isStaticMethod = false;
		bool isAsyncFunction = false;
		bool isOverrideMethod = false;
		bool isAbstractMethod = false;
		bool isFinalMethod = false;
		bool isConstMethod = false;
		bool isGeneratorFunction = false;
		bool isNativeMethod = false;
		bool isPropertyMethod = false;
		std::vector<semantic::TypeSharedPointer> raisesTypes;
		std::vector<std::shared_ptr<HIRFunctionDefinition>> nestedFunctionDefinitions;
		
		HIRFunctionDefinition(
			const std::string& functionName,
			semantic::TypeSharedPointer returnTypeDescriptor,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::FunctionDefinition, nullptr, sourceLocation ),
			functionName( functionName ),
			returnTypeDescriptor( std::move( returnTypeDescriptor ) ) {
		}
	
	};
	
	/** @brief Class definition with fields, methods, and inheritance. */
	struct HIRClassDefinition : HIRNode {
		
		std::string className;
		std::string classQualifiedName;
		std::string parentClassQualifiedName;
		ast::AccessModifier accessModifier = ast::AccessModifier::Default;
		std::vector<HIRFieldDescriptor> fieldDescriptors;
		std::vector<std::shared_ptr<HIRFunctionDefinition>> methodDefinitions;
		std::vector<std::string> implementedInterfaceQualifiedNames;
		std::vector<HIRGenericParameterDescriptor> genericParameters;
		std::vector<HIRNodeSharedPointer> nestedDeclarations;
		std::vector<std::string> usedTraitNames;
		bool isFinalClass = false;
		bool isAbstractClass = false;
		bool isReadonlyClass = false;
		bool isNativeClass = false;
		
		HIRClassDefinition(
			const std::string& className,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::ClassDefinition, nullptr, sourceLocation ),
			className( className ) {
		}
	
	};
	
	/** @brief Struct definition with fields and methods. */
	struct HIRStructDefinition : HIRNode {
		
		std::string structName;
		std::string structQualifiedName;
		ast::AccessModifier accessModifier = ast::AccessModifier::Default;
		std::vector<HIRFieldDescriptor> fieldDescriptors;
		std::vector<std::shared_ptr<HIRFunctionDefinition>> methodDefinitions;
		std::vector<HIRGenericParameterDescriptor> genericParameters;
		std::vector<HIRNodeSharedPointer> nestedDeclarations;
		std::vector<std::string> usedTraitNames;
		
		HIRStructDefinition(
			const std::string& structName,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::StructDefinition, nullptr, sourceLocation ),
			structName( structName ) {
		}
	
	};
	
	/** @brief Enum variant descriptor. */
	struct HIREnumVariantDescriptor {
		std::string variantName;
		HIRNodeSharedPointer backedValueExpression;
		HIRNodeSharedPointer discriminantExpression;
		std::vector<semantic::TypeSharedPointer> associatedTypes;
		std::vector<HIRFieldDescriptor> variantFields;
		std::vector<std::shared_ptr<HIRFunctionDefinition>> variantMethods;
	};
	
	/** @brief Enum definition with variants and optional backing type. */
	struct HIREnumDefinition : HIRNode {
		
		std::string enumName;
		std::string enumQualifiedName;
		ast::AccessModifier accessModifier = ast::AccessModifier::Default;
		semantic::TypeSharedPointer backedType;
		std::string parentClassQualifiedName;
		std::vector<HIREnumVariantDescriptor> variantDescriptors;
		std::vector<std::shared_ptr<HIRFunctionDefinition>> methodDefinitions;
		std::vector<HIRGenericParameterDescriptor> genericParameters;
		std::vector<std::string> implementedInterfaceQualifiedNames;
		
		HIREnumDefinition(
			const std::string& enumName,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::EnumDefinition, nullptr, sourceLocation ),
			enumName( enumName ) {
		}
	
	};
	
	/** @brief Interface definition with method signatures. */
	struct HIRInterfaceDefinition : HIRNode {
		
		std::string interfaceName;
		std::string interfaceQualifiedName;
		ast::AccessModifier accessModifier = ast::AccessModifier::Default;
		std::vector<std::shared_ptr<HIRFunctionDefinition>> methodDefinitions;
		std::vector<HIRGenericParameterDescriptor> genericParameters;
		std::vector<std::string> superInterfaceQualifiedNames;
		std::vector<HIRNodeSharedPointer> nestedDeclarations;
		bool isFinalInterface = false;
		
		HIRInterfaceDefinition(
			const std::string& interfaceName,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::InterfaceDefinition, nullptr, sourceLocation ),
			interfaceName( interfaceName ) {
		}
	
	};
	
	/** @brief External/FFI function declaration. */
	struct HIRExternFunctionDeclaration : HIRNode {
		
		std::string functionName;
		std::string linkageName;
		ast::AccessModifier accessModifier = ast::AccessModifier::Default;
		std::vector<HIRParameterDescriptor> parameterDescriptors;
		semantic::TypeSharedPointer returnTypeDescriptor;
		bool isVariadicFunction = false;
		
		HIRExternFunctionDeclaration(
			const std::string& functionName,
			const std::string& linkageName,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::ExternFunctionDeclaration, nullptr, sourceLocation ),
			functionName( functionName ),
			linkageName( linkageName ) {
		}
	
	};
	
	/** @brief Top-level global variable definition. */
	struct HIRGlobalVariableDefinition : HIRNode {
		
		std::string variableName;
		semantic::TypeSharedPointer variableType;
		HIRNodeSharedPointer initializerExpression;
		ast::AccessModifier accessModifier = ast::AccessModifier::Default;
		bool isGlobalVariable = false;
		
		HIRGlobalVariableDefinition(
			const std::string& variableName,
			semantic::TypeSharedPointer variableType,
			HIRNodeSharedPointer initializerExpression,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::GlobalVariableDefinition, variableType, sourceLocation ),
			variableName( variableName ),
			variableType( std::move( variableType ) ),
			initializerExpression( std::move( initializerExpression ) ) {
		}
	
	};
	
	/** @brief Constant definition (const keyword). */
	struct HIRConstantDefinition : HIRNode {
		
		std::string constantName;
		semantic::TypeSharedPointer constantType;
		HIRNodeSharedPointer initializerExpression;
		ast::AccessModifier accessModifier = ast::AccessModifier::Default;
		
		HIRConstantDefinition(
			const std::string& constantName,
			semantic::TypeSharedPointer constantType,
			HIRNodeSharedPointer initializerExpression,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::ConstantDefinition, constantType, sourceLocation ),
			constantName( constantName ),
			constantType( std::move( constantType ) ),
			initializerExpression( std::move( initializerExpression ) ) {
		}
	
	};
	
	/** @brief Root HIR node representing a compiled module. */
	struct HIRModule : HIRNode {
		
		std::string moduleName;
		std::string packageQualifiedName;
		std::vector<std::shared_ptr<HIRFunctionDefinition>> functionDefinitions;
		std::vector<std::shared_ptr<HIRClassDefinition>> classDefinitions;
		std::vector<std::shared_ptr<HIRStructDefinition>> structDefinitions;
		std::vector<std::shared_ptr<HIREnumDefinition>> enumDefinitions;
		std::vector<std::shared_ptr<HIRInterfaceDefinition>> interfaceDefinitions;
		std::vector<std::shared_ptr<HIRGlobalVariableDefinition>> globalVariableDefinitions;
		std::vector<std::shared_ptr<HIRConstantDefinition>> constantDefinitions;
		std::vector<std::shared_ptr<HIRExternFunctionDeclaration>> externFunctionDeclarations;
		
		HIRModule(
			const std::string& moduleName,
			const lookup::SourceSharedPointer& sourceLocation
		) : HIRNode( HIRNodeKind::Module, nullptr, sourceLocation ),
			moduleName( moduleName ) {
		}
	
	};

} // namespace uranite::ir::hir

#endif // end _URANITE_IR_HIR_HPP_
