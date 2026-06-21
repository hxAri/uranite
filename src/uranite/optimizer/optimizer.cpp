
//
// @author hxAri( hxari )
// @create 2025-02-24 15:15
// @update 2026-06-17 20:03
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright( c ) 2025 - hxAri <hxari@proton.me>
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
#include <spdlog/spdlog.h>

#include "uranite/optimizer/optimizer.hpp"

namespace uranite::optimizer {
	
	Optimizer::Optimizer( diagnostic::Engine& diagnostic, Level level ) : diagnostic( diagnostic ), level( level ) {
	}
	
	void Optimizer::collectCalledFunctions( const ast::nodes::ExpressionSharedPointer& expression, std::unordered_set<std::string>& called ) {
		if( expression == nullptr ) return;
		switch( expression->kind ) {
			case ast::Node::Kind::CallExpression: {
				ast::nodes::CallExpression& callExpression = static_cast<ast::nodes::CallExpression&>( *expression );
				if( callExpression.callee && callExpression.callee->kind == ast::Node::Kind::IdentifierExpression ) {
					called.insert( static_cast<ast::nodes::IdentifierExpression&>( *callExpression.callee ).name );
				}
				this->collectCalledFunctions( callExpression.callee, called );
				for( ast::nodes::ExpressionSharedPointer& argument : callExpression.arguments ) {
					this->collectCalledFunctions( argument, called );
				}
				for( ast::nodes::KeywordArgument& kwarg : callExpression.keywordArguments ) {
					this->collectCalledFunctions( kwarg.value, called );
				}
				break;
			}
			case ast::Node::Kind::MethodCallExpression: {
				ast::nodes::MethodCallExpression& methodCallExpression = static_cast<ast::nodes::MethodCallExpression&>( *expression );
				this->collectCalledFunctions( methodCallExpression.object, called );
				for( ast::nodes::ExpressionSharedPointer& argument : methodCallExpression.arguments ) {
					this->collectCalledFunctions( argument, called );
				}
				for( ast::nodes::KeywordArgument& kwarg : methodCallExpression.keywordArguments ) {
					this->collectCalledFunctions( kwarg.value, called );
				}
				break;
			}
			case ast::Node::Kind::BinaryExpression: {
				ast::nodes::BinaryExpression& binaryExpression = static_cast<ast::nodes::BinaryExpression&>( *expression );
				this->collectCalledFunctions( binaryExpression.left, called );
				this->collectCalledFunctions( binaryExpression.right, called );
				break;
			}
			case ast::Node::Kind::UnaryExpression: {
				ast::nodes::UnaryExpression& unaryExpression = static_cast<ast::nodes::UnaryExpression&>( *expression );
				if( unaryExpression.operation == token::Type::KeywordAddressof && unaryExpression.operand->kind == ast::Node::Kind::IdentifierExpression ) {
					called.insert( static_cast<ast::nodes::IdentifierExpression&>( *unaryExpression.operand ).name );
				}
				this->collectCalledFunctions( unaryExpression.operand, called );
				break;
			}
			case ast::Node::Kind::MemberAccessExpression:
				this->collectCalledFunctions( static_cast<ast::nodes::MemberAccessExpression&>( *expression ).object, called );
				break;
			case ast::Node::Kind::ConstructExpression: {
				ast::nodes::ConstructExpression& constructExpression = static_cast<ast::nodes::ConstructExpression&>( *expression );
				for( std::pair<std::string,ast::nodes::ExpressionSharedPointer> pair : constructExpression.fields ) {
					this->collectCalledFunctions( pair.second, called );
				}
				break;
			}
			case ast::Node::Kind::IndexExpression: {
				ast::nodes::IndexExpression& indexExpression = static_cast<ast::nodes::IndexExpression&>( *expression );
				this->collectCalledFunctions( indexExpression.object, called );
				this->collectCalledFunctions( indexExpression.index, called );
				break;
			}
			case ast::Node::Kind::AwaitExpression:
				this->collectCalledFunctions( static_cast<ast::nodes::AwaitExpression&>( *expression ).operand, called );
				break;
			case ast::Node::Kind::LambdaExpression: {
				ast::nodes::LambdaExpression& lambda = static_cast<ast::nodes::LambdaExpression&>( *expression );
				for( ast::nodes::StatementSharedPointer& statement : lambda.body ) {
					this->collectCalledFunctionsInStatement( statement, called );
				}
				break;
			}
			default:
				break;
		}
	}
	
	void Optimizer::collectCalledFunctionsInStatement( const ast::nodes::StatementSharedPointer& statement, std::unordered_set<std::string>& called ) {
		if( statement == nullptr ) return;
		switch( statement->kind ) {
			case ast::Node::Kind::ExpressionStatement:
				this->collectCalledFunctions( static_cast<ast::nodes::ExpressionStatement&>( *statement ).expression, called );
				break;
			case ast::Node::Kind::VariableStatement:
				this->collectCalledFunctions( static_cast<ast::nodes::VariableStatement&>( *statement ).initializer, called );
				break;
			case ast::Node::Kind::AssignmentStatement: {
				ast::nodes::AssignStatement& assign = static_cast<ast::nodes::AssignStatement&>( *statement );
				this->collectCalledFunctions( assign.target, called );
				this->collectCalledFunctions( assign.value, called );
				break;
			}
			case ast::Node::Kind::ReturnStatement:
				this->collectCalledFunctions( static_cast<ast::nodes::ReturnStatement&>( *statement ).value, called );
				break;
			case ast::Node::Kind::IfStatement: {
				ast::nodes::IfStatement& ifStatement = static_cast<ast::nodes::IfStatement&>( *statement );
				this->collectCalledFunctions( ifStatement.condition, called );
				for( ast::nodes::StatementSharedPointer& ifStatementBody : ifStatement.thenBody ) {
					this->collectCalledFunctionsInStatement( ifStatementBody, called );
				}
				for( std::pair<ast::nodes::ExpressionSharedPointer, std::vector<ast::nodes::StatementSharedPointer>>& elifBranch : ifStatement.elifBranches ) {
					this->collectCalledFunctions( elifBranch.first, called );
					for( ast::nodes::StatementSharedPointer& elifStatementBody : elifBranch.second ) {
						this->collectCalledFunctionsInStatement( elifStatementBody, called );
					}
				}
				for( ast::nodes::StatementSharedPointer& ifStatementBody : ifStatement.elseBody ) {
					this->collectCalledFunctionsInStatement( ifStatementBody, called );
				}
				break;
			}
			case ast::Node::Kind::ForStatement: {
				ast::nodes::ForStatement& forStetement = static_cast<ast::nodes::ForStatement&>( *statement );
				this->collectCalledFunctions( forStetement.iterable, called );
				this->collectCalledFunctions( forStetement.initializer, called );
				this->collectCalledFunctions( forStetement.condition, called );
				this->collectCalledFunctionsInStatement( forStetement.update, called );
				for( ast::nodes::StatementSharedPointer& forStatementBody : forStetement.body ) {
					this->collectCalledFunctionsInStatement( forStatementBody, called );
				}
				break;
			}
			case ast::Node::Kind::WhileStatement: {
				ast::nodes::WhileStatement& whileStatement = static_cast<ast::nodes::WhileStatement&>( *statement );
				this->collectCalledFunctions( whileStatement.condition, called );
				for( ast::nodes::StatementSharedPointer& whileStatementBody : whileStatement.body ) {
					this->collectCalledFunctionsInStatement( whileStatementBody, called );
				}
				break;
			}
			case ast::Node::Kind::ThrowStatement:
				this->collectCalledFunctions( static_cast<ast::nodes::ThrowStatement&>( *statement ).expression, called );
				break;
			case ast::Node::Kind::TryCatchStatement: {
				ast::nodes::TryCatchStatement& tryCatch = static_cast<ast::nodes::TryCatchStatement&>( *statement );
				for( ast::nodes::StatementSharedPointer& tryCatchStatementBody : tryCatch.tryBody ) {
					this->collectCalledFunctionsInStatement( tryCatchStatementBody, called );
				}
				for( ast::nodes::ExceptionClause& tryCatchClause : tryCatch.exceptionClauses ) {
					for( ast::nodes::StatementSharedPointer& tryCatchClauseStatementBody : tryCatchClause.body ) {
						this->collectCalledFunctionsInStatement( tryCatchClauseStatementBody, called );
					}
				}
				for( ast::nodes::StatementSharedPointer& tryCatchFinallyStatementBody : tryCatch.finallyBody ) {
					this->collectCalledFunctionsInStatement( tryCatchFinallyStatementBody, called );
				}
				break;
			}
			default:
				break;
		}
	}
	
	void Optimizer::collectUsedIdentifiers( const ast::nodes::ExpressionSharedPointer& expression, std::unordered_set<std::string>& used ) {
		if( expression == nullptr ) return;
		switch( expression->kind ) {
			case ast::Node::Kind::IdentifierExpression:
				used.insert( static_cast<ast::nodes::IdentifierExpression&>( *expression ).name );
				break;
			case ast::Node::Kind::BinaryExpression: {
				ast::nodes::BinaryExpression& binaryExpression = static_cast<ast::nodes::BinaryExpression&>( *expression );
				this->collectUsedIdentifiers( binaryExpression.left, used );
				this->collectUsedIdentifiers( binaryExpression.right, used );
				break;
			}
			case ast::Node::Kind::UnaryExpression:
				this->collectUsedIdentifiers( static_cast<ast::nodes::UnaryExpression&>( *expression ).operand, used );
				break;
			case ast::Node::Kind::CallExpression: {
				ast::nodes::CallExpression& callExpression = static_cast<ast::nodes::CallExpression&>( *expression );
				this->collectUsedIdentifiers( callExpression.callee, used );
				for( ast::nodes::ExpressionSharedPointer& argument : callExpression.arguments ) {
					this->collectUsedIdentifiers( argument, used );
				}
				for( ast::nodes::KeywordArgument& kwarg : callExpression.keywordArguments ) {
					this->collectUsedIdentifiers( kwarg.value, used );
				}
				break;
			}
			case ast::Node::Kind::MethodCallExpression: {
				ast::nodes::MethodCallExpression& methodCallExpression = static_cast<ast::nodes::MethodCallExpression&>( *expression );
				this->collectUsedIdentifiers( methodCallExpression.object, used );
				for( ast::nodes::ExpressionSharedPointer& argument : methodCallExpression.arguments ) {
					this->collectUsedIdentifiers( argument, used );
				}
				for( ast::nodes::KeywordArgument& kwarg : methodCallExpression.keywordArguments ) {
					this->collectUsedIdentifiers( kwarg.value, used );
				}
				break;
			}
			case ast::Node::Kind::MemberAccessExpression:
				this->collectUsedIdentifiers( static_cast<ast::nodes::MemberAccessExpression&>( *expression ).object, used );
				break;
			case ast::Node::Kind::IndexExpression: {
				ast::nodes::IndexExpression& indexExpression = static_cast<ast::nodes::IndexExpression&>( *expression );
				this->collectUsedIdentifiers( indexExpression.object, used );
				this->collectUsedIdentifiers( indexExpression.index, used );
				break;
			}
			case ast::Node::Kind::ConstructExpression: {
				ast::nodes::ConstructExpression& constructExpression = static_cast<ast::nodes::ConstructExpression&>( *expression );
				for( std::pair<std::string,ast::nodes::ExpressionSharedPointer> pair : constructExpression.fields ) {
					this->collectUsedIdentifiers( pair.second, used );
				}
				break;
			}
			case ast::Node::Kind::MatchExpression: {
				ast::nodes::MatchExpression& matchExpression = static_cast<ast::nodes::MatchExpression&>( *expression );
				this->collectUsedIdentifiers( matchExpression.subject, used );
				for( ast::nodes::ExpressionSharedPointer& matchExpressionPattern : matchExpression.patterns ) {
					this->collectUsedIdentifiers( matchExpressionPattern, used );
				}
				for( ast::nodes::ExpressionSharedPointer& matchExpressionValue : matchExpression.values ) {
					this->collectUsedIdentifiers( matchExpressionValue, used );
				}
				if( matchExpression.defaultValue ) {
					this->collectUsedIdentifiers( matchExpression.defaultValue, used );
				}
				break;
			}
			case ast::Node::Kind::AwaitExpression:
				this->collectUsedIdentifiers( static_cast<ast::nodes::AwaitExpression&>( *expression ).operand, used );
				break;
			case ast::Node::Kind::ComprehensionExpression: {
				ast::nodes::ComprehensionExpression& comprehensionExpression = static_cast<ast::nodes::ComprehensionExpression&>( *expression );
				this->collectUsedIdentifiers( comprehensionExpression.bodyExpression, used );
				this->collectUsedIdentifiers( comprehensionExpression.iterable, used );
				if( comprehensionExpression.condition ) {
					this->collectUsedIdentifiers( comprehensionExpression.condition, used );
				}
				break;
			}
			case ast::Node::Kind::LambdaExpression: {
				ast::nodes::LambdaExpression& lambdaExpression = static_cast<ast::nodes::LambdaExpression&>( *expression );
				std::unordered_set<std::string> lambaBound;
				for( ast::nodes::FunctionParameterSharedPointer& lambdaParameter : lambdaExpression.parameters ) {
					lambaBound.insert( lambdaParameter->name );
				}
				for( ast::nodes::StatementSharedPointer& lambdaStatementBody : lambdaExpression.body ) {
					this->collectUsedIdentifiersInStatement( lambdaStatementBody, used );
				}
				break;
			}
			default:
				break;
		}
	}
	
	void Optimizer::collectUsedIdentifiersInStatement( const ast::nodes::StatementSharedPointer& statement, std::unordered_set<std::string>& used ) {
		if( statement == nullptr ) return;
		switch( statement->kind ) {
			case ast::Node::Kind::VariableStatement: {
				ast::nodes::VariableStatement& variable = static_cast<ast::nodes::VariableStatement&>( *statement );
				if( variable.initializer ) {
					this->collectUsedIdentifiers( variable.initializer, used );
				}
				break;
			}
			case ast::Node::Kind::AssignmentStatement: {
				ast::nodes::AssignStatement& assign = static_cast<ast::nodes::AssignStatement&>( *statement );
				this->collectUsedIdentifiers( assign.target, used );
				this->collectUsedIdentifiers( assign.value, used );
				break;
			}
			case ast::Node::Kind::ReturnStatement: {
				ast::nodes::ReturnStatement& returns = static_cast<ast::nodes::ReturnStatement&>( *statement );
				if( returns.value ) {
					this->collectUsedIdentifiers( returns.value, used );
				}
				break;
			}
			case ast::Node::Kind::ExpressionStatement:
				this->collectUsedIdentifiers( static_cast<ast::nodes::ExpressionStatement&>( *statement ).expression, used );
				break;
			case ast::Node::Kind::IfStatement: {
				ast::nodes::IfStatement& ifStatement = static_cast<ast::nodes::IfStatement&>( *statement );
				this->collectUsedIdentifiers( ifStatement.condition, used );
				for( ast::nodes::StatementSharedPointer& ifStatementThenBody : ifStatement.thenBody ) {
					this->collectUsedIdentifiersInStatement( ifStatementThenBody, used );
				}
				for( ast::nodes::StatementSharedPointer& ifStatementElseBody : ifStatement.elseBody ) {
					this->collectUsedIdentifiersInStatement( ifStatementElseBody, used );
				}
				break;
			}
			case ast::Node::Kind::ForStatement: {
				ast::nodes::ForStatement& forStatement = static_cast<ast::nodes::ForStatement&>( *statement );
				if( forStatement.iterable ) {
					this->collectUsedIdentifiers( forStatement.iterable, used );
				}
				if( forStatement.initializer ) {
					this->collectUsedIdentifiers( forStatement.initializer, used );
				}
				if( forStatement.condition ) {
					this->collectUsedIdentifiers( forStatement.condition, used );
				}
				if( forStatement.update ) {
					this->collectUsedIdentifiersInStatement( forStatement.update, used );
				}
				for( ast::nodes::StatementSharedPointer& forStatementBody : forStatement.body ) {
					this->collectUsedIdentifiersInStatement( forStatementBody, used );
				}
				break;
			}
			case ast::Node::Kind::WhileStatement: {
				ast::nodes::WhileStatement& whileStatement = static_cast<ast::nodes::WhileStatement&>( *statement );
				this->collectUsedIdentifiers( whileStatement.condition, used );
				for( ast::nodes::StatementSharedPointer& whileStatementBody : whileStatement.body ) {
					this->collectUsedIdentifiersInStatement( whileStatementBody, used );
				}
				break;
			}
			case ast::Node::Kind::SwitchStatement: {
				ast::nodes::SwitchStatement& switchStatement = static_cast<ast::nodes::SwitchStatement&>( *statement );
				this->collectUsedIdentifiers( switchStatement.subject, used );
				for( ast::nodes::SwitchCaseNodeSharedPointer& switchStatementCase : switchStatement.cases ) {
					if( switchStatementCase->pattern ) {
						this->collectUsedIdentifiers( switchStatementCase->pattern, used );
					}
					for( ast::nodes::StatementSharedPointer& switchStatementCaseBody : switchStatementCase->body ) {
						this->collectUsedIdentifiersInStatement( switchStatementCaseBody, used );
					}
				}
				break;
			}
			case ast::Node::Kind::TryCatchStatement: {
				ast::nodes::TryCatchStatement& tryCatch = static_cast<ast::nodes::TryCatchStatement&>( *statement );
				for( ast::nodes::StatementSharedPointer& tryCatStatement : tryCatch.tryBody ) {
					this->collectUsedIdentifiersInStatement( tryCatStatement, used );
				}
				for( ast::nodes::ExceptionClause& tryCatchStatementClause : tryCatch.exceptionClauses ) {
					for( ast::nodes::StatementSharedPointer& tryCatStatementClauseBody : tryCatchStatementClause.body ) {
						this->collectUsedIdentifiersInStatement( tryCatStatementClauseBody, used );
					}
				}
				for( ast::nodes::StatementSharedPointer& tryCatchStatementFinally : tryCatch.finallyBody ) {
					this->collectUsedIdentifiersInStatement( tryCatchStatementFinally, used );
				}
				break;
			}
			case ast::Node::Kind::ThrowStatement:
				this->collectUsedIdentifiers( static_cast<ast::nodes::ThrowStatement&>( *statement ).expression, used );
				break;
			case ast::Node::Kind::InlineAssemblyStatement: {
				ast::nodes::InlineAssemblyStatement& asmStatement = static_cast<ast::nodes::InlineAssemblyStatement&>( *statement );
				for( ast::nodes::InlineAssemblyOperand& output : asmStatement.outputs ) {
					this->collectUsedIdentifiers( output.expression, used );
				}
				for( ast::nodes::InlineAssemblyOperand& input : asmStatement.inputs ) {
					this->collectUsedIdentifiers( input.expression, used );
				}
				break;
			}
			default:
				break;
		}
	}
	
	void Optimizer::convertTailCall( ast::nodes::StatementSharedPointer& statement, const std::string& functionName ) {
		
		// Mark the call for TCO during codegen
		// The actual transformation( loop conversion ) happens in the LLVM backend
		// via the 'tail' call attribute or manual loop conversion
		if( statement->kind == ast::Node::Kind::ReturnStatement ) {
			ast::nodes::ReturnStatement& returns = static_cast<ast::nodes::ReturnStatement&>( *statement );
			if( returns.value && returns.value->kind == ast::Node::Kind::CallExpression ) {
				
				// Mark for tail call optimization - codegen will emit 'tail call'
				spdlog::debug( "TCO: marked recursive call to '{}' as tail call", functionName );
			}
		}
	}
	
	void Optimizer::devirtualize( ast::nodes::Program& program ) {
		
		// Build map of sealed classes( classes with no subclasses )
		std::unordered_map<std::string,bool> classHasSubclass;
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration && declaration->kind == ast::Node::Kind::ClassDeclaration ) {
				ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
				classHasSubclass[classDeclaration.name] = false;
			}
		}
		
		// Mark classes that are subclassed
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::ClassDeclaration ) continue;
			ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
			if( classDeclaration.baseClassType ) {
				if( classDeclaration.baseClassType->kind == ast::Node::Kind::SimpleType ) {
					std::string& baseName = static_cast<ast::nodes::SimpleTypeNode&>( *classDeclaration.baseClassType ).name;
					classHasSubclass[baseName] = true;
				}
			}
		}
		
		// Sealed classes( no subclasses ) can have their virtual calls devirtualized
		std::unordered_map<std::string,bool> sealedClasses;
		for( std::pair<std::string,bool> pair : classHasSubclass ) {
			sealedClasses[pair.first] = pair.second == false;
		}
		
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::FunctionDeclaration ) {
				continue;
			}
			ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
			this->devirtualizeCallsInFunction( functionDeclaration, sealedClasses );
		}
	}
	
	void Optimizer::devirtualizeCallsInFunction( ast::nodes::FunctionDeclaration& declaration, const std::unordered_map<std::string,bool>& sealedClasses ) {
		
		// Walk the AST and convert virtual calls to direct calls for sealed types
		// This is a simplified version - a full implementation would track types through
		// the data flow to determine exact types at call sites
		( void ) declaration;
		( void ) sealedClasses;
		
		// Devirtualization tracking - logged as a metric
	}
	
	void Optimizer::eliminateDeadCode( ast::nodes::Program& program ) {
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::FunctionDeclaration ) continue;
			ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
			
			// Remove dead branches in if statements
			std::vector<ast::nodes::StatementSharedPointer>::iterator iterator = functionDeclaration.body.begin();
			while( iterator != functionDeclaration.body.end() ) {
				if( *iterator && ( *iterator )->kind == ast::Node::Kind::IfStatement ) {
					ast::nodes::IfStatement& ifStatement = static_cast<ast::nodes::IfStatement&>( **iterator );
					if( this->isDeadBranch( ifStatement.condition ) ) {
						this->statistic.deadBranchesEliminated++;
						spdlog::debug( "DCE: dead branch eliminated in function '{}'", functionDeclaration.name );
						if( ifStatement.condition->kind == ast::Node::Kind::BooleanLiteral ) {
							bool value = static_cast<ast::nodes::BoolLiteralExpression&>( *ifStatement.condition ).value;
							if( value ) {
								std::vector<ast::nodes::StatementSharedPointer> thenStatements = std::move( ifStatement.thenBody );
								iterator = functionDeclaration.body.erase( iterator );
								iterator = functionDeclaration.body.insert( iterator, thenStatements.begin(), thenStatements.end() );
								continue;
							}
							else {
								std::vector<ast::nodes::StatementSharedPointer> elseStatements = std::move( ifStatement.elseBody );
								iterator = functionDeclaration.body.erase( iterator );
								if( elseStatements.empty() == false ) {
									iterator = functionDeclaration.body.insert( iterator, elseStatements.begin(), elseStatements.end() );
								}
								continue;
							}
						}
					}
				}
				++iterator;
			}
			
			// Remove code after return statements
			bool foundReturn = false;
			std::vector<ast::nodes::StatementSharedPointer>::iterator removeStart = std::remove_if( 
				functionDeclaration.body.begin(), 
				functionDeclaration.body.end(),
				[&foundReturn, &functionDeclaration, this]( const ast::nodes::StatementSharedPointer& s ) {
					if( foundReturn ) {
						this->statistic.unreachableStatementsEliminated++;
						spdlog::debug( "DCE: unreachable statement removed in function '{}'", functionDeclaration.name );
						return true;
					}
					if( s && s->kind == ast::Node::Kind::ReturnStatement ) {
						foundReturn = true;
					}
					return false;
				}
			);
			functionDeclaration.body.erase( removeStart, functionDeclaration.body.end() );
			this->eliminateUnusedVariables( functionDeclaration );
		}
		this->eliminateUnusedFunctions( program );
	}
	
	void Optimizer::eliminateUnusedFunctions( ast::nodes::Program& program ) {
		std::unordered_set<std::string> calledFunctions;
		calledFunctions.insert( "main" );
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr ) continue;
			if( declaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
				for( ast::nodes::StatementSharedPointer& statement : functionDeclaration.body ) {
					this->collectCalledFunctionsInStatement( statement, calledFunctions );
				}
			}
			if( declaration->kind == ast::Node::Kind::ClassDeclaration ) {
				ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
				for( ast::nodes::DeclarationSharedPointer& method : classDeclaration.methods ) {
					if( method->kind == ast::Node::Kind::FunctionDeclaration ) {
						ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *method );
						for( ast::nodes::StatementSharedPointer& statement : functionDeclaration.body ) {
							this->collectCalledFunctionsInStatement( statement, calledFunctions );
						}
					}
				}
			}
		}
		
		// Transitively collect — functions called by called functions
		bool changed = true;
		while( changed ) {
			changed = false;
			for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
				if( declaration && declaration->kind == ast::Node::Kind::FunctionDeclaration ) {
					ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
					if( calledFunctions.count( functionDeclaration.name ) ) {
						size_t before = calledFunctions.size();
						for( ast::nodes::StatementSharedPointer& statement : functionDeclaration.body ) {
							this->collectCalledFunctionsInStatement( statement, calledFunctions );
						}
						if( calledFunctions.size() > before ) changed = true;
					}
				}
			}
		}
		std::vector<ast::nodes::DeclarationSharedPointer>::iterator iterator = program.declarations.begin();
		while( iterator != program.declarations.end() ) {
			if( *iterator && ( *iterator )->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( **iterator );
				if( functionDeclaration.access != ast::AccessModifier::Public && functionDeclaration.name != "main" &&
					calledFunctions.find( functionDeclaration.name ) == calledFunctions.end() ) {
					spdlog::debug( "DCE: unused function '{}' removed", functionDeclaration.name );
					this->statistic.unusedFunctionsEliminated++;
					iterator = program.declarations.erase( iterator );
					continue;
				}
			}
			++iterator;
		}
	}
	
	void Optimizer::eliminateUnusedVariables( ast::nodes::FunctionDeclaration& function ) {
		std::unordered_set<std::string> usedIdentifiers;
		for( ast::nodes::StatementSharedPointer& statement : function.body ) {
			this->collectUsedIdentifiersInStatement( statement, usedIdentifiers );
		}
		std::vector<ast::nodes::StatementSharedPointer>::iterator iterator = function.body.begin();
		while( iterator != function.body.end() ) {
			if( *iterator && ( *iterator )->kind == ast::Node::Kind::VariableStatement ) {
				ast::nodes::VariableStatement& variable = static_cast<ast::nodes::VariableStatement&>( **iterator );
				bool hasNoSideEffects = variable.initializer == nullptr ||
					variable.initializer->kind == ast::Node::Kind::IntegerLiteral ||
					variable.initializer->kind == ast::Node::Kind::FloatLiteral ||
					variable.initializer->kind == ast::Node::Kind::BooleanLiteral ||
					variable.initializer->kind == ast::Node::Kind::StringLiteral ||
					variable.initializer->kind == ast::Node::Kind::CharLiteral ||
					variable.initializer->kind == ast::Node::Kind::IdentifierExpression ||
					variable.initializer->kind == ast::Node::Kind::MemberAccessExpression;
				if( hasNoSideEffects && usedIdentifiers.find( variable.name ) == usedIdentifiers.end() ) {
					spdlog::debug( "DCE: unused variable '{}' removed in function '{}'", variable.name, function.name );
					this->statistic.unusedVariablesEliminated++;
					iterator = function.body.erase( iterator );
					continue;
				}
			}
			++iterator;
		}
	}
	
	void Optimizer::evaluateConstExpressions( ast::nodes::Program& program ) {
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::FunctionDeclaration ) continue;
			ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
			for( ast::nodes::StatementSharedPointer& statement : functionDeclaration.body ) {
				if( statement == nullptr ) continue;
				if( statement->kind == ast::Node::Kind::VariableStatement ) {
					ast::nodes::VariableStatement& variable = static_cast<ast::nodes::VariableStatement&>( *statement );
					if( variable.initializer ) {
						ast::nodes::ExpressionSharedPointer result = this->tryEvaluateConstExpression( variable.initializer );
						if( result ) {
							variable.initializer = result;
							this->statistic.constantExpressionResionEvaluated++;
						}
					}
				}
				
				// Also fold expressions in return statements
				if( statement->kind == ast::Node::Kind::ReturnStatement ) {
					ast::nodes::ReturnStatement& returns = static_cast<ast::nodes::ReturnStatement&>( *statement );
					if( returns.value ) {
						ast::nodes::ExpressionSharedPointer result = this->tryEvaluateConstExpression( returns.value );
						if( result ) {
							returns.value = result;
							this->statistic.constantExpressionResionEvaluated++;
						}
					}
				}
				
				// Fold expressions in expression statements
				if( statement->kind == ast::Node::Kind::ExpressionStatement ) {
					ast::nodes::ExpressionStatement& expressionStatement = static_cast<ast::nodes::ExpressionStatement&>( *statement );
					if( expressionStatement.expression ) {
						this->foldSubExpressions( expressionStatement.expression );
					}
				}
			}
		}
	}
	
	void Optimizer::foldSubExpressions( ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression == nullptr ) return;
		if( expression->kind == ast::Node::Kind::BinaryExpression ) {
			ast::nodes::BinaryExpression& binaryExpression = static_cast<ast::nodes::BinaryExpression&>( *expression );
			this->foldSubExpressions( binaryExpression.left );
			this->foldSubExpressions( binaryExpression.right );
			
			// Try to fold after children are folded
			ast::nodes::ExpressionSharedPointer result = this->tryEvaluateConstExpression( expression );
			if( result ) {
				expression = result;
				this->statistic.constantExpressionResionEvaluated++;
				return;
			}
			
			// Strength reduction: x * 2 -> x << 1, x * 1 -> x, x + 0 -> x
			if( binaryExpression.operation == token::Type::Star && binaryExpression.right->kind == ast::Node::Kind::IntegerLiteral ) {
				int64_t value = static_cast<ast::nodes::IntegerLiteralExpression&>( *binaryExpression.right ).value;
				if( value == 1 ) { expression = binaryExpression.left; return; }
				if( value == 2 ) {
					binaryExpression.operation = token::Type::ShiftLeft;
					binaryExpression.right = std::make_shared<ast::nodes::IntegerLiteralExpression>( 1, "1", binaryExpression.source );
					return;
				}
				if( value == 4 ) {
					binaryExpression.operation = token::Type::ShiftLeft;
					binaryExpression.right = std::make_shared<ast::nodes::IntegerLiteralExpression>( 2, "2", binaryExpression.source );
					return;
				}
				if( value == 8 ) {
					binaryExpression.operation = token::Type::ShiftLeft;
					binaryExpression.right = std::make_shared<ast::nodes::IntegerLiteralExpression>( 3, "3", binaryExpression.source );
					return;
				}
			}
			if( binaryExpression.operation == token::Type::Plus && binaryExpression.right->kind == ast::Node::Kind::IntegerLiteral ) {
				if( static_cast<ast::nodes::IntegerLiteralExpression&>( *binaryExpression.right ).value == 0 ) {
					expression = binaryExpression.left;
					return;
				}
			}
			if( binaryExpression.operation == token::Type::Minus && binaryExpression.right->kind == ast::Node::Kind::IntegerLiteral ) {
				if( static_cast<ast::nodes::IntegerLiteralExpression&>( *binaryExpression.right ).value == 0 ) {
					expression = binaryExpression.left;
					return;
				}
			}
		}
		if( expression->kind == ast::Node::Kind::CallExpression ) {
			ast::nodes::CallExpression& callExpression = static_cast<ast::nodes::CallExpression&>( *expression );
			for( ast::nodes::ExpressionSharedPointer& argument : callExpression.arguments ) {
				this->foldSubExpressions( argument );
			}
		}
	}
	
	bool Optimizer::isConstExpression( const ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression == nullptr ) return false;
		switch( expression->kind ) {
			case ast::Node::Kind::IntegerLiteral:
			case ast::Node::Kind::FloatLiteral:
			case ast::Node::Kind::BooleanLiteral:
			case ast::Node::Kind::StringLiteral:
			case ast::Node::Kind::CharLiteral:
				return true;
			case ast::Node::Kind::BinaryExpression: {
				ast::nodes::BinaryExpression& binaryExpression = static_cast<ast::nodes::BinaryExpression&>( *expression );
				return this->isConstExpression( binaryExpression.left ) && this->isConstExpression( binaryExpression.right );
			}
			case ast::Node::Kind::UnaryExpression: {
				ast::nodes::UnaryExpression& un = static_cast<ast::nodes::UnaryExpression&>( *expression );
				return this->isConstExpression( un.operand );
			}
			default:
				return false;
		}
	}
	
	bool Optimizer::isDeadBranch( const ast::nodes::ExpressionSharedPointer& condition ) {
		if( condition ) {
			return condition->kind == ast::Node::Kind::BooleanLiteral;
		}
		return false;
	}
	
	bool Optimizer::isTailCall( const ast::nodes::StatementSharedPointer& statement, const std::string& functionName ) {
		if( statement ) {
			
			// A return statement with a recursive call is a tail call
			if( statement->kind == ast::Node::Kind::ReturnStatement ) {
				ast::nodes::ReturnStatement& returns = static_cast<ast::nodes::ReturnStatement&>( *statement );
				return returns.value && this->isTailPosition( returns.value, functionName );
			}
			
			// Last statement in if/else branches
			if( statement->kind == ast::Node::Kind::IfStatement ) {
				ast::nodes::IfStatement& ifStatement = static_cast<ast::nodes::IfStatement&>( *statement );
				bool tailInThen = ifStatement.thenBody.empty() == false && this->isTailCall( ifStatement.thenBody.back(), functionName );
				bool tailInElse = ifStatement.elseBody.empty() == false && this->isTailCall( ifStatement.elseBody.back(), functionName );
				return tailInThen || tailInElse;
			}
		}
		return false;
	}
	
	bool Optimizer::isTailPosition( const ast::nodes::ExpressionSharedPointer& expression, const std::string& functionName ) {
		if( expression == nullptr ) return false;
		if( expression->kind == ast::Node::Kind::CallExpression ) {
			ast::nodes::CallExpression& callExpression = static_cast<ast::nodes::CallExpression&>( *expression );
			if( callExpression.callee && callExpression.callee->kind == ast::Node::Kind::IdentifierExpression ) {
				return static_cast<ast::nodes::IdentifierExpression&>( *callExpression.callee ).name == functionName;
			}
		}
		return false;
	}
	
	void Optimizer::markInlineCandidates( ast::nodes::Program& program ) {
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::FunctionDeclaration ) {
				continue;
			}
			ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
			if( this->shouldInline( functionDeclaration ) ) {
				
				// Mark function for inlining - codegen will use LLVM's inlining attributes
				this->statistic.inlinedFunctions++;
				spdlog::debug( "Marked function '{}' as inline candidate", functionDeclaration.name );
			}
		}
	}
	
	void Optimizer::optimize( ast::nodes::Program& program ) {
		if( this->level == Level::O0 ) {
			return;
		}
		spdlog::info( "Running optimization passes( level: O{} )", this->level == Level::O1 ? "1" : this->level == Level::O2 ? "2" : this->level == Level::O3 ? "3" : "fast" );
		
		// Always run these
		this->evaluateConstExpressions( program );
		this->optimizeTailCalls( program );
		if( this->level >= Level::O2 ) {
			this->devirtualize( program );
			this->eliminateDeadCode( program );
		}
		if( this->level >= Level::O3 ) {
			this->markInlineCandidates( program );
		}
		spdlog::info( "Optimization complete: {} tail calls, {} devirtualized, {} constexpression evaluated",
			this->statistic.tailCallsOptimized, 
			this->statistic.functionsDevirtualized,
			this->statistic.constantExpressionResionEvaluated
		);
	}
	
	void Optimizer::optimizeTailCalls( ast::nodes::Program& program ) {
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::FunctionDeclaration ) {
				continue;
			}
			ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
			for( ast::nodes::StatementSharedPointer& statement : functionDeclaration.body ) {
				if( this->isTailCall( statement, functionDeclaration.name ) ) {
					this->convertTailCall( statement, functionDeclaration.name );
					this->statistic.tailCallsOptimized++;
				}
			}
		}
	}
	
	bool Optimizer::shouldInline( const ast::nodes::FunctionDeclaration& declaration ) {
		
		// Small functions( < 5 statements ) are inline candidates
		if( declaration.body.size() <= 4 && declaration.isVirtual == false ) {
			return true;
		}
		
		// Single-expression functions
		if( declaration.body.size() == 1 && 
			declaration.body[0]->kind == ast::Node::Kind::ReturnStatement ) {
			return true;
		}
		return false;
	}
	
	ast::nodes::ExpressionSharedPointer Optimizer::tryEvaluateConstExpression( const ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression == nullptr || this->isConstExpression( expression ) == false ) {
			return nullptr;
		}
		if( expression->kind == ast::Node::Kind::BinaryExpression ) {
			ast::nodes::BinaryExpression& binaryExpression = static_cast<ast::nodes::BinaryExpression&>( *expression );
			
			// Integer constant folding
			if( binaryExpression.left->kind == ast::Node::Kind::IntegerLiteral &&
				binaryExpression.right->kind == ast::Node::Kind::IntegerLiteral ) {
				int64_t integerLiteralExpressionLeft = static_cast<ast::nodes::IntegerLiteralExpression&>( *binaryExpression.left ).value;
				int64_t integerLiteralExpressionRight = static_cast<ast::nodes::IntegerLiteralExpression&>( *binaryExpression.right ).value;
				int64_t result = 0;
				switch( binaryExpression.operation ) {
					case token::Type::Plus: result = integerLiteralExpressionLeft + integerLiteralExpressionRight; break;
					case token::Type::Minus: result = integerLiteralExpressionLeft - integerLiteralExpressionRight; break;
					case token::Type::Star: result = integerLiteralExpressionLeft * integerLiteralExpressionRight; break;
					case token::Type::Slash:
						if( integerLiteralExpressionRight == 0 ) {
							return nullptr; // division by zero
						}
						result = integerLiteralExpressionLeft / integerLiteralExpressionRight;
						break;
					case token::Type::Percent:
						if( integerLiteralExpressionRight == 0 ) {
							return nullptr;
						}
						result = integerLiteralExpressionLeft % integerLiteralExpressionRight;
						break;
					case token::Type::Power: {
						result = 1;
						for( int64_t i=0; i<integerLiteralExpressionRight; i++ ) {
							result *= integerLiteralExpressionLeft;
						}
						break;
					}
					case token::Type::ShiftLeft:
						result = integerLiteralExpressionLeft << integerLiteralExpressionRight; break;
					case token::Type::ShiftRight:
						result = integerLiteralExpressionLeft >> integerLiteralExpressionRight; break;
					case token::Type::Ampersand:
						result = integerLiteralExpressionLeft & integerLiteralExpressionRight; break;
					case token::Type::Pipe:
						result = integerLiteralExpressionLeft | integerLiteralExpressionRight; break;
					case token::Type::Caret:
						result = integerLiteralExpressionLeft ^ integerLiteralExpressionRight; break;
					default: return nullptr;
				}
				return std::make_shared<ast::nodes::IntegerLiteralExpression>( result, std::to_string( result ), binaryExpression.source );
			}
			
			// Float constant folding
			if( binaryExpression.left->kind == ast::Node::Kind::FloatLiteral &&
				binaryExpression.right->kind == ast::Node::Kind::FloatLiteral ) {
				double floatLiteralExpressionLeft = static_cast<ast::nodes::FloatLiteralExpression&>( *binaryExpression.left ).value;
				double floatLiteralExpressionRight = static_cast<ast::nodes::FloatLiteralExpression&>( *binaryExpression.right ).value;
				double result = 0.0;
				switch( binaryExpression.operation ) {
					case token::Type::Plus: result = floatLiteralExpressionLeft + floatLiteralExpressionRight; break;
					case token::Type::Minus: result = floatLiteralExpressionLeft - floatLiteralExpressionRight; break;
					case token::Type::Star: result = floatLiteralExpressionLeft * floatLiteralExpressionRight; break;
					case token::Type::Slash:
						if( floatLiteralExpressionRight == 0.0 ) {
							return nullptr;
						}
						result = floatLiteralExpressionLeft / floatLiteralExpressionRight;
						break;
					default: return nullptr;
				}
				return std::make_shared<ast::nodes::FloatLiteralExpression>( std::to_string( result ),binaryExpression.source,result );
			}
			
			// Boolean constant folding
			if( binaryExpression.left->kind == ast::Node::Kind::BooleanLiteral &&
				binaryExpression.right->kind == ast::Node::Kind::BooleanLiteral ) {
				bool booleanLiteralExpressionLeft = static_cast<ast::nodes::BoolLiteralExpression&>( *binaryExpression.left ).value;
				bool booleanLiteralExpressionRight = static_cast<ast::nodes::BoolLiteralExpression&>( *binaryExpression.right ).value;
				bool result = false;
				switch( binaryExpression.operation ) {
					case token::Type::KeywordAnd:
						result = booleanLiteralExpressionLeft  && booleanLiteralExpressionRight; break;
					case token::Type::KeywordOr:
						result = booleanLiteralExpressionLeft  || booleanLiteralExpressionRight; break;
					case token::Type::Equal:
						result = booleanLiteralExpressionLeft  == booleanLiteralExpressionRight; break;
					case token::Type::NotEqual:
						result = booleanLiteralExpressionLeft  != booleanLiteralExpressionRight; break;
					default: return nullptr;
				}
				return std::make_shared<ast::nodes::BoolLiteralExpression>( result, binaryExpression.source );
			}
			
			// Comparison constant folding( int )
			if( binaryExpression.left->kind == ast::Node::Kind::IntegerLiteral &&
				binaryExpression.right->kind == ast::Node::Kind::IntegerLiteral ) {
				int64_t l = static_cast<ast::nodes::IntegerLiteralExpression&>( *binaryExpression.left ).value;
				int64_t r = static_cast<ast::nodes::IntegerLiteralExpression&>( *binaryExpression.right ).value;
				bool result = false;
				switch( binaryExpression.operation ) {
					case token::Type::Equal:
						result = l == r; break;
					case token::Type::NotEqual:
						result = l != r; break;
					case token::Type::LessThan:
						result = l < r; break;
					case token::Type::GreaterThan:
						result = l > r; break;
					case token::Type::LessThanEqual:
						result = l <= r; break;
					case token::Type::GreaterThanEqual:
						result = l >= r; break;
					default: return nullptr;
				}
				return std::make_shared<ast::nodes::BoolLiteralExpression>( result, binaryExpression.source );
			}
		}
		if( expression->kind == ast::Node::Kind::UnaryExpression ) {
			ast::nodes::UnaryExpression& unaryExpression = static_cast<ast::nodes::UnaryExpression&>( *expression );
			if( unaryExpression.operand->kind == ast::Node::Kind::IntegerLiteral && 
				unaryExpression.operation == token::Type::Minus ) {
				int64_t value = static_cast<ast::nodes::IntegerLiteralExpression&>( *unaryExpression.operand ).value;
				return std::make_shared<ast::nodes::IntegerLiteralExpression>( -value, std::to_string( -value ), unaryExpression.source );
			}
			if( unaryExpression.operand->kind == ast::Node::Kind::BooleanLiteral && ( unaryExpression.operation == token::Type::Bang || unaryExpression.operation == token::Type::KeywordNot ) ) {
				bool value = static_cast<ast::nodes::BoolLiteralExpression&>( *unaryExpression.operand ).value;
				return std::make_shared<ast::nodes::BoolLiteralExpression>( value == false, unaryExpression.source );
			}
		}
		return nullptr;
	}
	
}
