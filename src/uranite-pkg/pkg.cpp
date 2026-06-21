
//
// @author hxAri (hxari)
// @create 2026-05-14 18:20
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

#include <argparse/argparse.hpp>
#include <filesystem>
#include <fmt/core.h>
#include <fmt/color.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include "manifest/manifest.hpp"
#include "manifest/lockfile.hpp"
#include "resolver/version.hpp"
#include "resolver/resolver.hpp"
#include "cache/cache.hpp"
#include "builder/builder.hpp"

static const std::string MANIFEST_FILENAME = "uranite.yaml";
static const std::string LOCKFILE_FILENAME = "uranite.lock";

static bool ensureManifestExists() {
	if( std::filesystem::exists( MANIFEST_FILENAME ) == false ) {
		fmt::print( stderr, "error: no {} found in current directory\n", MANIFEST_FILENAME );
		fmt::print( stderr, "hint: run 'uranite-pkg init' to create one\n" );
		return false;
	}
	return true;
}

static uranite::pkg::PackageManifest loadManifest() {
	return uranite::pkg::ManifestParser::parseFromFile( MANIFEST_FILENAME );
}

static int commandInit() {
	if( std::filesystem::exists( MANIFEST_FILENAME ) ) {
		fmt::print( stderr, "error: {} already exists in current directory\n", MANIFEST_FILENAME );
		return 1;
	}

	std::string directoryName = std::filesystem::current_path().filename().string();

	std::ofstream manifestFile( MANIFEST_FILENAME );
	if( manifestFile.is_open() == false ) {
		fmt::print( stderr, "error: could not create {}\n", MANIFEST_FILENAME );
		return 1;
	}

	manifestFile << "package:\n";
	manifestFile << "  name: \"" << directoryName << "\"\n";
	manifestFile << "  version: \"0.1.0\"\n";
	manifestFile << "  description: \"\"\n";
	manifestFile << "  authors: []\n";
	manifestFile << "  license: \"GPL-3.0\"\n";
	manifestFile << "  entry: \"src/main.urn\"\n";
	manifestFile << "\n";
	manifestFile << "dependencies: {}\n";
	manifestFile << "\n";
	manifestFile << "dev-dependencies: {}\n";
	manifestFile << "\n";
	manifestFile << "build:\n";
	manifestFile << "  optimization: \"2\"\n";
	manifestFile << "  output: \"build/" << directoryName << "\"\n";
	manifestFile << "  modules-path: \"modules\"\n";

	fmt::print( "Created {}\n", MANIFEST_FILENAME );

	if( std::filesystem::exists( "src" ) == false ) {
		std::filesystem::create_directory( "src" );
		fmt::print( "Created src/\n" );
	}

	std::string entryFilePath = "src/main.urn";
	if( std::filesystem::exists( entryFilePath ) == false ) {
		std::ofstream entryFile( entryFilePath );
		entryFile << "package main\n";
		entryFile << "\n";
		entryFile << "extern function printf( String format, I64 value ) -> I64;\n";
		entryFile << "\n";
		entryFile << "function main() -> I64:\n";
		entryFile << "    printf( \"Hello from %s!\\n\", 0 )\n";
		entryFile << "    return 0\n";
		fmt::print( "Created {}\n", entryFilePath );
	}

	return 0;
}

static int commandInstall() {
	if( ensureManifestExists() == false ) {
		return 1;
	}

	uranite::pkg::PackageManifest manifest = loadManifest();

	if( manifest.dependencies.empty() && manifest.developmentDependencies.empty() ) {
		fmt::print( "No dependencies declared.\n" );
		return 0;
	}

	uranite::pkg::DependencyResolver resolver;
	uranite::pkg::ResolutionResult resolution = resolver.resolve( manifest, true );

	if( resolution.hasConflicts ) {
		fmt::print( stderr, "Dependency resolution conflicts:\n" );
		for( const std::string& conflict : resolution.conflictMessages ) {
			fmt::print( stderr, "  - {}\n", conflict );
		}
		return 1;
	}

	uranite::pkg::PackageCache cache;
	cache.ensureCacheDirectoryExists();

	uint32_t installedCount = 0;
	uint32_t cachedCount = 0;

	for( const uranite::pkg::ResolvedDependency& resolved : resolution.resolvedPackages ) {
		if( cache.isPackageCached( resolved.packageName, resolved.resolvedVersion ) ) {
			cachedCount++;
			fmt::print( "  {} {} (cached)\n", resolved.packageName, resolved.resolvedVersion.toString() );
		}
		else {
			fmt::print( "  {} {} (would download from {})\n",
				resolved.packageName, resolved.resolvedVersion.toString(),
				resolved.sourceRepository.empty() ? "registry" : resolved.sourceRepository );
			installedCount++;
		}
	}

	uranite::pkg::Lockfile lockfile;
	for( const uranite::pkg::ResolvedDependency& resolved : resolution.resolvedPackages ) {
		uranite::pkg::LockfileEntry entry;
		entry.packageName = resolved.packageName;
		entry.resolvedVersionString = resolved.resolvedVersion.toString();
		entry.sourceIntegrityHash = resolved.integrityHash;
		entry.sourceRepository = resolved.sourceRepository;
		entry.transitiveDependencyNames = resolved.transitiveDependencyNames;
		lockfile.lockedEntries.push_back( entry );
	}
	uranite::pkg::LockfileManager::writeToFile( lockfile, LOCKFILE_FILENAME );

	fmt::print( "\nResolved {} packages ({} cached, {} to download)\n",
		resolution.resolvedPackages.size(), cachedCount, installedCount );
	fmt::print( "Wrote {}\n", LOCKFILE_FILENAME );

	return 0;
}

static int commandBuild() {
	if( ensureManifestExists() == false ) {
		return 1;
	}

	uranite::pkg::PackageManifest manifest = loadManifest();

	std::vector<uranite::pkg::ResolvedDependency> resolvedDependencies;

	if( uranite::pkg::LockfileManager::exists( LOCKFILE_FILENAME ) ) {
		uranite::pkg::Lockfile lockfile = uranite::pkg::LockfileManager::readFromFile( LOCKFILE_FILENAME );
		uranite::pkg::DependencyResolver resolver;
		uranite::pkg::ResolutionResult resolution = resolver.resolveFromLockfile( lockfile );
		resolvedDependencies = resolution.resolvedPackages;
	}
	else if( manifest.dependencies.empty() == false ) {
		uranite::pkg::DependencyResolver resolver;
		uranite::pkg::ResolutionResult resolution = resolver.resolve( manifest, false );
		if( resolution.hasConflicts ) {
			fmt::print( stderr, "Dependency resolution failed. Run 'uranite-pkg install' first.\n" );
			return 1;
		}
		resolvedDependencies = resolution.resolvedPackages;
	}

	uranite::pkg::PackageCache cache;
	uranite::pkg::BuildOrchestrator orchestrator( manifest, resolvedDependencies, cache );

	fmt::print( "Building {} {}...\n", manifest.packageName, manifest.packageVersion );

	uranite::pkg::BuildResult buildResult = orchestrator.build();

	if( buildResult.succeeded ) {
		fmt::print( "Built successfully: {}\n", buildResult.outputPath );
		return 0;
	}
	else {
		fmt::print( stderr, "Build failed: {}\n", buildResult.errorMessage );
		return buildResult.exitCode;
	}
}

static int commandRun( const std::vector<std::string>& programArguments ) {
	if( ensureManifestExists() == false ) {
		return 1;
	}

	uranite::pkg::PackageManifest manifest = loadManifest();

	std::vector<uranite::pkg::ResolvedDependency> resolvedDependencies;

	if( uranite::pkg::LockfileManager::exists( LOCKFILE_FILENAME ) ) {
		uranite::pkg::Lockfile lockfile = uranite::pkg::LockfileManager::readFromFile( LOCKFILE_FILENAME );
		uranite::pkg::DependencyResolver resolver;
		uranite::pkg::ResolutionResult resolution = resolver.resolveFromLockfile( lockfile );
		resolvedDependencies = resolution.resolvedPackages;
	}

	uranite::pkg::PackageCache cache;
	uranite::pkg::BuildOrchestrator orchestrator( manifest, resolvedDependencies, cache );

	uranite::pkg::BuildResult buildResult = orchestrator.buildAndRun( programArguments );

	return buildResult.exitCode;
}

static int commandList() {
	if( ensureManifestExists() == false ) {
		return 1;
	}

	uranite::pkg::PackageManifest manifest = loadManifest();

	fmt::print( "{} {}\n", manifest.packageName, manifest.packageVersion );

	if( manifest.dependencies.empty() == false ) {
		fmt::print( "\nDependencies:\n" );
		for( const uranite::pkg::PackageDependency& dependency : manifest.dependencies ) {
			fmt::print( "  {} {}", dependency.packageName, dependency.versionConstraint );
			if( dependency.sourceRepository.empty() == false ) {
				fmt::print( " ({})", dependency.sourceRepository );
			}
			fmt::print( "\n" );
		}
	}

	if( manifest.developmentDependencies.empty() == false ) {
		fmt::print( "\nDev Dependencies:\n" );
		for( const uranite::pkg::PackageDependency& dependency : manifest.developmentDependencies ) {
			fmt::print( "  {} {}", dependency.packageName, dependency.versionConstraint );
			if( dependency.sourceRepository.empty() == false ) {
				fmt::print( " ({})", dependency.sourceRepository );
			}
			fmt::print( "\n" );
		}
	}

	if( uranite::pkg::LockfileManager::exists( LOCKFILE_FILENAME ) ) {
		uranite::pkg::Lockfile lockfile = uranite::pkg::LockfileManager::readFromFile( LOCKFILE_FILENAME );

		if( lockfile.lockedEntries.empty() == false ) {
			fmt::print( "\nLocked versions:\n" );
			for( const uranite::pkg::LockfileEntry& entry : lockfile.lockedEntries ) {
				fmt::print( "  {} {}\n", entry.packageName, entry.resolvedVersionString );
				for( const std::string& transitiveName : entry.transitiveDependencyNames ) {
					fmt::print( "    -> {}\n", transitiveName );
				}
			}
		}
	}

	if( manifest.dependencies.empty() && manifest.developmentDependencies.empty() ) {
		fmt::print( "  (no dependencies)\n" );
	}

	return 0;
}

static int commandLock() {
	if( ensureManifestExists() == false ) {
		return 1;
	}

	uranite::pkg::PackageManifest manifest = loadManifest();

	uranite::pkg::DependencyResolver resolver;
	uranite::pkg::ResolutionResult resolution = resolver.resolve( manifest, true );

	if( resolution.hasConflicts ) {
		fmt::print( stderr, "Dependency resolution conflicts:\n" );
		for( const std::string& conflict : resolution.conflictMessages ) {
			fmt::print( stderr, "  - {}\n", conflict );
		}
		return 1;
	}

	uranite::pkg::Lockfile lockfile;
	for( const uranite::pkg::ResolvedDependency& resolved : resolution.resolvedPackages ) {
		uranite::pkg::LockfileEntry entry;
		entry.packageName = resolved.packageName;
		entry.resolvedVersionString = resolved.resolvedVersion.toString();
		entry.sourceIntegrityHash = resolved.integrityHash;
		entry.sourceRepository = resolved.sourceRepository;
		entry.transitiveDependencyNames = resolved.transitiveDependencyNames;
		lockfile.lockedEntries.push_back( entry );
	}
	uranite::pkg::LockfileManager::writeToFile( lockfile, LOCKFILE_FILENAME );

	fmt::print( "Wrote {} ({} packages)\n", LOCKFILE_FILENAME, lockfile.lockedEntries.size() );
	return 0;
}

static int commandUpdate() {
	if( ensureManifestExists() == false ) {
		return 1;
	}

	if( std::filesystem::exists( LOCKFILE_FILENAME ) ) {
		std::filesystem::remove( LOCKFILE_FILENAME );
		fmt::print( "Removed stale {}\n", LOCKFILE_FILENAME );
	}

	return commandInstall();
}

int main( int argc, char* argv[] ) {
	argparse::ArgumentParser program( "uranite-pkg", "1.0.0" );
	program.add_description( "Uranite package manager and build tool" );

	argparse::ArgumentParser initCommand( "init" );
	initCommand.add_description( "Create a new uranite.yaml manifest in the current directory" );

	argparse::ArgumentParser installCommand( "install" );
	installCommand.add_description( "Resolve and download all dependencies" );
	installCommand.add_argument( "package" )
		.help( "Optional specific package to install" )
		.nargs( argparse::nargs_pattern::optional );

	argparse::ArgumentParser buildCommand( "build" );
	buildCommand.add_description( "Build the project with dependency resolution" );

	argparse::ArgumentParser runCommand( "run" );
	runCommand.add_description( "Build and execute the project entry file" );
	runCommand.add_argument( "args" )
		.help( "Arguments to pass to the program" )
		.remaining();

	argparse::ArgumentParser listCommand( "list" );
	listCommand.add_description( "Show dependency tree" );

	argparse::ArgumentParser lockCommand( "lock" );
	lockCommand.add_description( "Regenerate uranite.lock without downloading" );

	argparse::ArgumentParser updateCommand( "update" );
	updateCommand.add_description( "Re-resolve to latest compatible versions" );

	argparse::ArgumentParser cleanCommand( "clean" );
	cleanCommand.add_description( "Remove build artifacts" );

	program.add_subparser( initCommand );
	program.add_subparser( installCommand );
	program.add_subparser( buildCommand );
	program.add_subparser( runCommand );
	program.add_subparser( listCommand );
	program.add_subparser( lockCommand );
	program.add_subparser( updateCommand );
	program.add_subparser( cleanCommand );

	try {
		program.parse_args( argc, argv );
	}
	catch( const std::runtime_error& parseError ) {
		fmt::print( stderr, "error: {}\n", parseError.what() );
		fmt::print( stderr, "{}", program.help().str() );
		return 1;
	}

	if( program.is_subcommand_used( "init" ) ) {
		return commandInit();
	}

	if( program.is_subcommand_used( "install" ) ) {
		return commandInstall();
	}

	if( program.is_subcommand_used( "build" ) ) {
		return commandBuild();
	}

	if( program.is_subcommand_used( "run" ) ) {
		std::vector<std::string> programArguments;
		try {
			programArguments = runCommand.get<std::vector<std::string>>( "args" );
		}
		catch( ... ) {}
		return commandRun( programArguments );
	}

	if( program.is_subcommand_used( "list" ) ) {
		return commandList();
	}

	if( program.is_subcommand_used( "lock" ) ) {
		return commandLock();
	}

	if( program.is_subcommand_used( "update" ) ) {
		return commandUpdate();
	}

	if( program.is_subcommand_used( "clean" ) ) {
		if( std::filesystem::exists( "build" ) ) {
			std::filesystem::remove_all( "build" );
			fmt::print( "Cleaned build/\n" );
		}
		else {
			fmt::print( "Nothing to clean.\n" );
		}
		return 0;
	}

	fmt::print( stderr, "{}", program.help().str() );
	return 1;
}
