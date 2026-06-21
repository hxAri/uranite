
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

#ifndef _URANITE_SEMANTIC_SYMBOL_HPP_
#define _URANITE_SEMANTIC_SYMBOL_HPP_

#include <memory>
#include <string>

#include "uranite/ast/node.hpp"
#include "uranite/lookup/source.hpp"
#include "uranite/semantic/typeref.hpp"

namespace uranite::semantic {
	
	/**
	 * @brief Represents a symbol within the compiler's symbol table or AST.
	 */
	struct Symbol {
		
		/**
		 * @brief Represents the various categories of symbols within the compiler's symbol table.
		 */
		enum class Kind {
			
			/** @brief A variant member within an enumeration. */
			EnumVariant,
			
			/** @brief A field or property within a structure or class. */
			Field,
			
			/** @brief A function or method definition. */
			Function,
			
			/** @brief A module or namespace container. */
			Module,
			
			/** @brief A parameter defined within a function signature. */
			Parameter,
			
			/** @brief A custom type definition (e.g., struct, class, or alias). */
			Type,
			
			/** @brief A local or global variable declaration. */
			Variable
			
		};
		
		/**
		 * @brief Tracks the ownership and borrowing state of a resource.
		 * 
		 * This structure is used to enforce memory safety rules by keeping track of
		 * active borrows, mutability constraints, and the lifecycle status of an object.
		 */
		struct Ownership {
			
			/** @brief The number of active immutable borrows for this resource. */
			int borrowCount = 0;
			
			/** @brief Indicates if a mutable borrow is currently active. */
			bool hasMutableBorrow = false;
			
			/** @brief Indicates if the resource is currently borrowed. */
			bool isBorrowed = false;
			
			/** @brief Indicates if the resource has been moved to another owner. */
			bool isMoved = false;
			
			/** @brief Indicates if the current context holds primary ownership of the resource. */
			bool isOwned = true;
			
			/**
			 * @brief Default constructor for Ownership.
			 * 
			 * Initializes a resource with default ownership state (owned, no borrows).
			 */
			Ownership() = default;
			
			/**
			 * @brief Parameterized constructor for custom ownership states.
			 * @param borrowCount The initial number of active borrows.
			 * @param hasMutableBorrow Flag for the existence of a mutable borrow.
			 * @param isOwned Flag indicating primary ownership.
			 * @param isBorrowed Flag indicating if the resource is borrowed.
			 * @param isMoved Flag indicating if the resource has been moved.
			 */
			Ownership(
				int borrowCount,
				bool hasMutableBorrow,
				bool isOwned,
				bool isBorrowed,
				bool isMoved
			) : borrowCount( borrowCount ),
				hasMutableBorrow( hasMutableBorrow ),
				isBorrowed( isBorrowed ),
				isMoved( isMoved ),
				isOwned( isOwned ) {
			}
			
		};
		
		using OwnershipSharedPointer = std::shared_ptr<Ownership>;
		
		/** @brief The access level of the symbol (e.g., Public, Private). */
		ast::AccessModifier access;
		
		/** @brief Indicates whether the symbol has been assigned an initial value. */
		bool isInitialized;
		
		/** @brief Indicates whether the symbol's value can be modified after initialization. */
		bool isMutable;
		
		/** @brief The classification of the symbol (e.g., Variable, Function, Constant). */
		Kind kind;
		
		/** @brief The identifier name of the symbol. */
		std::string name;
		
		/** @brief Shared pointer managing the ownership semantics of the symbol. */
		OwnershipSharedPointer ownership;
		
		/** @brief Shared pointer to the source code location where the symbol is defined. */
		lookup::SourceSharedPointer source;
		
		/** @brief The data type reference associated with this symbol. */
		TypeSharedPointer typeref;
		
		/**
		 * @brief Constructs a new Symbol with essential metadata.
		 * @param name The identifier name.
		 * @param kind The kind of symbol.
		 * @param typeref The type reference to be moved into the symbol.
		 * @param source The source location mapping for this symbol.
		 */
		Symbol(
			const std::string& name,
			Kind kind,
			const lookup::SourceSharedPointer& source,
			TypeSharedPointer typeref
		) : access( ast::AccessModifier::Default ),
			isInitialized( false ),
			isMutable( false ),
			kind( kind ),
			name( name ),
			ownership( std::make_shared<Ownership>() ),
			source( source ),
			typeref( std::move( typeref ) ) {
		}
		
		/**
		 * @brief Constructs a new Symbol with essential metadata.
		 * @param name The identifier name.
		 * @param kind The kind of symbol.
		 * @param typeref The type reference to be moved into the symbol.
		 * @param source The source location mapping for this symbol.
		 */
		Symbol(
			const std::string& name,
			Kind kind,
			const OwnershipSharedPointer& ownership,
			const lookup::SourceSharedPointer& source,
			TypeSharedPointer typeref
		) : access( ast::AccessModifier::Default ),
			isInitialized( false ),
			isMutable( false ),
			kind( kind ),
			name( name ),
			ownership( ownership ),
			source( source ),
			typeref( std::move( typeref ) ) {
		}
		
	};
	
	using OwnershipSharedPointer = Symbol::OwnershipSharedPointer;
	using SymbolSharedPointer = std::shared_ptr<Symbol>;
	
} // namespace uranite::semantic::symbol

#endif // end _URANITE_SEMANTIC_SYMBOL_HPP_
