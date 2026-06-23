
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

#ifndef _URANITE_DOC_MODEL_DOCUMENTATION_MODEL_HPP_
#define _URANITE_DOC_MODEL_DOCUMENTATION_MODEL_HPP_

#include <string>
#include <vector>

#include "uranite-doc/extractor/doccomment-parser.hpp"

namespace uranite::doc {
	
	/**
	 * @enum EntityKind
	 * @brief Classifies the structural type of an AST code declaration node.
	 * * Used by the documentation engine to correctly categorize and render entities 
	 * within the generated site architecture.
	 */
	enum class EntityKind {
		Function,   ///< A global or standalone block function declaration.
		Class,      ///< An Object-Oriented class declaration node.
		Struct,     ///< A low-level systems structured data type.
		Interface,  ///< A pure virtual contract layout or trait definition.
		Enum,       ///< A typed enumeration containing discrete variance flags.
		Constant,   ///< A read-only globally declared literal value definition.
		Property    ///< A class/struct compiled getter/setter computed property block.
	};
	
	/**
	 * @struct MethodDocumentationEntry
	 * @brief Encapsulates the documented metadata and declaration flags for a member function or method.
	 * * Captures OOP characteristics alongside the method's individual parsed doccomment blocks.
	 */
	struct MethodDocumentationEntry {
		
		/** @brief The identifier name of the method. */
		std::string methodName;
		
		/** @brief Visibility level scope of the method (e.g., "public", "private", "protected"). */
		std::string accessModifier;
		
		/** @brief Full string signature code snippet of the method declaration. */
		std::string signatureText;
		
		/** @brief Flag checking if the method is pure virtual / has no implementation block body. */
		bool isAbstract = false;
		
		/** @brief Flag identifying asynchronous task/execution context models. */
		bool isAsync = false;
		
		/** @brief Flag determining if the method belongs directly to the type context instead of instances. */
		bool isStatic = false;
		
		/** @brief Flag verifying if the method is overridable in specialized child class expansions. */
		bool isVirtual = false;
		
		/** @brief Flag indicating if this execution method represents a computed property macro block. */
		bool isProperty = false;
		
		/** @brief Structured abstract representation of the parsed Doxygen documentation block attached to this method. */
		ParsedDoccomment doccomment;
		
	};
	
	/**
	 * @struct FieldDocumentationEntry
	 * @brief Captures the documentation metrics and variable components for class or structure fields.
	 */
	struct FieldDocumentationEntry {
		
		/** @brief The variable identifier name of the field. */
		std::string fieldName;
		
		/** @brief String literal name of the field's mapped compiler data type. */
		std::string fieldType;
		
		/** @brief Visibility access control qualifier bound to the field variable. */
		std::string accessModifier;
		
		/** @brief Structured abstract representation of the parsed Doxygen documentation block attached to this field. */
		ParsedDoccomment doccomment;
		
	};
	
	/**
	 * @struct EntityDocumentationEntry
	 * @brief Comprehensive data node aggregation for a high-level component type declaration.
	 * * Serves as a recursive structural block node containing member variables, methods, generic parameters,
	 * parent derivation strings, and nested declarations inside its scopes (e.g., nested classes).
	 */
	struct EntityDocumentationEntry {
		
		/** @brief Classification kind determining if this node acts as a class, struct, enum, etc. */
		EntityKind entityKind;
		
		/** @brief Raw identifier name of the entity without namespace qualifiers. */
		std::string entityName;
		
		/** @brief Fully qualified namespace or package layout string path of the entity. */
		std::string qualifiedName;
		
		/** @brief Visibility context of the root entity type block declaration. */
		std::string accessModifier;
		
		/** @brief Identifier name of the direct parent class if this entity derives from a class. */
		std::string parentClassName;
		
		/** @brief List of all pure interface contracts or structural traits implemented by this entity type. */
		std::vector<std::string> implementedInterfaces;
		
		/** @brief Vector storing generic type template parameters (monomorphization keys). */
		std::vector<std::string> genericParameters;
		
		/** @brief The structured documentation attached directly to the root entity signature. */
		ParsedDoccomment entityDoccomment;
		
		/** @brief Collection of all individual internal member methods and member routines. */
		std::vector<MethodDocumentationEntry> methods;
		
		/** @brief Collection of all individual internal static or structural data instance field variables. */
		std::vector<FieldDocumentationEntry> fields;
		
		/** @brief Recursive container handling entities inner-declared directly within this entity block scope. */
		std::vector<EntityDocumentationEntry> nestedEntities;
		
		/** @brief Original textual layout starting line tracking coordinate within the physical source file. */
		uint32_t declarationLine = 0;
		
	};
	
	/**
	 * @struct ImportEntry
	 * @brief Represents a single import statement with its module path and selectively imported entity names.
	 */
	struct ImportEntry {

		/** @brief The dot-separated module path (e.g., "uranite.io.file"). */
		std::string modulePath;

		/** @brief Names of specific entities imported from the module (empty if importing the module itself). */
		std::vector<std::string> importedNames;

	};

	/**
	 * @struct ModuleDocumentation
	 * @brief Represents a compilation unit module mapping metadata for an entire isolated single source file.
	 */
	struct ModuleDocumentation {

		/** @brief Mapped logical workspace package name domain context. */
		std::string packageName;

		/** @brief Physical absolute or workspace relative source disk file location. */
		std::string sourceFilePath;

		/** @brief All top-level visible classes, functions, structs, or constants exported by this module. */
		std::vector<EntityDocumentationEntry> exportedEntities;

		/** @brief Structured import entries with module paths and selectively imported entity names. */
		std::vector<ImportEntry> importedModules;

		/** @brief Names of entities re-exported via export { ... } blocks without local declarations. */
		std::vector<std::string> reExportedNames;

	};
	
	/**
	 * @struct PackageTreeNode
	 * @brief A recursive tree layout modeling nested package namespaces and physical repository directory layers.
	 */
	struct PackageTreeNode {
		
		/** @brief Individual segment directory folder token string (e.g., "io", "semantic", "ast"). */
		std::string segmentName;
		
		/** @brief Collection of modules loaded directly inside the boundaries of this node's package directory. */
		std::vector<ModuleDocumentation> modules;
		
		/** @brief Children branches managing deeper nested sub-package nodes. */
		std::vector<PackageTreeNode> childPackages;
		
	};
	
	/**
	 * @struct ProjectMetadata
	 * @brief Structural configuration block mirroring project definitions (e.g., uranite project configurations).
	 */
	struct ProjectMetadata {
		
		/** @brief Formal display identifier name of the target compiled project. */
		std::string projectName;
		
		/** @brief Introductory descriptive synopsis of the package. */
		std::string projectDescription;
		
		/** @brief Target version control host location URL string. */
		std::string repositoryUrl;
		
		/** @brief Native license identifier identifier tag (e.g., "MIT", "Apache-2.0"). */
		std::string licenseIdentifier;
		
		/** @brief Extracted leading text paragraphs from the repository's root Markdown README. */
		std::string readmeExcerpt;
		
		/** @brief Toggle state tracking whether an index README file was discovered and parsed. */
		bool readmeFound = false;
		
	};
	
	/**
	 * @struct DocumentationSite
	 * @brief The absolute root data model encapsulating a full project documentation layout tree run.
	 * * Contains the base tree node, environment metadata checkpoints, and holistic project counters
	 * heavily utilized by site compilers and dashboard telemetry generators.
	 */
	struct DocumentationSite {
		
		/** @brief Entry branch index node anchoring the complete hierarchical package data model tree. */
		PackageTreeNode rootPackage;
		
		/** @brief ISO-standard production date/time marker defining when the documentation was rendered. */
		std::string generationTimestamp;
		
		/** @brief Target source input directory track location path. */
		std::string inputBaseDirectory;
		
		/** @brief Descriptive identity block mapping corporate config rules and project descriptions. */
		ProjectMetadata projectMetadata;
		
		/** @brief Project-wide global counter tracking total parsed source module items. */
		uint32_t totalModules = 0;
		
		/** @brief Project-wide global counter tracking total discovered object types and entity records. */
		uint32_t totalEntities = 0;
		
		/** @brief Project-wide global counter tracking total extracted individual functions and methods. */
		uint32_t totalMethods = 0;
		
	};
	
} // namespace uranite::doc

#endif // _URANITE_DOC_MODEL_DOCUMENTATION_MODEL_HPP_
