
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#include <algorithm>
#include <stdexcept>

#include "resolver.hpp"

namespace uranite::pkg {

	ResolutionResult DependencyResolver::resolve( const PackageManifest& manifest, bool includeDevelopmentDependencies ) {
		ResolutionResult result;
		std::unordered_map<std::string, ResolvedDependency> resolvedMap;
		std::unordered_set<std::string> visitedPackages;

		for( const PackageDependency& dependency : manifest.dependencies ) {
			this->resolveRecursive( dependency.packageName, dependency.versionConstraint,
				dependency.sourceRepository, resolvedMap, visitedPackages, result.conflictMessages );
		}

		if( includeDevelopmentDependencies ) {
			for( const PackageDependency& dependency : manifest.developmentDependencies ) {
				this->resolveRecursive( dependency.packageName, dependency.versionConstraint,
					dependency.sourceRepository, resolvedMap, visitedPackages, result.conflictMessages );
			}
		}

		for( std::pair<const std::string, ResolvedDependency>& resolvedPair : resolvedMap ) {
			result.resolvedPackages.push_back( resolvedPair.second );
		}

		result.hasConflicts = result.conflictMessages.empty() == false;
		return result;
	}

	ResolutionResult DependencyResolver::resolveFromLockfile( const Lockfile& lockfile ) {
		ResolutionResult result;

		for( const LockfileEntry& entry : lockfile.lockedEntries ) {
			ResolvedDependency resolved;
			resolved.packageName = entry.packageName;
			resolved.resolvedVersion = VersionParser::parse( entry.resolvedVersionString );
			resolved.sourceRepository = entry.sourceRepository;
			resolved.integrityHash = entry.sourceIntegrityHash;
			resolved.transitiveDependencyNames = entry.transitiveDependencyNames;
			result.resolvedPackages.push_back( resolved );
		}

		return result;
	}

	void DependencyResolver::resolveRecursive( const std::string& packageName, const std::string& versionConstraint,
		const std::string& sourceRepository, std::unordered_map<std::string, ResolvedDependency>& resolvedMap,
		std::unordered_set<std::string>& visitedPackages, std::vector<std::string>& conflictMessages ) {

		if( visitedPackages.count( packageName ) > 0 ) {
			if( resolvedMap.count( packageName ) > 0 ) {
				std::vector<VersionConstraint> constraints = VersionParser::parseConstraint( versionConstraint );
				ResolvedDependency& existingResolution = resolvedMap[packageName];
				if( VersionParser::satisfies( existingResolution.resolvedVersion, constraints ) == false ) {
					conflictMessages.push_back(
						"version conflict for " + packageName + ": resolved " +
						existingResolution.resolvedVersion.toString() + " but " +
						versionConstraint + " required" );
				}
			}
			return;
		}

		visitedPackages.insert( packageName );

		std::vector<VersionConstraint> constraints = VersionParser::parseConstraint( versionConstraint );
		std::vector<SemanticVersion> availableVersions = this->fetchAvailableVersions( packageName, sourceRepository );

		SemanticVersion selectedVersion;
		bool foundCompatible = false;

		if( availableVersions.empty() == false ) {
			try {
				selectedVersion = VersionParser::findHighestCompatible( availableVersions, constraints );
				foundCompatible = true;
			}
			catch( const std::runtime_error& ) {
				conflictMessages.push_back(
					"no compatible version found for " + packageName + " matching " + versionConstraint );
			}
		}
		else {
			selectedVersion = constraints[0].constraintVersion;
			foundCompatible = true;
		}

		if( foundCompatible == false ) {
			return;
		}

		ResolvedDependency resolved;
		resolved.packageName = packageName;
		resolved.resolvedVersion = selectedVersion;
		resolved.sourceRepository = sourceRepository;

		PackageManifest dependencyManifest = this->fetchPackageManifest( packageName, selectedVersion, sourceRepository );
		for( const PackageDependency& transitiveDependency : dependencyManifest.dependencies ) {
			resolved.transitiveDependencyNames.push_back( transitiveDependency.packageName );
			this->resolveRecursive( transitiveDependency.packageName, transitiveDependency.versionConstraint,
				transitiveDependency.sourceRepository, resolvedMap, visitedPackages, conflictMessages );
		}

		resolvedMap[packageName] = resolved;
	}

	std::vector<SemanticVersion> DependencyResolver::fetchAvailableVersions( const std::string& /*packageName*/,
		const std::string& /*sourceRepository*/ ) {
		// Registry fetch not yet implemented — returns empty, triggering constraint-version fallback.
		return {};
	}

	PackageManifest DependencyResolver::fetchPackageManifest( const std::string& /*packageName*/,
		const SemanticVersion& /*version*/, const std::string& /*sourceRepository*/ ) {
		// Registry fetch not yet implemented — returns empty manifest (no transitive deps).
		return PackageManifest{};
	}

	std::vector<std::string> DependencyResolver::topologicalSort( const std::vector<ResolvedDependency>& resolvedPackages ) {
		std::unordered_map<std::string, ResolvedDependency> resolvedMap;
		for( const ResolvedDependency& resolved : resolvedPackages ) {
			resolvedMap[resolved.packageName] = resolved;
		}

		std::unordered_set<std::string> visitedSet;
		std::unordered_set<std::string> recursionStack;
		std::vector<std::string> sortedOrder;

		for( const ResolvedDependency& resolved : resolvedPackages ) {
			if( visitedSet.count( resolved.packageName ) == 0 ) {
				this->topologicalSortVisit( resolved.packageName, resolvedMap,
					visitedSet, recursionStack, sortedOrder );
			}
		}

		return sortedOrder;
	}

	void DependencyResolver::topologicalSortVisit( const std::string& packageName,
		const std::unordered_map<std::string, ResolvedDependency>& resolvedMap,
		std::unordered_set<std::string>& visitedSet,
		std::unordered_set<std::string>& recursionStack,
		std::vector<std::string>& sortedOrder ) {

		if( recursionStack.count( packageName ) > 0 ) {
			throw std::runtime_error( "circular dependency detected: " + packageName );
		}
		if( visitedSet.count( packageName ) > 0 ) {
			return;
		}

		recursionStack.insert( packageName );

		std::unordered_map<std::string, ResolvedDependency>::const_iterator resolvedIterator = resolvedMap.find( packageName );
		if( resolvedIterator != resolvedMap.end() ) {
			for( const std::string& dependencyName : resolvedIterator->second.transitiveDependencyNames ) {
				this->topologicalSortVisit( dependencyName, resolvedMap, visitedSet, recursionStack, sortedOrder );
			}
		}

		recursionStack.erase( packageName );
		visitedSet.insert( packageName );
		sortedOrder.push_back( packageName );
	}

} // namespace uranite::pkg
