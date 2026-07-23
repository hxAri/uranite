
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
		this->booleanType = std::make_shared<Type>( Type::Kind::Bool, "bool", "uranite.builtin", qname::PRIM_BOOL );
		this->charType = std::make_shared<Type>( Type::Kind::Char, "char", "uranite.builtin", qname::PRIM_CHAR );
		this->errorType = std::make_shared<Type>( Type::Kind::Error, "<error>", "uranite.builtin", qname::PRIM_ERROR );
		this->float32Type = std::make_shared<FloatType>( 32 );
		this->float32Type->package = "uranite.builtin";
		this->float32Type->qualified = "uranite.builtin.f32";
		this->float64Type = std::make_shared<FloatType>( 64 );
		this->float64Type->package = "uranite.builtin";
		this->float64Type->qualified = "uranite.builtin.f64";
		this->integer16Type = std::make_shared<IntegerType>( 16, true );
		this->integer16Type->package = "uranite.builtin";
		this->integer16Type->qualified = "uranite.builtin.i16";
		this->integer32Type = std::make_shared<IntegerType>( 32, true );
		this->integer32Type->package = "uranite.builtin";
		this->integer32Type->qualified = "uranite.builtin.i32";
		this->integer64Type = std::make_shared<IntegerType>( 64, true );
		this->integer64Type->package = "uranite.builtin";
		this->integer64Type->qualified = "uranite.builtin.i64";
		this->integer8Type = std::make_shared<IntegerType>( 8, true );
		this->integer8Type->package = "uranite.builtin";
		this->integer8Type->qualified = "uranite.builtin.i8";
		this->objectType = std::make_shared<ClassType>( "Object" );
		this->objectType->package = "uranite.builtin";
		this->objectType->qualified = qname::OBJECT;
		this->stringType = std::make_shared<Type>( Type::Kind::String, "str", "uranite.builtin", qname::PRIM_STRING );
		this->unsigned16Type = std::make_shared<IntegerType>( 16, false );
		this->unsigned16Type->package = "uranite.builtin";
		this->unsigned16Type->qualified = "uranite.builtin.u16";
		this->unsigned32Type = std::make_shared<IntegerType>( 32, false );
		this->unsigned32Type->package = "uranite.builtin";
		this->unsigned32Type->qualified = "uranite.builtin.u32";
		this->unsigned64Type = std::make_shared<IntegerType>( 64, false );
		this->unsigned64Type->package = "uranite.builtin";
		this->unsigned64Type->qualified = "uranite.builtin.u64";
		this->unsigned8Type = std::make_shared<IntegerType>( 8, false );
		this->unsigned8Type->package = "uranite.builtin";
		this->unsigned8Type->qualified = "uranite.builtin.u8";
		this->voidType = std::make_shared<Type>( Type::Kind::Void, "void", "uranite.builtin", qname::PRIM_VOID );
		this->primitivesTypes = {
			{ "Boolean", this->booleanType },
			{ "Byte", this->unsigned8Type },
			{ "Char", this->charType },
			{ "Double", this->float64Type },
			{ "F32", this->float32Type },
			{ "F64", this->float64Type },
			{ "Float", this->float64Type },
			{ "I16", this->integer16Type },
			{ "I32", this->integer32Type },
			{ "I64", this->integer64Type },
			{ "I8", this->integer8Type },
			{ "Int", this->integer64Type },
			{ "Integer", this->integer32Type },
			{ "Long", this->integer64Type },
			{ "NoneType", this->voidType },
			{ "String", this->stringType },
			{ "U16", this->unsigned16Type },
			{ "U32", this->unsigned32Type },
			{ "U64", this->unsigned64Type },
			{ "U8", this->unsigned8Type },
			{ "UInt", this->unsigned64Type },
			{ "Void", this->voidType }
		};
		this->userTypesType["Object"] = this->objectType;
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
			if( classTargetType->qualified == qname::OBJECT || classTargetType->name == "Object" ) {
				return true;
			}
		}
		if( source->kind == Type::Kind::Class ) {
			ClassTypeSharedPointer classSourceType = std::static_pointer_cast<ClassType>( source );
			if( classSourceType->qualified == qname::OBJECT || classSourceType->name == "Object" ) {
				return true;
			}
		}
		if( target->kind == Type::Kind::Class && source->kind == Type::Kind::Class ) {
			ClassTypeSharedPointer targetClassType = std::static_pointer_cast<ClassType>( target );
			ClassTypeSharedPointer sourceClassType = std::static_pointer_cast<ClassType>( source );
			if( targetClassType->qualified.empty() == false && sourceClassType->qualified.empty() == false ) {
				if( targetClassType->qualified == sourceClassType->qualified ) {
					return true;
				}
			}
			if( targetClassType->astDeclaration && sourceClassType->astDeclaration &&
				targetClassType->astDeclaration->name == sourceClassType->astDeclaration->name ) {
				return true;
			}
			if( targetClassType->astDeclaration && targetClassType->astDeclaration->name == sourceClassType->name ) {
				return true;
			}
			if( sourceClassType->astDeclaration && sourceClassType->astDeclaration->name == targetClassType->name ) {
				return true;
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
					for( TypeSharedPointer& parentInterfaceType : currentInterfaceType->superInterfaces ) {
						typesToCheck.push_back( parentInterfaceType );
					}
					if( currentInterfaceType->superInterfaces.empty() && currentInterfaceType->astDeclaration ) {
						for( ast::nodes::TypeNodeSharedPointer& superNode : currentInterfaceType->astDeclaration->superInterfaces ) {
							if( superNode->kind == ast::Node::Kind::SimpleType ) {
								ast::nodes::SimpleTypeNode& simpleTypeNode = static_cast<ast::nodes::SimpleTypeNode&>( *superNode );
								TypeSharedPointer simpleType = this->lookupType( simpleTypeNode.name );
								if( simpleType ) {
									typesToCheck.push_back( simpleType );
								}
							}
							else if( superNode->kind == ast::Node::Kind::GenericType ) {
								ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *superNode );
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
					for( TypeSharedPointer& parentInterfaceType : currentInterfaceType->superInterfaces ) {
						typesToCheck.push_back( parentInterfaceType );
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
			bool targetIsIntOop = qname::isIntegerOop( target->qualified );
			bool sourceIsIntOop = qname::isIntegerOop( source->qualified );
			bool targetIsFloatOop = qname::isFloatOop( target->qualified );
			bool sourceIsFloatOop = qname::isFloatOop( source->qualified );
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
			bool targetIsIntOop = qname::isIntegerOop( target->qualified );
			bool sourceIsFloatOop = qname::isFloatOop( source->qualified );
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
			if( source->isVoid() ) {
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
				{"Boolean","bool"},{"String","str"},{"Char","char"},
				{"Byte","u8"},{"Integer","i32"},{"Long","i64"},{"Double","f64"},
				{"I8","i8"},{"I16","i16"},{"I32","i32"},{"I64","i64"},
				{"U8","u8"},{"U16","u16"},{"U32","u32"},{"U64","u64"},
				{"F32","f32"},{"F64","f64"},
				{"Int","i64"},{"UInt","u64"},{"Float","f64"}
			};
			std::unordered_map<std::string,std::string>::const_iterator mapIterator = oopToPrimitiveMap.find( source->name );
			if( mapIterator != oopToPrimitiveMap.end() && mapIterator->second == target->name ) {
				return true;
			}
		}
		if( source->isPrimitive() && target->kind == Type::Kind::Class ) {
			static const std::unordered_map<std::string,std::string> primitiveToOopMap = {
				{"bool","Boolean"},{"str","String"},{"char","Char"},
				{"i8","I8"},{"i16","I16"},{"i32","I32"},{"i64","I64"},
				{"u8","U8"},{"u16","U16"},{"u32","U32"},{"u64","U64"},
				{"f32","F32"},{"f64","F64"}
			};
			std::unordered_map<std::string,std::string>::const_iterator mapIterator = primitiveToOopMap.find( source->name );
			if( mapIterator != primitiveToOopMap.end() && mapIterator->second == target->name ) {
				return true;
			}
			static const std::unordered_map<std::string,std::string> oopToPrimitiveReverseMap = {
				{"Boolean","bool"},{"String","str"},{"Char","char"},
				{"Byte","u8"},{"Integer","i32"},{"Long","i64"},{"Double","f64"},
				{"I8","i8"},{"I16","i16"},{"I32","i32"},{"I64","i64"},
				{"U8","u8"},{"U16","u16"},{"U32","u32"},{"U64","u64"},
				{"F32","f32"},{"F64","f64"},
				{"Int","i64"},{"UInt","u64"},{"Float","f64"}
			};
			std::unordered_map<std::string,std::string>::const_iterator reverseMapIterator = oopToPrimitiveReverseMap.find( target->name );
			if( reverseMapIterator != oopToPrimitiveReverseMap.end() && reverseMapIterator->second == source->name ) {
				return true;
			}
			if( source->kind == Type::Kind::Integer && qname::isIntegerOop( target->qualified ) ) {
				return true;
			}
			if( source->kind == Type::Kind::Float && qname::isFloatOop( target->qualified ) ) {
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
		if( x->kind == Type::Kind::Optional && y->isVoid() ) {
			return true;
		}
		if( y->kind == Type::Kind::Optional && x->isVoid() ) {
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
