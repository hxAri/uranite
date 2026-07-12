
//
// @author hxAri (hxari)
// @create 2026-06-15 11:00
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

#ifndef _URANITE_DOC_EXTRACTOR_EXTRACTOR_HPP_
#define _URANITE_DOC_EXTRACTOR_EXTRACTOR_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "uranite/ast/node.hpp"
#include "uranite-doc/extractor/doccomment-parser.hpp"
#include "uranite-doc/model/documentation-model.hpp"
#include "uranite-fmt/comments/comment-extractor.hpp"

namespace uranite::doc {
	
	/**
	 * @class DocumentationExtractor
	 * @brief Main pipeline engine responsible for extracting structured documentation models from AST nodes and comments.
	 *
	 * This class orchestrates the complete extraction flow by traversing parsed Abstract Syntax Tree (AST) programs,
	 * capturing code declarations (classes, structs, interfaces, enums, fields, methods), mapping their preceding 
	 * documentation comments, and building a comprehensive project documentation layout tree.
	 */
	class DocumentationExtractor {
			
		public:
				
			/**
			 * @brief Recursively traverses a directory to build a complete project-wide documentation site model.
			 * * Automatically scans for all valid source modules, processes individual files, extracts their documentation,
			 * loads the underlying project metadata configuration, and structures everything into a unified site node package hierarchy.
			 * * @param directoryPath The path to the root directory of the source package or project.
			 * @return DocumentationSite The aggregated top-level documentation site architecture.
			 */
			DocumentationSite extractFromDirectory( const std::string& directoryPath );
			
			/**
			 * @brief Extracts documentation descriptors exclusively from a single standalone file path.
			 * * Handles raw file input streaming, triggers frontend tokenization and AST parsing internally, 
			 * and isolates code metadata mapping for the target file's module scope.
			 * * @param sourceFilePath Path to the physical source file on disk.
			 * @return ModuleDocumentation The standalone module-level documentation descriptor.
			 */
			ModuleDocumentation extractFromFile( const std::string& sourceFilePath );
			
		private:
			
			/**
			 * @brief Utility utility to convert an AST access modifier enumeration into its matching string literal representation.
			 * @param accessModifier The AST enum classification (e.g., Public, Private, Protected).
			 * @return std::string A human-readable string representation (e.g., "public").
			 */
			std::string accessModifierToString( ast::AccessModifier accessModifier );
			
			/**
			 * @brief Generates the signature declaration string for a function or method.
			 * * Builds text snippets such as `"fn calculate(x: int32) -> float64"` based on parameter packs 
			 * and return nodes to be used directly by document layout writers.
			 * * @param funcDecl Reference to the target function AST declaration node.
			 * @return std::string The compiled textual signature representation.
			 */
			std::string buildSignatureText( ast::nodes::FunctionDeclaration& funcDecl );
			
			/**
			 * @brief Isolates and maps metadata components specific to an OOP class declaration node.
			 * @param classDecl Reference to the class AST declaration node.
			 * @param extractedComments Reference to the master tokenized vector of comment records.
			 * @param packageName The current package module namespace boundary context.
			 * @return EntityDocumentationEntry The unified entity entry capturing fields, methods, and parent class layouts.
			 */
			EntityDocumentationEntry extractClassEntity( ast::nodes::ClassDeclaration& classDecl, const std::vector<formatter::comments::CommentEntry>& extractedComments, const std::string& packageName );
			
			/**
			 * @brief Isolates and maps metadata components specific to an enumeration declaration node.
			 * @param enumDecl Reference to the enum AST declaration node.
			 * @param extractedComments Reference to the master tokenized vector of comment records.
			 * @param packageName The current package module namespace boundary context.
			 * @return EntityDocumentationEntry The unified entity entry containing mapped enum variant elements.
			 */
			EntityDocumentationEntry extractEnumEntity( ast::nodes::EnumDeclaration& enumDecl, const std::vector<formatter::comments::CommentEntry>& extractedComments, const std::string& packageName );
			
			/**
			 * @brief Extracts documentation fields and comments for an individual member field variable.
			 * @param fieldDecl Reference to the class/struct member field declaration node.
			 * @param extractedComments Reference to the master tokenized vector of comment records.
			 * @return FieldDocumentationEntry Mapped member field documentation metadata.
			 */
			FieldDocumentationEntry extractField( ast::nodes::FieldDeclarationNode& fieldDecl, const std::vector<formatter::comments::CommentEntry>& extractedComments );
			
			/**
			 * @brief Isolates and maps metadata components specific to a global function declaration node.
			 * @param funcDecl Reference to the function AST declaration node.
			 * @param extractedComments Reference to the master tokenized vector of comment records.
			 * @param packageName The current package module namespace boundary context.
			 * @return EntityDocumentationEntry The unified entity entry capturing function signatures and documentation criteria.
			 */
			EntityDocumentationEntry extractFunctionEntity( ast::nodes::FunctionDeclaration& funcDecl, const std::vector<formatter::comments::CommentEntry>& extractedComments, const std::string& packageName );
			
			/**
			 * @brief Root driver to systematically extract modular declarations directly out of a fully parsed Program node.
			 * @param program Reference to the root node structure of the compiled target program.
			 * @param extractedComments Reference to the master tokenized vector of comment records.
			 * @param sourceFilePath Path to the source file tracking reference context.
			 * @return ModuleDocumentation The fully mapped module layout model.
			 */
			ModuleDocumentation extractFromProgram( ast::nodes::Program& program, const std::vector<formatter::comments::CommentEntry>& extractedComments, const std::string& sourceFilePath );
			
			/**
			 * @brief Isolates and maps metadata components specific to an OOP interface or trait declaration node.
			 * @param interfaceDecl Reference to the interface AST declaration node.
			 * @param extractedComments Reference to the master tokenized vector of comment records.
			 * @param packageName The current package module namespace boundary context.
			 * @return EntityDocumentationEntry The unified entity entry capturing pure virtual method footprints.
			 */
			EntityDocumentationEntry extractInterfaceEntity( ast::nodes::InterfaceDeclaration& interfaceDecl, const std::vector<formatter::comments::CommentEntry>& extractedComments, const std::string& packageName );
			
			/**
			 * @brief Extracts documentation descriptors and parameters for an isolated member method code structure.
			 * @param methodDecl Reference to the function declaration node acting as a scope method.
			 * @param extractedComments Reference to the master tokenized vector of comment records.
			 * @return MethodDocumentationEntry Mapped member method documentation metadata.
			 */
			MethodDocumentationEntry extractMethod( ast::nodes::FunctionDeclaration& methodDecl, const std::vector<formatter::comments::CommentEntry>& extractedComments );
			
			/**
			 * @brief Isolates and maps metadata components specific to a systems-level structured data type declaration node.
			 * @param structDecl Reference to the raw struct AST declaration node.
			 * @param extractedComments Reference to the master tokenized vector of comment records.
			 * @param packageName The current package module namespace boundary context.
			 * @return EntityDocumentationEntry The unified entity entry capturing raw data block member layouts.
			 */
			EntityDocumentationEntry extractStructEntity( ast::nodes::StructDeclaration& structDecl, const std::vector<formatter::comments::CommentEntry>& extractedComments, const std::string& packageName );
			
			/**
			 * @brief Lookup utility that binds a code entity's declaration line position with its associated parsed comment data.
			 * * Leverages line tracking properties to capture preceding multi-line block text boundaries 
			 * and transparently parses them into micro-structural data models.
			 * * @param declarationLine The text line index position where the entity definition begins.
			 * @param extractedComments Reference to the master tokenized vector of comment records.
			 * @return ParsedDoccomment A fully parsed documentation object block model.
			 */
			ParsedDoccomment findDoccommentForLine( uint32_t declarationLine, const std::vector<formatter::comments::CommentEntry>& extractedComments );
			
			/**
			 * @brief Hierarchical tree insert handler to place modules into their precise nested structural package position.
			 * * Slices package namespaces dynamically to create child packages or load paths inside the repository tree.
			 * * @param rootNode Reference to the root tree structure tracker to seed data entries into.
			 * @param moduleDoc Mapped module record block model payload to bind into the node hierarchy.
			 */
			void insertModuleIntoTree( PackageTreeNode& rootNode, const ModuleDocumentation& moduleDoc );
			
			/**
			 * @brief Reads project-wide configuration files (e.g., project descriptors or configurations) from a target path directory.
			 * @param projectRootPath Path to the project structure boundary root space.
			 * @return ProjectMetadata Extracted configuration parameters (e.g., project name, version control markers).
			 */
			ProjectMetadata readProjectMetadata( const std::string& projectRootPath ) const;
			
			/**
			 * @brief Unwinds a type identifier reference node tree into its native C-style text equivalent.
			 * * Converts explicit type bindings (e.g., arrays, templates, nested pointer qualifiers) 
			 * into static string names for direct user documentation viewing.
			 * * @param typeNode Shared pointer reference node mapping a specific compilation data type type.
			 * @return std::string The unrolled textual name of the matching primitive or complex data type symbol.
			 */
			std::string typeNodeToString( const ast::nodes::TypeNodeSharedPointer& typeNode );
		
	};

} // namespace uranite::doc

#endif // _URANITE_DOC_EXTRACTOR_EXTRACTOR_HPP_
