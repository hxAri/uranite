
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

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <string>

#include "uranite/ast/node.hpp"
#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/lexer/lexer.hpp"
#include "uranite/parser/parser.hpp"
#include "uranite/semantic/analyzer.hpp"
#include "uranite/token/token.hpp"

/**
 * @class GenericsFixTest
 * @brief Test fixture for verifying generics resolution and constraint fixes, inheriting from Google Test's ::testing::Test.
 *
 * This fixture provides specialized utilities to test parameterized types, template 
 * instantiations, and monomorphization edge cases within the AST infrastructure.
 */
class GenericsFixTest : public ::testing::Test {
	
	protected:
		
		/**
		 * @brief Helper method to tokenize and parse a code string using an externally provided diagnostic engine.
		 * * Unlike standard fixtures that encapsulate their own state, this method allows 
		 * individual test cases to inject a custom or mocked diagnostic engine to closely 
		 * monitor syntax errors during generic type parsing.
		 * * @param diagnostic The diagnostic engine reference to capture compilation anomalies.
		 * @param code The raw source code string containing generic constructs to be parsed.
		 * @return uranite::ast::nodes::ProgramSharedPointer The resulting AST root node representing the parsed program.
		 */
		uranite::ast::nodes::ProgramSharedPointer parse( uranite::diagnostic::Engine& diagnostic, const std::string& code ) {
			uranite::lexer::Lexer lexer( code, "test.urn", diagnostic );
			uranite::parser::Parser parser( lexer.tokenize(), diagnostic );
			return parser.parse();
		}
	
};

TEST_F( GenericsFixTest, Any ) {
}

TEST_F( GenericsFixTest, GenericClassWithMethodOnGenericParam ) {
	uranite::diagnostic::Engine diagnostic { 100, 100 };
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		diagnostic,
		"class Container<T>:\n"
		"    public T value\n"
		"    public function Container( self, T value ) -> Void:\n"
		"        self.value = value\n"
		"    public function getValue( self ) -> T:\n"
		"        return self.value\n"
	));
	ASSERT_NE( program, nullptr );
	if( program != nullptr ) {
		uranite::semantic::Analyzer analyzer( diagnostic );
		EXPECT_TRUE( analyzer.analyze( *program ) );
	}
}

TEST_F( GenericsFixTest, GenericStructMonomorphizationViaClass ) {
	uranite::diagnostic::Engine diagnostic { 100, 100 };
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		diagnostic,
		"struct Pair<K, V>:\n"
		"    public K key\n"
		"    public V value\n"
		"    public function Pair( self, K key, V value ) -> Void:\n"
		"        self.key = key\n"
		"        self.value = value\n"
		"class Holder<K, V>:\n"
		"    public Pair<K, V> pair\n"
		"    public function Holder( self, K key, V value ) -> Void:\n"
		"        self.pair = new Pair<K, V>( key, value )\n"
	));
	if( program != nullptr ) {
		uranite::semantic::Analyzer analyzer( diagnostic );
		bool success( analyzer.analyze( *program ) );
		if( success == false ) {
			for( const uranite::diagnostic::Diagnostic& diagnosted : diagnostic.diagnostics() ) {
				std::cerr << fmt::format( "{}:{}: error: {}", diagnosted.source->location->line, diagnosted.source->location->column, diagnosted.messages ) << std::endl;
			}
		}
		EXPECT_TRUE( success );
	}
	ASSERT_NE( program, nullptr );
}

TEST_F( GenericsFixTest, InterfaceImplementationWithGenericType ) {
	uranite::diagnostic::Engine diagnostic { 100, 100 };
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		diagnostic,
		"interface Iterator<T>:\n"
		"    public function next( self ) -> T\n"
		"class MyIterator<E> implements Iterator<E>:\n"
		"    public function next( self ) -> E:\n"
		"        return None as E\n"
		"class MyList<E>:\n"
		"    public function getIterator( self ) -> Iterator<E>:\n"
		"        return new MyIterator<E>()\n"
	));
	if( program != nullptr ) {
		uranite::semantic::Analyzer analyzer( diagnostic );
		bool success( analyzer.analyze( *program ) );
		if( success == false ) {
			for( const uranite::diagnostic::Diagnostic& diagnosted : diagnostic.diagnostics() ) {
				std::cerr << fmt::format( "{}:{}: error: {}", diagnosted.source->location->line, diagnosted.source->location->column, diagnosted.messages ) << std::endl;
			}
		}
		EXPECT_TRUE( success );
	}
	ASSERT_NE( program, nullptr );
}

TEST_F( GenericsFixTest, InterfaceMethodInheritanceChain ) {
	uranite::diagnostic::Engine diagnostic { 100, 100 };
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		diagnostic,
		"interface Base<T>:\n"
		"    public function size( self ) -> I64\n"
		"interface Middle<T> extends Base<T>:\n"
		"    public function get( self, I64 index ) -> T\n"
		"class Impl<E> implements Middle<E>:\n"
		"    public function size( self ) -> I64:\n"
		"        return 0\n"
		"    public function get( self, I64 index ) -> E:\n"
		"        return None as E\n"
	));
	ASSERT_NE( program, nullptr );
	if( program != nullptr ) {
		uranite::semantic::Analyzer analyzer( diagnostic );
		bool success( analyzer.analyze( *program ) );
		if( success == false ) {
			for( const uranite::diagnostic::Diagnostic& diagnosted : diagnostic.diagnostics() ) {
				std::cerr << fmt::format( "{}:{}: error: {}", diagnosted.source->location->line, diagnosted.source->location->column, diagnosted.messages ) << std::endl;
			}
		}
		EXPECT_TRUE( success );
	}
	ASSERT_NE( program, nullptr );
}

TEST_F( GenericsFixTest, SelfToGenericSpecialization ) {
	uranite::diagnostic::Engine diagnostic { 100, 100 };
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		diagnostic,
		"class Box<T>:\n"
		"    public function getSelf( self ) -> Box<T>:\n"
		"        return self\n"
	));
	if( program != nullptr ) {
		uranite::semantic::Analyzer analyzer( diagnostic );
		EXPECT_TRUE( analyzer.analyze( *program ) );
	}
	ASSERT_NE( program, nullptr );
}

TEST_F( GenericsFixTest, StructSubstituteTypeInGenericClass ) {
	uranite::diagnostic::Engine diagnostic { 100, 100 };
	uranite::ast::nodes::ProgramSharedPointer program( this->parse(
		diagnostic,
		"struct Entry<K, V>:\n"
		"    public K key\n"
		"    public V value\n"
		"    public function Entry( self, K key, V value ) -> Void:\n"
		"        self.key = key\n"
		"        self.value = value\n"
		"\n"
		"class Map<K, V>:\n"
		"    public Entry<K, V> first\n"
		"    public function Map( self, K key, V value ) -> Void:\n"
		"        self.first = new Entry<K, V>( key, value )\n"
	));
	if( program != nullptr ) {
		uranite::semantic::Analyzer analyzer( diagnostic );
		bool success( analyzer.analyze( *program ) );
		if( success == false ) {
			for( const uranite::diagnostic::Diagnostic& diagnosted : diagnostic.diagnostics() ) {
				std::cerr << fmt::format( "{}:{}: error: {}", diagnosted.source->location->line, diagnosted.source->location->column, diagnosted.messages ) << std::endl;
			}
		}
		EXPECT_TRUE( success );
	}
	ASSERT_NE( program, nullptr );
}
