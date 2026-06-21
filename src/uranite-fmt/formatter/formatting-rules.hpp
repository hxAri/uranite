
//
// @author hxAri (hxari)
// @create 2026-06-15 05:00
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

#ifndef _URANITE_FMT_FORMATTER_FORMATTING_RULES_HPP_
#define _URANITE_FMT_FORMATTER_FORMATTING_RULES_HPP_

#include <cstdint>

namespace uranite::formatter {
	
	/**
	 * @struct FormattingRules
	 * @brief Configuration dataset defining code style layout rules for the Aether Formatter.
	 *
	 * This structure aggregates configuration flags, line boundaries, and sorting mechanics.
	 * Modifying these parameters allows the orchestration engine to shift between varying standard 
	 * conventions (such as strict zero-padding style or loose whitespace padding).
	 */
	struct FormattingRules {
		
		/** @brief The total number of consecutive space characters representing a single level of indentation depth. 
		 * * Defaults to 4 spaces per indentation tab stop block.
		 */
		uint32_t indentationWidth = 4;
		
		/** @brief Toggles padding spacing immediately inside standard execution evaluation boundaries.
		 * * If set to true, renders expressions as `fn( x, y )`; if false, compresses them into `fn(x, y)`.
		 */
		bool useSpacesInsideParentheses = true;
		
		/** @brief Toggles padding spacing inside parameter signatures that contain no execution arguments.
		 * * If set to true, renders empty scopes as `fn( )`; if false, condenses them down to `fn()`.
		 */
		bool useSpacesInsideEmptyParens = false;
		
		/** @brief Inserts a strict single padding blank vertical break immediately after a structural package module definition header. */
		bool blankLineAfterPackage = true;
		
		/** @brief Inserts a strict single padding blank vertical line between separate import package dependency blocks. */
		bool blankLineBetweenImportGroups = true;
		
		/** @brief Enforces an isolated blank vertical layout line splitting apart distinct top-level entities (e.g., separate classes or global functions). */
		bool blankLineBetweenTopLevelDeclarations = true;
		
		/** @brief The hard horizontal column text marker boundary width limit, used to trigger wrapping macro line logic. 
		 * * Defaults to 120 character byte layouts per source text row.
		 */
		uint32_t maximumLineLength = 120;
		
		/** @brief Toggles automatic lexical alphabetical sorting on import modules according to their namespace path layouts. */
		bool sortImportsByPackage = false;
		
		/** @brief Clusters related import strings into grouped layout blocks matching root module package prefix segments. */
		bool groupImportsByPackagePrefix = false;
		
	};
	
} // namespace uranite::formatter

#endif // _URANITE_FMT_FORMATTER_FORMATTING_RULES_HPP_
