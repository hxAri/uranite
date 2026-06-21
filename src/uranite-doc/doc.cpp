
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
#include <fmt/color.h>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/lexer/lexer.hpp"
#include "uranite/parser/parser.hpp"
#include "uranite-doc/extractor/extractor.hpp"
#include "uranite-doc/renderer/html-renderer.hpp"
#include "uranite-doc/renderer/markdown-renderer.hpp"
#include "uranite-doc/validator/doccomment-validator.hpp"
#include "uranite-fmt/comments/comment-extractor.hpp"

/**
 * @brief Recursively collects all Aether source files within a target directory.
 * * Scans the specified directory path and all of its subdirectories to find regular 
 * files that contain the `.urn` file extension, appending their full paths to the collection vector.
 * * @param directoryPath The root directory path where the recursive search begins.
 * @param collectedFiles Output reference vector where the absolute or relative matching file paths are appended.
 */
static void collectAetherFiles( const std::string& directoryPath, std::vector<std::string>& collectedFiles ) {
    for( const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator( directoryPath ) ) {
        if( entry.is_regular_file() && entry.path().extension() == ".urn" ) {
            collectedFiles.push_back( entry.path().string() );
        }
    }
}

/**
 * @brief Prints a standardized high-level summary of the extracted documentation metrics to stdout.
 * * Outputs the aggregated telemetry tracking total numbers of parsed modules, structural entities 
 * (classes, structs, traits), and declared methods processed across the documentation run.
 * * @param site The documentation architecture site model holding the aggregated counter metrics.
 */
static void printDocumentationStats( const uranite::doc::DocumentationSite& site ) {
    fmt::print( "\nDocumentation extraction complete:\n" );
    fmt::print( "  Modules:  {}\n", site.totalModules );
    fmt::print( "  Entities: {}\n", site.totalEntities );
    fmt::print( "  Methods:  {}\n", site.totalMethods );
}

/**
 * @brief Recursively prints a hierarchical console tree representation of packages and modules.
 * * Iterates deep into the node structure, applying visual indentations based on tree depth 
 * to represent module packages along with their respective exported component item weights.
 * * @param node The current package tree sub-node directory segment to print.
 * @param indentLevel The current depth level in the hierarchy, used to calculate string space padding.
 */
static void printPackageTree( const uranite::doc::PackageTreeNode& node, uint32_t indentLevel ) {
    std::string indent( indentLevel * 2, ' ' );
    if( node.segmentName.empty() == false ) {
        fmt::print( "{}{}/\n", indent, node.segmentName );
    }
    for( const uranite::doc::ModuleDocumentation& moduleDoc : node.modules ) {
        std::string moduleIndent( ( indentLevel + 1 ) * 2, ' ' );
        uint32_t entityCount = moduleDoc.exportedEntities.size();
        fmt::print( "{}{} ({} entities)\n", moduleIndent, moduleDoc.packageName, entityCount );
    }
    for( const uranite::doc::PackageTreeNode& child : node.childPackages ) {
        printPackageTree( child, indentLevel + 1 );
    }
}

/**
 * @brief Reads the entire contents of a physical file into an in-memory string buffer.
 * @param filePath Path to the target source or configuration file on disk.
 * @return std::string The full file contents as a text payload string, or an empty string if opening the file fails.
 */
static std::string readFileContents( const std::string& filePath ) {
    std::ifstream fileStream( filePath );
    if( fileStream.is_open() == false ) {
        return "";
    }
    std::ostringstream contentStream;
    contentStream << fileStream.rdbuf();
    return contentStream.str();
}

/**
 * @brief Executes a dedicated structural validation pipeline on doccomments for a single file.
 * * This function runs a multi-stage process over the target file contents:
 * 1. Extracts raw textual comment blocks out of the file layout.
 * 2. Tokenizes and parses the raw code to verify there are no blocking syntax errors.
 * 3. Extracts structured documentation objects out of the file.
 * 4. Cross-references the structured data models back to the comment blocks using the validator.
 * Any mismatch or documentation violation found is printed straight to the terminal standard output.
 * * @param filePath Path to the file whose doccomments are to be audited.
 * @param validator The shared reference model validation engine tracking known typing constraints.
 * @return int Total count of documentation validation diagnostic warnings/errors found in the file.
 */
static int validateSingleFile( const std::string& filePath, uranite::doc::DoccommentValidator& validator ) {
    std::string sourceContent = readFileContents( filePath );
    if( sourceContent.empty() ) {
        return 0;
    }
    uranite::formatter::comments::CommentExtractor commentExtractor( sourceContent, filePath );
    std::vector<uranite::formatter::comments::CommentEntry> extractedComments = commentExtractor.extract();
    uranite::diagnostic::Engine diagnosticEngine( 0, -1 );
    uranite::lexer::Lexer lexer( sourceContent, filePath, diagnosticEngine );
    std::vector<uranite::token::Token> tokenStream = lexer.tokenize();
    if( diagnosticEngine.hasErrors() ) {
        return 0;
    }
    uranite::parser::Parser parser( tokenStream, diagnosticEngine );
    uranite::ast::nodes::ProgramSharedPointer program = parser.parse();
    if( diagnosticEngine.hasErrors() || program == nullptr ) {
        return 0;
    }
    uranite::doc::DocumentationExtractor extractor;
    uranite::doc::ModuleDocumentation moduleDoc = extractor.extractFromFile( filePath );
    std::vector<uranite::doc::ValidationDiagnostic> diagnostics = validator.validateModule( moduleDoc, extractedComments );
    for( const uranite::doc::ValidationDiagnostic& diagnostic : diagnostics ) {
        std::string severityLabel = diagnostic.severity == uranite::doc::ValidationSeverity::Error ? "error" : "warning";
        fmt::print( "{}:{}:{}: {} [{}] {}\n",
            diagnostic.sourceFilePath,
            diagnostic.lineNumber,
            diagnostic.columnNumber,
            severityLabel,
            diagnostic.ruleIdentifier,
            diagnostic.diagnosticMessage
        );
    }
    return diagnostics.size();
}

int main( int argc, char* argv[] ) {
	argparse::ArgumentParser program( "uranite-doc", "1.0.0" );
	program.add_description( "Uranite API documentation generator" );
	program.add_argument( "input" )
		.help( "Source directory or file to generate documentation from" )
		.nargs( argparse::nargs_pattern::optional );
	program.add_argument( "--output", "-o" )
		.help( "Output directory for generated documentation" )
		.default_value( std::string( "docs" ) );
	program.add_argument( "--format", "-f" )
		.help( "Output format: markdown or html" )
		.default_value( std::string( "markdown" ) );
	program.add_argument( "--project-root" )
		.help( "Project root directory for README/LICENSE discovery (defaults to parent of input)" )
		.default_value( std::string( "" ) );
	program.add_argument( "--validate-only" )
		.help( "Only validate doccomment structure, do not generate output" )
		.default_value( false )
		.implicit_value( true );
	program.add_argument( "--extract-only" )
		.help( "Extract and display documentation model without rendering" )
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
	std::string targetPath = *inputPath;
	bool validateOnly = program.get<bool>( "--validate-only" );
	bool extractOnly = program.get<bool>( "--extract-only" );
	if( std::filesystem::exists( targetPath ) == false ) {
		fmt::print( stderr, "error: path does not exist: \"{}\"\n", targetPath );
		return 1;
	}
	if( validateOnly ) {
		std::vector<std::string> sourceFiles;
		if( std::filesystem::is_directory( targetPath ) ) {
			collectAetherFiles( targetPath, sourceFiles );
		}
		else {
			sourceFiles.push_back( targetPath );
		}
		uranite::doc::DoccommentValidator validator;
		int totalIssues = 0;
		for( const std::string& sourceFile : sourceFiles ) {
			totalIssues += validateSingleFile( sourceFile, validator );
		}
		fmt::print( "Validated {} files: {} issues found\n", sourceFiles.size(), totalIssues );
		return totalIssues > 0 ? 1 : 0;
	}
	if( extractOnly ) {
		uranite::doc::DocumentationExtractor extractor;
		if( std::filesystem::is_directory( targetPath ) ) {
			uranite::doc::DocumentationSite site = extractor.extractFromDirectory( targetPath );
			printDocumentationStats( site );
			fmt::print( "\nPackage tree:\n" );
			printPackageTree( site.rootPackage, 0 );
		}
		else {
			uranite::doc::ModuleDocumentation moduleDoc = extractor.extractFromFile( targetPath );
			fmt::print( "Module: {}\n", moduleDoc.packageName );
			fmt::print( "Entities: {}\n", moduleDoc.exportedEntities.size() );
			for( const uranite::doc::EntityDocumentationEntry& entity : moduleDoc.exportedEntities ) {
				std::string kindLabel;
				switch( entity.entityKind ) {
					case uranite::doc::EntityKind::Class: kindLabel = "class"; break;
					case uranite::doc::EntityKind::Interface: kindLabel = "interface"; break;
					case uranite::doc::EntityKind::Struct: kindLabel = "struct"; break;
					case uranite::doc::EntityKind::Enum: kindLabel = "enum"; break;
					case uranite::doc::EntityKind::Function: kindLabel = "function"; break;
					case uranite::doc::EntityKind::Property: kindLabel = "property"; break;
					case uranite::doc::EntityKind::Constant: kindLabel = "const"; break;
				}
				fmt::print( "  {} {} ({} methods, {} fields)\n", kindLabel, entity.entityName, entity.methods.size(), entity.fields.size() );
				if( entity.entityDoccomment.summaryLine.empty() == false ) {
					fmt::print( "    Summary: {}\n", entity.entityDoccomment.summaryLine );
				}
			}
		}
		return 0;
	}
	std::string outputDirectory = program.get<std::string>( "--output" );
	std::string outputFormat = program.get<std::string>( "--format" );
	uranite::doc::DocumentationExtractor extractor;
	if( std::filesystem::is_directory( targetPath ) ) {
		uranite::doc::DocumentationSite site = extractor.extractFromDirectory( targetPath );
		if( outputFormat == "html" ) {
			uranite::doc::HtmlRenderer htmlRenderer;
			htmlRenderer.render( site, outputDirectory );
			fmt::print( "Generated HTML documentation: {} modules → {}/\n", site.totalModules, outputDirectory );
		}
		else {
			uranite::doc::MarkdownRenderer markdownRenderer;
			markdownRenderer.render( site, outputDirectory );
			fmt::print( "Generated Markdown documentation: {} modules → {}/\n", site.totalModules, outputDirectory );
		}
	}
	else {
		uranite::doc::ModuleDocumentation moduleDoc = extractor.extractFromFile( targetPath );
		uranite::doc::MarkdownRenderer markdownRenderer;
		markdownRenderer.renderModule( moduleDoc, outputDirectory );
		fmt::print( "Generated documentation for {} → {}/\n", moduleDoc.packageName, outputDirectory );
	}
	return 0;
}
