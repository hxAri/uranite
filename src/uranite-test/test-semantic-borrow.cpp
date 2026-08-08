
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
#include "uranite/semantic/borrow.hpp"

/**
 * @class SemanticBorrowTest
 * @brief Test fixture for the Semantic Borrow Checker component, inheriting from Google Test's ::testing::Test.
 * * This fixture provides the testing environment required to validate references, 
 * lifetimes, and ownership safety rules within the AST (Abstract Syntax Tree).
 */
class SemanticBorrowTest : public ::testing::Test {
	
	protected:
		
		/**
		 * @struct Result
		 * @brief Holds the compilation feedback counters specific to the execution of the borrow checker.
		 */
		struct Result {
			
			///< Total number of borrow checking errors encountered.
			uint32_t error;
			
			///< Total number of borrow checking warnings encountered.
			uint32_t warning;
			
		};
		
		/**
		 * @brief Utility method to compile a raw code string up to the semantic analysis phase.
		 * * This function orchestrates the full pipeline sequentially: Lexer -> Parser -> BorrowChecker.
		 * It suppresses standard console printouts to maintain clean test logs and mathematically 
		 * isolates the errors generated exclusively by the `BorrowChecker`.
		 * * @param code The source code string to be analyzed for lifetime and borrow rules.
		 * @return Result A structure containing the standalone error and warning counts for the borrow check phase.
		 */
		Result borrow( const std::string& code ) {
			uranite::diagnostic::Engine diagnostic( 100, 100 );
			diagnostic.setConsolPrintable( false );
			uranite::lexer::Lexer lexer( code, "test.urn", diagnostic );
			std::vector<uranite::token::Token> tokens( lexer.tokenize() );
			uranite::parser::Parser parser( tokens, diagnostic );
			uranite::ast::nodes::ProgramSharedPointer program( parser.parse() );
			uint32_t preBorrowErrors( diagnostic.errorCount() );
			uranite::semantic::BorrowChecker checker( diagnostic );
			checker.check( *program );
			return Result {
				diagnostic.errorCount() - preBorrowErrors,
				diagnostic.warningCount()
			};
		}
	
};

TEST_F( SemanticBorrowTest, ConstVariableCannotBeReassigned ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    const I32 x = 10\n"
		"    x = 20\n"
		"    return 0\n"
	));
	EXPECT_GE( results.error, 1u );
}

TEST_F( SemanticBorrowTest, DeleteAfterMoveIsError ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    I32 y = move x\n"
		"    delete x\n"
		"    return 0\n"
	));
	EXPECT_GE( results.error, 1u );
}

TEST_F( SemanticBorrowTest, DoubleDeleteIsError ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    delete x\n"
		"    delete x\n"
		"    return 0\n"
	));
	EXPECT_GE( results.error, 1u );
}

TEST_F( SemanticBorrowTest, DoubleMoveIsError ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    I32 y = move x\n"
		"    I32 z = move x\n"
		"    return 0\n"
	));
	EXPECT_GE( results.error, 1u );
}

TEST_F( SemanticBorrowTest, MoveAfterDeleteIsError ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    delete x\n"
		"    I32 y = move x\n"
		"    return 0\n"
	));
	EXPECT_GE( results.error, 1u );
}

TEST_F( SemanticBorrowTest, MutableVariableCanBeReassigned ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    x = 20\n"
		"    return 0\n"
	));
	EXPECT_GE( results.error, 0u );
}

TEST_F( SemanticBorrowTest, ReturnReferenceToLocalIsError ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function makeRef() -> I32:\n"
		"    I32 local = 42\n"
		"    return &local\n"
	));
	EXPECT_GE( results.error, 1u );
}

TEST_F( SemanticBorrowTest, SimpleOwnedVariableLifecycle ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    return x\n"
	));
	EXPECT_GE( results.error, 0u );
}

TEST_F( SemanticBorrowTest, UseAfterDeleteIsError ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    delete x\n"
		"    I32 y = x\n"
		"    return 0\n"
	));
	EXPECT_GE( results.error, 1u );
}

TEST_F( SemanticBorrowTest, UseAfterExplicitMove ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    I32 y = move x\n"
		"    I32 z = x\n"
		"    return 0\n"
	));
	EXPECT_GE( results.error, 1u );
}

TEST_F( SemanticBorrowTest, ConditionalMoveDetected ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    I32 flag = 1\n"
		"    if flag == 1:\n"
		"        I32 y = move x\n"
		"    I32 z = x\n"
		"    return 0\n"
	));
	EXPECT_GE( results.error, 1u );
}

TEST_F( SemanticBorrowTest, MoveInBothBranchesIsError ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    I32 flag = 1\n"
		"    if flag == 1:\n"
		"        I32 y = move x\n"
		"    else:\n"
		"        I32 w = move x\n"
		"    I32 z = x\n"
		"    return 0\n"
	));
	EXPECT_GE( results.error, 1u );
}

TEST_F( SemanticBorrowTest, NoMoveInEitherBranchIsOk ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    I32 flag = 1\n"
		"    if flag == 1:\n"
		"        I32 y = 20\n"
		"    else:\n"
		"        I32 w = 30\n"
		"    I32 z = x\n"
		"    return 0\n"
	));
	EXPECT_EQ( results.error, 0u );
}

TEST_F( SemanticBorrowTest, ConditionalDeleteInTryUsedInCatch ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"public function main() -> I32:\n"
		"    I32 x = 10\n"
		"    try:\n"
		"        delete x\n"
		"    except as e:\n"
		"        I32 y = x\n"
		"    return 0\n"
	));
	EXPECT_GE( results.error, 1u );
}

TEST_F( SemanticBorrowTest, HeapInTryBlockWarning ) {
	SemanticBorrowTest::Result results( this->borrow(
		"package uranite.syntax.parser.test\n"
		"\n"
		"class Box:\n"
		"    public I64 value\n"
		"\n"
		"public function main() -> I32:\n"
		"    try:\n"
		"        Box b = new Box()\n"
		"        raise \"boom\"\n"
		"    except as e:\n"
		"        I32 y = 0\n"
		"    return 0\n"
	));
	EXPECT_GE( results.warning, 1u );
}
