
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

#ifndef _URANITE_SEMANTIC_TYPE_HPP_
#define _URANITE_SEMANTIC_TYPE_HPP_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "uranite/ast/node.hpp"
#include "uranite/semantic/qualnames.hpp"

#include "fmt/format.h"

namespace uranite::semantic {
	
	/**
	 * @brief Base struct representing a type within the Uranite compiler system.
	 */
	struct Type {
		
		/**
		 * @brief Enumeration of all possible categories of types.
		 */
		enum class Kind {
			Array,
			Bool,
			Callable,   /**< Callable<Return, <Parameters...>> */
			Char,
			Class,
			Enum,
			Error,      /**< Sentinel for error recovery */
			Float,
			Function,
			Future,     /**< Future<T> — async return wrapper */
			Generator,  /**< Generator<T> — yields values lazily */
			GenericParameter,
			Integer,
			Interface,
			Meta,       /**< Meta<T> — type-as-value */
			Optional,
			Pointer,
			Reference,
			String,
			Struct,
			Trait,
			Tuple,
			Union,
			Unresolved, /**< Not yet resolved */
			Void
		};
		
		/** @brief The specific category or kind of this type. */
		Kind kind;
		
		/** @brief The identifier name of the type. */
		std::string name;
		
		/** @brief The module package where this type is defined. */
		std::string package;
		
		/** @brief The fully qualified name for identity comparisons. */
		std::string qualified;
		
		/**
		 * @brief Constructs a new Type object.
		 * @param kind The category of the type.
		 * @param name The name of the type.
		 * @param package The module package.
		 * @param qualified The fully qualified name.
		 */
		Type( Kind kind, const std::string& name, const std::string& package = "", const std::string& qualified = "" )
			: kind( kind ), name( name ), package( package ), qualified( qualified.empty() ? name : qualified ) {
		}
		
		/**
		 * @brief Virtual destructor for proper cleanup of derived type objects.
		 */
		virtual ~Type() = default;
		
		/**
		 * @brief Checks if this type matches another type reference using
		 *        fully-qualified names when available, falling back to short
		 *        names only for primitives and error-recovery types that lack
		 *        a package-qualified identity.
		 * @param other The other type reference to compare against.
		 * @return True if types are equivalent, false otherwise.
		 *
		 */
		virtual bool equals( const std::shared_ptr<Type>& other ) const {
			if( other == nullptr || this->kind != other->kind ) {
				return false;
			}
			if( this->qualified.empty() == false && other->qualified.empty() == false ) {
				return this->qualified == other->qualified;
			}
			if( this->package.empty() == false && other->package.empty() == false ) {
				return this->package == other->package && this->name == other->name;
			}
			return this->name == other->name;
		}
		
		/**
		 * @brief Gets the name of the type.
		 * @return The string representation of the type name.
		 */
		std::string getName() const {
			return this->name;
		}
		
		/**
		 * @brief Checks if the type is a boolean.
		 * @return True if the kind is Bool.
		 */
		bool isBool() const {
			return this->kind == Kind::Bool || this->qualified == qname::BOOLEAN;
		}
		
		/**
		 * @brief Checks if the type represents an error state.
		 * @return True if the kind is Throwable.
		 */
		bool isError() const {
			return this->kind == Kind::Error;
		}
		
		/**
		 * @brief Checks if the type is a floating-point number.
		 * @return True if the kind is Float.
		 */
		bool isFloatingPoint() const {
			return this->kind == Kind::Float || qname::isFloatOop( this->qualified );
		}
		
		/**
		 * @brief Checks if the type is an integral number.
		 * @return True if the kind is Integer.
		 */
		bool isIntegral() const {
			return this->kind == Kind::Integer || qname::isIntegerOop( this->qualified );
		}
		
		/**
		 * @brief Checks if the type is either an integer or a float.
		 * @return True if the type is numeric.
		 */
		bool isNumeric() const {
			return this->isIntegral() || this->isFloatingPoint();
		}
		
		/**
		 * @brief Checks if the type is a fundamental primitive (Void, Bool, Numeric, Char, or String).
		 * @return True if the type is primitive.
		 */
		bool isPrimitive() const {
			return this->kind == Kind::Void || this->kind == Kind::Bool || this->kind == Kind::Integer || this->kind == Kind::Float || this->kind == Kind::Char || this->kind == Kind::String;
		}
		
		/**
		 * @brief Checks if the type is a void type.
		 * @return True if the kind is Void.
		 */
		bool isVoid() const {
			return this->kind == Kind::Void ||
				this->qualified == qname::VOID || this->qualified == qname::PRIM_VOID;
		}
		
		/**
		 * @brief Returns a string representation of the type.
		 * @return The name of the type.
		 */
		virtual std::string toString() const {
			return this->name;
		}
		
	};
	
	using TypeSharedPointer = std::shared_ptr<Type>;
	
	/**
	 * @brief Represents an integer type with a specific bit width and signedness.
	 * 
	 * This struct extends the base Type to provide detailed information about
	 * integer representations used within the Uranite compiler.
	 */
	struct IntegerType : Type {
		
		/** @brief The number of bits used to represent this integer type. */
		int bitWidth;
		
		/** @brief Indicates whether the integer is signed (true) or unsigned (false). */
		bool isSigned;
		
		/**
		 * @brief Constructs a new IntegerType object.
		 * 
		 * The name of the type is automatically generated based on the signedness
		 * and bit width (e.g., "i32" for signed 32-bit, "u16" for unsigned 16-bit).
		 * 
		 * @param bitWidth The number of bits for the integer.
		 * @param isSigned True for signed integers, false for unsigned integers.
		 */
		IntegerType(
			int bitWidth,
			bool isSigned
		) : Type( Type::Kind::Integer, ( isSigned ? "i" : "u" ) + std::to_string( bitWidth ) ),
			bitWidth( bitWidth ),
			isSigned( isSigned ) {
		}
		
	};
	
	using IntegerTypeSharedPointer = std::shared_ptr<IntegerType>;
	
	/**
	 * @brief Represents a floating-point numeric type with a specific bit width.
	 * 
	 * inherited from Type, this struct defines the precision of floating-point
	 * numbers (e.g., 32-bit or 64-bit) within the Uranite compiler.
	 */
	struct FloatType : Type {
		
		/** @brief The total number of bits used to represent this floating-point type. */
		int bitWidth;
		
		/**
		 * @brief Constructs a new FloatType object.
		 * @param bitWidth The precision in bits (e.g., 32 for float, 64 for double).
		 */
		FloatType( int bitWidth ) : Type( Type::Kind::Float, "f" + std::to_string( bitWidth ) ), bitWidth( bitWidth ) {
		}
		
	};
	
	using FloatTypeSharedPointer = std::shared_ptr<FloatType>;
	
	/**
	 * @brief Represents an array type with a fixed or dynamic size.
	 */
	struct ArrayType : Type {
		
		/** @brief The type of the elements contained in the array. */
		TypeSharedPointer elementType;
		
		/** @brief The static size of the array; -1 indicates the array is dynamic. */
		int64_t size;
		
		/**
		 * @brief Constructs an ArrayType with an element type and size.
		 * @param element The type of the individual elements.
		 * @param size The total number of elements, or -1 for dynamic arrays.
		 */
		ArrayType(
			TypeSharedPointer element,
			int64_t size
		) : Type( Type::Kind::Array, "" ),
			elementType( std::move( element ) ),
			size( size ) {
		}
		
		/**
		 * @brief Returns a string representation of the array type.
		 * @return A string formatted as "[Type; Size]" or "[Type]".
		 */
		std::string toString() const override {
			if( this->size >= 0 ) {
				return fmt::format( "[ {}; {} ]", this->elementType->toString(), this->size );
			}
			return fmt::format( "[ {} ]", this->elementType->toString() );
		}
	
	};
	
	using ArrayTypeSharedPointer = std::shared_ptr<ArrayType>;
	
	/**
	 * @brief Represents a tuple type containing a fixed sequence of heterogeneous types.
	 */
	struct TupleType : Type {
		
		/** @brief The ordered collection of types within the tuple. */
		std::vector<TypeSharedPointer> elements;
		
		/**
		 * @brief Converts the tuple type to a string representation.
		 * @return A comma-separated list of types enclosed in parentheses, e.g., "(Int, Float)".
		 */
		std::string toString() const override {
			std::string result;
			for ( size_t i = 0; i < this->elements.size(); i++ ) {
				if ( i > 0 ) result+= ", ";
				result+= this->elements[i]->toString();
			}
			return fmt::format( "( {} )", result );
		}
		
		/**
		 * @brief Constructs a TupleType from a list of element types.
		 * @param elements A vector of shared pointers to the element types.
		 */
		TupleType( std::vector<TypeSharedPointer> elements ) : Type( Type::Kind::Tuple, "" ), elements( std::move( elements ) ) {
		}
		
	};
	
	using TupleTypeSharedPointer = std::shared_ptr<TupleType>;
	
	/**
	 * @brief Represents a function signature, including parameters, return type, and variadic status.
	 */
	struct FunctionType : Type {
		
		/** @brief List of exception types that this function might raise. */
		std::vector<TypeSharedPointer> exceptionTypes;
		
		/** @brief Flag indicating if the function accepts a variable number of arguments (extern ... only). */
		bool isVariadic = false;
		
		/** @brief Index of the variadic (Type name[]) parameter, or -1 if none. */
		int variadicParameterIndex = -1;
		
		/** @brief Index of the keyword (Type name{}) parameter, or -1 if none. */
		int keywordParameterIndex = -1;
		
		/** @brief Element type T of the variadic Args<T> parameter. */
		TypeSharedPointer variadicElementType;
		
		/** @brief Value type T of the keyword Kwargs<T> parameter. */
		TypeSharedPointer keywordValueType;
		
		/** @brief Names of keyword-only params (params with defaults after variadic). */
		std::vector<std::string> keywordOnlyParamNames;
		
		/** @brief Types of keyword-only params (parallel to keywordOnlyParamNames). */
		std::vector<TypeSharedPointer> keywordOnlyParamTypes;
		
		/** @brief The names of the function's parameters, used for snippet generation. */
		std::vector<std::string> parameterNames;
		
		/** @brief The types of the function's parameters. */
		std::vector<TypeSharedPointer> parameterTypes;
		
		/** @brief The type of the value returned by the function. */
		TypeSharedPointer returnType;
		
		/**
		 * @brief Constructs a FunctionType signature.
		 * @param parameters A vector of types for the function parameters.
		 * @param returnType The expected return type of the function.
		 * @param isVariadic Set to true if the function is variadic (e.g., printf).
		 */
		FunctionType(
			std::vector<TypeSharedPointer> parameters,
			TypeSharedPointer returnType,
			bool isVariadic=false
		) : Type( Type::Kind::Function, "" ),
			isVariadic( isVariadic ),
			parameterTypes( std::move( parameters ) ),
			returnType( std::move( returnType ) ) {
		}
		
		/**
		 * @brief Generates a string representation of the function signature.
		 * @return A string formatted as "fn(Parameter1, Parameter2, ...) -> ReturnType".
		 */
		std::string toString() const override {
			std::string result;
			for( size_t i=0; i<this->parameterTypes.size(); i++ ) {
				if( i > 0 ) {
					result+= ",";
				}
				result+= this->parameterTypes[i]->toString();
			}
			if( this->isVariadic ) {
				if( this->parameterTypes.empty() == false ) {
					result+= ",";
				}
				result+= "...";
			}
			return fmt::format( "function( {} ) -> {}", result, this->returnType->toString() );
		}
	
	};
	
	using FunctionTypeSharedPointer = std::shared_ptr<FunctionType>;
	
	/**
	 * @brief Represents an optional type wrapper, indicating a value that may or may not exist.
	 */
	struct OptionalType : Type {
		
		/** @brief The underlying type wrapped by this optional. */
		TypeSharedPointer inner;
		
		/**
		 * @brief Constructs an OptionalType.
		 * @param inner Shared pointer to the inner type.
		 */
		OptionalType( TypeSharedPointer inner ) : Type( Type::Kind::Optional, fmt::format( "Optional<{}>", inner->name ) ), inner( std::move( inner ) ) {
		}
		
		/**
		 * @brief Returns the string representation of the optional type.
		 * @return A string formatted as "?InnerType".
		 */
		std::string toString() const override {
			return "?" + this->inner->toString();
		}
	
	};
	
	using OptionalTypeSharedPointer = std::shared_ptr<OptionalType>;
	
	/**
	 * @brief Represents a pointer type pointing to an underlying type.
	 */
	struct PointerType : Type {
		
		/** @brief The type being pointed to. */
		TypeSharedPointer inner;
		
		/** @brief Flag indicating if the data pointed to can be modified. */
		bool isMutable;
		
		/**
		 * @brief Constructs a PointerType.
		 * @param inner Shared pointer to the inner type.
		 * @param isMutable Boolean flag for mutability.
		 */
		PointerType(
			TypeSharedPointer inner,
			bool isMutable
		) : Type( Type::Kind::Pointer, "" ),
			inner( std::move( inner ) ),
			isMutable( isMutable ) {
		}
		
		/**
		 * @brief Returns the string representation of the pointer type.
		 * @return A string formatted as "*mut InnerType" or "*InnerType".
		 */
		std::string toString() const override {
			return ( this->isMutable ? "*mut " : "*" ) + this->inner->toString();
		}
	
	};
	
	using PointerTypeSharedPointer = std::shared_ptr<PointerType>;
	
	/**
	 * @brief Represents a reference type to an underlying type.
	 */
	struct ReferenceType : Type {
		
		/** @brief The type being referenced. */
		TypeSharedPointer inner;
		
		/** @brief Flag indicating if the referenced data can be modified. */
		bool isMutable;
		
		/**
		 * @brief Constructs a ReferenceType.
		 * @param inner Shared pointer to the inner type.
		 * @param isMutable Boolean flag for mutability.
		 */
		ReferenceType(
			TypeSharedPointer inner,
			bool isMutable
		) : Type( Type::Kind::Reference, "" ),
			inner( std::move( inner ) ),
			isMutable( isMutable ) {
		}
		
		/**
		 * @brief Returns the string representation of the reference type.
		 * @return A string formatted as "&mut InnerType" or "&InnerType".
		 */
		std::string toString() const override {
			return ( isMutable ? "&mut " : "&" ) + inner->toString();
		}
	
	};
	
	using ReferenceTypeSharedPointer = std::shared_ptr<ReferenceType>;
	
	/**
	 * @brief Metadata representing a field within a compound type (class or struct).
	 */
	struct FieldInfo {
		
		/** @brief Access level of the field (e.g., public, private, protected). */
		ast::AccessModifier access;
		
		/** @brief The unique index position of the field within the type layout. */
		int index;
		
		/** @brief Flag indicating if the field is immutable after initialization. */
		bool isReadonly = false;
		
		/** @brief Flag indicating if the field is a static class-level field. */
		bool isStatic = false;
		
		/** @brief The identifier name of the field. */
		std::string name;
		
		/** @brief The data type of the field. */
		TypeSharedPointer type;
	
	};
	
	/**
	 * @brief Metadata representing a method within a compound type.
	 */
	struct MethodInfo {
		
		/** @brief Access level of the method. */
		ast::AccessModifier access;
		
		/** @brief The index in the interface method table; -1 if not an interface method. */
		int interfaceTableIndex = -1;
		
		/** @brief Flag indicating if the method cannot be overridden further. */
		bool isFinal = false;
		
		/** @brief Flag indicating if this method is an override of a base class method. */
		bool isOverride;
		
		/** @brief Flag indicating if this method behaves as a property getter or setter. */
		bool isProperty = false;
		
		/** @brief Flag indicating if the method belongs to the class itself rather than instances. */
		bool isStatic;
		
		/** @brief Flag indicating if the method can be overridden in derived classes. */
		bool isVirtual;
		
		/** @brief The identifier name of the method. */
		std::string name;
		
		/** @brief The signature type of the method (typically a FunctionType). */
		TypeSharedPointer type;
		
		/** @brief The index in the virtual table; -1 if the method is not virtual. */
		int virtualTableIndex;
	
	};
	
	/**
	 * @brief Represents a class type definition within the Uranite compiler.
	 * 
	 * This struct extends the base Type to include object-oriented features such as
	 * inheritance, interfaces, fields, methods, and virtual table management.
	 */
	struct ClassType : Type {
		
		/** @brief Pointer to the original Abstract Syntax Tree declaration for this class. */
		ast::nodes::ClassDeclaration* astDeclaration = nullptr;
		
		/** @brief The memory alignment requirement for this class in bytes. */
		int alignment = 8;
		
		/** @brief The base class from which this class inherits, if any. */
		TypeSharedPointer baseClass;
		
		/** @brief List of fields (member variables) defined within this class. */
		std::vector<FieldInfo> fields;
		
		/** @brief List of generic type parameters for template-like classes. */
		std::vector<TypeSharedPointer> genericParameters;
		
		/** @brief Whether this class implements the Droper interface for auto-cleanup. */
		bool implementsDroper = false;
		
		/** @brief List of interfaces implemented by this class. */
		std::vector<TypeSharedPointer> interfaces;
		
		/** @brief Indicates if the class is abstract and cannot be instantiated. */
		bool isAbstract;
		
		/** @brief Indicates if the class is marked as final, preventing further inheritance. */
		bool isFinal = false;
		
		/** @brief Indicates if the class is read-only, restricting modifications to its state. */
		bool isReadonly = false;
		
		/** @brief List of methods (member functions) defined within this class. */
		std::vector<MethodInfo> methods;
		
		/** @brief The total memory size required for an instance of this class after layout. */
		int objectSize = 0;
		
		/** @brief Mapping of generic placeholder names to their concrete type substitutions. */
		std::unordered_map<std::string, TypeSharedPointer> typeSubstitutions;
		
		/** @brief The Virtual Method Table containing pointers to overridable functions. */
		std::vector<MethodInfo*> virtualTable;
		
		/**
		 * @brief Constructs a new ClassType object.
		 * @param name The identifier name of the class.
		 */
		ClassType( const std::string& name ) : Type( Type::Kind::Class, name ), isAbstract( false ) {
		}
		
		/**
		 * @brief Searches for a field within the class by its name.
		 * @param name The name of the field to find.
		 * @return A pointer to the FieldInfo if found, otherwise nullptr.
		 */
		FieldInfo* findField( const std::string& name ) {
			for( FieldInfo& field : this->fields ) {
				if( field.name == name ) {
					return &field;
				}
			}
			return nullptr;
		}
		
		/**
		 * @brief Searches for a method within the class by its name.
		 * @param name The name of the method to find.
		 * @return A pointer to the MethodInfo if found, otherwise nullptr.
		 */
		MethodInfo* findMethod( const std::string& name ) {
			for( MethodInfo& method : this->methods ) {
				if( method.name == name ) {
					return &method;
				}
			}
			return nullptr;
		}

		bool implementsInterface( const std::string& qualifiedName ) const;

	};

	using ClassTypeSharedPointer = std::shared_ptr<ClassType>;

	/**
	 * @brief Represents a structd data type within the compiler's type system.
	 * 
	 * Inherits from the base Type and provides support for fields, methods,
	 * generics, and Abstract Syntax Tree (AST) linking for monomorphization.
	 */
	struct StructType : Type {
		
		/** @brief The memory alignment requirement for this struct in bytes. */
		int alignment = 8;
		
		/** @brief Pointer to the original AST node for monomorphization and code generation. */
		ast::nodes::StructDeclaration* astDeclaration = nullptr;
		
		/** @brief The collection of data fields defined within the struct. */
		std::vector<FieldInfo> fields;
		
		/** @brief The list of generic type parameters if this is a template/generic struct. */
		std::vector<TypeSharedPointer> genericParameters;
		
		/** @brief The collection of member methods defined for this struct. */
		std::vector<MethodInfo> methods;
		
		/** @brief The total size of the object in memory in bytes. */
		int objectSize = 0;
		
		/** @brief Mapping of generic placeholder names to concrete types for specialized instances. */
		std::unordered_map<std::string, TypeSharedPointer> typeSubstitutions;
		
		/**
		 * @brief Constructs a new StructType object.
		 * @param name The identifier name of the struct.
		 */
		StructType( const std::string& name ) : Type( Type::Kind::Struct, name ) {
		}
		
		/**
		 * @brief Searches for a field within the struct by its name.
		 * @param name The name of the field to locate.
		 * @return A pointer to the FieldInfo if found, otherwise nullptr.
		 */
		FieldInfo* findField( const std::string& name ) {
			for( FieldInfo& field : this->fields ) {
				if( field.name == name ) {
					return &field;
				}
			}
			return nullptr;
		}
		
		MethodInfo* findMethod( const std::string& name ) {
			for( MethodInfo& method : this->methods ) {
				if( method.name == name ) {
					return &method;
				}
			}
			return nullptr;
		}
	
	};
	
	using StructTypeSharedPointer = std::shared_ptr<StructType>;
	
	/**
	 * @brief Represents detailed information about a specific variant within an enumeration.
	 * 
	 * In the Uranite language, enum variants can hold associated data and possess their
	 * own specific methods, making them more powerful than simple integer constants.
	 */
	struct EnumVariantInfo {
		
		/** 
		 * @brief The list of types associated with this variant (for tagged unions/sum types).
		 */
		std::vector<TypeSharedPointer> associatedTypes;
		
		/** 
		 * @brief The unique integer value used to distinguish this variant at runtime.
		 */
		int discriminant = 0;
		
		/** 
		 * @brief The list of member functions or methods defined specifically for this variant.
		 */
		std::vector<MethodInfo> methods;
		
		/** 
		 * @brief The identifier name of the enumeration variant.
		 */
		std::string name;
	
	};
	
	/**
	 * @brief Represents an Enumeration type within the Uranite compiler.
	 * 
	 * This struct extends the base Type to support variants, methods,
	 * inheritance, and generic specialization for enums.
	 */
	struct EnumType : Type {
		
		/** @brief Pointer to the original AST node for monomorphization and code generation. */
		ast::nodes::EnumDeclaration* astDeclaration = nullptr;
		
		/** @brief The underlying primitive type used to store the enum values (e.g., Integer). */
		TypeSharedPointer backedType;
		
		/** @brief The base class from which this enum might inherit, if applicable. */
		TypeSharedPointer baseClass;
		
		/** @brief The list of generic parameters if this is a generic enum definition. */
		std::vector<TypeSharedPointer> genericParameters;
		
		/** @brief The list of interfaces implemented by this enumeration. */
		std::vector<TypeSharedPointer> interfaces;
		
		/** @brief The list of methods defined within the scope of this enum. */
		std::vector<MethodInfo> methods;
		
		/** @brief A map of template parameter names to their concrete type substitutions. */
		std::unordered_map<std::string, TypeSharedPointer> typeSubstitutions;
		
		/** @brief The list of variants (members) defined in this enum. */
		std::vector<EnumVariantInfo> variants;
		
		/**
		 * @brief Constructs a new EnumType object.
		 * @param name The identifier name of the enumeration.
		 */
		EnumType( const std::string& name ) : Type( Type::Kind::Enum, name ) {
		}
		
		/**
		 * @brief Searches for a method by its name within the enum scope.
		 * @param name The name of the method to find.
		 * @return A pointer to the MethodInfo if found, otherwise nullptr.
		 */
		MethodInfo* findMethod( const std::string& name ) {
			for( MethodInfo& method : this->methods ) {
				if( method.name == name ) {
					return &method;
				}
			}
			return nullptr;
		}
		
		/**
		 * @brief Searches for a specific variant by its name.
		 * @param name The name of the variant to find.
		 * @return A pointer to the EnumVariantInfo if found, otherwise nullptr.
		 */
		EnumVariantInfo* findVariant( const std::string& name ) {
			for( EnumVariantInfo& variant : this->variants ) {
				if( variant.name == name ) {
					return &variant;
				}
			}
			return nullptr;
		}
		
	};
	
	using EnumTypeSharedPointer = std::shared_ptr<EnumType>;
	
	/**
	 * @brief Represents an interface type within the Uranite type system.
	 * 
	 * This struct manages interface methods, inheritance from super-interfaces,
	 * and generic parameter substitutions for monomorphization.
	 */
	struct InterfaceType : Type {
		
		/** @brief Pointer to the original AST node for monomorphization purposes. */
		ast::nodes::InterfaceDeclaration* astDeclaration = nullptr;
		
		/** @brief List of generic parameters associated with this interface. */
		std::vector<TypeSharedPointer> genericParameters;
		
		/** @brief Collection of methods defined within this interface. */
		std::vector<MethodInfo> methods;
		
		/** @brief Internal flag to prevent circularity while populating methods. */
		bool methodsPopulating = false;
		
		bool inheritanceResolved = false;
		
		/** @brief Stable ordered list of method names for itable (Interface Table) indexing. */
		std::vector<std::string> methodOrder;
		
		/** @brief List of interfaces that this interface extends. */
		std::vector<TypeSharedPointer> superInterfaces;
		
		/** @brief Mapping of generic parameter names to their specific type substitutions. */
		std::unordered_map<std::string, TypeSharedPointer> typeSubstitutions;
		
		/**
		 * @brief Constructs a new InterfaceType object.
		 * @param name The name of the interface.
		 */
		InterfaceType( const std::string& name ) : Type( Type::Kind::Interface, name ) {
		}
		
		/**
		 * @brief Searches for a method by its name within this interface.
		 * @param name The name of the method to find.
		 * @return A pointer to the MethodInfo if found, otherwise nullptr.
		 */
		MethodInfo* findMethod( const std::string& name ) {
			for( MethodInfo& method : this->methods ) {
				if( method.name == name ) {
					return &method;
				}
			}
			return nullptr;
		}

		bool extendsInterface( const std::string& qualifiedName ) const {
			for( const TypeSharedPointer& superIface : this->superInterfaces ) {
				if( superIface->qualified == qualifiedName ||
					qname::startsWith( superIface->qualified, qualifiedName ) ) {
					return true;
				}
				if( superIface->kind == Type::Kind::Interface ) {
					if( std::static_pointer_cast<InterfaceType>( superIface )->extendsInterface( qualifiedName ) ) {
						return true;
					}
				}
			}
			return false;
		}

	};

	using InterfaceTypeSharedPointer = std::shared_ptr<InterfaceType>;
	
	/**
	 * @brief Represents a Trait type within the type system.
	 * 
	 * Traits define a set of methods and fields that can be implemented by
	 * other types, supporting polymorphism and code reuse.
	 */
	struct TraitType : Type {
		
		/** @brief Pointer to the original AST declaration node for this trait. */
		ast::nodes::TraitDeclaration* astDeclaration = nullptr;
		
		/** @brief List of fields defined within the trait. */
		std::vector<FieldInfo> fields;
		
		/** @brief List of generic parameters associated with this trait. */
		std::vector<TypeSharedPointer> genericParameters;
		
		/** @brief List of methods defined within the trait. */
		std::vector<MethodInfo> methods;
		
		/**
		 * @brief Constructs a new TraitType object.
		 * @param name The identifier name of the trait.
		 */
		TraitType( const std::string& name ) : Type( Type::Kind::Trait, name ) {
		}
		
		/**
		 * @brief Searches for a method by its name within this trait.
		 * @param name The name of the method to find.
		 * @return A pointer to the MethodInfo if found, otherwise nullptr.
		 */
		MethodInfo* findMethod( const std::string& name ) {
			for( MethodInfo& method : this->methods ) {
				if( method.name == name ) {
					return &method;
				}
			}
			return nullptr;
		}
		
	};
	
	using TraitTypeSharedPointer = std::shared_ptr<TraitType>;
	
	/**
	 * @brief Represents a union type that can hold one of several different types.
	 */
	struct UnionType : Type {
		
		/** @brief The collection of types that compose this union. */
		std::vector<TypeSharedPointer> types;
		
		/**
		 * @brief Constructs a new Union Type object and generates its display name.
		 * @param types A vector of shared pointers to the types included in the union.
		 */
		UnionType( std::vector<TypeSharedPointer> types ) : Type( Type::Kind::Union, "" ), types( std::move( types ) ) {
			std::string typeNames;
			for( size_t i=0; i<this->types.size(); i++ ) {
				if( i > 0 ) {
					typeNames+= ",";
				}
				typeNames+= this->types[i]->toString();
			}
			this->name = fmt::format( "Union<{}>", typeNames );
		}
		
		/**
		 * @brief Checks if a specific type is part of this union.
		 * @param type The type shared pointer to search for.
		 * @return True if the type exists within the union, false otherwise.
		 */
		bool containsType( const TypeSharedPointer& type ) const {
			for( const TypeSharedPointer& memberType : this->types ) {
				if( memberType->qualified.empty() == false && type->qualified.empty() == false ) {
					if( memberType->qualified == type->qualified ) {
						return true;
					}
				}
				else if( memberType->name == type->name && memberType->kind == type->kind ) {
					return true;
				}
			}
			return false;
		}
		
	};
	
	using UnionTypeSharedPointer = std::shared_ptr<UnionType>;
	
	/**
	 * @brief Represents a type parameter in a generic context, potentially with constraints.
	 */
	struct GenericParameterType : Type {
		
		/** @brief The list of trait or type constraints imposed on this generic parameter. */
		std::vector<TypeSharedPointer> constraints;
		
		/**
		 * @brief Constructs a new Generic Parameter Type object.
		 * @param name The identifier name of the generic parameter (e.g., "T").
		 */
		GenericParameterType( const std::string& name ) : Type( Type::Kind::GenericParameter, name ) {
		}
		
	};
	
	using GenericParameterTypeSharedPointer = std::shared_ptr<GenericParameterType>;
	
	/**
	 * @brief Represents a meta-type in the Uranite system, treating a type as a value.
	 * 
	 * For example: `Meta<Integer>` allows the `Integer` type
	 * itself to be passed or manipulated as a value.
	 */
	struct MetaType : Type {
		
		/** @brief The underlying type that this meta-type wraps. */
		TypeSharedPointer innerType;
		
		/**
		 * @brief Constructs a new MetaType object.
		 * @param innerType The shared pointer to the type being wrapped as a value.
		 */
		MetaType( TypeSharedPointer innerType ) : Type( Type::Kind::Meta, fmt::format( "Meta<{}>", innerType->name ) ), innerType( std::move( innerType ) ) {
		}
		
	};
	
	using MetaTypeSharedPointer = std::shared_ptr<MetaType>;
	
	/**
	 * @brief Represents a callable type signature in the Uranite type system.
	 * 
	 * This struct defines the signature for functions or objects that can be invoked,
	 * capturing both the return type and the specific sequence of parameter types.
	 */
	struct CallableType : Type {
		
		/** @brief The list of types for the parameters accepted by this callable. */
		std::vector<TypeSharedPointer> parameterTypes;
		
		/** @brief The type of the value returned by this callable. */
		TypeSharedPointer returnType;
		
		/**
		 * @brief Constructs a new Callable Type object.
		 * @param returnType Shared pointer to the type returned upon invocation.
		 * @param parameterTypes Vector of shared pointers representing the types of the arguments.
		 */
		CallableType(
			TypeSharedPointer returnType,
			std::vector<TypeSharedPointer> parameterTypes
		) : Type( Type::Kind::Callable, "Callable" ),
			parameterTypes( std::move( parameterTypes ) ),
			returnType( std::move( returnType ) ) {
			std::string parameters;
			for( size_t index=0; index<this->parameterTypes.size(); index++ ) {
				if( index > 0 ) {
					parameters+= ",";
				}
				parameters+= this->parameterTypes[index]->toString();
			}
			this->name = fmt::format( "Callable<{},<{}>>", this->returnType->toString(), parameters );
		}
		
	};
	
	using CallableTypeSharedPointer = std::shared_ptr<CallableType>;
	
	/**
	 * @brief Represents an asynchronous return wrapper type (Future<T>) in the Uranite type system.
	 * 
	 * This type indicates that a value of the specified inner type will be available
	 * at some point in the future, typically used for async operations.
	 */
	struct FutureType : Type {
		
		/** @brief Shared pointer to the underlying type wrapped by this future. */
		TypeSharedPointer innerType;
		
		/**
		 * @brief Constructs a new FutureType object.
		 * @param inner The underlying type that this future will eventually produce.
		 */
		FutureType( TypeSharedPointer inner ) : Type( Type::Kind::Future, fmt::format( "Future<{}>", inner->name ) ), innerType( std::move( inner ) ) {
		}
		
	};
	
	using FutureTypeSharedPointer = std::shared_ptr<FutureType>;
	
	/**
	 * @brief Represents a generator type that lazily yields values of a specific type.
	 * 
	 * Inherits from the base Type struct to provide specialized behavior for
	 * asynchronous or lazy evaluation sequences within the Uranite compiler.
	 */
	struct GeneratorType : Type {
		
		/** @brief The underlying type of the values produced by this generator. */
		TypeSharedPointer yieldType;
		
		/**
		 * @brief Constructs a new GeneratorType object.
		 * @param yieldType Shared pointer to the type that this generator will yield.
		 */
		GeneratorType( TypeSharedPointer yieldType ) : Type( Type::Kind::Generator, fmt::format( "Generator<{}>", yieldType->name ) ), yieldType( std::move( yieldType ) ) {
		}
		
	};
	
	using GeneratorTypeSharedPointer = std::shared_ptr<GeneratorType>;
	
	/**
	 * @brief A centralized registry for managing built-in primitive types and user-defined types.
	 * 
	 * This class serves as a factory and repository for all type instances used during
	 * the compilation process, ensuring type consistency across the system.
	 */
	class Registry {
		
		private:
			
			/** @brief Internal storage for the boolean type. */
			TypeSharedPointer booleanType;
			
			/** @brief Internal storage for the character type. */
			TypeSharedPointer charType;
			
			/** @brief Internal storage for the error sentinel type. */
			TypeSharedPointer errorType;
			
			/** @brief Internal storage for the 32-bit floating-point type. */
			TypeSharedPointer float32Type;
			
			/** @brief Internal storage for the 64-bit floating-point type. */
			TypeSharedPointer float64Type;
			
			/** @brief Internal storage for the 16-bit signed integer type. */
			TypeSharedPointer integer16Type;
			
			/** @brief Internal storage for the 32-bit signed integer type. */
			TypeSharedPointer integer32Type;
			
			/** @brief Internal storage for the 64-bit signed integer type. */
			TypeSharedPointer integer64Type;
			
			/** @brief Internal storage for the 8-bit signed integer type. */
			TypeSharedPointer integer8Type;
			
			/** @brief Internal storage for the root object type. */
			TypeSharedPointer objectType;
			
			/** @brief Fast-lookup map for primitive types. */
			std::unordered_map<std::string, TypeSharedPointer> primitivesTypes;
			
			/** @brief Internal storage for the string type. */
			TypeSharedPointer stringType;
			
			/** @brief Internal storage for the 16-bit unsigned integer type. */
			TypeSharedPointer unsigned16Type;
			
			/** @brief Internal storage for the 32-bit unsigned integer type. */
			TypeSharedPointer unsigned32Type;
			
			/** @brief Internal storage for the 64-bit unsigned integer type. */
			TypeSharedPointer unsigned64Type;
			
			/** @brief Internal storage for the 8-bit unsigned integer type. */
			TypeSharedPointer unsigned8Type;
			
			/** @brief Alias map: short name → qualified name for type resolution. */
			std::unordered_map<std::string, std::string> typeAliases;
			
			/** @brief Map of all custom user-defined types. */
			std::unordered_map<std::string, TypeSharedPointer> userTypesType;
			
			/** @brief Internal storage for the void type. */
			TypeSharedPointer voidType;
		
		public:
			
			/**
			 * @brief Default constructor that initializes all built-in primitive types.
			 */
			Registry();
			
			/**
			 * @brief Gets the boolean type reference.
			 * @return The TypeSharedPointererence for boolean.
			 */
			TypeSharedPointer getBool() const {
				return this->booleanType;
			}
			
			/**
			 * @brief Gets the character type reference.
			 * @return The TypeSharedPointererence for character.
			 */
			TypeSharedPointer getChar() const {
				return this->charType;
			}
			
			/**
			 * @brief Gets the error sentinel type reference.
			 * @return The TypeSharedPointererence used for error recovery.
			 */
			TypeSharedPointer getError() const {
				return this->errorType;
			}
			
			/**
			 * @brief Gets the 32-bit floating-point type reference.
			 * @return The TypeSharedPointererence for float32.
			 */
			TypeSharedPointer getFloat32() const {
				return this->float32Type;
			}
			
			/**
			 * @brief Gets the 64-bit floating-point type reference.
			 * @return The TypeSharedPointererence for float64.
			 */
			TypeSharedPointer getFloat64() const {
				return this->float64Type;
			}
			
			/**
			 * @brief Gets the 16-bit signed integer type reference.
			 * @return The TypeSharedPointererence for int16.
			 */
			TypeSharedPointer getInteger16() const {
				return this->integer16Type;
			}
			
			/**
			 * @brief Gets the 32-bit signed integer type reference.
			 * @return The TypeSharedPointererence for int32.
			 */
			TypeSharedPointer getInteger32() const {
				return this->integer32Type;
			}
			
			/**
			 * @brief Gets the 64-bit signed integer type reference.
			 * @return The TypeSharedPointererence for int64.
			 */
			TypeSharedPointer getInteger64() const {
				return this->integer64Type;
			}
			
			/**
			 * @brief Gets the 8-bit signed integer type reference.
			 * @return The TypeSharedPointererence for int8.
			 */
			TypeSharedPointer getInteger8() const {
				return this->integer8Type;
			}
			
			/**
			 * @brief Gets the base object type reference.
			 * @return The TypeSharedPointererence for the root object.
			 */
			TypeSharedPointer getObject() const {
				return this->objectType;
			}
			
			/**
			 * @brief Gets the string type reference.
			 * @return The TypeSharedPointererence for string.
			 */
			TypeSharedPointer getString() const {
				return this->stringType;
			}
			
			/**
			 * @brief Gets the 16-bit unsigned integer type reference.
			 * @return The TypeSharedPointererence for uint16.
			 */
			TypeSharedPointer getUnsigned16() const {
				return this->unsigned16Type;
			}
			
			/**
			 * @brief Gets the 32-bit unsigned integer type reference.
			 * @return The TypeSharedPointererence for uint32.
			 */
			TypeSharedPointer getUnsigned32() const {
				return this->unsigned32Type;
			}
			
			/**
			 * @brief Gets the 64-bit unsigned integer type reference.
			 * @return The TypeSharedPointererence for uint64.
			 */
			TypeSharedPointer getUnsigned64() const {
				return this->unsigned64Type;
			}
			
			/**
			 * @brief Gets the 8-bit unsigned integer type reference.
			 * @return The TypeSharedPointererence for uint8.
			 */
			TypeSharedPointer getUnsigned8() const {
				return this->unsigned8Type;
			}
			
			/**
			 * @brief Gets the map of all registered user-defined types.
			 * @return A constant reference to the user types map.
			 */
			const std::unordered_map<std::string, TypeSharedPointer>& getUserTypes() const {
				return this->userTypesType;
			}
			
			/**
			 * @brief Gets the void type reference.
			 * @return The TypeSharedPointererence for void.
			 */
			TypeSharedPointer getVoid() const {
				return this->voidType;
			}
			
			/**
			 * @brief Checks if the source type can be assigned to the target type.
			 *
			 * Handles: exact type match, error recovery passthrough, void
			 * compatibility (primitive ↔ OOP), None→nullable, numeric widening
			 * (int→float, small→large), Object target (accepts anything),
			 * class inheritance chains, interface conformance (direct +
			 * transitive), union member containment, and monomorphized generic
			 * class matching.
			 *
			 * @param target The destination type (lvalue being assigned to).
			 * @param source The source type (rvalue being assigned from).
			 * @return True if assignment is valid, false otherwise.
			 */
			bool isAssignable( const TypeSharedPointer& target, const TypeSharedPointer& source ) const;
			
			/**
			 * @brief Determines if two types are compatible for comparison operations.
			 * @param x The first type reference.
			 * @param y The second type reference.
			 * @return True if they can be compared, false otherwise.
			 */
			bool isComparable( const TypeSharedPointer& x, const TypeSharedPointer& y ) const;
			
			/**
			 * @brief Searches for a primitive type by its identifier.
			 * @param name The name of the primitive type.
			 * @return The corresponding TypeSharedPointererence, or nullptr if not found.
			 */
			TypeSharedPointer lookupPrimitive( const std::string& name ) const;
			
			/**
			 * @brief Searches for any registered type by name. Checks userTypesType
			 *        first, then falls back to primitivesTypes.
			 * @param name The short name of the type (e.g. "String", "Float").
			 * @return The corresponding type, or nullptr if not found.
			 */
			TypeSharedPointer lookupType( const std::string& name ) const;
			
			/**
			 * @brief Creates or retrieves an array type.
			 * @param element The type of the array elements.
			 * @param size The static size of the array (default is -1 for dynamic).
			 * @return A TypeSharedPointererence representing the array.
			 */
			TypeSharedPointer makeArray( TypeSharedPointer element, int64_t size = -1 );
			
			/**
			 * @brief Creates a callable type for high-level function references.
			 * @param returnType The return type of the callable.
			 * @param parameters The list of parameter types.
			 * @return A TypeSharedPointererence representing the callable signature.
			 */
			TypeSharedPointer makeCallable( TypeSharedPointer returnType, std::vector<TypeSharedPointer> parameters );
			
			/**
			 * @brief Creates a function type with specific parameters and return type.
			 * @param parameters The list of parameter types.
			 * @param returnType The type of the returned value.
			 * @param isVariadic Whether the function accepts a variable number of arguments.
			 * @return A TypeSharedPointererence representing the function.
			 */
			TypeSharedPointer makeFunction( std::vector<TypeSharedPointer> parameters, TypeSharedPointer returnType, bool isVariadic = false );
			
			/**
			 * @brief Creates a future type for asynchronous operations.
			 * @param inner The underlying type that will be available in the future.
			 * @return A TypeSharedPointererence representing the future.
			 */
			TypeSharedPointer makeFuture( TypeSharedPointer inner );
			
			/**
			 * @brief Creates a generator type for lazy evaluation.
			 * @param yieldType The type of values yielded by the generator.
			 * @return A TypeSharedPointererence representing the generator.
			 */
			TypeSharedPointer makeGenerator( TypeSharedPointer yieldType );
			
			/**
			 * @brief Creates a meta-type (type-as-value).
			 * @param inner The type being treated as a value.
			 * @return A TypeSharedPointererence representing the meta-type.
			 */
			TypeSharedPointer makeMeta( TypeSharedPointer inner );
			
			/**
			 * @brief Creates an optional (nullable) type wrapper.
			 * @param inner The underlying type.
			 * @return A TypeSharedPointererence representing the optional type.
			 */
			TypeSharedPointer makeOptional( TypeSharedPointer inner );
			
			/**
			 * @brief Creates a pointer type.
			 * @param inner The type being pointed to.
			 * @param mutableT Whether the pointer allows mutation of the underlying data.
			 * @return A TypeSharedPointererence representing the pointer.
			 */
			TypeSharedPointer makePointer( TypeSharedPointer inner, bool mutableT = false );
			
			/**
			 * @brief Creates a reference type.
			 * @param inner The type being referenced.
			 * @param mutableT Whether the reference allows mutation.
			 * @return A TypeSharedPointererence representing the reference.
			 */
			TypeSharedPointer makeReference( TypeSharedPointer inner, bool mutableT = false );
			
			/**
			 * @brief Creates a tuple type containing multiple heterogeneous types.
			 * @param elements The list of types contained in the tuple.
			 * @return A TypeSharedPointererence representing the tuple.
			 */
			TypeSharedPointer makeTuple( std::vector<TypeSharedPointer> elements );
			
			/**
			 * @brief Creates a union type representing a choice between multiple types.
			 * @param types The list of possible types in the union.
			 * @return A TypeSharedPointererence representing the union.
			 */
			TypeSharedPointer makeUnion( std::vector<TypeSharedPointer> types );
			
			/**
			 * @brief Registers a new user-defined type in the registry.
			 * @param name The name to register.
			 * @param type The type reference to associate with the name.
			 */
			void registerType( const std::string& name, TypeSharedPointer type );
			
			void registerAlias( const std::string& shortName, const std::string& qualifiedName );
			
			std::string resolveAlias( const std::string& name ) const;
			
			/**
			 * @brief Retrieves a list of all registered type names.
			 * @return A vector of strings containing the names of all types.
			 */
			std::vector<std::string> typeNames() const;
			
	};
	
	using RegistrySharedPointer = std::shared_ptr<Registry>;
	
} // namespace uranite::semantic::type

#endif // end _URANITE_SEMANTIC_TYPE_HPP_
