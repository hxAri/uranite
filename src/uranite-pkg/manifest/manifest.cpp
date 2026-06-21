
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

		std::istringstream stream( content );
		std::string line;

		while( std::getline( stream, line ) ) {
			int32_t indent = measureIndent( line );
			std::string trimmedLine = trimWhitespace( line );

			if( trimmedLine.empty() || trimmedLine[0] == '#' ) {
				continue;
			}

			while( pathStack.empty() == false && pathStack.back().first >= indent ) {
				pathStack.pop_back();
			}

			if( trimmedLine.size() >= 2 && trimmedLine[0] == '-' && trimmedLine[1] == ' ' ) {
				std::string itemValue = stripQuotes( trimmedLine.substr( 2 ) );

				if( pathStack.empty() == false ) {
					std::string parentKey = pathStack.back().second;
					std::string sectionPath;

					if( pathStack.size() >= 2 ) {
						std::vector<std::pair<int32_t, std::string>> sectionStack( pathStack.begin(), pathStack.end() - 1 );
						sectionPath = buildSectionPath( sectionStack );
					}

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
		if( packageSection.count( "authors" ) > 0 && packageSection["authors"].valueKind == ConfigValue::Kind::Array ) {
			manifest.authorList = packageSection["authors"].arrayValues;
		}

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
			manifest.dependencies.push_back( dependency );
		}

		for( std::pair<const std::string, std::unordered_map<std::string, ConfigValue>>& sectionPair : sections ) {
			std::string sectionName = sectionPair.first;
			if( sectionName.rfind( "dependencies.", 0 ) != 0 ) {
				continue;
			}
			if( sectionName.find( '.', 13 ) != std::string::npos ) {
				continue;
			}
			std::string packageName = sectionName.substr( 13 );
			PackageDependency dependency;
			dependency.packageName = packageName;
			std::unordered_map<std::string, ConfigValue>& fields = sectionPair.second;
			if( fields.count( "version" ) > 0 ) {
				dependency.versionConstraint = fields["version"].stringValue;
			}
			if( fields.count( "source" ) > 0 ) {
				dependency.sourceRepository = fields["source"].stringValue;
			}
			manifest.dependencies.push_back( dependency );
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
			manifest.developmentDependencies.push_back( dependency );
		}

		for( std::pair<const std::string, std::unordered_map<std::string, ConfigValue>>& sectionPair : sections ) {
			std::string sectionName = sectionPair.first;
			if( sectionName.rfind( "dev-dependencies.", 0 ) != 0 ) {
				continue;
			}
			if( sectionName.find( '.', 17 ) != std::string::npos ) {
				continue;
			}
			std::string packageName = sectionName.substr( 17 );
			PackageDependency dependency;
			dependency.packageName = packageName;
			dependency.isDevelopmentOnly = true;
			std::unordered_map<std::string, ConfigValue>& fields = sectionPair.second;
			if( fields.count( "version" ) > 0 ) {
				dependency.versionConstraint = fields["version"].stringValue;
			}
			if( fields.count( "source" ) > 0 ) {
				dependency.sourceRepository = fields["source"].stringValue;
			}
			manifest.developmentDependencies.push_back( dependency );
		}

		std::unordered_map<std::string, ConfigValue>& buildSection = sections["build"];
		if( buildSection.count( "optimization" ) > 0 ) {
			manifest.buildConfig.optimizationLevel = buildSection["optimization"].stringValue;
		}
		if( buildSection.count( "target" ) > 0 ) {
			manifest.buildConfig.targetTriple = buildSection["target"].stringValue;
		}
		if( buildSection.count( "output" ) > 0 ) {
			manifest.buildConfig.outputPath = buildSection["output"].stringValue;
		}
		if( buildSection.count( "modules-path" ) > 0 ) {
			manifest.buildConfig.modulesPath = buildSection["modules-path"].stringValue;
		}
		if( buildSection.count( "link-libraries" ) > 0 && buildSection["link-libraries"].valueKind == ConfigValue::Kind::Array ) {
			manifest.buildConfig.linkLibraries = buildSection["link-libraries"].arrayValues;
		}

		std::unordered_map<std::string, ConfigValue>& flagsSection = sections["build.flags"];
		if( flagsSection.count( "strip-debug" ) > 0 ) {
			manifest.buildConfig.stripDebugInfo = flagsSection["strip-debug"].stringValue == "true";
		}
		if( flagsSection.count( "emit-llvm" ) > 0 ) {
			manifest.buildConfig.emitLlvmIr = flagsSection["emit-llvm"].stringValue == "true";
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
