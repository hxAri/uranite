
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

#include <fmt/core.h>
#include <sstream>

#include "uranite-doc/validator/doccomment-validator.hpp"

namespace uranite::doc {
	
	void DoccommentValidator::registerKnownErrorType( const std::string& errorTypeName ) {
		this->knownErrorTypes_.insert( errorTypeName );
	}
	
	std::vector<ValidationDiagnostic> DoccommentValidator::validateModule(
		const ModuleDocumentation& moduleDoc,
		const std::vector<formatter::comments::CommentEntry>& extractedComments
	) {
		this->diagnostics_.clear();
		for( const EntityDocumentationEntry& entity : moduleDoc.exportedEntities ) {
			this->validateEntity( entity, moduleDoc.sourceFilePath, extractedComments );
		}
		return this->diagnostics_;
	}
	
	void DoccommentValidator::validateEntity(
		const EntityDocumentationEntry& entity,
		const std::string& sourceFilePath,
		const std::vector<formatter::comments::CommentEntry>& extractedComments
	) {
		if( entity.accessModifier == "public" ) {
			this->validateDoccomment( entity.entityDoccomment, entity.entityName, sourceFilePath, entity.declarationLine, extractedComments );
		}
		for( const MethodDocumentationEntry& method : entity.methods ) {
			if( method.accessModifier == "public" ) {
				uint32_t methodLine = 0;
				for( const formatter::comments::CommentEntry& comment : extractedComments ) {
					if( comment.commentKind == formatter::comments::CommentKind::DocComment ) {
						int32_t delta = static_cast<int32_t>( comment.startLine ) - static_cast<int32_t>( entity.declarationLine );
						if( delta > 0 ) {
							methodLine = entity.declarationLine;
							break;
						}
					}
				}
				this->validateDoccomment( method.doccomment, method.methodName, sourceFilePath, entity.declarationLine, extractedComments );
			}
		}
		for( const EntityDocumentationEntry& nested : entity.nestedEntities ) {
			this->validateEntity( nested, sourceFilePath, extractedComments );
		}
	}
	
	void DoccommentValidator::validateDoccomment(
		const ParsedDoccomment& doccomment,
		const std::string& entityName,
		const std::string& sourceFilePath,
		uint32_t declarationLine,
		const std::vector<formatter::comments::CommentEntry>& extractedComments
	) {		
		if( doccomment.summaryLine.empty() ) {
			std::string rawContent = this->findRawDoccommentForLine( declarationLine, extractedComments );
			if( rawContent.empty() == false ) {
				ValidationDiagnostic diagnostic;
				diagnostic.severity = ValidationSeverity::Warning;
				diagnostic.ruleIdentifier = "missing-summary";
				diagnostic.diagnosticMessage = fmt::format( "\"{}\" doccomment missing summary line", entityName );
				diagnostic.sourceFilePath = sourceFilePath;
				diagnostic.lineNumber = declarationLine;
				diagnostic.columnNumber = 1;
				this->diagnostics_.push_back( diagnostic );
			}
			return;
		}
		std::string rawContent = this->findRawDoccommentForLine( declarationLine, extractedComments );
		if( rawContent.empty() ) {
			return;
		}		
		if( rawContent.find( "@param" ) != std::string::npos || rawContent.find( "@return" ) != std::string::npos ) {
			ValidationDiagnostic diagnostic;
			diagnostic.severity = ValidationSeverity::Error;
			diagnostic.ruleIdentifier = "param-style";
			diagnostic.diagnosticMessage = fmt::format( "\"{}\" uses @param/@return; use Parameters:/Returns: sections", entityName );
			diagnostic.sourceFilePath = sourceFilePath;
			diagnostic.lineNumber = declarationLine;
			diagnostic.columnNumber = 1;
			this->diagnostics_.push_back( diagnostic );
		}		
		if( doccomment.complexity.has_value() ) {
			const ComplexityDocumentation& complexityInfo = doccomment.complexity.value();
			if( complexityInfo.timeComplexity.empty() ) {
				ValidationDiagnostic diagnostic;
				diagnostic.severity = ValidationSeverity::Error;
				diagnostic.ruleIdentifier = "complexity-format";
				diagnostic.diagnosticMessage = fmt::format( "\"{}\" Complexity section missing Time: field", entityName );
				diagnostic.sourceFilePath = sourceFilePath;
				diagnostic.lineNumber = declarationLine;
				diagnostic.columnNumber = 1;
				this->diagnostics_.push_back( diagnostic );
			}
			if( complexityInfo.spaceComplexity.empty() ) {
				ValidationDiagnostic diagnostic;
				diagnostic.severity = ValidationSeverity::Error;
				diagnostic.ruleIdentifier = "complexity-format";
				diagnostic.diagnosticMessage = fmt::format( "\"{}\" Complexity section missing Space: field", entityName );
				diagnostic.sourceFilePath = sourceFilePath;
				diagnostic.lineNumber = declarationLine;
				diagnostic.columnNumber = 1;
				this->diagnostics_.push_back( diagnostic );
			}
		}
		this->validateRawDoccommentContent( rawContent, entityName, sourceFilePath, declarationLine );		
		if( this->knownErrorTypes_.empty() == false ) {
			for( const RaisesDocumentation& raisesEntry : doccomment.raises ) {
				if( this->knownErrorTypes_.count( raisesEntry.exceptionType ) == 0 ) {
					ValidationDiagnostic diagnostic;
					diagnostic.severity = ValidationSeverity::Warning;
					diagnostic.ruleIdentifier = "raises-known-type";
					diagnostic.diagnosticMessage = fmt::format( "\"{}\" raises unknown error type \"{}\"", entityName, raisesEntry.exceptionType );
					diagnostic.sourceFilePath = sourceFilePath;
					diagnostic.lineNumber = declarationLine;
					diagnostic.columnNumber = 1;
					this->diagnostics_.push_back( diagnostic );
				}
			}
		}
	}
	
	void DoccommentValidator::validateRawDoccommentContent(
		const std::string& rawContent,
		const std::string& entityName,
		const std::string& sourceFilePath,
		uint32_t lineNumber
	) {		
		size_t complexityContentPosition = rawContent.find( "Complexity:" );
		if( complexityContentPosition != std::string::npos ) {
			std::string afterComplexity = rawContent.substr( complexityContentPosition );
			size_t complexityLineEnd = afterComplexity.find( '\n' );
			if( complexityLineEnd != std::string::npos ) {
				std::string complexityFirstLine = afterComplexity.substr( 0, complexityLineEnd );
				if( complexityFirstLine.find( "Time:" ) != std::string::npos || complexityFirstLine.find( "Space:" ) != std::string::npos ) {
					ValidationDiagnostic inlineDiagnostic;
					inlineDiagnostic.severity = ValidationSeverity::Warning;
					inlineDiagnostic.ruleIdentifier = "complexity-structure";
					inlineDiagnostic.diagnosticMessage = fmt::format( "\"{}\" Complexity section must use separate indented Time:/Space: lines", entityName );
					inlineDiagnostic.sourceFilePath = sourceFilePath;
					inlineDiagnostic.lineNumber = lineNumber;
					inlineDiagnostic.columnNumber = 1;
					this->diagnostics_.push_back( inlineDiagnostic );
				}
				else {
					std::string remainingLines = afterComplexity.substr( complexityLineEnd + 1 );
					bool foundTimeIndented = false;
					bool foundSpaceIndented = false;
					std::istringstream lineStream( remainingLines );
					std::string currentValidationLine;
					while( std::getline( lineStream, currentValidationLine ) ) {
						if( currentValidationLine.empty() ) {
							continue;
						}
						if( currentValidationLine[0] != ' ' && currentValidationLine[0] != '\t' ) {
							break;
						}
						if( currentValidationLine.find( "Time:" ) != std::string::npos ) {
							foundTimeIndented = true;
						}
						if( currentValidationLine.find( "Space:" ) != std::string::npos ) {
							foundSpaceIndented = true;
						}
					}
					if( foundTimeIndented == false ) {
						ValidationDiagnostic timeDiagnostic;
						timeDiagnostic.severity = ValidationSeverity::Warning;
						timeDiagnostic.ruleIdentifier = "complexity-structure";
						timeDiagnostic.diagnosticMessage = fmt::format( "\"{}\" Complexity section missing indented Time: line", entityName );
						timeDiagnostic.sourceFilePath = sourceFilePath;
						timeDiagnostic.lineNumber = lineNumber;
						timeDiagnostic.columnNumber = 1;
						this->diagnostics_.push_back( timeDiagnostic );
					}
					if( foundSpaceIndented == false ) {
						ValidationDiagnostic spaceDiagnostic;
						spaceDiagnostic.severity = ValidationSeverity::Warning;
						spaceDiagnostic.ruleIdentifier = "complexity-structure";
						spaceDiagnostic.diagnosticMessage = fmt::format( "\"{}\" Complexity section missing indented Space: line", entityName );
						spaceDiagnostic.sourceFilePath = sourceFilePath;
						spaceDiagnostic.lineNumber = lineNumber;
						spaceDiagnostic.columnNumber = 1;
						this->diagnostics_.push_back( spaceDiagnostic );
					}
				}
			}
			std::istringstream proseCheckStream( rawContent );
			std::string proseLine;
			while( std::getline( proseCheckStream, proseLine ) ) {
				bool hasTimeAndSpace = proseLine.find( "time" ) != std::string::npos && proseLine.find( "space" ) != std::string::npos;
				bool hasBigO = proseLine.find( "O(" ) != std::string::npos;
				if( hasTimeAndSpace && hasBigO ) {
					ValidationDiagnostic proseDiagnostic;
					proseDiagnostic.severity = ValidationSeverity::Warning;
					proseDiagnostic.ruleIdentifier = "complexity-structure";
					proseDiagnostic.diagnosticMessage = fmt::format( "\"{}\" Complexity uses prose format; use structured Time:/Space: fields", entityName );
					proseDiagnostic.sourceFilePath = sourceFilePath;
					proseDiagnostic.lineNumber = lineNumber;
					proseDiagnostic.columnNumber = 1;
					this->diagnostics_.push_back( proseDiagnostic );
					break;
				}
			}
		}		
		size_t parametersPosition = rawContent.find( "Parameters:" );
		size_t returnsPosition = rawContent.find( "Returns:" );
		size_t raisesPosition = rawContent.find( "Raises:" );
		size_t complexityPosition = rawContent.find( "Complexity:" );
		std::vector<std::pair<std::string, size_t>> foundSections;
		if( parametersPosition != std::string::npos ) {
			foundSections.push_back( { "Parameters", parametersPosition } );
		}
		if( returnsPosition != std::string::npos ) {
			foundSections.push_back( { "Returns", returnsPosition } );
		}
		if( raisesPosition != std::string::npos ) {
			foundSections.push_back( { "Raises", raisesPosition } );
		}
		if( complexityPosition != std::string::npos ) {
			foundSections.push_back( { "Complexity", complexityPosition } );
		}
		for( size_t sectionIndex = 1; sectionIndex < foundSections.size(); sectionIndex++ ) {
			if( foundSections[sectionIndex].second < foundSections[sectionIndex - 1].second ) {
				ValidationDiagnostic diagnostic;
				diagnostic.severity = ValidationSeverity::Warning;
				diagnostic.ruleIdentifier = "section-order";
				diagnostic.diagnosticMessage = fmt::format( "\"{}\" doccomment sections out of canonical order (Parameters → Returns → Raises → Complexity)", entityName );
				diagnostic.sourceFilePath = sourceFilePath;
				diagnostic.lineNumber = lineNumber;
				diagnostic.columnNumber = 1;
				this->diagnostics_.push_back( diagnostic );
				break;
			}
		}
	}
	
	std::string DoccommentValidator::findRawDoccommentForLine(
		uint32_t declarationLine,
		const std::vector<formatter::comments::CommentEntry>& extractedComments
	) const {
		for( const formatter::comments::CommentEntry& comment : extractedComments ) {
			if( comment.commentKind == formatter::comments::CommentKind::DocComment ) {
				int32_t delta = static_cast<int32_t>( comment.startLine ) - static_cast<int32_t>( declarationLine );
				if( delta >= 1 && delta <= 3 ) {
					return comment.commentContent;
				}
			}
		}
		return "";
	}

} // namespace uranite::doc
