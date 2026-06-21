
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builder.hpp"
#include "uranite/compiler/driver.hpp"
#include "uranite/compiler/option.hpp"

namespace uranite::pkg {

	BuildOrchestrator::BuildOrchestrator( const PackageManifest& manifest,
		const std::vector<ResolvedDependency>& resolvedDependencies,
		PackageCache& packageCache )
		: manifest_( manifest ), resolvedDependencies_( resolvedDependencies ),
		  packageCache_( packageCache ) {}

	std::vector<std::string> BuildOrchestrator::collectModulePaths() const {
		std::vector<std::string> modulePaths;

		std::string localModulesPath = this->manifest_.buildConfig.modulesPath;
		if( localModulesPath.empty() ) {
			localModulesPath = "modules";
		}
		if( std::filesystem::exists( localModulesPath ) ) {
			modulePaths.push_back( localModulesPath );
		}

		for( const ResolvedDependency& dependency : this->resolvedDependencies_ ) {
			std::string dependencyModulesPath = this->packageCache_.getModulesPath(
				dependency.packageName, dependency.resolvedVersion );
			if( std::filesystem::exists( dependencyModulesPath ) ) {
				modulePaths.push_back( dependencyModulesPath );
			}
		}

		return modulePaths;
	}

	std::vector<std::string> BuildOrchestrator::collectLinkLibraries() const {
		return this->manifest_.buildConfig.linkLibraries;
	}

	std::string BuildOrchestrator::resolveOutputPath() const {
		if( this->manifest_.buildConfig.outputPath.empty() == false ) {
			return this->manifest_.buildConfig.outputPath;
		}
		return "build/" + this->manifest_.packageName;
	}

	BuildResult BuildOrchestrator::build() {
		BuildResult result;

		std::string entryFile = this->manifest_.entrySourceFile;
		if( entryFile.empty() ) {
			entryFile = "src/main.urn";
		}

		if( std::filesystem::exists( entryFile ) == false ) {
			result.succeeded = false;
			result.errorMessage = "entry file not found: " + entryFile;
			return result;
		}

		std::string outputPath = this->resolveOutputPath();

		std::filesystem::path outputParentPath = std::filesystem::path( outputPath ).parent_path();
		if( outputParentPath.empty() == false && std::filesystem::exists( outputParentPath ) == false ) {
			std::filesystem::create_directories( outputParentPath );
		}

		compiler::Options compilerOptions;
		compilerOptions.output.source = entryFile;
		compilerOptions.output.target = outputPath;
		if( this->manifest_.packageType == "library" ) {
			compilerOptions.output.kind = compiler::Output::Kind::Object;
		}
		else {
			compilerOptions.output.kind = compiler::Output::Kind::Executable;
		}
		compilerOptions.linkLibraries = this->collectLinkLibraries();

		std::vector<std::string> modulePaths = this->collectModulePaths();
		if( modulePaths.empty() == false ) {
			compilerOptions.modulesPath = modulePaths[0];
			for( size_t pathIndex = 1; pathIndex < modulePaths.size(); pathIndex++ ) {
				compilerOptions.includePaths.push_back( modulePaths[pathIndex] );
			}
		}

		std::string optimizationLevel = this->manifest_.buildConfig.optimizationLevel;
		if( optimizationLevel == "0" ) {
			compilerOptions.optimization = optimizer::Level::O0;
		}
		else if( optimizationLevel == "1" ) {
			compilerOptions.optimization = optimizer::Level::O1;
		}
		else if( optimizationLevel == "3" ) {
			compilerOptions.optimization = optimizer::Level::O3;
		}
		else {
			compilerOptions.optimization = optimizer::Level::O2;
		}

		compilerOptions.stripDebugInfo = this->manifest_.buildConfig.stripDebugInfo;

		if( this->manifest_.buildConfig.targetTriple.empty() == false ) {
			compilerOptions.targetTriple = this->manifest_.buildConfig.targetTriple;
		}

		compiler::Driver driver( compilerOptions );
		int32_t compilerExitCode = driver.run();

		result.exitCode = compilerExitCode;
		result.succeeded = compilerExitCode == 0;
		result.outputPath = outputPath;

		if( result.succeeded == false ) {
			result.errorMessage = "compilation failed with exit code " + std::to_string( compilerExitCode );
		}

		return result;
	}

	BuildResult BuildOrchestrator::buildAndRun( const std::vector<std::string>& programArguments ) {
		BuildResult buildResult = this->build();

		if( buildResult.succeeded == false ) {
			return buildResult;
		}

		int32_t programExitCode = this->executeProgram( buildResult.outputPath, programArguments );
		buildResult.exitCode = programExitCode;
		buildResult.succeeded = programExitCode == 0;

		return buildResult;
	}

	int32_t BuildOrchestrator::executeProgram( const std::string& executablePath,
		const std::vector<std::string>& programArguments ) const {

		std::vector<const char*> argvPointers;
		argvPointers.push_back( executablePath.c_str() );
		for( const std::string& argument : programArguments ) {
			argvPointers.push_back( argument.c_str() );
		}
		argvPointers.push_back( nullptr );

		pid_t childProcessId = fork();
		if( childProcessId == 0 ) {
			execv( executablePath.c_str(), const_cast<char* const*>( argvPointers.data() ) );
			_exit( 127 );
		}
		if( childProcessId < 0 ) {
			return 1;
		}

		int32_t childStatus = 0;
		waitpid( childProcessId, &childStatus, 0 );

		if( WIFEXITED( childStatus ) ) {
			return WEXITSTATUS( childStatus );
		}
		return 128 + WTERMSIG( childStatus );
	}

} // namespace uranite::pkg
