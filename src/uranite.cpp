
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

#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <fmt/color.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Config/llvm-config.h>
#include <optional>
#include <spdlog/spdlog.h>

#include "uranite/common/functions.hpp"
#include "uranite/compiler/option.hpp"
#include "uranite/compiler/driver.hpp"
#include "uranite/version.hpp"

static void banner() {
	fmt::print( fmt::fg( fmt::color::rebecca_purple ) | fmt::emphasis::bold,
		"\n"
		"  ░▒▓█▓▒░░▒▓█▓▒░▒▓███████▓▒░░▒▓█▓▒░░▒▓█▓▒░▒▓███████▓▒░   ░▒▓█▓▒░▒▓████████▓▒░▒▓███████▓▒░   \n"
		"  ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓████▓▒░  ░▒▓█▓▒░          ░▒▓█▓▒░  \n"
		"  ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░  ░▒▓█▓▒░          ░▒▓█▓▒░  \n"
		"  ░▒▓█▓▒░░▒▓█▓▒░▒▓███████▓▒░░▒▓████████▓▒░▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░  ░▒▓█▓▒░   ░▒▓███████▓▒░   \n"
		"  ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░      ░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░  ░▒▓█▓▒░          ░▒▓█▓▒░  \n"
		"  ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░      ░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░  ░▒▓█▓▒░          ░▒▓█▓▒░  \n"
		"   ░▒▓██████▓▒░░▒▓█▓▒░░▒▓█▓▒░      ░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░  ░▒▓█▓▒░   ░▒▓███████▓▒░   \n"
		"\n"
	);
	fmt::print( fmt::fg( fmt::color::gray ), "Build {} {}\n", __DATE__, __TIME__ );
	fmt::print( fmt::fg( fmt::color::gray ), "Compiler v{} | Language v{}\n", _URANITE_COMPILER_VERSION_, _URANITE_LANGUAGE_VERSION_ );
	#if defined( __GNUC__ ) && defined( __GNUC_MINOR__ ) && defined( __GNUC_PATCHLEVEL__ )
		#if defined( __clang_major__ ) & defined( __clang_major__ )
			fmt::print( fmt::fg( fmt::color::gray ), "Clang v{}.{} | GCC/G++ v{}.{}.{}\n",
				__clang_major__, 
				__clang_minor__
				__GNUC__, 
				__GNUC_MINOR__, 
				__GNUC_PATCHLEVEL__ 
			);
		#else
			fmt::print( fmt::fg( fmt::color::gray ), "GCC/G++ v{}.{}.{}\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__ );
		#endif
	#endif
	fmt::print( fmt::fg( fmt::color::gray ), "Signature {}\n\n", _URANITE_GIT_HASH_ );
}

int main( int argc, char* argv[] ) {
	argparse::ArgumentParser program( "uranite", _URANITE_COMPILER_VERSION_ );
	program.add_description( _URANITE_LANGUAGE_DESCRIPTION_ );
	program.add_argument( "input" )
		.help( "Input source file (.urn)" )
		.nargs( argparse::nargs_pattern::optional );
	program.add_argument( "-o", "--output" )
		.help( "Output file path" )
		.default_value( std::string( "" ) );
	program.add_argument( "-O", "--opt-level" )
		.help( "Optimization level (0,1,2,3,fast)" )
		.default_value( std::string( "2" ) );
	program.add_argument( "--emit-llvm" )
		.help( "Emit LLVM IR instead of executable" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--emit-obj" )
		.help( "Emit object file instead of executable" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "-c", "--compile-only" )
		.help( "Compile only, do not link" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--dump-tokens" )
		.help( "Dump lexer token stream" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--dump-ast" )
		.help( "Dump abstract syntax tree" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--dump-hir" )
		.help( "Dump High-Level IR after lowering" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--dump-mir" )
		.help( "Dump Mid-Level IR (CFG) after lowering" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--dump-ir" )
		.help( "Dump LLVM IR to stdout" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--use-mir" )
		.help( "Use MIR-based LLVM codegen instead of AST-direct codegen" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "-v", "--verbose" )
		.help( "Enable verbose output" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--no-strip" )
		.help( "Do not strip debug symbols from output" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "-l", "--link" )
		.help( "Link with library" )
		.append()
		.default_value( std::vector<std::string>{} );
	program.add_argument( "-r", "--run" )
		.help( "Compile and immediately execute the program" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--repl" )
		.help( "Start interactive REPL mode" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--max-errors" )
		.help( "Stop compilation after N errors (0=unlimited,1=default)" )
		.default_value( 1 )
		.scan<'i', int>();
	program.add_argument( "-M", "--modules-path" )
		.help( "Path to the Uranite standard library modules directory" )
		.default_value( std::string( "" ) );
	program.add_argument( "-I", "--include" )
		.help( "Additional directory to search for module imports" )
		.append()
		.default_value( std::vector<std::string>{} );
	program.add_argument( "--target" )
		.help( "Target triple for cross-compilation (e.g., aarch64-linux-gnu, x86_64-linux-gnu)" )
		.default_value( std::string( "" ) );
	program.add_argument( "--version-info" )
		.help( "Show detailed version information" )
		.default_value( false )
		.implicit_value( true );
	int argcForParser = argc;
	std::vector<std::string> programArguments;
	for( int i = 1; i < argc; i++ ) {
		if( std::string( argv[i] ) == "--" ) {
			argcForParser = i;
			for( int j = i + 1; j < argc; j++ ) {
				programArguments.push_back( argv[j] );
			}
			break;
		}
	}
	try {
		program.parse_args( argcForParser, argv );
	}
	catch( const std::runtime_error& e ) {
		return uranite::common::functions::printerr( e );
	}
	if( program.get<bool>( "--version-info" ) ) {
		banner();
		fmt::print( "Host: {}\n", llvm::sys::getDefaultTargetTriple() );
		fmt::print( "LLVM Version: {}\n", LLVM_VERSION_STRING );
		return 0;
	}
	if( program.get<bool>( "--repl" ) ) {
		spdlog::set_level( spdlog::level::warn );
		uranite::compiler::Options options;
		options.replMode = true;
		uranite::compiler::Driver driver( options );
		return driver.runREPL();
	}
	std::optional<std::string> inputOpt = program.present( "input" );
	if( inputOpt == std::nullopt ) {
		banner();
		fmt::print( stderr, "{}", program.help().str() );
		return 1;
	}
	uranite::compiler::Options options;
	options.output.source = *inputOpt;
	options.output.target = program.get<std::string>( "--output" );
	options.dumpTokens = program.get<bool>( "--dump-tokens" );
	options.dumpAST = program.get<bool>( "--dump-ast" );
	options.dumpHIR = program.get<bool>( "--dump-hir" );
	options.dumpMIR = program.get<bool>( "--dump-mir" );
	options.dumpIR = program.get<bool>( "--dump-ir" );
	options.useMIR = program.get<bool>( "--use-mir" );
	options.verbose = program.get<bool>( "--verbose" );
	options.stripDebugInfo = !program.get<bool>( "--no-strip" );
	options.linkLibraries = program.get<std::vector<std::string>>( "--link" );
	options.executeAfterCompilation = program.get<bool>( "--run" );
	options.maximumErrorCount = static_cast<uint32_t>( program.get<int>( "--max-errors" ) );
	options.modulesPath = program.get<std::string>( "--modules-path" );
	options.includePaths = program.get<std::vector<std::string>>( "--include" );
	options.targetTriple = program.get<std::string>( "--target" );
	options.arguments = programArguments;
	if( program.get<bool>( "--emit-llvm" ) ) {
		options.output.kind = uranite::compiler::Output::Kind::LLVMIR;
	}
	else if( program.get<bool>( "--emit-obj" ) || program.get<bool>( "--compile-only" ) ) {
		options.output.kind = uranite::compiler::Output::Kind::Object;
	}
	else if( program.get<bool>( "--dump-tokens" ) ) {
		options.output.kind = uranite::compiler::Output::Kind::Tokens;
	}
	else if( program.get<bool>( "--dump-ast" ) ) {
		options.output.kind = uranite::compiler::Output::Kind::AST;
	}
	else if( program.get<bool>( "--dump-ir" ) ) {
		options.output.kind = uranite::compiler::Output::Kind::LLVMIR;
	}
	std::string optimizationLevel = program.get<std::string>( "--opt-level" );
	if( optimizationLevel == "0" ) {
		options.optimization = uranite::optimizer::Level::O0;
	}
	else if( optimizationLevel == "1" ) {
		options.optimization = uranite::optimizer::Level::O1;
	}
	else if( optimizationLevel == "2" ) {
		options.optimization = uranite::optimizer::Level::O2;
	}
	else if( optimizationLevel == "3" ) {
		options.optimization = uranite::optimizer::Level::O3;
	}
	else if( optimizationLevel == "fast" ) {
		options.optimization = uranite::optimizer::Level::OFast;
	}
	if( options.verbose ) {
		spdlog::set_level( spdlog::level::debug );
	} else {
		spdlog::set_level( spdlog::level::warn );
	}
	if( options.verbose ) {
		banner();
	}
	uranite::compiler::Driver driver( options );
	return driver.run();
}
