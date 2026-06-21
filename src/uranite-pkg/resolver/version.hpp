
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#ifndef _URANITE_PKG_VERSION_HPP_
#define _URANITE_PKG_VERSION_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace uranite::pkg {

	struct SemanticVersion {
		uint32_t majorVersion = 0;
		uint32_t minorVersion = 0;
		uint32_t patchVersion = 0;
		std::string prereleaseLabel;

		bool operator==( const SemanticVersion& other ) const;
		bool operator<( const SemanticVersion& other ) const;
		bool operator<=( const SemanticVersion& other ) const;
		bool operator>( const SemanticVersion& other ) const;
		bool operator>=( const SemanticVersion& other ) const;

		std::string toString() const;
	};

	enum class ConstraintOperator {
		Exact,
		GreaterThan,
		GreaterThanOrEqual,
		LessThan,
		LessThanOrEqual,
		Caret,
		Tilde
	};

	struct VersionConstraint {
		ConstraintOperator constraintOperator = ConstraintOperator::Caret;
		SemanticVersion constraintVersion;
	};

	class VersionParser {

		public:

			static SemanticVersion parse( const std::string& versionString );
			static std::vector<VersionConstraint> parseConstraint( const std::string& constraintString );
			static bool satisfies( const SemanticVersion& candidateVersion, const std::vector<VersionConstraint>& constraints );
			static SemanticVersion findHighestCompatible( const std::vector<SemanticVersion>& availableVersions,
				const std::vector<VersionConstraint>& constraints );

		private:

			static bool satisfiesSingle( const SemanticVersion& candidateVersion, const VersionConstraint& constraint );
	};

} // namespace uranite::pkg

#endif // _URANITE_PKG_VERSION_HPP_
