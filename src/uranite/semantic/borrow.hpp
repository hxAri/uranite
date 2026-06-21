
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

#ifndef _URANITE_SEMANTIC_BORROW_HPP_
#define _URANITE_SEMANTIC_BORROW_HPP_

#include <string>
#include <unordered_map>
#include <vector>

#include "uranite/ast/node.hpp"
#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/lookup/source.hpp"

namespace uranite::semantic {
	
	/** @brief Details about a variable that is using memory belonging to someone else. */
	struct BorrowInfo {
		
		/// @brief The name of the part that is using the memory.
		std::string borrowerName;
		
		/// @brief Set to true if the borrower is allowed to change the data.
		bool isMutable;
		
		/// @brief The name of the original owner of the memory.
		std::string ownerName;
		
		/// @brief The numerical level of the code area where this happened.
		int scopeDepth;
		
		/// @brief The place in the source code where this record was created.
		lookup::SourceSharedPointer source;
		
	};
	
	/** @brief Represents the different ways a variable can hold or relate to its memory. */
	enum class BorrowOwnershipState {
		
		/// @brief The memory is currently being used by someone else with permission to read it.
		Borrowed,
		
		/// @brief The memory has been removed or is no longer available for use.
		Dropped,
		
		/// @brief The control of the memory has been handed over to another part of the code.
		Moved,
		
		/// @brief The memory is currently being used by someone else who can also change it.
		MutableBorrowed,
		
		/// @brief The variable is the primary holder and controller of the memory.
		Owned
		
	};
	
	/** @brief Contains all details regarding who holds and uses a specific piece of memory. */
	struct BorrowOwnershipInfo {
		
		/// @brief A list of all current users who have permission to access this memory.
		std::vector< BorrowInfo > activeBorrows;
		
		/// @brief The name of the area that manages this memory if it belongs to a group.
		std::string arenaOwnerName;
		
		/// @brief Pointer to the AST node for persisting ownership annotations to codegen.
		ast::nodes::VariableStatement* astNode = nullptr;
		
		/// @brief The place in the source code where this variable was first created.
		lookup::SourceSharedPointer declarationSource;
		
		/// @brief Remembers if someone currently has permission to change this memory.
		bool hasMutableBorrow;
		
		/// @brief The number of users currently allowed to read this memory.
		int immutableBorrowCount = 0;
		
		/// @brief Tracks if this memory is managed as part of a larger group of items.
		bool isArenaManaged = false;
		
		/// @brief Tracks if this memory is stored in the general long-term storage area.
		bool isHeapAllocated = false;
		
		/// @brief Set to true if the primary holder is allowed to change the data.
		bool isMutable;
		
		/// @brief The name of the variable that holds this memory.
		std::string name;
		
		/// @brief The numerical level of the code area where this variable exists.
		int scopeDepth;
		
		/// @brief The current safety status of the memory.
		BorrowOwnershipState state;
	
	};
	
	/**
	 * @brief A tool that monitors how memory is used and shared throughout the code.
	 * 
	 * This class ensures that memory is accessed safely and follows ownership rules
	 * to prevent common mistakes like using memory after it has been deleted.
	 */
	class BorrowChecker {
		
		public:
			
			/**
			 * @brief Prepares the checker with a tool for reporting mistakes.
			 * @param diagnostic The engine used to send error messages to the user.
			 */
			BorrowChecker( diagnostic::Engine& diagnostic );
			
			/**
			 * @brief Starts the safety analysis for the entire program.
			 * @param program The full structure of the program to be checked.
			 */
			void check( ast::nodes::Program& program );
			
		private:
			
			// General Checking Operations
			
			/**
			 * @brief Looks at a memory assignment to make sure it is safe.
			 * @param statement The code representing the assignment operation.
			 */
			void checkAssignmentStatement( ast::nodes::AssignStatement& statement );
			
			/**
			 * @brief Examines a group of code instructions within a specific area.
			 * @param statements A list of pointers to the code instructions.
			 */
			void checkBlock( const std::vector<ast::nodes::StatementSharedPointer>& statements );
			
			/**
			 * @brief Verifies that a new name or item follows safety rules when created.
			 * @param declaration The code representing the new declaration.
			 */
			void checkDeclaration( const ast::nodes::DeclarationSharedPointer& declaration );
			
			/**
			 * @brief Inspects a specific calculation or value for ownership rules.
			 * @param expression The code representing the value or calculation.
			 * @param transferOwnership Set to true if the control of memory should be moved.
			 */
			void checkExpression( const ast::nodes::ExpressionSharedPointer& expression, bool transferOwnership = false );
			
			/**
			 * @brief Analyzes a repeating loop to ensure memory stays safe.
			 * @param statement The code representing the "for" loop structure.
			 */
			void checkForStatement( ast::nodes::ForStatement& statement );
			
			/**
			 * @brief Checks a function to ensure its internal memory is handled correctly.
			 * @param functionDeclaration The code representing the function.
			 */
			void checkFunctionDeclaration( ast::nodes::FunctionDeclaration& functionDeclaration );
			
			/**
			 * @brief Examines a choice point in the code for safety in all paths.
			 * @param statement The code representing the "if" branch.
			 */
			void checkIfStatement( ast::nodes::IfStatement& statement );
			
			/**
			 * @brief Verifies a point where the code exits and returns memory.
			 * @param statement The code representing the return instruction.
			 */
			void checkReturnStatement( ast::nodes::ReturnStatement& statement );
			
			/**
			 * @brief Analyzes a single instruction to ensure it follows safety standards.
			 * @param statement The pointer to the specific code instruction.
			 */
			void checkStatement( const ast::nodes::StatementSharedPointer& statement );
			
			/**
			 * @brief Monitors a section of code where safety rules are handled differently.
			 * @param statement The code representing the unsafe block.
			 */
			void checkUnsafeBlockStatement( ast::nodes::UnsafeBlockStatement& statement );
			
			/**
			 * @brief Inspects a variable creation to track its memory control.
			 * @param statement The code representing the variable definition.
			 */
			void checkVarStatement( ast::nodes::VariableStatement& statement );
			
			/**
			 * @brief Checks a loop that continues as long as a condition is met.
			 * @param statement The code representing the "while" loop.
			 */
			void checkWhileStatement( ast::nodes::WhileStatement& statement );
			
			// Ownership Operations
			
			/**
			 * @brief Allows one part of the code to use memory belonging to another part.
			 * @param ownerName The name of the original owner.
			 * @param borrowerName The name of the part that wants to use the memory.
			 * @param isMutable Set to true if the borrower is allowed to change the data.
			 * @param source The place in the source code where this happens.
			 */
			void borrowValue( const std::string& ownerName, const std::string& borrowerName, bool isMutable, const lookup::SourceSharedPointer& source );
			
			/**
			 * @brief Checks if a new request to use memory conflicts with current users.
			 * @param variableName The name of the memory being checked.
			 * @param isMutable Set to true if the check is for a changeable access.
			 * @param source The place in the source code where this happens.
			 */
			void checkBorrowConflict( const std::string& variableName, bool isMutable, const lookup::SourceSharedPointer& source );
			
			/**
			 * @brief Prevents the code from using memory after its control has moved.
			 * @param variableName The name of the memory being checked.
			 * @param source The place in the source code where this happens.
			 */
			void checkUseAfterMove( const std::string& variableName, const lookup::SourceSharedPointer& source );
			
			/**
			 * @brief Records the birth of a new variable and its control rights.
			 * @param variableName The name of the new variable.
			 * @param isMutable Set to true if the variable can be changed.
			 * @param source The place in the source code where this happens.
			 */
			void declareOwnership( const std::string& variableName, bool isMutable, const lookup::SourceSharedPointer& source );
			
			/**
			 * @brief Removes a variable and cleans up its memory rights.
			 * @param variableName The name of the variable to be removed.
			 */
			void dropValue( const std::string& variableName );
			
			/**
			 * @brief Hands over the control of memory from one place to another.
			 * @param variableName The name of the memory being moved.
			 * @param source The place in the source code where this happens.
			 */
			void moveValue( const std::string& variableName, const lookup::SourceSharedPointer& source );
			
			// Scope Management Operations
			
			/**
			 * @brief Leaves the current code area and cleans up local variables.
			 */
			void popScope();
			
			/**
			 * @brief Enters a new code area to track local variables.
			 */
			void pushScope();
			
			// Memory Cleanup Operations
			
			/**
			 * @brief Ensures memory from a loop cycle is cleaned before the next cycle.
			 * @param scopeVariables A list of names of the variables in the current area.
			 * @param source The place in the source code where the loop is.
			 */
			void checkLoopIterationCleanup( const std::vector<std::string>& scopeVariables, const lookup::SourceSharedPointer& source );
			
			/**
			 * @brief Captures the current ownership state of all tracked variables.
			 * @return A map from variable name to its current ownership state.
			 */
			std::unordered_map<std::string, BorrowOwnershipState> snapshotOwnershipStates();
			
			/**
			 * @brief Restores all tracked variable ownership states from a previously captured snapshot.
			 * @param snapshot The ownership states to restore.
			 */
			void restoreOwnershipStates( const std::unordered_map<std::string, BorrowOwnershipState>& snapshot );
			
			/**
			 * @brief Merges ownership states from multiple control flow branches using conservative rules.
			 *
			 * If any branch moves or drops a variable, the merged state reflects that pessimistically
			 * so that subsequent use-after-move or use-after-drop is detected.
			 *
			 * @param branchSnapshots A vector of ownership state snapshots, one per branch taken.
			 * @param mergeSource The source location of the merge point for diagnostic reporting.
			 */
			void mergeOwnershipStates(
				const std::vector<std::unordered_map<std::string, BorrowOwnershipState>>& branchSnapshots,
				const lookup::SourceSharedPointer& mergeSource
			);
			
			/**
			 * @brief Adds instructions to clean up memory that is no longer useful.
			 */
			void insertDestructors();
			
			// Class Properties
			
			/// Tracks how deep the current nested code areas are.
			int currentScopeDepth = 0;
			
			/// The tool used to send error messages to the user.
			diagnostic::Engine& diagnostic;
			
			/// Remembers if the analysis is currently inside a repeating loop.
			bool isInsideLoop = false;
			
			/// Remembers if the analysis is currently in a less restricted safety zone.
			bool isInsideUnsafeBlock = false;
			
			/// A map that connects variable names to their ownership status.
			std::unordered_map<std::string, BorrowOwnershipInfo> ownershipTracking;
			
			/// A list that stores variable names based on the code area where they exist.
			std::vector<std::vector<std::string>> variableScope;
		
	};
	
} // namespace uranite::semantic

#endif // end _URANITE_SEMANTIC_BORROW_HPP_
