
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

#ifndef _URANITE_COMPILER_OPTION_HPP_
#define _URANITE_COMPILER_OPTION_HPP_

#include <string>
#include <vector>

#include "uranite/optimizer/optimizer.hpp"

namespace uranite::compiler {
	
	/**
	 * @brief Represents the compilation output configuration.
	 * 
	 * This structure holds the necessary metadata for the compiler to determine
	 * what type of artifact to produce and where to save it.
	 */
	struct Output final {
		
		/** @brief Defines the specific stage or format of the output. */
		enum class Kind {
			AST,        ///< Print the Abstract Syntax Tree.
			Tokens,     ///< Print the generated token stream.
			LLVMIR,     ///< Output the LLVM Intermediate Representation (.ll).
			Object,     ///< Output the machine-specific object file (.o).
			Executable  ///< Output the final linked executable.
		};
		
		/** @brief The path to the input source file. */
		std::string source;
		
		/** @brief The path where the output artifact will be saved. */
		std::string target;
		
		/** @brief The desired output format, defaults to Kind::Executable. */
		Kind kind = Kind::Executable;
		
	};
	
	/**
	 * @brief Configuration options for the Uranite compiler and execution environment.
	 * 
	 * This structure holds settings related to code generation, optimization levels,
	 * debugging outputs, and search paths for libraries and modules.
	 */
	struct Options final {
		
		/** @brief List of command-line arguments to be passed to the compiled program. */
		std::vector<std::string> arguments;
		
		/** @brief Flag to enable dumping the Abstract Syntax Tree to the console. */
		bool dumpAST = false;
		
		/** @brief Flag to enable dumping the High-Level IR to the console. */
		bool dumpHIR = false;
		
		/** @brief Flag to enable dumping the Mid-Level IR to the console. */
		bool dumpMIR = false;
		
		/** @brief Flag to enable dumping the LLVM Intermediate Representation. */
		bool dumpIR = false;
		
		/** @brief Flag to enable dumping the lexer tokens to the console. */
		bool dumpTokens = false;
		
		/** @brief Flag to automatically execute the program after successful compilation. */
		bool executeAfterCompilation = false;
		
		/** @brief List of directory paths to search for header/include files. */
		std::vector<std::string> includePaths;
		
		/** @brief List of external libraries to link against the output binary. */
		std::vector<std::string> linkLibraries;
		
		/** @brief The maximum number of diagnostic errors allowed before aborting. */
		uint32_t maximumErrorCount = 1;
		
		/** @brief The file system path where Uranite modules are located. */
		std::string modulesPath;
		
		/** @brief The optimization level to be applied by the compiler backend. */
		optimizer::Level optimization = optimizer::Level::O2;
		
		/** @brief The output target configuration for the compilation process. */
		Output output;
		
		/** @brief Flag to enable Read-Eval-Print Loop mode for interactive execution. */
		bool replMode = false;
		
		/** @brief Flag to skip the linking phase and only produce object files. */
		bool skipLinking = false;
		
		/** @brief Flag to strip debug information from the final output binary. */
		bool stripDebugInfo = true;
		
		/** @brief The target architecture triple for cross-compilation. */
		std::string targetTriple;
		
		/** @brief Flag to enable verbose logging during the compilation process. */
		bool verbose = false;
		
	};
	
} // namespace uranite::compiler

#endif // end _URANITE_COMPILER_OPTION_HPP_
