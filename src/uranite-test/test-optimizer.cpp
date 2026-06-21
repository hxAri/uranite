
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
#include "uranite/optimizer/optimizer.hpp"

/**
 * @class OptimizerTest
 * @brief Test fixture for the AST Optimizer component, inheriting from Google Test's ::testing::Test.
 * * This fixture manages the baseline diagnostic system and provides utilities to 
 * validate Abstract Syntax Tree (AST) optimization passes, transformations, and reductions.
 */
class OptimizerTest : public ::testing::Test {
	
	protected:
		
		/**
		 * @brief Helper method to run full frontend parsing and execute optimization passes on the resulting AST.
		 * * Constructs the pipeline sequentially (Lexer -> Parser -> Optimizer) and invokes 
		 * the optimization routines based on the specified optimization level.
		 * * @param code The raw source code string to be tokenized, parsed, and optimized.
		 * @param level The target optimization intensity level (defaults to uranite::optimizer::Level::O0).
		 * @return uranite::optimizer::Statistic A snapshot of metrics gathered during the optimization process 
		 * (e.g., number of dead nodes removed, constant foldings applied).
		 */
		uranite::optimizer::Statistic optimize( const std::string& code, uranite::optimizer::Level level=uranite::optimizer::Level::O0 ) {
			uranite::diagnostic::Engine diagnostic { 100, 100 };
			uranite::lexer::Lexer lexer( code, "test.urn", diagnostic );
			uranite::parser::Parser parser( lexer.tokenize(), diagnostic );
			uranite::optimizer::Optimizer optimizer( diagnostic, level );
			optimizer.optimize( *parser.parse() );
			return optimizer.stats();
		}
	
};

TEST_F( OptimizerTest, Any ) {
}

TEST_F( OptimizerTest, ConstantFolding ) {
	uranite::optimizer::Statistic statistic( 
		this->optimize( 
			"function test() -> I32:\n"
			"    const I32 x = 2 + 3\n"
			"    return x\n"
		)
	);
	EXPECT_GE( statistic.constantExpressionResionEvaluated, 0 );
}
