
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

#include "uranite-doc/renderer/html-renderer.hpp"
#include "uranite-doc/renderer/table-of-contents-builder.hpp"

namespace uranite::doc {
	
	void HtmlRenderer::render( const DocumentationSite& site, const std::string& outputDirectory ) {
		std::filesystem::create_directories( outputDirectory );
		this->buildInheritanceMap( site );
		std::string sidebarContent = this->generateSidebar( site.rootPackage, site.inputBaseDirectory );
		std::string stylesheet = this->generateStylesheet();
		std::string stylesheetPath = outputDirectory + "/styles.css";
		std::ofstream styleFile( stylesheetPath );
		styleFile << stylesheet;
		std::string projectHeading;
		if( site.projectMetadata.projectName.empty() == false ) {
			projectHeading = site.projectMetadata.projectName;
		}
		else {
			projectHeading = "Uranite API Documentation";
		}
		std::string indexBody = fmt::format( "<h1>{}</h1>\n", this->escapeHtml( projectHeading ) );
		if( site.projectMetadata.readmeFound && site.projectMetadata.readmeExcerpt.empty() == false ) {
			indexBody+= fmt::format( "<div class=\"readme-excerpt\"><p>{}</p></div>\n", this->escapeHtml( site.projectMetadata.readmeExcerpt ) );
		}
		indexBody+= "<h2>Overview</h2>\n";
		indexBody+= "<div class=\"stats\">\n";
		indexBody+= fmt::format( "<p><strong>Modules:</strong> {}</p>\n", site.totalModules );
		indexBody+= fmt::format( "<p><strong>Entities:</strong> {}</p>\n", site.totalEntities );
		indexBody+= fmt::format( "<p><strong>Methods:</strong> {}</p>\n", site.totalMethods );
		if( site.projectMetadata.licenseIdentifier.empty() == false ) {
			indexBody+= fmt::format( "<p><strong>License:</strong> {}</p>\n", this->escapeHtml( site.projectMetadata.licenseIdentifier ) );
		}
		indexBody+= "</div>\n";
		TableOfContentsBuilder tocBuilder;
		tocBuilder.buildFromSite( site );
		indexBody+= "<hr>\n";
		indexBody+= tocBuilder.renderHtmlToc( site.inputBaseDirectory );
		indexBody+= "<hr>\n";
		std::string indexPath = outputDirectory + "/index.html";
		std::ofstream indexFile( indexPath );
		indexFile << this->wrapInTemplate( "Uranite API Documentation", indexBody, sidebarContent );
		this->renderPackageTreeHtml( site.rootPackage, outputDirectory, site.inputBaseDirectory, sidebarContent );
	}
	
	std::string HtmlRenderer::wrapInTemplate( const std::string& title, const std::string& bodyContent, const std::string& sidebarContent ) {
		std::string html;
		html+= "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
		html+= "<meta charset=\"UTF-8\">\n";
		html+= "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
		html+= fmt::format( "<title>{}</title>\n", this->escapeHtml( title ) );
		html+= "<link rel=\"stylesheet\" href=\"/styles.css\">\n";
		html+= "</head>\n<body>\n";
		html+= "<div class=\"layout\">\n";
		html+= "<nav class=\"sidebar\">\n";
		html+= "<h2><a href=\"/index.html\">Uranite</a></h2>\n";
		html+= sidebarContent;
		html+= "</nav>\n";
		html+= "<main class=\"content\">\n";
		html+= bodyContent;
		html+= "</main>\n";
		html+= "</div>\n";
		html+= "</body>\n</html>\n";
		return html;
	}
	
	std::string HtmlRenderer::generateSidebar( const PackageTreeNode& node, const std::string& inputBaseDirectory ) {
		std::string content = "<ul>\n";
		for( const ModuleDocumentation& moduleDoc : node.modules ) {
			std::string outputRelativePath = this->computeOutputRelativePath( moduleDoc, inputBaseDirectory );
			std::string linkPath = outputRelativePath + ".html";
			content+= fmt::format( "<li><a href=\"/{}\">{}</a></li>\n", linkPath, this->escapeHtml( moduleDoc.packageName ) );
		}
		for( const PackageTreeNode& child : node.childPackages ) {
			content+= fmt::format( "<li class=\"package\"><strong>{}</strong>\n", this->escapeHtml( child.segmentName ) );
			content+= this->generateSidebar( child, inputBaseDirectory );
			content+= "</li>\n";
		}
		content+= "</ul>\n";
		return content;
	}
	
	std::string HtmlRenderer::generateStylesheet() {
		return (
			":root {\n"
			"     --bg: #1a1b26;\n"
			"     --fg: #c0caf5;\n"
			"     --sidebar-bg: #16161e;\n"
			"     --accent: #7aa2f7;\n"
			"     --border: #292e42;\n"
			"     --code-bg: #24283b;\n"
			"     --heading: #bb9af7;\n"
			"     --muted: #565f89;\n"
			"}\n"
			"* {\n"
			"     margin: 0;\n"
			"     padding: 0;\n"
			"     box-sizing: border-box;\n"
			"}\n"
			"body {\n"
			"     font-family: 'Inter', -apple-system, sans-serif;\n"
			"     background: var(--bg);\n"
			"     color: var(--fg);\n"
			"     line-height: 1.6;\n"
			"}\n"
			".layout {\n"
			"     display: flex;\n"
			"     min-height: 100vh;\n"
			"}\n"
			".sidebar {\n"
			"     width: 280px;\n"
			"     background: var(--sidebar-bg);\n"
			"     padding: 1.5rem;\n"
			"     border-right: 1px solid var(--border);\n"
			"     overflow-y: auto;\n"
			"     position: fixed;\n"
			"     height: 100vh;\n"
			"}\n"
			".sidebar h2 {\n"
			"     margin-bottom: 1rem;\n"
			"}\n"
			".sidebar h2 a {\n"
			"     color: var(--accent);\n"
			"     text-decoration: none;\n"
			"}\n"
			".sidebar ul {\n"
			"     list-style: none;\n"
			"     padding-left: 0.75rem;\n"
			"}\n"
			".sidebar li {\n"
			"     margin: 0.25rem 0;\n"
			"     font-size: 0.85rem;\n"
			"}\n"
			".sidebar a {\n"
			"     color: var(--fg);\n"
			"     text-decoration: none;\n"
			"}\n"
			".sidebar a:hover {\n"
			"     color: var(--accent);\n"
			"}\n"
			".sidebar .package > strong {\n"
			"     color: var(--muted);\n"
			"     font-size: 0.8rem;\n"
			"     text-transform: uppercase;\n"
			"     letter-spacing: 0.05em;\n"
			"}\n"
			".content {\n"
			"     margin-left: 280px;\n"
			"     padding: 2rem 3rem;\n"
			"     max-width: 900px;\n"
			"     flex: 1;\n"
			"}\n"
			"h1 {\n"
			"     color: var(--heading);\n"
			"     margin-bottom: 1rem;\n"
			"     font-size: 1.8rem;\n"
			"}\n"
			"h2 {\n"
			"     color: var(--heading);\n"
			"     margin: 2rem 0 0.75rem;\n"
			"     font-size: 1.4rem;\n"
			"     border-bottom: 1px solid var(--border);\n"
			"     padding-bottom: 0.5rem;\n"
			"}\n"
			"h3 {\n"
			"     color: var(--accent);\n"
			"     margin: 1.5rem 0 0.5rem;\n"
			"     font-size: 1.15rem;\n"
			"}\n"
			"h4 {\n"
			"     color: var(--fg);\n"
			"     margin: 1rem 0 0.5rem;\n"
			"     font-size: 1rem;\n"
			"}\n"
			"code {\n"
			"     background: var(--code-bg);\n"
			"     padding: 0.15rem 0.4rem;\n"
			"     border-radius: 3px;\n"
			"     font-family: 'JetBrains Mono', monospace;\n"
			"     font-size: 0.9em;\n"
			"}\n"
			"pre {\n"
			"     background: var(--code-bg);\n"
			"     padding: 1rem;\n"
			"     border-radius: 6px;\n"
			"     overflow-x: auto;\n"
			"     margin: 0.75rem 0;\n"
			"}\n"
			"pre code {\n"
			"     padding: 0;\n"
			"     background: none;\n"
			"}\n"
			"table {\n"
			"     border-collapse: collapse;\n"
			"     width: 100%;\n"
			"     margin: 0.75rem 0;\n"
			"}\n"
			"th, td {\n"
			"     padding: 0.5rem 0.75rem;\n"
			"     text-align: left;\n"
			"     border: 1px solid var(--border);\n"
			"}\n"
			"th {\n"
			"     background: var(--code-bg);\n"
			"     color: var(--accent);\n"
			"     font-weight: 600;\n"
			"}\n"
			".stats p {\n"
			"     margin: 0.25rem 0;\n"
			"}\n"
			".method {\n"
			"     margin: 1rem 0;\n"
			"     padding: 1rem;\n"
			"     border-left: 3px solid var(--accent);\n"
			"     background: var(--code-bg);\n"
			"     border-radius: 0 6px 6px 0;\n"
			"}\n"
			".method-signature {\n"
			"     font-family: 'JetBrains Mono', monospace;\n"
			"     font-size: 0.9rem;\n"
			"     color: var(--accent);\n"
			"}\n"
			".modifier {\n"
			"     display: inline-block;\n"
			"     background: var(--border);\n"
			"     color: var(--muted);\n"
			"     padding: 0.1rem 0.4rem;\n"
			"     border-radius: 3px;\n"
			"     font-size: 0.75rem;\n"
			"     margin-right: 0.25rem;\n"
			"}\n"
			".inheritance-chain {\n"
			"     color: var(--muted);\n"
			"     font-size: 0.9rem;\n"
			"}\n"
			".inheritance-chain code {\n"
			"     color: var(--accent);\n"
			"}\n"
			"p {\n"
			"     margin: 0.5rem 0;\n"
			"}\n"
			"ul {\n"
			"     padding-left: 1.5rem;\n"
			"     margin: 0.5rem 0;\n"
			"}"	
		);
	}
	
	std::string HtmlRenderer::renderEntityHtml( const EntityDocumentationEntry& entity, uint32_t headingLevel ) {
		std::string headingTag = fmt::format( "h{}", headingLevel > 6 ? 6 : headingLevel );
		std::string kindLabel = this->entityKindToString( entity.entityKind );
		std::string content = fmt::format( "<{0}>{1} <code>{2}</code>", headingTag, this->escapeHtml( kindLabel ), this->escapeHtml( entity.entityName ) );
		if( entity.genericParameters.empty() == false ) {
			content+= "&lt;";
			for( size_t paramIndex = 0; paramIndex < entity.genericParameters.size(); paramIndex++ ) {
				if( paramIndex > 0 ) {
					content+= ", ";
				}
				content+= this->escapeHtml( entity.genericParameters[paramIndex] );
			}
			content+= "&gt;";
		}
		content+= fmt::format( "</{}>\n", headingTag );
		if( entity.parentClassName.empty() == false ) {
			std::string chain = this->renderInheritanceChainHtml( entity.parentClassName );
			if( chain.empty() == false ) {
				content+= fmt::format( "<p class=\"inheritance-chain\"><strong>Extends:</strong> {}</p>\n", chain );
			}
			else {
				content+= fmt::format( "<p><strong>Extends:</strong> <code>{}</code></p>\n", this->escapeHtml( entity.parentClassName ) );
			}
		}
		if( entity.implementedInterfaces.empty() == false ) {
			content+= "<p><strong>Implements:</strong> ";
			for( size_t interfaceIndex = 0; interfaceIndex < entity.implementedInterfaces.size(); interfaceIndex++ ) {
				if( interfaceIndex > 0 ) {
					content+= ", ";
				}
				content+= fmt::format( "<code>{}</code>", this->escapeHtml( entity.implementedInterfaces[interfaceIndex] ) );
			}
			content+= "</p>\n";
		}
		content+= this->renderDoccommentHtml( entity.entityDoccomment );
		if( entity.fields.empty() == false ) {
			content+= fmt::format( "<h{}>Fields</h{}>\n", headingLevel + 1, headingLevel + 1 );
			content+= "<table>\n<tr><th>Name</th><th>Type</th><th>Access</th></tr>\n";
			for( const FieldDocumentationEntry& field : entity.fields ) {
				content+= fmt::format( "<tr><td><code>{}</code></td><td><code>{}</code></td><td>{}</td></tr>\n",
					this->escapeHtml( field.fieldName ), this->escapeHtml( field.fieldType ), this->escapeHtml( field.accessModifier ) );
			}
			content+= "</table>\n";
		}
		if( entity.methods.empty() == false ) {
			content+= fmt::format( "<h{}>Methods</h{}>\n", headingLevel + 1, headingLevel + 1 );
			for( const MethodDocumentationEntry& method : entity.methods ) {
				content+= this->renderMethodHtml( method );
			}
		}
		for( const EntityDocumentationEntry& nested : entity.nestedEntities ) {
			content+= this->renderEntityHtml( nested, headingLevel + 1 );
		}
		return content;
	}
	
	std::string HtmlRenderer::renderMethodHtml( const MethodDocumentationEntry& method ) {
		std::string content = "<div class=\"method\">\n";
		content+= fmt::format( "<p class=\"method-signature\">{}</p>\n", this->escapeHtml( method.signatureText ) );
		if( method.isAsync ) {
			content+= "<span class=\"modifier\">async</span>";
		}
		if( method.isStatic ) {
			content+= "<span class=\"modifier\">static</span>";
		}
		if( method.isVirtual ) {
			content+= "<span class=\"modifier\">virtual</span>";
		}
		if( method.isAbstract ) {
			content+= "<span class=\"modifier\">abstract</span>";
		}
		if( method.isProperty ) {
			content+= "<span class=\"modifier\">property</span>";
		}
		content+= this->renderDoccommentHtml( method.doccomment );
		content+= "</div>\n";
		return content;
	}
	
	std::string HtmlRenderer::renderDoccommentHtml( const ParsedDoccomment& doccomment ) {
		std::string content;
		if( doccomment.summaryLine.empty() == false ) {
			content+= fmt::format( "<p>{}</p>\n", this->escapeHtml( doccomment.summaryLine ) );
		}
		if( doccomment.extendedDescription.empty() == false ) {
			content+= fmt::format( "<p>{}</p>\n", this->escapeHtml( doccomment.extendedDescription ) );
		}
		if( doccomment.parameters.empty() == false ) {
			content+= "<p><strong>Parameters:</strong></p>\n<ul>\n";
			for( const ParameterDocumentation& parameter : doccomment.parameters ) {
				content+= fmt::format( "<li><code>{}</code>", this->escapeHtml( parameter.parameterName ) );
				if( parameter.parameterType.empty() == false ) {
					content+= fmt::format( " (<code>{}</code>)", this->escapeHtml( parameter.parameterType ) );
				}
				if( parameter.descriptionText.empty() == false ) {
					content+= fmt::format( " &mdash; {}", this->escapeHtml( parameter.descriptionText ) );
				}
				content+= "</li>\n";
			}
			content+= "</ul>\n";
		}
		if( doccomment.returns.has_value() ) {
			const ReturnsDocumentation& returnsInfo = doccomment.returns.value();
			content+= "<p><strong>Returns:</strong>";
			if( returnsInfo.returnType.empty() == false ) {
				content+= fmt::format( " <code>{}</code>", this->escapeHtml( returnsInfo.returnType ) );
			}
			if( returnsInfo.descriptionText.empty() == false ) {
				content+= fmt::format( " &mdash; {}", this->escapeHtml( returnsInfo.descriptionText ) );
			}
			content+= "</p>\n";
		}
		if( doccomment.raises.empty() == false ) {
			content+= "<p><strong>Raises:</strong></p>\n<ul>\n";
			for( const RaisesDocumentation& raisesEntry : doccomment.raises ) {
				std::string chain = this->renderInheritanceChainHtml( raisesEntry.exceptionType );
				if( chain.empty() == false ) {
					content+= fmt::format( "<li>{} &mdash; {}</li>\n", chain, this->escapeHtml( raisesEntry.conditionDescription ) );
				}
				else {
					content+= fmt::format( "<li><code>{}</code> &mdash; {}</li>\n",
						this->escapeHtml( raisesEntry.exceptionType ), this->escapeHtml( raisesEntry.conditionDescription ) );
				}
			}
			content+= "</ul>\n";
		}
		if( doccomment.complexity.has_value() ) {
			const ComplexityDocumentation& complexityInfo = doccomment.complexity.value();
			content+= "<p><strong>Complexity:</strong></p>\n<ul>\n";
			if( complexityInfo.timeComplexity.empty() == false ) {
				content+= fmt::format( "<li>Time: <code>{}</code></li>\n", this->escapeHtml( complexityInfo.timeComplexity ) );
			}
			if( complexityInfo.spaceComplexity.empty() == false ) {
				content+= fmt::format( "<li>Space: <code>{}</code></li>\n", this->escapeHtml( complexityInfo.spaceComplexity ) );
			}
			content+= "</ul>\n";
		}
		return content;
	}
	
	std::string HtmlRenderer::escapeHtml( const std::string& text ) const {
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
	
	std::string HtmlRenderer::entityKindToString( EntityKind entityKind ) const {
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
	
	std::string HtmlRenderer::computeOutputRelativePath( const ModuleDocumentation& moduleDoc, const std::string& inputBaseDirectory ) const {
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
	
	std::string HtmlRenderer::buildModuleTableOfContentsHtml( const ModuleDocumentation& moduleDoc ) const {
		if( moduleDoc.exportedEntities.empty() && moduleDoc.importedModules.empty() ) {
			return "";
		}
		std::string toc = "<nav class=\"module-toc\">\n<h2>Table of Contents</h2>\n<ul>\n";
		if( moduleDoc.importedModules.empty() == false ) {
			toc+= "<li><a href=\"#imports\">Imports</a></li>\n";
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
			toc+= fmt::format( "<li><a href=\"#{}-{}\">{} <code>{}</code></a>\n",
				kindLabel, anchorName, this->escapeHtml( kindLabel ), this->escapeHtml( entity.entityName ) );
			bool hasPublicMethods = false;
			for( const MethodDocumentationEntry& method : entity.methods ) {
				if( method.accessModifier == "public" ) {
					hasPublicMethods = true;
					break;
				}
			}
			if( hasPublicMethods ) {
				toc+= "<ul>\n";
				for( const MethodDocumentationEntry& method : entity.methods ) {
					if( method.accessModifier != "public" ) {
						continue;
					}
					toc+= fmt::format( "<li><a href=\"#{}\"><code>{}()</code></a></li>\n",
						this->escapeHtml( method.methodName ), this->escapeHtml( method.methodName ) );
				}
				toc+= "</ul>\n";
			}
			toc+= "</li>\n";
		}
		toc+= "</ul>\n</nav>\n";
		return toc;
	}
	
	void HtmlRenderer::renderPackageTreeHtml( const PackageTreeNode& node, const std::string& outputDirectory, const std::string& inputBaseDirectory, const std::string& sidebarContent ) {
		for( const ModuleDocumentation& moduleDoc : node.modules ) {
			std::string bodyContent = fmt::format( "<h1>{}</h1>\n", this->escapeHtml( moduleDoc.packageName ) );
			bodyContent+= this->buildModuleTableOfContentsHtml( moduleDoc );
			if( moduleDoc.importedModules.empty() == false ) {
				bodyContent+= "<h2 id=\"imports\">Imports</h2>\n<ul>\n";
				for( const std::string& importPath : moduleDoc.importedModules ) {
					bodyContent+= fmt::format( "<li><code>{}</code></li>\n", this->escapeHtml( importPath ) );
				}
				bodyContent+= "</ul>\n";
			}
			for( const EntityDocumentationEntry& entity : moduleDoc.exportedEntities ) {
				bodyContent+= this->renderEntityHtml( entity, 2 );
			}
			std::string outputRelativePath = this->computeOutputRelativePath( moduleDoc, inputBaseDirectory );
			std::string filePath = outputDirectory + "/" + outputRelativePath + ".html";
			std::filesystem::path parentDirectory = std::filesystem::path( filePath ).parent_path();
			if( parentDirectory.empty() == false ) {
				std::filesystem::create_directories( parentDirectory );
			}
			std::ofstream outputFile( filePath );
			outputFile << this->wrapInTemplate( moduleDoc.packageName, bodyContent, sidebarContent );
		}
		for( const PackageTreeNode& child : node.childPackages ) {
			this->renderPackageTreeHtml( child, outputDirectory, inputBaseDirectory, sidebarContent );
		}
	}
	
	void HtmlRenderer::buildInheritanceMap( const DocumentationSite& site ) {
		this->inheritanceMap_.clear();
		this->entityToModulePath_.clear();
		this->collectEntitiesFromTree( site.rootPackage );
	}
	
	void HtmlRenderer::collectEntitiesFromTree( const PackageTreeNode& node ) {
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
	
	std::string HtmlRenderer::renderInheritanceChainHtml( const std::string& className ) {
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
				result+= " &rarr; ";
			}
			result+= fmt::format( "<code>{}</code>", this->escapeHtml( chain[chainIndex] ) );
		}
		return result;
	}

} // namespace uranite::doc
