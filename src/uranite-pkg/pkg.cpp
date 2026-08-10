
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
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fmt/core.h>
#include <fmt/color.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

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

	manifestFile << "build:\n";
	manifestFile << "    build-path: \"build/" << directoryName << "\"\n";
	manifestFile << "    optimization: 2\n";
	manifestFile << "    strip-symbols: false\n";
	manifestFile << "\n";
	manifestFile << "package:\n";
	manifestFile << "    authors:\n";
	manifestFile << "    -   email: \"\"\n";
	manifestFile << "        name: \"\"\n";
	manifestFile << "    description: \"\"\n";
	manifestFile << "    entry: \"src\"\n";
	manifestFile << "    license: \"GPL-3.0\"\n";
	manifestFile << "    name: \"" << directoryName << "\"\n";
	manifestFile << "    type: \"binary\"\n";
	manifestFile << "    version: \"0.1.0\"\n";
	manifestFile << "\n";
	manifestFile << "requirements:\n";
	manifestFile << "    dependencies: []\n";
	manifestFile << "    dev-dependencies: []\n";
	manifestFile << "    path: \"build/modules\"\n";
	manifestFile << "    system-dependencies: []\n";
	manifestFile << "\n";
	manifestFile << "scripts:\n";
	manifestFile << "    # pre-build: \"echo preparing...\"\n";
	manifestFile << "    # post-build: \"echo done!\"\n";
	manifestFile << "\n";
	manifestFile << "test:\n";
	manifestFile << "    coverage: false\n";
	manifestFile << "    parallel: false\n";
	manifestFile << "    path: \"testing/\"\n";
	manifestFile << "    timeout: 5000\n";

	fmt::print( "Created {}\n", MANIFEST_FILENAME );

	if( std::filesystem::exists( "src" ) == false ) {
		std::filesystem::create_directory( "src" );
		fmt::print( "Created src/\n" );
	}

	std::string entryFilePath = "src/__mod__.urn";
	if( std::filesystem::exists( entryFilePath ) == false ) {
		std::ofstream entryFile( entryFilePath );
		entryFile << "package main\n";
		entryFile << "\n";
		entryFile << "extern function printf( String format, I64 value ) -> I64;\n";
		entryFile << "\n";
		entryFile << "function main() -> I64:\n";
		entryFile << "    printf( \"Hello, World!\\n\", 0 )\n";
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

	if( manifest.requirements.dependencies.empty() && manifest.requirements.developmentDependencies.empty() ) {
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
	else if( manifest.requirements.dependencies.empty() == false ) {
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

	if( manifest.requirements.dependencies.empty() == false ) {
		fmt::print( "\nDependencies:\n" );
		for( const uranite::pkg::PackageDependency& dependency : manifest.requirements.dependencies ) {
			fmt::print( "  {} {}", dependency.packageName, dependency.versionConstraint );
			if( dependency.sourceRepository.empty() == false ) {
				fmt::print( " ({})", dependency.sourceRepository );
			}
			fmt::print( "\n" );
		}
	}

	if( manifest.requirements.developmentDependencies.empty() == false ) {
		fmt::print( "\nDev Dependencies:\n" );
		for( const uranite::pkg::PackageDependency& dependency : manifest.requirements.developmentDependencies ) {
			fmt::print( "  {} {}", dependency.packageName, dependency.versionConstraint );
			if( dependency.sourceRepository.empty() == false ) {
				fmt::print( " ({})", dependency.sourceRepository );
			}
			fmt::print( "\n" );
		}
	}

	if( manifest.requirements.systemDependencies.empty() == false ) {
		fmt::print( "\nSystem Dependencies:\n" );
		for( const std::string& systemDep : manifest.requirements.systemDependencies ) {
			fmt::print( "  {}\n", systemDep );
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

	if( manifest.requirements.dependencies.empty() && manifest.requirements.developmentDependencies.empty() ) {
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

static int waitForChild( pid_t childProcessId, int32_t timeoutMillis ) {
	int32_t childStatus = 0;
	if( timeoutMillis > 0 ) {
		auto startTime = std::chrono::steady_clock::now();
		while( true ) {
			pid_t waitResult = waitpid( childProcessId, &childStatus, WNOHANG );
			if( waitResult != 0 ) {
				break;
			}
			auto elapsedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - startTime ).count();
			if( elapsedMillis >= timeoutMillis ) {
				kill( childProcessId, SIGKILL );
				waitpid( childProcessId, &childStatus, 0 );
				return 124;
			}
			usleep( 10000 );
		}
	}
	else {
		waitpid( childProcessId, &childStatus, 0 );
	}

	if( WIFEXITED( childStatus ) ) {
		return WEXITSTATUS( childStatus );
	}
	return 128 + WTERMSIG( childStatus );
}

static int runShellCommand( const std::string& command,
	const std::unordered_map<std::string, std::string>& environment, int32_t timeoutMillis ) {

	pid_t childProcessId = fork();
	if( childProcessId == 0 ) {
		for( const std::pair<const std::string, std::string>& envEntry : environment ) {
			setenv( envEntry.first.c_str(), envEntry.second.c_str(), 1 );
		}
		execl( "/bin/sh", "sh", "-c", command.c_str(), nullptr );
		_exit( 127 );
	}
	if( childProcessId < 0 ) {
		return 1;
	}
	return waitForChild( childProcessId, timeoutMillis );
}

static int runArgvCommand( const std::vector<std::string>& arguments,
	const std::unordered_map<std::string, std::string>& environment, int32_t timeoutMillis ) {

	if( arguments.empty() ) {
		return 1;
	}

	pid_t childProcessId = fork();
	if( childProcessId == 0 ) {
		for( const std::pair<const std::string, std::string>& envEntry : environment ) {
			setenv( envEntry.first.c_str(), envEntry.second.c_str(), 1 );
		}
		std::vector<char*> argv;
		for( const std::string& arg : arguments ) {
			argv.push_back( const_cast<char*>( arg.c_str() ) );
		}
		argv.push_back( nullptr );
		execvp( argv[0], argv.data() );
		_exit( 127 );
	}
	if( childProcessId < 0 ) {
		return 1;
	}
	return waitForChild( childProcessId, timeoutMillis );
}

static int commandTest() {
	if( ensureManifestExists() == false ) {
		return 1;
	}

	uranite::pkg::PackageManifest manifest = loadManifest();

	if( manifest.scripts.count( "test" ) > 0 ) {
		fmt::print( "Running test script...\n" );
		int32_t scriptExitCode = runShellCommand(
			manifest.scripts.at( "test" ), manifest.testConfig.environment, 0 );
		if( scriptExitCode != 0 ) {
			fmt::print( stderr, "Test script failed with exit code {}\n", scriptExitCode );
			return scriptExitCode;
		}
		fmt::print( "Test script passed.\n\n" );
	}

	std::string testPath = manifest.testConfig.path;
	if( testPath.empty() ) {
		testPath = "testing/";
	}

	if( std::filesystem::exists( testPath ) == false || std::filesystem::is_directory( testPath ) == false ) {
		fmt::print( stderr, "Test directory not found: {}\n", testPath );
		return 1;
	}

	std::vector<std::string> testFiles;
	for( const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator( testPath ) ) {
		if( entry.is_regular_file() == false ) {
			continue;
		}
		std::string filename = entry.path().filename().string();
		if( filename.size() > 4 && filename.substr( filename.size() - 4 ) == ".urn" ) {
			testFiles.push_back( entry.path().string() );
		}
	}

	if( testFiles.empty() ) {
		fmt::print( "No test files found in {}\n", testPath );
		return 0;
	}

	std::sort( testFiles.begin(), testFiles.end() );

	std::string compilerPath = std::filesystem::canonical( "/proc/self/exe" ).parent_path().string() + "/uranite";
	if( std::filesystem::exists( compilerPath ) == false ) {
		fmt::print( stderr, "Compiler not found at {}\n", compilerPath );
		return 1;
	}

	std::string entrySourceDir = manifest.entrySourceFile;
	if( entrySourceDir.empty() ) {
		entrySourceDir = "src";
	}
	if( std::filesystem::is_directory( entrySourceDir ) == false ) {
		entrySourceDir = std::filesystem::path( entrySourceDir ).parent_path().string();
	}
	if( entrySourceDir.empty() ) {
		entrySourceDir = ".";
	}

	std::string localModulesPath = manifest.buildConfig.modulesPath;
	if( localModulesPath.empty() ) {
		localModulesPath = "modules";
	}

	std::string requirementsModulesPath = manifest.requirements.modulesPath;
	if( requirementsModulesPath.empty() ) {
		requirementsModulesPath = "build/modules";
	}

	std::vector<std::string> compilerBaseArgs = { compilerPath };
	compilerBaseArgs.push_back( "-I" );
	compilerBaseArgs.push_back( entrySourceDir );

	if( std::filesystem::exists( localModulesPath ) ) {
		compilerBaseArgs.push_back( "-I" );
		compilerBaseArgs.push_back( localModulesPath );
	}

	if( std::filesystem::exists( requirementsModulesPath ) ) {
		compilerBaseArgs.push_back( "-I" );
		compilerBaseArgs.push_back( requirementsModulesPath );
	}

	for( const std::string& includePath : manifest.buildConfig.includePaths ) {
		if( std::filesystem::exists( includePath ) ) {
			compilerBaseArgs.push_back( "-I" );
			compilerBaseArgs.push_back( includePath );
		}
	}

	for( const std::string& linkLibrary : manifest.buildConfig.linkLibraries ) {
		compilerBaseArgs.push_back( "-l" );
		compilerBaseArgs.push_back( linkLibrary );
	}

	if( manifest.buildConfig.optimizationLevel.empty() == false ) {
		compilerBaseArgs.push_back( "-O" );
		compilerBaseArgs.push_back( manifest.buildConfig.optimizationLevel );
	}

	int32_t timeoutMillis = manifest.testConfig.timeout > 0 ? manifest.testConfig.timeout : 10000;
	uint32_t passedCount = 0;
	uint32_t failedCount = 0;
	uint32_t timedOutCount = 0;
	std::vector<std::string> failedTests;

	std::string tempDirTemplate = std::filesystem::temp_directory_path().string() + "/uranite-test-XXXXXX";
	std::vector<char> tempDirBuffer( tempDirTemplate.begin(), tempDirTemplate.end() );
	tempDirBuffer.push_back( '\0' );
	char* tempDirResult = mkdtemp( tempDirBuffer.data() );
	if( tempDirResult == nullptr ) {
		fmt::print( stderr, "Failed to create temporary directory\n" );
		return 1;
	}
	std::string tempDir( tempDirResult );
	chmod( tempDirResult, 0700 );

	fmt::print( "Running {} tests from {}...\n\n", testFiles.size(), testPath );

	for( const std::string& testFile : testFiles ) {
		std::string testName = std::filesystem::path( testFile ).stem().string();
		std::string outputBinary = tempDir + "/" + testName;

		std::vector<std::string> compileArgs = compilerBaseArgs;
		compileArgs.push_back( testFile );
		compileArgs.push_back( "-o" );
		compileArgs.push_back( outputBinary );

		int32_t compileExitCode = runArgvCommand(
			compileArgs, manifest.testConfig.environment, timeoutMillis );

		if( compileExitCode != 0 ) {
			if( compileExitCode == 124 ) {
				fmt::print( fg( fmt::color::yellow ), "  TIMEOUT" );
				fmt::print( ": {} (compile)\n", testName );
				timedOutCount++;
			}
			else {
				fmt::print( fg( fmt::color::red ), "  FAIL" );
				fmt::print( ": {} (compile error)\n", testName );
			}
			failedCount++;
			failedTests.push_back( testName );
			continue;
		}

		int32_t runExitCode = runArgvCommand(
			{ outputBinary }, manifest.testConfig.environment, timeoutMillis );
		std::filesystem::remove( outputBinary );

		if( runExitCode == 0 ) {
			fmt::print( fg( fmt::color::green ), "  PASS" );
			fmt::print( ": {}\n", testName );
			passedCount++;
		}
		else if( runExitCode == 124 ) {
			fmt::print( fg( fmt::color::yellow ), "  TIMEOUT" );
			fmt::print( ": {} (exceeded {}ms)\n", testName, timeoutMillis );
			timedOutCount++;
			failedCount++;
			failedTests.push_back( testName );
		}
		else {
			fmt::print( fg( fmt::color::red ), "  FAIL" );
			fmt::print( ": {} (exit code {})\n", testName, runExitCode );
			failedCount++;
			failedTests.push_back( testName );
		}
	}

	std::filesystem::remove_all( tempDir );

	fmt::print( "\n" );
	fmt::print( "Results: {} passed, {} failed", passedCount, failedCount );
	if( timedOutCount > 0 ) {
		fmt::print( " ({} timed out)", timedOutCount );
	}
	fmt::print( " out of {} tests\n", testFiles.size() );

	if( failedTests.empty() == false ) {
		fmt::print( "\nFailed tests:\n" );
		for( const std::string& failedName : failedTests ) {
			fmt::print( "  - {}\n", failedName );
		}
	}

	return failedCount > 0 ? 1 : 0;
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

	argparse::ArgumentParser testCommand( "test" );
	testCommand.add_description( "Run test script or compile and execute test files" );

	argparse::ArgumentParser cleanCommand( "clean" );
	cleanCommand.add_description( "Remove build artifacts" );

	program.add_subparser( initCommand );
	program.add_subparser( installCommand );
	program.add_subparser( buildCommand );
	program.add_subparser( runCommand );
	program.add_subparser( listCommand );
	program.add_subparser( lockCommand );
	program.add_subparser( updateCommand );
	program.add_subparser( testCommand );
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

	if( program.is_subcommand_used( "test" ) ) {
		return commandTest();
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
