
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#ifndef _URANITE_PKG_RESOLVER_HPP_
#define _URANITE_PKG_RESOLVER_HPP_

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "version.hpp"
#include "../manifest/manifest.hpp"
#include "../manifest/lockfile.hpp"

namespace uranite::pkg {

	struct ResolvedDependency {
		std::string packageName;
		SemanticVersion resolvedVersion;
		std::string sourceRepository;
		std::string integrityHash;
		std::vector<std::string> transitiveDependencyNames;
	};

	struct ResolutionResult {
		std::vector<ResolvedDependency> resolvedPackages;
		bool hasConflicts = false;
		std::vector<std::string> conflictMessages;
	};

	class DependencyResolver {

		public:

			ResolutionResult resolve( const PackageManifest& manifest, bool includeDevelopmentDependencies );
			ResolutionResult resolveFromLockfile( const Lockfile& lockfile );
			std::vector<std::string> topologicalSort( const std::vector<ResolvedDependency>& resolvedPackages );

		private:

			void resolveRecursive( const std::string& packageName, const std::string& versionConstraint,
				const std::string& sourceRepository, std::unordered_map<std::string, ResolvedDependency>& resolvedMap,
				std::unordered_set<std::string>& visitedPackages, std::vector<std::string>& conflictMessages );

			std::vector<SemanticVersion> fetchAvailableVersions( const std::string& packageName,
				const std::string& sourceRepository );

			PackageManifest fetchPackageManifest( const std::string& packageName,
				const SemanticVersion& version, const std::string& sourceRepository );

			void topologicalSortVisit( const std::string& packageName,
				const std::unordered_map<std::string, ResolvedDependency>& resolvedMap,
				std::unordered_set<std::string>& visitedSet,
				std::unordered_set<std::string>& recursionStack,
				std::vector<std::string>& sortedOrder );
	};

} // namespace uranite::pkg

#endif // _URANITE_PKG_RESOLVER_HPP_
