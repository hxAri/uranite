
//
// @author hxAri (hxari)
// @create 2026-06-15 03:00
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

#include <sstream>

#include "uranite-doc/extractor/doccomment-parser.hpp"

namespace uranite::doc {
	
	std::string DoccommentParser::stripDelimiters( const std::string& rawContent ) {
		std::string content = rawContent;
		if( content.size() >= 3 && content.substr( 0, 3 ) == "\"\"\"" ) {
			content = content.substr( 3 );
		}
		else if( content.size() >= 3 && content.substr( 0, 3 ) == "'''" ) {
			content = content.substr( 3 );
		}
		if( content.size() >= 3 ) {
			std::string lastThree = content.substr( content.size() - 3 );
			if( lastThree == "\"\"\"" || lastThree == "'''" ) {
				content = content.substr( 0, content.size() - 3 );
			}
		}
		return content;
	}
	
	std::vector<std::string> DoccommentParser::splitIntoLines( const std::string& content ) {
		std::vector<std::string> lines;
		std::istringstream stream( content );
		std::string line;
		while( std::getline( stream, line ) ) {
			lines.push_back( line );
		}
		return lines;
	}
	
	std::string DoccommentParser::trimWhitespace( const std::string& text ) {
		size_t startPosition = text.find_first_not_of( " \t\r\n" );
		if( startPosition == std::string::npos ) {
			return "";
		}
		size_t endPosition = text.find_last_not_of( " \t\r\n" );
		return text.substr( startPosition, endPosition - startPosition + 1 );
	}
	
	std::string DoccommentParser::trimLeadingWhitespace( const std::string& text ) {
		size_t startPosition = text.find_first_not_of( " \t" );
		if( startPosition == std::string::npos ) {
			return "";
		}
		return text.substr( startPosition );
	}
	
	bool DoccommentParser::startsWithSection( const std::string& line, const std::string& sectionName ) {
		std::string trimmedLine = trimLeadingWhitespace( line );
		return trimmedLine.find( sectionName + ":" ) == 0;
	}
	
	size_t DoccommentParser::measureIndentation( const std::string& line ) {
		size_t indentation = 0;
		for( char character : line ) {
			if( character == ' ' ) {
				indentation++;
			}
			else if( character == '\t' ) {
				indentation+= 4;
			}
			else {
				break;
			}
		}
		return indentation;
	}
	
	ParsedDoccomment DoccommentParser::parse( const std::string& rawDoccommentContent ) {
		ParsedDoccomment result;
		std::string strippedContent = stripDelimiters( rawDoccommentContent );
		std::vector<std::string> contentLines = splitIntoLines( strippedContent );
		if( contentLines.empty() ) {
			return result;
		}
		enum class ParseSection {
			Summary,
			ExtendedDescription,
			Parameters,
			Returns,
			Raises,
			Complexity
		};
		ParseSection currentSection = ParseSection::Summary;
		std::string accumulatedText;
		std::string currentParameterName;
		std::string currentParameterType;
		std::string currentRaisesType;
		size_t sectionBaseIndent = 0;
		for( size_t lineIndex = 0; lineIndex < contentLines.size(); lineIndex++ ) {
			std::string rawLine = contentLines[lineIndex];
			std::string trimmedLine = trimLeadingWhitespace( rawLine );
			if( trimmedLine.empty() ) {
				if( currentSection == ParseSection::Summary && result.summaryLine.empty() == false ) {
					currentSection = ParseSection::ExtendedDescription;
				}
				if( currentSection == ParseSection::ExtendedDescription ) {
					if( accumulatedText.empty() == false ) {
						accumulatedText+= "\n";
					}
				}
				continue;
			}
			if( startsWithSection( rawLine, "Parameters" ) ) {
				if( currentSection == ParseSection::Summary || currentSection == ParseSection::ExtendedDescription ) {
					result.extendedDescription = trimWhitespace( accumulatedText );
					accumulatedText.clear();
				}
				currentSection = ParseSection::Parameters;
				sectionBaseIndent = measureIndentation( rawLine );
				continue;
			}
			if( startsWithSection( rawLine, "Returns" ) ) {
				if( currentSection == ParseSection::Parameters && currentParameterName.empty() == false ) {
					ParameterDocumentation parameterEntry;
					parameterEntry.parameterName = currentParameterName;
					parameterEntry.parameterType = currentParameterType;
					parameterEntry.descriptionText = trimWhitespace( accumulatedText );
					result.parameters.push_back( parameterEntry );
					currentParameterName.clear();
					currentParameterType.clear();
					accumulatedText.clear();
				}
				else if( currentSection == ParseSection::Summary || currentSection == ParseSection::ExtendedDescription ) {
					result.extendedDescription = trimWhitespace( accumulatedText );
					accumulatedText.clear();
				}
				currentSection = ParseSection::Returns;
				sectionBaseIndent = measureIndentation( rawLine );
				continue;
			}
			if( startsWithSection( rawLine, "Raises" ) ) {
				if( currentSection == ParseSection::Parameters && currentParameterName.empty() == false ) {
					ParameterDocumentation parameterEntry;
					parameterEntry.parameterName = currentParameterName;
					parameterEntry.parameterType = currentParameterType;
					parameterEntry.descriptionText = trimWhitespace( accumulatedText );
					result.parameters.push_back( parameterEntry );
					currentParameterName.clear();
					currentParameterType.clear();
					accumulatedText.clear();
				}
				else if( currentSection == ParseSection::Returns ) {
					ReturnsDocumentation returnsEntry;
					std::string returnsText = trimWhitespace( accumulatedText );
					std::vector<std::string> returnsLines = splitIntoLines( returnsText );
					if( returnsLines.empty() == false ) {
						std::string firstReturnsLine = trimWhitespace( returnsLines[0] );
						if( firstReturnsLine.size() > 1 && firstReturnsLine.back() == ':' ) {
							returnsEntry.returnType = firstReturnsLine.substr( 0, firstReturnsLine.size() - 1 );
							std::string remainingDescription;
							for( size_t returnLineIndex = 1; returnLineIndex < returnsLines.size(); returnLineIndex++ ) {
								if( remainingDescription.empty() == false ) {
									remainingDescription+= " ";
								}
								remainingDescription+= trimWhitespace( returnsLines[returnLineIndex] );
							}
							returnsEntry.descriptionText = remainingDescription;
						}
						else {
							returnsEntry.descriptionText = returnsText;
						}
					}
					result.returns = returnsEntry;
					accumulatedText.clear();
				}
				else if( currentSection == ParseSection::Summary || currentSection == ParseSection::ExtendedDescription ) {
					result.extendedDescription = trimWhitespace( accumulatedText );
					accumulatedText.clear();
				}
				currentSection = ParseSection::Raises;
				sectionBaseIndent = measureIndentation( rawLine );
				continue;
			}
			if( startsWithSection( rawLine, "Complexity" ) ) {
				if( currentSection == ParseSection::Raises && currentRaisesType.empty() == false ) {
					RaisesDocumentation raisesEntry;
					raisesEntry.exceptionType = currentRaisesType;
					raisesEntry.conditionDescription = trimWhitespace( accumulatedText );
					result.raises.push_back( raisesEntry );
					currentRaisesType.clear();
					accumulatedText.clear();
				}
				else if( currentSection == ParseSection::Parameters && currentParameterName.empty() == false ) {
					ParameterDocumentation parameterEntry;
					parameterEntry.parameterName = currentParameterName;
					parameterEntry.parameterType = currentParameterType;
					parameterEntry.descriptionText = trimWhitespace( accumulatedText );
					result.parameters.push_back( parameterEntry );
					currentParameterName.clear();
					currentParameterType.clear();
					accumulatedText.clear();
				}
				else if( currentSection == ParseSection::Returns ) {
					ReturnsDocumentation returnsEntry;
					returnsEntry.descriptionText = trimWhitespace( accumulatedText );
					result.returns = returnsEntry;
					accumulatedText.clear();
				}
				else if( currentSection == ParseSection::Summary || currentSection == ParseSection::ExtendedDescription ) {
					result.extendedDescription = trimWhitespace( accumulatedText );
					accumulatedText.clear();
				}
				currentSection = ParseSection::Complexity;
				sectionBaseIndent = measureIndentation( rawLine );
				continue;
			}
			switch( currentSection ) {
				case ParseSection::Summary: {
					if( result.summaryLine.empty() ) {
						result.summaryLine = trimmedLine;
					}
					else {
						result.summaryLine+= " " + trimmedLine;
					}
					break;
				}
				case ParseSection::ExtendedDescription: {
					if( accumulatedText.empty() == false ) {
						accumulatedText+= " ";
					}
					accumulatedText+= trimmedLine;
					break;
				}
				case ParseSection::Parameters: {
					size_t lineIndent = measureIndentation( rawLine );
					if( lineIndent <= sectionBaseIndent + 8 ) {
						if( currentParameterName.empty() == false ) {
							ParameterDocumentation parameterEntry;
							parameterEntry.parameterName = currentParameterName;
							parameterEntry.parameterType = currentParameterType;
							parameterEntry.descriptionText = trimWhitespace( accumulatedText );
							result.parameters.push_back( parameterEntry );
							accumulatedText.clear();
						}
						size_t parenOpen = trimmedLine.find( '(' );
						size_t parenClose = trimmedLine.find( ')' );
						size_t colonPosition = trimmedLine.find( ':' );
						if( parenOpen != std::string::npos && parenClose != std::string::npos && parenClose > parenOpen ) {
							currentParameterName = trimWhitespace( trimmedLine.substr( 0, parenOpen ) );
							currentParameterType = trimWhitespace( trimmedLine.substr( parenOpen + 1, parenClose - parenOpen - 1 ) );
						}
						else if( colonPosition != std::string::npos ) {
							currentParameterName = trimWhitespace( trimmedLine.substr( 0, colonPosition ) );
							currentParameterType.clear();
						}
						else {
							currentParameterName = trimmedLine;
							currentParameterType.clear();
						}
					}
					else {
						if( accumulatedText.empty() == false ) {
							accumulatedText+= " ";
						}
						accumulatedText+= trimmedLine;
					}
					break;
				}
				case ParseSection::Returns: {
					if( accumulatedText.empty() == false ) {
						accumulatedText+= "\n";
					}
					accumulatedText+= trimmedLine;
					break;
				}
				case ParseSection::Raises: {
					size_t colonPosition = trimmedLine.find( ':' );
					if( colonPosition != std::string::npos && measureIndentation( rawLine ) <= sectionBaseIndent + 8 ) {
						if( currentRaisesType.empty() == false ) {
							RaisesDocumentation raisesEntry;
							raisesEntry.exceptionType = currentRaisesType;
							raisesEntry.conditionDescription = trimWhitespace( accumulatedText );
							result.raises.push_back( raisesEntry );
							accumulatedText.clear();
						}
						currentRaisesType = trimWhitespace( trimmedLine.substr( 0, colonPosition ) );
						std::string afterColon = trimWhitespace( trimmedLine.substr( colonPosition + 1 ) );
						if( afterColon.empty() == false ) {
							accumulatedText = afterColon;
						}
					}
					else {
						if( accumulatedText.empty() == false ) {
							accumulatedText+= " ";
						}
						accumulatedText+= trimmedLine;
					}
					break;
				}
				case ParseSection::Complexity: {
					if( trimmedLine.find( "Time:" ) == 0 ) {
						ComplexityDocumentation complexityEntry;
						if( result.complexity.has_value() ) {
							complexityEntry = result.complexity.value();
						}
						complexityEntry.timeComplexity = trimWhitespace( trimmedLine.substr( 5 ) );
						result.complexity = complexityEntry;
					}
					else if( trimmedLine.find( "Space:" ) == 0 ) {
						ComplexityDocumentation complexityEntry;
						if( result.complexity.has_value() ) {
							complexityEntry = result.complexity.value();
						}
						complexityEntry.spaceComplexity = trimWhitespace( trimmedLine.substr( 6 ) );
						result.complexity = complexityEntry;
					}
					break;
				}
			}
		}
		if( currentSection == ParseSection::Summary ) {
			// Summary only — no extended description
		}
		else if( currentSection == ParseSection::ExtendedDescription ) {
			result.extendedDescription = trimWhitespace( accumulatedText );
		}
		else if( currentSection == ParseSection::Parameters && currentParameterName.empty() == false ) {
			ParameterDocumentation parameterEntry;
			parameterEntry.parameterName = currentParameterName;
			parameterEntry.parameterType = currentParameterType;
			parameterEntry.descriptionText = trimWhitespace( accumulatedText );
			result.parameters.push_back( parameterEntry );
		}
		else if( currentSection == ParseSection::Returns ) {
			ReturnsDocumentation returnsEntry;
			std::string returnsText = trimWhitespace( accumulatedText );
			std::vector<std::string> returnsLines = splitIntoLines( returnsText );
			if( returnsLines.empty() == false ) {
				std::string firstReturnsLine = trimWhitespace( returnsLines[0] );
				if( firstReturnsLine.size() > 1 && firstReturnsLine.back() == ':' ) {
					returnsEntry.returnType = firstReturnsLine.substr( 0, firstReturnsLine.size() - 1 );
					std::string remainingDescription;
					for( size_t returnLineIndex = 1; returnLineIndex < returnsLines.size(); returnLineIndex++ ) {
						if( remainingDescription.empty() == false ) {
							remainingDescription+= " ";
						}
						remainingDescription+= trimWhitespace( returnsLines[returnLineIndex] );
					}
					returnsEntry.descriptionText = remainingDescription;
				}
				else {
					returnsEntry.descriptionText = returnsText;
				}
			}
			result.returns = returnsEntry;
		}
		else if( currentSection == ParseSection::Raises && currentRaisesType.empty() == false ) {
			RaisesDocumentation raisesEntry;
			raisesEntry.exceptionType = currentRaisesType;
			raisesEntry.conditionDescription = trimWhitespace( accumulatedText );
			result.raises.push_back( raisesEntry );
		}
		return result;
	}

}
