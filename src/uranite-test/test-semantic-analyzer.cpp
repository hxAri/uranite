
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
#include "uranite/parser/parser.hpp"
#include "uranite/semantic/analyzer.hpp"

/**
 * @class SemanticAnalyzerTest
 * @brief Test fixture for the Semantic Analyzer component, inheriting from Google Test's ::testing::Test.
 *
 * This fixture manages the necessary environment to run semantic verification tests,
 * such as type checking, scope validation, and symbol resolution on the generated AST.
 */
class SemanticAnalyzerTest : public ::testing::Test {
	
	protected:
		
		/**
		 * @brief Helper method to run full frontend processing and perform semantic analysis on the code.
		 * * This utility builds the complete pipeline sequentially (Lexer -> Parser -> Analyzer).
		 * It suppresses standard console diagnostics during the run to ensure cleaner test output.
		 * * @param code The raw source code string to be tokenized, parsed, and semantically verified.
		 * @return true If the AST passes all semantic rules and constraints without fatal errors.
		 * @return false If any semantic violations (e.g., type mismatches, unresolved symbols) are detected.
		 */
		bool analyze( const std::string& code ) {
			uranite::diagnostic::Engine diagnostic( 100, 100 );
			diagnostic.setConsolPrintable( false );
			uranite::lexer::Lexer lexer( code, "test.urn", diagnostic );
			std::vector<uranite::token::Token> tokens( lexer.tokenize() );
			uranite::parser::Parser parser( tokens, diagnostic );
			uranite::ast::nodes::ProgramSharedPointer program( parser.parse() );
			uranite::semantic::Analyzer analyzer( diagnostic );
			return analyzer.analyze( *program );
		}
	
};

TEST_F( SemanticAnalyzerTest, Any ) {
}

TEST_F( SemanticAnalyzerTest, AnalyzesNestedClassDecl ) {
    bool result = this->analyze(
        "class Outer:\n"
        "    public I64 x\n"
        "    public class Inner:\n"
        "        public I64 y\n"
        "        public function Inner( self, I64 y ) -> Void:\n"
        "            self.y = y\n"
        "    public function Outer( self, I64 x ) -> Void:\n"
        "        self.x = x\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, BooleanAndOrOperators ) {
    bool result = this->analyze(
        "function test( Boolean a, Boolean b ) -> Boolean:\n"
        "    if a and b:\n"
        "        return True\n"
        "    if a or b:\n"
        "        return True\n"
        "    return False\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, ClassInheritanceAssignability ) {
    bool result = this->analyze(
        "class Animal:\n"
        "    public String name\n"
        "class Dog extends Animal:\n"
        "    public I64 age\n"
        "function test() -> Void:\n"
        "    Animal d = new Dog()\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, DetectsBreakOutsideLoop ) {
    bool result = this->analyze(
        "function test() -> Void:\n"
        "    break\n"
    );
    EXPECT_FALSE( result );
}

TEST_F( SemanticAnalyzerTest, DetectsMissingVisibilityOnField ) {
    bool result = this->analyze(
        "class Foo:\n"
        "    String name\n"
    );
    EXPECT_FALSE( result );
}

TEST_F( SemanticAnalyzerTest, DetectsMissingVisibilityOnMethod ) {
    bool result = this->analyze(
        "class Foo:\n"
        "    function bar( self ) -> Void:\n"
        "        return\n"
    );
    EXPECT_FALSE( result );
}

TEST_F( SemanticAnalyzerTest, GenericStructInsideGenericClass ) {
    bool result = this->analyze(
        "struct Entry<K, V>:\n"
        "    public K key\n"
        "    public V value\n"
        "    public function Entry( self, K key, V value ) -> Void:\n"
        "        self.key = key\n"
        "        self.value = value\n"
        "class Container<K, V>:\n"
        "    public Entry<K, V> entry\n"
        "    public function Container( self, K key, V value ) -> Void:\n"
        "        self.entry = new Entry<K, V>( key, value )\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, GenericStructUsedFromClass ) {
    bool result = this->analyze(
        "struct Entry<K, V>:\n"
        "    public K key\n"
        "    public V value\n"
        "    public function Entry( self, K key, V value ) -> Void:\n"
        "        self.key = key\n"
        "        self.value = value\n"
        "class Wrapper<K, V>:\n"
        "    public Entry<K, V> entry\n"
        "    public function Wrapper( self, K key, V value ) -> Void:\n"
        "        self.entry = new Entry<K, V>( key, value )\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, InterfaceClassAssignability ) {
    bool result = this->analyze(
        "interface Printable:\n"
        "    public function display( self ) -> Void\n"
        "class Widget implements Printable:\n"
        "    public function display( self ) -> Void:\n"
        "        return\n"
        "function test() -> Void:\n"
        "    Widget w = new Widget()\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, InterfaceDeclaration ) {
    bool result = this->analyze(
        "interface Printable:\n"
        "    public function toString( self ) -> String\n"
        "class Foo implements Printable:\n"
        "    public function toString( self ) -> String:\n"
        "        return \"Foo\"\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, InterfaceInheritsMethods ) {
    bool result = this->analyze(
		"from uranite.memory.memory import Memory\n"
        "interface Collection<E>:\n"
        "    public function length( self ) -> I64\n"
        "interface Sequence<E> extends Collection<E>:\n"
        "    public function get( self, I64 index ) -> E\n"
        "class MyList<E> implements Sequence<E>:\n"
		"    private Memory<E> stack\n"
		"    public function MyList( self ) -> Void:\n"
		"        self.stack = new Memory<E>( 16 )\n"
        "    public function length( self ) -> I64:\n"
        "        return 0\n"
        "    public function get( self, I64 index ) -> E:\n"
        "        return self.stack.get( index )\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, NonGenericStructDeclaration ) {
    bool result = this->analyze(
        "struct Vec2:\n"
        "    public I64 x\n"
        "    public I64 y\n"
        "    public function Vec2( self, I64 x, I64 y ) -> Void:\n"
        "        self.x = x\n"
        "        self.y = y\n"
        "    public function sum( self ) -> I64:\n"
        "        return self.x + self.y\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, NullableFieldInClass ) {
    bool result = this->analyze(
        "class Node:\n"
        "    public I64 value\n"
        "    public ?Node next\n"
        "function test() -> Void:\n"
        "    Node n = new Node()\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, ReadonlyClass ) {
    bool result = this->analyze(
        "Readonly class Config:\n"
        "    public String name\n"
        "    public function Config( self, String name ) -> Void:\n"
        "        self.name = name\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, StructMethodWithObjectParam ) {
    bool result = this->analyze(
        "struct Point:\n"
        "    public I64 x\n"
        "    public I64 y\n"
        "    public function Point( self, I64 x, I64 y ) -> Void:\n"
        "        self.x = x\n"
        "        self.y = y\n"
        "    public function equals( self, Object other ) -> Boolean:\n"
        "        return False\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, ValidSimpleProgram ) {
    bool result = this->analyze(
        "function main() -> I32:\n"
        "    I32 x = 42\n"
        "    return x\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, ValidSimpleProgramWithArgv ) {
    bool result = this->analyze(
		"from uranite.collection.array-list import ArrayList\n"
        "function main( I64 argc, ArrayList<String> argv ) -> I32:\n"
        "    I32 x = 42\n"
        "    return x\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, FunctionOverloadDifferentParamTypes ) {
    bool result = this->analyze(
        "function process( I64 value ) -> Void:\n"
        "    I64 x = value\n"
        "function process( String value ) -> Void:\n"
        "    String s = value\n"
        "function main() -> I32:\n"
        "    process( 42 )\n"
        "    process( \"hello\" )\n"
        "    return 0\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, FunctionOverloadDifferentArity ) {
    bool result = this->analyze(
        "function compute( I64 x ) -> I64:\n"
        "    return x\n"
        "function compute( I64 x, I64 y ) -> I64:\n"
        "    return x + y\n"
        "function main() -> I32:\n"
        "    I64 a = compute( 1 )\n"
        "    I64 b = compute( 1, 2 )\n"
        "    return 0\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, FunctionOverloadRejectsDuplicateSignature ) {
    bool result = this->analyze(
        "function duplicate() -> Int:\n"
        "    return 0\n"
        "function duplicate() -> String:\n"
        "    return \"String\"\n"
    );
    EXPECT_FALSE( result );
}

TEST_F( SemanticAnalyzerTest, FunctionOverloadRejectsDuplicateSignatureParameter ) {
    bool result = this->analyze(
        "function duplicate( I64 x ) -> Void:\n"
        "    I64 a = x\n"
        "function duplicate( I64 y ) -> Void:\n"
        "    I64 b = y\n"
    );
    EXPECT_FALSE( result );
}

TEST_F( SemanticAnalyzerTest, FunctionOverloadVariadicParameter ) {
    bool result = this->analyze(
        "function collect( I64 items[] ) -> Void:\n"
        "    I64 count = 0\n"
        "function collect( String label, I64 items[] ) -> Void:\n"
        "    I64 count = 0\n"
        "function main() -> I32:\n"
        "    collect( 1, 2, 3 )\n"
        "    collect( \"data\", 1, 2, 3 )\n"
        "    return 0\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, FunctionOverloadMixedFixedAndVariadic ) {
    bool result = this->analyze(
        "function format( String template ) -> String:\n"
        "    return template\n"
        "function format( String template, I64 values[] ) -> String:\n"
        "    return template\n"
        "function main() -> I32:\n"
        "    String a = format( \"plain\" )\n"
        "    String b = format( \"nums\", 1, 2 )\n"
        "    return 0\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, FunctionOverloadThreeWay ) {
    bool result = this->analyze(
        "function describe( I64 value ) -> I64:\n"
        "    return 1\n"
        "function describe( String value ) -> I64:\n"
        "    return 2\n"
        "function describe( Boolean value ) -> I64:\n"
        "    return 3\n"
        "function main() -> I32:\n"
        "    I64 a = describe( 42 )\n"
        "    I64 b = describe( \"hi\" )\n"
        "    I64 c = describe( True )\n"
        "    return 0\n"
    );
    EXPECT_TRUE( result );
}

TEST_F( SemanticAnalyzerTest, NonFunctionSymbolRejectsOverload ) {
    bool result = this->analyze(
        "function main() -> I32:\n"
        "    I64 x = 10\n"
        "    I64 x = 20\n"
        "    return 0\n"
    );
    EXPECT_FALSE( result );
}
