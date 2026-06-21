
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

#include "uranite/token/token.hpp"

namespace uranite::token {
	
	const std::unordered_map<std::string,Type>& keymaps() {
		static const std::unordered_map<std::string,Type> maps({
			{ "abstract", Type::KeywordAbstract },
			{ "addressof", Type::KeywordAddressof },
			{ "and", Type::KeywordAnd },
			{ "as", Type::KeywordAs },
			{ "asm", Type::KeywordAssembly },
			{ "async", Type::KeywordAsync },
			{ "await", Type::KeywordAwait },
			{ "backed", Type::KeywordBacked },
			{ "break", Type::KeywordBreak },
			{ "case", Type::KeywordCase },
			{ "class", Type::KeywordClass },
			{ "const", Type::KeywordConstant },
			{ "continue", Type::KeywordContinue },
			{ "defer", Type::KeywordDefer },
			{ "delete", Type::KeywordDelete },
			{ "else", Type::KeywordElse },
			{ "elif", Type::KeywordElif },
			{ "enum", Type::KeywordEnum },
			{ "except", Type::KeywordExcept },
			{ "export", Type::KeywordExport },
			{ "extends", Type::KeywordExtends },
			{ "extern", Type::KeywordExtern },
			{ "False", Type::KeywordFalse },
			{ "final", Type::KeywordFinal },
			{ "finally", Type::KeywordFinally },
			{ "for", Type::KeywordFor },
			{ "from", Type::KeywordFrom },
			{ "function", Type::KeywordFunction },
			{ "if", Type::KeywordIf },
			{ "implements", Type::KeywordImplements },
			{ "import", Type::KeywordImport },
			{ "in", Type::KeywordIn },
			{ "instanceof", Type::KeywordInstanceOf },
			{ "interface", Type::KeywordInterface },
			{ "is", Type::KeywordIs },
			{ "lambda", Type::KeywordLambda },
			{ "match", Type::KeywordMatch },
			{ "move", Type::KeywordMove },
			{ "mut", Type::KeywordMutable },
			{ "native", Type::KeywordNative },
			{ "new", Type::KeywordNew },
			{ "None", Type::KeywordNone },
			{ "not", Type::KeywordNot },
			{ "or", Type::KeywordOr },
			{ "override", Type::KeywordOverride },
			{ "own", Type::KeywordOwn },
			{ "package", Type::KeywordPackage },
			{ "parent", Type::KeywordParent },
			{ "pass", Type::KeywordPass },
			{ "private", Type::KeywordPrivate },
			{ "property", Type::KeywordProperty },
			{ "protect", Type::KeywordProtect },
			{ "public", Type::KeywordPublic },
			{ "raise", Type::KeywordRaise },
			{ "raises", Type::KeywordRaises },
			{ "readonly", Type::KeywordReadonly },
			{ "Readonly", Type::KeywordReadonly },
			{ "reference", Type::KeywordReference },
			{ "return", Type::KeywordReturn },
			{ "self", Type::KeywordSelf },
			{ "static", Type::KeywordStatic },
			{ "struct", Type::KeywordStruct },
			{ "subclassof", Type::KeywordSubclassOf },
			{ "switch", Type::KeywordSwitch },
			{ "trait", Type::KeywordTrait },
			{ "True", Type::KeywordTrue },
			{ "try", Type::KeywordTry },
			{ "type", Type::KeywordType },
			{ "unit", Type::KeywordUnit },
			{ "unsafe", Type::KeywordUnsafe },
			{ "use", Type::KeywordUse },
			{ "virtual", Type::KeywordVirtual },
			{ "volatile", Type::KeywordVolatile },
			{ "where", Type::KeywordWhere },
			{ "while", Type::KeywordWhile },
			{ "yield", Type::KeywordYield }
		});
		return maps;
	}
	
	const std::vector<Type>& keywordsControlFlows() {
		static const std::vector<Type> types({
			Type::KeywordBreak,
			Type::KeywordContinue,
			Type::KeywordElse,
			Type::KeywordElif,
			Type::KeywordFor,
			Type::KeywordIf,
			Type::KeywordMatch,
			Type::KeywordReturn,
			Type::KeywordWhile,
			Type::KeywordYield
		});
		return types;
	}
	
	const std::vector<Type>& keywordsDeclarations() {
		static const std::vector<Type> types({
			Type::KeywordClass,
			Type::KeywordConstant,
			Type::KeywordEnum,
			Type::KeywordExtern,
			Type::KeywordFrom,
			Type::KeywordFunction,
			Type::KeywordImplements,
			Type::KeywordImport,
			Type::KeywordInterface,
			Type::KeywordMutable,
			Type::KeywordPackage,
			Type::KeywordStatic,
			Type::KeywordStruct,
			Type::KeywordType
		});
		return types;
	}
	
	const std::vector<Type>& keywordsAccessModifiers() {
		static const std::vector<Type> types({
			Type::KeywordPrivate,
			Type::KeywordProtect,
			Type::KeywordPublic
		});
		return types;
	}
	
	const std::vector<Type>& keywordsOOPs() {
		static const std::vector<Type> types({
			Type::KeywordAbstract,
			Type::KeywordDelete,
			Type::KeywordExtends,
			Type::KeywordFinal,
			Type::KeywordNative,
			Type::KeywordNew,
			Type::KeywordOverride,
			Type::KeywordParent,
			Type::KeywordProperty,
			Type::KeywordReadonly,
			Type::KeywordSelf,
			Type::KeywordVirtual
		});
		return types;
	}
	
	const std::vector<Type>& keywordsMemorySafetys() {
		static const std::vector<Type> types({
			Type::KeywordMove,
			Type::KeywordOwn,
			Type::KeywordReference,
			Type::KeywordUnsafe
		});
		return types;
	}
	
	const std::vector<Type>& keywordsLogics() {
		static const std::vector<Type> types({
			Type::KeywordAnd,
			Type::KeywordNot,
			Type::KeywordOr
		});
		return types;
	}
	
	const std::vector<Type>& keywordsValues() {
		static const std::vector<Type> types({
			Type::KeywordFalse,
			Type::KeywordNone,
			Type::KeywordTrue
		});
		return types;
	}
	
	const std::vector<Type>& keywordsErrorHandling() {
		static const std::vector<Type> types({
			Type::KeywordExcept,
			Type::KeywordFinally,
			Type::KeywordRaise,
			Type::KeywordRaises,
			Type::KeywordTry
		});
		return types;
	}
	
	const std::vector<Type>& keywordsOthers() {
		static const std::vector<Type> types({
			Type::KeywordAs,
			Type::KeywordAssembly,
			Type::KeywordAsync,
			Type::KeywordAwait,
			Type::KeywordBacked,
			Type::KeywordCase,
			Type::KeywordDefer,
			Type::KeywordExport,
			Type::KeywordIn,
			Type::KeywordInstanceOf,
			Type::KeywordIs,
			Type::KeywordLambda,
			Type::KeywordPass,
			Type::KeywordSubclassOf,
			Type::KeywordSwitch,
			Type::KeywordTrait,
			Type::KeywordUnit,
			Type::KeywordUse,
			Type::KeywordVolatile,
			Type::KeywordWhere
		});
		return types;
	}
	
	const std::vector<Type>& literals() {
		static const std::vector<Type> types({
			Type::LiteralChar,
			Type::LiteralFloat,
			Type::LiteralInteger,
			Type::LiteralString
		});
		return types;
	}
	
	const std::vector<Type>& operatorsArithmetics() {
		static const std::vector<Type> types({
			Type::Decrement,
			Type::Increment,
			Type::Minus,
			Type::Percent,
			Type::Plus,
			Type::Power,
			Type::Slash,
			Type::Star
		});
		return types;
	}
	
	const std::vector<Type>& operatorsBitwises() {
		static const std::vector<Type> types({
			Type::Ampersand,
			Type::Caret,
			Type::Pipe,
			Type::ShiftLeft,
			Type::ShiftRight,
			Type::Tilde
		});
		return types;
	}
	
	const std::vector<Type>& operatorsComparisons() {
		static const std::vector<Type> types({
			Type::Equal,
			Type::GreaterThan,
			Type::GreaterThanEqual,
			Type::LessThan,
			Type::LessThanEqual,
			Type::NotEqual
		});
		return types;
	}
	
	const std::vector<Type>& operatorsAssignments() {
		static const std::vector<Type> types({
			Type::AmpersandAssignment,
			Type::Assignment,
			Type::CaretAssignment,
			Type::MinusAssignment,
			Type::PercentAssignment,
			Type::PipeAssignment,
			Type::PlusAssignment,
			Type::ShiftLeftAssignment,
			Type::ShiftRightAssignment,
			Type::SlashAssignment,
			Type::StarAssignment
		});
		return types;
	}
	
	const std::vector<Type>& punctuations() {
		static const std::vector<Type> types({
			Type::Arrow,
			Type::At,
			Type::Bang,
			Type::Colon,
			Type::Comma,
			Type::Dot,
			Type::DoubleColon,
			Type::DoubleDot,
			Type::Ellipsis,
			Type::FatArrow,
			Type::Hash,
			Type::Question,
			Type::Semicolon
		});
		return types;
	}
	
	const std::vector<Type>& delimiters() {
		static const std::vector<Type> types({
			Type::LeftBrace,
			Type::LeftBracket,
			Type::LeftParenthesis,
			Type::RightBrace,
			Type::RightBracket,
			Type::RightParenthesis
		});
		return types;
	}
	
	const std::vector<Type>& indentations() {
		static const std::vector<Type> types({
			Type::Dedent,
			Type::Indent,
			Type::Newline
		});
		return types;
	}
	
	const std::vector<Type>& specials() {
		static const std::vector<Type> types({
			Type::Eof,
			Type::Error
		});
		return types;
	}
	
	const char* toString( Type type ) {
		switch( type ) {
			case Type::Ampersand: return "&";
			case Type::AmpersandAssignment: return "&=";
			case Type::Arrow: return "->";
			case Type::Assignment: return "=";
			case Type::At: return "@";
			case Type::Bang: return "!";
			case Type::Caret: return "^";
			case Type::CaretAssignment: return "^=";
			case Type::Colon: return ":";
			case Type::Comma: return ",";
			case Type::Decrement: return "--";
			case Type::Dedent: return "DEDENT";
			case Type::Dot: return ".";
			case Type::DoubleColon: return "::";
			case Type::DoubleDot: return "..";
			case Type::Ellipsis: return "...";
			case Type::Eof: return "EOF";
			case Type::Equal: return "==";
			case Type::Error: return "ERROR";
			case Type::FatArrow: return "=>";
			case Type::GreaterThan: return ">";
			case Type::GreaterThanEqual: return ">=";
			case Type::Hash: return "#";
			case Type::Identifier: return "identifier";
			case Type::Increment: return "++";
			case Type::Indent: return "INDENT";
			case Type::KeywordAbstract: return "abstract";
			case Type::KeywordAnd: return "and";
			case Type::KeywordAs: return "as";
			case Type::KeywordAssembly: return "asm";
			case Type::KeywordAsync: return "async";
			case Type::KeywordAwait: return "await";
			case Type::KeywordBacked: return "backed";
			case Type::KeywordBreak: return "break";
			case Type::KeywordCase: return "case";
			case Type::KeywordClass: return "class";
			case Type::KeywordConstant: return "const";
			case Type::KeywordContinue: return "continue";
			case Type::KeywordDefer: return "defer";
			case Type::KeywordDelete: return "delete";
			case Type::KeywordElse: return "else";
			case Type::KeywordElif: return "elif";
			case Type::KeywordEnum: return "enum";
			case Type::KeywordExcept: return "except";
			case Type::KeywordExport: return "export";
			case Type::KeywordExtends: return "extends";
			case Type::KeywordExtern: return "extern";
			case Type::KeywordFalse: return "false";
			case Type::KeywordFinal: return "final";
			case Type::KeywordFinally: return "finally";
			case Type::KeywordFor: return "for";
			case Type::KeywordFrom: return "from";
			case Type::KeywordFunction: return "function";
			case Type::KeywordIf: return "if";
			case Type::KeywordImplements: return "implements";
			case Type::KeywordImport: return "import";
			case Type::KeywordIn: return "in";
			case Type::KeywordInstanceOf: return "instanceof";
			case Type::KeywordInterface: return "interface";
			case Type::KeywordIs: return "is";
			case Type::KeywordLambda: return "lambda";
			case Type::KeywordMatch: return "match";
			case Type::KeywordMove: return "move";
			case Type::KeywordMutable: return "mut";
			case Type::KeywordNative: return "native";
			case Type::KeywordNew: return "new";
			case Type::KeywordNone: return "none";
			case Type::KeywordNot: return "not";
			case Type::KeywordOr: return "or";
			case Type::KeywordOverride: return "override";
			case Type::KeywordOwn: return "own";
			case Type::KeywordPackage: return "package";
			case Type::KeywordParent: return "parent";
			case Type::KeywordPass: return "pass";
			case Type::KeywordPrivate: return "private";
			case Type::KeywordProperty: return "property";
			case Type::KeywordProtect: return "protect";
			case Type::KeywordPublic: return "public";
			case Type::KeywordRaise: return "raise";
			case Type::KeywordRaises: return "raises";
			case Type::KeywordReadonly: return "readonly";
			case Type::KeywordReference: return "ref";
			case Type::KeywordReturn: return "return";
			case Type::KeywordSelf: return "self";
			case Type::KeywordStatic: return "static";
			case Type::KeywordStruct: return "struct";
			case Type::KeywordSubclassOf: return "subclassof";
			case Type::KeywordSwitch: return "switch";
			case Type::KeywordTrait: return "trait";
			case Type::KeywordTrue: return "true";
			case Type::KeywordTry: return "try";
			case Type::KeywordType: return "type";
			case Type::KeywordUnit: return "unit";
			case Type::KeywordAddressof: return "addressof";
		case Type::KeywordUnsafe: return "unsafe";
			case Type::KeywordUse: return "use";
			case Type::KeywordVirtual: return "virtual";
			case Type::KeywordVolatile: return "volatile";
			case Type::KeywordWhere: return "where";
			case Type::KeywordWhile: return "while";
			case Type::KeywordYield: return "yield";
			case Type::LeftBrace: return "{";
			case Type::LeftBracket: return "[";
			case Type::LeftParenthesis: return "(";
			case Type::LessThan: return "<";
			case Type::LessThanEqual: return "<=";
			case Type::LiteralChar: return "char_literal";
			case Type::LiteralFloat: return "float";
			case Type::LiteralInteger: return "integer";
			case Type::LiteralRegex: return "regex";
			case Type::LiteralString: return "string";
			case Type::Minus: return "-";
			case Type::MinusAssignment: return "-=";
			case Type::Newline: return "NEWLINE";
			case Type::NotEqual: return "!=";
			case Type::Percent: return "%";
			case Type::PercentAssignment: return "%=";
			case Type::Pipe: return "|";
			case Type::PipeAssignment: return "|=";
			case Type::Plus: return "+";
			case Type::PlusAssignment: return "+=";
			case Type::Power: return "**";
			case Type::Question: return "?";
			case Type::RightBrace: return "}";
			case Type::RightBracket: return "]";
			case Type::RightParenthesis: return ")";
			case Type::Semicolon: return ";";
			case Type::ShiftLeft: return "<<";
			case Type::ShiftLeftAssignment: return "<<=";
			case Type::ShiftRight: return ">>";
			case Type::ShiftRightAssignment: return ">>=";
			case Type::Slash: return "/";
			case Type::SlashAssignment: return "/=";
			case Type::Star: return "*";
			case Type::StarAssignment: return "*=";
			case Type::Tilde: return "~";
			default:
				return "Unspecified";
		}
	}
	
}
