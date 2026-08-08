
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


#include <algorithm>

#include "uranite/semantic/borrow.hpp"
#include "uranite/semantic/qualnames.hpp"
#include "uranite/semantic/typeref.hpp"

#include "fmt/format.h"

namespace uranite::semantic {
	
	BorrowChecker::BorrowChecker( diagnostic::Engine& diagnostic ) : diagnostic( diagnostic ) {
	}
	
	void BorrowChecker::borrowValue( const std::string& ownerName, const std::string& borrowerName, bool isMutable, const lookup::SourceSharedPointer& source ) {
		std::unordered_map<std::string,BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( ownerName );
		if( ownershipIterator == this->ownershipTracking.end() ) {
			return;
		}
		BorrowOwnershipInfo& ownershipInformation = ownershipIterator->second;
		if( ownershipInformation.state == BorrowOwnershipState::Moved ) {
			std::string moveErrorMessage = fmt::format( "cannot borrow moved value \"{}\"", ownerName );
			this->diagnostic.error( source, moveErrorMessage );
			return;
		}
		if( isMutable ) {
			if( ownershipInformation.hasMutableBorrow ) {
				std::string multipleMutableErrorMessage = fmt::format( "cannot borrow \"{}\" as mutable more than once", ownerName );
				this->diagnostic.error( source, multipleMutableErrorMessage, "only one mutable borrow is allowed at a time" );
				return;
			}
			if( ownershipInformation.immutableBorrowCount > 0 ) {
				std::string conflictMutableErrorMessage = fmt::format( "cannot borrow \"{}\" as mutable while immutable borrows exist", ownerName );
				this->diagnostic.error( source, conflictMutableErrorMessage, "release immutable borrows first" );
				return;
			}
			if( ownershipInformation.isMutable == false ) {
				std::string immutableVariableErrorMessage = fmt::format( "cannot borrow immutable variable \"{}\" as mutable", ownerName );
				this->diagnostic.error( source, immutableVariableErrorMessage, "declare with \"mut\" to allow mutable borrows" );
				return;
			}
			ownershipInformation.hasMutableBorrow = true;
		}
		else {
			if( ownershipInformation.hasMutableBorrow ) {
				std::string conflictImmutableErrorMessage = fmt::format( "cannot borrow \"{}\" as immutable while a mutable borrow exists", ownerName );
				this->diagnostic.error( source, conflictImmutableErrorMessage );
				return;
			}
			ownershipInformation.immutableBorrowCount++;
		}
		BorrowInfo borrowRecord;
		borrowRecord.borrowerName = borrowerName;
		borrowRecord.isMutable = isMutable;
		borrowRecord.ownerName = ownerName;
		borrowRecord.scopeDepth = this->currentScopeDepth;
		borrowRecord.source = source;
		ownershipInformation.activeBorrows.push_back( borrowRecord );
		if( isMutable ) {
			ownershipInformation.state = BorrowOwnershipState::MutableBorrowed;
		}
		else {
			ownershipInformation.state = BorrowOwnershipState::Borrowed;
		}
	}
	
	void BorrowChecker::check( ast::nodes::Program& program ) {
		this->pushScope();
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			this->checkDeclaration( declaration );
		}
		this->popScope();
	}
	
	void BorrowChecker::checkAssignmentStatement( ast::nodes::AssignStatement& statement ) {
		this->checkExpression( statement.value );
		if( statement.target->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *statement.target );
			std::unordered_map<std::string,BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( identifierExpression.name );
			if( ownershipIterator != this->ownershipTracking.end() ) {
				BorrowOwnershipInfo& ownershipInformation = ownershipIterator->second;
				if( ownershipInformation.isMutable == false ) {
					std::string immutableAssignmentErrorMessage = fmt::format( "cannot assign to immutable variable \"{}\"", identifierExpression.name );
					this->diagnostic.error( statement.source, immutableAssignmentErrorMessage );
				}
				this->checkBorrowConflict( identifierExpression.name, true, statement.source );
			}
		}
	}
	
	void BorrowChecker::checkBlock( const std::vector<ast::nodes::StatementSharedPointer>& statements ) {
		this->pushScope();
		for( const ast::nodes::StatementSharedPointer& statement : statements ) {
			this->checkStatement( statement );
		}
		this->popScope();
	}
	
	void BorrowChecker::checkBorrowConflict( const std::string& variableName, bool isMutable, const lookup::SourceSharedPointer& source ) {
		std::unordered_map<std::string,BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( variableName );
		if( ownershipIterator == this->ownershipTracking.end() ) {
			return;
		}
		BorrowOwnershipInfo& ownershipInformation = ownershipIterator->second;
		if( isMutable && ownershipInformation.immutableBorrowCount > 0 ) {
			std::string mutableConflictErrorMessage = fmt::format( "cannot use \"{}\" mutably while immutable borrows exist", variableName );
			this->diagnostic.error( source, mutableConflictErrorMessage );
		}
	}
	
	void BorrowChecker::checkDeclaration( const ast::nodes::DeclarationSharedPointer& declaration ) {
		if( declaration == nullptr ) {
			return;
		}
		if( declaration->kind == ast::Node::Kind::FunctionDeclaration) {
			this->checkFunctionDeclaration(static_cast<ast::nodes::FunctionDeclaration&>( *declaration ) );
		}
	}
	
	void BorrowChecker::checkExpression( const ast::nodes::ExpressionSharedPointer& expression, bool transferOwnership ) {
		if( expression == nullptr ) {
			return;
		}
		switch( expression->kind ) {
			case ast::Node::Kind::BinaryExpression: {
				ast::nodes::BinaryExpression& binaryExpression = static_cast<ast::nodes::BinaryExpression&>( *expression );
				this->checkExpression( binaryExpression.left );
				this->checkExpression( binaryExpression.right );
				break;
			}
			case ast::Node::Kind::CallExpression: {
				ast::nodes::CallExpression& callExpression = static_cast<ast::nodes::CallExpression&>( *expression );
				this->checkExpression( callExpression.callee );
				for( ast::nodes::ExpressionSharedPointer& argument : callExpression.arguments ) {
					this->checkExpression( argument );
					if( argument->kind == ast::Node::Kind::IdentifierExpression ) {
						ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *argument );
						std::unordered_map<std::string,BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( identifierExpression.name );
						if( ownershipIterator != this->ownershipTracking.end() && ownershipIterator->second.state == BorrowOwnershipState::Owned ) {
							if( argument->semanticType && argument->semanticType->isPrimitive() == false &&
								argument->semanticType->kind != Type::Kind::Enum &&
								argument->semanticType->kind != Type::Kind::Class &&
								argument->semanticType->kind != Type::Kind::Interface &&
								argument->semanticType->kind != Type::Kind::Struct ) {
								this->moveValue( identifierExpression.name, argument->source );
							}
						}
					}
					if( argument->kind == ast::Node::Kind::UnaryExpression ) {
						ast::nodes::UnaryExpression& unaryExpression = static_cast<ast::nodes::UnaryExpression&>( *argument );
						if( unaryExpression.operation == token::Type::Ampersand && unaryExpression.operand && unaryExpression.operand->kind == ast::Node::Kind::IdentifierExpression ) {
							ast::nodes::IdentifierExpression& operandIdentifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *unaryExpression.operand );
							this->borrowValue( operandIdentifierExpression.name, "<call>", false, argument->source );
						}
					}
				}
				for( ast::nodes::KeywordArgument& kwarg : callExpression.keywordArguments ) {
					this->checkExpression( kwarg.value );
				}
				break;
			}
			case ast::Node::Kind::IdentifierExpression: {
				ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression& >( *expression );
				this->checkUseAfterMove( identifierExpression.name, identifierExpression.source );
				break;
			}
			case ast::Node::Kind::IndexExpression: {
				ast::nodes::IndexExpression& indexExpression = static_cast<ast::nodes::IndexExpression&>( *expression );
				this->checkExpression( indexExpression.object );
				this->checkExpression( indexExpression.index );
				break;
			}
			case ast::Node::Kind::MemberAccessExpression: {
				ast::nodes::MemberAccessExpression& memberAccessExpression = static_cast<ast::nodes::MemberAccessExpression& >( *expression );
				this->checkExpression( memberAccessExpression.object );
				break;
			}
			case ast::Node::Kind::MethodCallExpression: {
				ast::nodes::MethodCallExpression& methodCallExpression = static_cast<ast::nodes::MethodCallExpression& >( *expression );
				this->checkExpression( methodCallExpression.object );
				for( ast::nodes::ExpressionSharedPointer& argument : methodCallExpression.arguments ) {
					this->checkExpression( argument );
					if( argument->kind == ast::Node::Kind::IdentifierExpression ) {
						ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *argument );
						std::unordered_map<std::string, BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( identifierExpression.name );
						if( ownershipIterator != this->ownershipTracking.end() && ownershipIterator->second.state == BorrowOwnershipState::Owned ) {
							if( argument->semanticType && argument->semanticType->isPrimitive() == false &&
								argument->semanticType->kind != Type::Kind::Enum &&
								argument->semanticType->kind != Type::Kind::Class &&
								argument->semanticType->kind != Type::Kind::Interface &&
								argument->semanticType->kind != Type::Kind::Struct ) {
								this->moveValue( identifierExpression.name, argument->source );
							}
						}
					}
				}
				for( ast::nodes::KeywordArgument& kwarg : methodCallExpression.keywordArguments ) {
					this->checkExpression( kwarg.value );
				}
				break;
			}
			case ast::Node::Kind::UnaryExpression: {
				ast::nodes::UnaryExpression& unaryExpression = static_cast<ast::nodes::UnaryExpression&>( *expression );
				if( unaryExpression.operation == token::Type::Ampersand ) {
					if( unaryExpression.operand && unaryExpression.operand->kind == ast::Node::Kind::IdentifierExpression ) {
						ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *unaryExpression.operand );
						this->borrowValue( identifierExpression.name, "<temp>", false, unaryExpression.source );
					}
				}
				else if( unaryExpression.operation == token::Type::KeywordMove ) {
					if( unaryExpression.operand && unaryExpression.operand->kind == ast::Node::Kind::IdentifierExpression ) {
						ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *unaryExpression.operand );
						this->moveValue( identifierExpression.name, unaryExpression.source );
					}
				}
				else if( unaryExpression.operation == token::Type::Star ) {
					if( unaryExpression.operand && this->isInsideUnsafeBlock == false ) {
					}
					this->checkExpression( unaryExpression.operand );
				}
				else {
					this->checkExpression( unaryExpression.operand );
				}
				break;
			}
			default: {
				break;
			}
		}
	}
	
	void BorrowChecker::checkForStatement( ast::nodes::ForStatement& statement ) {
		if( statement.iterable ) {
			this->checkExpression( statement.iterable );
		}
		bool previousLoopStatus = this->isInsideLoop;
		this->isInsideLoop = true;
		this->pushScope();
		bool isCStyleForControl = ( statement.initializer != nullptr || statement.update != nullptr );
		this->declareOwnership( statement.variable, isCStyleForControl, statement.source );
		if( statement.variable2.empty() == false ) {
			this->declareOwnership( statement.variable2, false, statement.source );
		}
		if( statement.initializer ) {
			this->checkExpression( statement.initializer );
		}
		if( statement.condition ) {
			this->checkExpression( statement.condition );
		}
		if( statement.update ) {
			this->checkStatement( statement.update );
		}
		for( ast::nodes::StatementSharedPointer& bodyStatement : statement.body ) {
			this->checkStatement( bodyStatement );
		}
		if( this->variableScope.empty() == false ) {
			this->checkLoopIterationCleanup( this->variableScope.back(), statement.source );
		}
		this->popScope();
		this->isInsideLoop = previousLoopStatus;
	}
	
	void BorrowChecker::checkFunctionDeclaration( ast::nodes::FunctionDeclaration& functionDeclaration ) {
		this->pushScope();
		for( ast::nodes::FunctionParameterSharedPointer& functionParameter : functionDeclaration.parameters ) {
			if( functionParameter->isSelf ) {
				continue;
			}
			this->declareOwnership( functionParameter->name, functionParameter->isMutable, functionParameter->source );
		}
		this->checkBlock( functionDeclaration.body );
		this->popScope();
	}
	
	void BorrowChecker::checkIfStatement( ast::nodes::IfStatement& statement ) {
		this->checkExpression( statement.condition );
		std::unordered_map<std::string, BorrowOwnershipState> preIfSnapshot = this->snapshotOwnershipStates();
		std::vector<std::unordered_map<std::string, BorrowOwnershipState>> branchEndSnapshots;
		this->checkBlock( statement.thenBody );
		branchEndSnapshots.push_back( this->snapshotOwnershipStates() );
		this->restoreOwnershipStates( preIfSnapshot );
		for( std::pair<ast::nodes::ExpressionSharedPointer, std::vector<ast::nodes::StatementSharedPointer>>& elifBranch : statement.elifBranches ) {
			this->checkExpression( elifBranch.first );
			this->checkBlock( elifBranch.second );
			branchEndSnapshots.push_back( this->snapshotOwnershipStates() );
			this->restoreOwnershipStates( preIfSnapshot );
		}
		if( statement.elseBody.empty() == false ) {
			this->checkBlock( statement.elseBody );
			branchEndSnapshots.push_back( this->snapshotOwnershipStates() );
			this->restoreOwnershipStates( preIfSnapshot );
		}
		else {
			branchEndSnapshots.push_back( preIfSnapshot );
		}
		this->mergeOwnershipStates( branchEndSnapshots, statement.source );
	}
	
	void BorrowChecker::checkLoopIterationCleanup( const std::vector<std::string>& scopeVariables, const lookup::SourceSharedPointer& source ) {
		for( const std::string& variableName : scopeVariables ) {
			std::unordered_map<std::string, BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( variableName );
			if( ownershipIterator == this->ownershipTracking.end() ) {
				continue;
			}
			BorrowOwnershipInfo& ownershipInformation = ownershipIterator->second;
			if( ownershipInformation.isHeapAllocated && ownershipInformation.isArenaManaged == false &&
				ownershipInformation.state != BorrowOwnershipState::Dropped &&
				ownershipInformation.state != BorrowOwnershipState::Moved ) {
				std::string heapWarningMessage = fmt::format( "heap-allocated value \"{}\" is not freed before next loop iteration; add \"delete {}\" before end of loop body", variableName, variableName );
				this->diagnostic.warning( ownershipInformation.declarationSource, heapWarningMessage );
			}
		}
	}
	
	void BorrowChecker::checkReturnStatement( ast::nodes::ReturnStatement& statement ) {
		if( statement.value ) {
			this->checkExpression( statement.value );
			if( statement.value->kind == ast::Node::Kind::UnaryExpression ) {
				ast::nodes::UnaryExpression& unaryExpression = static_cast<ast::nodes::UnaryExpression&>( *statement.value );
				if( unaryExpression.operation == token::Type::Ampersand && unaryExpression.operand &&
					unaryExpression.operand->kind == ast::Node::Kind::IdentifierExpression ) {
					ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *unaryExpression.operand );
					std::unordered_map<std::string, BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( identifierExpression.name );
					if( ownershipIterator != this->ownershipTracking.end() && ownershipIterator->second.scopeDepth > 1 ) {
						std::string localReferenceErrorMessage = fmt::format( "cannot return reference to local variable \"{}\"", identifierExpression.name );
						this->diagnostic.error( statement.source, localReferenceErrorMessage, "the variable will be dropped when the function returns" );
					}
				}
			}
		}
	}
	
	void BorrowChecker::checkStatement( const ast::nodes::StatementSharedPointer& statement ) {
		if( statement == nullptr ) {
			return;
		}
		switch( statement->kind ) {
			case ast::Node::Kind::AssignmentStatement: {
				this->checkAssignmentStatement( static_cast<ast::nodes::AssignStatement&>( *statement ) );
				break;
			}
			case ast::Node::Kind::DeleteStatement: {
				ast::nodes::DeleteStatement& deleteStatement = static_cast<ast::nodes::DeleteStatement&>( *statement );
				if( deleteStatement.expression->kind == ast::Node::Kind::IdentifierExpression ) {
					ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *deleteStatement.expression );
					std::unordered_map<std::string, BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( identifierExpression.name );
					if( ownershipIterator != this->ownershipTracking.end() ) {
						BorrowOwnershipInfo& ownershipInformation = ownershipIterator->second;
						if( ownershipInformation.state == BorrowOwnershipState::Dropped ) {
							std::string doubleFreeErrorMessage = fmt::format( "double free: \"{}\" was already deleted", identifierExpression.name );
							this->diagnostic.error( deleteStatement.source, doubleFreeErrorMessage );
						}
						else if( ownershipInformation.state == BorrowOwnershipState::Moved ) {
							std::string movedDeleteErrorMessage = fmt::format( "cannot delete moved value \"{}\"", identifierExpression.name );
							this->diagnostic.error( deleteStatement.source, movedDeleteErrorMessage );
						}
						else if( ownershipInformation.activeBorrows.empty() == false ) {
							std::string activeBorrowDeleteErrorMessage = fmt::format( "cannot delete \"{}\" while it is borrowed", identifierExpression.name );
							this->diagnostic.error( deleteStatement.source, activeBorrowDeleteErrorMessage );
						}
						else {
							this->dropValue( identifierExpression.name );
						}
					}
				}
				else {
					this->checkExpression( deleteStatement.expression );
				}
				break;
			}
			case ast::Node::Kind::ExpressionStatement: {
				this->checkExpression( static_cast<ast::nodes::ExpressionStatement&>( *statement ).expression );
				break;
			}
			case ast::Node::Kind::ForStatement: {
				this->checkForStatement( static_cast<ast::nodes::ForStatement&>( *statement ) );
				break;
			}
			case ast::Node::Kind::IfStatement: {
				this->checkIfStatement( static_cast<ast::nodes::IfStatement&>( *statement ) );
				break;
			}
			case ast::Node::Kind::ReturnStatement: {
				this->checkReturnStatement( static_cast<ast::nodes::ReturnStatement&>( *statement ) );
				break;
			}
			case ast::Node::Kind::ThrowStatement: {
				this->checkExpression( static_cast<ast::nodes::ThrowStatement&>( *statement ).expression );
				break;
			}
			case ast::Node::Kind::TryCatchStatement: {
				ast::nodes::TryCatchStatement& tryCatchStatement = static_cast<ast::nodes::TryCatchStatement&>( *statement );
				std::unordered_map<std::string, BorrowOwnershipState> preTrySnapshot = this->snapshotOwnershipStates();
				std::vector<std::unordered_map<std::string, BorrowOwnershipState>> tryCatchBranchSnapshots;
				this->pushScope();
				for( const ast::nodes::StatementSharedPointer& tryBodyStatement : tryCatchStatement.tryBody ) {
					this->checkStatement( tryBodyStatement );
				}
				if( this->variableScope.empty() == false ) {
					for( const std::string& tryScopeVariable : this->variableScope.back() ) {
						std::unordered_map<std::string, BorrowOwnershipInfo>::iterator heapCheckIterator = this->ownershipTracking.find( tryScopeVariable );
						if( heapCheckIterator != this->ownershipTracking.end() &&
							heapCheckIterator->second.isHeapAllocated &&
							heapCheckIterator->second.isArenaManaged == false &&
							heapCheckIterator->second.state != BorrowOwnershipState::Dropped &&
							heapCheckIterator->second.state != BorrowOwnershipState::Moved ) {
							std::string heapLeakWarningMessage = fmt::format(
								"heap-allocated \"{}\" in try block may leak if exception is thrown; add \"delete {}\" before potential throw sites",
								tryScopeVariable, tryScopeVariable
							);
							this->diagnostic.warning( heapCheckIterator->second.declarationSource, heapLeakWarningMessage );
						}
					}
				}
				this->popScope();
				std::unordered_map<std::string, BorrowOwnershipState> postTrySnapshot = this->snapshotOwnershipStates();
				tryCatchBranchSnapshots.push_back( postTrySnapshot );
				std::vector<std::unordered_map<std::string, BorrowOwnershipState>> pessimisticBasis;
				pessimisticBasis.push_back( preTrySnapshot );
				pessimisticBasis.push_back( postTrySnapshot );
				this->restoreOwnershipStates( preTrySnapshot );
				this->mergeOwnershipStates( pessimisticBasis, statement->source );
				for( ast::nodes::ExceptionClause& exceptClause : tryCatchStatement.exceptionClauses ) {
					std::unordered_map<std::string, BorrowOwnershipState> preExceptSnapshot = this->snapshotOwnershipStates();
					this->checkBlock( exceptClause.body );
					tryCatchBranchSnapshots.push_back( this->snapshotOwnershipStates() );
					this->restoreOwnershipStates( preExceptSnapshot );
				}
				this->mergeOwnershipStates( tryCatchBranchSnapshots, statement->source );
				if( tryCatchStatement.finallyBody.empty() == false ) {
					this->checkBlock( tryCatchStatement.finallyBody );
				}
				break;
			}
			case ast::Node::Kind::UnsafeBlock: {
				this->checkUnsafeBlockStatement( static_cast<ast::nodes::UnsafeBlockStatement&>( *statement ) );
				break;
			}
			case ast::Node::Kind::VariableStatement: {
				this->checkVarStatement( static_cast<ast::nodes::VariableStatement&>( *statement ) );
				break;
			}
			case ast::Node::Kind::WhileStatement: {
				this->checkWhileStatement( static_cast<ast::nodes::WhileStatement&>( *statement ) );
				break;
			}
			default: {
				break;
			}
		}
	}
	
	void BorrowChecker::checkUnsafeBlockStatement( ast::nodes::UnsafeBlockStatement& statement ) {
		bool previousUnsafeStatus = this->isInsideUnsafeBlock;
		this->isInsideUnsafeBlock = true;
		this->checkBlock( statement.body );
		this->isInsideUnsafeBlock = previousUnsafeStatus;
	}
	
	void BorrowChecker::checkUseAfterMove( const std::string& variableName, const lookup::SourceSharedPointer& source ) {
		std::unordered_map<std::string,BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( variableName );
		if( ownershipIterator == this->ownershipTracking.end() ) {
			return;
		}
		BorrowOwnershipInfo& ownershipInformation = ownershipIterator->second;
		if( ownershipInformation.state == BorrowOwnershipState::Moved ) {
			std::string moveUsageErrorMessage = fmt::format( "use of moved value \"{}\"", variableName );
			this->diagnostic.error( source, moveUsageErrorMessage );
		}
		if( ownershipInformation.state == BorrowOwnershipState::Dropped ) {
			std::string dropUsageErrorMessage = fmt::format( "use of dropped value \"{}\"", variableName );
			this->diagnostic.error( source, dropUsageErrorMessage );
		}
	}
	
	void BorrowChecker::checkVarStatement( ast::nodes::VariableStatement& statement ) {
		if( statement.initializer ) {
			this->checkExpression( statement.initializer );
			if( statement.initializer->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *statement.initializer );
				std::unordered_map<std::string,BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( identifierExpression.name );
				if( ownershipIterator != this->ownershipTracking.end() ) {
					bool shouldMoveValue = true;
					if( statement.initializer->semanticType && statement.initializer->semanticType->isPrimitive() ) {
						shouldMoveValue = false;
					}
					if( statement.initializer->semanticType &&
						statement.initializer->semanticType->kind == Type::Kind::Enum ) {
						shouldMoveValue = false;
					}
					if( statement.initializer->semanticType &&
						statement.initializer->semanticType->kind == Type::Kind::Class ) {
						shouldMoveValue = false;
					}
					if( statement.initializer->semanticType &&
						statement.initializer->semanticType->kind == Type::Kind::Struct ) {
						shouldMoveValue = false;
					}
					if( shouldMoveValue ) {
						this->moveValue( identifierExpression.name, statement.source );
					}
				}
			}
		}
		this->declareOwnership( statement.name, statement.isMutable, statement.source );
		std::string constructorName;
		if( statement.initializer ) {
			std::unordered_map<std::string,BorrowOwnershipInfo>::iterator variableIterator = this->ownershipTracking.find( statement.name );
			if( variableIterator != this->ownershipTracking.end() ) {
				variableIterator->second.astNode = &statement;
				if( statement.initializer->kind == ast::Node::Kind::ConstructExpression ) {
					ast::nodes::ConstructExpression& constructorExpression = static_cast<ast::nodes::ConstructExpression&>( *statement.initializer );
					if( constructorExpression.type && constructorExpression.type->kind == ast::Node::Kind::GenericType ) {
						constructorName = static_cast<ast::nodes::GenericTypeNode&>( *constructorExpression.type ).name;
					}
					else if( constructorExpression.type && constructorExpression.type->kind == ast::Node::Kind::SimpleType ) {
						constructorName = static_cast<ast::nodes::SimpleTypeNode&>( *constructorExpression.type ).name;
					}
					if( constructorName == semantic::qualname::classes::arena::Name ) {
					}
					else {
						variableIterator->second.isHeapAllocated = true;
					}
				}
				if( statement.initializer->kind == ast::Node::Kind::MethodCallExpression ) {
					ast::nodes::MethodCallExpression& methodCallExpression = static_cast<ast::nodes::MethodCallExpression&>( *statement.initializer );
					if( methodCallExpression.method == qualname::classes::arena::methods::Alloc && methodCallExpression.object &&
						methodCallExpression.object->kind == ast::Node::Kind::IdentifierExpression ) {
						ast::nodes::IdentifierExpression& arenaIdentifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *methodCallExpression.object );
						std::unordered_map<std::string,BorrowOwnershipInfo>::iterator arenaIterator = this->ownershipTracking.find( arenaIdentifierExpression.name );
						if( arenaIterator != this->ownershipTracking.end() ) {
							variableIterator->second.isArenaManaged = true;
							variableIterator->second.arenaOwnerName = arenaIdentifierExpression.name;
						}
					}
				}
				ast::nodes::OwnershipAnnotation annotation;
				annotation.isHeapAllocated = variableIterator->second.isHeapAllocated;
				annotation.isArenaManaged = variableIterator->second.isArenaManaged;
				annotation.typeName = constructorName;
				statement.ownershipAnnotation = annotation;
			}
		}
	}
	
	void BorrowChecker::checkWhileStatement( ast::nodes::WhileStatement& statement ) {
		this->checkExpression( statement.condition );
		bool previousLoopStatus = this->isInsideLoop;
		this->isInsideLoop = true;
		this->pushScope();
		for( ast::nodes::StatementSharedPointer& bodyStatement : statement.body ) {
			this->checkStatement( bodyStatement );
		}
		if( this->variableScope.empty() == false ) {
			this->checkLoopIterationCleanup( this->variableScope.back(), statement.source );
		}
		this->popScope();
		this->isInsideLoop = previousLoopStatus;
	}
	
	std::unordered_map<std::string, BorrowOwnershipState> BorrowChecker::snapshotOwnershipStates() {
		std::unordered_map<std::string, BorrowOwnershipState> snapshot;
		for( const std::pair<const std::string, BorrowOwnershipInfo>& ownershipEntry : this->ownershipTracking ) {
			snapshot[ownershipEntry.first] = ownershipEntry.second.state;
		}
		return snapshot;
	}
	
	void BorrowChecker::restoreOwnershipStates( const std::unordered_map<std::string, BorrowOwnershipState>& snapshot ) {
		for( const std::pair<const std::string, BorrowOwnershipState>& savedEntry : snapshot ) {
			std::unordered_map<std::string, BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( savedEntry.first );
			if( ownershipIterator != this->ownershipTracking.end() ) {
				ownershipIterator->second.state = savedEntry.second;
			}
		}
	}
	
	void BorrowChecker::mergeOwnershipStates(
		const std::vector<std::unordered_map<std::string, BorrowOwnershipState>>& branchSnapshots,
		const lookup::SourceSharedPointer& mergeSource
	) {
		if( branchSnapshots.empty() ) {
			return;
		}
		std::unordered_map<std::string, BorrowOwnershipState> mergedStates;
		for( const std::unordered_map<std::string, BorrowOwnershipState>& branchSnapshot : branchSnapshots ) {
			for( const std::pair<const std::string, BorrowOwnershipState>& variableEntry : branchSnapshot ) {
				std::unordered_map<std::string, BorrowOwnershipState>::iterator mergedIterator = mergedStates.find( variableEntry.first );
				if( mergedIterator == mergedStates.end() ) {
					mergedStates[variableEntry.first] = variableEntry.second;
				}
				else {
					BorrowOwnershipState existingState = mergedIterator->second;
					BorrowOwnershipState branchState = variableEntry.second;
					if( existingState == branchState ) {
						continue;
					}
					if( branchState == BorrowOwnershipState::Dropped || existingState == BorrowOwnershipState::Dropped ) {
						mergedIterator->second = BorrowOwnershipState::Dropped;
					}
					else if( branchState == BorrowOwnershipState::Moved || existingState == BorrowOwnershipState::Moved ) {
						mergedIterator->second = BorrowOwnershipState::Moved;
					}
				}
			}
		}
		for( const std::pair<const std::string, BorrowOwnershipState>& mergedEntry : mergedStates ) {
			std::unordered_map<std::string, BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( mergedEntry.first );
			if( ownershipIterator != this->ownershipTracking.end() ) {
				ownershipIterator->second.state = mergedEntry.second;
			}
		}
	}
	
	void BorrowChecker::declareOwnership( const std::string& variableName, bool isMutable, const lookup::SourceSharedPointer& source ) {
		BorrowOwnershipInfo ownershipInformation;
		ownershipInformation.declarationSource = source;
		ownershipInformation.isMutable = isMutable;
		ownershipInformation.name = variableName;
		ownershipInformation.scopeDepth = this->currentScopeDepth;
		ownershipInformation.state = BorrowOwnershipState::Owned;
		this->ownershipTracking[variableName] = ownershipInformation;
		if( this->variableScope.empty() == false ) {
			this->variableScope.back().push_back( variableName );
		}
	}
	
	void BorrowChecker::dropValue( const std::string& variableName ) {
		std::unordered_map<std::string,BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( variableName );
		if( ownershipIterator == this->ownershipTracking.end() ) {
			return;
		}
		BorrowOwnershipInfo& variableInformation = ownershipIterator->second;
		for( std::pair<const std::string,BorrowOwnershipInfo>& ownershipEntry : this->ownershipTracking ) {
			BorrowOwnershipInfo& ownerInformation = ownershipEntry.second;
			std::vector<BorrowInfo>& activeBorrows = ownerInformation.activeBorrows;
			activeBorrows.erase(
				std::remove_if(
					activeBorrows.begin(),
					activeBorrows.end(),
					[&variableName]( const BorrowInfo& borrowRecord ) {
						return borrowRecord.borrowerName == variableName;
					}
				),
				activeBorrows.end()
			);
			ownerInformation.immutableBorrowCount = 0;
			ownerInformation.hasMutableBorrow = false;
			for( const BorrowInfo& borrowRecord : activeBorrows ) {
				if( borrowRecord.isMutable ) {
					ownerInformation.hasMutableBorrow = true;
				}
				else {
					ownerInformation.immutableBorrowCount++;
				}
			}
			if( activeBorrows.empty() && ownerInformation.state != BorrowOwnershipState::Moved ) {
				ownerInformation.state = BorrowOwnershipState::Owned;
			}
		}
		variableInformation.state = BorrowOwnershipState::Dropped;
	}
	
	void BorrowChecker::insertDestructors() {
		// RAII destructor insertion is handled during popScope()
    	// The actual destructor code generation happens in the codegen phase
	}
	
	void BorrowChecker::moveValue( const std::string& variableName, const lookup::SourceSharedPointer& source ) {
		std::unordered_map<std::string,BorrowOwnershipInfo>::iterator ownershipIterator = this->ownershipTracking.find( variableName );
		if( ownershipIterator == this->ownershipTracking.end() ) {
			return;
		}
		BorrowOwnershipInfo& ownershipInformation = ownershipIterator->second;
		if( ownershipInformation.state == BorrowOwnershipState::Moved ) {
			std::string moveErrorMessage = fmt::format( "use of moved value \"{}\"", variableName );
			this->diagnostic.error( source, moveErrorMessage, "value was previously moved" );
			return;
		}
		if( ownershipInformation.state == BorrowOwnershipState::Dropped ) {
			std::string dropMoveErrorMessage = fmt::format( "cannot move dropped value \"{}\"", variableName );
			this->diagnostic.error( source, dropMoveErrorMessage, "value was previously deleted" );
			return;
		}
		if( ownershipInformation.activeBorrows.empty() == false ) {
			std::string borrowConflictErrorMessage = fmt::format( "cannot move \"{}\" because it is borrowed", variableName );
			this->diagnostic.error( source, borrowConflictErrorMessage, "outstanding borrows must be released before moving" );
			return;
		}
		ownershipInformation.state = BorrowOwnershipState::Moved;
		if( ownershipInformation.astNode && ownershipInformation.astNode->ownershipAnnotation ) {
			ownershipInformation.astNode->ownershipAnnotation->isMoved = true;
		}
	}
	
	void BorrowChecker::popScope() {
		if( this->variableScope.empty() ) {
			return;
		}
		std::vector<std::string>& scopeVariables = this->variableScope.back();
		for( std::vector<std::string>::reverse_iterator variableIterator = scopeVariables.rbegin(); variableIterator != scopeVariables.rend(); ++variableIterator ) {
			this->dropValue( *variableIterator );
		}
		this->variableScope.pop_back();
		this->currentScopeDepth--;
	}
	
	void BorrowChecker::pushScope() {
		this->currentScopeDepth++;
    	this->variableScope.push_back({});
	}
	
}
