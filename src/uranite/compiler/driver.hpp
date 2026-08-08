
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

#ifndef _URANITE_COMPILER_DRIVER_HPP_
#define _URANITE_COMPILER_DRIVER_HPP_

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "uranite/ast/node.hpp"
#include "uranite/compiler/option.hpp"
#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/semantic/symbol.hpp"
#include "uranite/semantic/typeref.hpp"

namespace uranite::compiler {
	
	struct ModuleInfo {
		ast::nodes::ProgramSharedPointer program;
		std::string packageName;
		std::string canonicalPath;
		std::set<std::string> exportedEntities;
		std::unordered_map<std::string, ast::AccessModifier> entityAccessMap;
		std::unordered_map<std::string, semantic::TypeSharedPointer> analyzedTypes;
		std::unordered_map<std::string, std::vector<semantic::SymbolSharedPointer>> analyzedSymbols;
		enum class State { Parsed, Analyzing, Analyzed };
		State state = State::Parsed;
	};
	
	/**
	 * @brief Orchestrates the entire compilation pipeline for the Uranite language.
	 * 
	 * The Driver class manages the transition between compilation phases, including
	 * lexing, parsing, semantic analysis, memory safety verification (borrow checking),
	 * optimization, and final code generation or linking.
	 */
	class Driver {
		
		public:
			
			/**
			 * @brief Constructs a Driver instance with the specified compilation options.
			 * 
			 * @param options The configuration settings that govern the compilation behavior.
			 */
			Driver( const Options& options );
			
			/**
			 * @brief Executes the standard compilation flow based on the provided options.
			 * 
			 * @return An integer representing the exit status code (0 for success).
			 */
			int run();
			
			/**
			 * @brief Starts the interactive Read-Eval-Print Loop (REPL) environment.
			 * 
			 * @return An integer representing the exit status code upon session termination.
			 */
			int runREPL();
			
			/** @brief Locates the system modules directory based on environment and options. */
			std::string findModulesDirectory() const;
			
			/** @brief Loads a specific module file and integrates it into the target AST. */
			bool loadModule( const std::string& filePath, ast::nodes::Program& targetProgram, const ast::nodes::ImportDeclaration* importDeclaration = nullptr );
			
			/** @brief Automatically imports the core language features into the program. */
			bool loadPrelude( ast::nodes::Program& program );
			
			/** @brief Iterates through all import declarations to resolve external dependencies. */
			bool resolveImports( ast::nodes::Program& program );
			
			/** @brief Resolves a logical module path into an absolute file system path. */
			std::string resolveModulePath( const std::vector<std::string>& modulePath );
			
			/**
			 * @brief Returns the stdlib directory segment for the active target architecture.
			 *
			 * Derived from the configured target triple, or the host triple when none is set.
			 * Used to rewrite the reserved "native" import segment (e.g.
			 * `uranite.os.arch.native.syscall`) onto the matching arch directory such as
			 * "x86-64" or "aarch64".
			 */
			std::string targetArchSegment() const;
			
			/** @brief Returns the collection of loaded and analyzed modules. */
			const std::unordered_map<std::string, ModuleInfo>& getModules() const { return modules; }
		
		private:
			
			/** @brief Reads the raw source code from the input file into memory. */
			int readSource();
			
			/** @brief Compiles IR to object or executable, links runtime, optionally runs. */
			int linkFromIR( const std::string& inputIrFile );
			
			/** @brief Diagnostic engine instance for reporting errors and warnings. */
			diagnostic::Engine diagnostic;
			
			/** @brief Cached result of modules directory lookup. */
			mutable std::string cachedModulesDirectory_;
			
			/** @brief Whether the modules directory has been resolved yet. */
			mutable bool modulesDirectoryCached_ = false;
			
			std::unordered_map<std::string, ModuleInfo> modules;
			
			std::unordered_map<std::string, semantic::TypeSharedPointer> accumulatedModuleTypes_;
			std::unordered_map<std::string, std::vector<semantic::SymbolSharedPointer>> accumulatedModuleSymbols_;
			
			/** @brief Maps include paths to their root package name from __mod__.urn. */
			std::unordered_map<std::string, std::string> includePathPackagePrefix_;
			
			/** @brief Whether include path package prefixes have been resolved. */
			bool includePathPrefixesCached_ = false;
			
			/** @brief Cached root package name from the entry file's __mod__.urn. */
			std::string selfPackagePrefix_;
			
			/** @brief Whether the self-package prefix has been resolved. */
			bool selfPackagePrefixCached_ = false;
			
			/** @brief Resolves and caches package prefixes for all include paths. */
			void resolveIncludePathPrefixes();
			
			/** @brief Local copy of the configuration settings for the current driver session. */
			Options options;
			
			/** @brief The primary source content file being compiled. */
			std::string source;
		
	};
	
} // namespace uranite::compiler

#endif // end _URANITE_COMPILER_DRIVER_HPP_
