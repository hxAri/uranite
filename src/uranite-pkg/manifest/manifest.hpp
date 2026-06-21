
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

#include <string>
#include <unordered_map>
#include <vector>

namespace uranite::pkg {

	struct PackageDependency {
		std::string packageName;
		std::string versionConstraint;
		std::string sourceRepository;
		bool isDevelopmentOnly = false;
	};

	struct BuildConfiguration {
		std::string optimizationLevel = "2";
		std::string targetTriple;
		std::string outputPath;
		std::string modulesPath = "modules";
		std::vector<std::string> linkLibraries;
		bool stripDebugInfo = false;
		bool emitLlvmIr = false;
	};

	struct PackageManifest {
		std::string packageName;
		std::string packageVersion;
		std::string packageDescription;
		std::vector<std::string> authorList;
		std::string licenseIdentifier;
		std::string entrySourceFile = "src/main.urn";
		std::string packageType = "binary";
		std::vector<PackageDependency> dependencies;
		std::vector<PackageDependency> developmentDependencies;
		BuildConfiguration buildConfig;
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
	};

} // namespace uranite::pkg

#endif // _URANITE_PKG_MANIFEST_MANIFEST_HPP_
