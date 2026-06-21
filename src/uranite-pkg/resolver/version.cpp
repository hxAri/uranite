
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
#include <sstream>

#include "version.hpp"

namespace uranite::pkg {

	bool SemanticVersion::operator==( const SemanticVersion& other ) const {
		return this->majorVersion == other.majorVersion &&
			this->minorVersion == other.minorVersion &&
			this->patchVersion == other.patchVersion &&
			this->prereleaseLabel == other.prereleaseLabel;
	}

	bool SemanticVersion::operator<( const SemanticVersion& other ) const {
		if( this->majorVersion != other.majorVersion ) {
			return this->majorVersion < other.majorVersion;
		}
		if( this->minorVersion != other.minorVersion ) {
			return this->minorVersion < other.minorVersion;
		}
		if( this->patchVersion != other.patchVersion ) {
			return this->patchVersion < other.patchVersion;
		}
		if( this->prereleaseLabel.empty() && other.prereleaseLabel.empty() ) {
			return false;
		}
		if( this->prereleaseLabel.empty() ) {
			return false;
		}
		if( other.prereleaseLabel.empty() ) {
			return true;
		}
		return this->prereleaseLabel < other.prereleaseLabel;
	}

	bool SemanticVersion::operator<=( const SemanticVersion& other ) const {
		return *this == other || *this < other;
	}

	bool SemanticVersion::operator>( const SemanticVersion& other ) const {
		return other < *this;
	}

	bool SemanticVersion::operator>=( const SemanticVersion& other ) const {
		return *this == other || *this > other;
	}

	std::string SemanticVersion::toString() const {
		std::string result = std::to_string( this->majorVersion ) + "." +
			std::to_string( this->minorVersion ) + "." +
			std::to_string( this->patchVersion );
		if( this->prereleaseLabel.empty() == false ) {
			result+= "-" + this->prereleaseLabel;
		}
		return result;
	}

	SemanticVersion VersionParser::parse( const std::string& versionString ) {
		SemanticVersion version;
		std::string input = versionString;

		if( input.empty() == false && input[0] == 'v' ) {
			input = input.substr( 1 );
		}

		size_t prereleasePosition = input.find( '-' );
		std::string numericPart = input;
		if( prereleasePosition != std::string::npos ) {
			version.prereleaseLabel = input.substr( prereleasePosition + 1 );
			numericPart = input.substr( 0, prereleasePosition );
		}

		std::istringstream numericStream( numericPart );
		std::string segment;

		if( std::getline( numericStream, segment, '.' ) ) {
			version.majorVersion = static_cast<uint32_t>( std::stoul( segment ) );
		}
		if( std::getline( numericStream, segment, '.' ) ) {
			version.minorVersion = static_cast<uint32_t>( std::stoul( segment ) );
		}
		if( std::getline( numericStream, segment, '.' ) ) {
			version.patchVersion = static_cast<uint32_t>( std::stoul( segment ) );
		}

		return version;
	}

	std::vector<VersionConstraint> VersionParser::parseConstraint( const std::string& constraintString ) {
		std::vector<VersionConstraint> constraints;

		std::istringstream constraintStream( constraintString );
		std::string singleConstraint;

		while( std::getline( constraintStream, singleConstraint, ',' ) ) {
			size_t startPosition = singleConstraint.find_first_not_of( " \t" );
			if( startPosition == std::string::npos ) {
				continue;
			}
			singleConstraint = singleConstraint.substr( startPosition );

			VersionConstraint constraint;

			if( singleConstraint[0] == '^' ) {
				constraint.constraintOperator = ConstraintOperator::Caret;
				constraint.constraintVersion = parse( singleConstraint.substr( 1 ) );
			}
			else if( singleConstraint[0] == '~' ) {
				constraint.constraintOperator = ConstraintOperator::Tilde;
				constraint.constraintVersion = parse( singleConstraint.substr( 1 ) );
			}
			else if( singleConstraint.size() >= 2 && singleConstraint[0] == '>' && singleConstraint[1] == '=' ) {
				constraint.constraintOperator = ConstraintOperator::GreaterThanOrEqual;
				std::string versionPart = singleConstraint.substr( 2 );
				size_t versionStart = versionPart.find_first_not_of( " \t" );
				if( versionStart != std::string::npos ) {
					versionPart = versionPart.substr( versionStart );
				}
				constraint.constraintVersion = parse( versionPart );
			}
			else if( singleConstraint.size() >= 2 && singleConstraint[0] == '<' && singleConstraint[1] == '=' ) {
				constraint.constraintOperator = ConstraintOperator::LessThanOrEqual;
				std::string versionPart = singleConstraint.substr( 2 );
				size_t versionStart = versionPart.find_first_not_of( " \t" );
				if( versionStart != std::string::npos ) {
					versionPart = versionPart.substr( versionStart );
				}
				constraint.constraintVersion = parse( versionPart );
			}
			else if( singleConstraint[0] == '>' ) {
				constraint.constraintOperator = ConstraintOperator::GreaterThan;
				std::string versionPart = singleConstraint.substr( 1 );
				size_t versionStart = versionPart.find_first_not_of( " \t" );
				if( versionStart != std::string::npos ) {
					versionPart = versionPart.substr( versionStart );
				}
				constraint.constraintVersion = parse( versionPart );
			}
			else if( singleConstraint[0] == '<' ) {
				constraint.constraintOperator = ConstraintOperator::LessThan;
				std::string versionPart = singleConstraint.substr( 1 );
				size_t versionStart = versionPart.find_first_not_of( " \t" );
				if( versionStart != std::string::npos ) {
					versionPart = versionPart.substr( versionStart );
				}
				constraint.constraintVersion = parse( versionPart );
			}
			else {
				constraint.constraintOperator = ConstraintOperator::Exact;
				constraint.constraintVersion = parse( singleConstraint );
			}

			constraints.push_back( constraint );
		}

		if( constraints.empty() ) {
			VersionConstraint fallback;
			fallback.constraintOperator = ConstraintOperator::GreaterThanOrEqual;
			fallback.constraintVersion = parse( "0.0.0" );
			constraints.push_back( fallback );
		}

		return constraints;
	}

	bool VersionParser::satisfiesSingle( const SemanticVersion& candidateVersion, const VersionConstraint& constraint ) {
		const SemanticVersion& constraintVersion = constraint.constraintVersion;

		switch( constraint.constraintOperator ) {
			case ConstraintOperator::Exact:
				return candidateVersion == constraintVersion;

			case ConstraintOperator::GreaterThan:
				return candidateVersion > constraintVersion;

			case ConstraintOperator::GreaterThanOrEqual:
				return candidateVersion >= constraintVersion;

			case ConstraintOperator::LessThan:
				return candidateVersion < constraintVersion;

			case ConstraintOperator::LessThanOrEqual:
				return candidateVersion <= constraintVersion;

			case ConstraintOperator::Caret: {
				if( candidateVersion < constraintVersion ) {
					return false;
				}
				if( constraintVersion.majorVersion > 0 ) {
					return candidateVersion.majorVersion == constraintVersion.majorVersion;
				}
				if( constraintVersion.minorVersion > 0 ) {
					return candidateVersion.majorVersion == 0 &&
						candidateVersion.minorVersion == constraintVersion.minorVersion;
				}
				return candidateVersion == constraintVersion;
			}

			case ConstraintOperator::Tilde: {
				if( candidateVersion < constraintVersion ) {
					return false;
				}
				return candidateVersion.majorVersion == constraintVersion.majorVersion &&
					candidateVersion.minorVersion == constraintVersion.minorVersion;
			}
		}

		return false;
	}

	bool VersionParser::satisfies( const SemanticVersion& candidateVersion, const std::vector<VersionConstraint>& constraints ) {
		for( const VersionConstraint& constraint : constraints ) {
			if( satisfiesSingle( candidateVersion, constraint ) == false ) {
				return false;
			}
		}
		return true;
	}

	SemanticVersion VersionParser::findHighestCompatible( const std::vector<SemanticVersion>& availableVersions,
		const std::vector<VersionConstraint>& constraints ) {

		std::vector<SemanticVersion> compatibleVersions;
		for( const SemanticVersion& version : availableVersions ) {
			if( satisfies( version, constraints ) ) {
				compatibleVersions.push_back( version );
			}
		}

		if( compatibleVersions.empty() ) {
			throw std::runtime_error( "no compatible version found" );
		}

		std::sort( compatibleVersions.begin(), compatibleVersions.end() );
		return compatibleVersions.back();
	}

} // namespace uranite::pkg
