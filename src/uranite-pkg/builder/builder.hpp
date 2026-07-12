
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#ifndef _URANITE_PKG_BUILDER_HPP_
#define _URANITE_PKG_BUILDER_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "../manifest/manifest.hpp"
#include "../resolver/resolver.hpp"
#include "../cache/cache.hpp"

namespace uranite::pkg {

	struct BuildResult {
		bool succeeded = false;
		std::string outputPath;
		std::string errorMessage;
		int32_t exitCode = 0;
	};

	class BuildOrchestrator {

		public:

			BuildOrchestrator( const PackageManifest& manifest, const std::vector<ResolvedDependency>& resolvedDependencies,
				PackageCache& packageCache );

			BuildResult build();
			BuildResult buildAndRun( const std::vector<std::string>& programArguments );

		private:

			std::vector<std::string> collectModulePaths() const;
			std::vector<std::string> collectLinkLibraries() const;
			std::string resolveOutputPath() const;
			int32_t executeProgram( const std::string& executablePath, const std::vector<std::string>& programArguments ) const;

			PackageManifest manifest_;
			std::vector<ResolvedDependency> resolvedDependencies_;
			PackageCache& packageCache_;
	};

} // namespace uranite::pkg

#endif // _URANITE_PKG_BUILDER_HPP_
