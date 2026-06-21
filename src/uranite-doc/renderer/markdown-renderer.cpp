
//
// @author hxAri (hxari)
// @create 2026-06-15 11:30
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

#include "uranite-doc/renderer/markdown-renderer.hpp"
#include "uranite-doc/renderer/table-of-contents-builder.hpp"

namespace uranite::doc {
	
	void MarkdownRenderer::render( const DocumentationSite& site, const std::string& outputDirectory ) {
		std::filesystem::create_directories( outputDirectory );
		this->buildInheritanceMap( site );
		std::string projectHeading;
		if( site.projectMetadata.projectName.empty() == false ) {
			projectHeading = site.projectMetadata.projectName;
		}
		else {
			projectHeading = "Uranite API Documentation";
		}
		std::string rootIndex = fmt::format( "# {}\n\n", projectHeading );
		if( site.projectMetadata.readmeFound && site.projectMetadata.readmeExcerpt.empty() == false ) {
			rootIndex+= site.projectMetadata.readmeExcerpt + "\n\n";
		}
		rootIndex+= "## Overview\n\n";
		rootIndex+= fmt::format( "- **Modules**: {}\n", site.totalModules );
		rootIndex+= fmt::format( "- **Entities**: {}\n", site.totalEntities );
		rootIndex+= fmt::format( "- **Methods**: {}\n", site.totalMethods );
		if( site.projectMetadata.licenseIdentifier.empty() == false ) {
			rootIndex+= fmt::format( "- **License**: {}\n", site.projectMetadata.licenseIdentifier );
		}
		rootIndex+= "\n---\n\n";
		TableOfContentsBuilder tocBuilder;
		tocBuilder.buildFromSite( site );
		rootIndex+= tocBuilder.renderMarkdownToc( site.inputBaseDirectory );
		rootIndex+= "---\n\n## Packages\n\n";
		rootIndex+= this->renderPackageIndex( site.rootPackage, site.inputBaseDirectory );
		std::string indexPath = outputDirectory + "/index.md";
		std::ofstream indexFile( indexPath );
		indexFile << rootIndex;
		this->renderPackageTree( site.rootPackage, outputDirectory, site.inputBaseDirectory );
	}
	
	void MarkdownRenderer::renderModule( const ModuleDocumentation& moduleDoc, const std::string& outputDirectory ) {
		this->renderModuleWithBase( moduleDoc, outputDirectory, "" );
	}
	
	std::string MarkdownRenderer::buildModuleTableOfContents( const ModuleDocumentation& moduleDoc ) const {
		if( moduleDoc.exportedEntities.empty() && moduleDoc.importedModules.empty() ) {
			return "";
		}
		std::string toc = "## Table of Contents\n\n";
		if( moduleDoc.importedModules.empty() == false ) {
			toc+= "- [Imports](#imports)\n";
		}
		for( const EntityDocumentationEntry& entity : moduleDoc.exportedEntities ) {
			std::string kindLabel = this->entityKindToString( entity.entityKind );
			std::string anchorName = entity.entityName;
			for( size_t charPosition = 0; charPosition < anchorName.size(); charPosition++ ) {
				char character = anchorName[charPosition];
				if( character >= 'A' && character <= 'Z' ) {
					anchorName[charPosition] = character + 32;
				}
				else if( character == ' ' || character == '_' ) {
					anchorName[charPosition] = '-';
				}
			}
			toc+= fmt::format( "- [{} `{}`](#{}-{})\n", kindLabel, entity.entityName, kindLabel, anchorName );
			for( const MethodDocumentationEntry& method : entity.methods ) {
				if( method.accessModifier != "public" ) {
					continue;
				}
				toc+= fmt::format( "  - [`{}()`](#{})\n", method.methodName, method.methodName );
			}
		}
		toc+= "\n";
		return toc;
	}
	
	void MarkdownRenderer::renderModuleWithBase( const ModuleDocumentation& moduleDoc, const std::string& outputDirectory, const std::string& inputBaseDirectory ) {
		std::string content = fmt::format( "# {}\n\n", moduleDoc.packageName );
		content+= this->buildModuleTableOfContents( moduleDoc );
		if( moduleDoc.importedModules.empty() == false ) {
			content+= "## Imports\n\n";
			for( const std::string& importPath : moduleDoc.importedModules ) {
				content+= fmt::format( "- `{}`\n", importPath );
			}
			content+= "\n";
		}
		for( const EntityDocumentationEntry& entity : moduleDoc.exportedEntities ) {
			content+= this->renderEntity( entity, 2 );
		}
		std::string relativePath;
		if( inputBaseDirectory.empty() == false ) {
			relativePath = this->computeOutputRelativePath( moduleDoc, inputBaseDirectory );
		}
		else {
			std::string packageName = moduleDoc.packageName;
			size_t lastDotPosition = packageName.rfind( '.' );
			if( lastDotPosition != std::string::npos ) {
				relativePath = packageName.substr( lastDotPosition + 1 );
			}
			else {
				relativePath = packageName.empty() ? "unknown" : packageName;
			}
		}
		std::string filePath = outputDirectory + "/" + relativePath + ".md";
		std::filesystem::path parentDirectory = std::filesystem::path( filePath ).parent_path();
		if( parentDirectory.empty() == false ) {
			std::filesystem::create_directories( parentDirectory );
		}
		std::ofstream outputFile( filePath );
		outputFile << content;
	}
	
	std::string MarkdownRenderer::renderEntity( const EntityDocumentationEntry& entity, uint32_t headingLevel ) {
		std::string headingPrefix( headingLevel, '#' );
		std::string kindLabel = this->entityKindToString( entity.entityKind );
		std::string content = fmt::format( "{} {} `{}`", headingPrefix, kindLabel, entity.entityName );
		if( entity.genericParameters.empty() == false ) {
			content+= "<";
			for( size_t paramIndex = 0; paramIndex < entity.genericParameters.size(); paramIndex++ ) {
				if( paramIndex > 0 ) {
					content+= ", ";
				}
				content+= entity.genericParameters[paramIndex];
			}
			content+= ">";
		}
		content+= "\n\n";
		if( entity.parentClassName.empty() == false ) {
			std::string inheritanceChain = this->renderInheritanceChain( entity.parentClassName );
			if( inheritanceChain.empty() == false ) {
				content+= fmt::format( "**Extends**: {}\n\n", inheritanceChain );
			}
			else {
				content+= fmt::format( "**Extends**: `{}`\n\n", entity.parentClassName );
			}
		}
		if( entity.implementedInterfaces.empty() == false ) {
			content+= "**Implements**: ";
			for( size_t interfaceIndex = 0; interfaceIndex < entity.implementedInterfaces.size(); interfaceIndex++ ) {
				if( interfaceIndex > 0 ) {
					content+= ", ";
				}
				content+= fmt::format( "`{}`", entity.implementedInterfaces[interfaceIndex] );
			}
			content+= "\n\n";
		}
		content+= this->renderDoccomment( entity.entityDoccomment );
		if( entity.fields.empty() == false ) {
			content+= fmt::format( "{} Fields\n\n", std::string( headingLevel + 1, '#' ) );
			content+= "| Name | Type | Access |\n";
			content+= "|------|------|--------|\n";
			for( const FieldDocumentationEntry& field : entity.fields ) {
				content+= fmt::format( "| `{}` | `{}` | {} |\n", field.fieldName, field.fieldType, field.accessModifier );
			}
			content+= "\n";
		}
		if( entity.methods.empty() == false ) {
			content+= fmt::format( "{} Methods\n\n", std::string( headingLevel + 1, '#' ) );
			for( const MethodDocumentationEntry& method : entity.methods ) {
				content+= this->renderMethod( method );
			}
		}
		for( const EntityDocumentationEntry& nested : entity.nestedEntities ) {
			content+= this->renderEntity( nested, headingLevel + 1 );
		}
		return content;
	}
	
	std::string MarkdownRenderer::renderMethod( const MethodDocumentationEntry& method ) {
		std::string content = fmt::format( "#### `{}`\n\n", method.signatureText );
		std::string modifiers;
		if( method.isAsync ) {
			modifiers+= "`async` ";
		}
		if( method.isStatic ) {
			modifiers+= "`static` ";
		}
		if( method.isVirtual ) {
			modifiers+= "`virtual` ";
		}
		if( method.isAbstract ) {
			modifiers+= "`abstract` ";
		}
		if( method.isProperty ) {
			modifiers+= "`property` ";
		}
		if( modifiers.empty() == false ) {
			content+= modifiers + "\n\n";
		}
		content+= this->renderDoccomment( method.doccomment );
		return content;
	}
	
	std::string MarkdownRenderer::renderField( const FieldDocumentationEntry& field ) {
		return fmt::format( "- `{} {}` ({})\n", field.fieldType, field.fieldName, field.accessModifier );
	}
	std::string MarkdownRenderer::renderDoccomment( const ParsedDoccomment& doccomment ) {
		std::string content;
		if( doccomment.summaryLine.empty() == false ) {
			content+= doccomment.summaryLine + "\n\n";
		}
		if( doccomment.extendedDescription.empty() == false ) {
			content+= doccomment.extendedDescription + "\n\n";
		}
		if( doccomment.parameters.empty() == false ) {
			content+= "**Parameters**:\n\n";
			for( const ParameterDocumentation& parameter : doccomment.parameters ) {
				content+= fmt::format( "- `{}`", parameter.parameterName );
				if( parameter.parameterType.empty() == false ) {
					content+= fmt::format( " (`{}`)", parameter.parameterType );
				}
				if( parameter.descriptionText.empty() == false ) {
					content+= fmt::format( " — {}", parameter.descriptionText );
				}
				content+= "\n";
			}
			content+= "\n";
		}
		if( doccomment.returns.has_value() ) {
			const ReturnsDocumentation& returnsInfo = doccomment.returns.value();
			content+= "**Returns**:";
			if( returnsInfo.returnType.empty() == false ) {
				content+= fmt::format( " `{}`", returnsInfo.returnType );
			}
			if( returnsInfo.descriptionText.empty() == false ) {
				content+= fmt::format( " — {}", returnsInfo.descriptionText );
			}
			content+= "\n\n";
		}
		if( doccomment.raises.empty() == false ) {
			content+= "**Raises**:\n\n";
			for( const RaisesDocumentation& raisesEntry : doccomment.raises ) {
				std::string inheritanceChain = this->renderInheritanceChain( raisesEntry.exceptionType );
				if( inheritanceChain.empty() == false ) {
					content+= fmt::format( "- {} — {}\n", inheritanceChain, raisesEntry.conditionDescription );
				}
				else {
					content+= fmt::format( "- `{}` — {}\n", raisesEntry.exceptionType, raisesEntry.conditionDescription );
				}
			}
			content+= "\n";
		}
		if( doccomment.complexity.has_value() ) {
			content+= this->renderComplexity( doccomment.complexity.value() );
		}
		return content;
	}
	
	std::string MarkdownRenderer::renderComplexity( const ComplexityDocumentation& complexity ) {
		std::string content = "**Complexity**:\n";
		if( complexity.timeComplexity.empty() == false ) {
			content+= fmt::format( "- Time: `{}`\n", complexity.timeComplexity );
		}
		if( complexity.spaceComplexity.empty() == false ) {
			content+= fmt::format( "- Space: `{}`\n", complexity.spaceComplexity );
		}
		content+= "\n";
		return content;
	}
	
	std::string MarkdownRenderer::renderPackageIndex( const PackageTreeNode& node, const std::string& inputBaseDirectory ) {
		std::string content;
		for( const ModuleDocumentation& moduleDoc : node.modules ) {
			std::string outputRelativePath = this->computeOutputRelativePath( moduleDoc, inputBaseDirectory );
			std::string linkPath = outputRelativePath + ".md";
			uint32_t entityCount = moduleDoc.exportedEntities.size();
			content+= fmt::format( "- [{}]({}) ({} entities)\n", moduleDoc.packageName, linkPath, entityCount );
		}
		for( const PackageTreeNode& child : node.childPackages ) {
			content+= fmt::format( "\n### {}\n\n", child.segmentName );
			content+= this->renderPackageIndex( child, inputBaseDirectory );
		}
		return content;
	}
	
	void MarkdownRenderer::renderPackageTree( const PackageTreeNode& node, const std::string& outputDirectory, const std::string& inputBaseDirectory ) {
		for( const ModuleDocumentation& moduleDoc : node.modules ) {
			this->renderModuleWithBase( moduleDoc, outputDirectory, inputBaseDirectory );
		}
		for( const PackageTreeNode& child : node.childPackages ) {
			this->renderPackageTree( child, outputDirectory, inputBaseDirectory );
		}
	}
	
	std::string MarkdownRenderer::entityKindToString( EntityKind entityKind ) const {
		switch( entityKind ) {
			case EntityKind::Class: return "class";
			case EntityKind::Interface: return "interface";
			case EntityKind::Struct: return "struct";
			case EntityKind::Enum: return "enum";
			case EntityKind::Function: return "function";
			case EntityKind::Property: return "property";
			case EntityKind::Constant: return "const";
		}
		return "";
	}
	
	std::string MarkdownRenderer::computeOutputRelativePath( const ModuleDocumentation& moduleDoc, const std::string& inputBaseDirectory ) const {
		std::string canonicalSourcePath = std::filesystem::weakly_canonical( moduleDoc.sourceFilePath ).string();
		std::string canonicalBaseDirectory = std::filesystem::weakly_canonical( inputBaseDirectory ).string();
		if( canonicalBaseDirectory.empty() == false && canonicalBaseDirectory.back() != '/' ) {
			canonicalBaseDirectory+= '/';
		}
		std::string relativePath;
		if( canonicalSourcePath.compare( 0, canonicalBaseDirectory.size(), canonicalBaseDirectory ) == 0 ) {
			relativePath = canonicalSourcePath.substr( canonicalBaseDirectory.size() );
		}
		else {
			size_t lastSlashPosition = canonicalSourcePath.rfind( '/' );
			if( lastSlashPosition != std::string::npos ) {
				relativePath = canonicalSourcePath.substr( lastSlashPosition + 1 );
			}
			else {
				relativePath = canonicalSourcePath;
			}
		}
		if( relativePath.size() >= 3 && relativePath.substr( relativePath.size() - 3 ) == ".urn" ) {
			relativePath = relativePath.substr( 0, relativePath.size() - 3 );
		}
		std::string stem = relativePath;
		size_t lastSlashInResult = stem.rfind( '/' );
		if( lastSlashInResult != std::string::npos ) {
			stem = stem.substr( lastSlashInResult + 1 );
		}
		if( stem == "__mod__" ) {
			if( lastSlashInResult != std::string::npos ) {
				relativePath = relativePath.substr( 0, lastSlashInResult ) + "/index";
			}
			else {
				relativePath = "index";
			}
		}
		return relativePath;
	}
	
	void MarkdownRenderer::buildInheritanceMap( const DocumentationSite& site ) {
		this->inheritanceMap_.clear();
		this->entityToModulePath_.clear();
		this->collectEntitiesFromTree( site.rootPackage );
	}
	
	void MarkdownRenderer::collectEntitiesFromTree( const PackageTreeNode& node ) {
		for( const ModuleDocumentation& moduleDoc : node.modules ) {
			for( const EntityDocumentationEntry& entity : moduleDoc.exportedEntities ) {
				if( entity.parentClassName.empty() == false ) {
					this->inheritanceMap_[entity.entityName] = entity.parentClassName;
				}
				this->entityToModulePath_[entity.entityName] = moduleDoc.packageName;
				for( const EntityDocumentationEntry& nested : entity.nestedEntities ) {
					if( nested.parentClassName.empty() == false ) {
						this->inheritanceMap_[nested.entityName] = nested.parentClassName;
					}
					this->entityToModulePath_[nested.entityName] = moduleDoc.packageName;
				}
			}
		}
		for( const PackageTreeNode& child : node.childPackages ) {
			this->collectEntitiesFromTree( child );
		}
	}
	
	std::string MarkdownRenderer::renderInheritanceChain( const std::string& className ) {
		std::vector<std::string> chain;
		std::string current = className;
		uint32_t maxDepth = 20;
		while( current.empty() == false && maxDepth > 0 ) {
			chain.push_back( current );
			std::unordered_map<std::string, std::string>::iterator parentIterator = this->inheritanceMap_.find( current );
			if( parentIterator != this->inheritanceMap_.end() ) {
				current = parentIterator->second;
			}
			else {
				break;
			}
			maxDepth--;
		}
		if( chain.size() <= 1 ) {
			return "";
		}
		std::string result;
		for( size_t chainIndex = 0; chainIndex < chain.size(); chainIndex++ ) {
			if( chainIndex > 0 ) {
				result+= " → ";
			}
			result+= fmt::format( "`{}`", chain[chainIndex] );
		}
		return result;
	}

} // namespace uranite::doc
