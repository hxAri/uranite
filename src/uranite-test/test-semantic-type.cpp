
//
// @author hxAri (hxari)
// @create 2025-02-24 15:15
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

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <string>

#include "uranite/semantic/typeref.hpp"

/**
 * @class SemanticTypeTest
 * @brief Test fixture for the Semantic Type System component, inheriting from Google Test's ::testing::Test.
 *
 * This fixture sets up the foundational type registry environment required to test 
 * type creation, compatibility, casting, and resolution rules.
 */
class SemanticTypeTest : public ::testing::Test {
	
	protected:
		
		/**
		 * @brief Shared pointer to the centralized type registry.
		 * * The registry acts as the single source of truth for all type metadata 
		 * (e.g., primitives, user-defined structures, and custom objects), ensuring 
		 * type uniqueness and caching throughout the testing lifecycle.
		 */
		uranite::semantic::RegistrySharedPointer registry = std::make_shared<uranite::semantic::Registry>();
	
};

TEST_F( SemanticTypeTest, Any ) {
}

TEST_F( SemanticTypeTest, PrimitiveLookup ) {
	EXPECT_NE( this->registry->lookupPrimitive( "I32" ), nullptr );
	EXPECT_NE( this->registry->lookupPrimitive( "F64" ), nullptr );
	EXPECT_NE( this->registry->lookupPrimitive( "Boolean" ), nullptr );
	EXPECT_NE( this->registry->lookupPrimitive( "String" ), nullptr );
	EXPECT_EQ( this->registry->lookupPrimitive( "NonExistent" ), nullptr );
}

TEST_F( SemanticTypeTest, TypeArray ) {
	uranite::semantic::TypeSharedPointer i32( this->registry->getInteger32() );
	uranite::semantic::TypeSharedPointer array( this->registry->makeArray( i32, 10 ) );
	EXPECT_NE( array, nullptr );
	EXPECT_EQ( array->kind, uranite::semantic::Type::Kind::Array );
}

TEST_F( SemanticTypeTest, TypeAssignability ) {
	uranite::semantic::TypeSharedPointer i32( this->registry->getInteger32() );
	uranite::semantic::TypeSharedPointer i64( this->registry->getInteger64() );
	uranite::semantic::TypeSharedPointer f64( this->registry->getFloat64() );
	EXPECT_TRUE( this->registry->isAssignable( i64, i32 ) );  // widening
	EXPECT_TRUE( this->registry->isAssignable( f64, i32 ) );  // int to float
	EXPECT_TRUE( this->registry->isAssignable( i32, i32 ) );  // same type
}

TEST_F( SemanticTypeTest, TypeTuple ) {
	uranite::semantic::TypeSharedPointer i32( this->registry->getInteger32() );
	uranite::semantic::TypeSharedPointer tuple( this->registry->makeTuple( { i32, this->registry->getFloat64(), this->registry->getBool()} ) );
	EXPECT_NE( tuple, nullptr );
	EXPECT_EQ( tuple->kind, uranite::semantic::Type::Kind::Tuple );
}
