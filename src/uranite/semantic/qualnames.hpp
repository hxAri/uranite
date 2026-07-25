#pragma once

/**
 * @file qualnames.hpp
 * @brief Fully-qualified type name constants for identity-safe type comparisons.
 *
 * Format: "uranite.<package-path>.<TypeName>" — mirrors the module filesystem
 * layout (e.g. FLOAT = "uranite.language.float.Float" ↔ modules/language/float.urn).
 *
 * All type identity checks in the compiler must use these constants (via
 * ->qualified) rather than short names (->name) to prevent namespace
 * collisions. The typeIdentityMatch() helper in typeref.cpp handles the
 * qualified-first, name-fallback logic for cases where qualified may be
 * empty (monomorphized generics, error recovery).
 *
 * When adding a new OOP wrapper module in modules/language/, add a
 * corresponding constant here AND in the appropriate lookup set
 * (integerOopQualified, floatOopQualified, oopWrapperQualified).
 */

#include <string>
#include <unordered_set>

namespace uranite::semantic::qname {
	
	// Built-in (no module file)
	inline const std::string OBJECT = "uranite.builtin.Object";
	inline const std::string FUTURE = "uranite.builtin.Future";
	inline const std::string GENERATOR = "uranite.builtin.Generator";
	
	// Primitives — internal type names (not user-facing)
	inline const std::string PRIM_BOOL = "uranite.builtin.bool";
	inline const std::string PRIM_CHAR = "uranite.builtin.char";
	inline const std::string PRIM_STRING = "uranite.builtin.str";
	inline const std::string PRIM_VOID = "uranite.builtin.void";
	inline const std::string PRIM_ERROR = "uranite.builtin.error";
	
	// OOP wrapper classes — uranite.language.*
	inline const std::string BOOLEAN = "uranite.language.boolean.Boolean";
	inline const std::string BYTE = "uranite.language.byte.Byte";
	inline const std::string CHAR = "uranite.language.Char.Char";
	inline const std::string DOUBLE = "uranite.language.double.Double";
	inline const std::string F32 = "uranite.language.F32.F32";
	inline const std::string F64 = "uranite.language.F64.F64";
	inline const std::string FLOAT = "uranite.language.float.Float";
	inline const std::string I8 = "uranite.language.I8.I8";
	inline const std::string I16 = "uranite.language.I16.I16";
	inline const std::string I32 = "uranite.language.I32.I32";
	inline const std::string I64 = "uranite.language.I64.I64";
	inline const std::string INT = "uranite.language.int.Int";
	inline const std::string INTEGER = "uranite.language.integer.Integer";
	inline const std::string LONG = "uranite.language.long.Long";
	inline const std::string NONETYPE = "uranite.language.none.NoneType";
	inline const std::string STRING = "uranite.language.string.String";
	inline const std::string U8 = "uranite.language.U8.U8";
	inline const std::string U16 = "uranite.language.U16.U16";
	inline const std::string U32 = "uranite.language.U32.U32";
	inline const std::string U64 = "uranite.language.U64.U64";
	inline const std::string UINT = "uranite.language.uint.UInt";
	inline const std::string VOID = "uranite.language.Void.Void";
	
	// Compiler intrinsics
	inline const std::string MEMORY = "uranite.memory.memory.Memory";
	inline const std::string ARENA = "uranite.memory.arena.Arena";
	
	// Interfaces
	inline const std::string DROPER = "uranite.memory.droper.Droper";
	inline const std::string ITERABLE = "uranite.iterators.iterable.Iterable";
	inline const std::string ITERATOR = "uranite.iterators.iterator.Iterator";
	inline const std::string THROWABLE = "uranite.errors.throwable.Throwable";

	// Operator interfaces
	inline const std::string ADDABLE = "uranite.operators.addable.Addable";
	inline const std::string COMPARABLE = "uranite.operators.comparable.Comparable";
	inline const std::string DIVIDABLE = "uranite.operators.dividable.Dividable";
	inline const std::string EQUATABLE = "uranite.operators.equatable.Equatable";
	inline const std::string HASHABLE = "uranite.operators.hashable.Hashable";
	inline const std::string INDEXABLE = "uranite.operators.indexable.Indexable";
	inline const std::string MODULABLE = "uranite.operators.modulable.Modulable";
	inline const std::string MULTIPLIABLE = "uranite.operators.multipliable.Multipliable";
	inline const std::string NEGATABLE = "uranite.operators.negatable.Negatable";
	inline const std::string STRINGABLE = "uranite.operators.stringable.Stringable";
	inline const std::string SUBTRACTABLE = "uranite.operators.subtractable.Subtractable";
	
	// Collections
	inline const std::string ARGS = "uranite.collection.args.Args";
	inline const std::string KWARGS = "uranite.collection.kwargs.Kwargs";
	inline const std::string PAIR = "uranite.collection.pair.Pair";
	
	/** @brief Set of qualified names for all integer OOP wrapper types. O(1) lookup. */
	inline const std::unordered_set<std::string>& integerOopQualified() {
		static const std::unordered_set<std::string> s = {
			INT, I8, I16, I32, I64, INTEGER, LONG, BYTE,
			UINT, U8, U16, U32, U64
		};
		return s;
	}
	
	/** @brief Set of qualified names for all floating-point OOP wrapper types. O(1) lookup. */
	inline const std::unordered_set<std::string>& floatOopQualified() {
		static const std::unordered_set<std::string> s = {
			FLOAT, F32, F64, DOUBLE
		};
		return s;
	}
	
	/** @brief Set of qualified names for all OOP wrapper types (numeric + string + void + bool). O(1) lookup. */
	inline const std::unordered_set<std::string>& oopWrapperQualified() {
		static const std::unordered_set<std::string> s = {
			BOOLEAN, BYTE, CHAR, DOUBLE, F32, F64, FLOAT,
			I8, I16, I32, I64, INT, INTEGER, LONG,
			NONETYPE, STRING,
			U8, U16, U32, U64, UINT, VOID
		};
		return s;
	}
	
	/** @brief Check if qualified name is any OOP wrapper type. O(1). */
	inline bool isOopWrapper( const std::string& qualified ) {
		return oopWrapperQualified().count( qualified ) > 0;
	}
	
	/** @brief Check if qualified name is an integer OOP wrapper type. O(1). */
	inline bool isIntegerOop( const std::string& qualified ) {
		return integerOopQualified().count( qualified ) > 0;
	}
	
	/** @brief Check if qualified name is a floating-point OOP wrapper type. O(1). */
	inline bool isFloatOop( const std::string& qualified ) {
		return floatOopQualified().count( qualified ) > 0;
	}
	
	/** @brief Check if a string begins with the given prefix. O(min(n,m)). */
	inline bool startsWith( const std::string& str, const std::string& prefix ) {
		return str.size() >= prefix.size() && str.compare( 0, prefix.size(), prefix ) == 0;
	}

}
