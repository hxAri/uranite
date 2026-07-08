
//
// @author hxAri (hxari)
// @create 2025-02-24 15:15
// @update 2026-06-17 20:03
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
#include <fmt/color.h>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <set>
#include <spdlog/spdlog.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

#include <llvm/ADT/Triple.h>
#include <llvm/Support/Host.h>

#include "uranite/codegen/codegen.hpp"
#include "uranite/compiler/driver.hpp"
#include "uranite/ir/hir-lowering.hpp"
#include "uranite/ir/hir-printer.hpp"
#include "uranite/ir/hir-validator.hpp"
#include "uranite/ir/mir-analysis.hpp"
#include "uranite/ir/mir-borrow-checker.hpp"
#include "uranite/ir/mir-lowering.hpp"
#include "uranite/ir/mir-codegen.hpp"
#include "uranite/ir/mir-optimizer.hpp"
#include "uranite/ir/mir-printer.hpp"
#include "uranite/version.hpp"
#include "uranite/lexer/lexer.hpp"
#include "uranite/parser/parser.hpp"
#include "uranite/semantic/analyzer.hpp"
#include "uranite/semantic/borrow.hpp"
#include "uranite/token/token.hpp"
#include "uranite/visitors/printer.hpp"

namespace uranite::compiler {
	
	Driver::Driver( const Options& options ) : options( options ) {
		this->diagnostic.setMaxErrors( options.maximumErrorCount );
	}
	
	// int Driver::borrowCheck() {
	// }
	
	// int Driver::generateCode() {
	// }
	
	static std::string getExecutableDirectory() {
		char executablePathBuffer[4096];
		ssize_t bytesRead = readlink( "/proc/self/exe", executablePathBuffer, sizeof( executablePathBuffer ) - 1 );
		if( bytesRead <= 0 ) {
			return "";
		}
		executablePathBuffer[bytesRead] = '\0';
		std::filesystem::path executablePath( executablePathBuffer );
		return executablePath.parent_path().string();
	}
	
	static bool isValidModulesDirectory( const std::filesystem::path& candidatePath ) {
		if( std::filesystem::exists( candidatePath ) == false || std::filesystem::is_directory( candidatePath ) == false ) {
			return false;
		}
		if( std::filesystem::exists( candidatePath / "__mod__.urn" ) ) {
			return true;
		}
		if( std::filesystem::exists( candidatePath / "errors" / "error.urn" ) ) {
			return true;
		}
		return false;
	}
	
	std::string Driver::findModulesDirectory() const {
		if( this->modulesDirectoryCached_ ) {
			return this->cachedModulesDirectory_;
		}
		std::string result;
		if( this->options.modulesPath.empty() == false ) {
			std::filesystem::path explicitPath( this->options.modulesPath );
			if( std::filesystem::exists( explicitPath ) && std::filesystem::is_directory( explicitPath ) ) {
				result = explicitPath.string();
				this->cachedModulesDirectory_ = result;
				this->modulesDirectoryCached_ = true;
				return result;
			}
			if( this->options.verbose ) {
				spdlog::warn( "Specified modules path does not exist: {}", this->options.modulesPath );
			}
		}
		const char* envModulesPath = std::getenv( "URANITE_MODULES_PATH" );
		if( envModulesPath != nullptr ) {
			std::string envPathStr( envModulesPath );
			if( envPathStr.empty() == false ) {
				std::filesystem::path envPath( envPathStr );
				if( isValidModulesDirectory( envPath ) ) {
					result = envPath.string();
					this->cachedModulesDirectory_ = result;
					this->modulesDirectoryCached_ = true;
					return result;
				}
			}
		}
		std::string executableDirectory = getExecutableDirectory();
		if( executableDirectory.empty() == false ) {
			std::filesystem::path exeRelativePath = std::filesystem::path( executableDirectory ) / ".." / "lib" / "uranite" / "stdlibs";
			if( isValidModulesDirectory( exeRelativePath ) ) {
				result = std::filesystem::canonical( exeRelativePath ).string();
				this->cachedModulesDirectory_ = result;
				this->modulesDirectoryCached_ = true;
				return result;
			}
		}
		std::string compileTimeModulesDir = _URANITE_MODULES_DIR_;
		if( compileTimeModulesDir.empty() == false ) {
			std::filesystem::path compileTimePath( compileTimeModulesDir );
			if( isValidModulesDirectory( compileTimePath ) ) {
				result = compileTimePath.string();
				this->cachedModulesDirectory_ = result;
				this->modulesDirectoryCached_ = true;
				return result;
			}
		}
		std::filesystem::path currentWorkingDirectory = std::filesystem::current_path();
		std::filesystem::path modulePathFromCwd = currentWorkingDirectory / "stdlibs";
		if( isValidModulesDirectory( modulePathFromCwd ) ) {
			result = modulePathFromCwd.string();
			this->cachedModulesDirectory_ = result;
			this->modulesDirectoryCached_ = true;
			return result;
		}
		std::filesystem::path traversalPath = currentWorkingDirectory;
		for( int traversalIndex = 0; traversalIndex < 5; traversalIndex++ ) {
			if( traversalPath.has_parent_path() && traversalPath.parent_path() != traversalPath ) {
				traversalPath = traversalPath.parent_path();
				std::filesystem::path candidatePath = traversalPath / "stdlibs";
				if( isValidModulesDirectory( candidatePath ) ) {
					result = candidatePath.string();
					this->cachedModulesDirectory_ = result;
					this->modulesDirectoryCached_ = true;
					return result;
				}
			}
			else {
				break;
			}
		}
		
		this->cachedModulesDirectory_ = "";
		this->modulesDirectoryCached_ = true;
		return "";
	}
	
	// int Driver::link() {
	// }
	
	bool Driver::loadModule( const std::string& filePath, ast::nodes::Program& targetProgram, const ast::nodes::ImportDeclaration* importDeclaration ) {
		std::string canonicalPath = std::filesystem::canonical( filePath ).string();
		ast::nodes::ProgramSharedPointer moduleProgram;
		std::unordered_map<std::string, ModuleInfo>::iterator cacheIterator = this->modules.find( canonicalPath );
		if( cacheIterator != this->modules.end() ) {
			moduleProgram = cacheIterator->second.program;
		}
		else {
			std::ifstream moduleFile( filePath );
			if( moduleFile.is_open() == false ) {
				fmt::print( stderr, "error: could not open module \"{}\"\n", filePath );
				return false;
			}
			std::ostringstream moduleStream;
			moduleStream << moduleFile.rdbuf();
			std::string moduleSource = moduleStream.str();
			if( this->options.verbose ) {
				spdlog::info( "Loading module: {}", filePath );
			}
			diagnostic::Engine moduleDiagnostic( this->options.maximumErrorCount, -1 );
			lexer::Lexer moduleLexer( moduleSource, filePath, moduleDiagnostic );
			std::vector<token::Token> moduleTokens = moduleLexer.tokenize();
			moduleDiagnostic.setSourceLines( moduleLexer.sourceLines() );
			if( moduleDiagnostic.hasErrors() ) {
				fmt::print( stderr, "error: failed to lex module \"{}\"\n", filePath );
				return false;
			}
			parser::Parser moduleParser( moduleTokens, moduleDiagnostic );
			moduleProgram = moduleParser.parse();
			if( moduleDiagnostic.hasErrors() ) {
				fmt::print( stderr, "error: failed to parse module \"{}\"\n", filePath );
				return false;
			}
			ModuleInfo& moduleInfo = this->modules[canonicalPath];
			moduleInfo.program = moduleProgram;
			moduleInfo.canonicalPath = canonicalPath;
			if( moduleProgram->module != nullptr ) {
				moduleInfo.packageName = moduleProgram->module->name;
			}
			this->resolveImports( *moduleProgram );
			if( moduleInfo.state == ModuleInfo::State::Parsed ) {
				moduleInfo.state = ModuleInfo::State::Analyzing;
				diagnostic::Engine moduleDiagnostic( this->options.maximumErrorCount, -1 );
				semantic::Analyzer moduleAnalyzer( moduleDiagnostic );
				moduleAnalyzer.importModuleTypes( this->accumulatedModuleTypes_ );
				moduleAnalyzer.importModuleSymbols( this->accumulatedModuleSymbols_ );
				moduleAnalyzer.analyzeModuleRegistration( *moduleProgram );
				std::unordered_map<std::string, semantic::TypeSharedPointer> allTypes = moduleAnalyzer.getRegisteredTypes();
				std::unordered_map<std::string, std::vector<semantic::SymbolSharedPointer>> allSymbols = moduleAnalyzer.getRegisteredSymbols();
				for( std::unordered_map<std::string, semantic::TypeSharedPointer>::iterator typeIterator = allTypes.begin(); typeIterator != allTypes.end(); ++typeIterator ) {
					if( this->accumulatedModuleTypes_.find( typeIterator->first ) == this->accumulatedModuleTypes_.end() ) {
						moduleInfo.analyzedTypes[typeIterator->first] = typeIterator->second;
						this->accumulatedModuleTypes_[typeIterator->first] = typeIterator->second;
					}
				}
				for( std::unordered_map<std::string, std::vector<semantic::SymbolSharedPointer>>::iterator symbolIterator = allSymbols.begin(); symbolIterator != allSymbols.end(); ++symbolIterator ) {
					for( const semantic::SymbolSharedPointer& symbol : symbolIterator->second ) {
						moduleInfo.analyzedSymbols[symbolIterator->first].push_back( symbol );
						this->accumulatedModuleSymbols_[symbolIterator->first].push_back( symbol );
					}
				}
				moduleInfo.state = ModuleInfo::State::Analyzed;
			}
		}
		std::set<std::string> exportedIdentifiers;
		for( ast::nodes::ExportDeclarationSharedPointer& exportPointer : moduleProgram->exports ) {
			if( exportPointer == nullptr ) {
				continue;
			}
			for( ast::nodes::ExportItem& exportItem : exportPointer->items ) {
				exportedIdentifiers.insert( exportItem.name );
			}
			if( exportPointer->declaration != nullptr ) {
				if( exportPointer->declaration->kind == ast::Node::Kind::FunctionDeclaration ) {
					exportedIdentifiers.insert( static_cast<ast::nodes::FunctionDeclaration&>( *exportPointer->declaration ).name );
				}
				else if( exportPointer->declaration->kind == ast::Node::Kind::ClassDeclaration ) {
					exportedIdentifiers.insert( static_cast<ast::nodes::ClassDeclaration&>( *exportPointer->declaration ).name );
				}
				else if( exportPointer->declaration->kind == ast::Node::Kind::StructDeclaration ) {
					exportedIdentifiers.insert( static_cast<ast::nodes::StructDeclaration&>( *exportPointer->declaration ).name );
				}
				else if( exportPointer->declaration->kind == ast::Node::Kind::EnumDeclaration ) {
					exportedIdentifiers.insert( static_cast<ast::nodes::EnumDeclaration&>( *exportPointer->declaration ).name );
				}
				else if( exportPointer->declaration->kind == ast::Node::Kind::InterfaceDeclaration ) {
					exportedIdentifiers.insert( static_cast<ast::nodes::InterfaceDeclaration&>( *exportPointer->declaration ).name );
				}
			}
		}
		std::set<std::string> selectivelyImportedIdentifiers;
		bool isSelectiveImportOperation = false;
		if( importDeclaration != nullptr && importDeclaration->isFromImport && importDeclaration->importAll == false ) {
			isSelectiveImportOperation = true;
			for( const ast::nodes::ImportItem& importItem : importDeclaration->importItems ) {
				selectivelyImportedIdentifiers.insert( importItem.name );
			}
			bool expandedBaseClasses = true;
			while( expandedBaseClasses ) {
				expandedBaseClasses = false;
				for( ast::nodes::DeclarationSharedPointer& decl : moduleProgram->declarations ) {
					if( decl == nullptr || decl->kind != ast::Node::Kind::ClassDeclaration ) {
						continue;
					}
					ast::nodes::ClassDeclaration& classDecl = static_cast<ast::nodes::ClassDeclaration&>( *decl );
					if( selectivelyImportedIdentifiers.count( classDecl.name ) == 0 ) {
						continue;
					}
					if( classDecl.baseClassType != nullptr && classDecl.baseClassType->kind == ast::Node::Kind::SimpleType ) {
						std::string baseName = static_cast<ast::nodes::SimpleTypeNode&>( *classDecl.baseClassType ).name;
						if( selectivelyImportedIdentifiers.insert( baseName ).second ) {
							expandedBaseClasses = true;
						}
					}
					else if( classDecl.baseClassType != nullptr && classDecl.baseClassType->kind == ast::Node::Kind::GenericType ) {
						std::string baseName = static_cast<ast::nodes::GenericTypeNode&>( *classDecl.baseClassType ).name;
						if( selectivelyImportedIdentifiers.insert( baseName ).second ) {
							expandedBaseClasses = true;
						}
					}
				}
			}
		}
		std::set<std::string> existingDeclarationNames;
		std::set<std::string> existingFunctionSignatures;
		for( const ast::nodes::DeclarationSharedPointer& existingDeclaration : targetProgram.declarations ) {
			if( existingDeclaration == nullptr ) {
				continue;
			}
			std::string existingIdentifier;
			if( existingDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				const ast::nodes::FunctionDeclaration& existingFunction = static_cast<const ast::nodes::FunctionDeclaration&>( *existingDeclaration );
				existingIdentifier = existingFunction.name;
				std::string sourceFile = ( existingFunction.source != nullptr ) ? existingFunction.source->filename : "";
				std::string signatureKey = fmt::format( "{}#{}#{}", existingFunction.name, existingFunction.parameters.size(), sourceFile );
				existingFunctionSignatures.insert( signatureKey );
			}
			else if( existingDeclaration->kind == ast::Node::Kind::ClassDeclaration ) {
				existingIdentifier = static_cast<const ast::nodes::ClassDeclaration&>( *existingDeclaration ).name;
			}
			else if( existingDeclaration->kind == ast::Node::Kind::StructDeclaration ) {
				existingIdentifier = static_cast<const ast::nodes::StructDeclaration&>( *existingDeclaration ).name;
			}
			else if( existingDeclaration->kind == ast::Node::Kind::EnumDeclaration ) {
				existingIdentifier = static_cast<const ast::nodes::EnumDeclaration&>( *existingDeclaration ).name;
			}
			else if( existingDeclaration->kind == ast::Node::Kind::InterfaceDeclaration ) {
				existingIdentifier = static_cast<const ast::nodes::InterfaceDeclaration&>( *existingDeclaration ).name;
			}
			else if( existingDeclaration->kind == ast::Node::Kind::ExternDeclaration ) {
				existingIdentifier = static_cast<const ast::nodes::ExternDeclaration&>( *existingDeclaration ).name;
			}
			else if( existingDeclaration->kind == ast::Node::Kind::TypeAliasDeclaration ) {
				existingIdentifier = static_cast<const ast::nodes::TypeAliasDeclaration&>( *existingDeclaration ).name;
			}
			else if( existingDeclaration->kind == ast::Node::Kind::ConstantDeclaration ) {
				existingIdentifier = static_cast<const ast::nodes::ConstantDeclaration&>( *existingDeclaration ).name;
			}
			if( existingIdentifier.empty() == false ) {
				existingDeclarationNames.insert( existingIdentifier );
			}
		}
		for( ast::nodes::DeclarationSharedPointer& moduleDeclaration : moduleProgram->declarations ) {
			if( moduleDeclaration == nullptr ) {
				continue;
			}
			std::string declarationIdentifier;
			if( moduleDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				declarationIdentifier = static_cast<ast::nodes::FunctionDeclaration&>( *moduleDeclaration ).name;
			}
			else if( moduleDeclaration->kind == ast::Node::Kind::ClassDeclaration ) {
				declarationIdentifier = static_cast<ast::nodes::ClassDeclaration&>( *moduleDeclaration ).name;
			}
			else if( moduleDeclaration->kind == ast::Node::Kind::StructDeclaration ) {
				declarationIdentifier = static_cast<ast::nodes::StructDeclaration&>( *moduleDeclaration ).name;
			}
			else if( moduleDeclaration->kind == ast::Node::Kind::EnumDeclaration ) {
				declarationIdentifier = static_cast<ast::nodes::EnumDeclaration&>( *moduleDeclaration ).name;
			}
			else if( moduleDeclaration->kind == ast::Node::Kind::InterfaceDeclaration ) {
				declarationIdentifier = static_cast<ast::nodes::InterfaceDeclaration&>( *moduleDeclaration ).name;
			}
			else if( moduleDeclaration->kind == ast::Node::Kind::ExternDeclaration ) {
				declarationIdentifier = static_cast<ast::nodes::ExternDeclaration&>( *moduleDeclaration ).name;
			}
			else if( moduleDeclaration->kind == ast::Node::Kind::TypeAliasDeclaration ) {
				declarationIdentifier = static_cast<ast::nodes::TypeAliasDeclaration&>( *moduleDeclaration ).name;
			}
			else if( moduleDeclaration->kind == ast::Node::Kind::ConstantDeclaration ) {
				declarationIdentifier = static_cast<ast::nodes::ConstantDeclaration&>( *moduleDeclaration ).name;
			}
			bool isVisibleIdentifier = moduleDeclaration->access == ast::AccessModifier::Public || exportedIdentifiers.count( declarationIdentifier );
			if( isVisibleIdentifier == false && moduleDeclaration->access != ast::AccessModifier::Default ) {
				continue;
			}
			if( isSelectiveImportOperation && declarationIdentifier.empty() == false && selectivelyImportedIdentifiers.count( declarationIdentifier ) == 0 ) {
				if( moduleDeclaration->kind != ast::Node::Kind::ConstantDeclaration && moduleDeclaration->kind != ast::Node::Kind::FunctionDeclaration && moduleDeclaration->kind != ast::Node::Kind::ExternDeclaration ) {
					continue;
				}
			}
			if( moduleDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				const ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<const ast::nodes::FunctionDeclaration&>( *moduleDeclaration );
				std::string sourceFile = ( functionDeclaration.source != nullptr ) ? functionDeclaration.source->filename : "";
				std::string signatureKey = fmt::format( "{}#{}#{}", functionDeclaration.name, functionDeclaration.parameters.size(), sourceFile );
				if( existingFunctionSignatures.count( signatureKey ) > 0 ) {
					continue;
				}
				existingFunctionSignatures.insert( signatureKey );
			}
			else if( declarationIdentifier.empty() == false && existingDeclarationNames.count( declarationIdentifier ) > 0 ) {
				continue;
			}
			if( declarationIdentifier.empty() == false ) {
				existingDeclarationNames.insert( declarationIdentifier );
			}
			targetProgram.declarations.push_back( moduleDeclaration );
		}
		return true;
	}
	
	bool Driver::loadPrelude( ast::nodes::Program& program ) {
		std::string modulesDirectoryPath = this->findModulesDirectory();
		if( modulesDirectoryPath.empty() ) {
			if( this->options.verbose ) {
				spdlog::warn( "No modules directory found, skipping prelude loading" );
			}
			return true;
		}
		if( this->options.verbose ) {
			spdlog::info( "Loading prelude from: {}", modulesDirectoryPath );
		}
		std::vector<std::string> corePreludeModulePaths = {
			"errors/error.urn",
			"errors/exception.urn",
			"errors/throwable.urn",
			"errors/traceback.urn",
			"errors/warning.urn"
		};
		for( std::string& relativeModulePath : corePreludeModulePaths ) {
			std::filesystem::path absoluteModulePath = std::filesystem::path( modulesDirectoryPath ) / relativeModulePath;
			if( std::filesystem::exists( absoluteModulePath ) ) {
				size_t previousDeclarationCount = program.declarations.size();
				if( this->loadModule( absoluteModulePath.string(), program ) == false ) {
					if( this->options.verbose ) {
						spdlog::warn( "Failed to load prelude module: {}", relativeModulePath );
					}
				}
				for( size_t declarationIndex = previousDeclarationCount; declarationIndex < program.declarations.size(); declarationIndex++ ) {
					if( program.declarations[declarationIndex] != nullptr ) {
						program.declarations[declarationIndex]->isBuiltin = true;
					}
				}
			}
		}
		return true;
	}
	
	// int Driver::optimize() {
	// }
	
	// int Driver::parseAST() {
	// }
	
	int Driver::readSource() {
		std::ifstream sourceFile( this->options.output.source );
		if( sourceFile.is_open() == false ) {
			fmt::print( stderr, "error: could not open file \"{}\"\n", this->options.output.source );
			return 1;
		}
		std::ostringstream sourceContentStream;
		sourceContentStream << sourceFile.rdbuf();
		this->source = sourceContentStream.str();
		if( this->options.verbose ) {
			spdlog::info( "Read {} bytes from \"{}\"", this->source.size(), this->options.output.source );
		}
		return 0;
	}
	
	bool Driver::resolveImports( ast::nodes::Program& program ) {
		for( ast::nodes::ImportDeclarationSharedPointer& importDeclaration : program.imports ) {
			if( importDeclaration == nullptr ) {
				continue;
			}
			std::string resolvedModulePath = this->resolveModulePath( importDeclaration->path );
			if( resolvedModulePath.empty() ) {
				std::string formattedPathIdentifier;
				for( size_t pathIndex = 0; pathIndex < importDeclaration->path.size(); pathIndex++ ) {
					if( pathIndex > 0 ) {
						formattedPathIdentifier+= ".";
					}
					formattedPathIdentifier+= importDeclaration->path[pathIndex];
				}
				std::string errorMessage = fmt::format( "cannot resolve module \"{}\"", formattedPathIdentifier );
				this->diagnostic.error( importDeclaration->source, errorMessage );
				continue;
			}
			if( this->loadModule( resolvedModulePath, program, importDeclaration.get() ) == false ) {
				return false;
			}
		}
		return true;
	}
	
	void Driver::resolveIncludePathPrefixes() {
		if( this->includePathPrefixesCached_ ) {
			return;
		}
		this->includePathPrefixesCached_ = true;
		for( const std::string& currentIncludePath : this->options.includePaths ) {
			std::filesystem::path modFilePath = std::filesystem::path( currentIncludePath ) / "__mod__.urn";
			if( std::filesystem::exists( modFilePath ) == false ) {
				continue;
			}
			std::ifstream modFileStream( modFilePath.string() );
			if( modFileStream.is_open() == false ) {
				continue;
			}
			std::string currentLine;
			while( std::getline( modFileStream, currentLine ) ) {
				size_t packageKeywordPosition = currentLine.find( "package " );
				if( packageKeywordPosition != std::string::npos ) {
					std::string packageName = currentLine.substr( packageKeywordPosition + 8 );
					while( packageName.empty() == false && ( packageName.back() == ' ' || packageName.back() == '\t' || packageName.back() == '\r' || packageName.back() == '\n' ) ) {
						packageName.pop_back();
					}
					if( packageName.empty() == false ) {
						this->includePathPackagePrefix_[currentIncludePath] = packageName;
					}
					break;
				}
			}
		}
	}

	std::string Driver::targetArchSegment() const {
		std::string triple = this->options.targetTriple.empty()
			? llvm::sys::getDefaultTargetTriple()
			: this->options.targetTriple;
		llvm::Triple parsedTriple( triple );
		switch( parsedTriple.getArch() ) {
			case llvm::Triple::x86_64:
				return "x86-64";
			case llvm::Triple::aarch64:
			case llvm::Triple::aarch64_be:
				return "aarch64";
			case llvm::Triple::riscv64:
				return "riscv64";
			case llvm::Triple::arm:
			case llvm::Triple::armeb:
				return "arm";
			default:
				return parsedTriple.getArchName().str();
		}
	}

	std::string Driver::resolveModulePath( const std::vector<std::string>& modulePath ) {
		if( modulePath.empty() ) {
			return "";
		}

		std::function<std::string( const std::filesystem::path& )> checkPathExists = []( const std::filesystem::path& candidatePath ) -> std::string {
			if( std::filesystem::exists( candidatePath ) ) {
				return candidatePath.string();
			}
			return "";
		};

		bool isStdlibImport = ( modulePath[0] == "uranite" );

		std::vector<std::string> effectivePathSegments = modulePath;

		// Rewrite the reserved "native" segment onto the active target architecture
		// directory (e.g. uranite.os.arch.native.syscall -> .../os/arch/aarch64/syscall).
		// This lets arch-neutral stdlib modules import architecture-specific primitives
		// without hardcoding the host architecture.
		if( isStdlibImport ) {
			std::string archSegment = this->targetArchSegment();
			for( std::string& pathSegment : effectivePathSegments ) {
				if( pathSegment == "native" ) {
					pathSegment = archSegment;
				}
			}
		}
		if( isStdlibImport ) {
			effectivePathSegments.erase( effectivePathSegments.begin() );
		}
		
		std::string relativePathBase;
		for( size_t segmentIndex = 0; segmentIndex < effectivePathSegments.size(); segmentIndex++ ) {
			if( segmentIndex > 0 ) {
				relativePathBase = fmt::format( "{}/{}", relativePathBase, effectivePathSegments[segmentIndex] );
			}
			else {
				relativePathBase = effectivePathSegments[segmentIndex];
			}
		}
		
		std::string relativePathWithExtension = fmt::format( "{}.urn", relativePathBase );
		
		std::string directoryPatternPath;
		if( effectivePathSegments.empty() == false ) {
			std::string lastNameSegment = effectivePathSegments.back();
			if( lastNameSegment.empty() == false ) {
				lastNameSegment[0] = static_cast<char>( std::toupper( static_cast<unsigned char>( lastNameSegment[0] ) ) );
			}
			directoryPatternPath = fmt::format( "{}/{}.urn", relativePathBase, lastNameSegment );
		}
		
		std::string moduleInitializerPath;
		if( effectivePathSegments.empty() == false ) {
			moduleInitializerPath = fmt::format( "{}/__mod__.urn", relativePathBase );
		}
		
		std::function<std::string( const std::filesystem::path& )> tryAllModuleVariants = [&]( const std::filesystem::path& baseSearchPath ) -> std::string {
			std::string resolvedPath = checkPathExists( baseSearchPath / relativePathWithExtension );
			if( resolvedPath.empty() == false ) {
				return resolvedPath;
			}
			if( directoryPatternPath.empty() == false ) {
				resolvedPath = checkPathExists( baseSearchPath / directoryPatternPath );
				if( resolvedPath.empty() == false ) {
					return resolvedPath;
				}
			}
			if( moduleInitializerPath.empty() == false ) {
				resolvedPath = checkPathExists( baseSearchPath / moduleInitializerPath );
				if( resolvedPath.empty() == false ) {
					return resolvedPath;
				}
			}
			return "";
		};
		std::string finalResolvedResult;
		if( isStdlibImport ) {
			std::string modulesDirectoryLocation = this->findModulesDirectory();
			if( modulesDirectoryLocation.empty() == false ) {
				finalResolvedResult = tryAllModuleVariants( std::filesystem::path( modulesDirectoryLocation ) );
				if( finalResolvedResult.empty() == false ) {
					return finalResolvedResult;
				}
			}
		}
		else {
			std::filesystem::path mainSourceDirectory = std::filesystem::path( this->options.output.source.empty() ? "." : this->options.output.source ).parent_path();
			finalResolvedResult = tryAllModuleVariants( mainSourceDirectory );
			if( finalResolvedResult.empty() == false ) {
				return finalResolvedResult;
			}
			for( const std::string& currentIncludePath : this->options.includePaths ) {
				finalResolvedResult = tryAllModuleVariants( std::filesystem::path( currentIncludePath ) );
				if( finalResolvedResult.empty() == false ) {
					return finalResolvedResult;
				}
			}
			this->resolveIncludePathPrefixes();
			if( modulePath.empty() == false ) {
				std::string firstSegment = modulePath[0];
				for( const auto& prefixEntry : this->includePathPackagePrefix_ ) {
					std::string packagePrefix = prefixEntry.second;
					size_t dotPosition = packagePrefix.find( '.' );
					std::string rootPackageName = ( dotPosition != std::string::npos ) ? packagePrefix.substr( 0, dotPosition ) : packagePrefix;
					if( firstSegment == rootPackageName ) {
						std::vector<std::string> strippedPath( modulePath.begin() + 1, modulePath.end() );
						if( strippedPath.empty() == false ) {
							std::string strippedRelativePath;
							for( size_t segmentIndex = 0; segmentIndex < strippedPath.size(); segmentIndex++ ) {
								if( segmentIndex > 0 ) {
									strippedRelativePath = fmt::format( "{}/{}", strippedRelativePath, strippedPath[segmentIndex] );
								}
								else {
									strippedRelativePath = strippedPath[segmentIndex];
								}
							}
							std::string strippedWithExtension = fmt::format( "{}.urn", strippedRelativePath );
							std::string strippedDirectoryPattern;
							if( strippedPath.empty() == false ) {
								std::string lastSegment = strippedPath.back();
								if( lastSegment.empty() == false ) {
									lastSegment[0] = static_cast<char>( std::toupper( static_cast<unsigned char>( lastSegment[0] ) ) );
								}
								strippedDirectoryPattern = fmt::format( "{}/{}.urn", strippedRelativePath, lastSegment );
							}
							std::string strippedModInit = fmt::format( "{}/__mod__.urn", strippedRelativePath );
							std::filesystem::path includeBasePath( prefixEntry.first );
							std::string strippedResult = checkPathExists( includeBasePath / strippedWithExtension );
							if( strippedResult.empty() && strippedDirectoryPattern.empty() == false ) {
								strippedResult = checkPathExists( includeBasePath / strippedDirectoryPattern );
							}
							if( strippedResult.empty() ) {
								strippedResult = checkPathExists( includeBasePath / strippedModInit );
							}
							if( strippedResult.empty() == false ) {
								return strippedResult;
							}
						}
					}
				}
			}
		}
		return "";
	}
	
	int Driver::run() {
		if( int returnCode = this->readSource() ) {
			return returnCode;
		}
		try {
			if( this->options.verbose ) {
				spdlog::info( "Stage 1: Lexical Analysis" );
			}
			lexer::Lexer lexer( this->source, this->options.output.source, this->diagnostic );
			std::vector<token::Token> tokenStream = lexer.tokenize();
			this->diagnostic.setSourceLines( lexer.sourceLines() );
			if( this->diagnostic.hasErrors() ) {
				fmt::print( stderr, "Compilation failed with {} error(s) during lexing.\n", this->diagnostic.errorCount() );
				return 1;
			}
			if( this->options.dumpTokens ) {
				fmt::print( "=== Token Stream ===\n" );
				for( const token::Token& currentToken : tokenStream ) {
					fmt::print( "  [{:>4}:{:<3}] {:<15} \"{}\"\n", currentToken.source->location->line, currentToken.source->location->column, token::toString( currentToken.type ), currentToken.value );
				}
				if( this->options.output.kind == Output::Kind::Tokens ) {
					return 0;
				}
			}
			if( this->options.verbose ) {
				spdlog::info( "Stage 2: Parsing / AST Generation" );
			}
			parser::Parser parser( tokenStream, this->diagnostic );
			ast::nodes::ProgramSharedPointer programRoot = parser.parse();
			if( this->diagnostic.hasErrors() ) {
				fmt::print( stderr, "Compilation failed with {} error(s) during parsing.\n", this->diagnostic.errorCount() );
				return 1;
			}
			if( this->options.verbose ) {
				spdlog::info( "Stage 2.5: Prelude + Module Resolution" );
			}
			this->loadPrelude( *programRoot );
			if( this->resolveImports( *programRoot ) == false ) {
				if( this->diagnostic.hasErrors() ) {
					fmt::print( stderr, "Compilation failed with {} error(s) during module resolution.\n", this->diagnostic.errorCount() );
					return 1;
				}
			}
			bool needsArgsModule = false;
			bool needsKwargsModule = false;
			for( ast::nodes::DeclarationSharedPointer& decl : programRoot->declarations ) {
				if( decl == nullptr ) {
					continue;
				}
				std::vector<ast::nodes::DeclarationSharedPointer>* methodsList = nullptr;
				if( decl->kind == ast::Node::Kind::FunctionDeclaration ) {
					ast::nodes::FunctionDeclaration& funcDecl = static_cast<ast::nodes::FunctionDeclaration&>( *decl );
					for( ast::nodes::FunctionParameterSharedPointer& param : funcDecl.parameters ) {
						if( param->isVariadic ) needsArgsModule = true;
						if( param->isKeyword ) needsKwargsModule = true;
					}
				}
				else if( decl->kind == ast::Node::Kind::ClassDeclaration ) {
					methodsList = &static_cast<ast::nodes::ClassDeclaration&>( *decl ).methods;
				}
				else if( decl->kind == ast::Node::Kind::StructDeclaration ) {
					methodsList = &static_cast<ast::nodes::StructDeclaration&>( *decl ).methods;
				}
				else if( decl->kind == ast::Node::Kind::InterfaceDeclaration ) {
					methodsList = &static_cast<ast::nodes::InterfaceDeclaration&>( *decl ).methods;
				}
				if( methodsList != nullptr ) {
					for( ast::nodes::DeclarationSharedPointer& methodDecl : *methodsList ) {
						if( methodDecl && methodDecl->kind == ast::Node::Kind::FunctionDeclaration ) {
							ast::nodes::FunctionDeclaration& funcDecl = static_cast<ast::nodes::FunctionDeclaration&>( *methodDecl );
							for( ast::nodes::FunctionParameterSharedPointer& param : funcDecl.parameters ) {
								if( param->isVariadic ) needsArgsModule = true;
								if( param->isKeyword ) needsKwargsModule = true;
							}
						}
					}
				}
				if( needsArgsModule && needsKwargsModule ) {
					break;
				}
			}
			if( needsArgsModule || needsKwargsModule ) {
				std::string modulesDir = this->findModulesDirectory();
				if( modulesDir.empty() == false ) {
					if( needsArgsModule ) {
						std::filesystem::path argsModulePath = std::filesystem::path( modulesDir ) / "collection" / "args.urn";
						if( std::filesystem::exists( argsModulePath ) ) {
							this->loadModule( argsModulePath.string(), *programRoot );
						}
					}
					if( needsKwargsModule ) {
						std::filesystem::path kwargsModulePath = std::filesystem::path( modulesDir ) / "collection" / "kwargs.urn";
						if( std::filesystem::exists( kwargsModulePath ) ) {
							this->loadModule( kwargsModulePath.string(), *programRoot );
						}
					}
				}
			}
			bool needsAsyncRuntime = false;
			for( ast::nodes::DeclarationSharedPointer& decl : programRoot->declarations ) {
				if( decl == nullptr ) {
					continue;
				}
				if( decl->kind == ast::Node::Kind::FunctionDeclaration ) {
					ast::nodes::FunctionDeclaration& funcDecl = static_cast<ast::nodes::FunctionDeclaration&>( *decl );
					if( funcDecl.isAsync ) {
						needsAsyncRuntime = true;
						break;
					}
				}
				std::vector<ast::nodes::DeclarationSharedPointer>* methodsList = nullptr;
				if( decl->kind == ast::Node::Kind::ClassDeclaration ) {
					methodsList = &static_cast<ast::nodes::ClassDeclaration&>( *decl ).methods;
				}
				else if( decl->kind == ast::Node::Kind::StructDeclaration ) {
					methodsList = &static_cast<ast::nodes::StructDeclaration&>( *decl ).methods;
				}
				if( methodsList != nullptr ) {
					for( ast::nodes::DeclarationSharedPointer& methodDecl : *methodsList ) {
						if( methodDecl && methodDecl->kind == ast::Node::Kind::FunctionDeclaration ) {
							ast::nodes::FunctionDeclaration& funcDecl = static_cast<ast::nodes::FunctionDeclaration&>( *methodDecl );
							if( funcDecl.isAsync ) {
								needsAsyncRuntime = true;
								break;
							}
						}
					}
				}
				if( needsAsyncRuntime ) {
					break;
				}
			}
			if( needsAsyncRuntime ) {
				std::string modulesDir = this->findModulesDirectory();
				if( modulesDir.empty() == false ) {
					std::filesystem::path asyncRuntimePath = std::filesystem::path( modulesDir ) / "async" / "runtime.urn";
					if( std::filesystem::exists( asyncRuntimePath ) ) {
						this->loadModule( asyncRuntimePath.string(), *programRoot );
						if( this->options.verbose ) {
							spdlog::info( "Auto-imported async runtime module" );
						}
					}
				}
			}
			
			{
				std::string modulesDir = this->findModulesDirectory();
				if( modulesDir.empty() == false ) {
					std::filesystem::path mathErrorsPath = std::filesystem::path( modulesDir ) / "math" / "errors.urn";
					if( std::filesystem::exists( mathErrorsPath ) ) {
						this->loadModule( mathErrorsPath.string(), *programRoot );
						if( this->options.verbose ) {
							spdlog::info( "Auto-imported math errors module" );
						}
					}
				}
			}
			
			if( this->options.dumpAST ) {
				visitors::ASTPrinter astPrinterInstance;
				fmt::print( "=== Abstract Syntax Tree ===\n" );
				fmt::print( "{}\n", astPrinterInstance.print( *programRoot ) );
				if( this->options.output.kind == Output::Kind::AST ) {
					return 0;
				}
			}
			if( this->options.verbose ) {
				spdlog::info( "Stage 3: Semantic Analysis + Type System" );
			}
			semantic::Analyzer semanticAnalyzer( this->diagnostic );
			semanticAnalyzer.importModuleTypes( this->accumulatedModuleTypes_ );
			semanticAnalyzer.importModuleSymbols( this->accumulatedModuleSymbols_ );
			if( semanticAnalyzer.analyze( *programRoot ) == false ) {
				fmt::print( stderr, "Compilation failed with {} error(s) during semantic analysis.\n", this->diagnostic.errorCount() );
				return 1;
			}
			if( this->options.verbose ) {
				spdlog::info( "Semantic analysis passed" );
			}
			if( this->options.verbose ) {
				spdlog::info( "Stage 4: Borrow Checker / Ownership Analysis" );
			}
			semantic::BorrowChecker borrowCheckerInstance( this->diagnostic );
			borrowCheckerInstance.check( *programRoot );
			if( this->diagnostic.hasErrors() ) {
				fmt::print( stderr, "Compilation failed with {} error(s) during borrow checking.\n", this->diagnostic.errorCount() );
				return 1;
			}
			if( this->options.verbose ) {
				spdlog::info( "Borrow check passed" );
			}
			if( this->options.verbose || this->options.dumpHIR || this->options.dumpMIR || this->options.useMIR ) {
				if( this->options.verbose ) {
					spdlog::info( "Stage 4.5: HIR Lowering" );
				}
				ir::hir::HIRLowering hirLoweringPass( semanticAnalyzer, this->diagnostic );
				std::shared_ptr<ir::hir::HIRModule> hirModule = hirLoweringPass.lower( *programRoot );
				ir::hir::HIRValidator hirValidatorPass( this->diagnostic );
				bool hirIsValid = hirValidatorPass.validate( *hirModule );
				if( this->options.verbose ) {
					if( hirIsValid ) {
						spdlog::info( "HIR validation passed" );
					}
					else {
						spdlog::warn( "HIR validation found {} issue(s)", hirValidatorPass.validationErrors().size() );
					}
				}
				if( this->options.dumpHIR ) {
					ir::hir::HIRPrinter hirPrinter;
					std::string hirOutput = hirPrinter.print( *hirModule );
					fmt::print( "=== HIR ===\n{}\n", hirOutput );
					return 0;
				}
				if( this->options.verbose || this->options.dumpMIR || this->options.useMIR ) {
					if( this->options.verbose ) {
						spdlog::info( "Stage 4.6: MIR Lowering" );
					}
					ir::mir::MIRLowering mirLoweringPass( this->diagnostic );
					std::shared_ptr<ir::mir::MIRModuleDefinition> mirModule = mirLoweringPass.lower( *hirModule );
					if( this->options.verbose ) {
						size_t totalBlocks = 0;
						for( const std::shared_ptr<ir::mir::MIRFunctionDefinition>& mirFunction : mirModule->functionDefinitions ) {
							totalBlocks+= mirFunction->controlFlowBlocks.size();
						}
						spdlog::info( "MIR: {} functions, {} basic blocks", mirModule->functionDefinitions.size(), totalBlocks );
					}
					if( this->options.verbose ) {
						spdlog::info( "Stage 4.7: MIR Liveness Analysis" );
					}
					ir::mir::MIRLivenessAnalysis mirLivenessPass;
					for( std::shared_ptr<ir::mir::MIRFunctionDefinition>& mirFunction : mirModule->functionDefinitions ) {
						if( mirFunction != nullptr ) {
							mirLivenessPass.analyze( *mirFunction );
						}
					}
					if( this->options.verbose ) {
						spdlog::info( "Stage 4.8: MIR Borrow Checker" );
					}
					ir::mir::MIRBorrowChecker mirBorrowChecker( this->diagnostic );
					for( std::shared_ptr<ir::mir::MIRFunctionDefinition>& mirFunction : mirModule->functionDefinitions ) {
						if( mirFunction != nullptr ) {
							mirBorrowChecker.check( *mirFunction );
						}
					}
					if( this->options.verbose ) {
						spdlog::info( "MIR borrow check: {} violation(s)", mirBorrowChecker.violationCount() );
					}
					if( this->options.verbose ) {
						spdlog::info( "Stage 4.9: MIR Optimization" );
					}
					ir::mir::MIROptimizer mirOptimizer;
					mirOptimizer.optimize( *mirModule );
					if( this->options.verbose ) {
						spdlog::info( "MIR optimizer: {} instructions removed, {} blocks removed",
							mirOptimizer.removedInstructionCount(), mirOptimizer.removedBlockCount() );
					}
					if( this->options.dumpMIR ) {
						ir::mir::MIRPrinter mirPrinter;
						std::string mirOutput = mirPrinter.print( *mirModule );
						fmt::print( "=== MIR ===\n{}\n", mirOutput );
						return 0;
					}
					if( this->options.useMIR ) {
						if( this->options.verbose ) {
							spdlog::info( "Stage 5.0: MIR → LLVM IR Code Generation" );
						}
						ir::mir::MIRCodegen mirCodegenInstance( semanticAnalyzer, this->diagnostic );
						if( mirCodegenInstance.generate( *mirModule ) == false ) {
							fmt::print( stderr, "Compilation failed during MIR code generation.\n" );
							return 1;
						}
						if( this->options.verbose ) {
							spdlog::info( "MIR codegen: LLVM IR generated successfully" );
						}
						if( this->options.dumpIR ) {
							fmt::print( "=== LLVM IR (MIR) ===\n" );
							mirCodegenInstance.getModule()->print( llvm::errs(), nullptr );
						}
						std::string mirOutputFilePath = this->options.output.target;
						if( this->options.output.kind == Output::Kind::LLVMIR ) {
							if( mirOutputFilePath.empty() ) {
								mirOutputFilePath = fmt::format( "{}.ll", this->options.output.source );
							}
							if( mirCodegenInstance.writeIR( mirOutputFilePath ) == false ) {
								return 1;
							}
							return 0;
						}
						std::string mirTempIrFile = fmt::format( "/tmp/uranite-mir-{}.ll", getpid() );
						mirCodegenInstance.writeIR( mirTempIrFile );
						std::string mirTempObjFile = fmt::format( "/tmp/uranite-mir-{}.o", getpid() );
						std::string llcMirCommand = fmt::format( _URANITE_LLC_ " -O2 -relocation-model=pic -filetype=obj -o {} {}", mirTempObjFile, mirTempIrFile );
						if( this->options.verbose ) {
							spdlog::info( "Running: {}", llcMirCommand );
						}
						int llcMirReturnCode = system( llcMirCommand.c_str() );
						std::remove( mirTempIrFile.c_str() );
						if( llcMirReturnCode != 0 ) {
							fmt::print( stderr, "error: llc failed with code {}\n", llcMirReturnCode );
							return 1;
						}
						if( mirOutputFilePath.empty() ) {
							mirOutputFilePath = "a.out";
						}
						std::string ccMirCommand = fmt::format( "cc {} -o {} -lm", mirTempObjFile, mirOutputFilePath );
						if( this->options.verbose ) {
							spdlog::info( "Running: {}", ccMirCommand );
						}
						int ccMirReturnCode = system( ccMirCommand.c_str() );
						std::remove( mirTempObjFile.c_str() );
						if( ccMirReturnCode != 0 ) {
							fmt::print( stderr, "error: linker failed with code {}\n", ccMirReturnCode );
							return 1;
						}
						if( this->options.verbose ) {
							spdlog::info( "MIR codegen: compiled to \"{}\"", mirOutputFilePath );
						}
						fmt::print( "Compiled successfully to \"{}\"\n", mirOutputFilePath );
						return 0;
					}
				}
			}
			if( this->options.verbose ) {
				spdlog::info( "Stage 5: Optimization Passes" );
			}
			optimizer::Optimizer codeOptimizer( this->diagnostic, this->options.optimization );
			codeOptimizer.optimize( *programRoot );
			if( this->options.verbose ) {
				const optimizer::Statistic& optimizationStatistics = codeOptimizer.stats();
				spdlog::info( "Optimizations: {} TCO, {} constexpr, {} devirtualized, {} dead branches, {} unused vars, {} unused funcs, {} unreachable stmts", optimizationStatistics.tailCallsOptimized, optimizationStatistics.constantExpressionResionEvaluated, optimizationStatistics.functionsDevirtualized, optimizationStatistics.deadBranchesEliminated, optimizationStatistics.unusedVariablesEliminated, optimizationStatistics.unusedFunctionsEliminated, optimizationStatistics.unreachableStatementsEliminated );
			}
			if( this->options.verbose ) {
				spdlog::info( "Stage 6: LLVM IR Code Generation" );
			}
			codegen::LLVMCodegen llvmCodegenInstance( semanticAnalyzer, this->diagnostic );
			if( llvmCodegenInstance.generate( *programRoot ) == false ) {
				fmt::print( stderr, "Compilation failed during code generation.\n" );
				return 1;
			}
			if( this->options.verbose ) {
				spdlog::info( "LLVM IR generated successfully" );
			}
			if( this->options.dumpIR ) {
				fmt::print( "=== LLVM IR ===\n" );
				llvmCodegenInstance.dump();
			}
			std::string finalOutputFilePath = this->options.output.target;
			switch( this->options.output.kind ) {
				case Output::Kind::LLVMIR: {
					if( finalOutputFilePath.empty() ) {
						finalOutputFilePath = fmt::format( "{}.ll", this->options.output.source );
					}
					if( llvmCodegenInstance.writeIR( finalOutputFilePath ) == false ) {
						return 1;
					}
					if( this->options.verbose ) {
						spdlog::info( "LLVM IR written to \"{}\"", finalOutputFilePath );
					}
					return 0;
				}
				case Output::Kind::Object: {
					if( finalOutputFilePath.empty() ) {
						finalOutputFilePath = this->options.output.source;
						size_t lastDotPosition = finalOutputFilePath.rfind( '.' );
						if( lastDotPosition != std::string::npos ) {
							finalOutputFilePath = finalOutputFilePath.substr( 0, lastDotPosition );
						}
						finalOutputFilePath = fmt::format( "{}.o", finalOutputFilePath );
					}
					std::string temporaryIrFile = fmt::format( "{}.ll", finalOutputFilePath );
					llvmCodegenInstance.writeIR( temporaryIrFile );
					std::string llcCommand = fmt::format( _URANITE_LLC_ " -O2 -filetype=obj -o {} {}", finalOutputFilePath, temporaryIrFile );
					if( this->options.verbose ) {
						spdlog::info( "Running: {}", llcCommand );
					}
					int shellReturnCode = system( llcCommand.c_str() );
					std::remove( temporaryIrFile.c_str() );
					if( shellReturnCode != 0 ) {
						fmt::print( stderr, "error: llc failed with code {}\n", shellReturnCode );
						return 1;
					}
					if( this->options.verbose ) {
						spdlog::info( "Object file written to \"{}\"", finalOutputFilePath );
					}
					return 0;
				}
				case Output::Kind::Executable: {
					if( finalOutputFilePath.empty() ) {
						finalOutputFilePath = this->options.output.source;
						size_t lastDotPosition = finalOutputFilePath.rfind( '.' );
						if( lastDotPosition != std::string::npos ) {
							finalOutputFilePath = finalOutputFilePath.substr( 0, lastDotPosition );
						}
					}
					std::string temporaryIrFile = fmt::format( "{}.tmp.ll", finalOutputFilePath );
					std::string temporaryOptFile = fmt::format( "{}.tmp.opt.ll", finalOutputFilePath );
					std::string temporaryObjectFile = fmt::format( "{}.tmp.o", finalOutputFilePath );
					llvmCodegenInstance.writeIR( temporaryIrFile );
					std::string optCommand = fmt::format( _URANITE_OPT_ " -O2 -S -o {} {}", temporaryOptFile, temporaryIrFile );
					if( this->options.verbose ) {
						spdlog::info( "Running: {}", optCommand );
					}
					int optReturnCode = system( optCommand.c_str() );
					std::string llcInputFile = temporaryIrFile;
					if( optReturnCode == 0 ) {
						llcInputFile = temporaryOptFile;
					}
					std::string llcCompileCommand = fmt::format( _URANITE_LLC_ " -O2 -filetype=obj -relocation-model=pic -o {} {}", temporaryObjectFile, llcInputFile );
					if( this->options.verbose ) {
						spdlog::info( "Running: {}", llcCompileCommand );
					}
					int compileReturnCode = system( llcCompileCommand.c_str() );
					if( compileReturnCode != 0 ) {
						std::remove( temporaryIrFile.c_str() );
						std::remove( temporaryOptFile.c_str() );
						fmt::print( stderr, "error: llc failed\n" );
						return 1;
					}
					std::string linkerCommand = fmt::format( "cc -o {}", finalOutputFilePath );
					linkerCommand = fmt::format( "{} {}", linkerCommand, temporaryObjectFile );
					auto scanRuntimeDirectory = [&]( const std::filesystem::path& runtimeDirectory ) -> bool {
						if( std::filesystem::exists( runtimeDirectory ) == false || std::filesystem::is_directory( runtimeDirectory ) == false ) {
							return false;
						}
						bool foundAnyLibrary = false;
						for( const std::filesystem::directory_entry& directoryEntry : std::filesystem::directory_iterator( runtimeDirectory ) ) {
							if( directoryEntry.is_regular_file() == false ) {
								continue;
							}
							std::string entryFilename = directoryEntry.path().filename().string();
							if( entryFilename.find( "liburanite-" ) == 0 && entryFilename.size() > 2 &&
								entryFilename.substr( entryFilename.size() - 2 ) == ".a" ) {
								linkerCommand = fmt::format( "{} {}", linkerCommand, directoryEntry.path().string() );
								foundAnyLibrary = true;
							}
						}
						return foundAnyLibrary;
					};
					bool runtimeLibrariesResolved = false;
					std::string compileTimeCRuntimeDir = _URANITE_C_RUNTIME_DIR_;
					if( compileTimeCRuntimeDir.empty() == false ) {
						runtimeLibrariesResolved = scanRuntimeDirectory( std::filesystem::path( compileTimeCRuntimeDir ) );
					}
					if( runtimeLibrariesResolved == false ) {
						std::string executableDirectory = getExecutableDirectory();
						if( executableDirectory.empty() == false ) {
							std::filesystem::path exeRelativeRuntimePath = std::filesystem::path( executableDirectory ) / ".." / "lib" / "uranite" / "runtime";
							if( std::filesystem::exists( exeRelativeRuntimePath ) ) {
								runtimeLibrariesResolved = scanRuntimeDirectory( std::filesystem::canonical( exeRelativeRuntimePath ) );
							}
						}
					}
					if( runtimeLibrariesResolved == false ) {
						std::filesystem::path runtimeSearchPath = std::filesystem::current_path();
						for( int searchDepth = 0; searchDepth < 5; searchDepth++ ) {
							if( scanRuntimeDirectory( runtimeSearchPath / "build" / "runtime" ) ) {
								runtimeLibrariesResolved = true;
								break;
							}
							if( scanRuntimeDirectory( runtimeSearchPath / "runtime" ) ) {
								runtimeLibrariesResolved = true;
								break;
							}
							if( runtimeSearchPath.has_parent_path() && runtimeSearchPath.parent_path() != runtimeSearchPath ) {
								runtimeSearchPath = runtimeSearchPath.parent_path();
							}
							else {
								break;
							}
						}
					}
					linkerCommand = fmt::format( "{} -lpthread -lrt -lm", linkerCommand );
					bool needsFfiRuntime = false;
					for( std::pair<const std::string, ModuleInfo>& moduleEntry : this->modules ) {
						if( moduleEntry.first.find( "/ffi/" ) != std::string::npos ||
							moduleEntry.second.packageName.find( "uranite.ffi" ) == 0 ) {
							needsFfiRuntime = true;
							break;
						}
					}
					if( needsFfiRuntime ) {
						linkerCommand = fmt::format( "{} -ldl", linkerCommand );
					}
					for( const std::string& additionalLibrary : this->options.linkLibraries ) {
						linkerCommand = fmt::format( "{} -l{}", linkerCommand, additionalLibrary );
					}
					if( this->options.verbose ) {
						spdlog::info( "Running: {}", linkerCommand );
					}
					int linkReturnCode = system( linkerCommand.c_str() );
					std::remove( temporaryIrFile.c_str() );
					std::remove( temporaryOptFile.c_str() );
					std::remove( temporaryObjectFile.c_str() );
					if( linkReturnCode != 0 ) {
						fmt::print( stderr, "error: linker failed\n" );
						return 1;
					}
					if( this->options.stripDebugInfo ) {
						std::string stripCommand = fmt::format( "strip --strip-debug {} 2>/dev/null", finalOutputFilePath );
						system( stripCommand.c_str() );
					}
					if( this->options.verbose ) {
						spdlog::info( "Executable written to \"{}\"", finalOutputFilePath );
					}
					if( this->options.executeAfterCompilation ) {
						std::string binaryExecutionCommand = finalOutputFilePath;
						for( std::string& executionArgument : this->options.arguments ) {
							binaryExecutionCommand = fmt::format( "{} {}", binaryExecutionCommand, executionArgument );
						}
						int applicationExitCode = system( binaryExecutionCommand.c_str() );
						if( this->options.output.target.empty() ) {
							std::remove( finalOutputFilePath.c_str() );
						}
						return WIFEXITED( applicationExitCode ) ? WEXITSTATUS( applicationExitCode ) : 1;
					}
					fmt::print( "Compiled successfully: {}\n", finalOutputFilePath );
					return 0;
				}
				default: {
					return 0;
				}
			}
			return 0;
		}
		catch( const diagnostic::DiagnosticLimitReachedError& ) {
			size_t totalErrorCount = this->diagnostic.errorCount();
			if( totalErrorCount == 0 ) {
				totalErrorCount = 1;
			}
			fmt::print( stderr, "Compilation failed with {} error(s).\n", totalErrorCount );
			return 1;
		}
	}
	
	int Driver::runREPL() {
		fmt::print( fmt::fg( fmt::color::cyan ) | fmt::emphasis::bold, "Uranite REPL v0.1.0\n" );
		fmt::print( fmt::fg( fmt::color::gray ), "Type expressions or statements. Use ':quit' to exit, ':help' for commands.\n\n" );
		std::string inputBuffer;
		int currentIndentLevel = 0;
		int sessionLineNumber = 1;
		std::vector<std::string> sessionDeclarationSources;
		std::vector<std::string> sessionVariableSources;
		while( true ) {
			if( currentIndentLevel > 0 ) {
				fmt::print( fmt::fg( fmt::color::gray ), "... " );
				for( int indentIndex = 0; indentIndex < currentIndentLevel; indentIndex++ ) {
					fmt::print( "    " );
				}
			}
			else {
				fmt::print( fmt::fg( fmt::color::green ), "ae({})> ", sessionLineNumber );
			}
			std::string currentInputLine;
			std::getline( std::cin, currentInputLine );
			if( std::cin.fail() ) {
				fmt::print( "\n" );
				break;
			}
			if( currentIndentLevel == 0 ) {
				if( currentInputLine == ":quit" || currentInputLine == ":q" || currentInputLine == ":exit" ) {
					break;
				}
				if( currentInputLine == ":help" || currentInputLine == ":h" ) {
					fmt::print( "REPL Commands:\n" );
					fmt::print( "  :quit, :q, :exit  — Exit the REPL\n" );
					fmt::print( "  :help, :h         — Show this help\n" );
					fmt::print( "  :clear, :c        — Clear session state\n" );
					fmt::print( "  :ast              — Dump AST of last input\n" );
					fmt::print( "  :tokens           — Dump tokens of last input\n" );
					fmt::print( "  :type <expr>      — Show type of expression\n\n" );
					continue;
				}
				if( currentInputLine == ":clear" || currentInputLine == ":c" ) {
					sessionDeclarationSources.clear();
					sessionVariableSources.clear();
					fmt::print( "Session cleared.\n" );
					continue;
				}
				if( currentInputLine.empty() ) {
					continue;
				}
			}
			inputBuffer = fmt::format( "{}{}\n", inputBuffer, currentInputLine );
			std::string trimmedLine = currentInputLine;
			while( trimmedLine.empty() == false && ( trimmedLine.back() == ' ' || trimmedLine.back() == '\t' ) ) {
				trimmedLine.pop_back();
			}
			if( trimmedLine.empty() == false && trimmedLine.back() == ':' ) {
				currentIndentLevel++;
				continue;
			}
			if( currentIndentLevel > 0 ) {
				bool isLineIndented = false;
				for( char character : currentInputLine ) {
					if( character == ' ' || character == '\t' ) {
						isLineIndented = true;
						break;
					}
					break;
				}
				if( isLineIndented && currentInputLine.empty() == false ) {
					continue;
				}
				currentIndentLevel = 0;
			}
			bool shouldDumpAST = false;
			bool shouldDumpTokens = false;
			bool isTypeQueryOperation = false;
			std::string typeQueryExpression;
			if( inputBuffer.substr( 0, 4 ) == ":ast" ) {
				shouldDumpAST = true;
				inputBuffer = inputBuffer.substr( 4 );
			}
			else if( inputBuffer.substr( 0, 7 ) == ":tokens" ) {
				shouldDumpTokens = true;
				inputBuffer = inputBuffer.substr( 7 );
			}
			else if( inputBuffer.substr( 0, 5 ) == ":type" ) {
				isTypeQueryOperation = true;
				typeQueryExpression = inputBuffer.substr( 5 );
				while( typeQueryExpression.empty() == false && ( typeQueryExpression[0] == ' ' || typeQueryExpression[0] == '\t' ) ) {
					typeQueryExpression = typeQueryExpression.substr( 1 );
				}
				inputBuffer = typeQueryExpression;
			}
			std::string wrappedSourceText = "package repl\n\n";
			for( std::string& declarationSource : sessionDeclarationSources ) {
				wrappedSourceText = fmt::format( "{}{}\n", wrappedSourceText, declarationSource );
			}
			bool isInputADeclaration = false;
			std::string trimmedInput = inputBuffer;
			while( trimmedInput.empty() == false && ( trimmedInput[0] == ' ' || trimmedInput[0] == '\n' ) ) {
				trimmedInput = trimmedInput.substr( 1 );
			}
			if( trimmedInput.substr( 0, 8 ) == "function" || trimmedInput.substr( 0, 5 ) == "class" || trimmedInput.substr( 0, 6 ) == "struct" || trimmedInput.substr( 0, 4 ) == "enum" || trimmedInput.substr( 0, 9 ) == "interface" || trimmedInput.substr( 0, 6 ) == "extern" || trimmedInput.substr( 0, 6 ) == "import" || trimmedInput.substr( 0, 4 ) == "from" ) {
				isInputADeclaration = true;
				wrappedSourceText = fmt::format( "{}{}", wrappedSourceText, inputBuffer );
			}
			else {
				wrappedSourceText = fmt::format( "{}public function __repl_eval__() -> void:\n", wrappedSourceText );
				for( std::string& variableSource : sessionVariableSources ) {
					wrappedSourceText = fmt::format( "{}    {}\n", wrappedSourceText, variableSource );
				}
				std::istringstream inputStringStream( inputBuffer );
				std::string individualInputLine;
				while( std::getline( inputStringStream, individualInputLine ) ) {
					if( individualInputLine.empty() == false ) {
						wrappedSourceText = fmt::format( "{}    {}\n", wrappedSourceText, individualInputLine );
					}
				}
			}
			try {
				diagnostic::Engine replDiagnosticEngine;
				replDiagnosticEngine.setMaxErrors( 10 );
				lexer::Lexer replLexer( wrappedSourceText, "<repl>", replDiagnosticEngine );
				std::vector<token::Token> replTokens = replLexer.tokenize();
				replDiagnosticEngine.setSourceLines( replLexer.sourceLines() );
				if( shouldDumpTokens ) {
					for( const token::Token& currentReplToken : replTokens ) {
						fmt::print( "  [{:>4}:{:<3}] {:<15} \"{}\"\n", currentReplToken.source->location->line, currentReplToken.source->location->column, token::toString( currentReplToken.type ), currentReplToken.value );
					}
					inputBuffer.clear();
					sessionLineNumber++;
					continue;
				}
				if( replDiagnosticEngine.hasErrors() ) {
					fmt::print( fmt::fg( fmt::color::red ), "Syntax error in input\n" );
					replDiagnosticEngine.printAll();
					inputBuffer.clear();
					sessionLineNumber++;
					continue;
				}
				parser::Parser replParser( replTokens, replDiagnosticEngine );
				ast::nodes::ProgramSharedPointer replProgram = replParser.parse();
				if( replDiagnosticEngine.hasErrors() ) {
					fmt::print( fmt::fg( fmt::color::red ), "Parse error\n" );
					replDiagnosticEngine.printAll();
					inputBuffer.clear();
					sessionLineNumber++;
					continue;
				}
				if( shouldDumpAST ) {
					visitors::ASTPrinter astPrinterInstance;
					fmt::print( "{}\n", astPrinterInstance.print( *replProgram ) );
					inputBuffer.clear();
					sessionLineNumber++;
					continue;
				}
				semantic::Analyzer replSemanticAnalyzer( replDiagnosticEngine );
				if( replSemanticAnalyzer.analyze( *replProgram ) == false ) {
					fmt::print( fmt::fg( fmt::color::red ), "Type error\n" );
					replDiagnosticEngine.printAll();
					inputBuffer.clear();
					sessionLineNumber++;
					continue;
				}
				if( isTypeQueryOperation ) {
					for( ast::nodes::DeclarationSharedPointer& replDeclaration : replProgram->declarations ) {
						if( replDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
							ast::nodes::FunctionDeclaration& functionDecl = static_cast<ast::nodes::FunctionDeclaration&>( *replDeclaration );
							if( functionDecl.name == "__repl_eval__" && functionDecl.body.empty() == false ) {
								ast::nodes::StatementSharedPointer& lastStatement = functionDecl.body.back();
								if( lastStatement->kind == ast::Node::Kind::ExpressionStatement ) {
									ast::nodes::ExpressionStatement& expressionStmt = static_cast<ast::nodes::ExpressionStatement&>( *lastStatement );
									if( expressionStmt.expression != nullptr && expressionStmt.expression->semanticType != nullptr ) {
										fmt::print( fmt::fg( fmt::color::cyan ), "{}\n", expressionStmt.expression->semanticType->toString() );
									}
									else {
										fmt::print( "(unknown type)\n" );
									}
								}
							}
						}
					}
					inputBuffer.clear();
					sessionLineNumber++;
					continue;
				}
				if( isInputADeclaration ) {
					sessionDeclarationSources.push_back( inputBuffer );
				}
				else {
					bool looksLikeVariableDeclaration = false;
					std::string trimmedInputText = trimmedInput;
					while( trimmedInputText.empty() == false && trimmedInputText.back() == '\n' ) {
						trimmedInputText.pop_back();
					}
					if( trimmedInputText.empty() == false && ( std::isupper( static_cast<unsigned char>( trimmedInputText[0] ) ) || trimmedInputText[0] == '?' ) && trimmedInputText.find( '=' ) != std::string::npos ) {
						looksLikeVariableDeclaration = true;
					}
					if( trimmedInputText.substr( 0, 4 ) == "mut " ) {
						looksLikeVariableDeclaration = true;
					}
					if( looksLikeVariableDeclaration ) {
						sessionVariableSources.push_back( trimmedInputText );
					}
				}
				fmt::print( fmt::fg( fmt::color::green ), "OK\n" );
			}
			catch( const diagnostic::DiagnosticLimitReachedError& diagnosticLimitError ) {
				fmt::print( fmt::fg( fmt::color::red ), "Error limit reached: {}\n", diagnosticLimitError.what() );
			}
			catch( const errors::Throwable& compilerError ) {
				fmt::print( fmt::fg( fmt::color::red ), "Compiler error: {}\n", compilerError.what() );
			}
			catch( const std::exception& unexpectedError ) {
				fmt::print( fmt::fg( fmt::color::red ), "Internal error: {}\n", unexpectedError.what() );
			}
			inputBuffer.clear();
			sessionLineNumber++;
		}
		return 0;
	}
	
	// int Driver::semanticAnalysis() {
	// }
	
	// int Driver::tokenize() {
	// }
	
}
