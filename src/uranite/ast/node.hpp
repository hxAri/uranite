
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

#ifndef _URANITE_AST_NODE_HPP_
#define _URANITE_AST_NODE_HPP_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "uranite/lookup/source.hpp"
#include "uranite/token/token.hpp"

namespace uranite::semantic {
	
	struct Type;
	struct Symbol;
	
	using TypeSharedPointer = std::shared_ptr<Type>;
	using SymbolSharedPointer = std::shared_ptr<Symbol>;

} // namespace uranite::semantic

namespace uranite::ast {
	
	/**
	 * @brief Defines the visibility and accessibility levels of class members and symbols.
	 */
	enum class AccessModifier {
		
		/** @brief Accessible within the same package or module (package-private). */
		Default,
		
		/** @brief Accessible only within the defining class. */
		Private,
		
		/** @brief Accessible within the defining class and its subclasses. */
		Protect,
		
		/** @brief Accessible from any part of the program. */
		Public
		
	};
	
	/**
	 * @brief Base structure for all nodes in the Abstract Syntax Tree (AST).
	 *
	 * Every construct in the Uranite language is represented as a Node,
	 * categorized by its specific Kind and linked to its source location.
	 */
	struct Node {
		
		/**
		 * @brief Enumeration of all possible node categories in the AST.
		 *
		 * Groups are preserved for logical structure, while individual
		 * entries within groups are sorted alphabetically where appropriate.
		 */
		enum class Kind {
			
			// Program
			ExportDeclaration,
			ImportDeclaration,
			ModuleDeclaration,
			Program,
			
			// Declarations
			ClassDeclaration,
			ConstantDeclaration,
			EnumDeclaration,
			ExternDeclaration,
			FunctionDeclaration,
			ImplementationDeclaration,
			InterfaceDeclaration,
			StructDeclaration,
			TraitDeclaration,
			TypeAliasDeclaration,
			
			// Statements
			AssignmentStatement,
			BlockStatement,
			BreakStatement,
			ContinueStatement,
			DeferStatement,
			DeleteStatement,
			ExpressionStatement,
			ForStatement,
			IfStatement,
			InlineAssemblyStatement,
			MatchStatement,
			PassStatement,
			ReturnStatement,
			SwitchCase,
			SwitchStatement,
			ThrowStatement,
			TryCatchStatement,
			UnsafeBlock,
			VariableStatement,
			WhileStatement,
			
			// Expressions
			ArrayExpression,
			AwaitExpression,
			BinaryExpression,
			BooleanLiteral,
			CallExpression,
			CastExpression,
			CharLiteral,
			ComprehensionExpression,
			ConstructExpression,
			FloatLiteral,
			IdentifierExpression,
			IndexExpression,
			InstanceofExpression,
			IntegerLiteral,
			LambdaExpression,
			MatchArm,
			MatchExpression,
			MemberAccessExpression,
			MethodCallExpression,
			NoneLiteral,
			RangeExpression,
			RegexLiteral,
			SelfExpression,
			StringLiteral,
			SubclassofExpression,
			SuperExpression,
			TupleExpression,
			TypeReferenceExpression,
			UnaryExpression,
			YieldExpression,
			
			// Type nodes
			ArrayType,
			CallableType,
			FunctionType,
			GenericType,
			MetaType,
			OptionalType,
			PointerType,
			ReferenceType,
			ResultType,
			SimpleType,
			TupleType,
			UnionType,
			
			// Other
			EnumVariant,
			FieldDeclaration,
			FunctionParameter,
			GenericParameter
			
		};
		
		/** @brief The specific category of this AST node. */
		Kind kind;
		
		/** @brief Shared pointer to the source information where this node is defined. */
		lookup::SourceSharedPointer source;
		
		/**
		 * @brief Constructs a new Node object.
		 * @param kind The specific category of the node.
		 * @param source Shared pointer to the source location metadata.
		 */
		Node(
			Kind kind,
			const lookup::SourceSharedPointer& source
		) : kind( kind ),
			source( source ) {
		}
		
		/**
		 * @brief Virtual destructor for proper polymorphic cleanup of derived nodes.
		 */
		virtual ~Node() = default;
		
		inline std::string toString() const {
			switch( this->kind ) {
				case Kind::ArrayExpression:
					return "ArrayExpression";
				case Kind::ArrayType:
					return "ArrayType";
				case Kind::AssignmentStatement:
					return "AssignmentStatement";
				case Kind::AwaitExpression:
					return "AwaitExpression";
				case Kind::BinaryExpression:
					return "BinaryExpression";
				case Kind::BlockStatement:
					return "BlockStatement";
				case Kind::BooleanLiteral:
					return "BooleanLiteral";
				case Kind::BreakStatement:
					return "BreakStatement";
				case Kind::CallExpression:
					return "CallExpression";
				case Kind::CallableType:
					return "CallableType";
				case Kind::CastExpression:
					return "CastExpression";
				case Kind::CharLiteral:
					return "CharLiteral";
				case Kind::ClassDeclaration:
					return "ClassDeclaration";
				case Kind::ComprehensionExpression:
					return "ComprehensionExpression";
				case Kind::ConstructExpression:
					return "ConstructExpression";
				case Kind::ContinueStatement:
					return "ContinueStatement";
				case Kind::DeferStatement:
					return "DeferStatement";
				case Kind::DeleteStatement:
					return "DeleteStatement";
				case Kind::EnumDeclaration:
					return "EnumDeclaration";
				case Kind::EnumVariant:
					return "EnumVariant";
				case Kind::ExportDeclaration:
					return "ExportDeclaration";
				case Kind::ExpressionStatement:
					return "ExpressionStatement";
				case Kind::ExternDeclaration:
					return "ExternDeclaration";
				case Kind::FieldDeclaration:
					return "FieldDeclaration";
				case Kind::FloatLiteral:
					return "FloatLiteral";
				case Kind::ForStatement:
					return "ForStatement";
				case Kind::FunctionDeclaration:
					return "FunctionDeclaration";
				case Kind::FunctionParameter:
					return "FunctionParameter";
				case Kind::FunctionType:
					return "FunctionType";
				case Kind::GenericParameter:
					return "GenericParameter";
				case Kind::GenericType:
					return "GenericType";
				case Kind::IdentifierExpression:
					return "IdentifierExpression";
				case Kind::IfStatement:
					return "IfStatement";
				case Kind::InlineAssemblyStatement:
					return "InlineAssemblyStatement";
				case Kind::ImplementationDeclaration:
					return "ImplementationDeclaration";
				case Kind::ImportDeclaration:
					return "ImportDeclaration";
				case Kind::IndexExpression:
					return "IndexExpression";
				case Kind::InstanceofExpression:
					return "InstanceofExpression";
				case Kind::IntegerLiteral:
					return "IntegerLiteral";
				case Kind::InterfaceDeclaration:
					return "InterfaceDeclaration";
				case Kind::LambdaExpression:
					return "LambdaExpression";
				case Kind::MatchArm:
					return "MatchArm";
				case Kind::MatchExpression:
					return "MatchExpression";
				case Kind::MatchStatement:
					return "MatchStatement";
				case Kind::MemberAccessExpression:
					return "MemberAccessExpression";
				case Kind::MetaType:
					return "MetaType";
				case Kind::MethodCallExpression:
					return "MethodCallExpression";
				case Kind::ModuleDeclaration:
					return "ModuleDeclaration";
				case Kind::NoneLiteral:
					return "NoneLiteral";
				case Kind::OptionalType:
					return "OptionalType";
				case Kind::PassStatement:
					return "PassStatement";
				case Kind::PointerType:
					return "PointerType";
				case Kind::Program:
					return "Program";
				case Kind::RangeExpression:
					return "RangeExpression";
				case Kind::ReferenceType:
					return "ReferenceType";
				case Kind::ResultType:
					return "ResultType";
				case Kind::ReturnStatement:
					return "ReturnStatement";
				case Kind::RegexLiteral:
					return "RegexLiteral";
				case Kind::SelfExpression:
					return "SelfExpression";
				case Kind::SimpleType:
					return "SimpleType";
				case Kind::StringLiteral:
					return "StringLiteral";
				case Kind::StructDeclaration:
					return "StructDeclaration";
				case Kind::SubclassofExpression:
					return "SubclassofExpression";
				case Kind::SuperExpression:
					return "SuperExpression";
				case Kind::SwitchCase:
					return "SwitchCase";
				case Kind::SwitchStatement:
					return "SwitchStatement";
				case Kind::ThrowStatement:
					return "ThrowStatement";
				case Kind::TraitDeclaration:
					return "TraitDeclaration";
				case Kind::TryCatchStatement:
					return "TryCatchStatement";
				case Kind::TupleExpression:
					return "TupleExpression";
				case Kind::TupleType:
					return "TupleType";
				case Kind::TypeAliasDeclaration:
					return "TypeAliasDeclaration";
				case Kind::TypeReferenceExpression:
					return "TypeReferenceExpression";
				case Kind::UnaryExpression:
					return "UnaryExpression";
				case Kind::UnionType:
					return "UnionType";
				case Kind::UnsafeBlock:
					return "UnsafeBlock";
				case Kind::VariableStatement:
					return "VariableStatement";
				case Kind::WhileStatement:
					return "WhileStatement";
				case Kind::YieldExpression:
					return "YieldExpression";
				default:
					return "Unspecified";
			}
		}
		
	};
	
	using NodeSharedPointer = std::shared_ptr<Node>;
	
	namespace nodes {
		
		/**
		 * @brief Base node for all declarations within the Abstract Syntax Tree.
		 */
		struct Declaration : Node {
			
			/** @brief The access level (visibility) of the declaration. */
			AccessModifier access = AccessModifier::Default;
			
			/** @brief Flag indicating if this is a built-in compiler declaration. */
			bool isBuiltin = false;
			
			/**
			 * @brief Constructs a declaration node.
			 * @param kind The specific category of declaration.
			 * @param location The shared pointer to the source location.
			 */
			Declaration( Node::Kind kind, const lookup::SourceSharedPointer& location ) : Node( kind, location ) {}
			
		};
		
		using DeclarationSharedPointer = std::shared_ptr<Declaration>;
		
		/**
		 * @brief Base node for all expression nodes that evaluate to a value.
		 */
		struct Expression : Node {
			
			semantic::SymbolSharedPointer resolvedSymbol;
			semantic::TypeSharedPointer semanticType;
			
			Expression( Node::Kind kind, const lookup::SourceSharedPointer& location ) : Node( kind, location ) {}
		
		};
		
		using ExpressionSharedPointer = std::shared_ptr<Expression>;
		
		/**
		 * @brief Base node for all statement nodes that represent an action or control flow.
		 */
		struct Statement : Node {
			
			/**
			 * @brief Constructs a statement node.
			 * @param kind The specific category of statement.
			 * @param location The shared pointer to the source location.
			 */
			Statement( Node::Kind kind, const lookup::SourceSharedPointer& location ) : Node( kind, location ) {}
			
		};
		
		using StatementSharedPointer = std::shared_ptr<Statement>;
		
		/**
		 * @brief Base node for all type-related nodes within the Abstract Syntax Tree.
		 */
		struct TypeNode : Node {
			
			/**
			 * @brief Constructs a type node.
			 * @param kind The specific category of type node.
			 * @param location The shared pointer to the source location.
			 */
			TypeNode( Node::Kind kind, const lookup::SourceSharedPointer& location ) : Node( kind, location ) {}
			
		};
		
		using TypeNodeSharedPointer = std::shared_ptr<TypeNode>;
		
		// ==========================================
		// Type Nodes
		// ==========================================
		
		/**
		 * @brief Represents an array type node in the abstract syntax tree (e.g., T[size]).
		 */
		struct ArrayTypeNode : TypeNode {
			
			/** @brief The type of the elements contained within the array. */
			TypeNodeSharedPointer elementType;
			
			/** @brief The expression defining the array size; nullptr indicates a dynamic array. */
			ExpressionSharedPointer size;
			
			/**
			 * @brief Constructs an ArrayTypeNode.
			 * @param element The type of the array elements.
			 * @param size The size expression of the array.
			 * @param source Shared pointer to the source location information.
			 */
			ArrayTypeNode(
				TypeNodeSharedPointer element,
				ExpressionSharedPointer size,
				const lookup::SourceSharedPointer& source
			) : TypeNode( Node::Kind::ArrayType, source ),
				elementType( std::move( element ) ),
				size( std::move( size ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a high-level callable type signature node.
		 */
		struct CallableTypeNode : TypeNode {
			
			/** @brief List of types for the parameters of the callable. */
			std::vector<TypeNodeSharedPointer> parameterTypes;
			
			/** @brief The expected return type of the callable. */
			TypeNodeSharedPointer returnType;
			
			/**
			 * @brief Constructs a CallableTypeNode.
			 * @param returnType The return type signature.
			 * @param parameters The list of parameter type nodes.
			 * @param source Shared pointer to the source location information.
			 */
			CallableTypeNode(
				TypeNodeSharedPointer returnType,
				std::vector<TypeNodeSharedPointer> parameters,
				const lookup::SourceSharedPointer& source
			) : TypeNode( Node::Kind::CallableType, source ),
				parameterTypes( std::move( parameters ) ),
				returnType( std::move( returnType ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a function type signature node within the AST.
		 */
		struct FunctionTypeNode : TypeNode {
			
			/** @brief Types of the function parameters. */
			std::vector<TypeNodeSharedPointer> parameterTypes;
			
			/** @brief Type of the value returned by the function. */
			TypeNodeSharedPointer returnType;
			
			/**
			 * @brief Constructs a FunctionTypeNode.
			 * @param parameters The list of parameter type nodes.
			 * @param returnType The return type node.
			 * @param source Shared pointer to the source location information.
			 */
			FunctionTypeNode(
				std::vector<TypeNodeSharedPointer> parameters,
				TypeNodeSharedPointer returnType,
				const lookup::SourceSharedPointer& source
			) : TypeNode( Node::Kind::FunctionType, source ),
				parameterTypes( std::move( parameters ) ),
				returnType( std::move( returnType ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a generic type node with arguments (e.g., Map<Key, Value>).
		 */
		struct GenericTypeNode : TypeNode {
			
			/** @brief The base name of the generic type. */
			std::string name;
			
			/** @brief The list of type arguments provided to the generic. */
			std::vector<TypeNodeSharedPointer> typeArguments;
			
			/**
			 * @brief Constructs a GenericTypeNode.
			 * @param name The identifier name of the generic base.
			 * @param arguments The list of type argument nodes.
			 * @param source Shared pointer to the source location information.
			 */
			GenericTypeNode(
				const std::string& name,
				std::vector<TypeNodeSharedPointer> arguments,
				const lookup::SourceSharedPointer& source
			) : TypeNode( Node::Kind::GenericType, source ),
				name( name ),
				typeArguments( std::move( arguments ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a meta-type node (type-as-value).
		 */
		struct MetaTypeNode : TypeNode {
			
			/** @brief The underlying type node being reflected as a value. */
			TypeNodeSharedPointer innerType;
			
			/**
			 * @brief Constructs a MetaTypeNode.
			 * @param innerType The underlying type being reflected.
			 * @param source Shared pointer to the source location.
			 */
			MetaTypeNode(
				TypeNodeSharedPointer innerType,
				const lookup::SourceSharedPointer& source
			) : TypeNode( Node::Kind::MetaType, source ),
				innerType( std::move( innerType ) ) {
			}
			
		};
		
		/**
		 * @brief Represents an optional type wrapper (e.g., T?).
		 */
		struct OptionalTypeNode : TypeNode {
			
			/** @brief The underlying type that is marked as optional. */
			TypeNodeSharedPointer innerType;
			
			/**
			 * @brief Constructs an OptionalTypeNode.
			 * @param innerType The type that is optional.
			 * @param source Shared pointer to the source location.
			 */
			OptionalTypeNode(
				TypeNodeSharedPointer innerType,
				const lookup::SourceSharedPointer& source
			) : TypeNode( Node::Kind::OptionalType, source ),
				innerType( std::move( innerType ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a raw pointer type node.
		 */
		struct PointerTypeNode : TypeNode {
			
			/** @brief The underlying type being pointed to. */
			TypeNodeSharedPointer innerType;
			
			/** @brief Flag indicating if the pointer allows mutation of the underlying data. */
			bool isMutable;
			
			/**
			 * @brief Constructs a PointerTypeNode.
			 * @param innerType The type being pointed to.
			 * @param mutable_ Flag for mutability of the pointer.
			 * @param source Shared pointer to the source location.
			 */
			PointerTypeNode(
				TypeNodeSharedPointer innerType,
				bool mutable_,
				const lookup::SourceSharedPointer& source
			) : TypeNode( Node::Kind::PointerType, source ),
				innerType( std::move( innerType ) ),
				isMutable( mutable_ ) {
			}
			
		};
		
		/**
		 * @brief Represents a reference type node.
		 */
		struct ReferenceTypeNode : TypeNode {
			
			/** @brief The underlying type being referenced. */
			TypeNodeSharedPointer innerType;
			
			/** @brief Flag indicating if the reference allows mutation. */
			bool isMutable;
			
			/**
			 * @brief Constructs a ReferenceTypeNode.
			 * @param innerType The type being referenced.
			 * @param mutable_ Flag for mutability of the reference.
			 * @param source Shared pointer to the source location.
			 */
			ReferenceTypeNode(
				TypeNodeSharedPointer innerType,
				bool mutable_,
				const lookup::SourceSharedPointer& source
			) : TypeNode( Node::Kind::ReferenceType, source ),
				innerType( std::move( innerType ) ),
				isMutable( mutable_ ) {
			}
			
		};
		
		/**
		 * @brief Represents a simple named type node (e.g., integer, string).
		 */
		struct SimpleTypeNode : TypeNode {
			
			/** @brief The identifier name of the type. */
			std::string name;
			
			/**
			 * @brief Constructs a SimpleTypeNode.
			 * @param name The name of the type.
			 * @param source Shared pointer to the source location.
			 */
			SimpleTypeNode(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : TypeNode( Node::Kind::SimpleType, source ),
				name( name ) {
			}
			
		};
		
		/**
		 * @brief Represents a tuple type node (e.g., (Type1, Type2)).
		 */
		struct TupleTypeNode : TypeNode {
			
			/** @brief The constituent types that form the tuple. */
			std::vector<TypeNodeSharedPointer> elements;
			
			/**
			 * @brief Constructs a TupleTypeNode.
			 * @param elements The constituent types of the tuple.
			 * @param source Shared pointer to the source location.
			 */
			TupleTypeNode(
				std::vector<TypeNodeSharedPointer> elements,
				const lookup::SourceSharedPointer& source
			) : TypeNode( Node::Kind::TupleType, source ),
				elements( std::move( elements ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a union type node (e.g., TypeA | TypeB).
		 */
		struct UnionTypeNode : TypeNode {
			
			/** @brief The collection of different types that form the union. */
			std::vector<TypeNodeSharedPointer> types;
			
			/**
			 * @brief Constructs a UnionTypeNode.
			 * @param types The different types part of the union.
			 * @param source Shared pointer to the source location.
			 */
			UnionTypeNode(
				std::vector<TypeNodeSharedPointer> types,
				const lookup::SourceSharedPointer& source
			) : TypeNode( Node::Kind::UnionType, source ),
				types( std::move( types ) ) {
			}
			
		};
		
		// ==========================================
		// Parameters & Support
		// ==========================================
		
		/**
		 * @brief Represents a parameter within a function declaration.
		 */
		struct FunctionParameterNode : Node {
			
			/** @brief Access level if the parameter is declared as a property. */
			AccessModifier propertyAccess = AccessModifier::Default;
			
			/** @brief Optional default value expression for the parameter. */
			ExpressionSharedPointer defaultValue;
			
			/** @brief Indicates if the parameter can be modified within the function body. */
			bool isMutable = false;
			
			/** @brief Indicates if this is a constructor property declaration. */
			bool isProperty = false;
			
			/** @brief Indicates if the property is declared as readonly. */
			bool isReadonly = false;
			
			/** @brief Indicates if the parameter is passed by reference. */
			bool isReference = false;
			
			/** @brief Indicates if this is the 'self' or 'this' parameter. */
			bool isSelf = false;
			
			/** @brief Indicates if this is a variadic parameter (Type name[]). */
			bool isVariadic = false;
			
			/** @brief Indicates if this is a keyword parameter (Type name{}). */
			bool isKeyword = false;
			
			/** @brief The identifier name of the parameter. */
			std::string name;
			
			/** @brief The explicit type node associated with this parameter. */
			TypeNodeSharedPointer type;
			
			/**
			 * @brief Constructs a new Function Parameter Node.
			 * @param name The name of the parameter.
			 * @param type The type definition node.
			 * @param source Shared pointer to the source location.
			 */
			FunctionParameterNode(
				const std::string& name,
				TypeNodeSharedPointer type,
				const lookup::SourceSharedPointer& source
			) : Node( Node::Kind::FunctionParameter, source ),
				name( name ),
				type( std::move( type ) ) {
			}
			
		};
		
		using FunctionParameterSharedPointer = std::shared_ptr<FunctionParameterNode>;
		
		/**
		 * @brief Represents a generic type parameter (e.g., <T>).
		 */
		struct GenericParameterNode : Node {
			
			/** @brief List of interface or trait constraints applied to this generic parameter. */
			std::vector<TypeNodeSharedPointer> constraints;
			
			/** @brief The default type node used if no type argument is provided. */
			TypeNodeSharedPointer defaultType;
			
			/** @brief The identifier name of the generic parameter. */
			std::string name;
			
			/**
			 * @brief Constructs a new Generic Parameter Node.
			 * @param name The name of the generic parameter.
			 * @param source Shared pointer to the source location.
			 */
			GenericParameterNode(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Node( Node::Kind::GenericParameter, source ),
				name( name ) {
			}
			
		};
		
		using GenericParameterSharedPointer = std::shared_ptr<GenericParameterNode>;
		
		// ==========================================
		// Declarations
		// ==========================================
		
		/**
		 * @brief Represents a data field/member declaration.
		 */
		struct FieldDeclarationNode : Node {
			
			/** @brief Visibility/Access modifier of the field. */
			AccessModifier access = AccessModifier::Default;
			
			/** @brief Optional expression for the default initialization value. */
			ExpressionSharedPointer defaultValue;
			
			/** @brief Flag indicating if the field is final. */
			bool isFinal = false;
			
			/** @brief Flag indicating if the field is readonly. */
			bool isReadonly = false;
			
			/** @brief Flag indicating if the field is static (class-level, not per-instance). */
			bool isStatic = false;
			
			/** @brief The identifier name of the field. */
			std::string name;
			
			/** @brief The type definition of the field. */
			TypeNodeSharedPointer type;
			
			FieldDeclarationNode(
				const std::string& name,
				TypeNodeSharedPointer type,
				const lookup::SourceSharedPointer& source
			) : Node( Node::Kind::FieldDeclaration, source ),
				name( name ),
				type( std::move( type ) ) {
			}
			
		};
		
		using FieldDeclarationSharedPointer = std::shared_ptr<FieldDeclarationNode>;
		
		/**
		 * @brief Represents a class definition.
		 */
		struct ClassDeclaration : Declaration {
			
			/** @brief The base class from which this class inherits. */
			TypeNodeSharedPointer baseClassType;
			
			/** @brief List of data member fields within the class. */
			std::vector<FieldDeclarationSharedPointer> fields;
			
			/** @brief List of generic parameters for template-like class definitions. */
			std::vector<GenericParameterSharedPointer> genericParameters;
			
			/** @brief List of interfaces implemented by this class. */
			std::vector<TypeNodeSharedPointer> interfaces;
			
			/** @brief Flag indicating if the class is abstract. */
			bool isAbstract = false;
			
			/** @brief Flag indicating if the class is final and cannot be inherited. */
			bool isFinal = false;
			
			/** @brief Flag indicating if the class is implemented in native code. */
			bool isNative = false;
			
			/** @brief Flag indicating if the class members are readonly by default. */
			bool isReadonly = false;
			
			/** @brief List of member function methods. */
			std::vector<DeclarationSharedPointer> methods;
			
			/** @brief The identifier name of the class. */
			std::string name;
			
			/** @brief List of declarations nested within this class (inner classes, enums). */
			std::vector<DeclarationSharedPointer> nestedDeclarations;
			
			/** @brief List of trait names applied to this class via the 'use' keyword. */
			std::vector<std::string> usedTraits;
			
			ClassDeclaration(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Declaration( Node::Kind::ClassDeclaration, source ),
				name( name ) {
			}
			
		};
		
		using ClassDeclarationSharedPointer = std::shared_ptr<ClassDeclaration>;
		
		/**
		 * @brief Represents a variant of an enumeration.
		 */
		struct EnumVariantNode : Node {
			
			/** @brief List of types associated with tuple-style variants. */
			std::vector<TypeNodeSharedPointer> associatedTypes;
			
			/** @brief The value assigned to the variant for backed-type enums. */
			ExpressionSharedPointer backedValue;
			
			/** @brief Explicit discriminant value for the variant. */
			ExpressionSharedPointer discriminant;
			
			/** @brief List of fields for struct-style variants. */
			std::vector<FieldDeclarationSharedPointer> fields;
			
			/** @brief List of methods defined specifically for this variant. */
			std::vector<DeclarationSharedPointer> methods;
			
			/** @brief The identifier name of the variant. */
			std::string name;
			
			EnumVariantNode(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Node( Node::Kind::EnumVariant, source ),
				name( name ) {
			}
			
		};
		
		using EnumVariantSharedPointer = std::shared_ptr<EnumVariantNode>;
		
		/**
		 * @brief Represents an enumeration definition.
		 */
		struct EnumDeclaration : Declaration {
			
			/** @brief The underlying primitive type for backed enums. */
			TypeNodeSharedPointer backedType;
			
			/** @brief The base class type if the enum extends another type. */
			TypeNodeSharedPointer baseClassType;
			
			/** @brief List of generic parameters for the enumeration. */
			std::vector<GenericParameterSharedPointer> genericParameters;
			
			/** @brief List of interfaces implemented by the enumeration. */
			std::vector<TypeNodeSharedPointer> interfaces;
			
			/** @brief List of member methods defined for the enumeration. */
			std::vector<DeclarationSharedPointer> methods;
			
			/** @brief The identifier name of the enumeration. */
			std::string name;
			
			/** @brief List of variants defined within this enumeration. */
			std::vector<EnumVariantSharedPointer> variants;
			
			EnumDeclaration(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Declaration( Node::Kind::EnumDeclaration, source ),
				name( name ) {
			}
			
		};
		
		using EnumDeclarationSharedPointer = std::shared_ptr<EnumDeclaration>;
		
		/**
		 * @brief Represents a parameter within an external function declaration.
		 */
		struct ExternParameter {
			
			/** @brief The identifier name of the parameter. */
			std::string name;
			
			/** @brief The pointer to the type definition of this parameter. */
			TypeNodeSharedPointer type;
			
		};
		
		/**
		 * @brief Represents an external symbol declaration (Foreign Function Interface).
		 */
		struct ExternDeclaration : Declaration {
			
			/** @brief Flag indicating if the external function accepts variadic arguments. */
			bool isVariadic = false;
			
			/** @brief The actual symbol name used for linking in C/External object. */
			std::string linkName;
			
			/** @brief The local identifier name for the function. */
			std::string name;
			
			/** @brief List of external parameters. */
			std::vector<ExternParameter> parameters;
			
			/** @brief The return type of the external function. */
			TypeNodeSharedPointer returnType;
			
			ExternDeclaration(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Declaration( Node::Kind::ExternDeclaration, source ),
				linkName( name ),
				name( name ) {
			}
			
		};
		
		using ExternDeclarationSharedPointer = std::shared_ptr<ExternDeclaration>;
		
		/**
		 * @brief Represents an entity that is exported from a module.
		 */
		struct ExportItem {
			
			/** @brief The alternative name assigned for the export scope. */
			std::string alias;
			
			/** @brief The original identifier name of the entity. */
			std::string name;
			
		};
		
		/**
		 * @brief Represents an explicit export declaration.
		 */
		struct ExportDeclaration : Declaration {
			
			/** @brief The declaration being exported (function, class, etc.). */
			DeclarationSharedPointer declaration;
			
			/** @brief List of specific items exported (e.g., export { Foo, Bar }). */
			std::vector<ExportItem> items;
			
			ExportDeclaration( const lookup::SourceSharedPointer& source ) : Declaration( Node::Kind::ExportDeclaration, source ) {
			}
			
		};
		
		using ExportDeclarationSharedPointer = std::shared_ptr<ExportDeclaration>;
		
		/**
		 * @brief Represents a function or method definition.
		 */
		struct FunctionDeclaration : Declaration {
			
			/** @brief List of statements comprising the function body. */
			std::vector<StatementSharedPointer> body;
			
			/** @brief List of generic parameters for the function. */
			std::vector<GenericParameterSharedPointer> genericParameters;
			
			/** @brief Flag indicating if the function is abstract. */
			bool isAbstract = false;
			
			/** @brief Flag indicating if the function is asynchronous. */
			bool isAsync = false;
			
			/** @brief Flag indicating if the function is constant. */
			bool isConst = false;
			
			/** @brief Flag indicating if the function is final. */
			bool isFinal = false;
			
			/** @brief Flag indicating if the function is a generator. */
			bool isGenerator = false;
			
			/** @brief Flag indicating if the function is implemented in native code. */
			bool isNative = false;
			
			/** @brief Flag indicating if the function overrides a base class function. */
			bool isOverride = false;
			
			/** @brief Flag indicating if the function behaves as a property. */
			bool isProperty = false;
			
			/** @brief Flag indicating if the function is static. */
			bool isStatic = false;
			
			/** @brief Flag indicating if the function is virtual. */
			bool isVirtual = false;
			
			/** @brief The identifier name of the function. */
			std::string name;
			
			/** @brief List of internal function definitions. */
			std::vector<DeclarationSharedPointer> nestedFunctions;
			
			/** @brief List of formal parameters for the function. */
			std::vector<FunctionParameterSharedPointer> parameters;
			
			/** @brief List of exception types that this function may throw. */
			std::vector<TypeNodeSharedPointer> raisesTypes;
			
			/** @brief The return type of the function. */
			TypeNodeSharedPointer returnType;
			
			FunctionDeclaration(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Declaration( Node::Kind::FunctionDeclaration, source ),
				name( name ) {
			}
			
		};
		
		using FunctionDeclarationSharedPointer = std::shared_ptr<FunctionDeclaration>;
		
		/**
		 * @brief Represents an implementation block.
		 */
		struct ImplementDeclaration : Declaration {
			
			/** @brief List of generic parameters for the implementation block. */
			std::vector<GenericParameterSharedPointer> genericParameters;
			
			/** @brief The interface type being implemented (null if inherent implementation). */
			TypeNodeSharedPointer interfaceType;
			
			/** @brief List of methods defined within the implementation block. */
			std::vector<DeclarationSharedPointer> methods;
			
			/** @brief The target type that this block is implementing. */
			TypeNodeSharedPointer targetType;
			
			ImplementDeclaration( const lookup::SourceSharedPointer& source ) : Declaration( Node::Kind::ImplementationDeclaration, source ) {
			}
			
		};
		
		using ImplementDeclarationSharedPointer = std::shared_ptr<ImplementDeclaration>;
		
		// ==========================================
		// Expressions
		// ==========================================
		
		/**
		 * @brief Represents an array literal expression (e.g., [1, 2, 3]).
		 */
		struct ArrayExpression : Expression {
			
			/** @brief A collection of expressions representing the individual elements of the array. */
			std::vector<ExpressionSharedPointer> elements;
			
			/**
			 * @brief Constructs a new Array Expression object.
			 * @param elements The list of element expressions.
			 * @param source Shared pointer to the source location information.
			 */
			ArrayExpression(
				std::vector<ExpressionSharedPointer> elements,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::ArrayExpression, source ),
				elements( std::move( elements ) ) {
			}
			
		};
		
		/**
		 * @brief Represents an asynchronous await expression used to pause execution for a future.
		 */
		struct AwaitExpression : Expression {
			
			/** @brief The expression representing the task or future being awaited. */
			ExpressionSharedPointer operand;
			
			/**
			 * @brief Constructs a new Await Expression object.
			 * @param operand The target expression to await.
			 * @param source Shared pointer to the source location information.
			 */
			AwaitExpression(
				ExpressionSharedPointer operand,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::AwaitExpression, source ),
				operand( std::move( operand ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a binary operation expression involving two operands (e.g., a + b).
		 */
		struct BinaryExpression : Expression {
			
			/** @brief The expression on the left-hand side of the operator. */
			ExpressionSharedPointer left;
			
			/** @brief The specific operator type used in the operation. */
			token::Type operation;
			
			/** @brief The expression on the right-hand side of the operator. */
			ExpressionSharedPointer right;
			
			/**
			 * @brief Constructs a new Binary Expression object.
			 * @param operation The token type representing the operation.
			 * @param left The left-hand side operand.
			 * @param right The right-hand side operand.
			 * @param source Shared pointer to the source location information.
			 */
			BinaryExpression( token::Type operation, ExpressionSharedPointer left, ExpressionSharedPointer right, const lookup::SourceSharedPointer& source )
				: Expression( Node::Kind::BinaryExpression, source ),
				left( std::move( left ) ),
				operation( operation ),
				right( std::move( right ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a boolean literal value (true or false).
		 */
		struct BoolLiteralExpression : Expression {
			
			/** @brief The literal boolean value. */
			bool value;
			
			/**
			 * @brief Constructs a new Bool Literal Expression object.
			 * @param value The boolean value to store.
			 * @param source Shared pointer to the source location information.
			 */
			BoolLiteralExpression(
				bool value,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::BooleanLiteral, source ),
				value( value ) {
			}
			
		};
		
		/**
		 * @brief Represents a function or method call expression, including potential generic arguments.
		 */
		/**
		 * @brief A named argument passed at a call site (name=value).
		 */
		struct KeywordArgument {
			std::string name;
			ExpressionSharedPointer value;
			lookup::SourceSharedPointer source;
		};
		
		struct CallExpression : Expression {
			
			/** @brief The list of expressions being passed as arguments to the call. */
			std::vector<ExpressionSharedPointer> arguments;
			
			/** @brief The expression representing the entity being invoked (the callee). */
			ExpressionSharedPointer callee;
			
			/** @brief Named keyword arguments (name=value) for kwargs parameters. */
			std::vector<KeywordArgument> keywordArguments;
			
			/** @brief Optional generic type arguments explicitly provided for the call. */
			std::vector<TypeNodeSharedPointer> typeArguments;
			
			/**
			 * @brief Constructs a new Call Expression object.
			 * @param callee The expression to be called.
			 * @param arguments The arguments for the call.
			 * @param source Shared pointer to the source location information.
			 */
			CallExpression( ExpressionSharedPointer callee, std::vector<ExpressionSharedPointer> arguments, const lookup::SourceSharedPointer& source )
				: Expression( Node::Kind::CallExpression, source ),
				arguments( std::move( arguments ) ),
				callee( std::move( callee ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a type casting expression, converting an expression to a target type.
		 */
		struct CastExpression : Expression {
			
			/** @brief The underlying expression that is being cast. */
			ExpressionSharedPointer expression;
			
			/** @brief The target type node for the cast operation. */
			TypeNodeSharedPointer targetType;
			
			/**
			 * @brief Constructs a new Cast Expression object.
			 * @param expression The expression to transform.
			 * @param targetType The destination type.
			 * @param source Shared pointer to the source location information.
			 */
			CastExpression( ExpressionSharedPointer expression, TypeNodeSharedPointer targetType, const lookup::SourceSharedPointer& source )
				: Expression( Node::Kind::CastExpression, source ),
				expression( std::move( expression ) ),
				targetType( std::move( targetType ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a single character literal (e.g., 'a').
		 */
		struct CharLiteralExpression : Expression {
			
			/** @brief The literal character value. */
			char value;
			
			/**
			 * @brief Constructs a new Char Literal Expression object.
			 * @param value The character value to store.
			 * @param source Shared pointer to the source location information.
			 */
			CharLiteralExpression( char value, const lookup::SourceSharedPointer& source )
				: Expression( Node::Kind::CharLiteral, source ),
				value( value ) {
			}
			
		};
		
		/**
		 * @brief Represents a list or set comprehension expression.
		 *
		 * Used for constructing collections based on existing iterables with optional filtering.
		 */
		struct ComprehensionExpression : Expression {
			
			/** @brief The expression that produces the value for each iteration. */
			ExpressionSharedPointer bodyExpression;
			
			/** @brief Optional filtering condition that must be true for the iteration to yield a value. */
			ExpressionSharedPointer condition;
			
			/** @brief The source collection or iterable being traversed. */
			ExpressionSharedPointer iterable;
			
			/** @brief The name of the iteration variable. */
			std::string variable;
			
			/** @brief The explicit or inferred type of the iteration variable. */
			TypeNodeSharedPointer variableType;
			
			/**
			 * @brief Constructs a new Comprehension Expression object.
			 * @param bodyExpression The value-producing expression.
			 * @param condition Optional filtering expression.
			 * @param iterable The source collection.
			 * @param source Shared pointer to the source code location.
			 * @param variable The name of the iterator.
			 * @param variableType The type node of the iterator.
			 */
			ComprehensionExpression(
				ExpressionSharedPointer bodyExpression,
				ExpressionSharedPointer condition,
				ExpressionSharedPointer iterable,
				const lookup::SourceSharedPointer& source,
				const std::string& variable,
				TypeNodeSharedPointer variableType
			) : Expression( Node::Kind::ComprehensionExpression, source ),
				bodyExpression( std::move( bodyExpression ) ),
				condition( std::move( condition ) ),
				iterable( std::move( iterable ) ),
				variable( variable ),
				variableType( std::move( variableType ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a structure or class construction expression.
		 *
		 * Used when instantiating a type with specific field initializers.
		 */
		struct ConstructExpression : Expression {
			
			/** @brief List of field names paired with their respective initialization expressions. */
			std::vector<std::pair<std::string, ExpressionSharedPointer>> fields;
			
			/** @brief The type node representing the structure or class being constructed. */
			TypeNodeSharedPointer type;
			
			/**
			 * @brief Constructs a new Construct Expression object.
			 * @param fields The initializers for the fields.
			 * @param source Shared pointer to the source code location.
			 * @param type The type to be instantiated.
			 */
			ConstructExpression(
				std::vector<std::pair<std::string,ExpressionSharedPointer>> fields,
				const lookup::SourceSharedPointer& source,
				TypeNodeSharedPointer type
			) : Expression( Node::Kind::ConstructExpression, source ),
				fields( std::move( fields ) ),
				type( std::move( type ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a floating-point literal in the source code.
		 */
		struct FloatLiteralExpression : Expression {
			
			/** @brief The raw string representation of the float as it appeared in the source. */
			std::string rawRepresentation;
			
			/** @brief The actual parsed double-precision value. */
			double value;
			
			/**
			 * @brief Constructs a new Float Literal Expression object.
			 * @param rawRepresentation The literal string from the source.
			 * @param source Shared pointer to the source code location.
			 * @param value The numerical value of the literal.
			 */
			FloatLiteralExpression(
				const std::string& rawRepresentation,
				const lookup::SourceSharedPointer& source,
				double value
			) : Expression( Node::Kind::FloatLiteral, source ),
				rawRepresentation( rawRepresentation ),
				value( value ) {
			}
			
		};
		
		/**
		 * @brief Represents a simple name or identifier expression.
		 */
		struct IdentifierExpression : Expression {
			
			/** @brief The name of the identifier. */
			std::string name;
			
			/**
			 * @brief Constructs a new Identifier Expression object.
			 * @param name The identifier string.
			 * @param source Shared pointer to the source code location.
			 */
			IdentifierExpression(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::IdentifierExpression, source ),
				name( name ) {
			}
			
		};
		
		/**
		 * @brief Represents an array or dictionary indexing expression.
		 */
		struct IndexExpression : Expression {
			
			/** @brief The index expression used for addressing. */
			ExpressionSharedPointer index;
			
			/** @brief The object being indexed (e.g., an array or map). */
			ExpressionSharedPointer object;
			
			/**
			 * @brief Constructs an IndexExpression.
			 * @param object The object being accessed.
			 * @param index The index value expression.
			 * @param source The source code location information.
			 */
			IndexExpression(
				ExpressionSharedPointer object,
				ExpressionSharedPointer index,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::IndexExpression, source ),
				index( std::move( index ) ),
				object( std::move( object ) ) {
			}
			
		};
		
		/**
		 * @brief Represents an 'instanceof' type check expression.
		 */
		struct InstanceofExpression : Expression {
			
			/** @brief The expression or object whose type is being checked. */
			ExpressionSharedPointer object;
			
			/** @brief The target type to check against. */
			TypeNodeSharedPointer targetType;
			
			/**
			 * @brief Constructs an InstanceofExpression.
			 * @param object The object expression to verify.
			 * @param targetType The type used for the comparison.
			 * @param source The source code location information.
			 */
			InstanceofExpression(
				ExpressionSharedPointer object,
				TypeNodeSharedPointer targetType,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::InstanceofExpression, source ),
				object( std::move( object ) ),
				targetType( std::move( targetType ) ) {
			}
			
		};
		
		/**
		 * @brief Represents an integer literal.
		 */
		struct IntegerLiteralExpression : Expression {
			
			/** @brief The raw string representation of the integer from the source code. */
			std::string raw;
			
			/** @brief The successfully parsed 64-bit integer value. */
			int64_t value;
			
			/**
			 * @brief Constructs an IntegerLiteralExpression.
			 * @param value The numerical value of the literal.
			 * @param raw The original string from the source.
			 * @param source The source code location information.
			 */
			IntegerLiteralExpression(
				int64_t value,
				const std::string& raw,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::IntegerLiteral, source ),
				raw( raw ),
				value( value ) {
			}
			
		};
		
		using IntegerLiteralExpressionSharedPointer = std::shared_ptr<IntegerLiteralExpression>;
		
		/**
		 * @brief Represents an anonymous function (lambda) expression.
		 */
		struct LambdaExpression : Expression {
			
			/** @brief The collection of statements forming the function body. */
			std::vector<StatementSharedPointer> body;
			
			/** @brief Flag indicating if the lambda is an asynchronous function. */
			bool isAsync = false;
			
			/** @brief The list of parameters accepted by the lambda. */
			std::vector<FunctionParameterSharedPointer> parameters;
			
			/** @brief The explicit or inferred return type of the lambda. */
			TypeNodeSharedPointer returnType;
			
			/**
			 * @brief Constructs a LambdaExpression.
			 * @param parameters The defined parameters for this lambda.
			 * @param returnType The expected return type.
			 * @param body The logic contained within the lambda.
			 * @param source The source code location information.
			 */
			LambdaExpression(
				std::vector<FunctionParameterSharedPointer> parameters,
				TypeNodeSharedPointer returnType,
				std::vector<StatementSharedPointer> body,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::LambdaExpression, source ),
				body( std::move( body ) ),
				parameters( std::move( parameters ) ),
				returnType( std::move( returnType ) ) {}
		};
		
		using LambdaExpressionSharedPointer = std::shared_ptr<LambdaExpression>;
		
		/**
		 * @brief Represents a functional 'match' expression.
		 */
		struct MatchExpression : Expression {
			
			/** @brief The fallback result if no patterns match the subject. */
			ExpressionSharedPointer defaultValue;
			
			/** @brief The sequence of patterns to compare against the subject. */
			std::vector<ExpressionSharedPointer> patterns;
			
			/** @brief The subject expression being evaluated. */
			ExpressionSharedPointer subject;
			
			/** @brief The corresponding result values for each pattern. */
			std::vector<ExpressionSharedPointer> values;
			
			/**
			 * @brief Constructs a MatchExpression.
			 * @param subject The value to match.
			 * @param patterns The available match cases.
			 * @param values The results associated with each case.
			 * @param defaultValue The default case result.
			 * @param source The source code location information.
			 */
			MatchExpression(
				ExpressionSharedPointer subject,
				std::vector<ExpressionSharedPointer> patterns,
				std::vector<ExpressionSharedPointer> values,
				ExpressionSharedPointer defaultValue,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::MatchExpression, source ),
				defaultValue( std::move( defaultValue ) ),
				patterns( std::move( patterns ) ),
				subject( std::move( subject ) ),
				values( std::move( values ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a direct member/field access expression (e.g., object.member).
		 */
		struct MemberAccessExpression : Expression {
			
			/** @brief The identifier name of the member being accessed. */
			std::string member;
			
			/** @brief The object owning the accessed member. */
			ExpressionSharedPointer object;
			
			/**
			 * @brief Constructs a MemberAccessExpression.
			 * @param object The instance being accessed.
			 * @param member The name of the field or member.
			 * @param source The source code location information.
			 */
			MemberAccessExpression(
				ExpressionSharedPointer object,
				const std::string& member,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::MemberAccessExpression, source ),
				member( member ),
				object( std::move( object ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a call to a method on an object.
		 */
		struct MethodCallExpression : Expression {
			
			/** @brief The arguments passed to the method during the call. */
			std::vector<ExpressionSharedPointer> arguments;
			
			/** @brief Named keyword arguments (name=value) for kwargs parameters. */
			std::vector<KeywordArgument> keywordArguments;
			
			/** @brief The name of the method to be invoked. */
			std::string method;
			
			/** @brief The object instance on which the method is called. */
			ExpressionSharedPointer object;
			
			/** @brief The generic type arguments provided for the method call. */
			std::vector<TypeNodeSharedPointer> typeArguments;
			
			/**
			 * @brief Constructs a MethodCallExpression.
			 * @param object The receiver object.
			 * @param method The method identifier.
			 * @param arguments The list of call arguments.
			 * @param source The source code location information.
			 */
			MethodCallExpression(
				ExpressionSharedPointer object,
				const std::string& method,
				std::vector<ExpressionSharedPointer> arguments,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::MethodCallExpression, source ),
				arguments( std::move( arguments ) ),
				method( method ),
				object( std::move( object ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a literal for 'none' or null.
		 */
		struct NoneLiteralExpression : Expression {
			
			/**
			 * @brief Constructs a NoneLiteralExpression.
			 * @param source The source code location information.
			 */
			NoneLiteralExpression( const lookup::SourceSharedPointer& source ) : Expression( Node::Kind::NoneLiteral, source ) {
			}
			
		};
		
		/**
		 * @brief Represents a range expression (e.g., start..end).
		 */
		struct RangeExpression : Expression {
			
			/** @brief The expression marking the end of the range. */
			ExpressionSharedPointer end;
			
			/** @brief Whether the end value is included in the range. */
			bool inclusive;
			
			/** @brief The expression marking the start of the range. */
			ExpressionSharedPointer start;
			
			/**
			 * @brief Constructs a RangeExpression.
			 * @param start The starting value.
			 * @param end The ending value.
			 * @param inclusive True if the range includes the end point.
			 * @param source The source code location information.
			 */
			RangeExpression(
				ExpressionSharedPointer start,
				ExpressionSharedPointer end,
				bool inclusive,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::RangeExpression, source ),
				end( std::move( end ) ),
				inclusive( inclusive ),
				start( std::move( start ) ) {
			}
			
		};
		
		/**
		 * @brief Represents the 'self' or 'this' reference within an instance scope.
		 */
		struct SelfExpression : Expression {
			
			/**
			 * @brief Constructs a SelfExpression.
			 * @param source The source code location information.
			 */
			SelfExpression( const lookup::SourceSharedPointer& source ) : Expression( Node::Kind::SelfExpression, source ) {
			}
			
		};
		
		/**
		 * @brief Represents a string literal.
		 */
		struct RegexLiteralExpression : Expression {
			std::string pattern;
			RegexLiteralExpression(
				const std::string& pattern,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::RegexLiteral, source ),
				pattern( pattern ) {
			}
		};
		
		struct StringLiteralExpression : Expression {
			
			/** @brief The actual string content of the literal. */
			std::string value;
			
			/**
			 * @brief Constructs a StringLiteralExpression.
			 * @param value The content of the string.
			 * @param source The source code location information.
			 */
			StringLiteralExpression(
				const std::string& value,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::StringLiteral, source ),
				value( value ) {
			}
			
		};
		
		/**
		 * @brief Represents a subtyping check expression.
		 */
		struct SubclassofExpression : Expression {
			
			/** @brief The specific type being tested for inheritance. */
			TypeNodeSharedPointer sourceType;
			
			/** @brief The potential parent or target type. */
			TypeNodeSharedPointer targetType;
			
			/**
			 * @brief Constructs a SubclassofExpression.
			 * @param sourceType The child type.
			 * @param target The parent type to check against.
			 * @param source The source code location information.
			 */
			SubclassofExpression(
				TypeNodeSharedPointer sourceType,
				TypeNodeSharedPointer target,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::SubclassofExpression, source ),
				sourceType( std::move( sourceType ) ),
				targetType( std::move( target ) ) {
			}
			
		};
		
		/**
		 * @brief Represents the 'super' reference to a parent class.
		 */
		struct SuperExpression : Expression {
			
			/**
			 * @brief Constructs a SuperExpression.
			 * @param source The source code location information.
			 */
			SuperExpression( const lookup::SourceSharedPointer& source ) : Expression( Node::Kind::SuperExpression, source ) {
			}
			
		};
		
		/**
		 * @brief Represents a tuple literal expression containing multiple elements.
		 */
		struct TupleExpression : Expression {
			
			/** @brief The collection of expressions representing the tuple members. */
			std::vector<ExpressionSharedPointer> elements;
			
			/**
			 * @brief Constructs a TupleExpression.
			 * @param elements The list of tuple members.
			 * @param source The source code location information.
			 */
			TupleExpression(
				std::vector<ExpressionSharedPointer> elements,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::TupleExpression, source ),
				elements( std::move( elements ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a symbolic reference to a type in an expression context.
		 */
		struct TypeReferenceExpression : Expression {
			
			/** @brief The identifier name of the type being referenced. */
			std::string typeName;
			
			/**
			 * @brief Constructs a TypeReferenceExpression.
			 * @param name The name of the type.
			 * @param source The source code location information.
			 */
			TypeReferenceExpression(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::TypeReferenceExpression, source ),
				typeName( name ) {
			}
			
		};
		
		/**
		 * @brief Represents a unary operation expression (e.g., negation or logical NOT).
		 */
		struct UnaryExpression : Expression {
			
			/** @brief True if the operator comes before the operand (prefix), false if after (postfix). */
			bool isPrefix;
			
			/** @brief The operand being operated upon. */
			ExpressionSharedPointer operand;
			
			/** @brief The kind of unary operation being performed. */
			token::Type operation;
			
			/**
			 * @brief Constructs a UnaryExpression.
			 * @param operation The operator token type.
			 * @param operand The target expression.
			 * @param prefix Whether it is a prefix operator.
			 * @param source The source code location information.
			 */
			UnaryExpression(
				token::Type operation,
				ExpressionSharedPointer operand,
				bool prefix,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::UnaryExpression, source ),
				isPrefix( prefix ),
				operand( std::move( operand ) ),
				operation( operation ) {
			}
			
		};
		
		/**
		 * @brief Represents a generator 'yield' expression for lazy value production.
		 */
		struct YieldExpression : Expression {
			
			/** @brief The expression whose value is being yielded by the generator. */
			ExpressionSharedPointer value;
			
			/**
			 * @brief Constructs a YieldExpression.
			 * @param value The expression to yield.
			 * @param source The source code location information.
			 */
			YieldExpression(
				ExpressionSharedPointer value,
				const lookup::SourceSharedPointer& source
			) : Expression( Node::Kind::YieldExpression, source ),
				value( std::move( value ) ) {
			}
			
		};
		
		// ==========================================
		// Statements
		// ==========================================
		
		/**
		 * @brief Represents an assignment statement.
		 */
		struct AssignStatement : Statement {
			
			/** @brief The assignment operation type (e.g., ASSIGN, PLUS_ASSIGN). */
			token::Type operation;
			
			/** @brief The left-hand side expression being assigned to. */
			ExpressionSharedPointer target;
			
			/** @brief The right-hand side expression representing the value. */
			ExpressionSharedPointer value;
			
			/**
			 * @brief Constructs an assignment statement.
			 * @param target Left-hand side expression.
			 * @param operation Token type of the assignment.
			 * @param value Right-hand side expression.
			 * @param source Source mapping information.
			 */
			AssignStatement(
				ExpressionSharedPointer target,
				token::Type operation,
				ExpressionSharedPointer value,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::AssignmentStatement, source ),
				operation( operation ),
				target( std::move( target ) ),
				value( std::move( value ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a block statement containing multiple nested statements.
		 */
		struct BlockStatement : Statement {
			
			/** @brief Sequential list of statements contained within this block. */
			std::vector<StatementSharedPointer> statements;
			
			/**
			 * @brief Constructs a block statement.
			 * @param statements Vector of shared pointers to statements.
			 * @param source Source mapping information.
			 */
			BlockStatement(
				std::vector<StatementSharedPointer> statements,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::BlockStatement, source ),
				statements( std::move( statements ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a 'break' control flow statement.
		 */
		struct BreakStatement : Statement {
			
			/**
			 * @brief Constructs a break statement.
			 * @param source Source mapping information.
			 */
			BreakStatement( const lookup::SourceSharedPointer& source ) : Statement( Node::Kind::BreakStatement, source ) {
			}
			
		};
		
		/**
		 * @brief Represents a 'continue' control flow statement.
		 */
		struct ContinueStatement : Statement {
			
			/**
			 * @brief Constructs a continue statement.
			 * @param source Source mapping information.
			 */
			ContinueStatement( const lookup::SourceSharedPointer& source ) : Statement( Node::Kind::ContinueStatement, source ) {}
			
		};
		
		/**
		 * @brief Represents a 'defer' statement for deferred execution logic.
		 */
		struct DeferStatement : Statement {
			
			/** @brief The specific statement to be executed at the end of the current scope. */
			StatementSharedPointer body;
			
			/**
			 * @brief Constructs a defer statement.
			 * @param body The statement to defer.
			 * @param source Source mapping information.
			 */
			DeferStatement(
				StatementSharedPointer body,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::DeferStatement, source ),
				body( std::move( body ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a manual memory deletion statement.
		 */
		struct DeleteStatement : Statement {
			
			/** @brief The expression identifying the object to be deallocated. */
			ExpressionSharedPointer expression;
			
			/**
			 * @brief Constructs a delete statement.
			 * @param expression The expression to delete.
			 * @param source Source mapping information.
			 */
			DeleteStatement(
				ExpressionSharedPointer expression,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::DeleteStatement, source ),
				expression( std::move( expression ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a single catch clause for handling specific exceptions.
		 */
		struct ExceptionClause {
			
			/** @brief The code block to execute if an exception matches. */
			std::vector<StatementSharedPointer> body;
			
			/** @brief List of exception types that this clause can handle. */
			std::vector<TypeNodeSharedPointer> exceptionTypes;
			
			/** @brief Source mapping for the exception clause. */
			lookup::SourceSharedPointer source;
			
			/** @brief The identifier name for the caught exception object. */
			std::string variableName;
			
		};
		
		/**
		 * @brief Represents a statement wrapper that encapsulates a single expression.
		 */
		struct ExpressionStatement : Statement {
			
			/** @brief The expression that constitutes this statement. */
			ExpressionSharedPointer expression;
			
			/**
			 * @brief Constructs an expression statement.
			 * @param expression The wrapped expression.
			 * @param source Source mapping information.
			 */
			ExpressionStatement(
				ExpressionSharedPointer expression,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::ExpressionStatement, source ),
				expression( std::move( expression ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a 'for' loop supporting both iterator and C-style logic.
		 */
		struct ForStatement : Statement {
			
			/** @brief Statements within the loop body. */
			std::vector<StatementSharedPointer> body;
			
			/** @brief The termination condition for C-style loops. */
			ExpressionSharedPointer condition;
			
			/** @brief Initializer expression for C-style loops. */
			ExpressionSharedPointer initializer;
			
			/** @brief The expression being iterated over in iterator-style loops. */
			ExpressionSharedPointer iterable;
			
			/** @brief Boolean flag indicating if this is a C-style loop (init; cond; update). */
			bool isCStyle = false;
			
			/** @brief The update statement executed after each iteration in C-style loops. */
			StatementSharedPointer update;
			
			/** @brief The primary iteration variable name. */
			std::string variable;
			
			/** @brief The secondary iteration variable name (e.g., for key-value pairs). */
			std::string variable2;
			
			/** @brief Explicit type annotation for the primary variable. */
			TypeNodeSharedPointer variableType;
			
			/** @brief Explicit type annotation for the secondary variable. */
			TypeNodeSharedPointer variableType2;
			
			/**
			 * @brief Constructs an iterator-style for statement.
			 * @param variable The name of the iteration variable.
			 * @param iterable The expression to iterate over.
			 * @param body The statements in the loop.
			 * @param source Source mapping information.
			 */
			ForStatement(
				const std::string& variable,
				ExpressionSharedPointer iterable,
				std::vector<StatementSharedPointer> body,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::ForStatement, source ),
				body( std::move( body ) ),
				iterable( std::move( iterable ) ),
				variable( variable ) {
			}
			
		};
		
		using ForStatementSharedPointer = std::shared_ptr<ForStatement>;
		
		/**
		 * @brief Represents a conditional branching statement.
		 */
		struct IfStatement : Statement {
			
			/** @brief The Boolean expression evaluated for the primary branch. */
			ExpressionSharedPointer condition;
			
			/** @brief Optional branches defined by 'else if' structures. */
			std::vector<std::pair<ExpressionSharedPointer, std::vector<StatementSharedPointer>>> elifBranches;
			
			/** @brief The block executed if no conditions match. */
			std::vector<StatementSharedPointer> elseBody;
			
			/** @brief The block executed if the primary condition is true. */
			std::vector<StatementSharedPointer> thenBody;
			
			/**
			 * @brief Constructs an if statement.
			 * @param condition The branch condition.
			 * @param thenBody Statements for the primary branch.
			 * @param source Source mapping information.
			 */
			IfStatement(
				ExpressionSharedPointer condition,
				std::vector<StatementSharedPointer> thenBody,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::IfStatement, source ),
				condition( std::move( condition ) ),
				thenBody( std::move( thenBody ) ) {
			}
			
		};
		
		using IfStatementSharedPointer = std::shared_ptr<IfStatement>;
		
		/**
		 * @brief Represents a single arm of a match statement.
		 */
		struct MatchArmNode : Node {
			
			/** @brief Statements to execute when this arm is matched. */
			std::vector<StatementSharedPointer> body;
			
			/** @brief Optional Boolean expression that must be true for the match to proceed. */
			ExpressionSharedPointer guard;
			
			/** @brief The pattern expression to match against the subject. */
			ExpressionSharedPointer pattern;
			
			/**
			 * @brief Constructs a match arm.
			 * @param pattern Match pattern.
			 * @param guard Optional guard expression.
			 * @param body Arm body statements.
			 * @param source Source mapping information.
			 */
			MatchArmNode(
				ExpressionSharedPointer pattern,
				ExpressionSharedPointer guard,
				std::vector<StatementSharedPointer> body,
				const lookup::SourceSharedPointer& source
			) : Node( Node::Kind::MatchArm, source ),
				body( std::move( body ) ),
				guard( std::move( guard ) ),
				pattern( std::move( pattern ) ) {
			}
			
		};
		
		using MatchArmNodeSharedPointer = std::shared_ptr<MatchArmNode>;
		
		/**
		 * @brief Represents a pattern-matching control flow structure.
		 */
		struct MatchStatement : Statement {
			
			/** @brief The set of arms (patterns and bodies) in the match block. */
			std::vector<MatchArmNodeSharedPointer> arms;
			
			/** @brief The expression whose value is being matched. */
			ExpressionSharedPointer subject;
			
			/**
			 * @brief Constructs a match statement.
			 * @param subject Expression to match.
			 * @param arms Available match branches.
			 * @param source Source mapping information.
			 */
			MatchStatement(
				ExpressionSharedPointer subject,
				std::vector<MatchArmNodeSharedPointer> arms,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::MatchStatement, source ),
				arms( std::move( arms ) ),
				subject( std::move( subject ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a 'pass' or no-operation statement.
		 */
		struct PassStatement : Statement {
			
			/**
			 * @brief Constructs a pass statement.
			 * @param source Source mapping information.
			 */
			PassStatement( const lookup::SourceSharedPointer& source ) : Statement( Node::Kind::PassStatement, source ) {
			}
			
		};
		
		/**
		 * @brief Represents a function return statement.
		 */
		struct ReturnStatement : Statement {
			
			/** @brief The value to return (nullptr if returning void). */
			ExpressionSharedPointer value;
			
			/**
			 * @brief Constructs a return statement.
			 * @param value Expression to return.
			 * @param source Source mapping information.
			 */
			ReturnStatement(
				ExpressionSharedPointer value,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::ReturnStatement, source ),
				value( std::move( value ) ) {
			}
			
		};
		
		using ReturnStatementSharedPointer = std::shared_ptr<ReturnStatement>;
		
		/**
		 * @brief Represents an individual case branch within a switch statement.
		 */
		struct SwitchCaseNode : Node {
			
			/** @brief Logic to execute for this case. */
			std::vector<StatementSharedPointer> body;
			
			/** @brief Indicates if this is the fallback 'default' case. */
			bool isDefault = false;
			
			/** @brief The specific value or pattern to compare against. */
			ExpressionSharedPointer pattern;
			
			/**
			 * @brief Constructs a switch case.
			 * @param pattern Comparison pattern.
			 * @param isDefault Default flag.
			 * @param body Case statements.
			 * @param source Source mapping information.
			 */
			SwitchCaseNode(
				ExpressionSharedPointer pattern,
				bool isDefault,
				std::vector<StatementSharedPointer> body,
				const lookup::SourceSharedPointer& source
			) : Node( Node::Kind::SwitchCase, source ),
				body( std::move( body ) ),
				isDefault( isDefault ),
				pattern( std::move( pattern ) ) {
			}
			
		};
		
		using SwitchCaseNodeSharedPointer = std::shared_ptr<SwitchCaseNode>;
		
		/**
		 * @brief Represents a multi-way branch based on a single expression value.
		 */
		struct SwitchStatement : Statement {
			
			/** @brief Collection of case branches. */
			std::vector<SwitchCaseNodeSharedPointer> cases;
			
			/** @brief The expression being evaluated for selection. */
			ExpressionSharedPointer subject;
			
			/**
			 * @brief Constructs a switch statement.
			 * @param subject Expression to evaluate.
			 * @param cases List of cases.
			 * @param source Source mapping information.
			 */
			SwitchStatement(
				ExpressionSharedPointer subject,
				std::vector<SwitchCaseNodeSharedPointer> cases,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::SwitchStatement, source ),
				cases( std::move( cases ) ),
				subject( std::move( subject ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a statement used to raise an exception.
		 */
		struct ThrowStatement : Statement {
			
			/** @brief The expression representing the exception object to be thrown. */
			ExpressionSharedPointer expression;
			
			/**
			 * @brief Constructs a throw statement.
			 * @param expression Exception expression.
			 * @param source Source mapping information.
			 */
			ThrowStatement(
				ExpressionSharedPointer expression,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::ThrowStatement, source ),
				expression( std::move( expression ) ) {
			}
			
		};
		
		/**
		 * @brief Represents a try-catch-finally error handling block.
		 */
		struct TryCatchStatement : Statement {
			
			/** @brief List of exception clauses for handling specific errors. */
			std::vector<ExceptionClause> exceptionClauses;
			
			/** @brief The block of code guaranteed to execute at the end. */
			std::vector<StatementSharedPointer> finallyBody;
			
			/** @brief The block of code being monitored for exceptions. */
			std::vector<StatementSharedPointer> tryBody;
			
			/**
			 * @brief Constructs a try-catch statement.
			 * @param source Source mapping information.
			 */
			TryCatchStatement( const lookup::SourceSharedPointer& source ) : Statement( Node::Kind::TryCatchStatement, source ) {
			}
			
		};
		
		using TryCatchStatementSharedPointer = std::shared_ptr<TryCatchStatement>;
		
		struct InlineAssemblyOperand {
			std::string constraint;
			ExpressionSharedPointer expression;
		};
		
		struct InlineAssemblyStatement : Statement {
			
			bool isVolatile;
			std::string asmTemplate;
			std::vector<InlineAssemblyOperand> outputs;
			std::vector<InlineAssemblyOperand> inputs;
			std::vector<std::string> clobbers;
			
			InlineAssemblyStatement(
				bool isVolatile,
				const std::string& asmTemplate,
				std::vector<InlineAssemblyOperand> outputs,
				std::vector<InlineAssemblyOperand> inputs,
				std::vector<std::string> clobbers,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::InlineAssemblyStatement, source ),
				isVolatile( isVolatile ),
				asmTemplate( asmTemplate ),
				outputs( std::move( outputs ) ),
				inputs( std::move( inputs ) ),
				clobbers( std::move( clobbers ) ) {
			}
		
		};
		
		/**
		 * @brief Represents a block of code where safety constraints are bypassed.
		 */
		struct UnsafeBlockStatement : Statement {
			
			/** @brief Statements residing within the unsafe context. */
			std::vector<StatementSharedPointer> body;
			
			/**
			 * @brief Constructs an unsafe block.
			 * @param body Internal statements.
			 * @param source Source mapping information.
			 */
			UnsafeBlockStatement(
				std::vector<StatementSharedPointer> body,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::UnsafeBlock, source ),
				body( std::move( body ) ) {
			}
			
		};
		
		struct OwnershipAnnotation {
			bool isHeapAllocated = false;
			bool isArenaManaged = false;
			bool isMoved = false;
			std::string typeName;
		};
		
		/**
		 * @brief Represents a variable binding or declaration
		 */
		struct VariableStatement : Statement {
			
			/** @brief Initial value assigned to the variable. */
			ExpressionSharedPointer initializer;
			
			/** @brief Flag indicating if the binding is immutable (constant). */
			bool isConstant;
			
			/** @brief Flag indicating if the variable can be reassigned. */
			bool isMutable;
			
			/** @brief The identifier name of the variable. */
			std::string name;
			
			/** @brief Ownership data populated by borrow checker for codegen auto-free. */
			std::optional<OwnershipAnnotation> ownershipAnnotation;
			
			/** @brief Optional explicit type annotation. */
			TypeNodeSharedPointer type;
			
			/**
			 * @brief Constructs a variable declaration.
			 * @param name Variable identifier.
			 * @param type Type information.
			 * @param initializer Starting value.
			 * @param mutable_ Mutability flag.
			 * @param constant Constancy flag.
			 * @param source Source mapping information.
			 */
			VariableStatement(
				const std::string& name,
				TypeNodeSharedPointer type,
				ExpressionSharedPointer initializer,
				bool mutable_,
				bool constant,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::VariableStatement, source ),
				initializer( std::move( initializer ) ),
				isConstant( constant ),
				isMutable( mutable_ ),
				name( name ),
				type( std::move( type ) ) {
			}
			
		};
		
		using VariableStatementSharedPointer = std::shared_ptr<VariableStatement>;
		
		/**
		 * @brief Represents a loop that continues while a condition remains true.
		 */
		struct WhileStatement : Statement {
			
			/** @brief Statements to execute repeatedly. */
			std::vector<StatementSharedPointer> body;
			
			/** @brief The Boolean expression checked before each iteration. */
			ExpressionSharedPointer condition;
			
			/**
			 * @brief Constructs a while statement.
			 * @param condition Loop condition.
			 * @param body Loop body.
			 * @param source Source mapping information.
			 */
			WhileStatement(
				ExpressionSharedPointer condition,
				std::vector<StatementSharedPointer> body,
				const lookup::SourceSharedPointer& source
			) : Statement( Node::Kind::WhileStatement, source ),
				body( std::move( body ) ),
				condition( std::move( condition ) ) {
			}
		};
		
		/**
		 * @brief Represents an individual entity being imported from another module.
		 *
		 * This structure holds the original name of the entity and an optional alias
		 * if the entity is renamed during the import process (e.g., using an 'as' clause).
		 */
		struct ImportItem {
			
			/** @brief An optional alternative name used to reference the entity within the current scope. */
			std::string alias;
			
			/** @brief The original identifier name of the entity from the source module. */
			std::string name;
			
		};
		
		/**
		 * @brief Represents a module-level import declaration.
		 */
		struct ImportDeclaration : Declaration {
			
			/** @brief Optional alias used for the import (e.g., 'z' in 'import x.y as z'). */
			std::string alias;
			
			/** @brief Flag indicating if all symbols are imported (e.g., 'import x.y.*'). */
			bool importAll = false;
			
			/** @brief List of specific items being imported in a 'from...import' statement. */
			std::vector<ImportItem> importItems;
			
			/** @brief Flag indicating if this is a 'from...import' style declaration. */
			bool isFromImport = false;
			
			/** @brief The hierarchical path of the module (e.g., ["std", "io"]). */
			std::vector<std::string> path;
			
			/**
			 * @brief Constructs an ImportDeclaration.
			 * @param path The module path components.
			 * @param source Shared pointer to the source location.
			 */
			ImportDeclaration(
				std::vector<std::string> path,
				const lookup::SourceSharedPointer& source
			) : Declaration( Node::Kind::ImportDeclaration, source ),
				path( std::move( path ) ) {
			}
			
		};
		
		using ImportDeclarationSharedPointer = std::shared_ptr<ImportDeclaration>;
		
		/**
		 * @brief Represents an interface definition.
		 */
		struct InterfaceDeclaration : Declaration {
			
			/** @brief List of generic parameters associated with this interface. */
			std::vector<GenericParameterSharedPointer> genericParameters;
			
			/** @brief Flag indicating if this interface is marked as final. */
			bool isFinal = false;
			
			/** @brief Abstract or default methods defined within the interface. */
			std::vector<DeclarationSharedPointer> methods;
			
			/** @brief The unique identifier name of the interface. */
			std::string name;
			
			/** @brief Nested type or constant definitions within the interface scope. */
			std::vector<DeclarationSharedPointer> nestedDeclarations;
			
			/** @brief List of interfaces that this interface inherits from. */
			std::vector<TypeNodeSharedPointer> superInterfaces;
			
			/**
			 * @brief Constructs an InterfaceDeclaration.
			 * @param name The interface identifier.
			 * @param source Shared pointer to the source location.
			 */
			InterfaceDeclaration(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Declaration( Node::Kind::InterfaceDeclaration, source ),
				name( name ) {
			}
			
		};
		
		using InterfaceDeclarationSharedPointer = std::shared_ptr<InterfaceDeclaration>;
		
		/**
		 * @brief Represents a module name declaration.
		 */
		struct ModuleDeclaration : Declaration {
			
			/** @brief The unique identifier name of the module. */
			std::string name;
			
			/**
			 * @brief Constructs a ModuleDeclaration.
			 * @param name The module identifier.
			 * @param source Shared pointer to the source location.
			 */
			ModuleDeclaration(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Declaration( Node::Kind::ModuleDeclaration, source ),
				name( name ) {
			}
			
		};
		
		using ModuleDeclarationSharedPointer = std::shared_ptr<ModuleDeclaration>;
		
		/**
		 * @brief Represents a struct definition.
		 */
		struct StructDeclaration : Declaration {
			
			/** @brief List of data members or fields within the struct. */
			std::vector<FieldDeclarationSharedPointer> fields;
			
			/** @brief List of generic parameters for template-like struct definitions. */
			std::vector<GenericParameterSharedPointer> genericParameters;
			
			/** @brief Member methods or functions associated with the struct. */
			std::vector<DeclarationSharedPointer> methods;
			
			/** @brief The unique identifier name of the struct. */
			std::string name;
			
			/** @brief Nested declarations defined within the struct scope. */
			std::vector<DeclarationSharedPointer> nestedDeclarations;
			
			/** @brief List of trait names that this struct uses. */
			std::vector<std::string> usedTraits;
			
			/**
			 * @brief Constructs a StructDeclaration.
			 * @param name The struct identifier.
			 * @param source Shared pointer to the source location.
			 */
			StructDeclaration(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Declaration( Node::Kind::StructDeclaration, source ),
				name( name ) {
			}
			
		};
		
		using StructDeclarationSharedPointer = std::shared_ptr<StructDeclaration>;
		
		/**
		 * @brief Represents a trait definition.
		 */
		struct TraitDeclaration : Declaration {
			
			/** @brief Required fields that must be implemented or provided by the target. */
			std::vector<FieldDeclarationSharedPointer> fields;
			
			/** @brief List of generic parameters for the trait. */
			std::vector<GenericParameterSharedPointer> genericParameters;
			
			/** @brief Trait methods that define the required behavior. */
			std::vector<DeclarationSharedPointer> methods;
			
			/** @brief The unique identifier name of the trait. */
			std::string name;
			
			/**
			 * @brief Constructs a TraitDeclaration.
			 * @param name The trait identifier.
			 * @param source Shared pointer to the source location.
			 */
			TraitDeclaration(
				const std::string& name,
				const lookup::SourceSharedPointer& source
			) : Declaration( Node::Kind::TraitDeclaration, source ),
				name( name ) {
			}
			
		};
		
		using TraitDeclarationSharedPointer = std::shared_ptr<TraitDeclaration>;
		
		struct ConstantDeclaration : Declaration {
			
			ExpressionSharedPointer initializer;
			bool isGlobalVariable;
			std::string name;
			TypeNodeSharedPointer type;
			
			ConstantDeclaration(
				const std::string& name,
				TypeNodeSharedPointer type,
				ExpressionSharedPointer initializer,
				ast::AccessModifier access,
				const lookup::SourceSharedPointer& source,
				bool isGlobalVariable = false
			) : Declaration( Node::Kind::ConstantDeclaration, source ),
				initializer( std::move( initializer ) ),
				isGlobalVariable( isGlobalVariable ),
				name( name ),
				type( std::move( type ) ) {
				this->access = access;
			}
		
		};
		
		using ConstantDeclarationSharedPointer = std::shared_ptr<ConstantDeclaration>;
		
		/**
		 * @brief Represents a type aliasing declaration.
		 */
		struct TypeAliasDeclaration : Declaration {
			
			/** @brief The underlying type node that is being aliased. */
			TypeNodeSharedPointer aliasedTypeNode;
			
			/** @brief Generic parameters if the alias itself is generic. */
			std::vector<GenericParameterSharedPointer> genericParameters;
			
			/** @brief The new identifier name for the alias. */
			std::string name;
			
			/**
			 * @brief Constructs a TypeAliasDeclaration.
			 * @param name The alias identifier.
			 * @param type Shared pointer to the type being aliased.
			 * @param source Shared pointer to the source location.
			 */
			TypeAliasDeclaration(
				const std::string& name,
				TypeNodeSharedPointer type,
				const lookup::SourceSharedPointer& source
			) : Declaration( Node::Kind::TypeAliasDeclaration, source ),
				aliasedTypeNode( std::move( type ) ),
				name( name ) {
			}
			
		};
		
		using TypeAliasDeclarationSharedPointer = std::shared_ptr<TypeAliasDeclaration>;
		
		/**
		 * @brief The root node of the Abstract Syntax Tree representing a single source file.
		 */
		struct Program : Node {
			
			/** @brief A list of all global-level declarations within the program. */
			std::vector<DeclarationSharedPointer> declarations;
			
			/** @brief A list of all export statements defined in the source file. */
			std::vector<ExportDeclarationSharedPointer> exports;
			
			/** @brief A list of all import statements used by the source file. */
			std::vector<ImportDeclarationSharedPointer> imports;
			
			/** @brief The specific module declaration associated with this program. */
			ModuleDeclarationSharedPointer module;
			
			/**
			 * @brief Constructs a new Program node.
			 * @param source Shared pointer to the source location information.
			 */
			Program( const lookup::SourceSharedPointer& source ) : Node( Node::Kind::Program, source ) {
			}
			
		};
		
		using ProgramSharedPointer = std::shared_ptr<Program>;
		
	} // namespace uranite::node::nodes
	
} // namespace uranite::node

#endif // end _URANITE_AST_NODE_HPP_
