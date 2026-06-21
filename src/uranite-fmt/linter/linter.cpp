
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

#include <fmt/core.h>

#include "uranite/visitors/visitor.hpp"
#include "uranite-fmt/linter/linter.hpp"

namespace uranite::formatter::linter {
	
	Linter::Linter( const std::string& sourceFilePath, const std::vector<comments::CommentEntry>& extractedComments, const LintRuleConfig& ruleConfig )
		: sourceFilePath_( sourceFilePath ),
		  extractedComments_( extractedComments ),
		  ruleConfig_( ruleConfig ) {
		this->allowedShortNames_ = { "self", "it", "id", "io", "ip", "ok" };
	}
	
	std::vector<LintDiagnostic> Linter::lint( ast::nodes::Program& program ) {
		this->diagnostics_.clear();
		if( this->ruleConfig_.enableImportOnePerLine ) {
			std::unordered_map<std::string, std::vector<uint32_t>> singleImportsByPackage;
			for( const ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
				if( declaration->kind == ast::Node::Kind::ImportDeclaration ) {
					ast::nodes::ImportDeclaration& importDecl = static_cast<ast::nodes::ImportDeclaration&>( *declaration );
					if( importDecl.isFromImport && importDecl.importItems.size() == 1 && importDecl.importAll == false ) {
						std::string packagePath;
						for( size_t segmentIndex = 0; segmentIndex < importDecl.path.size(); segmentIndex++ ) {
							if( segmentIndex > 0 ) {
								packagePath+= ".";
							}
							packagePath+= importDecl.path[segmentIndex];
						}
						uint32_t lineNumber = importDecl.source != nullptr ? importDecl.source->location->line : 0;
						singleImportsByPackage[packagePath].push_back( lineNumber );
					}
				}
			}
			for( const std::pair<const std::string, std::vector<uint32_t>>& entry : singleImportsByPackage ) {
				if( entry.second.size() > 1 ) {
					for( uint32_t lineNumber : entry.second ) {
						this->emitDiagnostic(
							LintSeverity::Warning,
							"style/import-one-per-line",
							fmt::format( "multiple single-entity imports from \"{}\"; combine into one brace-wrapped import", entry.first ),
							lineNumber, 1
						);
					}
				}
			}
		}
		for( const ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration->kind == ast::Node::Kind::ImportDeclaration ) {
				continue;
			}
			if( declaration->kind == ast::Node::Kind::ExportDeclaration ) {
				ast::nodes::ExportDeclaration& exportDecl = static_cast<ast::nodes::ExportDeclaration&>( *declaration );
				if( exportDecl.declaration != nullptr ) {
					visitors::dispatchVisit( *this, *exportDecl.declaration );
				}
				continue;
			}
			if( declaration->kind == ast::Node::Kind::ConstantDeclaration ) {
				ast::nodes::ConstantDeclaration& constDecl = static_cast<ast::nodes::ConstantDeclaration&>( *declaration );
				if( this->ruleConfig_.enableCrypticVariable ) {
					uint32_t lineNumber = constDecl.source != nullptr ? constDecl.source->location->line : 0;
					uint32_t columnNumber = constDecl.source != nullptr ? constDecl.source->location->column : 0;
					this->checkVariableName( constDecl.name, lineNumber, columnNumber );
				}
				continue;
			}
			visitors::dispatchVisit( *this, *declaration );
		}
		return this->diagnostics_;
	}
	
	void Linter::emitDiagnostic( LintSeverity severity, const std::string& ruleIdentifier, const std::string& message, uint32_t lineNumber, uint32_t columnNumber ) {
		LintDiagnostic diagnostic;
		diagnostic.severity = severity;
		diagnostic.ruleIdentifier = ruleIdentifier;
		diagnostic.diagnosticMessage = message;
		diagnostic.sourceFilePath = this->sourceFilePath_;
		diagnostic.lineNumber = lineNumber;
		diagnostic.columnNumber = columnNumber;
		this->diagnostics_.push_back( diagnostic );
	}
	
	bool Linter::hasDoccommentForLine( uint32_t declarationLine ) const {
		for( const comments::CommentEntry& comment : this->extractedComments_ ) {
			if( comment.commentKind == comments::CommentKind::DocComment ) {
				int32_t delta = static_cast<int32_t>( comment.startLine ) - static_cast<int32_t>( declarationLine );
				if( delta >= 1 && delta <= 3 ) {
					return true;
				}
			}
		}
		return false;
	}
	
	std::string Linter::getDoccommentForLine( uint32_t declarationLine ) const {
		for( const comments::CommentEntry& comment : this->extractedComments_ ) {
			if( comment.commentKind == comments::CommentKind::DocComment ) {
				int32_t delta = static_cast<int32_t>( comment.startLine ) - static_cast<int32_t>( declarationLine );
				if( delta >= 1 && delta <= 3 ) {
					return comment.commentContent;
				}
			}
		}
		return "";
	}
	
	void Linter::checkVariableName( const std::string& variableName, uint32_t lineNumber, uint32_t columnNumber ) {
		if( this->ruleConfig_.enableCrypticVariable == false ) {
			return;
		}
		if( this->allowedShortNames_.count( variableName ) > 0 ) {
			return;
		}
		if( variableName.length() < this->ruleConfig_.minimumVariableNameLength ) {
			this->emitDiagnostic(
				LintSeverity::Warning,
				"naming/cryptic-variable",
				fmt::format( "variable \"{}\" has a cryptic name; use a descriptive identifier", variableName ),
				lineNumber, columnNumber
			);
		}
	}
	
	void Linter::checkParameterName( const std::string& parameterName, uint32_t lineNumber, uint32_t columnNumber ) {
		if( this->ruleConfig_.enableCrypticParameter == false ) {
			return;
		}
		if( this->allowedShortNames_.count( parameterName ) > 0 ) {
			return;
		}
		if( parameterName.length() < this->ruleConfig_.minimumVariableNameLength ) {
			this->emitDiagnostic(
				LintSeverity::Warning,
				"naming/cryptic-parameter",
				fmt::format( "parameter \"{}\" has a cryptic name; use a descriptive identifier", parameterName ),
				lineNumber, columnNumber
			);
		}
	}
	
	void Linter::checkDoccomment( const std::string& entityKind, const std::string& entityName, uint32_t declarationLine, uint32_t declarationColumn, bool hasBody ) {
		bool hasDoccomment = this->hasDoccommentForLine( declarationLine );
		if( this->ruleConfig_.enableMissingDoccomment && hasDoccomment == false ) {
			this->emitDiagnostic(
				LintSeverity::Warning,
				"doc/missing-doccomment",
				fmt::format( "public {} \"{}\" has no doccomment", entityKind, entityName ),
				declarationLine, declarationColumn
			);
			return;
		}
		if( hasDoccomment == false ) {
			return;
		}
		std::string docContent = this->getDoccommentForLine( declarationLine );
		if( this->ruleConfig_.enableAtParamStyle ) {
			if( docContent.find( "@param" ) != std::string::npos || docContent.find( "@return" ) != std::string::npos ) {
				this->emitDiagnostic(
					LintSeverity::Error,
					"doc/at-param-style",
					fmt::format( "{} \"{}\" uses @param/@return style; use Parameters:/Returns: sections instead", entityKind, entityName ),
					declarationLine, declarationColumn
				);
			}
		}
		if( this->ruleConfig_.enableMissingComplexity && hasBody ) {
			if( docContent.find( "Complexity:" ) == std::string::npos ) {
				this->emitDiagnostic(
					LintSeverity::Warning,
					"doc/missing-complexity",
					fmt::format( "{} \"{}\" doccomment missing Complexity: section", entityKind, entityName ),
					declarationLine, declarationColumn
				);
			}
		}
		if( this->ruleConfig_.enableInvalidComplexityFormat ) {
			size_t complexityPosition = docContent.find( "Complexity:" );
			if( complexityPosition != std::string::npos ) {
				std::string afterComplexity = docContent.substr( complexityPosition + 11 );
				bool hasTimeField = afterComplexity.find( "Time:" ) != std::string::npos;
				bool hasSpaceField = afterComplexity.find( "Space:" ) != std::string::npos;
				bool hasProseO = afterComplexity.find( "O(" ) != std::string::npos;
				if( hasProseO && hasTimeField == false && hasSpaceField == false ) {
					this->emitDiagnostic(
						LintSeverity::Error,
						"doc/invalid-complexity-format",
						fmt::format( "{} \"{}\" uses prose complexity format; use structured Time:/Space: fields", entityKind, entityName ),
						declarationLine, declarationColumn
					);
				}
			}
		}
	}
	
	void Linter::checkFunctionLength( const std::string& functionName, uint32_t startLine, uint32_t endLine ) {
		if( this->ruleConfig_.enableLongFunction == false ) {
			return;
		}
		uint32_t bodyLength = endLine - startLine;
		if( bodyLength > this->ruleConfig_.maximumFunctionBodyLines ) {
			this->emitDiagnostic(
				LintSeverity::Warning,
				"complexity/long-function",
				fmt::format( "function \"{}\" is {} lines long (limit: {})", functionName, bodyLength, this->ruleConfig_.maximumFunctionBodyLines ),
				startLine, 1
			);
		}
	}
	
	void Linter::walkBodyForNesting( const std::vector<ast::nodes::StatementSharedPointer>& body, uint32_t currentDepth ) {
		for( const ast::nodes::StatementSharedPointer& statement : body ) {
			if( statement == nullptr ) {
				continue;
			}
			if( statement->kind == ast::Node::Kind::IfStatement ) {
				ast::nodes::IfStatement& ifNode = static_cast<ast::nodes::IfStatement&>( *statement );
				uint32_t lineNumber = ifNode.source != nullptr ? ifNode.source->location->line : 0;
				uint32_t columnNumber = ifNode.source != nullptr ? ifNode.source->location->column : 0;
				uint32_t innerDepth = currentDepth + 1;
				if( this->ruleConfig_.enableDeepNesting && innerDepth > this->ruleConfig_.maximumNestingDepth ) {
					this->emitDiagnostic(
						LintSeverity::Warning,
						"complexity/deep-nesting",
						fmt::format( "nesting depth {} exceeds limit of {}", innerDepth, this->ruleConfig_.maximumNestingDepth ),
						lineNumber, columnNumber
					);
				}
				this->walkBodyForNesting( ifNode.thenBody, innerDepth );
				for( const std::pair<ast::nodes::ExpressionSharedPointer, std::vector<ast::nodes::StatementSharedPointer>>& elifBranch : ifNode.elifBranches ) {
					this->walkBodyForNesting( elifBranch.second, innerDepth );
				}
				this->walkBodyForNesting( ifNode.elseBody, innerDepth );
			}
			else if( statement->kind == ast::Node::Kind::WhileStatement ) {
				ast::nodes::WhileStatement& whileNode = static_cast<ast::nodes::WhileStatement&>( *statement );
				uint32_t lineNumber = whileNode.source != nullptr ? whileNode.source->location->line : 0;
				uint32_t columnNumber = whileNode.source != nullptr ? whileNode.source->location->column : 0;
				uint32_t innerDepth = currentDepth + 1;
				if( this->ruleConfig_.enableDeepNesting && innerDepth > this->ruleConfig_.maximumNestingDepth ) {
					this->emitDiagnostic(
						LintSeverity::Warning,
						"complexity/deep-nesting",
						fmt::format( "nesting depth {} exceeds limit of {}", innerDepth, this->ruleConfig_.maximumNestingDepth ),
						lineNumber, columnNumber
					);
				}
				this->walkBodyForNesting( whileNode.body, innerDepth );
			}
			else if( statement->kind == ast::Node::Kind::ForStatement ) {
				ast::nodes::ForStatement& forNode = static_cast<ast::nodes::ForStatement&>( *statement );
				uint32_t lineNumber = forNode.source != nullptr ? forNode.source->location->line : 0;
				uint32_t columnNumber = forNode.source != nullptr ? forNode.source->location->column : 0;
				uint32_t innerDepth = currentDepth + 1;
				if( this->ruleConfig_.enableDeepNesting && innerDepth > this->ruleConfig_.maximumNestingDepth ) {
					this->emitDiagnostic(
						LintSeverity::Warning,
						"complexity/deep-nesting",
						fmt::format( "nesting depth {} exceeds limit of {}", innerDepth, this->ruleConfig_.maximumNestingDepth ),
						lineNumber, columnNumber
					);
				}
				this->walkBodyForNesting( forNode.body, innerDepth );
			}
			else if( statement->kind == ast::Node::Kind::TryCatchStatement ) {
				ast::nodes::TryCatchStatement& tryNode = static_cast<ast::nodes::TryCatchStatement&>( *statement );
				uint32_t innerDepth = currentDepth + 1;
				this->walkBodyForNesting( tryNode.tryBody, innerDepth );
				for( const ast::nodes::ExceptionClause& exceptionClause : tryNode.exceptionClauses ) {
					this->walkBodyForNesting( exceptionClause.body, innerDepth );
				}
				this->walkBodyForNesting( tryNode.finallyBody, innerDepth );
			}
		}
	}
	
	void Linter::visit( ast::nodes::FunctionDeclaration& node ) {
		uint32_t lineNumber = node.source != nullptr ? node.source->location->line : 0;
		uint32_t columnNumber = node.source != nullptr ? node.source->location->column : 0;
		bool isPublic = node.access == ast::AccessModifier::Public;
		bool hasBody = node.body.empty() == false;
		if( this->ruleConfig_.enableCrypticParameter ) {
			for( const ast::nodes::FunctionParameterSharedPointer& parameter : node.parameters ) {
				uint32_t paramLine = parameter->source != nullptr ? parameter->source->location->line : lineNumber;
				uint32_t paramColumn = parameter->source != nullptr ? parameter->source->location->column : columnNumber;
				this->checkParameterName( parameter->name, paramLine, paramColumn );
			}
		}
		if( isPublic ) {
			this->checkDoccomment( "function", node.name, lineNumber, columnNumber, hasBody );
		}
		if( this->ruleConfig_.enableMissingReturnType && node.returnType == nullptr && node.isAbstract == false ) {
			this->emitDiagnostic(
				LintSeverity::Warning,
				"style/missing-return-type",
				fmt::format( "function \"{}\" has no explicit return type", node.name ),
				lineNumber, columnNumber
			);
		}
		if( hasBody && node.body.size() > 0 ) {
			ast::nodes::StatementSharedPointer lastStatement = node.body.back();
			uint32_t endLine = lastStatement != nullptr && lastStatement->source != nullptr ? lastStatement->source->location->line : lineNumber;
			this->checkFunctionLength( node.name, lineNumber, endLine );
		}
		if( hasBody ) {
			this->walkBodyForNesting( node.body, 1 );
		}
		for( const ast::nodes::DeclarationSharedPointer& nestedDecl : node.nestedFunctions ) {
			visitors::dispatchVisit( *this, *nestedDecl );
		}
	}
	
	void Linter::visit( ast::nodes::ClassDeclaration& node ) {
		uint32_t lineNumber = node.source != nullptr ? node.source->location->line : 0;
		uint32_t columnNumber = node.source != nullptr ? node.source->location->column : 0;
		bool isPublic = node.access == ast::AccessModifier::Public;
		if( isPublic ) {
			this->checkDoccomment( "class", node.name, lineNumber, columnNumber, false );
		}
		if( this->ruleConfig_.enableCrypticVariable ) {
			for( const ast::nodes::FieldDeclarationSharedPointer& field : node.fields ) {
				uint32_t fieldLine = field->source != nullptr ? field->source->location->line : lineNumber;
				uint32_t fieldColumn = field->source != nullptr ? field->source->location->column : columnNumber;
				this->checkVariableName( field->name, fieldLine, fieldColumn );
			}
		}
		for( const ast::nodes::DeclarationSharedPointer& method : node.methods ) {
			visitors::dispatchVisit( *this, *method );
		}
		for( const ast::nodes::DeclarationSharedPointer& nested : node.nestedDeclarations ) {
			visitors::dispatchVisit( *this, *nested );
		}
	}
	
	void Linter::visit( ast::nodes::InterfaceDeclaration& node ) {
		uint32_t lineNumber = node.source != nullptr ? node.source->location->line : 0;
		uint32_t columnNumber = node.source != nullptr ? node.source->location->column : 0;
		bool isPublic = node.access == ast::AccessModifier::Public;
		if( isPublic ) {
			this->checkDoccomment( "interface", node.name, lineNumber, columnNumber, false );
		}
		for( const ast::nodes::DeclarationSharedPointer& method : node.methods ) {
			visitors::dispatchVisit( *this, *method );
		}
	}
	
	void Linter::visit( ast::nodes::StructDeclaration& node ) {
		uint32_t lineNumber = node.source != nullptr ? node.source->location->line : 0;
		uint32_t columnNumber = node.source != nullptr ? node.source->location->column : 0;
		bool isPublic = node.access == ast::AccessModifier::Public;
		if( isPublic ) {
			this->checkDoccomment( "struct", node.name, lineNumber, columnNumber, false );
		}
		if( this->ruleConfig_.enableCrypticVariable ) {
			for( const ast::nodes::FieldDeclarationSharedPointer& field : node.fields ) {
				uint32_t fieldLine = field->source != nullptr ? field->source->location->line : 0;
				uint32_t fieldColumn = field->source != nullptr ? field->source->location->column : 0;
				this->checkVariableName( field->name, fieldLine, fieldColumn );
			}
		}
	}
	
	void Linter::visit( ast::nodes::EnumDeclaration& node ) {
		uint32_t lineNumber = node.source != nullptr ? node.source->location->line : 0;
		uint32_t columnNumber = node.source != nullptr ? node.source->location->column : 0;
		bool isPublic = node.access == ast::AccessModifier::Public;
		if( isPublic ) {
			this->checkDoccomment( "enum", node.name, lineNumber, columnNumber, false );
		}
	}
	
	void Linter::visit( ast::nodes::VariableStatement& node ) {
		uint32_t lineNumber = node.source != nullptr ? node.source->location->line : 0;
		uint32_t columnNumber = node.source != nullptr ? node.source->location->column : 0;
		this->checkVariableName( node.name, lineNumber, columnNumber );
	}
	
	void Linter::visit( ast::nodes::ImportDeclaration& node ) {}
	void Linter::visit( ast::nodes::IfStatement& node ) {}
	void Linter::visit( ast::nodes::WhileStatement& node ) {}
	void Linter::visit( ast::nodes::ForStatement& node ) {}
	void Linter::visit( ast::nodes::TryCatchStatement& node ) {}
	void Linter::visit( ast::nodes::MatchStatement& node ) {}

}
