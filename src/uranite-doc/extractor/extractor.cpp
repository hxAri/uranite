
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

#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <sstream>

#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/lexer/lexer.hpp"
#include "uranite/parser/parser.hpp"
#include "uranite-doc/extractor/extractor.hpp"

namespace uranite::doc {
	
	static std::string readFileContents( const std::string& filePath ) {
		std::ifstream fileStream( filePath );
		if( fileStream.is_open() == false ) {
			return "";
		}
		std::ostringstream contentStream;
		contentStream << fileStream.rdbuf();
		return contentStream.str();
	}
	
	static void collectUraniteFiles( const std::string& directoryPath, std::vector<std::string>& collectedFiles ) {
		for( const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator( directoryPath ) ) {
			if( entry.is_regular_file() && entry.path().extension() == ".urn" ) {
				collectedFiles.push_back( entry.path().string() );
			}
		}
	}
	
	ModuleDocumentation DocumentationExtractor::extractFromFile( const std::string& sourceFilePath ) {
		std::string sourceContent = readFileContents( sourceFilePath );
		if( sourceContent.empty() ) {
			ModuleDocumentation emptyModule;
			emptyModule.sourceFilePath = sourceFilePath;
			return emptyModule;
		}
		formatter::comments::CommentExtractor commentExtractor( sourceContent, sourceFilePath );
		std::vector<formatter::comments::CommentEntry> extractedComments = commentExtractor.extract();
		diagnostic::Engine diagnosticEngine( 0, -1 );
		lexer::Lexer tokenizer( sourceContent, sourceFilePath, diagnosticEngine );
		std::vector<token::Token> tokenStream = tokenizer.tokenize();
		if( diagnosticEngine.hasErrors() ) {
			ModuleDocumentation errorModule;
			errorModule.sourceFilePath = sourceFilePath;
			return errorModule;
		}
		parser::Parser syntaxParser( tokenStream, diagnosticEngine );
		ast::nodes::ProgramSharedPointer program = syntaxParser.parse();
		if( diagnosticEngine.hasErrors() || program == nullptr ) {
			ModuleDocumentation errorModule;
			errorModule.sourceFilePath = sourceFilePath;
			return errorModule;
		}
		return this->extractFromProgram( *program, extractedComments, sourceFilePath );
	}
	
	DocumentationSite DocumentationExtractor::extractFromDirectory( const std::string& directoryPath ) {
		DocumentationSite site;
		site.rootPackage.segmentName = "uranite";
		site.inputBaseDirectory = directoryPath;
		site.projectMetadata = this->readProjectMetadata( directoryPath );
		std::vector<std::string> sourceFiles;
		collectUraniteFiles( directoryPath, sourceFiles );
		for( const std::string& sourceFile : sourceFiles ) {
			ModuleDocumentation moduleDoc = this->extractFromFile( sourceFile );
			if( moduleDoc.packageName.empty() ) {
				continue;
			}
			site.totalModules++;
			for( const EntityDocumentationEntry& entity : moduleDoc.exportedEntities ) {
				site.totalEntities++;
				site.totalMethods+= entity.methods.size();
			}
			this->insertModuleIntoTree( site.rootPackage, moduleDoc );
		}
		return site;
	}
	
	ModuleDocumentation DocumentationExtractor::extractFromProgram(
		ast::nodes::Program& program,
		const std::vector<formatter::comments::CommentEntry>& extractedComments,
		const std::string& sourceFilePath
	) {
		ModuleDocumentation moduleDoc;
		moduleDoc.sourceFilePath = sourceFilePath;
		moduleDoc.packageName = program.module != nullptr ? program.module->name : "";
		for( const ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration->kind == ast::Node::Kind::ImportDeclaration ) {
				ast::nodes::ImportDeclaration& importDecl = static_cast<ast::nodes::ImportDeclaration&>( *declaration );
				std::string importPath;
				for( size_t segmentIndex = 0; segmentIndex < importDecl.path.size(); segmentIndex++ ) {
					if( segmentIndex > 0 ) {
						importPath+= ".";
					}
					importPath+= importDecl.path[segmentIndex];
				}
				moduleDoc.importedModules.push_back( importPath );
				continue;
			}
			ast::nodes::DeclarationSharedPointer targetDecl = declaration;
			if( declaration->kind == ast::Node::Kind::ExportDeclaration ) {
				ast::nodes::ExportDeclaration& exportDecl = static_cast<ast::nodes::ExportDeclaration&>( *declaration );
				if( exportDecl.declaration != nullptr ) {
					targetDecl = exportDecl.declaration;
				}
				else {
					continue;
				}
			}
			if( targetDecl->kind == ast::Node::Kind::ClassDeclaration ) {
				ast::nodes::ClassDeclaration& classDecl = static_cast<ast::nodes::ClassDeclaration&>( *targetDecl );
				EntityDocumentationEntry entity = this->extractClassEntity( classDecl, extractedComments, moduleDoc.packageName );
				moduleDoc.exportedEntities.push_back( entity );
			}
			else if( targetDecl->kind == ast::Node::Kind::InterfaceDeclaration ) {
				ast::nodes::InterfaceDeclaration& interfaceDecl = static_cast<ast::nodes::InterfaceDeclaration&>( *targetDecl );
				EntityDocumentationEntry entity = this->extractInterfaceEntity( interfaceDecl, extractedComments, moduleDoc.packageName );
				moduleDoc.exportedEntities.push_back( entity );
			}
			else if( targetDecl->kind == ast::Node::Kind::StructDeclaration ) {
				ast::nodes::StructDeclaration& structDecl = static_cast<ast::nodes::StructDeclaration&>( *targetDecl );
				EntityDocumentationEntry entity = this->extractStructEntity( structDecl, extractedComments, moduleDoc.packageName );
				moduleDoc.exportedEntities.push_back( entity );
			}
			else if( targetDecl->kind == ast::Node::Kind::EnumDeclaration ) {
				ast::nodes::EnumDeclaration& enumDecl = static_cast<ast::nodes::EnumDeclaration&>( *targetDecl );
				EntityDocumentationEntry entity = this->extractEnumEntity( enumDecl, extractedComments, moduleDoc.packageName );
				moduleDoc.exportedEntities.push_back( entity );
			}
			else if( targetDecl->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& funcDecl = static_cast<ast::nodes::FunctionDeclaration&>( *targetDecl );
				EntityDocumentationEntry entity = this->extractFunctionEntity( funcDecl, extractedComments, moduleDoc.packageName );
				moduleDoc.exportedEntities.push_back( entity );
			}
			else if( targetDecl->kind == ast::Node::Kind::ConstantDeclaration ) {
				ast::nodes::ConstantDeclaration& constDecl = static_cast<ast::nodes::ConstantDeclaration&>( *targetDecl );
				EntityDocumentationEntry entity;
				entity.entityKind = EntityKind::Constant;
				entity.entityName = constDecl.name;
				entity.accessModifier = this->accessModifierToString( constDecl.access );
				entity.declarationLine = constDecl.source != nullptr ? constDecl.source->location->line : 0;
				entity.entityDoccomment = this->findDoccommentForLine( entity.declarationLine, extractedComments );
				moduleDoc.exportedEntities.push_back( entity );
			}
		}
		return moduleDoc;
	}
	
	EntityDocumentationEntry DocumentationExtractor::extractClassEntity(
		ast::nodes::ClassDeclaration& classDecl,
		const std::vector<formatter::comments::CommentEntry>& extractedComments,
		const std::string& packageName
	) {
		EntityDocumentationEntry entity;
		entity.entityKind = EntityKind::Class;
		entity.entityName = classDecl.name;
		entity.qualifiedName = packageName + "." + classDecl.name;
		entity.accessModifier = this->accessModifierToString( classDecl.access );
		entity.declarationLine = classDecl.source != nullptr ? classDecl.source->location->line : 0;
		if( classDecl.baseClassType != nullptr ) {
			entity.parentClassName = this->typeNodeToString( classDecl.baseClassType );
		}
		for( const ast::nodes::TypeNodeSharedPointer& interfaceType : classDecl.interfaces ) {
			entity.implementedInterfaces.push_back( this->typeNodeToString( interfaceType ) );
		}
		for( const ast::nodes::GenericParameterSharedPointer& genericParam : classDecl.genericParameters ) {
			entity.genericParameters.push_back( genericParam->name );
		}
		entity.entityDoccomment = this->findDoccommentForLine( entity.declarationLine, extractedComments );
		for( const ast::nodes::FieldDeclarationSharedPointer& field : classDecl.fields ) {
			entity.fields.push_back( this->extractField( *field, extractedComments ) );
		}
		for( const ast::nodes::DeclarationSharedPointer& method : classDecl.methods ) {
			if( method->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& methodDecl = static_cast<ast::nodes::FunctionDeclaration&>( *method );
				entity.methods.push_back( this->extractMethod( methodDecl, extractedComments ) );
			}
		}
		for( const ast::nodes::DeclarationSharedPointer& nested : classDecl.nestedDeclarations ) {
			if( nested->kind == ast::Node::Kind::ClassDeclaration ) {
				ast::nodes::ClassDeclaration& nestedClass = static_cast<ast::nodes::ClassDeclaration&>( *nested );
				entity.nestedEntities.push_back( this->extractClassEntity( nestedClass, extractedComments, entity.qualifiedName ) );
			}
			else if( nested->kind == ast::Node::Kind::EnumDeclaration ) {
				ast::nodes::EnumDeclaration& nestedEnum = static_cast<ast::nodes::EnumDeclaration&>( *nested );
				entity.nestedEntities.push_back( this->extractEnumEntity( nestedEnum, extractedComments, entity.qualifiedName ) );
			}
		}
		return entity;
	}
	
	EntityDocumentationEntry DocumentationExtractor::extractInterfaceEntity(
		ast::nodes::InterfaceDeclaration& interfaceDecl,
		const std::vector<formatter::comments::CommentEntry>& extractedComments,
		const std::string& packageName
	) {
		EntityDocumentationEntry entity;
		entity.entityKind = EntityKind::Interface;
		entity.entityName = interfaceDecl.name;
		entity.qualifiedName = packageName + "." + interfaceDecl.name;
		entity.accessModifier = this->accessModifierToString( interfaceDecl.access );
		entity.declarationLine = interfaceDecl.source != nullptr ? interfaceDecl.source->location->line : 0;
		for( const ast::nodes::GenericParameterSharedPointer& genericParam : interfaceDecl.genericParameters ) {
			entity.genericParameters.push_back( genericParam->name );
		}
		for( const ast::nodes::TypeNodeSharedPointer& superInterface : interfaceDecl.superInterfaces ) {
			entity.implementedInterfaces.push_back( this->typeNodeToString( superInterface ) );
		}
		entity.entityDoccomment = this->findDoccommentForLine( entity.declarationLine, extractedComments );
		for( const ast::nodes::DeclarationSharedPointer& method : interfaceDecl.methods ) {
			if( method->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& methodDecl = static_cast<ast::nodes::FunctionDeclaration&>( *method );
				entity.methods.push_back( this->extractMethod( methodDecl, extractedComments ) );
			}
		}
		return entity;
	}
	
	EntityDocumentationEntry DocumentationExtractor::extractStructEntity(
		ast::nodes::StructDeclaration& structDecl,
		const std::vector<formatter::comments::CommentEntry>& extractedComments,
		const std::string& packageName
	) {
		EntityDocumentationEntry entity;
		entity.entityKind = EntityKind::Struct;
		entity.entityName = structDecl.name;
		entity.qualifiedName = packageName + "." + structDecl.name;
		entity.accessModifier = this->accessModifierToString( structDecl.access );
		entity.declarationLine = structDecl.source != nullptr ? structDecl.source->location->line : 0;
		for( const ast::nodes::GenericParameterSharedPointer& genericParam : structDecl.genericParameters ) {
			entity.genericParameters.push_back( genericParam->name );
		}
		entity.entityDoccomment = this->findDoccommentForLine( entity.declarationLine, extractedComments );
		for( const ast::nodes::FieldDeclarationSharedPointer& field : structDecl.fields ) {
			entity.fields.push_back( this->extractField( *field, extractedComments ) );
		}
		for( const ast::nodes::DeclarationSharedPointer& method : structDecl.methods ) {
			if( method->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& methodDecl = static_cast<ast::nodes::FunctionDeclaration&>( *method );
				entity.methods.push_back( this->extractMethod( methodDecl, extractedComments ) );
			}
		}
		return entity;
	}
	
	EntityDocumentationEntry DocumentationExtractor::extractEnumEntity(
		ast::nodes::EnumDeclaration& enumDecl,
		const std::vector<formatter::comments::CommentEntry>& extractedComments,
		const std::string& packageName
	) {
		EntityDocumentationEntry entity;
		entity.entityKind = EntityKind::Enum;
		entity.entityName = enumDecl.name;
		entity.qualifiedName = packageName + "." + enumDecl.name;
		entity.accessModifier = this->accessModifierToString( enumDecl.access );
		entity.declarationLine = enumDecl.source != nullptr ? enumDecl.source->location->line : 0;
		entity.entityDoccomment = this->findDoccommentForLine( entity.declarationLine, extractedComments );
		for( const ast::nodes::DeclarationSharedPointer& method : enumDecl.methods ) {
			if( method->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& methodDecl = static_cast<ast::nodes::FunctionDeclaration&>( *method );
				entity.methods.push_back( this->extractMethod( methodDecl, extractedComments ) );
			}
		}
		return entity;
	}
	
	EntityDocumentationEntry DocumentationExtractor::extractFunctionEntity(
		ast::nodes::FunctionDeclaration& funcDecl,
		const std::vector<formatter::comments::CommentEntry>& extractedComments,
		const std::string& packageName
	) {
		EntityDocumentationEntry entity;
		entity.entityKind = funcDecl.isProperty ? EntityKind::Property : EntityKind::Function;
		entity.entityName = funcDecl.name;
		entity.qualifiedName = packageName + "." + funcDecl.name;
		entity.accessModifier = this->accessModifierToString( funcDecl.access );
		entity.declarationLine = funcDecl.source != nullptr ? funcDecl.source->location->line : 0;
		entity.entityDoccomment = this->findDoccommentForLine( entity.declarationLine, extractedComments );
		MethodDocumentationEntry selfMethod = this->extractMethod( funcDecl, extractedComments );
		entity.methods.push_back( selfMethod );
		return entity;
	}
	
	MethodDocumentationEntry DocumentationExtractor::extractMethod(
		ast::nodes::FunctionDeclaration& methodDecl,
		const std::vector<formatter::comments::CommentEntry>& extractedComments
	) {
		MethodDocumentationEntry method;
		method.methodName = methodDecl.name;
		method.accessModifier = this->accessModifierToString( methodDecl.access );
		method.isAbstract = methodDecl.isAbstract;
		method.isAsync = methodDecl.isAsync;
		method.isStatic = methodDecl.isStatic;
		method.isVirtual = methodDecl.isVirtual;
		method.isProperty = methodDecl.isProperty;
		method.signatureText = this->buildSignatureText( methodDecl );
		uint32_t declarationLine = methodDecl.source != nullptr ? methodDecl.source->location->line : 0;
		method.doccomment = this->findDoccommentForLine( declarationLine, extractedComments );
		return method;
	}
	
	FieldDocumentationEntry DocumentationExtractor::extractField(
		ast::nodes::FieldDeclarationNode& fieldDecl,
		const std::vector<formatter::comments::CommentEntry>& extractedComments
	) {
		FieldDocumentationEntry field;
		field.fieldName = fieldDecl.name;
		field.fieldType = this->typeNodeToString( fieldDecl.type );
		field.accessModifier = fieldDecl.access == ast::AccessModifier::Public ? "public"
			: fieldDecl.access == ast::AccessModifier::Protect ? "protect"
			: fieldDecl.access == ast::AccessModifier::Private ? "private"
			: "";
		uint32_t declarationLine = fieldDecl.source != nullptr ? fieldDecl.source->location->line : 0;
		field.doccomment = this->findDoccommentForLine( declarationLine, extractedComments );
		return field;
	}
	
	ParsedDoccomment DocumentationExtractor::findDoccommentForLine(
		uint32_t declarationLine,
		const std::vector<formatter::comments::CommentEntry>& extractedComments
	) {
		for( const formatter::comments::CommentEntry& comment : extractedComments ) {
			if( comment.commentKind == formatter::comments::CommentKind::DocComment ) {
				int32_t delta = static_cast<int32_t>( comment.startLine ) - static_cast<int32_t>( declarationLine );
				if( delta >= 1 && delta <= 3 ) {
					return DoccommentParser::parse( comment.commentContent );
				}
			}
		}
		return ParsedDoccomment();
	}
	
	std::string DocumentationExtractor::buildSignatureText( ast::nodes::FunctionDeclaration& funcDecl ) {
		std::string signature;
		if( funcDecl.isAsync ) {
			signature+= "async ";
		}
		if( funcDecl.isProperty ) {
			signature+= "property ";
		}
		else {
			signature+= "function ";
		}
		signature+= funcDecl.name + "( ";
		for( size_t paramIndex = 0; paramIndex < funcDecl.parameters.size(); paramIndex++ ) {
			if( paramIndex > 0 ) {
				signature+= ", ";
			}
			ast::nodes::FunctionParameterNode& parameter = *funcDecl.parameters[paramIndex];
			if( parameter.name == "self" ) {
				signature+= "self";
			}
			else {
				signature+= this->typeNodeToString( parameter.type ) + " " + parameter.name;
				if( parameter.isVariadic ) {
					signature+= "[]";
				}
				if( parameter.isKeyword ) {
					signature+= "{}";
				}
			}
		}
		signature+= " )";
		if( funcDecl.returnType != nullptr ) {
			signature+= " -> " + this->typeNodeToString( funcDecl.returnType );
		}
		return signature;
	}
	
	std::string DocumentationExtractor::typeNodeToString( const ast::nodes::TypeNodeSharedPointer& typeNode ) {
		if( typeNode == nullptr ) {
			return "<unknown>";
		}
		switch( typeNode->kind ) {
			case ast::Node::Kind::SimpleType: {
				ast::nodes::SimpleTypeNode& simpleType = static_cast<ast::nodes::SimpleTypeNode&>( *typeNode );
				return simpleType.name;
			}
			case ast::Node::Kind::GenericType: {
				ast::nodes::GenericTypeNode& genericType = static_cast<ast::nodes::GenericTypeNode&>( *typeNode );
				std::string result = genericType.name + "<";
				for( size_t typeArgIndex = 0; typeArgIndex < genericType.typeArguments.size(); typeArgIndex++ ) {
					if( typeArgIndex > 0 ) {
						result+= ", ";
					}
					result+= this->typeNodeToString( genericType.typeArguments[typeArgIndex] );
				}
				result+= ">";
				return result;
			}
			case ast::Node::Kind::OptionalType: {
				ast::nodes::OptionalTypeNode& optionalType = static_cast<ast::nodes::OptionalTypeNode&>( *typeNode );
				return "?" + this->typeNodeToString( optionalType.innerType );
			}
			case ast::Node::Kind::ArrayType: {
				ast::nodes::ArrayTypeNode& arrayType = static_cast<ast::nodes::ArrayTypeNode&>( *typeNode );
				return "[" + this->typeNodeToString( arrayType.elementType ) + "]";
			}
			case ast::Node::Kind::ReferenceType: {
				ast::nodes::ReferenceTypeNode& refType = static_cast<ast::nodes::ReferenceTypeNode&>( *typeNode );
				return "&" + this->typeNodeToString( refType.innerType );
			}
			case ast::Node::Kind::PointerType: {
				ast::nodes::PointerTypeNode& ptrType = static_cast<ast::nodes::PointerTypeNode&>( *typeNode );
				return "*" + this->typeNodeToString( ptrType.innerType );
			}
			default:
				return "<type>";
		}
	}
	
	std::string DocumentationExtractor::accessModifierToString( ast::AccessModifier accessModifier ) {
		switch( accessModifier ) {
			case ast::AccessModifier::Public: return "public";
			case ast::AccessModifier::Protect: return "protect";
			case ast::AccessModifier::Private: return "private";
			default: return "";
		}
	}
	
	ProjectMetadata DocumentationExtractor::readProjectMetadata( const std::string& projectRootPath ) const {
		ProjectMetadata metadata;
		std::vector<std::string> searchPaths = {
			projectRootPath + "/README.md",
			projectRootPath + "/../README.md"
		};
		std::string readmeContent;
		for( const std::string& candidatePath : searchPaths ) {
			if( std::filesystem::exists( candidatePath ) ) {
				readmeContent = readFileContents( candidatePath );
				if( readmeContent.empty() == false ) {
					metadata.readmeFound = true;
					break;
				}
			}
		}
		if( metadata.readmeFound ) {
			std::istringstream readmeStream( readmeContent );
			std::string currentLine;
			bool foundHeading = false;
			std::string bodyAccumulator;
			while( std::getline( readmeStream, currentLine ) ) {
				if( foundHeading == false && currentLine.size() > 2 && currentLine[0] == '#' && currentLine[1] == ' ' ) {
					metadata.projectName = currentLine.substr( 2 );
					foundHeading = true;
					continue;
				}
				if( foundHeading ) {
					if( currentLine.empty() && metadata.projectDescription.empty() && bodyAccumulator.empty() ) {
						continue;
					}
					if( currentLine.size() > 0 && currentLine[0] == '#' ) {
						break;
					}
					if( metadata.projectDescription.empty() && currentLine.empty() == false ) {
						metadata.projectDescription = currentLine;
					}
					if( bodyAccumulator.size() < 500 ) {
						if( bodyAccumulator.empty() == false ) {
							bodyAccumulator+= "\n";
						}
						bodyAccumulator+= currentLine;
					}
				}
			}
			if( bodyAccumulator.size() > 500 ) {
				bodyAccumulator = bodyAccumulator.substr( 0, 500 );
			}
			metadata.readmeExcerpt = bodyAccumulator;
		}
		std::vector<std::string> licensePaths = {
			projectRootPath + "/LICENSE",
			projectRootPath + "/../LICENSE",
			projectRootPath + "/LICENSE.md",
			projectRootPath + "/../LICENSE.md"
		};
		for( const std::string& licensePath : licensePaths ) {
			if( std::filesystem::exists( licensePath ) ) {
				std::string licenseContent = readFileContents( licensePath );
				if( licenseContent.empty() == false ) {
					size_t firstNewlinePosition = licenseContent.find( '\n' );
					if( firstNewlinePosition != std::string::npos ) {
						metadata.licenseIdentifier = licenseContent.substr( 0, firstNewlinePosition );
					}
					else {
						metadata.licenseIdentifier = licenseContent;
					}
					break;
				}
			}
		}
		return metadata;
	}
	
	void DocumentationExtractor::insertModuleIntoTree( PackageTreeNode& rootNode, const ModuleDocumentation& moduleDoc ) {
		std::string packageName = moduleDoc.packageName;
		std::vector<std::string> segments;
		std::string currentSegment;
		for( char character : packageName ) {
			if( character == '.' ) {
				if( currentSegment.empty() == false ) {
					segments.push_back( currentSegment );
					currentSegment.clear();
				}
			}
			else {
				currentSegment+= character;
			}
		}
		if( currentSegment.empty() == false ) {
			segments.push_back( currentSegment );
		}
		if( segments.empty() ) {
			rootNode.modules.push_back( moduleDoc );
			return;
		}
		size_t startSegment = 0;
		if( segments[0] == "uranite" ) {
			startSegment = 1;
		}
		PackageTreeNode* currentNode = &rootNode;
		for( size_t segmentIndex = startSegment; segmentIndex < segments.size() - 1; segmentIndex++ ) {
			bool found = false;
			for( PackageTreeNode& child : currentNode->childPackages ) {
				if( child.segmentName == segments[segmentIndex] ) {
					currentNode = &child;
					found = true;
					break;
				}
			}
			if( found == false ) {
				PackageTreeNode newChild;
				newChild.segmentName = segments[segmentIndex];
				currentNode->childPackages.push_back( newChild );
				currentNode = &currentNode->childPackages.back();
			}
		}
		currentNode->modules.push_back( moduleDoc );
	}

} // namespace uranite::doc
