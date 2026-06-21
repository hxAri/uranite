
//
// @author hxAri (hxari)
// @create 2026-06-15 11:30
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

#ifndef _URANITE_DOC_RENDERER_MARKDOWN_RENDERER_HPP_
#define _URANITE_DOC_RENDERER_MARKDOWN_RENDERER_HPP_

#include <string>
#include <unordered_map>

#include "uranite-doc//model/documentation-model.hpp"

namespace uranite::doc {
	
	/**
	 * @class MarkdownRenderer
	 * @brief Generation engine responsible for compiling documentation data models into physical Markdown files.
	 *
	 * This class handles the layout orchestration, file stream writing, and semantic text formatting 
	 * required to turn parsed AST entities, signatures, and doccomments into clean, highly readable, 
	 * and cross-linked Markdown documents matching directory structure layouts.
	 */
	class MarkdownRenderer {
		
		public:
			
			/**
			 * @brief Master driver to render an entire documentation site into a target output directory.
			 * * This method initializes global lookups (like inheritance maps and cross-reference indices),
			 * recursively builds the physical folder structures, and generates comprehensive index files 
			 * alongside individual module documentations.
			 * * @param site The complete extracted documentation architecture holding all parsed package metadata.
			 * @param outputDirectory The physical filesystem destination path where the Markdown files will be generated.
			 */
			void render( const DocumentationSite& site, const std::string& outputDirectory );
			
			/**
			 * @brief Renders a single standalone module's documentation without base-directory context offsets.
			 * @param moduleDoc The specific module-level documentation descriptor to compile.
			 * @param outputDirectory Target destination folder path for the resulting file.
			 */
			void renderModule( const ModuleDocumentation& moduleDoc, const std::string& outputDirectory );
			
			/**
			 * @brief Renders an individual module while fully preserving its relative nesting positioning context.
			 * * Computes precise relative offsets against the base workspace directory to maintain solid 
			 * cross-document hyperlink navigations between separate Markdown module sheets.
			 * * @param moduleDoc The specific module-level documentation descriptor to compile.
			 * @param outputDirectory The root destination folder where the package structure mirrors.
			 * @param inputBaseDirectory The source root tracking boundary context, used to evaluate relative depth.
			 */
			void renderModuleWithBase( const ModuleDocumentation& moduleDoc, const std::string& outputDirectory, const std::string& inputBaseDirectory );
			
		private:
			
			/**
			 * @brief Scans the site hierarchy to build a flat lookup table mapping derived classes to their parents.
			 * * Populates internal resolution maps to allow the renderer to construct linear parentage 
			 * trees for OOP class entities dynamically.
			 * * @param site The target documentation site metadata instance.
			 */
			void buildInheritanceMap( const DocumentationSite& site );
			
			/**
			 * @brief Generates an internal textual Table of Contents anchor list specific to a single module page.
			 * * Compiles standard Markdown hash link references tracking functions, classes, and fields 
			 * listed inside the target module document.
			 * * @param moduleDoc Reference to the module descriptor being indexed.
			 * @return std::string A fully formatted local Markdown link list string payload.
			 */
			std::string buildModuleTableOfContents( const ModuleDocumentation& moduleDoc ) const;
			
			/**
			 * @brief Helper utility to flat-register entities into an absolute internal path index.
			 * * populates `entityToModulePath_` by binding entity identifiers to their declaring module paths, 
			 * enabling automatic cross-linking across distinct files.
			 * * @param node The current sub-package tree node directory level to scan.
			 */
			void collectEntitiesFromTree( const PackageTreeNode& node );
			
			/**
			 * @brief Computes the matching relative output destination file path layout mirror for a module.
			 * @param moduleDoc Reference to the module descriptor.
			 * @param inputBaseDirectory Root project space workspace path.
			 * @return std::string The translated physical output filepath string.
			 */
			std::string computeOutputRelativePath( const ModuleDocumentation& moduleDoc, const std::string& inputBaseDirectory ) const;
			
			/**
			 * @brief Translates an EntityKind enum classification flag into its displayable keyword name string.
			 * @param entityKind The kind identifier of the component.
			 * @return std::string A lower-case plain text keyword representation (e.g., "struct", "class").
			 */
			std::string entityKindToString( EntityKind entityKind ) const;
			
			/**
			 * @brief Formats algorithmic performance metrics into a clean, stylized Markdown text section.
			 * @param complexity The structured object holding parsed Big-O notation parameters.
			 * @return std::string A Markdown table block or blockquote detailing time/space bounds.
			 */
			std::string renderComplexity( const ComplexityDocumentation& complexity );
			
			/**
			 * @brief Formats structural components of a doccomment into a cohesive paragraph and tag system block.
			 * * Iterates parameters, return briefs, and error boundary throws to compile them into standard Markdown list nodes.
			 * * @param doccomment The structured representation of the comment to render.
			 * @return std::string The final formatted Markdown segment text.
			 */
			std::string renderDoccomment( const ParsedDoccomment& doccomment );
			
			/**
			 * @brief Compiles a comprehensive high-level entity block declaration (e.g., a Class layout) down to text.
			 * * Dynamically adjusts markdown layout heading intensities based on depth tracking parameters,
			 * recursively unpacking sub-nested types, local field registers, and encapsulated method tables.
			 * * @param entity The complete structure layout of the class, struct, or interface entry.
			 * @param headingLevel Current markdown heading marker level depth (e.g., 2 for `##`, 3 for `###`).
			 * @return std::string The resulting markdown text block string.
			 */
			std::string renderEntity( const EntityDocumentationEntry& entity, uint32_t headingLevel );
			
			/**
			 * @brief Formats an individual type field variable description element into a row or list node.
			 * @param field The field documentation metadata entry record.
			 * @return std::string Markdown block representing the field tracking row.
			 */
			std::string renderField( const FieldDocumentationEntry& field );
			
			/**
			 * @brief Resolves and draws a visual inheritance lineage chain for classes using lookup indices.
			 * * Produces clear breadcrumb trail indicators tracking parent extensions (e.g., `Base -> Derived`).
			 * * @param className The absolute name of the target type to trace back.
			 * @return std::string Textual Markdown representation of the class's full inheritance track.
			 */
			std::string renderInheritanceChain( const std::string& className );
			
			/**
			 * @brief Compiles a member method or global routine entry block into a dedicated code signature section.
			 * @param method The internal structure entry tracking method metrics.
			 * @return std::string The localized Markdown representation of the complete method block.
			 */
			std::string renderMethod( const MethodDocumentationEntry& method );
			
			/**
			 * @brief Generates an individual directory-level landing page index document for a package node folder.
			 * @param node The current package branch node.
			 * @param inputBaseDirectory Root project space layout mirror checkpoint.
			 * @return std::string The index summary Markdown string payload.
			 */
			std::string renderPackageIndex( const PackageTreeNode& node, const std::string& inputBaseDirectory );
			
			/**
			 * @brief Recursively drives through the multi-tiered PackageTreeNode structures to write directories and markdown files.
			 * @param node The current package sub-node directory layer.
			 * @param outputDirectory Destination path location.
			 * @param inputBaseDirectory Tracking baseline workspace layout context.
			 */
			void renderPackageTree( const PackageTreeNode& node, const std::string& outputDirectory, const std::string& inputBaseDirectory );
			
			/** @brief Global index cache mapping every absolute entity type name to its physical markdown generation destination file path. */
			std::unordered_map<std::string, std::string> entityToModulePath_;
			
			/** @brief Internal relational graph database mapping class names directly back to their declared parent types. */
			std::unordered_map<std::string, std::string> inheritanceMap_;
		
	};

} // namespace uranite::doc

#endif // _URANITE_DOC_RENDERER_MARKDOWN_RENDERER_HPP_
