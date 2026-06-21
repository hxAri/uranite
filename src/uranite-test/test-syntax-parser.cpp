
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
#include <vector>

#include "uranite/ast/node.hpp"
#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/lexer/lexer.hpp"
#include "uranite/parser/parser.hpp"
#include "uranite/token/token.hpp"

/**
 * @class SyntaxParserTest
 * 
 * @brief Test fixture for the Parser component, inheriting from Google Test's ::testing::Test.
 * 
 * This class sets up the necessary environment—such as the diagnostic engine—to
 * easily run semantic and syntactic parsing test cases against the Parser.
 * 
 */
class SyntaxParserTest : public ::testing::Test {
	
	protected:
		
		/** 
		 * @brief The diagnostic engine instance used to capture compilation errors or warnings.
		 * * Initialized with an ID or configuration flag of 0.
		 */
		uranite::diagnostic::Engine diagnostic { 0, 0 };
		
		/**
		 * @brief Helper method to tokenize and parse a given string of source code.
		 * 
		 * This utility encapsulates the entire frontend pipeline (Lexer -> Parser) 
		 * into a single call, making individual test cases highly readable.
		 * 
		 * @param code The source code string to be analyzed.
		 * 
		 * @return uranite::ast::nodes::ProgramSharedPointer The resulting Abstract Syntax Tree (AST) root node.
		 * 
		 */
		uranite::ast::nodes::ProgramSharedPointer parse( const std::string& code ) {
			uranite::lexer::Lexer lexer( code, "test.urn", this->diagnostic );
			uranite::parser::Parser parser( lexer.tokenize(), this->diagnostic );
			return parser.parse();
		}
	
};

TEST_F( SyntaxParserTest, Any ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    return 0\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_EQ( program->declarations.size(), 1u );
	ASSERT_EQ( program->declarations[0]->kind, uranite::ast::Node::Kind::FunctionDeclaration );
}

TEST_F( SyntaxParserTest, ParsesClassDeclaration ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"class Animal:\n"
		"    String name\n"
		"    public function speak( self ) -> String:\n"
		"        return \"...\"\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_EQ( program->declarations.size(), 1u );
	EXPECT_EQ( program->declarations[0]->kind, uranite::ast::Node::Kind::ClassDeclaration );
	if( program->declarations[0]->kind == uranite::ast::Node::Kind::ClassDeclaration ) {
		uranite::ast::nodes::ClassDeclaration& classDeclaration( static_cast<uranite::ast::nodes::ClassDeclaration&>( *program->declarations[0] ) );
		EXPECT_EQ( classDeclaration.name, "Animal" );
		EXPECT_EQ( classDeclaration.fields.size(), 1u );
		EXPECT_EQ( classDeclaration.methods.size(), 1u );
	}
}

TEST_F( SyntaxParserTest, ParsesEnumDeclaration ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"enum Color:\n"
		"    unit Red\n"
		"    unit Green\n"
		"    unit Blue\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_EQ( program->declarations.size(), 1u );
	if( program->declarations[0]->kind == uranite::ast::Node::Kind::EnumDeclaration ) {
		uranite::ast::nodes::EnumDeclaration& enumDeclaration( static_cast<uranite::ast::nodes::EnumDeclaration&>( *program->declarations[0] ) );
		EXPECT_EQ( enumDeclaration.name, "Color" );
		EXPECT_EQ( enumDeclaration.variants.size(), 3u );
	}
}

TEST_F( SyntaxParserTest, ParsesEnumDeclarationAdvanced ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"enum Color backed Number:\n"
		"    unit Red 0xa\n"
		"    unit Green 0xb\n"
		"    unit Blue 0xc:\n"
		"        public function isBlue( self ) -> Boolean:\n"
		"             return True\n"
		"    public function isBlue( self ) -> Boolean:\n"
		"        return self.value == Color.Blue\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_EQ( program->declarations.size(), 1u );
	if( program->declarations[0]->kind == uranite::ast::Node::Kind::EnumDeclaration ) {
		uranite::ast::nodes::EnumDeclaration& enumDeclaration( static_cast<uranite::ast::nodes::EnumDeclaration&>( *program->declarations[0] ) );
		EXPECT_EQ( enumDeclaration.name, "Color" );
		EXPECT_EQ( enumDeclaration.methods.size(), 1u );
		EXPECT_EQ( enumDeclaration.variants.size(), 3u );
		EXPECT_EQ( enumDeclaration.variants[2]->methods.size(), 1u );
	}
}

TEST_F( SyntaxParserTest, ParsesExportBlock ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"function test() -> Void:\n"
		"    return\n"
		"\n"
		"export {\n"
		"    test\n"
		"}\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_FALSE( this->diagnostic.hasErrors() );
	ASSERT_GE( program->exports.size(), 1u );
	EXPECT_EQ( program->exports[0]->items.size(), 1u );
	EXPECT_EQ( program->exports[0]->items[0].name, "test" );
}

TEST_F( SyntaxParserTest, ParsesExportDeclaration ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"export function main() -> Void:\n"
		"    return\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_FALSE( this->diagnostic.hasErrors() );
	ASSERT_GE( program->exports.size(), 1u );
	EXPECT_NE( program->exports[0]->declaration, nullptr );
	EXPECT_EQ( program->declarations.size(), 1u );
}

TEST_F( SyntaxParserTest, ParsesExportWithAlias ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"function helper() -> Void:\n"
		"    return\n"
		"\n"
		"export {\n"
		"    helper as publicHelper\n"
		"}\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_FALSE( this->diagnostic.hasErrors() );
	ASSERT_GE( program->exports.size(), 1u );
	EXPECT_EQ( program->exports[0]->items[0].name, "helper" );
	EXPECT_EQ( program->exports[0]->items[0].alias, "publicHelper" );
}

TEST_F( SyntaxParserTest, ParsesFromImport ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"from uranite.functions import {\n"
		"    printf,\n"
		"    puts as p\n"
		"}\n"
		"\n"
		"function test() -> Void:\n"
		"    return\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_EQ( program->imports.size(), 1u );
	EXPECT_TRUE( program->imports[0]->isFromImport );
	EXPECT_EQ( program->imports[0]->importItems.size(), 2u );
	EXPECT_EQ( program->imports[0]->importItems[0].name, "printf" );
	EXPECT_EQ( program->imports[0]->importItems[1].name, "puts" );
	EXPECT_EQ( program->imports[0]->importItems[1].alias, "p" );
}

TEST_F( SyntaxParserTest, ParsesIfElse ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"function test( I32 x) -> I32:\n"
		"    if x > 0:\n"
		"        return 1\n"
		"    elif x == 0:\n"
		"        return 0\n"
		"    else:\n"
		"        return -1\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_FALSE( this->diagnostic.hasErrors() );
}

TEST_F( SyntaxParserTest, ParsesNestedClassInClass ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"class Outer:\n"
		"    public I64 x\n"
		"    public class Inner:\n"
		"        public I64 y\n"
		"        public function Inner( self, I64 y ) -> Void:\n"
		"            self.y = y\n"
		"    public function Outer( self, I64 x ) -> Void:\n"
		"        self.x = x\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_EQ( program->declarations.size(), 1u );
	uranite::ast::nodes::ClassDeclaration& classDeclarationOuter( static_cast<uranite::ast::nodes::ClassDeclaration&>( *program->declarations[0] ) );
	EXPECT_EQ( classDeclarationOuter.name, "Outer" );
	ASSERT_EQ( classDeclarationOuter.nestedDeclarations.size(), 1u );
	uranite::ast::nodes::ClassDeclaration& classDeclarationInner( static_cast<uranite::ast::nodes::ClassDeclaration&>( *classDeclarationOuter.nestedDeclarations[0] ) );
	EXPECT_EQ( classDeclarationInner.name, "Inner" );
}

TEST_F( SyntaxParserTest, ParsesNestedFunctionInFunction ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"function outer() -> I32:\n"
		"    function inner() -> I32:\n"
		"        return 42\n"
		"    return inner()\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_EQ( program->declarations.size(), 1u );
	uranite::ast::nodes::FunctionDeclaration& functionDeclaration( static_cast<uranite::ast::nodes::FunctionDeclaration&>( *program->declarations[0] ) );
	EXPECT_EQ( functionDeclaration.name, "outer" );
	ASSERT_EQ( functionDeclaration.nestedFunctions.size(), 1u );
	uranite::ast::nodes::FunctionDeclaration& functionDeclarationNested( static_cast<uranite::ast::nodes::FunctionDeclaration&>( *functionDeclaration.nestedFunctions[0] ) );
	EXPECT_EQ( functionDeclarationNested.name, "inner" );
}

TEST_F( SyntaxParserTest, ParsesNestedInterfaceInClass ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"class Container:\n"
		"    public I64 size\n"
		"    public interface Callback:\n"
		"        public function onEvent( self ) -> Void\n"
		"    public function Container( self ) -> Void:\n"
		"        self.size = 0\n"
	));
	ASSERT_NE( program, nullptr );
	if( program->declarations[0]->kind == uranite::ast::Node::Kind::ClassDeclaration ) {
		uranite::ast::nodes::ClassDeclaration& classDeclaration( static_cast<uranite::ast::nodes::ClassDeclaration&>( *program->declarations[0] ) );
		ASSERT_EQ( classDeclaration.nestedDeclarations.size(), 1u );
		EXPECT_EQ( classDeclaration.nestedDeclarations[0]->kind, uranite::ast::Node::Kind::InterfaceDeclaration );
	}
}

TEST_F( SyntaxParserTest, ParsesPackageAndImport ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"from uranite.language import Integer"
		"\n"
		"public function main() -> I32:\n"
		"    return 0\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_NE( program->module, nullptr );
	EXPECT_EQ( program->module->name, "uranite.syntax.parser.test" );
	ASSERT_EQ( program->imports.size(), 1u );
	EXPECT_EQ( program->imports[0]->path.size(), 2u );
}

TEST_F( SyntaxParserTest, ParsesSimpleFunction ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    return 0\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_EQ( program->declarations.size(), 1u );
	EXPECT_EQ( program->declarations[0]->kind, uranite::ast::Node::Kind::FunctionDeclaration );
	if( program->declarations[0]->kind == uranite::ast::Node::Kind::FunctionDeclaration ) {
		uranite::ast::nodes::FunctionDeclaration& functionDeclaration( static_cast<uranite::ast::nodes::FunctionDeclaration&>(*program->declarations[0] ) );
		EXPECT_EQ( functionDeclaration.name, "main" );
	}
}

TEST_F( SyntaxParserTest, ParsesVariableDeclaration ) {
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function test() -> Void:\n"
		"    I32 x = 42\n"
		"    I32 y = 10\n"
	));
	ASSERT_NE( program, nullptr );
	ASSERT_FALSE( this->diagnostic.hasErrors() );
}
