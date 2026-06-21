
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
#include <regex>
#include <stdexcept>

#include "cache.hpp"

namespace uranite::pkg {

	static bool isValidPackageName( const std::string& packageName ) {
		static const std::regex validPattern( "^[A-Za-z0-9_][A-Za-z0-9._-]{0,63}$" );
		return std::regex_match( packageName, validPattern );
	}

	static bool isPathContainedWithin( const std::string& targetPath, const std::string& parentPath ) {
		std::filesystem::path canonicalTarget = std::filesystem::weakly_canonical( targetPath );
		std::filesystem::path canonicalParent = std::filesystem::weakly_canonical( parentPath );
		std::string targetString = canonicalTarget.string();
		std::string parentString = canonicalParent.string();
		if( targetString.size() < parentString.size() ) {
			return false;
		}
		return targetString.compare( 0, parentString.size(), parentString ) == 0 &&
			( targetString.size() == parentString.size() || targetString[parentString.size()] == '/' );
	}

	PackageCache::PackageCache() {
		const char* homeDirectory = std::getenv( "HOME" );
		if( homeDirectory == nullptr ) {
			this->cacheRootDirectory_ = "/tmp/.uranite/cache";
		}
		else {
			this->cacheRootDirectory_ = std::string( homeDirectory ) + "/.uranite/cache";
		}
	}

	PackageCache::PackageCache( const std::string& cacheRootDirectory )
		: cacheRootDirectory_( cacheRootDirectory ) {}

	std::string PackageCache::buildPackageDirectoryName( const std::string& packageName,
		const SemanticVersion& version ) const {
		return packageName + "-" + version.toString();
	}

	std::string PackageCache::getCacheRootDirectory() const {
		return this->cacheRootDirectory_;
	}

	void PackageCache::ensureCacheDirectoryExists() const {
		if( std::filesystem::exists( this->cacheRootDirectory_ ) == false ) {
			std::filesystem::create_directories( this->cacheRootDirectory_ );
		}
	}

	bool PackageCache::isPackageCached( const std::string& packageName, const SemanticVersion& version ) const {
		std::string packageDirectoryName = this->buildPackageDirectoryName( packageName, version );
		std::string fullPath = this->cacheRootDirectory_ + "/" + packageDirectoryName;
		return std::filesystem::exists( fullPath ) && std::filesystem::is_directory( fullPath );
	}

	std::string PackageCache::getPackagePath( const std::string& packageName, const SemanticVersion& version ) const {
		std::string packageDirectoryName = this->buildPackageDirectoryName( packageName, version );
		return this->cacheRootDirectory_ + "/" + packageDirectoryName;
	}

	std::string PackageCache::getModulesPath( const std::string& packageName, const SemanticVersion& version ) const {
		std::string packagePath = this->getPackagePath( packageName, version );
		std::string modulesPath = packagePath + "/modules";
		if( std::filesystem::exists( modulesPath ) ) {
			return modulesPath;
		}
		return packagePath;
	}

	void PackageCache::storePackage( const std::string& packageName, const SemanticVersion& version,
		const std::string& sourceDirectoryPath ) {

		if( isValidPackageName( packageName ) == false ) {
			throw std::runtime_error( "invalid package name: " + packageName );
		}

		this->ensureCacheDirectoryExists();

		std::string packageDirectoryName = this->buildPackageDirectoryName( packageName, version );
		std::string targetPath = this->cacheRootDirectory_ + "/" + packageDirectoryName;

		if( isPathContainedWithin( targetPath, this->cacheRootDirectory_ ) == false ) {
			throw std::runtime_error( "path traversal detected for package: " + packageName );
		}

		if( std::filesystem::exists( targetPath ) ) {
			std::filesystem::remove_all( targetPath );
		}

		std::filesystem::copy( sourceDirectoryPath, targetPath,
			std::filesystem::copy_options::recursive );
	}

	void PackageCache::removePackage( const std::string& packageName, const SemanticVersion& version ) {
		if( isValidPackageName( packageName ) == false ) {
			throw std::runtime_error( "invalid package name: " + packageName );
		}

		std::string packageDirectoryName = this->buildPackageDirectoryName( packageName, version );
		std::string targetPath = this->cacheRootDirectory_ + "/" + packageDirectoryName;

		if( isPathContainedWithin( targetPath, this->cacheRootDirectory_ ) == false ) {
			throw std::runtime_error( "path traversal detected for package: " + packageName );
		}

		if( std::filesystem::exists( targetPath ) ) {
			std::filesystem::remove_all( targetPath );
		}
	}

	std::vector<CachedPackage> PackageCache::listCachedPackages() const {
		std::vector<CachedPackage> cachedPackages;

		if( std::filesystem::exists( this->cacheRootDirectory_ ) == false ) {
			return cachedPackages;
		}

		for( const std::filesystem::directory_entry& entry :
			std::filesystem::directory_iterator( this->cacheRootDirectory_ ) ) {
			if( entry.is_directory() == false ) {
				continue;
			}

			std::string directoryName = entry.path().filename().string();
			size_t lastDashPosition = directoryName.rfind( '-' );
			if( lastDashPosition == std::string::npos ) {
				continue;
			}

			CachedPackage cached;
			cached.packageName = directoryName.substr( 0, lastDashPosition );
			std::string versionString = directoryName.substr( lastDashPosition + 1 );

			try {
				cached.resolvedVersion = VersionParser::parse( versionString );
			}
			catch( ... ) {
				continue;
			}

			cached.cachedDirectoryPath = entry.path().string();
			cachedPackages.push_back( cached );
		}

		return cachedPackages;
	}

} // namespace uranite::pkg
