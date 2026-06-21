
//
// @author hxAri (hxari)
// @create 2025-02-24 15:15
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

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <string>

#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/lexer/lexer.hpp"
#include "uranite/token/token.hpp"

/**
 * @class SyntaxLexerTest
 * 
 * @brief Test fixture for the Lexer component, inheriting from Google Test's ::testing::Test.
 * 
 * This fixture provides the baseline environment required to execute lexical analysis 
 * tests, specifically managing the diagnostic subsystem to track syntax errors.
 * 
 */
class SyntaxLexerTest : public ::testing::Test {
	
	protected:
		
		/** 
		 * @brief The diagnostic engine instance from the uranite namespace.
		 * * Initialized with default error and warning configurations (0, 0).
		 */
		uranite::diagnostic::Engine diagnostic { 0, 0 };
		
		/**
		 * @brief Helper method to run the lexical analyzer on a given string of source code.
		 * 
		 * Instantiates an internal Lexer with the provided virtual filename and 
		 * source code, then processes it to return a sequential list of tokens.
		 * 
		 * @param filename The virtual or actual source file path used for diagnostic reporting.
		 * @param code The raw source code string to be tokenized.
		 * 
		 * @return std::vector<uranite::token::Token> A vector containing the generated tokens.
		 */
		std::vector<uranite::token::Token> tokenize( const std::string& filename, const std::string& code ) {
			uranite::lexer::Lexer lexer( code, filename, this->diagnostic );
			return lexer.tokenize();
		}
	
};

TEST_F( SyntaxLexerTest, Any ) {
}

TEST_F( SyntaxLexerTest, TokenizeKeywordControlFlow ) {
	std::vector<uranite::token::Token> tokens( 
		this->tokenize( 
			"tokenize-keyword-control-flow.urn", 
			"break continue elif else for if match return while yield"
		)
	);
	ASSERT_GE( tokens.size(), 10u );
	ASSERT_EQ( tokens[0].type, uranite::token::Type::KeywordBreak );
	ASSERT_EQ( tokens[1].type, uranite::token::Type::KeywordContinue );
	ASSERT_EQ( tokens[2].type, uranite::token::Type::KeywordElif );
	ASSERT_EQ( tokens[3].type, uranite::token::Type::KeywordElse );
	ASSERT_EQ( tokens[4].type, uranite::token::Type::KeywordFor );
	ASSERT_EQ( tokens[5].type, uranite::token::Type::KeywordIf );
	ASSERT_EQ( tokens[6].type, uranite::token::Type::KeywordMatch );
	ASSERT_EQ( tokens[7].type, uranite::token::Type::KeywordReturn );
	ASSERT_EQ( tokens[8].type, uranite::token::Type::KeywordWhile );
	ASSERT_EQ( tokens[9].type, uranite::token::Type::KeywordYield );
}

TEST_F( SyntaxLexerTest, TokenizeKeywordDeclaration ) {
	std::vector<uranite::token::Token> tokens( 
		this->tokenize( 
			"tokenize-keyword-declaration.urn", 
			"class const enum extern from function implements "
			"import interface mut package static struct type"
		)
	);
	ASSERT_GE( tokens.size(), 14u );
	ASSERT_EQ( tokens[0].type, uranite::token::Type::KeywordClass );
	ASSERT_EQ( tokens[1].type, uranite::token::Type::KeywordConstant );
	ASSERT_EQ( tokens[2].type, uranite::token::Type::KeywordEnum );
	ASSERT_EQ( tokens[3].type, uranite::token::Type::KeywordExtern );
	ASSERT_EQ( tokens[4].type, uranite::token::Type::KeywordFrom );
	ASSERT_EQ( tokens[5].type, uranite::token::Type::KeywordFunction );
	ASSERT_EQ( tokens[6].type, uranite::token::Type::KeywordImplements );
	ASSERT_EQ( tokens[7].type, uranite::token::Type::KeywordImport );
	ASSERT_EQ( tokens[8].type, uranite::token::Type::KeywordInterface );
	ASSERT_EQ( tokens[9].type, uranite::token::Type::KeywordMutable );
	ASSERT_EQ( tokens[10].type, uranite::token::Type::KeywordPackage );
	ASSERT_EQ( tokens[11].type, uranite::token::Type::KeywordStatic );
	ASSERT_EQ( tokens[12].type, uranite::token::Type::KeywordStruct );
	ASSERT_EQ( tokens[13].type, uranite::token::Type::KeywordType );
}

TEST_F( SyntaxLexerTest, TokenizeKeywordAccessModifier ) {
	std::vector<uranite::token::Token> tokens( 
		this->tokenize( 
			"tokenize-keyword-access-modifier.urn", 
			"private protect public"
		)
	);
	ASSERT_GE( tokens.size(), 3u );
	ASSERT_EQ( tokens[0].type, uranite::token::Type::KeywordPrivate );
	ASSERT_EQ( tokens[1].type, uranite::token::Type::KeywordProtect );
	ASSERT_EQ( tokens[2].type, uranite::token::Type::KeywordPublic );
}

TEST_F( SyntaxLexerTest, TokenizeKeywordOOP ) {
	std::vector<uranite::token::Token> tokens( 
		this->tokenize( 
			"tokenize-keyword-oop.urn", 
			"abstract delete extends final native new "
			"override parent property readonly self virtual"
		)
	);
	ASSERT_GE( tokens.size(), 12u );
	ASSERT_EQ( tokens[0].type, uranite::token::Type::KeywordAbstract );
	ASSERT_EQ( tokens[1].type, uranite::token::Type::KeywordDelete );
	ASSERT_EQ( tokens[2].type, uranite::token::Type::KeywordExtends );
	ASSERT_EQ( tokens[3].type, uranite::token::Type::KeywordFinal );
	ASSERT_EQ( tokens[4].type, uranite::token::Type::KeywordNative );
	ASSERT_EQ( tokens[5].type, uranite::token::Type::KeywordNew );
	ASSERT_EQ( tokens[6].type, uranite::token::Type::KeywordOverride );
	ASSERT_EQ( tokens[7].type, uranite::token::Type::KeywordParent );
	ASSERT_EQ( tokens[8].type, uranite::token::Type::KeywordProperty );
	ASSERT_EQ( tokens[9].type, uranite::token::Type::KeywordReadonly );
	ASSERT_EQ( tokens[10].type, uranite::token::Type::KeywordSelf );
	ASSERT_EQ( tokens[11].type, uranite::token::Type::KeywordVirtual );
}

TEST_F( SyntaxLexerTest, TokenizeKeywordMemorySafety ) {
	std::vector<uranite::token::Token> tokens( 
		this->tokenize( 
			"tokenize-keyword-memory-safety.urn", 
			"move own reference unsafe"
		)
	);
	ASSERT_GE( tokens.size(), 4u );
	ASSERT_EQ( tokens[0].type, uranite::token::Type::KeywordMove );
	ASSERT_EQ( tokens[1].type, uranite::token::Type::KeywordOwn );
	ASSERT_EQ( tokens[2].type, uranite::token::Type::KeywordReference );
	ASSERT_EQ( tokens[3].type, uranite::token::Type::KeywordUnsafe );
}
	
TEST_F( SyntaxLexerTest, TokenizeKeywordLogic ) {
	std::vector<uranite::token::Token> tokens( 
		this->tokenize( 
			"tokenize-keyword-logic.urn", 
			"and not or"
		)
	);
	ASSERT_GE( tokens.size(), 3u );
	ASSERT_EQ( tokens[0].type, uranite::token::Type::KeywordAnd );
	ASSERT_EQ( tokens[1].type, uranite::token::Type::KeywordNot );
	ASSERT_EQ( tokens[2].type, uranite::token::Type::KeywordOr );
}

TEST_F( SyntaxLexerTest, TokenizeKeywordValue ) {
	std::vector<uranite::token::Token> tokens( 
		this->tokenize( 
			"tokenize-keyword-value.urn", 
			"False None True"
		)
	);
	ASSERT_GE( tokens.size(), 3u );
	ASSERT_EQ( tokens[0].type, uranite::token::Type::KeywordFalse );
	ASSERT_EQ( tokens[1].type, uranite::token::Type::KeywordNone );
	ASSERT_EQ( tokens[2].type, uranite::token::Type::KeywordTrue );
}

TEST_F( SyntaxLexerTest, TokenizeKeywordException ) {
	std::vector<uranite::token::Token> tokens( 
		this->tokenize( 
			"tokenize-keyword-exception.urn",
			"except finally raise raises try"
		)
	);
	ASSERT_GE( tokens.size(), 5u );
	ASSERT_EQ( tokens[0].type, uranite::token::Type::KeywordExcept );
	ASSERT_EQ( tokens[1].type, uranite::token::Type::KeywordFinally );
	ASSERT_EQ( tokens[2].type, uranite::token::Type::KeywordRaise );
	ASSERT_EQ( tokens[3].type, uranite::token::Type::KeywordRaises );
	ASSERT_EQ( tokens[4].type, uranite::token::Type::KeywordTry );
}

TEST_F( SyntaxLexerTest, TokenizeKeywordOther ) {
	std::vector<uranite::token::Token> tokens( 
		this->tokenize( 
			"tokenize-keyword-other.urn",
			"as asm async await backed case defer in instanceof is "
			"lambda pass subclassof switch trait unit use volatile where"
		)
	);
	ASSERT_GE( tokens.size(), 19u );
	ASSERT_EQ( tokens[0].type, uranite::token::Type::KeywordAs );
	ASSERT_EQ( tokens[1].type, uranite::token::Type::KeywordAssembly );
	ASSERT_EQ( tokens[2].type, uranite::token::Type::KeywordAsync );
	ASSERT_EQ( tokens[3].type, uranite::token::Type::KeywordAwait );
	ASSERT_EQ( tokens[4].type, uranite::token::Type::KeywordBacked );
	ASSERT_EQ( tokens[5].type, uranite::token::Type::KeywordCase );
	ASSERT_EQ( tokens[6].type, uranite::token::Type::KeywordDefer );
	ASSERT_EQ( tokens[7].type, uranite::token::Type::KeywordIn );
	ASSERT_EQ( tokens[8].type, uranite::token::Type::KeywordInstanceOf );
	ASSERT_EQ( tokens[9].type, uranite::token::Type::KeywordIs );
	ASSERT_EQ( tokens[10].type, uranite::token::Type::KeywordLambda );
	ASSERT_EQ( tokens[11].type, uranite::token::Type::KeywordPass );
	ASSERT_EQ( tokens[12].type, uranite::token::Type::KeywordSubclassOf );
	ASSERT_EQ( tokens[13].type, uranite::token::Type::KeywordSwitch );
	ASSERT_EQ( tokens[14].type, uranite::token::Type::KeywordTrait );
	ASSERT_EQ( tokens[15].type, uranite::token::Type::KeywordUnit );
	ASSERT_EQ( tokens[16].type, uranite::token::Type::KeywordUse );
	ASSERT_EQ( tokens[17].type, uranite::token::Type::KeywordVolatile );
	ASSERT_EQ( tokens[18].type, uranite::token::Type::KeywordWhere );
}

TEST_F( SyntaxLexerTest, TokenizeKeywordExport ) {
	std::vector<uranite::token::Token> tokens( 
		this->tokenize( 
			"tokenize-keyword-export.urn", 
			"export"
		)
	);
	ASSERT_GE( tokens.size(), 1u );
	ASSERT_EQ( tokens[0].type, uranite::token::Type::KeywordExport );
}

TEST_F( SyntaxLexerTest, TokenizeLiterals ) {
	std::vector<uranite::token::Token> tokens(
		this->tokenize(
			"tokenize-literals.urn",
			"42 3.14 \"hello\" 'x' True False"
		)
	);
	ASSERT_GE( tokens.size(), 6u);
	EXPECT_EQ( tokens[0].type, uranite::token::Type::LiteralInteger );
	EXPECT_EQ( tokens[0].value, "42" );
	EXPECT_EQ( tokens[1].type, uranite::token::Type::LiteralFloat );
	EXPECT_EQ( tokens[2].type, uranite::token::Type::LiteralString );
	EXPECT_EQ( tokens[2].value, "hello" );
	EXPECT_EQ( tokens[3].type, uranite::token::Type::LiteralChar );
	EXPECT_EQ( tokens[4].type, uranite::token::Type::KeywordTrue );
	EXPECT_EQ( tokens[5].type, uranite::token::Type::KeywordFalse );
}

TEST_F( SyntaxLexerTest, TokenizeOperators ) {
	std::vector<uranite::token::Token> tokens(
		this->tokenize(
			"tokenize-operators.urn",
			"+ - * / == != <= >= -> =>"
		)
	);
	ASSERT_GE( tokens.size(), 10u );
	EXPECT_EQ( tokens[0].type, uranite::token::Type::Plus );
	EXPECT_EQ( tokens[1].type, uranite::token::Type::Minus );
	EXPECT_EQ( tokens[2].type, uranite::token::Type::Star );
	EXPECT_EQ( tokens[3].type, uranite::token::Type::Slash );
	EXPECT_EQ( tokens[4].type, uranite::token::Type::Equal );
	EXPECT_EQ( tokens[5].type, uranite::token::Type::NotEqual );
	EXPECT_EQ( tokens[6].type, uranite::token::Type::LessThanEqual );
	EXPECT_EQ( tokens[7].type, uranite::token::Type::GreaterThanEqual );
	EXPECT_EQ( tokens[8].type, uranite::token::Type::Arrow );
	EXPECT_EQ( tokens[9].type, uranite::token::Type::FatArrow );
}

TEST_F( SyntaxLexerTest, TokenizeIndentation ) {
	std::vector<uranite::token::Token> tokens(
		this->tokenize(
			"tokenize-indentation.urn",
			"function main():\n"
			"    return 0\n"
		)
	);
	bool foundIndent( false );
	bool foundDedent( false );
	for( uranite::token::Token& token : tokens ) {
		if( token.type == uranite::token::Type::Indent ) {
			foundIndent = true;
		}
		if( token.type == uranite::token::Type::Dedent ) {
			foundDedent = true;
		}
	}
	EXPECT_TRUE( foundIndent );
	EXPECT_TRUE( foundDedent );
}

TEST_F( SyntaxLexerTest, TokenizeHexBinaryOctal ) {
	std::vector<uranite::token::Token> tokens(
		this->tokenize(
			"tokenize-hex-binary-octal.urn",
			"0xFF 0b1010 0o77"
		)
	);
	ASSERT_GE( tokens.size(), 3u );
	EXPECT_EQ( tokens[0].type, uranite::token::Type::LiteralInteger );
	EXPECT_EQ( tokens[0].value, "0xFF" );
	EXPECT_EQ( tokens[1].type, uranite::token::Type::LiteralInteger );
	EXPECT_EQ( tokens[1].value, "0b1010" );
	EXPECT_EQ( tokens[2].type, uranite::token::Type::LiteralInteger );
	EXPECT_EQ( tokens[2].value, "0o77" );
}

TEST_F( SyntaxLexerTest, TokenizeComments ) {
	std::vector<uranite::token::Token> tokens(
		this->tokenize(
			"tokenize-comments.urn",
			"# this is a comment\n"
			"42"
		)
	);
	ASSERT_GE( tokens.size(), 1u );
	EXPECT_EQ( tokens[0].type, uranite::token::Type::LiteralInteger );
	EXPECT_EQ( tokens[0].value, "42" );
}

TEST_F( SyntaxLexerTest, TokenizePrimitiveTypesAsIdentifiers ) {
	std::vector<uranite::token::Token> tokens(
		this->tokenize(
			"tokenize-primitive-types-as-identifiers.urn",
			"I32 U64 F64 Boolean String Void"
		)
	);
	ASSERT_GE( tokens.size(), 6u );
	EXPECT_EQ( tokens[0].type, uranite::token::Type::Identifier );
	EXPECT_EQ( tokens[0].value, "I32" );
	EXPECT_EQ( tokens[1].type, uranite::token::Type::Identifier );
	EXPECT_EQ( tokens[1].value, "U64" );
	EXPECT_EQ( tokens[2].type, uranite::token::Type::Identifier );
	EXPECT_EQ( tokens[2].value, "F64" );
	EXPECT_EQ( tokens[3].type, uranite::token::Type::Identifier );
	EXPECT_EQ( tokens[3].value, "Boolean" );
	EXPECT_EQ( tokens[4].type, uranite::token::Type::Identifier );
	EXPECT_EQ( tokens[4].value, "String" );
	EXPECT_EQ( tokens[5].type, uranite::token::Type::Identifier );
	EXPECT_EQ( tokens[5].value, "Void" );
}

TEST_F( SyntaxLexerTest, TokenizeBackslashLineContinuation ) {
	std::vector<uranite::token::Token> tokens(
		this->tokenize(
			"tokenize-backslash-line-continuation.urn",
			"I32 x = 1 + \\\n"
			"2 + 3"
		)
	);
	bool foundNewline( false );
	for( uranite::token::Token& token : tokens ) {
		if( token.type == uranite::token::Type::Newline && 
			token.source->location->line < 2 ) {
				foundNewline = true;
		}
	}
	EXPECT_FALSE( foundNewline );
	ASSERT_GE( tokens.size(), 7u );
	EXPECT_EQ( tokens[0].type, uranite::token::Type::Identifier );
	EXPECT_EQ( tokens[0].value, "I32" );
	EXPECT_EQ( tokens[3].type, uranite::token::Type::LiteralInteger );
	EXPECT_EQ( tokens[3].value, "1" );
	EXPECT_EQ( tokens[5].type, uranite::token::Type::LiteralInteger );
	EXPECT_EQ( tokens[5].value, "2" );
}
