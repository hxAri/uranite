
//
// @author hxAri (hxari)
// @create 2026-05-14 18:20
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
#include <filesystem>
#include <fmt/core.h>
#include <fmt/color.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/lexer/lexer.hpp"
#include "uranite/parser/parser.hpp"
#include "uranite-fmt/comments/comment-extractor.hpp"
#include "uranite-fmt/comments/comment-reattacher.hpp"
#include "uranite-fmt/formatter/formatter.hpp"
#include "uranite-fmt/linter/linter.hpp"

static int formatSingleFile( const std::string& filePath, bool writeInPlace, bool checkOnly );
static int lintSingleFile( const std::string& filePath );
static std::string readFileContents( const std::string& filePath );

/**
 * @brief Recursively collects all Uranite source files within a target directory path.
 * * Scans the specified workspace directory and all its underlying subfolders to isolate 
 * regular files containing the native `.urn` file extension, appending their path strings 
 * to the provided storage container.
 * * @param directoryPath The root directory path where the recursive iterator begins its search.
 * @param collectedFiles Output reference vector where the discovered source file paths are appended.
 */
static void collectUraniteFiles( const std::string& directoryPath, std::vector<std::string>& collectedFiles ) {
	for( const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator( directoryPath ) ) {
		if( entry.is_regular_file() && entry.path().extension() == ".urn" ) {
			collectedFiles.push_back( entry.path().string() );
		}
	}
}

/**
 * @brief Drives the complete syntactic formatting and comment reattachment pipeline for a single file.
 * * This function runs a multi-stage process over the target source file contents:
 * 1. Extracts standalone comment ranges to preserve them during printing.
 * 2. Tokenizes and parses the code into a standard Abstract Syntax Tree (AST).
 * 3. Feeds the program AST into the structural formatter to generate pretty-printed source text.
 * 4. Invokes the `CommentReattacher` to safely re-inject the extracted comments back into their correct logical coordinates.
 * * Depending on the configuration flags, it can either print the resulting stream directly to stdout, 
 * overwrite the original file in-place, or operate in validation mode (`checkOnly`) to detect deviations without mutation.
 * * @param filePath Path to the Uranite source file slated for code formatting.
 * @param writeInPlace Set to true to overwrite the physical disk file with the cleanly formatted string payload.
 * @param checkOnly Set to true to switch to verification mode, validating compliance without modifying the code layout.
 * @return int Returns 0 on successful processing or when a file complies with formatting layout rules; returns 1 on syntax errors, IO failures, or when modifications are detected in verification mode.
 */
static int formatSingleFile( const std::string& filePath, bool writeInPlace, bool checkOnly ) {
	std::string sourceContent = readFileContents( filePath );
	if( sourceContent.empty() ) {
		return 1;
	}
	uranite::formatter::comments::CommentExtractor commentExtractor( sourceContent, filePath );
	std::vector<uranite::formatter::comments::CommentEntry> extractedComments = commentExtractor.extract();
	uranite::diagnostic::Engine diagnosticEngine( 0, -1 );
	uranite::lexer::Lexer lexer( sourceContent, filePath, diagnosticEngine );
	std::vector<uranite::token::Token> tokenStream = lexer.tokenize();
	if( diagnosticEngine.hasErrors() ) {
		fmt::print( stderr, "error: failed to lex \"{}\"\n", filePath );
		return 1;
	}
	uranite::parser::Parser parser( tokenStream, diagnosticEngine );
	uranite::ast::nodes::ProgramSharedPointer program = parser.parse();
	if( diagnosticEngine.hasErrors() ) {
		fmt::print( stderr, "error: failed to parse \"{}\"\n", filePath );
		return 1;
	}
	uranite::formatter::Formatter sourceFormatter;
	std::string formattedOutput = sourceFormatter.format( *program );
	uranite::formatter::comments::CommentReattacher reattacher( extractedComments );
	std::string finalOutput = reattacher.reattach( formattedOutput, sourceContent );
	if( checkOnly ) {
		if( finalOutput != sourceContent ) {
			fmt::print( "Would reformat: {}\n", filePath );
			return 1;
		}
		return 0;
	}
	if( writeInPlace ) {
		std::ofstream outputFile( filePath );
		if( outputFile.is_open() == false ) {
			fmt::print( stderr, "error: could not write to \"{}\"\n", filePath );
			return 1;
		}
		outputFile << finalOutput;
		fmt::print( "Formatted: {}\n", filePath );
		return 0;
	}
	fmt::print( "{}", finalOutput );
	return 0;
}

/**
 * @brief Executes style enforcement, static analysis rules, and syntactic linting on a single file.
 * * Processes the target file contents through the compiler frontend (Lexer and Parser), passing the resulting 
 * AST and tokenized comment array into the Uranite Linter subsystem. Discovered style anomalies, code smells, 
 * or structural rule violations are dumped directly to standard error with detailed coordinates.
 * * @param filePath Path to the target source file to be audited by the lint rules.
 * @return int Returns 0 if the file passes all configured style guidelines and structural rule checks without errors; returns 1 on frontend tracking compilation errors or if lint diagnostics are detected.
 */
static int lintSingleFile( const std::string& filePath ) {
	std::string sourceContent = readFileContents( filePath );
	if( sourceContent.empty() ) {
		return 1;
	}
	uranite::formatter::comments::CommentExtractor commentExtractor( sourceContent, filePath );
	std::vector<uranite::formatter::comments::CommentEntry> extractedComments = commentExtractor.extract();
	uranite::diagnostic::Engine diagnosticEngine( 0, -1 );
	uranite::lexer::Lexer lexer( sourceContent, filePath, diagnosticEngine );
	std::vector<uranite::token::Token> tokenStream = lexer.tokenize();
	if( diagnosticEngine.hasErrors() ) {
		fmt::print( stderr, "error: failed to lex \"{}\"\n", filePath );
		return 1;
	}
	uranite::parser::Parser parser( tokenStream, diagnosticEngine );
	uranite::ast::nodes::ProgramSharedPointer program = parser.parse();
	if( diagnosticEngine.hasErrors() ) {
		fmt::print( stderr, "error: failed to parse \"{}\"\n", filePath );
		return 1;
	}
	uranite::formatter::linter::LintRuleConfig ruleConfig;
	uranite::formatter::linter::Linter linter( filePath, extractedComments, ruleConfig );
	std::vector<uranite::formatter::linter::LintDiagnostic> diagnostics = linter.lint( *program );
	for( const uranite::formatter::linter::LintDiagnostic& diagnostic : diagnostics ) {
		std::string severityLabel = diagnostic.severity == uranite::formatter::linter::LintSeverity::Error ? "error" : "warning";
		fmt::print( "{}:{}:{}: {} [{}] {}\n",
			diagnostic.sourceFilePath,
			diagnostic.lineNumber,
			diagnostic.columnNumber,
			severityLabel,
			diagnostic.ruleIdentifier,
			diagnostic.diagnosticMessage
		);
	}
	return diagnostics.empty() ? 0 : 1;
}

/**
 * @brief Reads the entire raw text contents of a target file into an in-memory string buffer.
 * @param filePath Path to the physical file located on the host filesystem.
 * @return std::string The complete text content payload stream, or an empty string if file opening fails or if the file is empty.
 */
static std::string readFileContents( const std::string& filePath ) {
	std::ifstream fileStream( filePath );
	if( fileStream.is_open() == false ) {
		fmt::print( stderr, "error: could not open file \"{}\"\n", filePath );
		return "";
	}
	std::ostringstream contentStream;
	contentStream << fileStream.rdbuf();
	return contentStream.str();
}

int main( int argc, char* argv[] ) {
	argparse::ArgumentParser program( "uranite-fmt", "1.0.0" );
	program.add_description( "Uranite source code formatter and linter" );
	program.add_argument( "input" )
		.help( "Source file or directory to format" )
		.nargs( argparse::nargs_pattern::optional );
	program.add_argument( "--write", "-w" )
		.help( "Format and overwrite files in-place" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--check" )
		.help( "Check if files are formatted (exit code 0 if formatted, 1 if not)" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--lint" )
		.help( "Run linter diagnostics" )
		.default_value( false )
		.implicit_value( true );
	try {
		program.parse_args( argc, argv );
	}
	catch( const std::runtime_error& parseError ) {
		fmt::print( stderr, "error: {}\n", parseError.what() );
		fmt::print( stderr, "{}", program.help().str() );
		return 1;
	}
	std::optional<std::string> inputPath = program.present( "input" );
	if( inputPath == std::nullopt ) {
		fmt::print( stderr, "{}", program.help().str() );
		return 1;
	}
	bool writeInPlace = program.get<bool>( "--write" );
	bool checkOnly = program.get<bool>( "--check" );
	bool lintOnly = program.get<bool>( "--lint" );
	std::string targetPath = *inputPath;
	if( std::filesystem::is_directory( targetPath ) ) {
		std::vector<std::string> sourceFiles;
		collectUraniteFiles( targetPath, sourceFiles );
		int failureCount = 0;
		int successCount = 0;
		for( const std::string& sourceFile : sourceFiles ) {
			int fileResult = lintOnly
				? lintSingleFile( sourceFile )
				: formatSingleFile( sourceFile, writeInPlace, checkOnly );
			if( fileResult == 0 ) {
				successCount++;
			}
			else {
				failureCount++;
			}
		}
		if( lintOnly ) {
			fmt::print( "{} files linted: {} clean, {} with diagnostics\n",
				successCount + failureCount, successCount, failureCount );
		}
		else if( checkOnly ) {
			fmt::print( "{} files checked: {} formatted, {} need formatting\n",
				successCount + failureCount, successCount, failureCount );
		}
		else if( writeInPlace ) {
			fmt::print( "{} files formatted\n", successCount );
		}
		return failureCount > 0 ? 1 : 0;
	}
	if( lintOnly ) {
		return lintSingleFile( targetPath );
	}
	return formatSingleFile( targetPath, writeInPlace, checkOnly );
}
