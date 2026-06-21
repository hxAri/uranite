
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

#ifndef _URANITE_DESCRIPTOR_DESCRIPTOR_HPP_
#define _URANITE_DESCRIPTOR_DESCRIPTOR_HPP_

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#include "uranite/semantic/typeref.hpp"

namespace uranite::descriptor {
	
	/**
	 * @brief A functional alias for generating LLVM Intermediate Representation (IR) values.
	 * 
	 * This delegate is used to define the logic for emitting LLVM instructions for
	 * functions, methods, or intrinsic operations.
	 * 
	 * @param builder The LLVM IR builder used to emit instructions.
	 * @param context The global LLVM context for managing types and constant data.
	 * @param self A pointer to the 'self' or 'this' instance (null if not applicable).
	 * @param arguments A vector of LLVM values representing the arguments passed to the generator.
	 * @return llvm::Value* The resulting LLVM value produced by the generation logic.
	 */
	using IRGenerator = std::function<llvm::Value*(
		llvm::IRBuilder<>& builder,
		llvm::LLVMContext& context,
		llvm::Value* self,
		std::vector<llvm::Value*>& arguments,
		std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments
	)>;
	
	/**
	 * @brief A functional alias for factory methods that produce LLVM Type instances.
	 * 
	 * Used to lazily resolve or create LLVM types within a specific compilation context.
	 * 
	 * @param context The LLVM context where the type should be created or retrieved.
	 * @return llvm::Type* The generated LLVM type pointer.
	 */
	using LLVMTypeFactory = std::function<llvm::Type*( llvm::LLVMContext& context )>;
	
	/**
	 * @brief Represents a parameter within a built-in method definition.
	 */
	struct MethodParameter {
		
		/** @brief The identifier name of the parameter. */
		std::string name;
		
		/** @brief The name of the type associated with this parameter. */
		std::string typeName;
		
		MethodParameter(
			const std::string& name,
			const std::string& typeName
		) : name( name ),
			typeName( typeName ) {
		}
	
	};
	
	/**
	 * @brief Describes a built-in method, including its signature and IR generation logic.
	 */
	struct BuiltinMethodDescriptor {
		
		/** @brief The generator function responsible for emitting LLVM IR for this method. */
		IRGenerator irGenerator;
		
		/** @brief The identifier name of the method. */
		std::string name;
		
		/** @brief The list of parameters accepted by this method. */
		std::vector<MethodParameter> parameters;
		
		/** @brief The name of the type returned by this method. */
		std::string returnTypeName;
		
		BuiltinMethodDescriptor(
			const std::string& name,
			const std::string& returnTypeName,
			const std::vector<MethodParameter> &parameters,
			const IRGenerator& irGenerator
		) : irGenerator( irGenerator ),
			name( name ),
			parameters( parameters ),
			returnTypeName( returnTypeName ) {
		}
	
	};
	
	/**
	 * @brief Describes a built-in type system entity and its metadata.
	 */
	struct BuiltinTypeDescriptor {
		
		/** @brief Indicates whether the type is final and cannot be inherited from. */
		bool isFinal = false;
		
		/** @brief The factory responsible for creating the corresponding LLVM type. */
		LLVMTypeFactory llvmTypeFactory;
		
		/** @brief The list of built-in methods associated with this type. */
		std::vector<BuiltinMethodDescriptor> methods;
		
		/** @brief The identifier name of the type. */
		std::string name;
		
		/** @brief The package or namespace where this type resides. */
		std::string package;
		
		/** @brief The name of the parent type from which this type inherits. */
		std::string parentName;
		
		BuiltinTypeDescriptor(
			const bool& isFinal,
			const std::string& name,
			const std::string& package,
			const std::string& parentName,
			const std::vector<BuiltinMethodDescriptor> methods,
			const LLVMTypeFactory& llvmTypeFactory
		) : isFinal( isFinal ),
			llvmTypeFactory( llvmTypeFactory ),
			methods( methods ),
			name( name ),
			package( package ),
			parentName( parentName ) {
		}
	
	};
	
	/**
	 * @brief A final Builtin registry class for managing built-in type descriptors and their associated methods.
	 * 
	 * This registry handles the mapping between primitive types and their Object-Oriented Programming (OOP)
	 * counterparts, as well as integration with LLVM and semantic analysis.
	 */
	class Builtin final {
		
		public:
			
			/**
			 * @brief Default constructor for the Builtin.
			 */
			Builtin();
			
			/**
			 * @brief Retrieves all registered type descriptors.
			 * @return A constant reference to the map of all types.
			 */
			const std::unordered_map<std::string, BuiltinTypeDescriptor>& allTypes() const {
				return this->types;
			}
			
			/**
			 * @brief Gets the corresponding LLVM type for a given type name.
			 * @param typeName The name of the type to resolve.
			 * @param context The LLVM context used for type generation.
			 * @return A pointer to the LLVM Type, or nullptr if not found.
			 */
			llvm::Type* getLLVMType( const std::string& typeName, llvm::LLVMContext& context ) const;
			
			/**
			 * @brief Checks if a given name belongs to a registered built-in type.
			 * @param name The name to verify.
			 * @return True if it is a built-in type, false otherwise.
			 */
			static bool isBuiltinType( const std::string& name );
			
			/**
			 * @brief Checks if a given name represents a primitive type.
			 * @param name The name to verify.
			 * @return True if it is a primitive name, false otherwise.
			 */
			static bool isPrimitiveName( const std::string& name );
			
			/**
			 * @brief Searches for a built-in type descriptor by its name.
			 * @param name The name of the type.
			 * @return A constant pointer to the descriptor, or nullptr if not found.
			 */
			const BuiltinTypeDescriptor* lookup( const std::string& name ) const;
			
			/**
			 * @brief Searches for a specific method within a built-in type.
			 * @param typeName The name of the containing type.
			 * @param methodName The name of the method to find.
			 * @return A constant pointer to the method descriptor, or nullptr if not found.
			 */
			const BuiltinMethodDescriptor* lookupMethod( const std::string& typeName, const std::string& methodName ) const;
			
			/**
			 * @brief Maps a primitive name to its corresponding OOP wrapper name.
			 * @param primitiveName The name of the primitive (e.g., "int32").
			 * @return The OOP equivalent name (e.g., "Integer").
			 */
			static const std::string& oopNameForPrimitive( const std::string& primitiveName );
			
			/**
			 * @brief Populates a semantic registry with the types from this registry.
			 * @param registry The semantic registry to be populated.
			 */
			void populateSemaTypes( semantic::Registry& registry ) const;
			
			/**
			 * @brief Maps an OOP wrapper name back to its primitive counterpart.
			 * @param oopName The name of the OOP wrapper.
			 * @return The primitive equivalent name.
			 */
			static const std::string& primitiveNameForOop( const std::string& oopName );
			
			/**
			 * @brief Registers all built-in types into the registry.
			 */
			void registerAllTypes();
		
		private:
			
			/**
			 * @brief Registers the boolean primitive type.
			 */
			void registerBooleanType();
			
			/**
			 * @brief Registers the byte primitive type.
			 */
			void registerByteType();
			
			/**
			 * @brief Registers the character primitive type.
			 */
			void registerCharType();
			
			/**
			 * @brief Registers all floating-point primitive types.
			 */
			void registerFloatTypes();
			
			/**
			 * @brief Registers all signed integer primitive types.
			 */
			void registerIntTypes();
			
			/**
			 * @brief Registers the special internal types.
			 */
			void registerSpecialTypes();
			
			/**
			 * @brief Registers the string primitive type.
			 */
			void registerStringType();
			
			/**
			 * @brief Internal helper to register a specific type descriptor.
			 * @param descriptor The descriptor to register.
			 */
			void registerType( BuiltinTypeDescriptor descriptor );
			
			/**
			 * @brief Registers all unsigned integer primitive types.
			 */
			void registerUIntTypes();
			
			/** @brief Mapping from OOP names to primitive names. */
			static const std::unordered_map<std::string,std::string> oopToPrimitive;
			
			/** @brief Mapping from primitive names to OOP names. */
			static const std::unordered_map<std::string,std::string> primitiveToOOP;
			
			/** @brief Internal storage for all registered built-in type descriptors. */
			std::unordered_map<std::string,BuiltinTypeDescriptor> types;
		
	};
	
} // namespace uranite::descriptor

#endif // end _URANITE_DESCRIPTOR_DESCRIPTOR_HPP_
