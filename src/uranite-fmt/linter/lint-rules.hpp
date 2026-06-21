
//
// @author hxAri (hxari)
// @create 2026-06-15 10:30
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

#ifndef _URANITE_FMT_LINTER_LINT_RULES_HPP_
#define _URANITE_FMT_LINTER_LINT_RULES_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace uranite::formatter::linter {
	
	/**
	 * @enum LintSeverity
	 * @brief Classifies the enforcement severity tier of a triggered static analysis lint rule.
	 * * This determines how the diagnostic issue behaves during CI/CD quality gate enforcement
	 * and how it is formatted within compilation terminal streams.
	 */
	enum class LintSeverity {
		Warning, ///< Non-breaking code odor or style divergence. The compilation pass remains successful.
		Error    ///< Severe convention breach or structural issue. This will mark the lint pass as failed.
	};
	
	/**
	 * @struct LintDiagnostic
	 * @brief Encapsulates a finalized static analysis violation record bound to specific source coordinates.
	 * * It stores everything necessary to emit standard GNU-style error diagnostics 
	 * (e.g., `path/file.urn:line:col: severity [rule-id] message`).
	 */
	struct LintDiagnostic {
		
		/** @brief The restriction severity level associated with this triggered violation. */
		LintSeverity severity;
		
		/** @brief Unique registration identifier string matching the breached rule (e.g., "missing-doccomment"). */
		std::string ruleIdentifier;
		
		/** @brief Descriptive diagnostics message explaining the issue and detailing corrective remedies. */
		std::string diagnosticMessage;
		
		/** @brief Target source filesystem path pinpointing where the code anomaly resides. */
		std::string sourceFilePath;
		
		/** @brief Text line coordinate tracking precisely where the lint rule triggered. */
		uint32_t lineNumber;
		
		/** @brief Character column offset coordinate tracking where the lint rule triggered. */
		uint32_t columnNumber;
	};
	
	/**
	 * @struct LintRuleConfig
	 * @brief Aggregates the absolute parameter controls and rule switches that calibrate Aether's Linter subsystem.
	 * * This configuration structure manages thresholds for function lengths, nesting depths, identifier constraints, 
	 * and structural doccomment documentation rules.
	 */
	struct LintRuleConfig {
		
		/** @brief Enforces minimum character width guidelines on variable identifiers to prevent short, cryptic names. */
		bool enableCrypticVariable = true;
		
		/** @brief Enforces minimum character width guidelines on functional parameter signatures to ensure name clarity. */
		bool enableCrypticParameter = true;
		
		/** @brief Requires multiple import declarations to be grouped individually into separate line boundaries. */
		bool enableImportOnePerLine = true;
		
		/** @brief Flags public api entities (classes, structs, functions) that completely lack an attached doccomment block. */
		bool enableMissingDoccomment = true;
		
		/** @brief Flags structural doccomments that miss an explicit `@complexity` performance complexity descriptor tag. */
		bool enableMissingComplexity = true;
		
		/** @brief Validates the textual string pattern of `@complexity` values to ensure they comply with valid Big-O notation structures. */
		bool enableInvalidComplexityFormat = true;
		
		/** @brief Validates documentation tags to ensure parameter listings conform exclusively to `@param name` stylistic layouts. */
		bool enableAtParamStyle = true;
		
		/** @brief Flags execution routine functions whose physical body length exceeds configured line boundaries. */
		bool enableLongFunction = true;
		
		/** @brief Triggers a validation flag when conditional scope stacking (e.g., deeply nested `if` or `match` blocks) exceeds depth limits. */
		bool enableDeepNesting = true;
		
		/** @brief Mandates explicit trailing return type annotations on every formal routine declaration signature. */
		bool enableMissingReturnType = true;
		
		/** @brief Scalar character count constraint defining when a variable name is classified as cryptic.
		 * @see enableCrypticVariable
		 */
		uint32_t minimumVariableNameLength = 3;
		
		/** @brief Scalar line ceiling tracking the maximum permitted line offset block for single function execution scopes.
		 * @see enableLongFunction
		 */
		uint32_t maximumFunctionBodyLines = 200;
		
		/** @brief Ceiling threshold defining the maximum permitted nested indentation scopes deep within an individual block scope.
		 * @see enableDeepNesting
		 */
		uint32_t maximumNestingDepth = 6;
		
	};
	
} // namespace uranite::formatter::linter

#endif // _URANITE_FMT_LINTER_LINT_RULES_HPP_
