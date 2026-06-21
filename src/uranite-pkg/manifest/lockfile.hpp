
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#ifndef _URANITE_PKG_LOCKFILE_HPP_
#define _URANITE_PKG_LOCKFILE_HPP_

#include <string>
#include <vector>

namespace uranite::pkg {

	struct LockfileEntry {
		std::string packageName;
		std::string resolvedVersionString;
		std::string sourceIntegrityHash;
		std::string sourceRepository;
		std::vector<std::string> transitiveDependencyNames;
	};

	struct Lockfile {
		std::string lockfileFormatVersion = "1";
		std::vector<LockfileEntry> lockedEntries;
	};

	class LockfileManager {

		public:

			static Lockfile readFromFile( const std::string& filePath );
			static void writeToFile( const Lockfile& lockfile, const std::string& filePath );
			static bool exists( const std::string& filePath );

		private:

			static std::string escapeYamlString( const std::string& input );
	};

} // namespace uranite::pkg

#endif // _URANITE_PKG_LOCKFILE_HPP_
