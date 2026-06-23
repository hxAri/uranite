
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

#ifndef _URANITE_DOC_RENDERER_HTML_RENDERER_HPP_
#define _URANITE_DOC_RENDERER_HTML_RENDERER_HPP_

#include <string>
#include <unordered_map>

#include "uranite-doc/model/documentation-model.hpp"
#include "uranite-doc/renderer/markdown-renderer.hpp"

namespace uranite::doc {
	
	/**
	 * @class HtmlRenderer
	 * @brief Generation engine responsible for compiling documentation data models into web-ready HTML sites.
	 *
	 * This class handles the construction of standalone web documentation, managing layout structure templates,
	 * generation of responsive sidebar navigation systems, injection of embedded core stylesheets, and full 
	 * HTML escaping to safeguard generated code representations.
	 */
	class HtmlRenderer {
		
		public:
			
			/**
			 * @brief Master driver to compile and write the entire documentation site into physical HTML files.
			 * * This method initializes cross-reference indices, dynamically builds global structural layouts 
			 * (such as the persistent navigation sidebar), and walks the package tree to output hyperlinked web pages.
			 * * @param site The complete extracted documentation site instance holding all parsed package metadata.
			 * @param outputDirectory The physical filesystem destination path where the HTML files and directories will be written.
			 */
			void render( const DocumentationSite& site, const std::string& outputDirectory );
			
		private:
			
			/**
			 * @brief Scans the site hierarchy to build a flat lookup table mapping derived classes to their parents.
			 * * Populates internal relational maps to allow the renderer to unwind and resolve linear 
			 * class parentage tracks during HTML generation passes.
			 * * @param site The target documentation site metadata instance.
			 */
			void buildInheritanceMap( const DocumentationSite& site );
			
			/**
			 * @brief Generates a localized semantic HTML Table of Contents block specific to a single module page.
			 * * Compiles clean list systems (`<ul>`, `<li>`, `<a>`) containing matching internal anchor links 
			 * pointing directly to types, functions, and variables detailed inside the module.
			 * * @param moduleDoc Reference to the specific module descriptor being indexed.
			 * @return std::string A formatted HTML string payload representing the module's local contents table.
			 */
			std::string buildModuleTableOfContentsHtml( const ModuleDocumentation& moduleDoc ) const;
			
			/**
			 * @brief Helper utility to flat-register entities into an absolute internal path index.
			 * * Populates `entityToModulePath_` by binding entity names to their parent module target URLs, 
			 * allowing automated cross-linking across distinct web documents.
			 * * @param node The current sub-package tree node directory level to scan.
			 */
			void collectEntitiesFromTree( const PackageTreeNode& node );
			
			/**
			 * @brief Computes the matching relative output destination web path layout mirror for a module.
			 * @param moduleDoc Reference to the target module descriptor.
			 * @param inputBaseDirectory Root project space workspace path used to evaluate depth.
			 * @return std::string The translated physical output HTML filepath string.
			 */
			std::string computeOutputRelativePath( const ModuleDocumentation& moduleDoc, const std::string& inputBaseDirectory ) const;
				
			/**
			 * @brief Translates an EntityKind enum classification flag into its displayable keyword name string.
			 * @param entityKind The kind identifier of the component.
			 * @return std::string A lower-case plain text keyword representation (e.g., "interface", "class").
			 */
			std::string entityKindToString( EntityKind entityKind ) const;
			
			/**
			 * @brief Sanitizes string content inputs by escaping active web characters to prevent layout breaks or XSS.
			 * * Replaces active code markers (such as `<`, `>`, `&`, `"`) with safe HTML entities.
			 * * @param text The raw unsanitized code or comment text string.
			 * @return std::string The fully escaped, browser-safe layout string payload.
			 */
			std::string escapeHtml( const std::string& text ) const;
			
			/**
			 * @brief Generates the markup structure for a persistent, multi-tiered navigational sidebar tree.
			 * * Processes the package nodes recursively to emit stylized tree navigation links, marking active paths 
			 * based on package depth calculations.
			 * * @param node The root package tree node anchoring the repository layout.
			 * @param inputBaseDirectory Baseline workspace directory layout tracking context.
			 * @return std::string Raw semantic HTML segment representing the side navigation pane.
			 */
			std::string generateSidebar( const PackageTreeNode& node, const std::string& inputBaseDirectory );
			
			/**
			 * @brief Emits the embedded CSS stylesheet data stream used to skin the generated web pages.
			 * * Returns layout instructions, theme configuration styles, and code block styling rules 
			 * tailored for Uranite's documentation visual brand.
			 * * @return std::string The complete inline CSS structural code block payload.
			 */
			std::string generateStylesheet();
			
			/**
			 * @brief Formats structural components of a parsed doccomment into descriptive semantic HTML layout boxes.
			 * * Converts sections like `@param` or `@return` blocks into stylized documentation grids or lists.
			 * * @param doccomment The structured representation of the documentation comment block.
			 * @return std::string HTML layout content detailing descriptions and tagged constraint fields.
			 */
			std::string renderDoccommentHtml( const ParsedDoccomment& doccomment );
			
			/**
			 * @brief Compiles a comprehensive high-level entity block declaration (e.g., a Class or Struct layout) down to web markup.
			 * * Dynamically offsets semantic header tags (`<h1>` through `<h6>`) according to scope depth, nesting 
			 * underlying fields, member methods, and nested components into clear visual content blocks.
			 * * @param entity The complete structure layout of the class, struct, or interface entry.
			 * @param headingLevel Current HTML header nesting index context.
			 * @return std::string The fully rendered HTML section string.
			 */
			std::string renderEntityHtml( const EntityDocumentationEntry& entity, uint32_t headingLevel );
			
			/**
			 * @brief Resolves and renders a linear visual inheritance breadcrumb trail for OOP classes.
			 * * Evaluates lookups to link ancestors dynamically (e.g., rendering clickable hyperlink nodes: `Base -> Intermediate -> Derived`).
			 * * @param className The absolute identifier name of the type to trace back.
			 * @return std::string HTML structural links mapping out the entity's complete derivation path.
			 */
			std::string renderInheritanceChainHtml( const std::string& className );
			
			/**
			 * @brief Compiles a member method or global routine entry block into an isolated code container box.
			 * @param method The internal structure entry tracking method metrics.
			 * @return std::string The localized HTML representation containing signatures, flags, and method summaries.
			 */
			std::string renderMethodHtml( const MethodDocumentationEntry& method );
			
			/**
			 * @brief Recursively drives through the package nodes to map directories and physically output finished HTML documents.
			 * @param node The current package branch node.
			 * @param outputDirectory Physical destination folder path on disk.
			 * @param inputBaseDirectory Tracking baseline workspace layout context.
			 * @param sidebarContent Pre-compiled global navigation HTML markup injected into every module sheet.
			 */
			void renderPackageTreeHtml( const PackageTreeNode& node, const std::string& outputDirectory, const std::string& inputBaseDirectory, const std::string& sidebarContent );
			
			/**
			 * @brief Master page template layout wrapper that bounds standalone page segments into a valid HTML5 document shell.
			 * * Seamlessly orchestrates page layout wrapping, linking the global sidebar pane, injecting the core 
			 * styles, and nesting module body contents inside standard `<head>` and `<body>` tags.
			 * * @param title The document title string bound to browser headers.
			 * @param bodyContent Central module content block string.
			 * @param sidebarContent Navigational sidebar layout block string.
			 * @return std::string The fully assembled, standard-compliant HTML file string payload.
			 */
			std::string wrapInTemplate( const std::string& title, const std::string& bodyContent, const std::string& sidebarContent );
				
			/** @brief Global index cache mapping every absolute entity type name to its physical HTML generation target path URL. */
			std::unordered_map<std::string, std::string> entityToModulePath_;
			
			/** @brief Internal relational graph database mapping class names directly back to their declared parent types. */
			std::unordered_map<std::string, std::string> inheritanceMap_;
		
	};

} // namespace uranite::doc

#endif // _URANITE_DOC_RENDERER_HTML_RENDERER_HPP_
