
//
// @author hxAri (hxari)
// @create 2026-06-15 18:00
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

#include <algorithm>
#include <filesystem>
#include <fmt/core.h>

#include "uranite-doc/renderer/table-of-contents-builder.hpp"

namespace uranite::doc {
	
	void TableOfContentsBuilder::buildFromSite( const DocumentationSite& site ) {
		this->categories_.clear();
		this->uncategorizedModules_.clear();
		TableOfContentsCategory coreCategory;
		coreCategory.categoryName = "Core";
		coreCategory.matchingPrefixes = { "language", "errors", "memory", "functions" };
		this->categories_.push_back( coreCategory );
		TableOfContentsCategory collectionsCategory;
		collectionsCategory.categoryName = "Collections";
		collectionsCategory.matchingPrefixes = { "collection", "iterators" };
		this->categories_.push_back( collectionsCategory );
		TableOfContentsCategory concurrencyCategory;
		concurrencyCategory.categoryName = "Concurrency";
		concurrencyCategory.matchingPrefixes = { "async", "coroutine", "fiber", "threading" };
		this->categories_.push_back( concurrencyCategory );
		TableOfContentsCategory ioNetworkingCategory;
		ioNetworkingCategory.categoryName = "I/O & Networking";
		ioNetworkingCategory.matchingPrefixes = { "io", "net", "web", "requests" };
		this->categories_.push_back( ioNetworkingCategory );
		TableOfContentsCategory systemCategory;
		systemCategory.categoryName = "System";
		systemCategory.matchingPrefixes = { "os", "sys", "kernel" };
		this->categories_.push_back( systemCategory );
		TableOfContentsCategory securityCryptoCategory;
		securityCryptoCategory.categoryName = "Security & Crypto";
		securityCryptoCategory.matchingPrefixes = { "crypto", "security" };
		this->categories_.push_back( securityCryptoCategory );
		TableOfContentsCategory utilityCategory;
		utilityCategory.categoryName = "Utility";
		utilityCategory.matchingPrefixes = { "convert", "datetime", "time", "string", "regexp", "logging", "math", "testing" };
		this->categories_.push_back( utilityCategory );
		TableOfContentsCategory applicationCategory;
		applicationCategory.categoryName = "Application";
		applicationCategory.matchingPrefixes = { "cli", "adelia", "process", "subprocess", "ipc", "ffi" };
		this->categories_.push_back( applicationCategory );
		this->collectModulesFromTree( site.rootPackage );
	}
	
	void TableOfContentsBuilder::collectModulesFromTree( const PackageTreeNode& node ) {
		for( const ModuleDocumentation& moduleDoc : node.modules ) {
			std::string assignedCategory = this->categorizeModule( moduleDoc.packageName );
			bool categoryFound = false;
			for( TableOfContentsCategory& category : this->categories_ ) {
				if( category.categoryName == assignedCategory ) {
					category.assignedModules.push_back( &moduleDoc );
					categoryFound = true;
					break;
				}
			}
			if( categoryFound == false ) {
				this->uncategorizedModules_.push_back( &moduleDoc );
			}
		}
		for( const PackageTreeNode& childNode : node.childPackages ) {
			this->collectModulesFromTree( childNode );
		}
	}
	
	std::string TableOfContentsBuilder::categorizeModule( const std::string& packageName ) const {
		std::string secondSegment;
		size_t firstDotPosition = packageName.find( '.' );
		if( firstDotPosition != std::string::npos ) {
			size_t secondDotPosition = packageName.find( '.', firstDotPosition + 1 );
			if( secondDotPosition != std::string::npos ) {
				secondSegment = packageName.substr( firstDotPosition + 1, secondDotPosition - firstDotPosition - 1 );
			}
			else {
				secondSegment = packageName.substr( firstDotPosition + 1 );
			}
		}
		else {
			secondSegment = packageName;
		}
		for( const TableOfContentsCategory& category : this->categories_ ) {
			for( const std::string& prefix : category.matchingPrefixes ) {
				if( secondSegment == prefix ) {
					return category.categoryName;
				}
			}
		}
		return "";
	}
	
	std::string TableOfContentsBuilder::computeModuleLinkPath( const ModuleDocumentation& moduleDoc, const std::string& inputBaseDirectory ) const {
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
	
	std::string TableOfContentsBuilder::entityKindLabel( EntityKind entityKind ) const {
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
	
	std::string TableOfContentsBuilder::escapeHtml( const std::string& text ) const {
		std::string result;
		result.reserve( text.size() );
		for( char character : text ) {
			switch( character ) {
				case '&': result+= "&amp;"; break;
				case '<': result+= "&lt;"; break;
				case '>': result+= "&gt;"; break;
				case '"': result+= "&quot;"; break;
				default: result+= character; break;
			}
		}
		return result;
	}
	
	std::string TableOfContentsBuilder::renderMarkdownToc( const std::string& inputBaseDirectory ) const {
		std::string content;
		for( const TableOfContentsCategory& category : this->categories_ ) {
			if( category.assignedModules.empty() ) {
				continue;
			}
			content+= fmt::format( "## {}\n\n", category.categoryName );
			for( const ModuleDocumentation* moduleDocPointer : category.assignedModules ) {
				std::string linkPath = this->computeModuleLinkPath( *moduleDocPointer, inputBaseDirectory ) + ".md";
				std::string displayName = moduleDocPointer->packageName;
				size_t lastDotPosition = displayName.rfind( '.' );
				if( lastDotPosition != std::string::npos ) {
					displayName = displayName.substr( lastDotPosition + 1 );
				}
				content+= fmt::format( "### [{}]({})\n\n", displayName, linkPath );
				uint32_t entityDisplayCount = 0;
				static const uint32_t maxEntityDisplay = 8;
				std::vector<const EntityDocumentationEntry*> sortedEntities;
				for( const EntityDocumentationEntry& entity : moduleDocPointer->exportedEntities ) {
					sortedEntities.push_back( &entity );
				}
				std::sort( sortedEntities.begin(), sortedEntities.end(),
					[]( const EntityDocumentationEntry* entityA, const EntityDocumentationEntry* entityB ) -> bool {
						int32_t kindOrderA = static_cast<int32_t>( entityA->entityKind );
						int32_t kindOrderB = static_cast<int32_t>( entityB->entityKind );
						if( kindOrderA != kindOrderB ) {
							return kindOrderA < kindOrderB;
						}
						return entityA->entityName < entityB->entityName;
					}
				);
				for( const EntityDocumentationEntry* entityPointer : sortedEntities ) {
					if( entityDisplayCount >= maxEntityDisplay ) {
						uint32_t remainingCount = sortedEntities.size() - maxEntityDisplay;
						content+= fmt::format( "- *...and {} more*\n", remainingCount );
						break;
					}
					std::string kindLabel = this->entityKindLabel( entityPointer->entityKind );
					std::string entityLine = fmt::format( "- **{}** `{}`", kindLabel, entityPointer->entityName );
					if( entityPointer->genericParameters.empty() == false ) {
						entityLine+= "<";
						for( size_t paramIndex = 0; paramIndex < entityPointer->genericParameters.size(); paramIndex++ ) {
							if( paramIndex > 0 ) {
								entityLine+= ", ";
							}
							entityLine+= entityPointer->genericParameters[paramIndex];
						}
						entityLine+= ">";
					}
					if( entityPointer->entityDoccomment.summaryLine.empty() == false ) {
						entityLine+= fmt::format( " — {}", entityPointer->entityDoccomment.summaryLine );
					}
					content+= entityLine + "\n";
					if( entityPointer->methods.empty() == false ) {
						std::string methodList = "  - ";
						uint32_t methodDisplayCount = 0;
						static const uint32_t maxMethodDisplay = 5;
						for( const MethodDocumentationEntry& method : entityPointer->methods ) {
							if( method.accessModifier != "public" ) {
								continue;
							}
							if( methodDisplayCount >= maxMethodDisplay ) {
								methodList+= " ...";
								break;
							}
							if( methodDisplayCount > 0 ) {
								methodList+= ", ";
							}
							methodList+= fmt::format( "`{}()`", method.methodName );
							methodDisplayCount++;
						}
						if( methodDisplayCount > 0 ) {
							content+= methodList + "\n";
						}
					}
					entityDisplayCount++;
				}
				content+= "\n";
			}
		}
		if( this->uncategorizedModules_.empty() == false ) {
			content+= "## Other\n\n";
			for( const ModuleDocumentation* moduleDocPointer : this->uncategorizedModules_ ) {
				std::string linkPath = this->computeModuleLinkPath( *moduleDocPointer, inputBaseDirectory ) + ".md";
				content+= fmt::format( "- [{}]({})\n", moduleDocPointer->packageName, linkPath );
			}
			content+= "\n";
		}
		return content;
	}
	
	std::string TableOfContentsBuilder::renderHtmlToc( const std::string& inputBaseDirectory ) const {
		std::string content;
		for( const TableOfContentsCategory& category : this->categories_ ) {
			if( category.assignedModules.empty() ) {
				continue;
			}
			content+= fmt::format( "<details open>\n<summary><h2>{}</h2></summary>\n", this->escapeHtml( category.categoryName ) );
			for( const ModuleDocumentation* moduleDocPointer : category.assignedModules ) {
				std::string linkPath = this->computeModuleLinkPath( *moduleDocPointer, inputBaseDirectory ) + ".html";
				std::string displayName = moduleDocPointer->packageName;
				size_t lastDotPosition = displayName.rfind( '.' );
				if( lastDotPosition != std::string::npos ) {
					displayName = displayName.substr( lastDotPosition + 1 );
				}
				content+= fmt::format( "<h3><a href=\"/{}\">{}</a></h3>\n", linkPath, this->escapeHtml( displayName ) );
				content+= "<ul>\n";
				uint32_t entityDisplayCount = 0;
				static const uint32_t maxEntityDisplay = 8;
				for( const EntityDocumentationEntry& entity : moduleDocPointer->exportedEntities ) {
					if( entityDisplayCount >= maxEntityDisplay ) {
						uint32_t remainingCount = moduleDocPointer->exportedEntities.size() - maxEntityDisplay;
						content+= fmt::format( "<li><em>...and {} more</em></li>\n", remainingCount );
						break;
					}
					std::string kindLabel = this->entityKindLabel( entity.entityKind );
					content+= fmt::format( "<li><strong>{}</strong> <code>{}</code>", this->escapeHtml( kindLabel ), this->escapeHtml( entity.entityName ) );
					if( entity.entityDoccomment.summaryLine.empty() == false ) {
						content+= fmt::format( " &mdash; {}", this->escapeHtml( entity.entityDoccomment.summaryLine ) );
					}
					content+= "</li>\n";
					entityDisplayCount++;
				}
				content+= "</ul>\n";
			}
			content+= "</details>\n";
		}
		if( this->uncategorizedModules_.empty() == false ) {
			content+= "<details open>\n<summary><h2>Other</h2></summary>\n<ul>\n";
			for( const ModuleDocumentation* moduleDocPointer : this->uncategorizedModules_ ) {
				std::string linkPath = this->computeModuleLinkPath( *moduleDocPointer, inputBaseDirectory ) + ".html";
				content+= fmt::format( "<li><a href=\"/{}\">{}</a></li>\n", linkPath, this->escapeHtml( moduleDocPointer->packageName ) );
			}
			content+= "</ul>\n</details>\n";
		}
		return content;
	}

} // namespace uranite::doc
