
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

#ifndef _URANITE_OPTIMIZER_OPTIMIZER_HPP_
#define _URANITE_OPTIMIZER_OPTIMIZER_HPP_

#include <unordered_map>
#include <unordered_set>

#include "uranite/ast/node.hpp"
#include "uranite/diagnostic/diagnostic.hpp"

namespace uranite::optimizer {
	
	/**
	 * @brief Specifies the optimization levels available for the Uranite compiler backend.
	 * 
	 * These levels determine the balance between compilation speed, binary size, 
	 * and runtime performance of the generated code.
	 */
	enum class Level {
		
		/** @brief Aggressive optimizations for maximum performance. */
		O3,
		
		/** @brief Standard optimizations suitable for most production builds. */
		O2,
		
		/** @brief Basic optimizations that do not significantly increase compilation time. */
		O1,
		
		/** @brief No optimization; prioritizes compilation speed and debuggability. */
		O0,
		
		/** @brief Maximum speed optimizations that may sacrifice strict standard compliance or precision. */
		OFast
		
	};
	
	/**
	 * @brief Represents a collection of compiler optimization statistics.
	 * 
	 * This structure tracks the frequency of various optimization techniques 
	 * applied during the compilation process.
	 */
	struct Statistic {
		
		/** @brief Total number of constant expressions successfully evaluated. */
		int constantExpressionResionEvaluated = 0;
		
		/** @brief Total number of dead code branches removed. */
		int deadBranchesEliminated = 0;
		
		/** @brief Total number of functions that were devirtualized. */
		int functionsDevirtualized = 0;
		
		/** @brief Total number of functions that were inlined into their callers. */
		int inlinedFunctions = 0;
		
		/** @brief Total number of loops that were unrolled. */
		int loopsUnrolled = 0;
		
		/** @brief Total number of tail calls optimized into jumps. */
		int tailCallsOptimized = 0;
		
		/** @brief Total number of unreachable code statements removed. */
		int unreachableStatementsEliminated = 0;
		
		/** @brief Total number of unused functions removed from the binary. */
		int unusedFunctionsEliminated = 0;
		
		/** @brief Total number of unused local variables removed. */
		int unusedVariablesEliminated = 0;
		
	};
	
	/**
	 * @brief Orchestrates and executes various optimization passes on the Uranite AST.
	 * 
	 * The Optimizer class implements multiple strategies including tail-call optimization,
	 * compile-time constant evaluation, devirtualization, and dead code elimination
	 * to improve the efficiency of the generated code based on the specified level.
	 */
	class Optimizer {
		
		public:
			
			/**
			 * @brief Constructs an Optimizer instance with a diagnostic engine and a target optimization level.
			 * 
			 * @param diagnostic Reference to the diagnostic engine for reporting optimization-related events.
			 * @param level The optimization level to be applied during the process.
			 */
			Optimizer( diagnostic::Engine& diagnostic, Level level = Level::O2 );
			
			/**
			 * @brief Entry point for the optimization process on the entire program structure.
			 * 
			 * @param program The root node of the program AST to be optimized.
			 */
			void optimize( ast::nodes::Program& program );
			
			/**
			 * @brief Retrieves the statistics accumulated during the optimization passes.
			 * 
			 * @return A constant reference to the Statistic structure containing optimization metrics.
			 */
			const Statistic& stats() const {
				return this->statistic;
			}
			
		private:
			
			/** @brief Identifies and marks functions that meet the heuristics for inlining. */
			void markInlineCandidates( ast::nodes::Program& program );
			
			/** @brief Analyzes and replaces virtual calls with direct calls where type information is certain. */
			void devirtualize( ast::nodes::Program& program );
			
			/** @brief Scans for functions within sealed classes to facilitate direct call transformation. */
			void devirtualizeCallsInFunction( ast::nodes::FunctionDeclaration& declaration, const std::unordered_map<std::string,bool>& sealedClasses );
			
			/** @brief Evaluates expressions at compile-time and replaces them with constant literals. */
			void evaluateConstExpressions( ast::nodes::Program& program );
			
			/** @brief Removes unreachable code segments, unused variables, and orphaned functions. */
			void eliminateDeadCode( ast::nodes::Program& program );
			
			/** @brief Removes function definitions that are never called within the program scope. */
			void eliminateUnusedFunctions( ast::nodes::Program& program );
			
			/** @brief Removes local variable declarations that have no impact on program state. */
			void eliminateUnusedVariables( ast::nodes::FunctionDeclaration& function );
			
			/** @brief Transforms recursive tail calls into iterative structures to save stack space. */
			void optimizeTailCalls( ast::nodes::Program& program );
			
			/** @brief Reduces complex expression trees into simpler forms through algebraic identities. */
			void foldSubExpressions( ast::nodes::ExpressionSharedPointer& expression );
			
			/** @brief Traverses an expression to record all function identifiers being invoked. */
			void collectCalledFunctions( const ast::nodes::ExpressionSharedPointer& expression, std::unordered_set<std::string>& called );
			
			/** @brief Traverses a statement to record all function identifiers being invoked. */
			void collectCalledFunctionsInStatement( const ast::nodes::StatementSharedPointer& statement, std::unordered_set<std::string>& called );
			
			/** @brief Traverses an expression to record all variable identifiers being accessed. */
			void collectUsedIdentifiers( const ast::nodes::ExpressionSharedPointer& expression, std::unordered_set<std::string>& used );
			
			/** @brief Traverses a statement to record all variable identifiers being accessed. */
			void collectUsedIdentifiersInStatement( const ast::nodes::StatementSharedPointer& statement, std::unordered_set<std::string>& used );
			
			/** @brief Converts a verified tail-call statement into an equivalent jump or loop structure. */
			void convertTailCall( ast::nodes::StatementSharedPointer& statement, const std::string& functionName );
			
			/** @brief Determines if a branch condition can be resolved as false at compile-time. */
			bool isDeadBranch( const ast::nodes::ExpressionSharedPointer& condition );
			
			/** @brief Determines if an expression is composed entirely of constant literals and operators. */
			bool isConstExpression( const ast::nodes::ExpressionSharedPointer& expression );
			
			/** @brief Checks if a statement represents a valid tail-call for the specified function. */
			bool isTailCall( const ast::nodes::StatementSharedPointer& statement, const std::string& functionName );
			
			/** @brief Determines if an expression sits in a position where its result is the final return value. */
			bool isTailPosition( const ast::nodes::ExpressionSharedPointer& expression, const std::string& functionName );
			
			/** @brief Heuristically decides if a function declaration is a suitable candidate for inlining. */
			bool shouldInline( const ast::nodes::FunctionDeclaration& declaration );
			
			/** @brief Attempts to reduce an expression to a single constant value; returns original if failed. */
			ast::nodes::ExpressionSharedPointer tryEvaluateConstExpression( const ast::nodes::ExpressionSharedPointer& expression );
			
			/** @brief Reference to the diagnostic system for logging and error reporting. */
			diagnostic::Engine& diagnostic;
			
			/** @brief The active optimization level governing the aggressiveness of passes. */
			Level level;
			
			/** @brief Internal container for tracking optimization pass results and counts. */
			Statistic statistic;
		
	};
	
} // namespace uranite::optimizer

#endif // end _URANITE_OPTIMIZER_OPTIMIZER_HPP_
