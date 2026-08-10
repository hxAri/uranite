
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "manifest.hpp"

namespace uranite::pkg {

	std::string ManifestParser::trimWhitespace( const std::string& text ) {
		size_t startPosition = text.find_first_not_of( " \t\r\n" );
		if( startPosition == std::string::npos ) {
			return "";
		}
		size_t endPosition = text.find_last_not_of( " \t\r\n" );
		return text.substr( startPosition, endPosition - startPosition + 1 );
	}

	std::string ManifestParser::stripQuotes( const std::string& text ) {
		std::string trimmed = trimWhitespace( text );
		if( trimmed.size() >= 2 ) {
			if( ( trimmed.front() == '"' && trimmed.back() == '"' ) ||
				( trimmed.front() == '\'' && trimmed.back() == '\'' ) ) {
				return trimmed.substr( 1, trimmed.size() - 2 );
			}
		}
		return trimmed;
	}

	int32_t ManifestParser::measureIndent( const std::string& line ) {
		int32_t indentCount = 0;
		for( size_t charIndex = 0; charIndex < line.size(); charIndex++ ) {
			if( line[charIndex] == ' ' ) {
				indentCount++;
			}
			else if( line[charIndex] == '\t' ) {
				indentCount+= 4;
			}
			else {
				break;
			}
		}
		return indentCount;
	}

	std::string ManifestParser::buildSectionPath( const std::vector<std::pair<int32_t, std::string>>& pathStack ) {
		std::string sectionPath;
		for( size_t entryIndex = 0; entryIndex < pathStack.size(); entryIndex++ ) {
			if( entryIndex > 0 ) {
				sectionPath+= ".";
			}
			sectionPath+= pathStack[entryIndex].second;
		}
		return sectionPath;
	}

	std::vector<std::string> ManifestParser::parseInlineSequence( const std::string& sequenceContent ) {
		std::vector<std::string> elements;
		std::string content = trimWhitespace( sequenceContent );

		if( content.size() >= 2 && content.front() == '[' && content.back() == ']' ) {
			content = content.substr( 1, content.size() - 2 );
		}

		std::string currentElement;
		bool insideQuotes = false;
		int32_t braceDepth = 0;

		for( size_t charIndex = 0; charIndex < content.size(); charIndex++ ) {
			char character = content[charIndex];

			if( character == '"' && ( charIndex == 0 || content[charIndex - 1] != '\\' ) ) {
				insideQuotes = !insideQuotes;
				currentElement+= character;
			}
			else if( character == '\'' && insideQuotes == false && ( charIndex == 0 || content[charIndex - 1] != '\\' ) ) {
				insideQuotes = !insideQuotes;
				currentElement+= character;
			}
			else if( character == '{' && insideQuotes == false ) {
				braceDepth++;
				currentElement+= character;
			}
			else if( character == '}' && insideQuotes == false ) {
				braceDepth--;
				currentElement+= character;
			}
			else if( character == ',' && insideQuotes == false && braceDepth == 0 ) {
				std::string trimmedElement = trimWhitespace( currentElement );
				if( trimmedElement.empty() == false ) {
					elements.push_back( stripQuotes( trimmedElement ) );
				}
				currentElement.clear();
			}
			else {
				currentElement+= character;
			}
		}

		std::string lastElement = trimWhitespace( currentElement );
		if( lastElement.empty() == false ) {
			elements.push_back( stripQuotes( lastElement ) );
		}

		return elements;
	}

	std::unordered_map<std::string, std::string> ManifestParser::parseInlineMapping( const std::string& mappingContent ) {
		std::unordered_map<std::string, std::string> result;
		std::string content = trimWhitespace( mappingContent );

		if( content.size() >= 2 && content.front() == '{' && content.back() == '}' ) {
			content = content.substr( 1, content.size() - 2 );
		}

		std::string currentPair;
		bool insideQuotes = false;

		for( size_t charIndex = 0; charIndex < content.size(); charIndex++ ) {
			char character = content[charIndex];

			if( character == '"' && ( charIndex == 0 || content[charIndex - 1] != '\\' ) ) {
				insideQuotes = !insideQuotes;
				currentPair+= character;
			}
			else if( character == ',' && insideQuotes == false ) {
				size_t colonPosition = currentPair.find( ':' );
				if( colonPosition != std::string::npos ) {
					std::string key = trimWhitespace( currentPair.substr( 0, colonPosition ) );
					std::string value = stripQuotes( currentPair.substr( colonPosition + 1 ) );
					result[key] = value;
				}
				currentPair.clear();
			}
			else {
				currentPair+= character;
			}
		}

		if( currentPair.empty() == false ) {
			size_t colonPosition = currentPair.find( ':' );
			if( colonPosition != std::string::npos ) {
				std::string key = trimWhitespace( currentPair.substr( 0, colonPosition ) );
				std::string value = stripQuotes( currentPair.substr( colonPosition + 1 ) );
				result[key] = value;
			}
		}

		return result;
	}

	std::unordered_map<std::string, std::unordered_map<std::string, ManifestParser::ConfigValue>> ManifestParser::parseYaml( const std::string& content ) {
		std::unordered_map<std::string, std::unordered_map<std::string, ConfigValue>> sections;

		std::vector<std::pair<int32_t, std::string>> pathStack;
		std::unordered_map<std::string, size_t> arrayObjectIndexCounters;

		std::istringstream stream( content );
		std::string line;

		while( std::getline( stream, line ) ) {
			int32_t indent = measureIndent( line );
			std::string trimmedLine = trimWhitespace( line );

			if( trimmedLine.empty() || trimmedLine[0] == '#' ) {
				continue;
			}

			bool isArrayItem = ( trimmedLine.size() >= 2 && trimmedLine[0] == '-' && ( trimmedLine[1] == ' ' || trimmedLine[1] == '\t' ) );
			int32_t popThreshold = isArrayItem ? indent + 1 : indent;
			while( pathStack.empty() == false && pathStack.back().first >= popThreshold ) {
				pathStack.pop_back();
			}

			if( isArrayItem ) {
				std::string itemContent = trimWhitespace( trimmedLine.substr( 2 ) );

				if( pathStack.empty() ) {
					continue;
				}

				std::string parentKey = pathStack.back().second;
				std::string sectionPath;
				if( pathStack.size() >= 2 ) {
					std::vector<std::pair<int32_t, std::string>> sectionStack( pathStack.begin(), pathStack.end() - 1 );
					sectionPath = buildSectionPath( sectionStack );
				}

				size_t itemColonPosition = std::string::npos;
				bool itemInsideQuotes = false;
				for( size_t charIndex = 0; charIndex < itemContent.size(); charIndex++ ) {
					if( itemContent[charIndex] == '"' || itemContent[charIndex] == '\'' ) {
						itemInsideQuotes = !itemInsideQuotes;
					}
					else if( itemContent[charIndex] == ':' && itemInsideQuotes == false ) {
						if( charIndex + 1 < itemContent.size() && itemContent[charIndex + 1] == ' ' ) {
							itemColonPosition = charIndex;
							break;
						}
						else if( charIndex + 1 == itemContent.size() ) {
							itemColonPosition = charIndex;
							break;
						}
					}
				}

				if( itemColonPosition != std::string::npos && itemContent.front() != '"' && itemContent.front() != '\'' ) {
					std::string fullArrayKey = sectionPath.empty() ? parentKey : sectionPath + "." + parentKey;
					size_t objectIndex = arrayObjectIndexCounters[fullArrayKey]++;
					std::string indexedSectionPath = fullArrayKey + "." + std::to_string( objectIndex );

					std::string firstKey = trimWhitespace( itemContent.substr( 0, itemColonPosition ) );
					std::string firstRawValue;
					if( itemColonPosition + 1 < itemContent.size() ) {
						firstRawValue = trimWhitespace( itemContent.substr( itemColonPosition + 1 ) );
					}

					if( firstRawValue.empty() == false ) {
						ConfigValue scalarValue;
						scalarValue.valueKind = ConfigValue::Kind::String;
						scalarValue.stringValue = stripQuotes( firstRawValue );
						sections[indexedSectionPath][firstKey] = scalarValue;
					}

					pathStack.push_back( { indent + 1, std::to_string( objectIndex ) } );
				}
				else {
					std::string itemValue = stripQuotes( itemContent );
					sections[sectionPath][parentKey].valueKind = ConfigValue::Kind::Array;
					sections[sectionPath][parentKey].arrayValues.push_back( itemValue );
				}
				continue;
			}

			size_t colonPosition = std::string::npos;
			bool insideQuotes = false;

			for( size_t charIndex = 0; charIndex < trimmedLine.size(); charIndex++ ) {
				char character = trimmedLine[charIndex];
				if( character == '"' || character == '\'' ) {
					insideQuotes = !insideQuotes;
				}
				else if( character == ':' && insideQuotes == false ) {
					if( charIndex + 1 < trimmedLine.size() && trimmedLine[charIndex + 1] == ' ' ) {
						colonPosition = charIndex;
						break;
					}
					else if( charIndex + 1 == trimmedLine.size() ) {
						colonPosition = charIndex;
						break;
					}
				}
			}

			if( colonPosition == std::string::npos ) {
				continue;
			}

			std::string key = trimWhitespace( trimmedLine.substr( 0, colonPosition ) );
			std::string rawValue;
			if( colonPosition + 1 < trimmedLine.size() ) {
				rawValue = trimWhitespace( trimmedLine.substr( colonPosition + 1 ) );
			}

			if( rawValue.empty() ) {
				pathStack.push_back( { indent, key } );
				continue;
			}

			if( rawValue == "{}" ) {
				continue;
			}

			std::string sectionPath = buildSectionPath( pathStack );

			if( rawValue == "[]" ) {
				ConfigValue emptyArray;
				emptyArray.valueKind = ConfigValue::Kind::Array;
				sections[sectionPath][key] = emptyArray;
			}
			else if( rawValue.front() == '[' ) {
				ConfigValue arrayValue;
				arrayValue.valueKind = ConfigValue::Kind::Array;
				arrayValue.arrayValues = parseInlineSequence( rawValue );
				sections[sectionPath][key] = arrayValue;
			}
			else if( rawValue.front() == '{' ) {
				ConfigValue tableValue;
				tableValue.valueKind = ConfigValue::Kind::InlineTable;
				tableValue.tableValues = parseInlineMapping( rawValue );
				sections[sectionPath][key] = tableValue;
			}
			else {
				ConfigValue scalarValue;
				scalarValue.valueKind = ConfigValue::Kind::String;
				scalarValue.stringValue = stripQuotes( rawValue );
				sections[sectionPath][key] = scalarValue;
			}
		}

		return sections;
	}

	PackageDependency ManifestParser::parseDependencyString( const std::string& dependencySpec, bool isDevelopmentOnly ) {
		PackageDependency dependency;
		dependency.isDevelopmentOnly = isDevelopmentOnly;

		size_t atPosition = dependencySpec.find( '@' );
		if( atPosition != std::string::npos ) {
			std::string sourceIdentifier = dependencySpec.substr( 0, atPosition );
			dependency.versionConstraint = dependencySpec.substr( atPosition + 1 );

			size_t slashPosition = sourceIdentifier.find( '/' );
			if( slashPosition != std::string::npos ) {
				dependency.packageName = sourceIdentifier.substr( slashPosition + 1 );
				dependency.sourceRepository = sourceIdentifier;
			}
			else {
				dependency.packageName = sourceIdentifier;
			}
		}
		else {
			size_t slashPosition = dependencySpec.find( '/' );
			if( slashPosition != std::string::npos ) {
				dependency.packageName = dependencySpec.substr( slashPosition + 1 );
				dependency.sourceRepository = dependencySpec;
			}
			else {
				dependency.packageName = dependencySpec;
			}
		}

		return dependency;
	}

	PackageManifest ManifestParser::parseFromString( const std::string& yamlContent ) {
		std::unordered_map<std::string, std::unordered_map<std::string, ConfigValue>> sections = parseYaml( yamlContent );
		PackageManifest manifest;

		std::unordered_map<std::string, ConfigValue>& packageSection = sections["package"];
		if( packageSection.count( "name" ) > 0 ) {
			manifest.packageName = packageSection["name"].stringValue;
		}
		if( packageSection.count( "version" ) > 0 ) {
			manifest.packageVersion = packageSection["version"].stringValue;
		}
		if( packageSection.count( "description" ) > 0 ) {
			manifest.packageDescription = packageSection["description"].stringValue;
		}
		if( packageSection.count( "license" ) > 0 ) {
			manifest.licenseIdentifier = packageSection["license"].stringValue;
		}
		if( packageSection.count( "entry" ) > 0 ) {
			manifest.entrySourceFile = packageSection["entry"].stringValue;
		}
		if( packageSection.count( "type" ) > 0 ) {
			manifest.packageType = packageSection["type"].stringValue;
		}
		if( packageSection.count( "keywords" ) > 0 && packageSection["keywords"].valueKind == ConfigValue::Kind::Array ) {
			manifest.keywords = packageSection["keywords"].arrayValues;
		}
		if( packageSection.count( "exclude" ) > 0 && packageSection["exclude"].valueKind == ConfigValue::Kind::Array ) {
			manifest.excludePatterns = packageSection["exclude"].arrayValues;
		}

		for( size_t authorIndex = 0; ; authorIndex++ ) {
			std::string authorSectionPath = "package.authors." + std::to_string( authorIndex );
			if( sections.count( authorSectionPath ) == 0 ) {
				break;
			}
			std::unordered_map<std::string, ConfigValue>& authorFields = sections[authorSectionPath];
			AuthorInfo authorInfo;
			if( authorFields.count( "name" ) > 0 ) {
				authorInfo.name = authorFields["name"].stringValue;
			}
			if( authorFields.count( "email" ) > 0 ) {
				authorInfo.email = authorFields["email"].stringValue;
			}
			if( authorFields.count( "role" ) > 0 ) {
				authorInfo.role = authorFields["role"].stringValue;
			}
			if( authorFields.count( "url" ) > 0 ) {
				authorInfo.url = authorFields["url"].stringValue;
			}
			manifest.authorInfoList.push_back( authorInfo );
		}

		if( manifest.authorInfoList.empty() && packageSection.count( "authors" ) > 0 &&
			packageSection["authors"].valueKind == ConfigValue::Kind::Array ) {
			for( const std::string& authorString : packageSection["authors"].arrayValues ) {
				AuthorInfo authorInfo;
				authorInfo.name = authorString;
				manifest.authorInfoList.push_back( authorInfo );
			}
		}

		std::unordered_map<std::string, ConfigValue>& repositorySection = sections["package.repository"];
		if( repositorySection.count( "type" ) > 0 ) {
			manifest.repository.type = repositorySection["type"].stringValue;
		}
		if( repositorySection.count( "url" ) > 0 ) {
			manifest.repository.url = repositorySection["url"].stringValue;
		}

		std::unordered_map<std::string, ConfigValue>& requirementsSection = sections["requirements"];
		if( requirementsSection.count( "path" ) > 0 ) {
			manifest.requirements.modulesPath = requirementsSection["path"].stringValue;
		}
		if( requirementsSection.count( "dependencies" ) > 0 &&
			requirementsSection["dependencies"].valueKind == ConfigValue::Kind::Array ) {
			for( const std::string& depSpec : requirementsSection["dependencies"].arrayValues ) {
				manifest.requirements.dependencies.push_back( parseDependencyString( depSpec, false ) );
			}
		}
		if( requirementsSection.count( "dev-dependencies" ) > 0 &&
			requirementsSection["dev-dependencies"].valueKind == ConfigValue::Kind::Array ) {
			for( const std::string& depSpec : requirementsSection["dev-dependencies"].arrayValues ) {
				manifest.requirements.developmentDependencies.push_back( parseDependencyString( depSpec, true ) );
			}
		}
		if( requirementsSection.count( "system-dependencies" ) > 0 &&
			requirementsSection["system-dependencies"].valueKind == ConfigValue::Kind::Array ) {
			manifest.requirements.systemDependencies = requirementsSection["system-dependencies"].arrayValues;
		}

		for( std::pair<const std::string, std::unordered_map<std::string, ConfigValue>>& sectionPair : sections ) {
			std::string sectionName = sectionPair.first;
			if( sectionName.rfind( "requirements.", 0 ) != 0 ) {
				continue;
			}
			std::string remainder = sectionName.substr( 13 );
			if( remainder.rfind( "dependencies.", 0 ) == 0 || remainder.rfind( "dev-dependencies.", 0 ) == 0 ) {
				continue;
			}
			if( remainder.find( '.' ) != std::string::npos ) {
				continue;
			}
			bool isDev = false;
			PackageDependency dependency;
			dependency.packageName = remainder;
			std::unordered_map<std::string, ConfigValue>& fields = sectionPair.second;
			if( fields.count( "version" ) > 0 ) {
				dependency.versionConstraint = fields["version"].stringValue;
			}
			if( fields.count( "source" ) > 0 ) {
				dependency.sourceRepository = fields["source"].stringValue;
			}
			if( isDev ) {
				manifest.requirements.developmentDependencies.push_back( dependency );
			}
			else {
				manifest.requirements.dependencies.push_back( dependency );
			}
		}

		// Legacy format: top-level dependencies/dev-dependencies sections
		for( std::pair<const std::string, ConfigValue>& depEntry : sections["dependencies"] ) {
			PackageDependency dependency;
			dependency.packageName = depEntry.first;
			if( depEntry.second.valueKind == ConfigValue::Kind::InlineTable ) {
				if( depEntry.second.tableValues.count( "version" ) > 0 ) {
					dependency.versionConstraint = depEntry.second.tableValues["version"];
				}
				if( depEntry.second.tableValues.count( "source" ) > 0 ) {
					dependency.sourceRepository = depEntry.second.tableValues["source"];
				}
			}
			else {
				dependency.versionConstraint = depEntry.second.stringValue;
			}
			manifest.requirements.dependencies.push_back( dependency );
		}

		for( std::pair<const std::string, ConfigValue>& depEntry : sections["dev-dependencies"] ) {
			PackageDependency dependency;
			dependency.packageName = depEntry.first;
			dependency.isDevelopmentOnly = true;
			if( depEntry.second.valueKind == ConfigValue::Kind::InlineTable ) {
				if( depEntry.second.tableValues.count( "version" ) > 0 ) {
					dependency.versionConstraint = depEntry.second.tableValues["version"];
				}
				if( depEntry.second.tableValues.count( "source" ) > 0 ) {
					dependency.sourceRepository = depEntry.second.tableValues["source"];
				}
			}
			else {
				dependency.versionConstraint = depEntry.second.stringValue;
			}
			manifest.requirements.developmentDependencies.push_back( dependency );
		}

		std::unordered_map<std::string, ConfigValue>& buildSection = sections["build"];
		if( buildSection.count( "build-path" ) > 0 ) {
			manifest.buildConfig.buildPath = buildSection["build-path"].stringValue;
		}
		else if( buildSection.count( "output" ) > 0 ) {
			manifest.buildConfig.buildPath = buildSection["output"].stringValue;
		}
		if( buildSection.count( "optimization" ) > 0 ) {
			manifest.buildConfig.optimizationLevel = buildSection["optimization"].stringValue;
		}
		if( buildSection.count( "target" ) > 0 ) {
			manifest.buildConfig.targetTriple = buildSection["target"].stringValue;
		}
		if( buildSection.count( "modules-path" ) > 0 ) {
			manifest.buildConfig.modulesPath = buildSection["modules-path"].stringValue;
		}
		if( buildSection.count( "static-link" ) > 0 ) {
			manifest.buildConfig.staticLink = buildSection["static-link"].stringValue == "true";
		}
		if( buildSection.count( "strip-symbols" ) > 0 ) {
			manifest.buildConfig.stripSymbols = buildSection["strip-symbols"].stringValue == "true";
		}
		if( buildSection.count( "features" ) > 0 && buildSection["features"].valueKind == ConfigValue::Kind::Array ) {
			manifest.buildConfig.features = buildSection["features"].arrayValues;
		}
		if( buildSection.count( "include-paths" ) > 0 && buildSection["include-paths"].valueKind == ConfigValue::Kind::Array ) {
			manifest.buildConfig.includePaths = buildSection["include-paths"].arrayValues;
		}
		if( buildSection.count( "link-libraries" ) > 0 && buildSection["link-libraries"].valueKind == ConfigValue::Kind::Array ) {
			manifest.buildConfig.linkLibraries = buildSection["link-libraries"].arrayValues;
		}

		std::unordered_map<std::string, ConfigValue>& flagsSection = sections["build.flags"];
		if( flagsSection.count( "strip-debug" ) > 0 ) {
			manifest.buildConfig.stripSymbols = flagsSection["strip-debug"].stringValue == "true";
		}
		if( flagsSection.count( "emit-llvm" ) > 0 ) {
			manifest.buildConfig.emitLlvmIr = flagsSection["emit-llvm"].stringValue == "true";
		}

		auto parseProfile = []( const std::unordered_map<std::string, ConfigValue>& profileSection ) -> BuildProfile {
			BuildProfile profile;
			if( profileSection.count( "optimization" ) > 0 ) {
				profile.optimizationLevel = profileSection.at( "optimization" ).stringValue;
			}
			if( profileSection.count( "debug-info" ) > 0 ) {
				profile.debugInfo = profileSection.at( "debug-info" ).stringValue == "true";
			}
			if( profileSection.count( "strip-symbols" ) > 0 ) {
				profile.stripSymbols = profileSection.at( "strip-symbols" ).stringValue == "true";
			}
			if( profileSection.count( "link-time-optimization" ) > 0 ) {
				profile.linkTimeOptimization = profileSection.at( "link-time-optimization" ).stringValue == "true";
			}
			return profile;
		};

		if( sections.count( "build.profiles.dev" ) > 0 ) {
			manifest.buildConfig.profiles["dev"] = parseProfile( sections["build.profiles.dev"] );
		}
		if( sections.count( "build.profiles.release" ) > 0 ) {
			manifest.buildConfig.profiles["release"] = parseProfile( sections["build.profiles.release"] );
		}

		for( std::pair<const std::string, ConfigValue>& scriptEntry : sections["scripts"] ) {
			if( scriptEntry.second.valueKind == ConfigValue::Kind::String ) {
				manifest.scripts[scriptEntry.first] = scriptEntry.second.stringValue;
			}
		}

		std::unordered_map<std::string, ConfigValue>& testSection = sections["test"];
		if( testSection.count( "path" ) > 0 ) {
			manifest.testConfig.path = testSection["path"].stringValue;
		}
		else if( testSection.count( "entry" ) > 0 ) {
			manifest.testConfig.path = testSection["entry"].stringValue;
		}
		if( testSection.count( "coverage" ) > 0 ) {
			manifest.testConfig.coverage = testSection["coverage"].stringValue == "true";
		}
		if( testSection.count( "coverage-path" ) > 0 ) {
			manifest.testConfig.coveragePath = testSection["coverage-path"].stringValue;
		}
		if( testSection.count( "parallel" ) > 0 ) {
			manifest.testConfig.parallel = testSection["parallel"].stringValue == "true";
		}
		if( testSection.count( "timeout" ) > 0 ) {
			try {
				manifest.testConfig.timeout = std::stoi( testSection["timeout"].stringValue );
			}
			catch( ... ) {}
		}

		for( std::pair<const std::string, ConfigValue>& envEntry : sections["test.environment"] ) {
			manifest.testConfig.environment[envEntry.first] = envEntry.second.stringValue;
		}

		return manifest;
	}

	PackageManifest ManifestParser::parseFromFile( const std::string& filePath ) {
		std::ifstream fileStream( filePath );
		if( fileStream.is_open() == false ) {
			throw std::runtime_error( "could not open manifest file: " + filePath );
		}
		std::ostringstream contentStream;
		contentStream << fileStream.rdbuf();
		return parseFromString( contentStream.str() );
	}

} // namespace uranite::pkg
