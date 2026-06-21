
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

#ifndef _URANITE_TOKEN_TOKEN_HPP_
#define _URANITE_TOKEN_TOKEN_HPP_

#include <string>
#include <unordered_map>
#include <vector>

#include "uranite/lookup/source.hpp"

namespace uranite::token {
	
	enum class Type {
		
		// Keywords - Control Flow
		KeywordBreak,
		KeywordContinue,
		KeywordElse,
		KeywordElif,
		KeywordFor,
		KeywordIf,
		KeywordMatch,
		KeywordReturn,
		KeywordWhile,
		KeywordYield,
		
		// Keywords - Declarations
		KeywordClass,
		KeywordConstant,
		KeywordEnum,
		KeywordExtern,
		KeywordFrom,
		KeywordFunction,
		KeywordImplements,
		KeywordImport,
		KeywordInterface,
		KeywordMutable,
		KeywordPackage,
		KeywordStatic,
		KeywordStruct,
		KeywordType,
		
		// Keywords - Access Modifiers
		KeywordPrivate,
		KeywordProtect,
		KeywordPublic,
		
		// Keywords - Oop
		KeywordAbstract,
		KeywordDelete,
		KeywordExtends,
		KeywordFinal,
		KeywordNative,
		KeywordNew,
		KeywordOverride,
		KeywordParent,
		KeywordProperty,
		KeywordReadonly,
		KeywordSelf,
		KeywordVirtual,
		
		// Keywords - Memory & Safety
		KeywordAddressof,
		KeywordMove,
		KeywordOwn,
		KeywordReference,
		KeywordUnsafe,
		
		// Keywords - Logic
		KeywordAnd,
		KeywordNot,
		KeywordOr,
		
		// Keywords - Values
		KeywordFalse,
		KeywordNone,
		KeywordTrue,
		
		// Keywords - Error Handling
		KeywordExcept,
		KeywordFinally,
		KeywordRaise,
		KeywordRaises,
		KeywordTry,
		
		// Keywords - Other
		KeywordAs,
		KeywordAssembly,
		KeywordAsync,
		KeywordAwait,
		KeywordBacked,
		KeywordCase,
		KeywordDefer,
		KeywordIn,
		KeywordInstanceOf,
		KeywordIs,
		KeywordLambda,
		KeywordPass,
		KeywordSubclassOf,
		KeywordSwitch,
		KeywordTrait,
		KeywordUnit,
		KeywordUse,
		KeywordVolatile,
		KeywordWhere,
		
		// Keywords - Export
		KeywordExport,
		
		// Literals
		LiteralChar,
		LiteralFloat,
		LiteralInteger,
		LiteralRegex,
		LiteralString,
		
		// Identifier
		Identifier,
		
		// Operators - Arithmetic
		Decrement,      // --
		Increment,      // ++
		Minus,          // -
		Percent,        // %
		Plus,           // +
		Power,          // **
		Slash,          // /
		Star,           // *
		
		// Operators - Bitwise
		Ampersand,      // &
		Caret,          // ^
		Pipe,           // |
		ShiftLeft,     // <<
		ShiftRight,    // >>
		Tilde,          // ~
		
		// Operators - Comparison
		Equal,                  // ==
		GreaterThan,           // >
		GreaterThanEqual,     // >=
		LessThan,              // <
		LessThanEqual,         // <=
		NotEqual,              // !=
		
		// Operators - Assignment
		AmpersandAssignment,    // &=
		Assignment,              // =
		CaretAssignment,        // ^=
		MinusAssignment,        // -=
		PercentAssignment,      // %=
		PipeAssignment,         // |=
		PlusAssignment,         // +=
		ShiftLeftAssignment,   // <<=
		ShiftRightAssignment,  // >>=
		SlashAssignment,        // /=
		StarAssignment,         // *=
		
		// Punctuation
		Arrow,          // ->
		At,             // @
		Bang,           // !
		Colon,          // :
		Comma,          // ,
		Dot,            // .
		DoubleColon,   // ::
		DoubleDot,     // ..
		Ellipsis,       // ...
		FatArrow,      // =>
		Hash,           // # (Used Internally, Comments Stripped)
		Question,       // ?
		Semicolon,      // ;
		
		// Delimiters
		LeftBrace,         // {
		LeftBracket,       // [
		LeftParenthesis,   // (
		RightBrace,        // }
		RightBracket,      // ]
		RightParenthesis,  // )
		
		// Indentation
		Dedent,
		Indent,
		Newline,
		
		// Special
		Eof,
		Error
		
	};
	
	/**
	 * @brief Retrieves the master mapping of string literals to their corresponding token Types.
	 * 
	 * This lookup table is primarily used by the Lexer to efficiently distinguish 
	 * between raw identifiers and reserved language keywords.
	 * 
	 * @return const std::unordered_map<std::string, Type>& A static reference to the keyword map.
	 * 
	 */
	const std::unordered_map<std::string, Type>& keymaps();
	
	/**
	 * @brief Returns a list of matching bracket and parenthesis token types (e.g., '(', ')', '{', '}', '[', ']').
	 * 
	 * @return const std::vector<Type>& Reference to the delimiter tokens vector.
	 * 
	 */
	const std::vector<Type>& delimiters();
	
	/**
	 * @brief Returns a list of token types specialized for layout or indentation tracking (e.g., Indent, Outdent).
	 * 
	 * @return const std::vector<Type>& Reference to the indentation tokens vector.
	 * 
	 */
	const std::vector<Type>& indentations();
	
	/**
	 * @brief Returns a list of token types for access specifiers (e.g., 'public', 'private', 'protected').
	 * 
	 * @return const std::vector<Type>& Reference to the access modifiers token vector.
	 * 
	 */
	const std::vector<Type>& keywordsAccessModifiers();
	
	/**
	 * @brief Returns a list of token types used for execution branching and loops (e.g., 'if', 'else', 'while', 'return').
	 * 
	 * @return const std::vector<Type>& Reference to the control flow tokens vector.
	 * 
	 */
	const std::vector<Type>& keywordsControlFlows();
	
	/**
	 * @brief Returns a list of token types used for declarations (e.g., 'fn', 'let', 'struct', 'class').
	 * 
	 * @return const std::vector<Type>& Reference to the declaration tokens vector.
	 * 
	 */
	const std::vector<Type>& keywordsDeclarations();
	
	/**
	 * @brief Returns a list of token types used for error boundary systems (e.g., 'try', 'catch', 'throw').
	 * 
	 * @return const std::vector<Type>& Reference to the error handling tokens vector.
	 * 
	 */
	const std::vector<Type>& keywordsErrorHandling();
	
	/**
	 * @brief Returns a list of keyword-based logical token types (e.g., 'and', 'or', 'not').
	 * 
	 * @return const std::vector<Type>& Reference to the logical expression tokens vector.
	 * 
	 */
	const std::vector<Type>& keywordsLogics();
	
	/**
	 * @brief Returns a list of token types dealing with memory safety models (e.g., 'owned', 'shared', 'weakref').
	 * 
	 * @return const std::vector<Type>& Reference to the memory safety tokens vector.
	 * 
	 */
	const std::vector<Type>& keywordsMemorySafetys();
	
	/**
	 * @brief Returns a list of token types related to OOP concepts (e.g., 'extends', 'implements', 'override', 'abstract').
	 * 
	 * @return const std::vector<Type>& Reference to the object-oriented tokens vector.
	 * 
	 */
	const std::vector<Type>& keywordsOOPs();
	
	/**
	 * @brief Returns a list of reserved keyword token types for literal values (e.g., 'true', 'false', 'null').
	 * 
	 * @return const std::vector<Type>& Reference to the value keywords vector.
	 * 
	 */
	const std::vector<Type>& keywordsValues();
	
	/**
	 * @brief Returns a list of miscellaneous reserved keywords that do not fit standard categories.
	 * 
	 * @return const std::vector<Type>& Reference to the miscellaneous keywords vector.
	 * 
	 */
	const std::vector<Type>& keywordsOthers();
	
	/**
	 * @brief Returns a list of token types for raw data literals (e.g., integer, float, string, char literals).
	 * 
	 * @return const std::vector<Type>& Reference to the raw literals tokens vector.
	 * 
	 */
	const std::vector<Type>& literals();
	
	/**
	 * @brief Returns a list of token types for basic arithmetic symbols (e.g., '+', '-', '*', '/').
	 * 
	 * @return const std::vector<Type>& Reference to the arithmetic operators vector.
	 * 
	 */
	const std::vector<Type>& operatorsArithmetics();
	
	/**
	 * @brief Returns a list of token types for direct or compound assignments (e.g., '=', '+=', '-=').
	 * 
	 * @return const std::vector<Type>& Reference to the assignment operators vector.
	 * 
	 */
	const std::vector<Type>& operatorsAssignments();
	
	/**
	 * @brief Returns a list of token types for bitwise operations (e.g., '&', '|', '^', '<<', '>>').
	 * 
	 * @return const std::vector<Type>& Reference to the bitwise operators vector.
	 * 
	 */
	const std::vector<Type>& operatorsBitwises();
	
	/**
	 * @brief Returns a list of token types used for structural or value comparisons (e.g., '==', '!=', '<', '>=').
	 * 
	 * @return const std::vector<Type>& Reference to the comparison operators vector.
	 * 
	 */
	const std::vector<Type>& operatorsComparisons();
	
	/**
	 * @brief Returns a list of basic punctuation token types (e.g., ',', '.', ';', ':').
	 * 
	 * @return const std::vector<Type>& Reference to the punctuation tokens vector.
	 * 
	 */
	const std::vector<Type>& punctuations();
	
	/**
	 * @brief Returns a list of operational or compiler-internal token types (e.g., EndOfFile, Invalid).
	 * 
	 * @return const std::vector<Type>& Reference to the special internal tokens vector.
	 * 
	 */
	const std::vector<Type>& specials();
	
	/**
	 * @brief Converts a given token Type enum value into its human-readable string representation.
	 * 
	 * Heavily utilized by the diagnostic engine, AST printers, and error reporters 
	 * to render clear compilation feedback.
	 * 
	 * @param type The token type to be converted.
	 * 
	 * @return const char* A null-terminated C-string containing the name or symbol of the token type.
	 * 
	 */
	const char* toString( Type type );
	
	/**
	 * @struct Token
	 * 
	 * @brief Represents a single lexical unit (token) produced by the Lexer.
	 * 
	 * It encapsulates the token type, its precise location within the source code (for diagnostics), and its literal string value.
	 * 
	 */
	struct Token {
		
		/** @brief The classification type of the token. */
		Type type;
		
		/** @brief Shared pointer to the source location metadata (file, line, column). */
		lookup::SourceSharedPointer source;
		
		/** @brief The raw lexeme string captured from the source text. */
		std::string value;
		
		/**
		 * @brief Default constructor.
		 * Initializes the token as an End-Of-File (`Type::Eof`) marker with an empty source and null value.
		 */
		Token() :
			type( Type::Eof ),
			source( std::make_shared<lookup::Source>() ),
			value( "\0" ) {
		}
		
		/**
		 * @brief Parameterized constructor to initialize a fully-formed Token.
		 * @param type The classification category of the token.
		 * @param source The source location tracking pointer.
		 * @param value The literal text string of the token.
		 * 
		 */
		Token(
			Type type,
			const lookup::SourceSharedPointer& source,
			const std::string& value
		) : type( type ),
			source( source ),
			value( value ) {
		}
		
		/**
		 * @brief Checks if the token matches a specific type.
		 * 
		 * @param type The token type to compare against.
		 * 
		 * @return true if types match, false otherwise.
		 * 
		 */
		bool is( Type type ) const {
			return this->type == type;
		}
		
		/**
		 * @brief Checks if the token does not match a specific type.
		 * 
		 * @param type The token type to compare against.
		 * 
		 * @return true if types are different, false otherwise.
		 * 
		 */
		bool isNot( Type type ) const {
			return this->type != type;
		}
		
		/**
		 * @brief Base case for checking if the token matches one of two specific types.
		 * 
		 * @param type1 The first token type to check.
		 * @param type2 The second token type to check.
		 * 
		 * @return true if the token matches either type1 or type2.
		 * 
		 */
		bool isOneOf( Type type1, Type type2 ) const {
			return this->is( type1 ) || this->is( type2 );
		}
		
		/**
		 * @brief Variadic template function to check if the token matches any of the given types.
		 * 
		 * @tparam Ts Parameter pack representing additional token types.
		 * 
		 * @param type The first token type in the current expansion.
		 * @param types The remaining token types to match recursively.
		 * 
		 * @return true if the token matches any type in the list.
		 * 
		 */
		template<typename... Ts>
		bool isOneOf( Type type, Ts... types ) const {
			return this->is( type ) || this->isOneOf( types...);
		}
		
		/**
		 * @brief Determines if the token falls within the reserved keyword range.
		 * 
		 * @return true if the token type is classified as a keyword.
		 * 
		 */
		bool isKeyword() const {
			return this->type >= Type::KeywordBreak && this->type <= Type::KeywordExport;
		}
		
		/**
		 * @brief Determines if the token is a data literal (e.g., character, integer, or string).
		 * 
		 * @return true if the token type is classified as a literal.
		 * 
		 */
		bool isLiteral() const {
			return this->type >= Type::LiteralChar && this->type <= Type::LiteralString;
		}
		
		/**
		 * @brief Determines if the token is any operational symbol (arithmetic, bitwise, assignment, etc.).
		 * 
		 * @return true if the token type is within the overall operator range.
		 * 
		 */
		bool isOperator() const {
			return this->type >= Type::Decrement && this->type <= Type::StarAssignment;
		}
		
		/**
		 * @brief Determines if the token is a mutation or assignment operator (e.g., `=`, `+=`, `*=`).
		 * 
		 * @return true if the token type is an assignment operator.
		 * 
		 */
		bool isAssignment() const {
			return this->type >= Type::Assignment && this->type <= Type::StarAssignment;
		}
		
		/**
		 * @brief Determines if the token is a logical comparison operator (e.g., `==`, `!=`).
		 * 
		 * @return true if the token type is a comparison operator.
		 * 
		 */
		bool isComparisonOp() const {
			return this->type >= Type::Equal && this->type <= Type::NotEqual;
		}
		
		/**
		 * @brief Determines if the token specifies access control visibility (e.g., `public`, `private`).
		 * 
		 * @return true if the token type is an access modifier keyword.
		 * 
		 */
		bool isAccessModifier() const {
			return this->type >= Type::KeywordPrivate && this->type <= Type::KeywordPublic;
		}
		
	};
	
} // namespace uranite::token

#endif // end _URANITE_TOKEN_TOKEN_HPP_
