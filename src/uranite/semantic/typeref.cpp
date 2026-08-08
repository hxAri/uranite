
//
// @author hxAri (hxari)
// @create 2025-02-24 15:15
// @update 2026-06-17 20:03
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

#include <algorithm>
#include <set>

#include "uranite/semantic/typeref.hpp"

namespace uranite::semantic {
	
	Registry::Registry() {
		this->booleanType = std::make_shared<Type>( Type::Kind::Bool, "bool", "uranite.builtin", qualname::PrimBool );
		this->charType = std::make_shared<Type>( Type::Kind::Char, "char", "uranite.builtin", qualname::PrimChar );
		this->errorType = std::make_shared<Type>( Type::Kind::Error, "<error>", "uranite.builtin", qualname::PrimError );
		this->float32Type = std::make_shared<FloatType>( 32 );
		this->float32Type->package = qualname::primitives::Package;
		this->float32Type->qualified = qualname::primitives::F32;
		this->float64Type = std::make_shared<FloatType>( 64 );
		this->float64Type->package = qualname::primitives::Package;
		this->float64Type->qualified = qualname::primitives::F64;
		this->integer16Type = std::make_shared<IntegerType>( 16, true );
		this->integer16Type->package = qualname::primitives::Package;
		this->integer16Type->qualified = qualname::primitives::I16;
		this->integer32Type = std::make_shared<IntegerType>( 32, true );
		this->integer32Type->package = qualname::primitives::Package;
		this->integer32Type->qualified = qualname::primitives::I32;
		this->integer64Type = std::make_shared<IntegerType>( 64, true );
		this->integer64Type->package = qualname::primitives::Package;
		this->integer64Type->qualified = qualname::primitives::I64;
		this->integer8Type = std::make_shared<IntegerType>( 8, true );
		this->integer8Type->package = qualname::primitives::Package;
		this->integer8Type->qualified = qualname::primitives::I8;
		this->objectType = std::make_shared<ClassType>( qualname::classes::object::Name );
		this->objectType->package = qualname::primitives::Package;
		this->objectType->qualified = qualname::Object;
		this->stringType = std::make_shared<Type>( Type::Kind::String, "str", "uranite.builtin", qualname::PrimString );
		this->unsigned16Type = std::make_shared<IntegerType>( 16, false );
		this->unsigned16Type->package = qualname::primitives::Package;
		this->unsigned16Type->qualified = qualname::primitives::U16;
		this->unsigned32Type = std::make_shared<IntegerType>( 32, false );
		this->unsigned32Type->package = qualname::primitives::Package;
		this->unsigned32Type->qualified = qualname::primitives::U32;
		this->unsigned64Type = std::make_shared<IntegerType>( 64, false );
		this->unsigned64Type->package = qualname::primitives::Package;
		this->unsigned64Type->qualified = qualname::primitives::U64;
		this->unsigned8Type = std::make_shared<IntegerType>( 8, false );
		this->unsigned8Type->package = qualname::primitives::Package;
		this->unsigned8Type->qualified = qualname::primitives::U8;
		this->voidType = std::make_shared<Type>( Type::Kind::Void, "void", "uranite.builtin", qualname::PrimVoid );
		this->primitivesTypes = {
			{ qualname::classes::boolean::Name, this->booleanType },
			{ qualname::classes::byte::Name, this->unsigned8Type },
			{ qualname::classes::Char::Name, this->charType },
			{ qualname::classes::Double::Name, this->float64Type },
			{ qualname::classes::f32::Name, this->float32Type },
			{ qualname::classes::f64::Name, this->float64Type },
			{ qualname::classes::Float::Name, this->float64Type },
			{ qualname::classes::i16::Name, this->integer16Type },
			{ qualname::classes::i32::Name, this->integer32Type },
			{ qualname::classes::i64::Name, this->integer64Type },
			{ qualname::classes::i8::Name, this->integer8Type },
			{ qualname::classes::Int::Name, this->integer64Type },
			{ qualname::classes::integer::Name, this->integer32Type },
			{ qualname::classes::Long::Name, this->integer64Type },
			{ qualname::classes::nonetype::Name, this->voidType },
			{ qualname::classes::string::Name, this->stringType },
			{ qualname::classes::u16::Name, this->unsigned16Type },
			{ qualname::classes::u32::Name, this->unsigned32Type },
			{ qualname::classes::u64::Name, this->unsigned64Type },
			{ qualname::classes::u8::Name, this->unsigned8Type },
			{ qualname::classes::uint::Name, this->unsigned64Type },
			{ qualname::classes::Void::Name, this->voidType }
		};
		this->userTypesType[qualname::classes::object::Name] = this->objectType;
	}
	
	/**
	 * @brief Compare two types for identity using fully-qualified names as the
	 *        authoritative key. Falls back to short-name comparison only when
	 *        qualified names are absent (primitives, error recovery, or
	 *        monomorphized types whose qualified name was not yet propagated).
	 *
	 * @param left  First type operand (not null).
	 * @param right Second type operand (not null).
	 * @return True when the two types represent the same logical type identity.
	 */
	static bool typeIdentityMatch( const TypeSharedPointer& left, const TypeSharedPointer& right ) {
		if( left->qualified.empty() == false && right->qualified.empty() == false ) {
			return left->qualified == right->qualified;
		}
		return left->name == right->name;
	}
	
	bool Registry::isAssignable( const TypeSharedPointer& target, const TypeSharedPointer& source ) const {
		if( target == nullptr || source == nullptr ) {
			return false;
		}
		if( target->isError() || source->isError() ) {
			return true;
		}
		if( target->equals( source ) ) {
			return true;
		}
		if( target->isVoid() && source->isVoid() ) {
			return true;
		}
		if( target->kind == Type::Kind::Class ) {
			ClassTypeSharedPointer classTargetType = std::static_pointer_cast<ClassType>( target );
			if( classTargetType->qualified == qualname::Object || classTargetType->name == "Object" ) {
				return true;
			}
		}
		if( target->kind == Type::Kind::Class && source->kind == Type::Kind::Class ) {
			ClassTypeSharedPointer targetClassType = std::static_pointer_cast<ClassType>( target );
			ClassTypeSharedPointer sourceClassType = std::static_pointer_cast<ClassType>( source );
			auto checkGenericCompatibility = [&]( const ClassTypeSharedPointer& targetCls, const ClassTypeSharedPointer& sourceCls ) -> bool {
				if( targetCls->genericParameters.empty() == false && sourceCls->genericParameters.empty() == false ) {
					if( targetCls->genericParameters.size() != sourceCls->genericParameters.size() ) {
						return false;
					}
					for( size_t genericIndex = 0; genericIndex < targetCls->genericParameters.size(); genericIndex++ ) {
						if( this->isAssignable( targetCls->genericParameters[genericIndex], sourceCls->genericParameters[genericIndex] ) == false ) {
							return false;
						}
					}
					return true;
				}
				if( targetCls->typeSubstitutions.empty() == false && sourceCls->typeSubstitutions.empty() == false ) {
					for( const std::pair<const std::string, TypeSharedPointer>& entry : targetCls->typeSubstitutions ) {
						std::unordered_map<std::string, TypeSharedPointer>::const_iterator it = sourceCls->typeSubstitutions.find( entry.first );
						if( it == sourceCls->typeSubstitutions.end() ) {
							return false;
						}
						if( this->isAssignable( entry.second, it->second ) == false ) {
							return false;
						}
					}
					return true;
				}
				return true;
			};
			if( targetClassType->qualified.empty() == false && sourceClassType->qualified.empty() == false ) {
				if( targetClassType->qualified == sourceClassType->qualified ) {
					return checkGenericCompatibility( targetClassType, sourceClassType );
				}
			}
			if( targetClassType->astDeclaration && sourceClassType->astDeclaration &&
				targetClassType->astDeclaration->name == sourceClassType->astDeclaration->name ) {
				return checkGenericCompatibility( targetClassType, sourceClassType );
			}
			if( targetClassType->astDeclaration && targetClassType->astDeclaration->name == sourceClassType->name ) {
				return checkGenericCompatibility( targetClassType, sourceClassType );
			}
			if( sourceClassType->astDeclaration && sourceClassType->astDeclaration->name == targetClassType->name ) {
				return checkGenericCompatibility( targetClassType, sourceClassType );
			}
		}
		if( target->kind == Type::Kind::Interface && source->kind == Type::Kind::Class ) {
			ClassTypeSharedPointer sourceClassType = std::static_pointer_cast<ClassType>( source);
			InterfaceTypeSharedPointer targetInterfaceType = std::static_pointer_cast<InterfaceType>( target);
			std::vector<TypeSharedPointer> typesToCheck = sourceClassType->interfaces;
			if( sourceClassType->astDeclaration ) {
				TypeSharedPointer baseTemplateType = this->lookupType( sourceClassType->astDeclaration->name );
				if( baseTemplateType && baseTemplateType->kind == Type::Kind::Class ) {
					ClassTypeSharedPointer baseClassType = std::static_pointer_cast<ClassType>( baseTemplateType);
					for( TypeSharedPointer& interfaceType : baseClassType->interfaces ) {
						typesToCheck.push_back( interfaceType );
					}
				}
			}
			std::vector<TypeSharedPointer> checkedTypes;
			while( typesToCheck.empty() == false ) {
				TypeSharedPointer currentType = typesToCheck.back();
				typesToCheck.pop_back();
				if( currentType == nullptr || std::find( checkedTypes.begin(),checkedTypes.end(),currentType ) != checkedTypes.end() ) {
					continue;
				}
				checkedTypes.push_back( currentType );
				if( typeIdentityMatch( currentType, target ) ) {
					return true;
				}
				if( currentType->kind == Type::Kind::Interface ) {
					InterfaceTypeSharedPointer currentInterfaceType = std::static_pointer_cast<InterfaceType>( currentType );
					if( currentInterfaceType->astDeclaration && targetInterfaceType->astDeclaration && currentInterfaceType->astDeclaration == targetInterfaceType->astDeclaration ) {
						return true;
					}
					for( TypeSharedPointer& parentInterfaceType : currentInterfaceType->parentInterfaces ) {
						typesToCheck.push_back( parentInterfaceType );
					}
					if( currentInterfaceType->parentInterfaces.empty() && currentInterfaceType->astDeclaration ) {
						for( ast::nodes::TypeNodeSharedPointer& parentNode : currentInterfaceType->astDeclaration->parentInterfaces ) {
							if( parentNode->kind == ast::Node::Kind::SimpleType ) {
								ast::nodes::SimpleTypeNode& simpleTypeNode = static_cast<ast::nodes::SimpleTypeNode&>( *parentNode );
								TypeSharedPointer simpleType = this->lookupType( simpleTypeNode.name );
								if( simpleType ) {
									typesToCheck.push_back( simpleType );
								}
							}
							else if( parentNode->kind == ast::Node::Kind::GenericType ) {
								ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *parentNode );
								TypeSharedPointer genericType = this->lookupType( genericTypeNode.name );
								if( genericType ) {
									typesToCheck.push_back( genericType );
								}
							}
						}
					}
				}
			}
			TypeSharedPointer baseType = sourceClassType->baseClass;
			while( baseType && baseType->kind == Type::Kind::Class ) {
				ClassTypeSharedPointer baseClassType = std::static_pointer_cast<ClassType>( baseType );
				for( TypeSharedPointer& interfaceType : baseClassType->interfaces ) {
					if( typeIdentityMatch( interfaceType, target ) ) {
						return true;
					}
					if( interfaceType->kind == Type::Kind::Interface ) {
						InterfaceTypeSharedPointer interfaceImplementationType = std::static_pointer_cast<InterfaceType>( interfaceType );
						if( interfaceImplementationType->astDeclaration && targetInterfaceType->astDeclaration && interfaceImplementationType->astDeclaration == targetInterfaceType->astDeclaration ) {
							return true;
						}
					}
				}
				baseType = baseClassType->baseClass;
			}
		}
		if( target->kind == Type::Kind::Class && source->kind == Type::Kind::Interface ) {
			ClassTypeSharedPointer targetClassType = std::static_pointer_cast<ClassType>( target );
			InterfaceTypeSharedPointer sourceInterfaceType = std::static_pointer_cast<InterfaceType>( source );
			std::vector<TypeSharedPointer> typesToCheck = targetClassType->interfaces;
			if( targetClassType->astDeclaration ) {
				TypeSharedPointer baseTemplateType = this->lookupType( targetClassType->astDeclaration->name );
				if( baseTemplateType && baseTemplateType->kind == Type::Kind::Class ) {
					ClassTypeSharedPointer baseClassType = std::static_pointer_cast<ClassType>( baseTemplateType );
					for( TypeSharedPointer& interfaceType : baseClassType->interfaces ) {
						typesToCheck.push_back( interfaceType );
					}
				}
			}
			std::vector<TypeSharedPointer> checkedTypes;
			while( typesToCheck.empty() == false ) {
				TypeSharedPointer currentType = typesToCheck.back();
				typesToCheck.pop_back();
				if( currentType == nullptr || std::find( checkedTypes.begin(),checkedTypes.end(),currentType ) != checkedTypes.end() ) {
					continue;
				}
				checkedTypes.push_back( currentType );
				if( typeIdentityMatch( currentType, source ) ) {
					return true;
				}
				if( currentType->kind == Type::Kind::Interface ) {
					InterfaceTypeSharedPointer currentInterfaceType = std::static_pointer_cast<InterfaceType>( currentType );
					if( currentInterfaceType->astDeclaration && sourceInterfaceType->astDeclaration && currentInterfaceType->astDeclaration == sourceInterfaceType->astDeclaration ) {
						return true;
					}
					for( TypeSharedPointer& parentInterfaceType : currentInterfaceType->parentInterfaces ) {
						typesToCheck.push_back( parentInterfaceType );
					}
				}
			}
		}
		if( target->kind == Type::Kind::Interface && source->kind == Type::Kind::Interface ) {
			InterfaceTypeSharedPointer targetInterfaceType = std::static_pointer_cast<InterfaceType>( target );
			InterfaceTypeSharedPointer sourceInterfaceType = std::static_pointer_cast<InterfaceType>( source );
			if( sourceInterfaceType->astDeclaration && targetInterfaceType->astDeclaration && sourceInterfaceType->astDeclaration == targetInterfaceType->astDeclaration ) {
				return true;
			}
			std::vector<TypeSharedPointer> typesToCheck = sourceInterfaceType->parentInterfaces;
			if( typesToCheck.empty() && sourceInterfaceType->astDeclaration ) {
				for( ast::nodes::TypeNodeSharedPointer& parentNode : sourceInterfaceType->astDeclaration->parentInterfaces ) {
					if( parentNode->kind == ast::Node::Kind::SimpleType ) {
						ast::nodes::SimpleTypeNode& simpleTypeNode = static_cast<ast::nodes::SimpleTypeNode&>( *parentNode );
						TypeSharedPointer resolvedType = this->lookupType( simpleTypeNode.name );
						if( resolvedType ) {
							typesToCheck.push_back( resolvedType );
						}
					}
					else if( parentNode->kind == ast::Node::Kind::GenericType ) {
						ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *parentNode );
						TypeSharedPointer resolvedType = this->lookupType( genericTypeNode.name );
						if( resolvedType ) {
							typesToCheck.push_back( resolvedType );
						}
					}
				}
			}
			std::vector<TypeSharedPointer> checkedTypes;
			while( typesToCheck.empty() == false ) {
				TypeSharedPointer currentType = typesToCheck.back();
				typesToCheck.pop_back();
				if( currentType == nullptr || std::find( checkedTypes.begin(), checkedTypes.end(), currentType ) != checkedTypes.end() ) {
					continue;
				}
				checkedTypes.push_back( currentType );
				if( typeIdentityMatch( currentType, target ) ) {
					return true;
				}
				if( currentType->kind == Type::Kind::Interface ) {
					InterfaceTypeSharedPointer currentInterfaceType = std::static_pointer_cast<InterfaceType>( currentType );
					if( currentInterfaceType->astDeclaration && targetInterfaceType->astDeclaration && currentInterfaceType->astDeclaration == targetInterfaceType->astDeclaration ) {
						return true;
					}
					for( TypeSharedPointer& parentInterfaceType : currentInterfaceType->parentInterfaces ) {
						typesToCheck.push_back( parentInterfaceType );
					}
					if( currentInterfaceType->parentInterfaces.empty() && currentInterfaceType->astDeclaration ) {
						for( ast::nodes::TypeNodeSharedPointer& parentNode : currentInterfaceType->astDeclaration->parentInterfaces ) {
							if( parentNode->kind == ast::Node::Kind::SimpleType ) {
								ast::nodes::SimpleTypeNode& simpleTypeNode = static_cast<ast::nodes::SimpleTypeNode&>( *parentNode );
								TypeSharedPointer resolvedType = this->lookupType( simpleTypeNode.name );
								if( resolvedType ) {
									typesToCheck.push_back( resolvedType );
								}
							}
							else if( parentNode->kind == ast::Node::Kind::GenericType ) {
								ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *parentNode );
								TypeSharedPointer resolvedType = this->lookupType( genericTypeNode.name );
								if( resolvedType ) {
									typesToCheck.push_back( resolvedType );
								}
							}
						}
					}
				}
			}
		}
		if( target->kind == Type::Kind::Class && source->kind == Type::Kind::Class ) {
			ClassTypeSharedPointer sourceClassType = std::static_pointer_cast<ClassType>( source );
			if( sourceClassType->baseClass && typeIdentityMatch( sourceClassType->baseClass, target ) ) {
				return true;
			}
			if( sourceClassType->astDeclaration ) {
				TypeSharedPointer baseTemplateType = this->lookupType( sourceClassType->astDeclaration->name );
				if( baseTemplateType && baseTemplateType->kind == Type::Kind::Class ) {
					ClassTypeSharedPointer baseClassType = std::static_pointer_cast<ClassType>( baseTemplateType );
					if( baseClassType->baseClass && typeIdentityMatch( baseClassType->baseClass, target ) ) {
						return true;
					}
				}
			}
		}
		if( target->kind == Type::Kind::Integer && source->kind == Type::Kind::Integer ) {
			return true;
		}
		if( target->kind == Type::Kind::Class && source->kind == Type::Kind::Class ) {
			bool targetIsIntOop = qualname::isIntegerOop( target->qualified );
			bool sourceIsIntOop = qualname::isIntegerOop( source->qualified );
			bool targetIsFloatOop = qualname::isFloatOop( target->qualified );
			bool sourceIsFloatOop = qualname::isFloatOop( source->qualified );
			if( ( targetIsIntOop && sourceIsIntOop ) ||
				( targetIsFloatOop && sourceIsFloatOop ) ||
				( targetIsFloatOop && sourceIsIntOop ) ) {
				return true;
			}
		}
		if( target->isIntegral() && source->isIntegral() ) {
			return true;
		}
		if( target->isFloatingPoint() && source->isIntegral() ) {
			return true;
		}
		if( target->isFloatingPoint() && source->isFloatingPoint() ) {
			return true;
		}
		if( target->isIntegral() && source->isFloatingPoint() ) {
			return true;
		}
		if( target->kind == Type::Kind::Class && source->kind == Type::Kind::Class ) {
			bool targetIsIntOop = qualname::isIntegerOop( target->qualified );
			bool sourceIsFloatOop = qualname::isFloatOop( source->qualified );
			if( targetIsIntOop && sourceIsFloatOop ) {
				return true;
			}
		}
		if( target->kind == Type::Kind::Float && source->kind == Type::Kind::Float ) {
			FloatTypeSharedPointer targetFloatType = std::static_pointer_cast<FloatType>( target );
			FloatTypeSharedPointer sourceFloatType = std::static_pointer_cast<FloatType>( source );
			if( targetFloatType->bitWidth >= sourceFloatType->bitWidth ) {
				return true;
			}
		}
		if( target->kind == Type::Kind::Float && source->kind == Type::Kind::Integer ) {
			return true;
		}
		if( target->kind == Type::Kind::GenericParameter || source->kind == Type::Kind::GenericParameter ) {
			return true;
		}
		if( target->kind == Type::Kind::Optional ) {
			if( source->isVoid() || source->isNone() ) {
				return true;
			}
			OptionalTypeSharedPointer targetOptionalType = std::static_pointer_cast<OptionalType>( target );
			if( source->kind == Type::Kind::Optional ) {
				OptionalTypeSharedPointer sourceOptionalType = std::static_pointer_cast<OptionalType>( source );
				return this->isAssignable( targetOptionalType->inner,sourceOptionalType->inner );
			}
			return this->isAssignable( targetOptionalType->inner,source );
		}
		if( source->kind == Type::Kind::Optional ) {
			OptionalTypeSharedPointer sourceOptionalType = std::static_pointer_cast<OptionalType>( source );
			return this->isAssignable( target,sourceOptionalType->inner );
		}
		if( target->kind == Type::Kind::Reference ) {
			ReferenceTypeSharedPointer referenceTargetType = std::static_pointer_cast<ReferenceType>( target );
			return this->isAssignable( referenceTargetType->inner,source );
		}
		if( target->kind == Type::Kind::Class && source->kind == Type::Kind::Class ) {
			ClassTypeSharedPointer sourceClassType = std::static_pointer_cast<ClassType>( source );
			TypeSharedPointer baseType = sourceClassType->baseClass;
			while( baseType ) {
				if( typeIdentityMatch( baseType, target ) ) {
					return true;
				}
				if( baseType->kind == Type::Kind::Class ) {
					baseType = std::static_pointer_cast<ClassType>( baseType )->baseClass;
				}
				else {
					break;
				}
			}
			for( TypeSharedPointer& interfaceType : sourceClassType->interfaces ) {
				if( interfaceType && typeIdentityMatch( interfaceType, target ) ) {
					return true;
				}
			}
		}
		if( target->isPrimitive() && source->kind == Type::Kind::Class ) {
			static const std::unordered_map<std::string,std::string> oopToPrimitiveMap = {
				{qualname::classes::boolean::Name,"bool"},{qualname::classes::string::Name,"str"},{qualname::classes::Char::Name,"char"},
				{qualname::classes::byte::Name,"u8"},{qualname::classes::integer::Name,"i32"},{qualname::classes::Long::Name,"i64"},{qualname::classes::Double::Name,"f64"},
				{qualname::classes::i8::Name,"i8"},{qualname::classes::i16::Name,"i16"},{qualname::classes::i32::Name,"i32"},{qualname::classes::i64::Name,"i64"},
				{qualname::classes::u8::Name,"u8"},{qualname::classes::u16::Name,"u16"},{qualname::classes::u32::Name,"u32"},{qualname::classes::u64::Name,"u64"},
				{qualname::classes::f32::Name,"f32"},{qualname::classes::f64::Name,"f64"},
				{qualname::classes::Int::Name,"i64"},{qualname::classes::uint::Name,"u64"},{qualname::classes::Float::Name,"f64"}
			};
			std::unordered_map<std::string,std::string>::const_iterator mapIterator = oopToPrimitiveMap.find( source->name );
			if( mapIterator != oopToPrimitiveMap.end() && mapIterator->second == target->name ) {
				return true;
			}
		}
		if( source->isPrimitive() && target->kind == Type::Kind::Class ) {
			static const std::unordered_map<std::string,std::string> primitiveToOopMap = {
				{"bool",qualname::classes::boolean::Name},{"str",qualname::classes::string::Name},{"char",qualname::classes::Char::Name},
				{"i8",qualname::classes::i8::Name},{"i16",qualname::classes::i16::Name},{"i32",qualname::classes::i32::Name},{"i64",qualname::classes::i64::Name},
				{"u8",qualname::classes::u8::Name},{"u16",qualname::classes::u16::Name},{"u32",qualname::classes::u32::Name},{"u64",qualname::classes::u64::Name},
				{"f32",qualname::classes::f32::Name},{"f64",qualname::classes::f64::Name}
			};
			std::unordered_map<std::string,std::string>::const_iterator mapIterator = primitiveToOopMap.find( source->name );
			if( mapIterator != primitiveToOopMap.end() && mapIterator->second == target->name ) {
				return true;
			}
			static const std::unordered_map<std::string,std::string> oopToPrimitiveReverseMap = {
				{qualname::classes::boolean::Name,"bool"},{qualname::classes::string::Name,"str"},{qualname::classes::Char::Name,"char"},
				{qualname::classes::byte::Name,"u8"},{qualname::classes::integer::Name,"i32"},{qualname::classes::Long::Name,"i64"},{qualname::classes::Double::Name,"f64"},
				{qualname::classes::i8::Name,"i8"},{qualname::classes::i16::Name,"i16"},{qualname::classes::i32::Name,"i32"},{qualname::classes::i64::Name,"i64"},
				{qualname::classes::u8::Name,"u8"},{qualname::classes::u16::Name,"u16"},{qualname::classes::u32::Name,"u32"},{qualname::classes::u64::Name,"u64"},
				{qualname::classes::f32::Name,"f32"},{qualname::classes::f64::Name,"f64"},
				{qualname::classes::Int::Name,"i64"},{qualname::classes::uint::Name,"u64"},{qualname::classes::Float::Name,"f64"}
			};
			std::unordered_map<std::string,std::string>::const_iterator reverseMapIterator = oopToPrimitiveReverseMap.find( target->name );
			if( reverseMapIterator != oopToPrimitiveReverseMap.end() && reverseMapIterator->second == source->name ) {
				return true;
			}
			if( source->kind == Type::Kind::Integer && qualname::isIntegerOop( target->qualified ) ) {
				return true;
			}
			if( source->kind == Type::Kind::Float && qualname::isFloatOop( target->qualified ) ) {
				return true;
			}
		}
		if( target->kind == Type::Kind::Meta && source->kind == Type::Kind::Meta ) {
			MetaTypeSharedPointer targetMetaType = std::static_pointer_cast<MetaType>( target );
			MetaTypeSharedPointer sourceMetaType = std::static_pointer_cast<MetaType>( source );
			return this->isAssignable( targetMetaType->innerType,sourceMetaType->innerType );
		}
		if( target->kind == Type::Kind::Future && source->kind == Type::Kind::Future ) {
			FutureTypeSharedPointer targetFutureType = std::static_pointer_cast<FutureType>( target );
			FutureTypeSharedPointer sourceFutureType = std::static_pointer_cast<FutureType>( source );
			return this->isAssignable( targetFutureType->innerType,sourceFutureType->innerType );
		}
		if( target->kind == Type::Kind::Union ) {
			UnionTypeSharedPointer unionTargetType = std::static_pointer_cast<UnionType>( target );
			for( TypeSharedPointer& memberType : unionTargetType->types ) {
				if( this->isAssignable( memberType,source ) ) {
					return true;
				}
			}
		}
		if( source->kind == Type::Kind::Union ) {
			UnionTypeSharedPointer unionSourceType = std::static_pointer_cast<UnionType>( source );
			bool isAllMemberAssignable = true;
			for( TypeSharedPointer& memberType : unionSourceType->types ) {
				if( this->isAssignable( target,memberType ) == false ) {
					isAllMemberAssignable = false;
					break;
				}
			}
			if( isAllMemberAssignable ) {
				return true;
			}
		}
		if( target->kind == Type::Kind::Callable && source->kind == Type::Kind::Function ) {
			CallableTypeSharedPointer targetCallableType = std::static_pointer_cast<CallableType>( target );
			FunctionTypeSharedPointer sourceFunctionType = std::static_pointer_cast<FunctionType>( source );
			if( targetCallableType->parameterTypes.size() != sourceFunctionType->parameterTypes.size() ) {
				return false;
			}
			for( size_t parameterIndex = 0; parameterIndex < targetCallableType->parameterTypes.size(); parameterIndex++ ) {
				if( this->isAssignable( targetCallableType->parameterTypes[parameterIndex],sourceFunctionType->parameterTypes[parameterIndex] ) == false ) {
					return false;
				}
			}
			return this->isAssignable( targetCallableType->returnType,sourceFunctionType->returnType );
		}
		if( target->kind == Type::Kind::Function && source->kind == Type::Kind::Callable ) {
			FunctionTypeSharedPointer targetFunctionType = std::static_pointer_cast<FunctionType>( target );
			CallableTypeSharedPointer sourceCallableType = std::static_pointer_cast<CallableType>( source );
			if( targetFunctionType->parameterTypes.size() != sourceCallableType->parameterTypes.size() ) {
				return false;
			}
			for( size_t parameterIndex = 0; parameterIndex < targetFunctionType->parameterTypes.size(); parameterIndex++ ) {
				if( this->isAssignable( targetFunctionType->parameterTypes[parameterIndex],sourceCallableType->parameterTypes[parameterIndex] ) == false ) {
					return false;
				}
			}
			return this->isAssignable( targetFunctionType->returnType,sourceCallableType->returnType );
		}
		if( target->kind == Type::Kind::Callable && source->kind == Type::Kind::Callable ) {
			CallableTypeSharedPointer targetCallableType = std::static_pointer_cast<CallableType>( target );
			CallableTypeSharedPointer sourceCallableType = std::static_pointer_cast<CallableType>( source );
			if( targetCallableType->parameterTypes.size() != sourceCallableType->parameterTypes.size() ) {
				return false;
			}
			for( size_t parameterIndex = 0; parameterIndex < targetCallableType->parameterTypes.size(); parameterIndex++ ) {
				if( this->isAssignable( targetCallableType->parameterTypes[parameterIndex],sourceCallableType->parameterTypes[parameterIndex] ) == false ) {
					return false;
				}
			}
			return this->isAssignable( targetCallableType->returnType,sourceCallableType->returnType );
		}
		return false;
	}
	
	bool Registry::isComparable( const TypeSharedPointer& x, const TypeSharedPointer& y ) const {
		if( x == nullptr || y == nullptr ) {
			return false;
		}
		if( x->isError() || y->isError() ) {
			return true;
		}
		if( x->kind == Type::Kind::Reference ) {
			return this->isComparable( std::static_pointer_cast<ReferenceType>( x )->inner, y );
		}
		if( y->kind == Type::Kind::Reference ) {
			return this->isComparable( x, std::static_pointer_cast<ReferenceType>( y )->inner );
		}
		if( x->isNumeric() && y->isNumeric() ) {
			return true;
		}
		if( x->equals( y ) ) {
			return true;
		}
		if( x->kind == Type::Kind::Optional && ( y->isVoid() || y->isNone() ) ) {
			return true;
		}
		if( y->kind == Type::Kind::Optional && ( x->isVoid() || x->isNone() ) ) {
			return true;
		}
		if( x->isNone() || y->isNone() ) {
			return true;
		}
		if( x->kind == Type::Kind::Optional ) {
			OptionalTypeSharedPointer optionalType = std::static_pointer_cast<OptionalType>( x );
			return this->isComparable( optionalType->inner, y );
		}
		if( y->kind == Type::Kind::Optional ) {
			OptionalTypeSharedPointer optionalType = std::static_pointer_cast<OptionalType>( y );
			return this->isComparable( x, optionalType->inner );
		}
		if( x->kind == Type::Kind::Meta && y->kind == Type::Kind::Meta ) {
			return true;
		}
		if( this->isAssignable( x, y ) || this->isAssignable( y, x ) ) {
			return true;
		}
		return false;
	}
	
	TypeSharedPointer Registry::lookupPrimitive( const std::string& name ) const {
		std::unordered_map<std::string,TypeSharedPointer>::const_iterator primitiveIterator = this->primitivesTypes.find( name );
		if( primitiveIterator != this->primitivesTypes.end() ) {
			return primitiveIterator->second;
		}
		return nullptr;
	}
	
	TypeSharedPointer Registry::lookupType( const std::string& name ) const {
		std::unordered_map<std::string,TypeSharedPointer>::const_iterator typeIterator = this->userTypesType.find( name );
		if( typeIterator != this->userTypesType.end() ) {
			return typeIterator->second;
		}
		std::unordered_map<std::string,std::string>::const_iterator aliasIterator = this->typeAliases.find( name );
		if( aliasIterator != this->typeAliases.end() ) {
			typeIterator = this->userTypesType.find( aliasIterator->second );
			if( typeIterator != this->userTypesType.end() ) {
				return typeIterator->second;
			}
		}
		return this->lookupPrimitive( name );
	}
	
	TypeSharedPointer Registry::makeArray( TypeSharedPointer element, int64_t size ) {
		return std::make_shared<ArrayType>( std::move( element ), size );
	}
	
	TypeSharedPointer Registry::makeCallable( TypeSharedPointer returnType, std::vector<TypeSharedPointer> parameters ) {
		return std::make_shared<CallableType>( std::move( returnType ), std::move( parameters ) );
	}
	
	TypeSharedPointer Registry::makeFunction( std::vector<TypeSharedPointer> parameters, TypeSharedPointer returnType, bool isVariadic ) {
		return std::make_shared<FunctionType>( std::move( parameters ), std::move( returnType ), isVariadic );
	}
	
	TypeSharedPointer Registry::makeFuture( TypeSharedPointer inner ) {
		return std::make_shared<FutureType>( std::move( inner ) );
	}
	
	TypeSharedPointer Registry::makeGenerator( TypeSharedPointer yieldType ) {
		return std::make_shared<GeneratorType>( std::move( yieldType ) );
	}
	
	TypeSharedPointer Registry::makeMeta( TypeSharedPointer inner ) {
		return std::make_shared<MetaType>( std::move( inner ) );
	}
	
	TypeSharedPointer Registry::makeOptional( TypeSharedPointer inner ) {
		return std::make_shared<OptionalType>( std::move( inner ) );
	}
	
	TypeSharedPointer Registry::makePointer( TypeSharedPointer inner, bool mutableT ) {
		return std::make_shared<PointerType>( std::move( inner ), mutableT );
	}
	
	TypeSharedPointer Registry::makeReference( TypeSharedPointer inner, bool mutableT ) {
		return std::make_shared<ReferenceType>( std::move( inner ), mutableT );
	}
	
	TypeSharedPointer Registry::makeTuple( std::vector<TypeSharedPointer> elements ) {
		return std::make_shared<TupleType>( std::move( elements ) );
	}
	
	TypeSharedPointer Registry::makeUnion( std::vector<TypeSharedPointer> types ) {
		return std::make_shared<UnionType>( std::move( types ) );
	}
	
	void Registry::registerType( const std::string& name, TypeSharedPointer type ) {
		this->userTypesType[name] = type;
		if( type->qualified.empty() == false && type->qualified != name ) {
			this->userTypesType[type->qualified] = type;
			this->typeAliases[name] = type->qualified;
		}
	}
	
	void Registry::unregisterType( const std::string& name ) {
		this->userTypesType.erase( name );
	}
	
	void Registry::registerAlias( const std::string& shortName, const std::string& qualifiedName ) {
		this->typeAliases[shortName] = qualifiedName;
	}
	
	std::string Registry::resolveAlias( const std::string& name ) const {
		std::unordered_map<std::string,std::string>::const_iterator aliasIterator = this->typeAliases.find( name );
		if( aliasIterator != this->typeAliases.end() ) {
			return aliasIterator->second;
		}
		return name;
	}
	
	bool ClassType::implementsInterface( const std::string& qualifiedName ) const {
		for( const TypeSharedPointer& iface : this->interfaces ) {
			if( iface->qualified == qualifiedName ||
				qualname::startsWith( iface->qualified, qualifiedName ) ) {
				return true;
			}
			if( iface->kind == Type::Kind::Interface ) {
				if( std::static_pointer_cast<InterfaceType>( iface )->extendsInterface( qualifiedName ) ) {
					return true;
				}
			}
		}
		if( this->baseClass && this->baseClass->kind == Type::Kind::Class ) {
			return std::static_pointer_cast<ClassType>( this->baseClass )->implementsInterface( qualifiedName );
		}
		return false;
	}
	
	std::vector<std::string> Registry::typeNames() const {
		std::vector<std::string> names;
		for( std::pair<std::string,TypeSharedPointer> primitiveEntry : this->primitivesTypes ) {
			names.push_back( primitiveEntry.first );
		}
		for( std::pair<std::string,TypeSharedPointer> userTypeEntry : this->userTypesType ) {
			names.push_back( userTypeEntry.first );
		}
		return names;
	}
	
}
