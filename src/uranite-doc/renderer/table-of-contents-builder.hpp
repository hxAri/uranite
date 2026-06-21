
//
// @author hxAri (hxari)
// @create 2026-06-15 18:00
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

#ifndef _URANITE_DOC_RENDERER_TABLE_OF_CONTENTS_BUILDER_HPP_
#define _URANITE_DOC_RENDERER_TABLE_OF_CONTENTS_BUILDER_HPP_

#include <string>
#include <vector>

#include "uranite-doc/model/documentation-model.hpp"

namespace uranite::doc {
	
	/**
	 * @struct TableOfContentsCategory
	 * @brief Models a logical category group inside the generated documentation's Table of Contents.
	 * * This structure handles runtime routing or indexing, allowing multiple parsed modules 
	 * to be dynamically grouped into layout buckets (e.g., "Core", "Semantic Analysis", "IR Lowering") 
	 * based on matching string prefix rules.
	 */
	struct TableOfContentsCategory {
		
		/** @brief The formal display name of the category (e.g., "Frontend Pipeline"). */
		std::string categoryName;
		
		/** * @brief Collection of namespace or package prefixes assigned to filter modules into this category. 
		 * * For instance, a prefix `"uranite::parser"` will capture any module originating from that path.
		 */
		std::vector<std::string> matchingPrefixes;
		
		/** * @brief Const observer pointers pointing directly to the filtered modules assigned to this section. 
		 * * Leverages raw pointers as lightweight observers, ensuring no structural lifetime ownership overhead 
		 * over the master module metadata stored inside the root site tree.
		 */
		std::vector<const ModuleDocumentation*> assignedModules;
		
	};
	
	/**
	 * @class TableOfContentsBuilder
	 * @brief Engine responsible for organizing, indexing, and rendering the project's Table of Contents (ToC).
	 *
	 * This builder flattens and groups processed modules into logical categories based on package prefixes.
	 * It provides formatting capabilities to generate structured, web-ready HTML index systems or static 
	 * Markdown navigational tables for the compiled documentation site.
	 */
	class TableOfContentsBuilder {
			
		public:
				
			/**
			 * @brief Standard entry point to construct the category indexes from a complete documentation site node.
			 * * This method triggers the internal recursive tree traversal to harvest all module definitions,
			 * analyzes their logical path names, and maps them into pre-configured categorization buckets.
			 * * @param site The root documentation architecture node containing the complete module hierarchy tree.
			 */
			void buildFromSite( const DocumentationSite& site );
			
			/**
			 * @brief Renders the organized categories into a clean, standalone HTML navigation sidebar or index page.
			 * * Generates semantic HTML tags (`<ul>`, `<li>`, `<a>`), applies proper relative URL linking paths,
			 * and ensures structural output text fields are fully escaped against injection anomalies.
			 * * @param inputBaseDirectory The base workspace source directory, used to compute relative file paths.
			 * @return std::string The fully compiled and formatted HTML Table of Contents string payload.
			 */
			std::string renderHtmlToc( const std::string& inputBaseDirectory ) const;
			
			/**
			 * @brief Renders the organized categories into a standard Markdown tree layout document.
			 * * Beneficial for producing index trackers or landing directories (e.g., `README.md` indices) 
			 * matching Git repository formatting boundaries.
			 * * @param inputBaseDirectory The base workspace source directory, used to compute relative markdown links.
			 * @return std::string The formatted Markdown representation of the table of contents.
			 */
			std::string renderMarkdownToc( const std::string& inputBaseDirectory ) const;
			
		private:
				
			/**
			 * @brief Matches a package's structural name against the category registration list to find its target group.
			 * * Evaluates the string using sequential prefix-matching logic to bucket the module appropriately.
			 * * @param packageName The fully qualified domain path name of the module package.
			 * @return std::string The identifier name of the matched category, or an empty string if it falls into uncategorized buckets.
			 */
			std::string categorizeModule( const std::string& packageName ) const;
			
			/**
			 * @brief Recursively traverses the multi-layered PackageTreeNode structures to harvest modules.
			 * * Flattens the hierarchical directory package layers into lightweight observer lists for faster categorization indexing.
			 * * @param node The current sub-package tree node being scanned.
			 */
			void collectModulesFromTree( const PackageTreeNode& node );
			
			/**
			 * @brief Computes the target destination link path file for a specific module web file.
			 * * Determines absolute file locations and offsets them against the base directory to construct solid, 
			 * unbroken cross-document relative reference links.
			 * * @param moduleDoc Reference descriptor mapping the target module entity.
			 * @param inputBaseDirectory Root project space tracking context.
			 * @return std::string The fully resolved relative navigation link path string.
			 */
			std::string computeModuleLinkPath( const ModuleDocumentation& moduleDoc, const std::string& inputBaseDirectory ) const;
			
			/**
			 * @brief Converts an internal EntityKind enum into a localized string badge or text tag format.
			 * * Transforms tokens (e.g., `EntityKind::Struct`) into clean display tags like `"struct"` or `"class"` 
			 * for metadata indexing blocks.
			 * * @param entityKind The classification category of the underlying AST entity.
			 * @return std::string The displayable plain-text tag name matching the kind.
			 */
			std::string entityKindLabel( EntityKind entityKind ) const;
			
			/**
			 * @brief Sanitizes string content inputs by escaping active web characters to prevent layout breaks.
			 * * Replaces problematic HTML structural characters (such as `<`, `>`, `&`, `"`) with their safe, 
			 * corresponding HTML entity equivalents.
			 * * @param text The raw unsanitized input text string.
			 * @return std::string The fully escaped, layout-safe text payload.
			 */
			std::string escapeHtml( const std::string& text ) const;
				
			/** @brief Registered collection of formal Table of Contents navigation category definitions. */
			std::vector<TableOfContentsCategory> categories_;
			
			/** @brief Fallback observer list tracking modules that failed to match any registered category prefix filters. */
			std::vector<const ModuleDocumentation*> uncategorizedModules_;
			
	};

} // namespace uranite::doc

#endif // _URANITE_DOC_RENDERER_TABLE_OF_CONTENTS_BUILDER_HPP_
