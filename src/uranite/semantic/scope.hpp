
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

#ifndef _URANITE_SEMANTIC_SCOPE_HPP_
#define _URANITE_SEMANTIC_SCOPE_HPP_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "uranite/semantic/symbol.hpp"
#include "uranite/semantic/typeref.hpp"

namespace uranite::semantic {
	
	/**
	 * @brief Represents a lexical or logical scope within the Uranite compiler.
	 * 
	 * Manages symbol visibility, parent-child scope relationships, and specific
	 * metadata for functional, class, or loop-based blocks.
	 */
	class Scope {
		
		public:
		
			/**
			 * @brief Enumeration of different types of scopes.
			 */
			enum class Kind {
				Block,
				Class,
				Function,
				Global,
				Loop,
				Module,
				Switch,
				Unsafe
			};
			
			/**
			 * @brief Constructs a new Scope object.
			 * @param kind The category of this scope.
			 * @param parent A shared pointer to the enclosing (parent) scope.
			 */
			Scope( Kind kind, std::shared_ptr<Scope> parent = nullptr );
			
			/** @brief Reference to the type if this scope belongs to a class. */
			TypeSharedPointer classType;
			
			/** @brief Reference to the expected return type if this scope belongs to a function. */
			TypeSharedPointer returnType;
			
			/**
			 * @brief Defines a new symbol within the current scope.
			 * @param name The identifier name of the symbol.
			 * @param symbol The pointer to the symbol definition.
			 * @return True if the symbol was successfully defined, false if it already exists.
			 */
			bool define( const std::string& name, SymbolSharedPointer symbol );
			
			/**
			 * @brief Checks if this scope or any of its parents are within a class.
			 * @return True if inside a class scope.
			 */
			bool isInsideClass() const;
			
			/**
			 * @brief Checks if this scope or any of its parents are within a function.
			 * @return True if inside a function scope.
			 */
			bool isInsideFunction() const;
			
			/**
			 * @brief Checks if this scope or any of its parents are within a loop.
			 * @return True if inside a loop scope.
			 */
			bool isInsideLoop() const;
			
			/**
			 * @brief Checks if this scope or any of its parents are marked as unsafe.
			 * @return True if inside an unsafe scope.
			 */
			bool isInsideUnsafe() const;
			
			/**
			 * @brief Gets the kind of the current scope.
			 * @return The Kind value.
			 */
			Kind kind() const {
				return this->kindT;
			}
			
			/**
			 * @brief Performs a recursive lookup for a symbol by name.
			 * @param name The identifier name to search for.
			 * @return The first SymbolSharedPointer if found in this or parent scopes, otherwise nullptr.
			 */
			SymbolSharedPointer lookup( const std::string& name ) const;

			/**
			 * @brief Searches for a symbol only within the current local scope.
			 * @param name The identifier name to search for.
			 * @return The first SymbolSharedPointer if found locally, otherwise nullptr.
			 */
			SymbolSharedPointer lookupLocal( const std::string& name ) const;

			/**
			 * @brief Performs a recursive lookup for all symbols with the given name (overload set).
			 * @param name The identifier name to search for.
			 * @return A vector of all matching symbols from this scope or parent scopes.
			 */
			std::vector<SymbolSharedPointer> lookupAll( const std::string& name ) const;

			/**
			 * @brief Searches for all symbols with the given name only within the current local scope.
			 * @param name The identifier name to search for.
			 * @return A vector of all matching symbols defined locally.
			 */
			std::vector<SymbolSharedPointer> lookupAllLocal( const std::string& name ) const;

			/**
			 * @brief Gets the parent scope.
			 * @return A shared pointer to the parent Scope.
			 */
			std::shared_ptr<Scope> parent() const {
				return this->parentT;
			}

			/**
			 * @brief Retrieves the map of all symbols defined in this scope.
			 * @return A constant reference to the symbols map (vector-per-key for overload support).
			 */
			const std::unordered_map<std::string, std::vector<SymbolSharedPointer>>& symbols() const {
				return this->symbolsT;
			}

		private:

			Kind kindT;
			std::shared_ptr<Scope> parentT;
			std::unordered_map<std::string, std::vector<SymbolSharedPointer>> symbolsT;
		
	};
	
	using ScopeSharedPointer = std::shared_ptr<Scope>;
	
} // namespace uranite::semantic

#endif // end _URANITE_SEMANTIC_SCOPE_HPP_
