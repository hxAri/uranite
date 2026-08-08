#pragma once

/**
 * @file qualnames.hpp
 * @brief Centralized registry of all entity names used across the compiler.
 *
 * Every type name, method name, identifier, and qualified name comparison
 * in codegen, semantic analysis, MIR, and HIR must reference constants
 * from this file — never hardcoded strings.
 *
 * Organized as nested namespaces mirroring the language entity hierarchy:
 *   Identifier   — reserved identifiers (self, None, true, false)
 *   Functions     — free functions (main) with their parameter names
 *   Primitives    — internal primitive type qualified names
 *   Classes       — user-facing classes with qualified names, short names, methods
 *   Interfaces    — operator/protocol interfaces with qualified names and methods
 *   OperatorMapping — centralized operator-to-interface dispatch table
 */

#include <string>
#include <unordered_set>

#include "uranite/token/token.hpp"

namespace uranite::semantic::qualname {
	
	namespace identifier {
		static constexpr const char* Self = "self";
		static constexpr const char* SelfType = "Self";
		static constexpr const char* Meta = "Meta";
		static constexpr const char* Callable = "Callable";
		static constexpr const char* Array = "Array";
		static constexpr const char* Optional = "Optional";
		static constexpr const char* None = "None";
		static constexpr const char* TrueLit = "true";
		static constexpr const char* FalseLit = "false";
	};
	
	namespace fields {
		static constexpr const char* Value = "value";
		static constexpr const char* Name = "name";
		static constexpr const char* Key = "key";
		static constexpr const char* File = "file";
		static constexpr const char* Line = "line";
		static constexpr const char* Traceback = "traceback";
		static constexpr const char* ModuleName = "moduleName";
		static constexpr const char* FunctionName = "functionName";
		static constexpr const char* Other = "other";
		static constexpr const char* Delimiter = "delimiter";
	};
	
	namespace functions {
		namespace main {
			static constexpr const char* Name = "main";
			namespace params {
				static constexpr const char* Argc = "argc";
				static constexpr const char* Argv = "argv";
			};
		};
		static constexpr const char* Puts = "puts";
	};
	
	namespace modules {
		static constexpr const char* Uranite = "uranite";
		static constexpr const char* Native = "native";
		static constexpr const char* FfiPathSegment = "/ffi/";
		static constexpr const char* FfiPackagePrefix = "uranite.ffi";
	};
	
	namespace typeparams {
		static constexpr const char* K = "K";
		static constexpr const char* V = "V";
	};
	
	namespace primitives {
		static constexpr const char* Package = "uranite.builtin";
		namespace names {
			static constexpr const char* Bool = "bool";
			static constexpr const char* Char = "char";
			static constexpr const char* Str = "str";
			static constexpr const char* Void = "void";
			static constexpr const char* F32 = "f32";
			static constexpr const char* F64 = "f64";
			static constexpr const char* I8 = "i8";
			static constexpr const char* I16 = "i16";
			static constexpr const char* I32 = "i32";
			static constexpr const char* I64 = "i64";
			static constexpr const char* U8 = "u8";
			static constexpr const char* U16 = "u16";
			static constexpr const char* U32 = "u32";
			static constexpr const char* U64 = "u64";
		};
		static inline const std::string Bool = "uranite.builtin.bool";
		static inline const std::string Char = "uranite.builtin.char";
		static inline const std::string String = "uranite.builtin.str";
		static inline const std::string Void = "uranite.builtin.void";
		static inline const std::string Error = "uranite.builtin.error";
		static inline const std::string F32 = "uranite.builtin.f32";
		static inline const std::string F64 = "uranite.builtin.f64";
		static inline const std::string I8 = "uranite.builtin.i8";
		static inline const std::string I16 = "uranite.builtin.i16";
		static inline const std::string I32 = "uranite.builtin.i32";
		static inline const std::string I64 = "uranite.builtin.i64";
		static inline const std::string U8 = "uranite.builtin.u8";
		static inline const std::string U16 = "uranite.builtin.u16";
		static inline const std::string U32 = "uranite.builtin.u32";
		static inline const std::string U64 = "uranite.builtin.u64";
	};
	
	namespace classes {
		namespace object {
			
			static inline const std::string Qualified = "uranite.builtin.Object";
			static constexpr const char* Name = "Object";
			namespace methods {
				static constexpr const char* ToString = "toString";
				static constexpr const char* HashCode = "hashCode";
				static constexpr const char* Equals = "equals";
				static constexpr const char* GetValue = "getValue";
				static constexpr const char* Value = "value";
				static constexpr const char* BitwiseAnd = "bitwiseAnd";
				static constexpr const char* BitwiseOr = "bitwiseOr";
				static constexpr const char* BitwiseXor = "bitwiseXor";
				static constexpr const char* ShiftLeft = "shiftLeft";
				static constexpr const char* ShiftRight = "shiftRight";
				static constexpr const char* BitwiseNot = "bitwiseNot";
				static constexpr const char* Abs = "abs";
				static constexpr const char* CompareTo = "compareTo";
				static constexpr const char* GreaterThanOrEqual = "greaterThanOrEqual";
				static constexpr const char* Gte = "gte";
				static constexpr const char* LessThanOrEqual = "lessThanOrEqual";
				static constexpr const char* Lte = "lte";
				static constexpr const char* Min = "min";
				static constexpr const char* Max = "max";
				static constexpr const char* ToI64 = "toI64";
				static constexpr const char* ToInt = "toInt";
				static constexpr const char* ToLong = "toLong";
				static constexpr const char* ToI32 = "toI32";
				static constexpr const char* ToFloat = "toFloat";
				static constexpr const char* ToDouble = "toDouble";
				static constexpr const char* ToF64 = "toF64";
				static constexpr const char* IsInfinite = "isInfinite";
				static constexpr const char* IsNan = "isNaN";
				static constexpr const char* IsFinite = "isFinite";
				static constexpr const char* Floor = "floor";
				static constexpr const char* Ceil = "ceil";
				static constexpr const char* Round = "round";
				static constexpr const char* Sqrt = "sqrt";
				static constexpr const char* Power = "power";
				static constexpr const char* IsZero = "isZero";
				static constexpr const char* IsPositive = "isPositive";
				static constexpr const char* IsNegative = "isNegative";
				static constexpr const char* LogicalAnd = "logicalAnd";
				static constexpr const char* LogicalOr = "logicalOr";
				static constexpr const char* Hash = "hash";
				static constexpr const char* IsAlpha = "isAlpha";
				static constexpr const char* IsDigit = "isDigit";
				static constexpr const char* IsAlphanumeric = "isAlphanumeric";
				static constexpr const char* IsWhitespace = "isWhitespace";
			};
		};
		namespace future {
			static inline const std::string Qualified = "uranite.builtin.Future";
			static constexpr const char* Name = "Future";
		};
		namespace generator {
			static inline const std::string Qualified = "uranite.collection.generator.Generator";
			static constexpr const char* Name = "Generator";
		};
		namespace boolean {
			static inline const std::string Qualified = "uranite.language.boolean.Boolean";
			static constexpr const char* Package = "uranite.language.boolean";
			static constexpr const char* Name = "Boolean";
		};
		namespace byte {
			static inline const std::string Qualified = "uranite.language.byte.Byte";
			static constexpr const char* Package = "uranite.language.byte";
			static constexpr const char* Name = "Byte";
		};
		namespace Char {
			static inline const std::string Qualified = "uranite.language.char.Char";
			static constexpr const char* Package = "uranite.language.char";
			static constexpr const char* Name = "Char";
		};
		namespace Double {
			static inline const std::string Qualified = "uranite.language.double.Double";
			static constexpr const char* Package = "uranite.language.double";
			static constexpr const char* Name = "Double";
		};
		namespace f32 {
			static inline const std::string Qualified = "uranite.language.f32.F32";
			static constexpr const char* Package = "uranite.language.f32";
			static constexpr const char* Name = "F32";
		};
		namespace f64 {
			static inline const std::string Qualified = "uranite.language.f64.F64";
			static constexpr const char* Package = "uranite.language.f64";
			static constexpr const char* Name = "F64";
		};
		namespace Float {
			static inline const std::string Qualified = "uranite.language.float.Float";
			static constexpr const char* Package = "uranite.language.float";
			static constexpr const char* Name = "Float";
		};
		namespace i8 {
			static inline const std::string Qualified = "uranite.language.i8.I8";
			static constexpr const char* Package = "uranite.language.i8";
			static constexpr const char* Name = "I8";
		};
		namespace i16 {
			static inline const std::string Qualified = "uranite.language.i16.I16";
			static constexpr const char* Package = "uranite.language.i16";
			static constexpr const char* Name = "I16";
		};
		namespace i32 {
			static inline const std::string Qualified = "uranite.language.i32.I32";
			static constexpr const char* Package = "uranite.language.i32";
			static constexpr const char* Name = "I32";
		};
		namespace i64 {
			static inline const std::string Qualified = "uranite.language.i64.I64";
			static constexpr const char* Package = "uranite.language.i64";
			static constexpr const char* Name = "I64";
		};
		namespace Int {
			static inline const std::string Qualified = "uranite.language.int.Int";
			static constexpr const char* Package = "uranite.language.int";
			static constexpr const char* Name = "Int";
		};
		namespace integer {
			static inline const std::string Qualified = "uranite.language.integer.Integer";
			static constexpr const char* Package = "uranite.language.integer";
			static constexpr const char* Name = "Integer";
		};
		namespace Long {
			static inline const std::string Qualified = "uranite.language.long.Long";
			static constexpr const char* Package = "uranite.language.long";
			static constexpr const char* Name = "Long";
		};
		namespace nonetype {
			static inline const std::string Qualified = "uranite.language.none.NoneType";
			static constexpr const char* Package = "uranite.language.none";
			static constexpr const char* Name = "NoneType";
		};
		namespace string {
			static inline const std::string Qualified = "uranite.language.string.String";
			static constexpr const char* Package = "uranite.language.string";
			static constexpr const char* Name = "String";
			namespace methods {
				static constexpr const char* Length = "length";
				static constexpr const char* Contains = "contains";
				static constexpr const char* Equals = "equals";
				static constexpr const char* ToString = "toString";
				static constexpr const char* CharCodeAt = "charCodeAt";
				static constexpr const char* Substring = "substring";
				static constexpr const char* IsEmpty = "isEmpty";
				static constexpr const char* Concat = "concat";
				static constexpr const char* StartsWith = "startsWith";
				static constexpr const char* EndsWith = "endsWith";
				static constexpr const char* IndexOf = "indexOf";
				static constexpr const char* CharAt = "charAt";
				static constexpr const char* ToUpper = "toUpper";
				static constexpr const char* ToLower = "toLower";
				static constexpr const char* Trim = "trim";
				static constexpr const char* Replace = "replace";
				static constexpr const char* Split = "split";
				static constexpr const char* Format = "format";
			};
		};
		namespace u8 {
			static inline const std::string Qualified = "uranite.language.u8.U8";
			static constexpr const char* Package = "uranite.language.u8";
			static constexpr const char* Name = "U8";
		};
		namespace u16 {
			static inline const std::string Qualified = "uranite.language.u16.U16";
			static constexpr const char* Package = "uranite.language.u16";
			static constexpr const char* Name = "U16";
		};
		namespace u32 {
			static inline const std::string Qualified = "uranite.language.u32.U32";
			static constexpr const char* Package = "uranite.language.u32";
			static constexpr const char* Name = "U32";
		};
		namespace u64 {
			static inline const std::string Qualified = "uranite.language.u64.U64";
			static constexpr const char* Package = "uranite.language.u64";
			static constexpr const char* Name = "U64";
		};
		namespace uint {
			static inline const std::string Qualified = "uranite.language.uint.UInt";
			static constexpr const char* Package = "uranite.language.uint";
			static constexpr const char* Name = "UInt";
		};
		namespace Void {
			static inline const std::string Qualified = "uranite.language.void.Void";
			static constexpr const char* Package = "uranite.language.void";
			static constexpr const char* Name = "Void";
		};
		namespace memory {
			static inline const std::string Qualified = "uranite.memory.memory.Memory";
			static constexpr const char* Name = "Memory";
			namespace methods {
				static constexpr const char* Get = "get";
				static constexpr const char* Set = "set";
				static constexpr const char* Free = "free";
				static constexpr const char* CopyTo = "copyTo";
			};
		};
		namespace arena {
			static inline const std::string Qualified = "uranite.memory.arena.Arena";
			static constexpr const char* Name = "Arena";
			namespace methods {
				static constexpr const char* Alloc = "alloc";
				static constexpr const char* FreeAll = "freeAll";
				static constexpr const char* Destroy = "destroy";
				static constexpr const char* Count = "count";
				static constexpr const char* Capacity = "capacity";
			};
		};
		namespace args {
			static inline const std::string Qualified = "uranite.collection.args.Args";
			static constexpr const char* Name = "Args";
			static constexpr const char* Prefix = "Args<";
		};
		namespace kwargs {
			static inline const std::string Qualified = "uranite.collection.kwargs.Kwargs";
			static constexpr const char* Name = "Kwargs";
		};
		namespace pair {
			static inline const std::string Qualified = "uranite.collection.pair.Pair";
			static constexpr const char* Name = "Pair";
		};
		namespace arraylist {
			static inline const std::string Qualified = "uranite.collection.array-list.ArrayList";
			static constexpr const char* Name = "ArrayList";
		};
		namespace hashmap {
			static inline const std::string Qualified = "uranite.collection.hash-map.HashMap";
			static constexpr const char* Name = "HashMap";
		};
		namespace hashset {
			static inline const std::string Qualified = "uranite.collection.hash-set.HashSet";
			static constexpr const char* Name = "HashSet";
		};
		namespace tuple {
			static inline const std::string Qualified = "uranite.collection.tuple.Tuple";
			static constexpr const char* Name = "Tuple";
		};
		namespace error {
			static inline const std::string Qualified = "uranite.errors.error.Error";
			static constexpr const char* Name = "Error";
		};
		namespace exception {
			static inline const std::string Qualified = "uranite.errors.exception.Exception";
			static constexpr const char* Name = "Exception";
		};
		namespace warning {
			static inline const std::string Qualified = "uranite.errors.warning.Warning";
			static constexpr const char* Name = "Warning";
		};
		namespace traceback {
			static inline const std::string Qualified = "uranite.errors.traceback.Traceback";
			static constexpr const char* Name = "Traceback";
		};
		namespace arithmeticerror {
			static inline const std::string Qualified = "uranite.math.errors.ArithmeticError";
			static constexpr const char* Name = "ArithmeticError";
		};
		namespace zerodivisionerror {
			static inline const std::string Qualified = "uranite.math.errors.ZeroDivisionError";
			static constexpr const char* Name = "ZeroDivisionError";
		};
		namespace overflowerror {
			static inline const std::string Qualified = "uranite.math.errors.OverflowError";
			static constexpr const char* Name = "OverflowError";
		};
		namespace underflowerror {
			static inline const std::string Qualified = "uranite.math.errors.UnderflowError";
			static constexpr const char* Name = "UnderflowError";
		};
	};
	
	namespace interfaces {
		namespace addable {
			static inline const std::string Qualified = "uranite.operators.addable.Addable";
			static constexpr const char* Name = "Addable";
			namespace methods {
				static constexpr const char* Add = "add";
			};
		};
		namespace subtractable {
			static inline const std::string Qualified = "uranite.operators.subtractable.Subtractable";
			static constexpr const char* Name = "Subtractable";
			namespace methods {
				static constexpr const char* Subtract = "subtract";
				static constexpr const char* Sub = "sub";
			};
		};
		namespace multipliable {
			static inline const std::string Qualified = "uranite.operators.multipliable.Multipliable";
			static constexpr const char* Name = "Multipliable";
			namespace methods {
				static constexpr const char* Multiply = "multiply";
				static constexpr const char* Mul = "mul";
			};
		};
		namespace dividable {
			static inline const std::string Qualified = "uranite.operators.dividable.Dividable";
			static constexpr const char* Name = "Dividable";
			namespace methods {
				static constexpr const char* Divide = "divide";
				static constexpr const char* Div = "div";
			};
		};
		namespace modulable {
			static inline const std::string Qualified = "uranite.operators.modulable.Modulable";
			static constexpr const char* Name = "Modulable";
			namespace methods {
				static constexpr const char* Modulo = "modulo";
				static constexpr const char* Mod = "mod";
				static constexpr const char* Remainder = "remainder";
			};
		};
		namespace equatable {
			static inline const std::string Qualified = "uranite.operators.equatable.Equatable";
			static constexpr const char* Name = "Equatable";
			namespace methods {
				static constexpr const char* Equals = "equals";
				static constexpr const char* NotEquals = "notEquals";
			};
		};
		namespace comparable {
			static inline const std::string Qualified = "uranite.operators.comparable.Comparable";
			static constexpr const char* Name = "Comparable";
			namespace methods {
				static constexpr const char* LessThan = "lessThan";
				static constexpr const char* GreaterThan = "greaterThan";
				static constexpr const char* LessOrEqual = "lessOrEqual";
				static constexpr const char* GreaterOrEqual = "greaterOrEqual";
				static constexpr const char* Lt = "lt";
				static constexpr const char* Gt = "gt";
			};
		};
		namespace negatable {
			static inline const std::string Qualified = "uranite.operators.negatable.Negatable";
			static constexpr const char* Name = "Negatable";
			namespace methods {
				static constexpr const char* Negate = "negate";
				static constexpr const char* Neg = "neg";
			};
		};
		namespace stringable {
			static inline const std::string Qualified = "uranite.operators.stringable.Stringable";
			static constexpr const char* Name = "Stringable";
			namespace methods {
				static constexpr const char* ToString = "toString";
			};
		};
		namespace hashable {
			static inline const std::string Qualified = "uranite.operators.hashable.Hashable";
			static constexpr const char* Name = "Hashable";
			namespace methods {
				static constexpr const char* HashCode = "hashCode";
			};
		};
		namespace indexable {
			static inline const std::string Qualified = "uranite.operators.indexable.Indexable";
			static constexpr const char* Name = "Indexable";
			namespace methods {
				static constexpr const char* Get = "get";
			};
		};
		namespace iterable {
			static inline const std::string Qualified = "uranite.iterators.iterable.Iterable";
			static constexpr const char* Name = "Iterable";
			namespace methods {
				static constexpr const char* Iterator = "iterator";
			};
		};
		namespace iterator {
			static inline const std::string Qualified = "uranite.iterators.iterator.Iterator";
			static constexpr const char* Name = "Iterator";
			namespace methods {
				static constexpr const char* Has = "has";
				static constexpr const char* Next = "next";
			};
		};
		namespace droper {
			static inline const std::string Qualified = "uranite.memory.droper.Droper";
			static constexpr const char* Name = "Droper";
			namespace methods {
				static constexpr const char* Drop = "drop";
			};
		};
		namespace throwable {
			static inline const std::string Qualified = "uranite.errors.throwable.Throwable";
			static constexpr const char* Name = "Throwable";
		};
	};
	
	struct OperatorMapping {
		int tokenType;
		const std::string& interfaceQualified;
		const char* methodName;
		const char* interfaceName;
		bool negateResult;
	};
	
	inline const OperatorMapping ArithmeticMappings[] = {
		{ ( int ) token::Type::Plus,             interfaces::addable::Qualified,       interfaces::addable::methods::Add,              interfaces::addable::Name,       false },
		{ ( int ) token::Type::Minus,            interfaces::subtractable::Qualified,   interfaces::subtractable::methods::Subtract,    interfaces::subtractable::Name,  false },
		{ ( int ) token::Type::Star,             interfaces::multipliable::Qualified,   interfaces::multipliable::methods::Multiply,    interfaces::multipliable::Name,  false },
		{ ( int ) token::Type::Slash,            interfaces::dividable::Qualified,      interfaces::dividable::methods::Divide,         interfaces::dividable::Name,     false },
		{ ( int ) token::Type::Percent,          interfaces::modulable::Qualified,      interfaces::modulable::methods::Modulo,         interfaces::modulable::Name,     false },
	};
	
	inline const OperatorMapping FullOperatorMappings[] = {
		{ ( int ) token::Type::Plus,             interfaces::addable::Qualified,       interfaces::addable::methods::Add,                     interfaces::addable::Name,       false },
		{ ( int ) token::Type::Minus,            interfaces::subtractable::Qualified,   interfaces::subtractable::methods::Subtract,            interfaces::subtractable::Name,  false },
		{ ( int ) token::Type::Star,             interfaces::multipliable::Qualified,   interfaces::multipliable::methods::Multiply,            interfaces::multipliable::Name,  false },
		{ ( int ) token::Type::Slash,            interfaces::dividable::Qualified,      interfaces::dividable::methods::Divide,                 interfaces::dividable::Name,     false },
		{ ( int ) token::Type::Percent,          interfaces::modulable::Qualified,      interfaces::modulable::methods::Modulo,                 interfaces::modulable::Name,     false },
		{ ( int ) token::Type::Equal,            interfaces::equatable::Qualified,      interfaces::equatable::methods::Equals,                 interfaces::equatable::Name,     false },
		{ ( int ) token::Type::NotEqual,         interfaces::equatable::Qualified,      interfaces::equatable::methods::Equals,                 interfaces::equatable::Name,     true  },
		{ ( int ) token::Type::LessThan,         interfaces::comparable::Qualified,     interfaces::comparable::methods::LessThan,              interfaces::comparable::Name,    false },
		{ ( int ) token::Type::GreaterThan,      interfaces::comparable::Qualified,     interfaces::comparable::methods::GreaterThan,           interfaces::comparable::Name,    false },
		{ ( int ) token::Type::LessThanEqual,    interfaces::comparable::Qualified,     interfaces::comparable::methods::LessOrEqual,          interfaces::comparable::Name,    false },
		{ ( int ) token::Type::GreaterThanEqual, interfaces::comparable::Qualified,     interfaces::comparable::methods::GreaterOrEqual,       interfaces::comparable::Name,    false },
	};
	
	// Backward-compatible aliases for existing code
	inline const std::string& Object = classes::object::Qualified;
	inline const std::string& Future = classes::future::Qualified;
	inline const std::string& Generator = classes::generator::Qualified;
	
	inline const std::string& PrimBool = primitives::Bool;
	inline const std::string& PrimChar = primitives::Char;
	inline const std::string& PrimString = primitives::String;
	inline const std::string& PrimVoid = primitives::Void;
	inline const std::string& PrimError = primitives::Error;
	
	inline const std::string& Boolean = classes::boolean::Qualified;
	inline const std::string& Byte = classes::byte::Qualified;
	inline const std::string& Char = classes::Char::Qualified;
	inline const std::string& Double = classes::Double::Qualified;
	inline const std::string& F32 = classes::f32::Qualified;
	inline const std::string& F64 = classes::f64::Qualified;
	inline const std::string& Float = classes::Float::Qualified;
	inline const std::string& I8 = classes::i8::Qualified;
	inline const std::string& I16 = classes::i16::Qualified;
	inline const std::string& I32 = classes::i32::Qualified;
	inline const std::string& I64 = classes::i64::Qualified;
	inline const std::string& Int = classes::Int::Qualified;
	inline const std::string& Integer = classes::integer::Qualified;
	inline const std::string& Long = classes::Long::Qualified;
	inline const std::string& NoneType = classes::nonetype::Qualified;
	inline const std::string& String = classes::string::Qualified;
	inline const std::string& U8 = classes::u8::Qualified;
	inline const std::string& U16 = classes::u16::Qualified;
	inline const std::string& U32 = classes::u32::Qualified;
	inline const std::string& U64 = classes::u64::Qualified;
	inline const std::string& UInt = classes::uint::Qualified;
	inline const std::string& Void = classes::Void::Qualified;
	
	inline const std::string& Memory = classes::memory::Qualified;
	inline const std::string& Arena = classes::arena::Qualified;
	
	inline const std::string& Droper = interfaces::droper::Qualified;
	inline const std::string& Iterable = interfaces::iterable::Qualified;
	inline const std::string& Iterator = interfaces::iterator::Qualified;
	inline const std::string& Throwable = interfaces::throwable::Qualified;
	
	inline const std::string& Addable = interfaces::addable::Qualified;
	inline const std::string& Comparable = interfaces::comparable::Qualified;
	inline const std::string& Dividable = interfaces::dividable::Qualified;
	inline const std::string& Equatable = interfaces::equatable::Qualified;
	inline const std::string& Hashable = interfaces::hashable::Qualified;
	inline const std::string& Indexable = interfaces::indexable::Qualified;
	inline const std::string& Modulable = interfaces::modulable::Qualified;
	inline const std::string& Multipliable = interfaces::multipliable::Qualified;
	inline const std::string& Negatable = interfaces::negatable::Qualified;
	inline const std::string& Stringable = interfaces::stringable::Qualified;
	inline const std::string& Subtractable = interfaces::subtractable::Qualified;
	
	inline const std::string& Args = classes::args::Qualified;
	inline const std::string& Kwargs = classes::kwargs::Qualified;
	inline const std::string& Pair = classes::pair::Qualified;
	
	inline const std::unordered_set<std::string>& integerOopQualified() {
		static const std::unordered_set<std::string> qualified = {
			Int, I8, I16, I32, I64, Integer, Long, Byte,
			UInt, U8, U16, U32, U64
		};
		return qualified;
	}
	
	inline const std::unordered_set<std::string>& floatOopQualified() {
		static const std::unordered_set<std::string> qualified = {
			Float, F32, F64, Double
		};
		return qualified;
	}
	
	inline const std::unordered_set<std::string>& oopWrapperQualified() {
		static const std::unordered_set<std::string> qualified = {
			Boolean, Byte, Char, Double, F32, F64, Float,
			I8, I16, I32, I64, Int, Integer, Long,
			NoneType, String,
			U8, U16, U32, U64, UInt, Void
		};
		return qualified;
	}
	
	inline bool isOopWrapper( const std::string& qualified ) {
		return oopWrapperQualified().count( qualified ) > 0;
	}
	
	inline bool isIntegerOop( const std::string& qualified ) {
		return integerOopQualified().count( qualified ) > 0;
	}
	
	inline bool isFloatOop( const std::string& qualified ) {
		return floatOopQualified().count( qualified ) > 0;
	}
	
	inline bool startsWith( const std::string& str, const std::string& prefix ) {
		return str.size() >= prefix.size() && str.compare( 0, prefix.size(), prefix ) == 0;
	}
	
	inline const std::unordered_set<std::string>& errorHierarchyNames() {
		static const std::unordered_set<std::string> hierarchies = {
			classes::error::Name,
			classes::exception::Name,
			classes::warning::Name,
			interfaces::throwable::Name
		};
		return hierarchies;
	}
	
	inline const std::unordered_set<std::string>& builtinIdentifiers() {
		static const std::unordered_set<std::string> identifiers = {
			classes::i64::Name, classes::i32::Name, classes::i16::Name, classes::i8::Name,
			classes::u64::Name, classes::u32::Name, classes::u16::Name, classes::u8::Name,
			classes::Int::Name, classes::uint::Name, classes::Float::Name, classes::Double::Name,
			classes::string::Name, classes::boolean::Name, classes::Char::Name, classes::Void::Name,
			classes::object::Name, classes::memory::Name, classes::byte::Name, "Bool",
			classes::args::Name, classes::kwargs::Name, classes::future::Name, classes::generator::Name,
			classes::error::Name, classes::exception::Name, classes::warning::Name,
			interfaces::throwable::Name, classes::traceback::Name,
			classes::arithmeticerror::Name, classes::zerodivisionerror::Name,
			classes::overflowerror::Name, classes::underflowerror::Name,
			identifier::None, identifier::TrueLit, identifier::FalseLit
		};
		return identifiers;
	}

}
