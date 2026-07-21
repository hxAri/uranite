
//
// @author hxAri (hxari)
// @create 13-06-2026
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <string>

#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/ir/hir.hpp"
#include "uranite/ir/hir/lowering.hpp"
#include "uranite/ir/hir/printer.hpp"
#include "uranite/lexer/lexer.hpp"
#include "uranite/parser/parser.hpp"
#include "uranite/semantic/analyzer.hpp"

/**
 * @class HIRLoweringTest
 * @brief Test fixture for the HIR (High-level Intermediate Representation) Lowering phase.
 * * This fixture inherits from Google Test's ::testing::Test and provides the necessary
 * infrastructure to verify the translation of a fully analyzed Abstract Syntax Tree (AST)
 * into a structured High-level IR Module.
 */
class HIRLoweringTest : public ::testing::Test {
	
	protected:
		
		/**
		 * @brief Helper method to process raw source code through the frontend pipeline and lower it to HIR.
		 * * This utility orchestrates the execution of multiple compiler phases in order:
		 * Lexer -> Parser -> Semantic Analyzer -> HIR Lowering.
		 * * It isolates the lowering process inside a clean testing sandbox, passing the resolved
		 * symbol and type metadata from the `Analyzer` directly into the `HIRLowering` engine.
		 * Standard console diagnostics are disabled to ensure test reports remain uncluttered.
		 * * @param code The raw source code string to be tokenized, parsed, analyzed, and lowered.
		 * @return std::shared_ptr<uranite::ir::hir::HIRModule> Shared pointer to the generated HIR Module root node.
		 */
		std::shared_ptr<uranite::ir::hir::HIRModule> lowerSource( const std::string& code ) {
			uranite::diagnostic::Engine diagnostic( 100, 100 );
			diagnostic.setConsolPrintable( false );
			uranite::lexer::Lexer lexer( code, "test.urn", diagnostic );
			std::vector<uranite::token::Token> tokens( lexer.tokenize() );
			uranite::parser::Parser parser( tokens, diagnostic );
			uranite::ast::nodes::ProgramSharedPointer program( parser.parse() );
			uranite::semantic::Analyzer analyzer( diagnostic );
			analyzer.analyze( *program );
			uranite::ir::hir::HIRLowering lowering( analyzer, diagnostic );
			return lowering.lower( *program );
		}
	
};

TEST_F( HIRLoweringTest, DesugarsElifChainToNestedIf ) {
    std::shared_ptr<uranite::ir::hir::HIRModule> hirModule = this->lowerSource(
        "function classify( I64 value ) -> I64:\n"
        "    if value > 0:\n"
        "        return 1\n"
        "    elif value < 0:\n"
        "        return -1\n"
        "    else:\n"
        "        return 0\n"
    );
    ASSERT_NE( hirModule, nullptr );
    ASSERT_EQ( hirModule->functionDefinitions.size(), 1 );
    std::shared_ptr<uranite::ir::hir::HIRBlock> functionBody = hirModule->functionDefinitions[0]->functionBody;
    ASSERT_NE( functionBody, nullptr );
    ASSERT_GE( functionBody->blockStatements.size(), 1 );
    uranite::ir::hir::HIRNodeSharedPointer firstStatement = functionBody->blockStatements[0];
    ASSERT_EQ( firstStatement->nodeKind, uranite::ir::hir::HIRNodeKind::If );
    uranite::ir::hir::HIRIf& ifNode = static_cast<uranite::ir::hir::HIRIf&>( *firstStatement );
    ASSERT_NE( ifNode.elseBranch, nullptr );
    EXPECT_EQ( ifNode.elseBranch->nodeKind, uranite::ir::hir::HIRNodeKind::If );
}

TEST_F( HIRLoweringTest, LowersClassWithMethodsAndFields ) {
    std::shared_ptr<uranite::ir::hir::HIRModule> hirModule = this->lowerSource(
        "class Point:\n"
        "    public I64 coordX\n"
        "    public I64 coordY\n"
        "    public function Point( self, I64 coordX, I64 coordY ) -> Void:\n"
        "        self.coordX = coordX\n"
        "        self.coordY = coordY\n"
    );
    ASSERT_NE( hirModule, nullptr );
    ASSERT_EQ( hirModule->classDefinitions.size(), 1 );
    EXPECT_EQ( hirModule->classDefinitions[0]->className, "Point" );
    EXPECT_EQ( hirModule->classDefinitions[0]->fieldDescriptors.size(), 2 );
    EXPECT_EQ( hirModule->classDefinitions[0]->fieldDescriptors[0].fieldName, "coordX" );
    EXPECT_EQ( hirModule->classDefinitions[0]->fieldDescriptors[1].fieldName, "coordY" );
    EXPECT_GE( hirModule->classDefinitions[0]->methodDefinitions.size(), 1 );
}

TEST_F( HIRLoweringTest, LowersDeferStatement ) {
    std::shared_ptr<uranite::ir::hir::HIRModule> hirModule = this->lowerSource(
        "function cleanup() -> Void:\n"
        "    defer:\n"
        "        pass\n"
        "    return\n"
    );
    ASSERT_NE( hirModule, nullptr );
    ASSERT_EQ( hirModule->functionDefinitions.size(), 1 );
    std::shared_ptr<uranite::ir::hir::HIRBlock> functionBody = hirModule->functionDefinitions[0]->functionBody;
    ASSERT_NE( functionBody, nullptr );
    ASSERT_GE( functionBody->blockStatements.size(), 1 );
    EXPECT_EQ( functionBody->blockStatements[0]->nodeKind, uranite::ir::hir::HIRNodeKind::Defer );
}

TEST_F( HIRLoweringTest, LowersEnumDeclaration ) {
    std::shared_ptr<uranite::ir::hir::HIRModule> hirModule = this->lowerSource(
        "enum Direction:\n"
        "    unit North\n"
        "    unit South\n"
        "    unit East\n"
        "    unit West\n"
    );
    ASSERT_NE( hirModule, nullptr );
    ASSERT_EQ( hirModule->enumDefinitions.size(), 1 );
    EXPECT_EQ( hirModule->enumDefinitions[0]->enumName, "Direction" );
    EXPECT_EQ( hirModule->enumDefinitions[0]->variantDescriptors.size(), 4 );
}

TEST_F( HIRLoweringTest, LowersExternDeclaration ) {
    std::shared_ptr<uranite::ir::hir::HIRModule> hirModule = this->lowerSource(
        "extern function puts( String message ) -> I64\n"
    );
    ASSERT_NE( hirModule, nullptr );
    ASSERT_EQ( hirModule->externFunctionDeclarations.size(), 1 );
    EXPECT_EQ( hirModule->externFunctionDeclarations[0]->functionName, "puts" );
    EXPECT_EQ( hirModule->externFunctionDeclarations[0]->parameterDescriptors.size(), 1 );
}

TEST_F( HIRLoweringTest, LowersMatchStatement ) {
    std::shared_ptr<uranite::ir::hir::HIRModule> hirModule = this->lowerSource(
        "function dispatch( I64 code ) -> I64:\n"
        "    match code:\n"
        "        case 1:\n"
        "            return 10\n"
        "        case 2:\n"
        "            return 20\n"
        "    return 0\n"
    );
    ASSERT_NE( hirModule, nullptr );
    ASSERT_EQ( hirModule->functionDefinitions.size(), 1 );
    std::shared_ptr<uranite::ir::hir::HIRBlock> functionBody = hirModule->functionDefinitions[0]->functionBody;
    ASSERT_NE( functionBody, nullptr );
    ASSERT_GE( functionBody->blockStatements.size(), 1 );
}

TEST_F( HIRLoweringTest, LowersSimpleFunction ) {
    std::shared_ptr<uranite::ir::hir::HIRModule> hirModule = this->lowerSource(
        "function add( I64 left, I64 right ) -> I64:\n"
        "    return left + right\n"
    );
    ASSERT_NE( hirModule, nullptr );
    ASSERT_EQ( hirModule->functionDefinitions.size(), 1 );
    EXPECT_EQ( hirModule->functionDefinitions[0]->functionName, "add" );
    EXPECT_EQ( hirModule->functionDefinitions[0]->parameterDescriptors.size(), 2 );
    EXPECT_EQ( hirModule->functionDefinitions[0]->parameterDescriptors[0].parameterName, "left" );
    EXPECT_EQ( hirModule->functionDefinitions[0]->parameterDescriptors[1].parameterName, "right" );
    ASSERT_NE( hirModule->functionDefinitions[0]->functionBody, nullptr );
    EXPECT_GE( hirModule->functionDefinitions[0]->functionBody->blockStatements.size(), 1 );
}

TEST_F( HIRLoweringTest, LowersTryCatchStatement ) {
    std::shared_ptr<uranite::ir::hir::HIRModule> hirModule = this->lowerSource(
        "function risky() -> Void:\n"
        "    try:\n"
        "        pass\n"
        "    except error:\n"
        "        pass\n"
    );
    ASSERT_NE( hirModule, nullptr );
    ASSERT_EQ( hirModule->functionDefinitions.size(), 1 );
    std::shared_ptr<uranite::ir::hir::HIRBlock> functionBody = hirModule->functionDefinitions[0]->functionBody;
    ASSERT_NE( functionBody, nullptr );
    ASSERT_GE( functionBody->blockStatements.size(), 1 );
    EXPECT_EQ( functionBody->blockStatements[0]->nodeKind, uranite::ir::hir::HIRNodeKind::TryCatch );
}

TEST_F( HIRLoweringTest, LowersWhileLoopToHIRLoop ) {
    std::shared_ptr<uranite::ir::hir::HIRModule> hirModule = this->lowerSource(
        "function countdown( I64 start ) -> Void:\n"
        "    mutable I64 counter = start\n"
        "    while counter > 0:\n"
        "        counter = counter - 1\n"
    );
    ASSERT_NE( hirModule, nullptr );
    ASSERT_EQ( hirModule->functionDefinitions.size(), 1 );
    std::shared_ptr<uranite::ir::hir::HIRBlock> functionBody = hirModule->functionDefinitions[0]->functionBody;
    ASSERT_NE( functionBody, nullptr );
    ASSERT_GE( functionBody->blockStatements.size(), 3 );
    EXPECT_EQ( functionBody->blockStatements[0]->nodeKind, uranite::ir::hir::HIRNodeKind::VariableBinding );
    EXPECT_EQ( functionBody->blockStatements[1]->nodeKind, uranite::ir::hir::HIRNodeKind::Assignment );
    uranite::ir::hir::HIRNodeSharedPointer loopStatement = functionBody->blockStatements[2];
    ASSERT_EQ( loopStatement->nodeKind, uranite::ir::hir::HIRNodeKind::Loop );
    uranite::ir::hir::HIRLoop& loop = static_cast<uranite::ir::hir::HIRLoop&>( *loopStatement );
    EXPECT_NE( loop.loopCondition, nullptr );
    EXPECT_NE( loop.loopBody, nullptr );
    EXPECT_EQ( loop.isIteratorLoop, false );
}

TEST_F( HIRLoweringTest, PrinterProducesOutput ) {
    std::shared_ptr<uranite::ir::hir::HIRModule> hirModule = this->lowerSource(
        "function greet() -> Void:\n"
        "    pass\n"
    );
    ASSERT_NE( hirModule, nullptr );
    uranite::ir::hir::HIRPrinter printer;
    std::string output = printer.print( *hirModule );
    EXPECT_FALSE( output.empty() );
    EXPECT_NE( output.find( "HIRFunctionDefinition" ), std::string::npos );
    EXPECT_NE( output.find( "greet" ), std::string::npos );
}
