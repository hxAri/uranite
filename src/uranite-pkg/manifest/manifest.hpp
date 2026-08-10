
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#ifndef _URANITE_PKG_MANIFEST_MANIFEST_HPP_
#define _URANITE_PKG_MANIFEST_MANIFEST_HPP_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace uranite::pkg {

	struct AuthorInfo {
		std::string name;
		std::string email;
		std::string role;
		std::string url;
	};

	struct RepositoryInfo {
		std::string type;
		std::string url;
	};

	struct PackageDependency {
		std::string packageName;
		std::string versionConstraint;
		std::string sourceRepository;
		bool isDevelopmentOnly = false;
	};

	struct BuildProfile {
		std::string optimizationLevel;
		bool debugInfo = false;
		bool stripSymbols = false;
		bool linkTimeOptimization = false;
	};

	struct BuildConfiguration {
		std::string buildPath;
		std::string optimizationLevel = "2";
		std::string targetTriple;
		std::string modulesPath = "modules";
		std::vector<std::string> features;
		std::vector<std::string> includePaths;
		std::vector<std::string> linkLibraries;
		bool staticLink = false;
		bool stripSymbols = false;
		bool emitLlvmIr = false;
		std::unordered_map<std::string, BuildProfile> profiles;
	};

	using ScriptsConfiguration = std::unordered_map<std::string, std::string>;

	struct TestConfiguration {
		std::string path;
		bool coverage = false;
		std::string coveragePath;
		bool parallel = false;
		int32_t timeout = 0;
		std::unordered_map<std::string, std::string> environment;
	};

	struct RequirementsConfiguration {
		std::string modulesPath = "build/modules";
		std::vector<PackageDependency> dependencies;
		std::vector<PackageDependency> developmentDependencies;
		std::vector<std::string> systemDependencies;
	};

	struct PackageManifest {
		std::string packageName;
		std::string packageVersion;
		std::string packageDescription;
		std::string licenseIdentifier;
		std::string entrySourceFile = "src/main.urn";
		std::string packageType = "binary";
		std::vector<AuthorInfo> authorInfoList;
		std::vector<std::string> keywords;
		std::vector<std::string> excludePatterns;
		RepositoryInfo repository;
		RequirementsConfiguration requirements;
		BuildConfiguration buildConfig;
		ScriptsConfiguration scripts;
		TestConfiguration testConfig;
	};

	class ManifestParser {

		public:

			struct ConfigValue {
				enum class Kind { String, Array, InlineTable };
				Kind valueKind = Kind::String;
				std::string stringValue;
				std::vector<std::string> arrayValues;
				std::unordered_map<std::string, std::string> tableValues;
			};

			static PackageManifest parseFromFile( const std::string& filePath );
			static PackageManifest parseFromString( const std::string& yamlContent );
			static std::unordered_map<std::string, std::unordered_map<std::string, ConfigValue>> parseYaml( const std::string& content );

		private:

			static std::string trimWhitespace( const std::string& text );
			static std::string stripQuotes( const std::string& text );
			static int32_t measureIndent( const std::string& line );
			static std::string buildSectionPath( const std::vector<std::pair<int32_t, std::string>>& pathStack );
			static std::vector<std::string> parseInlineSequence( const std::string& sequenceContent );
			static std::unordered_map<std::string, std::string> parseInlineMapping( const std::string& mappingContent );
			static PackageDependency parseDependencyString( const std::string& dependencySpec, bool isDevelopmentOnly );
	};

} // namespace uranite::pkg

#endif // _URANITE_PKG_MANIFEST_MANIFEST_HPP_
