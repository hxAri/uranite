
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
#include "uranite/ir/mir-printer.hpp"
#include "uranite/lexer/lexer.hpp"
#include "uranite/parser/parser.hpp"
#include "uranite/semantic/analyzer.hpp"

/**
 * @class MIRLoweringTest
 * @brief Test fixture for verifying the lowering pass from HIR to MIR layers.
 * * This fixture inherits from Google Test's ::testing::Test and focuses specifically on
 * validating the structural correctness, instruction flattening, and symbol preservation
 * during the transformation from High-level IR to Medium-level IR.
 */
class MIRLoweringTest : public ::testing::Test {
		
	protected:
		
		/**
		 * @brief Helper method to lower raw source code through the complete compiler frontend into an MIR Module.
		 * * Automatically orchestrates the execution of every prerequisite compiler pass in sequence:
		 * Lexer -> Parser -> Semantic Analyzer -> HIR Lowering -> MIR Lowering.
		 * * This utility simplifies the setup of individual lower-bound tests by abstracting the 
		 * complete pipeline construction, returning a fully realized MIR Module ready for structural assertions.
		 * Standard console diagnostic prints are disabled to keep test execution logs clean.
		 * * @param code The raw source code string to be processed and lowered.
		 * @return std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> Shared pointer to the newly generated MIR Module definition.
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

TEST_F( MIRLoweringTest, BreakJumpsToLoopExit ) {
    std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
        "function search( I64 limit ) -> Void:\n"
        "    mutable I64 index = 0\n"
        "    while index < limit:\n"
        "        if index > 5:\n"
        "            break\n"
        "        index = index + 1\n"
    );
    ASSERT_NE( mirModule, nullptr );
    ASSERT_EQ( mirModule->functionDefinitions.size(), 1 );
    std::shared_ptr<uranite::ir::mir::MIRFunctionDefinition>& function = mirModule->functionDefinitions[0];
    bool foundJumpToExit = false;
    for( const std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& block : function->controlFlowBlocks ) {
        for( const uranite::ir::mir::MIRInstruction& instruction : block->blockInstructions ) {
            if( instruction.instructionKind == uranite::ir::mir::MIRInstructionKind::JumpUnconditional ) {
                foundJumpToExit = true;
            }
        }
    }
    EXPECT_TRUE( foundJumpToExit );
}

TEST_F( MIRLoweringTest, ClassMethodsLoweredAsFunctions ) {
    std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
        "class Counter:\n"
        "    public I64 value\n"
        "    public function Counter( self, I64 value ) -> Void:\n"
        "        self.value = value\n"
    );
    ASSERT_NE( mirModule, nullptr );
    ASSERT_GE( mirModule->functionDefinitions.size(), 1 );
    EXPECT_EQ( mirModule->functionDefinitions[0]->ownerClassQualifiedName, "Counter" );
    EXPECT_TRUE( mirModule->typeLayoutTable.count( "Counter" ) > 0 );
}

TEST_F( MIRLoweringTest, FunctionCallEmitsCallInstruction ) {
    std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
        "function greet() -> Void:\n"
        "    pass\n"
        "function main() -> Void:\n"
        "    greet()\n"
    );
    ASSERT_NE( mirModule, nullptr );
    ASSERT_GE( mirModule->functionDefinitions.size(), 2 );
	
    // Find the main function (second one)
    std::shared_ptr<uranite::ir::mir::MIRFunctionDefinition>& mainFunction = mirModule->functionDefinitions[1];
    EXPECT_EQ( mainFunction->functionName, "main" );
    bool foundCall = false;
    for( const std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& block : mainFunction->controlFlowBlocks ) {
        for( const uranite::ir::mir::MIRInstruction& instruction : block->blockInstructions ) {
            if( instruction.instructionKind == uranite::ir::mir::MIRInstructionKind::CallFunction ) {
                foundCall = true;
                EXPECT_EQ( instruction.calledFunctionQualifiedName, "greet" );
            }
        }
    }
    EXPECT_TRUE( foundCall );
}

TEST_F( MIRLoweringTest, FunctionHasEntryBlock ) {
    std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
        "function noop() -> Void:\n"
        "    pass\n"
    );
    ASSERT_NE( mirModule, nullptr );
    ASSERT_EQ( mirModule->functionDefinitions.size(), 1 );
    EXPECT_EQ( mirModule->functionDefinitions[0]->functionName, "noop" );
    ASSERT_GE( mirModule->functionDefinitions[0]->controlFlowBlocks.size(), 1 );
    EXPECT_EQ( mirModule->functionDefinitions[0]->entryBlockIdentifier, 0 );
}

TEST_F( MIRLoweringTest, IfStatementCreatesMultipleBlocks ) {
    std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
        "function check( I64 value ) -> I64:\n"
        "    if value > 0:\n"
        "        return 1\n"
        "    else:\n"
        "        return -1\n"
    );
    ASSERT_NE( mirModule, nullptr );
    ASSERT_EQ( mirModule->functionDefinitions.size(), 1 );
    std::shared_ptr<uranite::ir::mir::MIRFunctionDefinition>& function = mirModule->functionDefinitions[0];
    
	// entry + if.then + if.merge + if.else = at least 4 blocks
    EXPECT_GE( function->controlFlowBlocks.size(), 4 );
}

TEST_F( MIRLoweringTest, ParametersRegisteredAsVariables ) {
    std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
        "function add( I64 left, I64 right ) -> I64:\n"
        "    return left + right\n"
    );
    ASSERT_NE( mirModule, nullptr );
    ASSERT_EQ( mirModule->functionDefinitions.size(), 1 );
    std::shared_ptr<uranite::ir::mir::MIRFunctionDefinition>& function = mirModule->functionDefinitions[0];
    EXPECT_EQ( function->parameterVariableIdentifiers.size(), 2 );
    for( uranite::ir::mir::MIRVariableIdentifier paramVariable : function->parameterVariableIdentifiers ) {
        EXPECT_TRUE( function->variableDescriptorTable.count( paramVariable ) > 0 );
        EXPECT_TRUE( function->variableDescriptorTable[paramVariable].isParameterVariable );
    }
}

TEST_F( MIRLoweringTest, PrinterProducesOutput ) {
    std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
        "function add( I64 left, I64 right ) -> I64:\n"
        "    return left + right\n"
    );
    ASSERT_NE( mirModule, nullptr );
    uranite::ir::mir::MIRPrinter printer;
    std::string output = printer.print( *mirModule );
    EXPECT_FALSE( output.empty() );
    EXPECT_NE( output.find( "Function add" ), std::string::npos );
    EXPECT_NE( output.find( "ret" ), std::string::npos );
}

TEST_F( MIRLoweringTest, ReturnTerminatesBlock ) {
    std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
        "function identity( I64 value ) -> I64:\n"
        "    return value\n"
    );
    ASSERT_NE( mirModule, nullptr );
    ASSERT_EQ( mirModule->functionDefinitions.size(), 1 );
    std::shared_ptr<uranite::ir::mir::MIRFunctionDefinition>& function = mirModule->functionDefinitions[0];
    ASSERT_GE( function->controlFlowBlocks.size(), 1 );
    std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& entryBlock = function->controlFlowBlocks[0];
    EXPECT_TRUE( entryBlock->isTerminated );
    ASSERT_GE( entryBlock->blockInstructions.size(), 1 );
    uranite::ir::mir::MIRInstruction& lastInstruction = entryBlock->blockInstructions.back();
    EXPECT_EQ( lastInstruction.instructionKind, uranite::ir::mir::MIRInstructionKind::ReturnValue );
}

TEST_F( MIRLoweringTest, VariableDeclarationEmitsAllocAndStore ) {
    std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
        "function init() -> Void:\n"
        "    I64 count = 42\n"
    );
    ASSERT_NE( mirModule, nullptr );
    ASSERT_EQ( mirModule->functionDefinitions.size(), 1 );
    std::shared_ptr<uranite::ir::mir::MIRFunctionDefinition>& function = mirModule->functionDefinitions[0];
    ASSERT_GE( function->controlFlowBlocks.size(), 1 );
    bool foundAlloc = false;
    bool foundStore = false;
    for( const uranite::ir::mir::MIRInstruction& instruction : function->controlFlowBlocks[0]->blockInstructions ) {
        if( instruction.instructionKind == uranite::ir::mir::MIRInstructionKind::AllocateLocal ) {
            foundAlloc = true;
        }
        if( instruction.instructionKind == uranite::ir::mir::MIRInstructionKind::StoreVariable ) {
            foundStore = true;
        }
    }
    EXPECT_TRUE( foundAlloc );
    EXPECT_TRUE( foundStore );
}

TEST_F( MIRLoweringTest, WhileLoopCreatesHeaderBodyExitBlocks ) {
    std::shared_ptr<uranite::ir::mir::MIRModuleDefinition> mirModule = this->lowerToMIR(
        "function countdown( I64 start ) -> Void:\n"
        "    mutable I64 counter = start\n"
        "    while counter > 0:\n"
        "        counter = counter - 1\n"
    );
    ASSERT_NE( mirModule, nullptr );
    ASSERT_EQ( mirModule->functionDefinitions.size(), 1 );
    std::shared_ptr<uranite::ir::mir::MIRFunctionDefinition>& function = mirModule->functionDefinitions[0];
    
	// entry + loop.header + loop.body + loop.exit = at least 4 blocks
    EXPECT_GE( function->controlFlowBlocks.size(), 4 );
	
    // Find loop header — should have a conditional branch
    bool foundConditionalBranch = false;
    for( const std::shared_ptr<uranite::ir::mir::MIRBasicBlock>& block : function->controlFlowBlocks ) {
        for( const uranite::ir::mir::MIRInstruction& instruction : block->blockInstructions ) {
            if( instruction.instructionKind == uranite::ir::mir::MIRInstructionKind::BranchConditional ) {
                foundConditionalBranch = true;
            }
        }
    }
    EXPECT_TRUE( foundConditionalBranch );
}
