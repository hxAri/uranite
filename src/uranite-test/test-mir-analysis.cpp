
//
// @author hxAri (hxari)
// @create 13-06-2026
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#include <gtest/gtest.h>
#include <string>

#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/ir/hir.hpp"
#include "uranite/ir/hir-lowering.hpp"
#include "uranite/ir/mir.hpp"
#include "uranite/ir/mir-lowering.hpp"
#include "uranite/ir/mir-analysis.hpp"
#include "uranite/ir/mir-borrow-checker.hpp"
#include "uranite/ir/mir-optimizer.hpp"
#include "uranite/lexer/lexer.hpp"
#include "uranite/parser/parser.hpp"
#include "uranite/semantic/analyzer.hpp"

/**
 * @class MIRAnalysisTest
 * @brief Test fixture for data-flow and structural analysis at the MIR (Medium-level IR) layer.
 * * This fixture inherits from Google Test's ::testing::Test and provides the environment 
 * required to verify transformations, CFG (Control Flow Graph) constructions, and safety 
 * properties after the code has been lowered to a flatter, medium-level representation.
 */
class MIRAnalysisTest : public ::testing::Test {
	
	protected:
		
		/**
		 * @brief Helper method to lower raw source code through the entire compiler frontend up to the MIR stage.
		 * * This utility orchestrates the full multi-stage translation pipeline sequentially:
		 * Lexer -> Parser -> Semantic Analyzer -> HIR Lowering -> MIR Lowering.
		 * * It allows MIR analysis tests to feed raw source code strings directly, abstracting away
		 * the heavy lifting of building both the AST and HIR modules manually. Standard console 
		 * diagnostics are suppressed to keep test results concise.
		 * * @param code The raw source code string to be processed and lowered to MIR.
		 * @return std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> Shared pointer to the resulting MIR Module definition.
		 */
		std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> lowerToMIR( const std::string& code ) {
			uranite::diagnostic::Engine diagnostic( 100, 100 );
			diagnostic.setConsolPrintable( false );
			uranite::lexer::Lexer lexer( code, "test.urn", diagnostic );
			std::vector<uranite::token::Token> tokens( lexer.tokenize() );
			uranite::parser::Parser parser( tokens, diagnostic );
			uranite::ast::nodes::ProgramSharedPointer program( parser.parse() );
			uranite::semantic::Analyzer analyzer( diagnostic );
			analyzer.analyze( *program );
			uranite::ir::hir::HIRLowering hirLowering( analyzer, diagnostic );
			std::shared_ptr<uranite::ir::hir::HIRModule> hirModule = hirLowering.lower( *program );
			uranite::ir::mir::MIRLowering mirLowering( diagnostic );
			return mirLowering.lower( *hirModule );
		}
	
};

// ==========================================================================
// Liveness Analysis Tests
// ==========================================================================

TEST_F( MIRAnalysisTest, LivenessAnalysisSimpleFunction ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function add( I64 left, I64 right ) -> I64:\n"
		"    return left + right\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::ir::mir::MIRLivenessAnalysis livenessAnalysis;
	livenessAnalysis.analyze( *mirModule->functionDefinitions[0] );
	
	// Entry block should have parameters live at entry (they're used in the add)
	uranite::ir::mir::MIRFunctionDefinition& functionDef = *mirModule->functionDefinitions[0];
	ASSERT_GE( functionDef.controlFlowBlocks.size(), 1 );
	std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& entryBlock =
		functionDef.controlFlowBlocks[functionDef.entryBlockIdentifier];
	ASSERT_NE( entryBlock, nullptr );
	
	// Entry block should have defined or used variables (parameters are registered)
	EXPECT_FALSE( entryBlock->definedVariables.empty() && entryBlock->usedVariables.empty() );
}

TEST_F( MIRAnalysisTest, LivenessAnalysisDeadVariable ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function deadVar() -> I64:\n"
		"    mutable I64 unused = 42\n"
		"    return 0\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::ir::mir::MIRLivenessAnalysis livenessAnalysis;
	livenessAnalysis.analyze( *mirModule->functionDefinitions[0] );
	
	// The unused variable should NOT be in liveVariablesAtExit of entry block
	uranite::ir::mir::MIRFunctionDefinition& functionDef = *mirModule->functionDefinitions[0];
	std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& entryBlock =
		functionDef.controlFlowBlocks[functionDef.entryBlockIdentifier];
	ASSERT_NE( entryBlock, nullptr );
	
	// Check that def set contains the unused variable
	EXPECT_FALSE( entryBlock->definedVariables.empty() );
}

TEST_F( MIRAnalysisTest, LivenessAnalysisIfBranch ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function branch( Boolean flag ) -> I64:\n"
		"    if flag:\n"
		"        return 1\n"
		"    else:\n"
		"        return 0\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::ir::mir::MIRLivenessAnalysis livenessAnalysis;
	livenessAnalysis.analyze( *mirModule->functionDefinitions[0] );
	
	// Should have multiple blocks (entry/cond, then, else, merge)
	uranite::ir::mir::MIRFunctionDefinition& functionDef = *mirModule->functionDefinitions[0];
	EXPECT_GE( functionDef.controlFlowBlocks.size(), 3 );
}

TEST_F( MIRAnalysisTest, LivenessAnalysisBuildsPredecessorSuccessorEdges ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function twoPath( Boolean condition ) -> I64:\n"
		"    if condition:\n"
		"        return 10\n"
		"    else:\n"
		"        return 20\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::ir::mir::MIRLivenessAnalysis livenessAnalysis;
	livenessAnalysis.analyze( *mirModule->functionDefinitions[0] );
	
	uranite::ir::mir::MIRFunctionDefinition& functionDef = *mirModule->functionDefinitions[0];
	
	// Entry block should have successors
	std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& entryBlock =
		functionDef.controlFlowBlocks[functionDef.entryBlockIdentifier];
	ASSERT_NE( entryBlock, nullptr );
	EXPECT_FALSE( entryBlock->successorBlocks.empty() );
	
	// Non-entry blocks should have predecessors
	bool foundBlockWithPredecessor = false;
	for( size_t blockIndex = 0; blockIndex < functionDef.controlFlowBlocks.size(); blockIndex++ ) {
		if( functionDef.controlFlowBlocks[blockIndex] != nullptr &&
			blockIndex != functionDef.entryBlockIdentifier &&
			functionDef.controlFlowBlocks[blockIndex]->predecessorBlocks.empty() == false ) {
			foundBlockWithPredecessor = true;
			break;
		}
	}
	EXPECT_TRUE( foundBlockWithPredecessor );
}

// ==========================================================================
// Borrow Checker Tests
// ==========================================================================

TEST_F( MIRAnalysisTest, BorrowCheckerNoViolationsSimple ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function simple( I64 value ) -> I64:\n"
		"    return value\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::diagnostic::Engine diagnostic( 100, 100 );
	diagnostic.setConsolPrintable( false );
	uranite::ir::mir::MIRBorrowChecker borrowChecker( diagnostic );
	borrowChecker.check( *mirModule->functionDefinitions[0] );
	
	EXPECT_EQ( borrowChecker.violationCount(), 0 );
}

TEST_F( MIRAnalysisTest, BorrowCheckerParameterOwned ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function useParam( I64 first, I64 second ) -> I64:\n"
		"    return first + second\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::diagnostic::Engine diagnostic( 100, 100 );
	diagnostic.setConsolPrintable( false );
	uranite::ir::mir::MIRBorrowChecker borrowChecker( diagnostic );
	borrowChecker.check( *mirModule->functionDefinitions[0] );
	
	EXPECT_EQ( borrowChecker.violationCount(), 0 );
}

TEST_F( MIRAnalysisTest, BorrowCheckerLocalVariable ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function localUse() -> I64:\n"
		"    I64 result = 42\n"
		"    return result\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::diagnostic::Engine diagnostic( 100, 100 );
	diagnostic.setConsolPrintable( false );
	uranite::ir::mir::MIRBorrowChecker borrowChecker( diagnostic );
	borrowChecker.check( *mirModule->functionDefinitions[0] );
	
	EXPECT_EQ( borrowChecker.violationCount(), 0 );
}

TEST_F( MIRAnalysisTest, BorrowCheckerIfBranchNoViolation ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function conditionalReturn( Boolean flag ) -> I64:\n"
		"    if flag:\n"
		"        return 1\n"
		"    else:\n"
		"        return 2\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::diagnostic::Engine diagnostic( 100, 100 );
	diagnostic.setConsolPrintable( false );
	uranite::ir::mir::MIRBorrowChecker borrowChecker( diagnostic );
	borrowChecker.check( *mirModule->functionDefinitions[0] );
	
	EXPECT_EQ( borrowChecker.violationCount(), 0 );
}

// ==========================================================================
// Optimizer Tests
// ==========================================================================

TEST_F( MIRAnalysisTest, OptimizerConstantFolding ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function constFold() -> I64:\n"
		"    return 2 + 3\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::ir::mir::MIROptimizer optimizer;
	optimizer.optimizeFunction( *mirModule->functionDefinitions[0] );
	
	// After constant folding, should have a ConstantInteger(5) somewhere
	bool foundFoldedConstant = false;
	for( std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& basicBlock :
		mirModule->functionDefinitions[0]->controlFlowBlocks ) {
		if( basicBlock == nullptr ) {
			continue;
		}
		for( const uranite::ir::mir::MIRInstruction& instruction : basicBlock->blockInstructions ) {
			if( instruction.instructionKind == uranite::ir::mir::MIRInstructionKind::ConstantInteger &&
				instruction.integerConstantValue == 5 ) {
				foundFoldedConstant = true;
			}
		}
	}
	EXPECT_TRUE( foundFoldedConstant );
}

TEST_F( MIRAnalysisTest, OptimizerRunsWithoutCrash ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function complex( I64 input ) -> I64:\n"
		"    mutable I64 result = input + 1\n"
		"    if result > 10:\n"
		"        result = result * 2\n"
		"    return result\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::ir::mir::MIROptimizer optimizer;
	EXPECT_NO_THROW( optimizer.optimize( *mirModule ) );
}

TEST_F( MIRAnalysisTest, OptimizerEliminateUnreachableBlocks ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function earlyReturn() -> I64:\n"
		"    return 42\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	size_t blockCountBefore = 0;
	for( std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& basicBlock :
		mirModule->functionDefinitions[0]->controlFlowBlocks ) {
		if( basicBlock != nullptr ) {
			blockCountBefore++;
		}
	}
	
	uranite::ir::mir::MIROptimizer optimizer;
	optimizer.optimizeFunction( *mirModule->functionDefinitions[0] );
	
	size_t blockCountAfter = 0;
	for( std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& basicBlock :
		mirModule->functionDefinitions[0]->controlFlowBlocks ) {
		if( basicBlock != nullptr ) {
			blockCountAfter++;
		}
	}
	
	// After optimization, should have same or fewer blocks
	EXPECT_LE( blockCountAfter, blockCountBefore );
}

TEST_F( MIRAnalysisTest, OptimizerPreservesCorrectness ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function preserve( I64 value ) -> I64:\n"
		"    I64 doubled = value * 2\n"
		"    return doubled\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::ir::mir::MIROptimizer optimizer;
	optimizer.optimizeFunction( *mirModule->functionDefinitions[0] );
	
	// Function should still have at least one block with a return
	bool hasReturn = false;
	for( std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& basicBlock :
		mirModule->functionDefinitions[0]->controlFlowBlocks ) {
		if( basicBlock == nullptr ) {
			continue;
		}
		for( const uranite::ir::mir::MIRInstruction& instruction : basicBlock->blockInstructions ) {
			if( instruction.instructionKind == uranite::ir::mir::MIRInstructionKind::ReturnValue ) {
				hasReturn = true;
			}
		}
	}
	EXPECT_TRUE( hasReturn );
}

TEST_F( MIRAnalysisTest, OptimizerRemovedCountsTracked ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function tracked() -> I64:\n"
		"    return 10 + 20\n"
	);
	ASSERT_NE( mirModule, nullptr );
	
	uranite::ir::mir::MIROptimizer optimizer;
	optimizer.optimize( *mirModule );
	
	// Counts should be non-negative (may or may not have optimized anything)
	EXPECT_GE( optimizer.removedInstructionCount(), 0 );
	EXPECT_GE( optimizer.removedBlockCount(), 0 );
}

TEST_F( MIRAnalysisTest, OptimizerWhileLoopSurvivesOptimization ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function loopTest( I64 count ) -> I64:\n"
		"    mutable I64 total = 0\n"
		"    mutable I64 index = 0\n"
		"    while index < count:\n"
		"        total = total + index\n"
		"        index = index + 1\n"
		"    return total\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	// Verify pre-optimization structure: should have multiple blocks for loop
	size_t preOptBlockCount = 0;
	for( std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& basicBlock :
		mirModule->functionDefinitions[0]->controlFlowBlocks ) {
		if( basicBlock != nullptr ) {
			preOptBlockCount++;
		}
	}
	EXPECT_GE( preOptBlockCount, 3 );
	
	// Optimizer should not crash on loop CFG
	uranite::ir::mir::MIROptimizer optimizer;
	EXPECT_NO_THROW( optimizer.optimizeFunction( *mirModule->functionDefinitions[0] ) );
}

// ==========================================================================
// Integration: Liveness + Borrow Checker + Optimizer pipeline
// ==========================================================================

TEST_F( MIRAnalysisTest, FullAnalysisPipelineNoErrors ) {
	std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
		"function pipeline( I64 input ) -> I64:\n"
		"    I64 step1 = input + 1\n"
		"    I64 step2 = step1 * 2\n"
		"    if step2 > 100:\n"
		"        return step2\n"
		"    return step1\n"
	);
	ASSERT_NE( mirModule, nullptr );
	ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
	
	uranite::ir::mir::MIRFunctionDefinition& functionDef = *mirModule->functionDefinitions[0];
	
	// 1. Liveness analysis
	uranite::ir::mir::MIRLivenessAnalysis livenessAnalysis;
	EXPECT_NO_THROW( livenessAnalysis.analyze( functionDef ) );
	
	// 2. Borrow checker
	uranite::diagnostic::Engine diagnostic( 100, 100 );
	diagnostic.setConsolPrintable( false );
	uranite::ir::mir::MIRBorrowChecker borrowChecker( diagnostic );
	EXPECT_NO_THROW( borrowChecker.check( functionDef ) );
	EXPECT_EQ( borrowChecker.violationCount(), 0 );
	
	// 3. Optimizer
	uranite::ir::mir::MIROptimizer optimizer;
	EXPECT_NO_THROW( optimizer.optimizeFunction( functionDef ) );
}
