
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#ifndef _URANITE_PKG_CACHE_HPP_
#define _URANITE_PKG_CACHE_HPP_

#include <string>
#include <vector>

#include "../resolver/version.hpp"

namespace uranite::pkg {

	struct CachedPackage {
		std::string packageName;
		SemanticVersion resolvedVersion;
		std::string cachedDirectoryPath;
		std::string sourceIntegrityHash;
	};

	class PackageCache {

		public:

			PackageCache();
			explicit PackageCache( const std::string& cacheRootDirectory );

			bool isPackageCached( const std::string& packageName, const SemanticVersion& version ) const;
			std::string getPackagePath( const std::string& packageName, const SemanticVersion& version ) const;
			std::string getModulesPath( const std::string& packageName, const SemanticVersion& version ) const;
			void storePackage( const std::string& packageName, const SemanticVersion& version,
				const std::string& sourceDirectoryPath );
			void removePackage( const std::string& packageName, const SemanticVersion& version );
			std::vector<CachedPackage> listCachedPackages() const;
			void ensureCacheDirectoryExists() const;
			std::string getCacheRootDirectory() const;

		private:

			std::string buildPackageDirectoryName( const std::string& packageName, const SemanticVersion& version ) const;
			std::string cacheRootDirectory_;
	};

} // namespace uranite::pkg

#endif // _URANITE_PKG_CACHE_HPP_
