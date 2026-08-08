
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

#include <fmt/format.h>

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>

#include "uranite/descriptor/descriptor.hpp"
#include "uranite/semantic/qualnames.hpp"

namespace uranite::descriptor {
	
	static llvm::Type* getPointeeType( llvm::Value* value ) {
		if( auto* alloca = llvm::dyn_cast<llvm::AllocaInst>( value ) )
			return alloca->getAllocatedType();
		if( auto* gep = llvm::dyn_cast<llvm::GetElementPtrInst>( value ) )
			return gep->getResultElementType();
		if( auto* global = llvm::dyn_cast<llvm::GlobalVariable>( value ) )
			return global->getValueType();
		return llvm::PointerType::getUnqual( value->getContext() );
	}
	
	struct FormatPlaceholder {
		size_t startPos;
		size_t endPos;
		int argIndex;
		std::string name;
		char type;
		int width;
		int precision;
		char fill;
		char align;
	};
	
	static bool extractStringLiteral( llvm::Value* value, std::string& result ) {
		llvm::GlobalVariable* gv = nullptr;
		if( llvm::ConstantExpr* constExpr = llvm::dyn_cast<llvm::ConstantExpr>( value ) ) {
			if( constExpr->getOpcode() == llvm::Instruction::GetElementPtr ) {
				gv = llvm::dyn_cast<llvm::GlobalVariable>( constExpr->getOperand( 0 ) );
			}
		}
		else if( llvm::GetElementPtrInst* gep = llvm::dyn_cast<llvm::GetElementPtrInst>( value ) ) {
			gv = llvm::dyn_cast<llvm::GlobalVariable>( gep->getPointerOperand() );
		}
		else if( llvm::GlobalVariable* globalVar = llvm::dyn_cast<llvm::GlobalVariable>( value ) ) {
			gv = globalVar;
		}
		if( gv && gv->hasInitializer() ) {
			if( llvm::ConstantDataSequential* cds = llvm::dyn_cast<llvm::ConstantDataSequential>( gv->getInitializer() ) ) {
				result = cds->getAsString().str();
				if( result.empty() == false && result.back() == '\0' ) {
					result.pop_back();
				}
				return true;
			}
		}
		return false;
	}
	
	static std::vector<FormatPlaceholder> parseFormatString( const std::string& fmt ) {
		std::vector<FormatPlaceholder> placeholders;
		int autoIndex = 0;
		size_t i = 0;
		while( i < fmt.size() ) {
			if( fmt[i] == '{' ) {
				if( i + 1 < fmt.size() && fmt[i + 1] == '{' ) {
					i+= 2;
					continue;
				}
				size_t start = i;
				i++;
				FormatPlaceholder ph;
				ph.startPos = start;
				ph.argIndex = -1;
				ph.type = 0;
				ph.width = -1;
				ph.precision = -1;
				ph.fill = ' ';
				ph.align = 0;
				std::string content;
				while( i < fmt.size() && fmt[i] != '}' ) {
					content+= fmt[i];
					i++;
				}
				if( i < fmt.size() ) {
					ph.endPos = i;
					i++;
				}
				else {
					ph.endPos = fmt.size() - 1;
				}
				size_t colonPos = content.find( ':' );
				std::string indexPart = ( colonPos != std::string::npos ) ? content.substr( 0, colonPos ) : content;
				std::string specPart = ( colonPos != std::string::npos ) ? content.substr( colonPos + 1 ) : "";
				if( indexPart.empty() ) {
					ph.argIndex = autoIndex++;
				}
				else {
					bool allDigits = true;
					for( char c : indexPart ) {
						if( c < '0' || c > '9' ) {
							allDigits = false;
							break;
						}
					}
					if( allDigits ) {
						ph.argIndex = std::stoi( indexPart );
					}
					else {
						ph.name = indexPart;
						ph.argIndex = -1;
					}
				}
				if( specPart.empty() == false ) {
					size_t si = 0;
					if( specPart.size() >= 2 && ( specPart[1] == '<' || specPart[1] == '>' || specPart[1] == '^' ) ) {
						ph.fill = specPart[0];
						ph.align = specPart[1];
						si = 2;
					}
					else if( specPart.size() >= 1 && ( specPart[0] == '<' || specPart[0] == '>' || specPart[0] == '^' ) ) {
						ph.align = specPart[0];
						si = 1;
					}
					std::string widthStr;
					while( si < specPart.size() && specPart[si] >= '0' && specPart[si] <= '9' ) {
						widthStr+= specPart[si];
						si++;
					}
					if( widthStr.empty() == false ) {
						ph.width = std::stoi( widthStr );
					}
					if( si < specPart.size() && specPart[si] == '.' ) {
						si++;
						std::string precStr;
						while( si < specPart.size() && specPart[si] >= '0' && specPart[si] <= '9' ) {
							precStr+= specPart[si];
							si++;
						}
						if( precStr.empty() == false ) {
							ph.precision = std::stoi( precStr );
						}
					}
					if( si < specPart.size() ) {
						ph.type = specPart[si];
					}
				}
				placeholders.push_back( ph );
			}
			else if( fmt[i] == '}' && i + 1 < fmt.size() && fmt[i + 1] == '}' ) {
				i+= 2;
			}
			else {
				i++;
			}
		}
		return placeholders;
	}
	
	static std::string buildEscapedLiteral( const std::string& fmt, const std::vector<FormatPlaceholder>& placeholders ) {
		std::string result;
		size_t pos = 0;
		size_t phIdx = 0;
		while( pos < fmt.size() ) {
			if( phIdx < placeholders.size() && pos == placeholders[phIdx].startPos ) {
				result+= "%PLACEHOLDER%";
				pos = placeholders[phIdx].endPos + 1;
				phIdx++;
			}
			else if( fmt[pos] == '{' && pos + 1 < fmt.size() && fmt[pos + 1] == '{' ) {
				result+= '{';
				pos+= 2;
			}
			else if( fmt[pos] == '}' && pos + 1 < fmt.size() && fmt[pos + 1] == '}' ) {
				result+= '}';
				pos+= 2;
			}
			else if( fmt[pos] == '%' ) {
				result+= "%%";
				pos++;
			}
			else {
				result+= fmt[pos];
				pos++;
			}
		}
		return result;
	}
	
	static std::string buildSnprintfFormat( const std::string& fmt, const std::vector<FormatPlaceholder>& placeholders, std::vector<llvm::Value*>& arguments, std::vector<int>& argOrder ) {
		std::string escaped = buildEscapedLiteral( fmt, placeholders );
		std::string result;
		size_t pos = 0;
		size_t phIdx = 0;
		while( pos < escaped.size() ) {
			size_t marker = escaped.find( "%PLACEHOLDER%", pos );
			if( marker == std::string::npos ) {
				result+= escaped.substr( pos );
				break;
			}
			result+= escaped.substr( pos, marker - pos );
			if( phIdx < placeholders.size() ) {
				const FormatPlaceholder& ph = placeholders[phIdx];
				int idx = ph.argIndex;
				if( idx < 0 || idx >= static_cast<int>( arguments.size() ) ) {
					idx = 0;
				}
				if( arguments.empty() ) {
					result+= "%ld";
					argOrder.push_back( -1 );
					pos = marker + 13;
					phIdx++;
					continue;
				}
				argOrder.push_back( idx );
				llvm::Value* arg = arguments[idx];
				llvm::Type* argType = arg->getType();
				char typeChar = ph.type;
				if( typeChar == 0 ) {
					if( argType == llvm::PointerType::getUnqual( arg->getContext() ) ) {
						typeChar = 's';
					}
					else if( argType->isPointerTy() ) {
						typeChar = 'O';
					}
					else if( argType->isDoubleTy() || argType->isFloatTy() ) {
						typeChar = 'f';
					}
					else if( argType->isIntegerTy( 1 ) ) {
						typeChar = 'B';
					}
					else {
						typeChar = 'd';
					}
				}
				std::string spec = "%";
				if( ph.align == '<' ) {
					spec+= "-";
				}
				if( ph.fill == '0' && ph.align != '<' && ph.align != '^' ) {
					spec+= "0";
				}
				if( ph.width > 0 ) {
					spec+= std::to_string( ph.width );
				}
				if( ph.precision >= 0 ) {
					spec+= ".";
					spec+= std::to_string( ph.precision );
				}
				switch( typeChar ) {
					case 's':
						spec+= "s";
						break;
					case 'd':
						spec+= "ld";
						break;
					case 'x':
						spec+= "lx";
						break;
					case 'X':
						spec+= "lX";
						break;
					case 'o':
						spec+= "lo";
						break;
					case 'b':
						spec+= "BINARY";
						break;
					case 'f':
						if( ph.precision < 0 ) {
							spec = "%f";
							if( ph.width > 0 ) {
								spec = "%" + std::to_string( ph.width ) + "f";
							}
						}
						else {
							spec+= "f";
						}
						break;
					case 'e':
						spec+= "e";
						break;
					case 'B':
						spec+= "s";
						break;
					case 'O':
						spec+= "s";
						break;
					default:
						spec+= "ld";
						break;
				}
				result+= spec;
			}
			pos = marker + 13;
			phIdx++;
		}
		return result;
	}
	
	static llvm::Value* coerceToString( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* arg ) {
		llvm::Type* i8PtrType = llvm::PointerType::getUnqual( context );
		llvm::Type* i64Type = llvm::Type::getInt64Ty( context );
		llvm::Module* mod = builder.GetInsertBlock()->getParent()->getParent();
		if( arg->getType() == i8PtrType ) {
			return arg;
		}
		if( arg->getType()->isIntegerTy( 1 ) ) {
			llvm::Constant* trueStr = builder.CreateGlobalStringPtr( "True", "bool.true" );
			llvm::Constant* falseStr = builder.CreateGlobalStringPtr( "False", "bool.false" );
			return builder.CreateSelect( arg, trueStr, falseStr, "bool.str" );
		}
		if( arg->getType()->isPointerTy() ) {
			llvm::Type* pointee = getPointeeType( arg );
			if( pointee->isStructTy() ) {
				llvm::StructType* structType = llvm::cast<llvm::StructType>( pointee );
				if( structType->hasName() ) {
					std::string className = structType->getName().str();
					std::string toStringName = "_UR_" + className + "_toString";
					llvm::Function* toStringFn = mod->getFunction( toStringName );
					if( toStringFn && toStringFn->getReturnType() == i8PtrType ) {
						return builder.CreateCall( toStringFn, { arg }, "obj.str" );
					}
					std::string metaGlobalName = "_UR_meta_" + className;
					llvm::GlobalVariable* metaGlobal = mod->getGlobalVariable( metaGlobalName, true );
					std::string reprPrefix;
					if( metaGlobal && metaGlobal->hasInitializer() ) {
						if( llvm::ConstantDataArray* cda = llvm::dyn_cast<llvm::ConstantDataArray>( metaGlobal->getInitializer() ) ) {
							reprPrefix = cda->getAsCString().str();
						}
					}
					if( reprPrefix.empty() ) {
						reprPrefix = className + " object";
					}
					std::string reprFmt = "<" + reprPrefix + " at 0x%lx>";
					size_t reprBufSize = reprPrefix.size() + 32;
					llvm::FunctionCallee snprintfFn = mod->getOrInsertFunction( "snprintf", llvm::FunctionType::get( llvm::Type::getInt32Ty( context ), { i8PtrType, i64Type, i8PtrType }, true ) );
					llvm::FunctionCallee mallocFn = mod->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
					llvm::Value* buf = builder.CreateCall( mallocFn, { llvm::ConstantInt::get( i64Type, reprBufSize ) }, "repr.buf" );
					llvm::Value* ptrVal = builder.CreatePtrToInt( arg, i64Type, "ptr.int" );
					llvm::Constant* reprFmtStr = builder.CreateGlobalStringPtr( reprFmt, "repr.fmt" );
					builder.CreateCall( snprintfFn, { buf, llvm::ConstantInt::get( i64Type, reprBufSize ), reprFmtStr, ptrVal } );
					return buf;
				}
			}
			llvm::FunctionCallee snprintfFn = mod->getOrInsertFunction( "snprintf", llvm::FunctionType::get( llvm::Type::getInt32Ty( context ), { i8PtrType, i64Type, i8PtrType }, true ) );
			llvm::FunctionCallee mallocFn = mod->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
			llvm::Value* buf = builder.CreateCall( mallocFn, { llvm::ConstantInt::get( i64Type, 24 ) }, "ptr.buf" );
			llvm::Value* ptrVal = builder.CreatePtrToInt( arg, i64Type, "ptr.int" );
			llvm::Constant* ptrFmt = builder.CreateGlobalStringPtr( "0x%lx", "ptr.fmt" );
			builder.CreateCall( snprintfFn, { buf, llvm::ConstantInt::get( i64Type, 24 ), ptrFmt, ptrVal } );
			return buf;
		}
		if( arg->getType()->isDoubleTy() || arg->getType()->isFloatTy() ) {
			llvm::FunctionCallee snprintfFn = mod->getOrInsertFunction( "snprintf", llvm::FunctionType::get( llvm::Type::getInt32Ty( context ), { i8PtrType, i64Type, i8PtrType }, true ) );
			llvm::FunctionCallee mallocFn = mod->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
			llvm::Value* buf = builder.CreateCall( mallocFn, { llvm::ConstantInt::get( i64Type, 48 ) }, "flt.buf" );
			llvm::Value* val = arg;
			if( arg->getType()->isFloatTy() ) {
				val = builder.CreateFPExt( arg, llvm::Type::getDoubleTy( context ), "f2d" );
			}
			llvm::Constant* fltFmt = builder.CreateGlobalStringPtr( "%f", "flt.fmt" );
			builder.CreateCall( snprintfFn, { buf, llvm::ConstantInt::get( i64Type, 48 ), fltFmt, val } );
			return buf;
		}
		if( arg->getType()->isIntegerTy() ) {
			llvm::FunctionCallee snprintfFn = mod->getOrInsertFunction( "snprintf", llvm::FunctionType::get( llvm::Type::getInt32Ty( context ), { i8PtrType, i64Type, i8PtrType }, true ) );
			llvm::FunctionCallee mallocFn = mod->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
			llvm::Value* buf = builder.CreateCall( mallocFn, { llvm::ConstantInt::get( i64Type, 24 ) }, "int.buf" );
			llvm::Value* val = arg;
			if( arg->getType() != i64Type ) {
				val = builder.CreateSExt( arg, i64Type, "iext" );
			}
			llvm::Constant* intFmt = builder.CreateGlobalStringPtr( "%ld", "int.fmt" );
			builder.CreateCall( snprintfFn, { buf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, val } );
			return buf;
		}
		return builder.CreateGlobalStringPtr( "<unknown>", "unk.str" );
	}
	
	static size_t estimateBufferSize( const std::string& fmt, const std::vector<FormatPlaceholder>& placeholders ) {
		size_t base = fmt.size() + 1;
		for( const FormatPlaceholder& ph : placeholders ) {
			int extra = 32;
			if( ph.width > extra ) {
				extra = ph.width + 8;
			}
			if( ph.type == 's' ) {
				extra = 256;
			}
			base+= extra;
		}
		return base;
	}
	
	/**
	 * @brief Retrieves or inserts the declaration for the standard C 'malloc' function.
	 * 
	 * This helper function checks the provided LLVM module for the "malloc" function.
	 * If not found, it inserts a declaration with the signature:
	 * void* malloc(size_t size) -> (i8* (i64) in LLVM IR).
	 * 
	 * @param module The LLVM Module to search or modify.
	 * @param context The LLVM Context for type generation.
	 * @return llvm::FunctionCallee The function callee for 'malloc'.
	 */
	static llvm::FunctionCallee getMalloc( llvm::Module* module, llvm::LLVMContext& context ) {
		return module->getOrInsertFunction( "malloc", llvm::FunctionType::get(
			llvm::PointerType::getUnqual( context ),
			{
				llvm::Type::getInt64Ty( context )
			},
			false
		));
	}
	
	/**
	 * @brief Retrieves or inserts the declaration for the standard C 'snprintf' function.
	 * 
	 * This helper function checks the provided LLVM module for the "snprintf" function.
	 * If not found, it inserts a variadic declaration with the signature:
	 * int snprintf(char* str, size_t size, const char* format, ...) -> (i32 (i8*, i64, i8*, ...) in LLVM IR).
	 * 
	 * @param module The LLVM Module to search or modify.
	 * @param context The LLVM Context for type generation.
	 * @return llvm::FunctionCallee The function callee for 'snprintf'.
	 */
	static llvm::FunctionCallee getSnprintf( llvm::Module* module, llvm::LLVMContext& context ) {
		return module->getOrInsertFunction( "snprintf", llvm::FunctionType::get(
			llvm::Type::getInt32Ty( context ),
			{
				llvm::PointerType::getUnqual( context ),
				llvm::Type::getInt64Ty( context ),
				llvm::PointerType::getUnqual( context )
			},
			true
		));
	}
	
	const std::unordered_map<std::string, std::string> Builtin::oopToPrimitive = {
		{ semantic::qualname::classes::Double::Name,   "f64" },
		{ semantic::qualname::classes::f32::Name,      "f32" },
		{ semantic::qualname::classes::f64::Name,      "f64" },
		{ semantic::qualname::classes::Float::Name,    "f64" },
		{ semantic::qualname::classes::byte::Name,     "u8" },
		{ semantic::qualname::classes::i8::Name,       "i8" },
		{ semantic::qualname::classes::i16::Name,      "i16" },
		{ semantic::qualname::classes::i32::Name,      "i32" },
		{ semantic::qualname::classes::i64::Name,      "i64" },
		{ semantic::qualname::classes::Int::Name,      "i64" },
		{ semantic::qualname::classes::integer::Name,  "i32" },
		{ semantic::qualname::classes::Long::Name,     "i64" },
		{ semantic::qualname::classes::u8::Name,       "u8" },
		{ semantic::qualname::classes::u16::Name,      "u16" },
		{ semantic::qualname::classes::u32::Name,      "u32" },
		{ semantic::qualname::classes::u64::Name,      "u64" },
		{ semantic::qualname::classes::uint::Name,     "u64" },
		{ semantic::qualname::classes::boolean::Name,  semantic::qualname::primitives::names::Bool },
		{ semantic::qualname::classes::Char::Name,     semantic::qualname::primitives::names::Char },
		{ semantic::qualname::classes::nonetype::Name, semantic::qualname::primitives::names::Void },
		{ semantic::qualname::classes::string::Name,   semantic::qualname::primitives::names::Str },
		{ semantic::qualname::classes::Void::Name,     semantic::qualname::primitives::names::Void }
	};
	
	const std::unordered_map<std::string, std::string> Builtin::primitiveToOOP = {
		{ semantic::qualname::primitives::names::F32,  semantic::qualname::classes::f32::Name },
		{ semantic::qualname::primitives::names::F64,  semantic::qualname::classes::f64::Name },
		{ semantic::qualname::primitives::names::I8,   semantic::qualname::classes::i8::Name },
		{ semantic::qualname::primitives::names::I16,  semantic::qualname::classes::i16::Name },
		{ semantic::qualname::primitives::names::I32,  semantic::qualname::classes::i32::Name },
		{ semantic::qualname::primitives::names::I64,  semantic::qualname::classes::i64::Name },
		{ semantic::qualname::primitives::names::U8,   semantic::qualname::classes::u8::Name },
		{ semantic::qualname::primitives::names::U16,  semantic::qualname::classes::u16::Name },
		{ semantic::qualname::primitives::names::U32,  semantic::qualname::classes::u32::Name },
		{ semantic::qualname::primitives::names::U64,  semantic::qualname::classes::u64::Name },
		{ semantic::qualname::primitives::names::Bool, semantic::qualname::classes::boolean::Name },
		{ semantic::qualname::primitives::names::Char, semantic::qualname::classes::Char::Name },
		{ semantic::qualname::primitives::names::Str,  semantic::qualname::classes::string::Name },
		{ semantic::qualname::primitives::names::Void, semantic::qualname::classes::Void::Name }
	};
	
	const std::set<std::string> Builtin::oopWrapperNames = {
		semantic::qualname::classes::Int::Name, semantic::qualname::classes::i8::Name, semantic::qualname::classes::i16::Name, semantic::qualname::classes::i32::Name, semantic::qualname::classes::i64::Name,
		semantic::qualname::classes::uint::Name, semantic::qualname::classes::u8::Name, semantic::qualname::classes::u16::Name, semantic::qualname::classes::u32::Name, semantic::qualname::classes::u64::Name,
		semantic::qualname::classes::Float::Name, semantic::qualname::classes::f32::Name, semantic::qualname::classes::f64::Name, semantic::qualname::classes::Double::Name,
		semantic::qualname::classes::Long::Name, semantic::qualname::classes::integer::Name, semantic::qualname::classes::boolean::Name, semantic::qualname::classes::byte::Name,
		semantic::qualname::classes::Char::Name, semantic::qualname::classes::string::Name, semantic::qualname::classes::Void::Name, semantic::qualname::classes::object::Name
	};
	
	const std::set<std::string> Builtin::numericOopNames = {
		semantic::qualname::classes::Int::Name, semantic::qualname::classes::i8::Name, semantic::qualname::classes::i16::Name, semantic::qualname::classes::i32::Name, semantic::qualname::classes::i64::Name,
		semantic::qualname::classes::uint::Name, semantic::qualname::classes::u8::Name, semantic::qualname::classes::u16::Name, semantic::qualname::classes::u32::Name, semantic::qualname::classes::u64::Name,
		semantic::qualname::classes::Float::Name, semantic::qualname::classes::f32::Name, semantic::qualname::classes::f64::Name, semantic::qualname::classes::Double::Name,
		semantic::qualname::classes::integer::Name, semantic::qualname::classes::Long::Name, semantic::qualname::classes::byte::Name
	};
	
	const std::set<std::string> Builtin::floatOopNames = {
		semantic::qualname::classes::Float::Name, semantic::qualname::classes::f32::Name, semantic::qualname::classes::f64::Name, semantic::qualname::classes::Double::Name
	};
	
	const std::unordered_set<std::string> Builtin::unsignedOopNames = {
		semantic::qualname::classes::uint::Name, semantic::qualname::classes::u64::Name, semantic::qualname::classes::u32::Name, semantic::qualname::classes::u16::Name, semantic::qualname::classes::u8::Name, semantic::qualname::classes::byte::Name
	};
	
	const std::unordered_map<std::string, int> Builtin::integerBitWidths = {
		{semantic::qualname::classes::Int::Name, 64}, {semantic::qualname::classes::i64::Name, 64}, {semantic::qualname::classes::Long::Name, 64}, {semantic::qualname::classes::integer::Name, 64}, {semantic::qualname::classes::uint::Name, 64}, {semantic::qualname::classes::u64::Name, 64},
		{semantic::qualname::classes::i32::Name, 32}, {semantic::qualname::classes::u32::Name, 32}, {semantic::qualname::classes::i16::Name, 16}, {semantic::qualname::classes::u16::Name, 16},
		{semantic::qualname::classes::i8::Name, 8}, {semantic::qualname::classes::u8::Name, 8}, {semantic::qualname::classes::byte::Name, 8}, {semantic::qualname::classes::Char::Name, 32}
	};
	
	Builtin::Builtin() {
		this->registerAllTypes();
	}
	
	llvm::Type* Builtin::getLLVMType( const std::string& typeName, llvm::LLVMContext& context ) const {
		const BuiltinTypeDescriptor* descriptor = this->lookup( typeName );
		if( descriptor && descriptor->llvmTypeFactory ) {
			return descriptor->llvmTypeFactory( context );
		}
		return nullptr;
	}
	
	bool Builtin::isBuiltinType( const std::string& name ) {
		if( Builtin::oopToPrimitive.count( name ) > 0 ) {
			return true;
		}
		std::string::size_type lastDot = name.rfind( '.' );
		if( lastDot != std::string::npos ) {
			return Builtin::oopToPrimitive.count( name.substr( lastDot + 1 ) ) > 0;
		}
		return false;
	}
	
	bool Builtin::isPrimitiveName( const std::string& name ) {
		return Builtin::primitiveToOOP.count( name ) > 0;
	}
	
	const BuiltinTypeDescriptor* Builtin::lookup( const std::string& name ) const {
		std::unordered_map<std::string,BuiltinTypeDescriptor>::const_iterator typesIterator = this->types.find( name );
		if( typesIterator != this->types.end() ) {
			return &typesIterator->second;
		}
		std::string::size_type lastDot = name.rfind( '.' );
		if( lastDot != std::string::npos ) {
			typesIterator = this->types.find( name.substr( lastDot + 1 ) );
			if( typesIterator != this->types.end() ) {
				return &typesIterator->second;
			}
		}
		return nullptr;
	}
	
	const BuiltinMethodDescriptor* Builtin::lookupMethod( const std::string& typeName, const std::string& methodName ) const {
		const BuiltinTypeDescriptor* typeDescriptor = this->lookup( typeName );
		if( typeDescriptor ) {
			for( const BuiltinMethodDescriptor& methodDescriptor : typeDescriptor->methods ) {
				if( methodDescriptor.name == methodName ) {
					return &methodDescriptor;
				}
			}
		}
		if( typeDescriptor && typeDescriptor->parentName.empty() == false ) {
			return this->lookupMethod( typeDescriptor->parentName, methodName );
		}
		return nullptr;
	}
	
	const std::string& Builtin::oopNameForPrimitive( const std::string& primitiveName ) {
		static const std::string emptyString;
		std::unordered_map<std::string,std::string>::const_iterator mappingIterator = Builtin::primitiveToOOP.find( primitiveName );
		if( mappingIterator != Builtin::primitiveToOOP.end() ) {
			return mappingIterator->second;
		}
		return emptyString;
	}
	
	void Builtin::populateSemaTypes( semantic::Registry& registry ) const {
		for( std::unordered_map<std::string,BuiltinTypeDescriptor>::const_iterator typeIterator = this->types.begin(); typeIterator != this->types.end(); ++typeIterator ) {
			const std::string& typeName = typeIterator->first;
			const BuiltinTypeDescriptor& typeDescriptor = typeIterator->second;
			semantic::TypeSharedPointer existingType = registry.lookupType( typeName );
			if( existingType ) {
				continue;
			}
			semantic::ClassTypeSharedPointer classType = std::make_shared<semantic::ClassType>( typeName );
			classType->package = typeDescriptor.package;
			classType->qualified = typeDescriptor.package.empty() ? typeName : fmt::format( "{}.{}", typeDescriptor.package, typeName );
			classType->isFinal = typeDescriptor.isFinal;
			if( typeDescriptor.parentName.empty() == false ) {
				semantic::TypeSharedPointer parentType = registry.lookupType( typeDescriptor.parentName );
				if( parentType ) {
					classType->baseClass = parentType;
				}
			}
			for( const BuiltinMethodDescriptor& methodDescriptor : typeDescriptor.methods ) {
				semantic::MethodInfo methodInformation;
				methodInformation.name = methodDescriptor.name;
				methodInformation.access = ast::AccessModifier::Public;
				methodInformation.isVirtual = false;
				methodInformation.isOverride = false;
				methodInformation.isStatic = false;
				methodInformation.isFinal = false;
				methodInformation.virtualTableIndex = -1;
				std::vector<semantic::TypeSharedPointer> parameterTypes;
				for( const MethodParameter& parameterDescriptor : methodDescriptor.parameters ) {
					semantic::TypeSharedPointer parameterType = registry.lookupType( parameterDescriptor.typeName );
					if( parameterType ) {
						parameterTypes.push_back( parameterType );
					}
					else {
						parameterTypes.push_back( registry.getError() );
					}
				}
				semantic::TypeSharedPointer returnType = registry.lookupType( methodDescriptor.returnTypeName );
				if( returnType == nullptr ) {
					returnType = registry.getVoid();
				}
				methodInformation.type = registry.makeFunction( parameterTypes, returnType );
				classType->methods.push_back( methodInformation );
			}
			registry.registerType( typeName, classType );
		}
	}
	
	const std::string& Builtin::primitiveNameForOop( const std::string& oopName ) {
		static const std::string emptyString;
		std::unordered_map<std::string,std::string>::const_iterator mappingIterator = Builtin::oopToPrimitive.find( oopName );
		if( mappingIterator != Builtin::oopToPrimitive.end() ) {
			return mappingIterator->second;
		}
		std::string::size_type lastDot = oopName.rfind( '.' );
		if( lastDot != std::string::npos ) {
			mappingIterator = Builtin::oopToPrimitive.find( oopName.substr( lastDot + 1 ) );
			if( mappingIterator != Builtin::oopToPrimitive.end() ) {
				return mappingIterator->second;
			}
		}
		return emptyString;
	}
	
	void Builtin::registerAllTypes() {
		this->registerBooleanType();
		this->registerByteType();
		this->registerCharType();
		this->registerFloatTypes();
		this->registerIntTypes();
		this->registerSpecialTypes();
		this->registerStringType();
		this->registerUIntTypes();
	}
	
	void Builtin::registerBooleanType() {
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::boolean::Name,
			semantic::qualname::classes::boolean::Package,
			"",
			std::vector<BuiltinMethodDescriptor>({
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::GetValue,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>&, llvm::LLVMContext&, llvm::Value* self, std::vector<llvm::Value*>&, std::vector<std::pair<std::string, llvm::Value*>>& ) {
						return self;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::negatable::methods::Negate,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext&, llvm::Value* self, std::vector<llvm::Value*>&, std::vector<std::pair<std::string, llvm::Value*>>& ) {
						return builder.CreateNot( self, "bneg" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::LogicalAnd,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({
						MethodParameter {
							semantic::qualname::fields::Other,
							semantic::qualname::classes::boolean::Name
						}
					}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext&, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							return builder.CreateAnd( self, arguments[ 0 ], "bandtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::LogicalOr,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({
						MethodParameter {
							semantic::qualname::fields::Other,
							semantic::qualname::classes::boolean::Name
						}
					}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext&, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& ) -> llvm::Value* {
						return arguments.empty() ? nullptr : builder.CreateOr( self, arguments[ 0 ], "bortmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Hash,
					semantic::qualname::classes::i64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>&, std::vector<std::pair<std::string, llvm::Value*>>& ) {
						return builder.CreateZExt( self, llvm::Type::getInt64Ty( context ), "bhash" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToString,
					semantic::qualname::classes::string::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>&, std::vector<std::pair<std::string, llvm::Value*>>& ) {
						llvm::Constant* trueStringPointer = builder.CreateGlobalStringPtr( "True", "true.str" );
						llvm::Constant* falseStringPointer = builder.CreateGlobalStringPtr( "False", "false.str" );
						return builder.CreateSelect( self, trueStringPointer, falseStringPointer, "boolstr" );
					}
				}
			}),
			[]( llvm::LLVMContext& context ) {
				return llvm::Type::getInt1Ty( context );
			}
		});
	}
	
	void Builtin::registerByteType() {
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::byte::Name,
			semantic::qualname::classes::byte::Package,
			"",
			std::vector<BuiltinMethodDescriptor>({
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::GetValue,
					semantic::qualname::classes::u8::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return self;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToInt,
					semantic::qualname::classes::i32::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateZExt( self, llvm::Type::getInt32Ty( context ), "btoi" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::BitwiseAnd,
					semantic::qualname::classes::byte::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::byte::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return arguments.empty() ? nullptr : builder.CreateAnd( self, arguments[0], "bandtmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::BitwiseOr,
					semantic::qualname::classes::byte::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::byte::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return arguments.empty() ? nullptr : builder.CreateOr( self, arguments[0], "bortmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::BitwiseXor,
					semantic::qualname::classes::byte::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::byte::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return arguments.empty() ? nullptr : builder.CreateXor( self, arguments[0], "bxortmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ShiftLeft,
					semantic::qualname::classes::byte::Name,
					std::vector<MethodParameter>({ MethodParameter { "amount", semantic::qualname::classes::i32::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() ) return nullptr;
						llvm::Value* amount = builder.CreateTrunc( arguments[0], llvm::Type::getInt8Ty( context ), "shamt" );
						return builder.CreateShl( self, amount, "bshltmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ShiftRight,
					semantic::qualname::classes::byte::Name,
					std::vector<MethodParameter>({ MethodParameter { "amount", semantic::qualname::classes::i32::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() ) return nullptr;
						llvm::Value* amount = builder.CreateTrunc( arguments[0], llvm::Type::getInt8Ty( context ), "shamt" );
						return builder.CreateLShr( self, amount, "bshrtmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Hash,
					semantic::qualname::classes::i64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateZExt( self, llvm::Type::getInt64Ty( context ), "byhash" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToString,
					semantic::qualname::classes::string::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
						llvm::Value* buffer = builder.CreateCall( getMalloc( currentModule, context ), { llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 8 ) }, "buf" );
						llvm::Constant* formatString = builder.CreateGlobalStringPtr( "%u", "byte.fmt" );
						llvm::Value* byteValue = builder.CreateZExt( self, llvm::Type::getInt64Ty( context ), "bval" );
						builder.CreateCall( getSnprintf( currentModule, context ), { buffer, llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 8 ), formatString, byteValue } );
						return buffer;
					}
				}
			}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt8Ty( context );
			}
		});
	}
	
	void Builtin::registerCharType() {
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::Char::Name,
			semantic::qualname::classes::Char::Package,
			"",
			std::vector<BuiltinMethodDescriptor>({
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::GetValue,
					semantic::qualname::classes::Char::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return self;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::IsAlpha,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::IntegerType* int32Type = llvm::Type::getInt32Ty( context );
						llvm::Value* characterValue = builder.CreateIntCast( self, int32Type, true, "ch" );
						llvm::Value* isUpperCase = builder.CreateAnd(
							builder.CreateICmpSGE( characterValue, llvm::ConstantInt::get( int32Type, 'A' ), "geA" ),
							builder.CreateICmpSLE( characterValue, llvm::ConstantInt::get( int32Type, 'Z' ), "leZ" ), "upper" );
						llvm::Value* isLowerCase = builder.CreateAnd(
							builder.CreateICmpSGE( characterValue, llvm::ConstantInt::get( int32Type, 'a' ), "gea" ),
							builder.CreateICmpSLE( characterValue, llvm::ConstantInt::get( int32Type, 'z' ), "lez" ), "lower" );
						return builder.CreateOr( isUpperCase, isLowerCase, "isalpha" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::IsDigit,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::IntegerType* int32Type = llvm::Type::getInt32Ty( context );
						llvm::Value* characterValue = builder.CreateIntCast( self, int32Type, true, "ch" );
						return builder.CreateAnd(
							builder.CreateICmpSGE( characterValue, llvm::ConstantInt::get( int32Type, '0' ), "ge0" ),
							builder.CreateICmpSLE( characterValue, llvm::ConstantInt::get( int32Type, '9' ), "le9" ), "isdigit" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::IsAlphanumeric,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::IntegerType* int32Type = llvm::Type::getInt32Ty( context );
						llvm::Value* characterValue = builder.CreateIntCast( self, int32Type, true, "ch" );
						llvm::Value* isUpperCase = builder.CreateAnd(
							builder.CreateICmpSGE( characterValue, llvm::ConstantInt::get( int32Type, 'A' ), "geA" ),
							builder.CreateICmpSLE( characterValue, llvm::ConstantInt::get( int32Type, 'Z' ), "leZ" ), "upper" );
						llvm::Value* isLowerCase = builder.CreateAnd(
							builder.CreateICmpSGE( characterValue, llvm::ConstantInt::get( int32Type, 'a' ), "gea" ),
							builder.CreateICmpSLE( characterValue, llvm::ConstantInt::get( int32Type, 'z' ), "lez" ), "lower" );
						llvm::Value* isAlpha = builder.CreateOr( isUpperCase, isLowerCase, "isalpha" );
						llvm::Value* isDigit = builder.CreateAnd(
							builder.CreateICmpSGE( characterValue, llvm::ConstantInt::get( int32Type, '0' ), "ge0" ),
							builder.CreateICmpSLE( characterValue, llvm::ConstantInt::get( int32Type, '9' ), "le9" ), "isdigit" );
						return builder.CreateOr( isAlpha, isDigit, "isalnum" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::IsWhitespace,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::IntegerType* int32Type = llvm::Type::getInt32Ty( context );
						llvm::Value* characterValue = builder.CreateIntCast( self, int32Type, true, "ch" );
						llvm::Value* isSpace = builder.CreateICmpEQ( characterValue, llvm::ConstantInt::get( int32Type, ' ' ), "issp" );
						llvm::Value* isTab = builder.CreateICmpEQ( characterValue, llvm::ConstantInt::get( int32Type, '\t' ), "istab" );
						llvm::Value* isNewline = builder.CreateICmpEQ( characterValue, llvm::ConstantInt::get( int32Type, '\n' ), "isnl" );
						llvm::Value* isReturn = builder.CreateICmpEQ( characterValue, llvm::ConstantInt::get( int32Type, '\r' ), "iscr" );
						return builder.CreateOr( builder.CreateOr( isSpace, isTab ), builder.CreateOr( isNewline, isReturn ), "isws" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::string::methods::ToUpper,
					semantic::qualname::classes::Char::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::IntegerType* int32Type = llvm::Type::getInt32Ty( context );
						llvm::Value* characterValue = builder.CreateIntCast( self, int32Type, true, "ch" );
						llvm::Value* isLower = builder.CreateAnd(
							builder.CreateICmpSGE( characterValue, llvm::ConstantInt::get( int32Type, 'a' ), "gea" ),
							builder.CreateICmpSLE( characterValue, llvm::ConstantInt::get( int32Type, 'z' ), "lez" ), "islower" );
						llvm::Value* upperValue = builder.CreateSub( characterValue, llvm::ConstantInt::get( int32Type, 32 ), "toupper" );
						llvm::Value* result = builder.CreateSelect( isLower, upperValue, characterValue, "upper.sel" );
						return builder.CreateTrunc( result, self->getType(), "upper.trunc" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::string::methods::ToLower,
					semantic::qualname::classes::Char::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::IntegerType* int32Type = llvm::Type::getInt32Ty( context );
						llvm::Value* characterValue = builder.CreateIntCast( self, int32Type, true, "ch" );
						llvm::Value* isUpper = builder.CreateAnd(
							builder.CreateICmpSGE( characterValue, llvm::ConstantInt::get( int32Type, 'A' ), "geA" ),
							builder.CreateICmpSLE( characterValue, llvm::ConstantInt::get( int32Type, 'Z' ), "leZ" ), "isupper" );
						llvm::Value* lowerValue = builder.CreateAdd( characterValue, llvm::ConstantInt::get( int32Type, 32 ), "tolower" );
						llvm::Value* result = builder.CreateSelect( isUpper, lowerValue, characterValue, "lower.sel" );
						return builder.CreateTrunc( result, self->getType(), "lower.trunc" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Hash,
					semantic::qualname::classes::i64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateZExt( self, llvm::Type::getInt64Ty( context ), "chash" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToString,
					semantic::qualname::classes::string::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
						llvm::Value* buffer = builder.CreateCall( getMalloc( currentModule, context ), { llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 2 ) }, "buf" );
						llvm::Value* char8Value = builder.CreateIntCast( self, llvm::Type::getInt8Ty( context ), false, "ch8" );
						builder.CreateStore( char8Value, buffer );
						llvm::Value* nullPosition = builder.CreateGEP( llvm::Type::getInt8Ty( context ), buffer, llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 1 ), "nullpos" );
						builder.CreateStore( llvm::ConstantInt::get( llvm::Type::getInt8Ty( context ), 0 ), nullPosition );
						return buffer;
					}
				}
			}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt32Ty( context );
			}
		});
	}
	
	void Builtin::registerFloatTypes() {
		this->registerType( BuiltinTypeDescriptor {
			false,
			semantic::qualname::classes::Float::Name,
			semantic::qualname::classes::Float::Package,
			"",
			std::vector<BuiltinMethodDescriptor>({
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::GetValue,
					semantic::qualname::classes::f64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return self;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::addable::methods::Add,
					semantic::qualname::classes::Float::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::Float::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return arguments.empty() ? nullptr : builder.CreateFAdd( self, arguments[0], "faddtmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::subtractable::methods::Subtract,
					semantic::qualname::classes::Float::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::Float::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return arguments.empty() ? nullptr : builder.CreateFSub( self, arguments[0], "fsubtmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::multipliable::methods::Multiply,
					semantic::qualname::classes::Float::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::Float::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return arguments.empty() ? nullptr : builder.CreateFMul( self, arguments[0], "fmultmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::dividable::methods::Divide,
					semantic::qualname::classes::Float::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::Float::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return arguments.empty() ? nullptr : builder.CreateFDiv( self, arguments[0], "fdivtmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::negatable::methods::Negate,
					semantic::qualname::classes::Float::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateFNeg( self, "fnegtmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Abs,
					semantic::qualname::classes::Float::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::Value* negatedValue = builder.CreateFNeg( self, "fneg" );
						llvm::Value* isNegative = builder.CreateFCmpOLT( self, llvm::ConstantFP::get( self->getType(), 0.0 ), "isneg" );
						return builder.CreateSelect( isNegative, negatedValue, self, "fabstmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::equatable::methods::Equals,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::Float::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return arguments.empty() ? nullptr : builder.CreateFCmpOEQ( self, arguments[0], "feqtmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::CompareTo,
					semantic::qualname::classes::i32::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::Float::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::Type* i32Type = llvm::Type::getInt32Ty( context );
							llvm::Value* isLessThan = builder.CreateFCmpOLT( self, arguments[0], "flt" );
							llvm::Value* isGreaterThan = builder.CreateFCmpOGT( self, arguments[0], "fgt" );
							llvm::Value* greaterThanValue = builder.CreateSelect( isGreaterThan, llvm::ConstantInt::get( i32Type, 1 ), llvm::ConstantInt::get( i32Type, 0 ), "gtsel" );
							return builder.CreateSelect( isLessThan, llvm::ConstantInt::get( i32Type, -1, true ), greaterThanValue, "fcmptmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::IsNan,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateFCmpUNO( self, self, "isnan" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::IsInfinite,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::Value* selfSubSelf = builder.CreateFSub( self, self, "sub.self" );
						llvm::Value* subIsNaN = builder.CreateFCmpUNO( selfSubSelf, selfSubSelf, "sub.isnan" );
						llvm::Value* selfIsOrdered = builder.CreateFCmpORD( self, self, "self.ord" );
						return builder.CreateAnd( selfIsOrdered, subIsNaN, "isinf" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Floor,
					semantic::qualname::classes::Float::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::Type* selfType = self->getType();
						llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
						llvm::Function* floorFn = llvm::Intrinsic::getDeclaration( currentModule, llvm::Intrinsic::floor, { selfType } );
						return builder.CreateCall( floorFn, { self }, "floortmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Ceil,
					semantic::qualname::classes::Float::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::Type* selfType = self->getType();
						llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
						llvm::Function* ceilFn = llvm::Intrinsic::getDeclaration( currentModule, llvm::Intrinsic::ceil, { selfType } );
						return builder.CreateCall( ceilFn, { self }, "ceiltmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Round,
					semantic::qualname::classes::Float::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::Type* selfType = self->getType();
						llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
						llvm::Function* roundFn = llvm::Intrinsic::getDeclaration( currentModule, llvm::Intrinsic::round, { selfType } );
						return builder.CreateCall( roundFn, { self }, "roundtmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToDouble,
					semantic::qualname::classes::f64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateFPCast( self, llvm::Type::getDoubleTy( context ), "todouble" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToFloat,
					semantic::qualname::classes::Float::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return self;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToInt,
					semantic::qualname::classes::i64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateFPToSI( self, llvm::Type::getInt64Ty( context ), "ftoi" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Hash,
					semantic::qualname::classes::i64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateBitCast( self, llvm::Type::getInt64Ty( context ), "fhash" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToString,
					semantic::qualname::classes::string::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
						llvm::Value* buffer = builder.CreateCall( getMalloc( currentModule, context ), { llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 32 ) }, "buf" );
						llvm::Constant* formatString = builder.CreateGlobalStringPtr( "%g", "float.fmt" );
						llvm::Value* floatValue = builder.CreateFPCast( self, llvm::Type::getDoubleTy( context ), "fval" );
						builder.CreateCall( getSnprintf( currentModule, context ), { buffer, llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 32 ), formatString, floatValue } );
						return buffer;
					}
				}
			}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getDoubleTy( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor { true, semantic::qualname::classes::f32::Name, semantic::qualname::classes::f32::Package, semantic::qualname::classes::Float::Name, std::vector<BuiltinMethodDescriptor>({}), []( llvm::LLVMContext& context ) -> llvm::Type* { return llvm::Type::getFloatTy( context ); } });
		this->registerType( BuiltinTypeDescriptor { true, semantic::qualname::classes::f64::Name, semantic::qualname::classes::f64::Package, semantic::qualname::classes::Float::Name, std::vector<BuiltinMethodDescriptor>({}), []( llvm::LLVMContext& context ) -> llvm::Type* { return llvm::Type::getDoubleTy( context ); } });
		this->registerType( BuiltinTypeDescriptor { true, semantic::qualname::classes::Double::Name, semantic::qualname::classes::Double::Package, semantic::qualname::classes::Float::Name, std::vector<BuiltinMethodDescriptor>({}), []( llvm::LLVMContext& context ) -> llvm::Type* { return llvm::Type::getDoubleTy( context ); } });
	}
	
	void Builtin::registerIntTypes() {
		std::function<std::vector<BuiltinMethodDescriptor>(const std::string&)> makeIntArithmeticMethods = [this]( const std::string& typeName ) -> std::vector<BuiltinMethodDescriptor> {
			return std::vector<BuiltinMethodDescriptor>({
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::GetValue,
					semantic::qualname::classes::i64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return self;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::addable::methods::Add,
					typeName,
					std::vector<MethodParameter>({ MethodParameter { semantic::qualname::fields::Other, typeName } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::IntegerType* int64Type = llvm::Type::getInt64Ty( context );
							llvm::Value* leftHandSide = builder.CreateIntCast( self, int64Type, true, "lhs" );
							llvm::Value* rightHandSide = builder.CreateIntCast( arguments[0], int64Type, true, "rhs" );
							return builder.CreateAdd( leftHandSide, rightHandSide, "addtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::subtractable::methods::Subtract,
					typeName,
					std::vector<MethodParameter>({ MethodParameter { semantic::qualname::fields::Other, typeName } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::IntegerType* int64Type = llvm::Type::getInt64Ty( context );
							llvm::Value* leftHandSide = builder.CreateIntCast( self, int64Type, true, "lhs" );
							llvm::Value* rightHandSide = builder.CreateIntCast( arguments[0], int64Type, true, "rhs" );
							return builder.CreateSub( leftHandSide, rightHandSide, "subtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::multipliable::methods::Multiply,
					typeName,
					std::vector<MethodParameter>({ MethodParameter { semantic::qualname::fields::Other, typeName } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::IntegerType* int64Type = llvm::Type::getInt64Ty( context );
							llvm::Value* leftHandSide = builder.CreateIntCast( self, int64Type, true, "lhs" );
							llvm::Value* rightHandSide = builder.CreateIntCast( arguments[0], int64Type, true, "rhs" );
							return builder.CreateMul( leftHandSide, rightHandSide, "multmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::dividable::methods::Divide,
					typeName,
					std::vector<MethodParameter>({ MethodParameter { semantic::qualname::fields::Other, typeName } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::IntegerType* int64Type = llvm::Type::getInt64Ty( context );
							llvm::Value* leftHandSide = builder.CreateIntCast( self, int64Type, true, "lhs" );
							llvm::Value* rightHandSide = builder.CreateIntCast( arguments[0], int64Type, true, "rhs" );
							return builder.CreateSDiv( leftHandSide, rightHandSide, "divtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::modulable::methods::Modulo,
					typeName,
					std::vector<MethodParameter>({ MethodParameter { semantic::qualname::fields::Other, typeName } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::IntegerType* int64Type = llvm::Type::getInt64Ty( context );
							llvm::Value* leftHandSide = builder.CreateIntCast( self, int64Type, true, "lhs" );
							llvm::Value* rightHandSide = builder.CreateIntCast( arguments[0], int64Type, true, "rhs" );
							return builder.CreateSRem( leftHandSide, rightHandSide, "modtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::negatable::methods::Negate,
					typeName,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::Value* castedValue = builder.CreateIntCast( self, llvm::Type::getInt64Ty( context ), true, "val" );
						return builder.CreateNeg( castedValue, "negtmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Abs,
					typeName,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::IntegerType* int64Type = llvm::Type::getInt64Ty( context );
						llvm::Value* castedValue = builder.CreateIntCast( self, int64Type, true, "val" );
						llvm::Value* negatedValue = builder.CreateNeg( castedValue, "neg" );
						llvm::Value* isNegative = builder.CreateICmpSLT( castedValue, llvm::ConstantInt::get( int64Type, 0 ), "isneg" );
						return builder.CreateSelect( isNegative, negatedValue, castedValue, "abstmp" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::equatable::methods::Equals,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({ MethodParameter { semantic::qualname::fields::Other, typeName } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::IntegerType* int64Type = llvm::Type::getInt64Ty( context );
							llvm::Value* leftHandSide = builder.CreateIntCast( self, int64Type, true, "lhs" );
							llvm::Value* rightHandSide = builder.CreateIntCast( arguments[0], int64Type, true, "rhs" );
							return builder.CreateICmpEQ( leftHandSide, rightHandSide, "eqtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::CompareTo,
					semantic::qualname::classes::i32::Name,
					std::vector<MethodParameter>({ MethodParameter { semantic::qualname::fields::Other, typeName } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::IntegerType* int64Type = llvm::Type::getInt64Ty( context );
							llvm::IntegerType* int32Type = llvm::Type::getInt32Ty( context );
							llvm::Value* leftHandSide = builder.CreateIntCast( self, int64Type, true, "lhs" );
							llvm::Value* rightHandSide = builder.CreateIntCast( arguments[0], int64Type, true, "rhs" );
							llvm::Value* isLessThan = builder.CreateICmpSLT( leftHandSide, rightHandSide, "lt" );
							llvm::Value* isGreaterThan = builder.CreateICmpSGT( leftHandSide, rightHandSide, "gt" );
							llvm::Value* greaterThanResult = builder.CreateSelect( isGreaterThan, llvm::ConstantInt::get( int32Type, 1 ), llvm::ConstantInt::get( int32Type, 0 ), "gtsel" );
							return builder.CreateSelect( isLessThan, llvm::ConstantInt::get( int32Type, -1, true ), greaterThanResult, "cmptmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Min,
					typeName,
					std::vector<MethodParameter>({ MethodParameter { semantic::qualname::fields::Other, typeName } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::IntegerType* int64Type = llvm::Type::getInt64Ty( context );
							llvm::Value* leftHandSide = builder.CreateIntCast( self, int64Type, true, "lhs" );
							llvm::Value* rightHandSide = builder.CreateIntCast( arguments[0], int64Type, true, "rhs" );
							return builder.CreateSelect( builder.CreateICmpSLT( leftHandSide, rightHandSide, "cmp" ), leftHandSide, rightHandSide, "mintmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Max,
					typeName,
					std::vector<MethodParameter>({ MethodParameter { semantic::qualname::fields::Other, typeName } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::IntegerType* int64Type = llvm::Type::getInt64Ty( context );
							llvm::Value* leftHandSide = builder.CreateIntCast( self, int64Type, true, "lhs" );
							llvm::Value* rightHandSide = builder.CreateIntCast( arguments[0], int64Type, true, "rhs" );
							return builder.CreateSelect( builder.CreateICmpSGT( leftHandSide, rightHandSide, "cmp" ), leftHandSide, rightHandSide, "maxtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Hash,
					semantic::qualname::classes::i64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateIntCast( self, llvm::Type::getInt64Ty( context ), true, "hash" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToString,
					semantic::qualname::classes::string::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
						llvm::Value* buffer = builder.CreateCall( getMalloc( currentModule, context ), { llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 24 ) }, "buf" );
						llvm::Constant* formatString = builder.CreateGlobalStringPtr( "%ld", "int.fmt" );
						llvm::Value* integerValue = builder.CreateIntCast( self, llvm::Type::getInt64Ty( context ), true, "ival" );
						builder.CreateCall( getSnprintf( currentModule, context ), { buffer, llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 24 ), formatString, integerValue } );
						return buffer;
					}
				}
			});
		};
		this->registerType( BuiltinTypeDescriptor {
			false,
			semantic::qualname::classes::Int::Name,
			semantic::qualname::classes::Int::Package,
			"",
			makeIntArithmeticMethods( semantic::qualname::classes::Int::Name ),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt64Ty( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::i8::Name,
			semantic::qualname::classes::i8::Package,
			semantic::qualname::classes::Int::Name,
			std::vector<BuiltinMethodDescriptor>({}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt8Ty( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::i16::Name,
			semantic::qualname::classes::i16::Package,
			semantic::qualname::classes::Int::Name,
			std::vector<BuiltinMethodDescriptor>({}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt16Ty( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::i32::Name,
			semantic::qualname::classes::i32::Package,
			semantic::qualname::classes::Int::Name,
			std::vector<BuiltinMethodDescriptor>({}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt32Ty( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::i64::Name,
			semantic::qualname::classes::i64::Package,
			semantic::qualname::classes::Int::Name,
			std::vector<BuiltinMethodDescriptor>({}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt64Ty( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::integer::Name,
			semantic::qualname::classes::integer::Package,
			semantic::qualname::classes::Int::Name,
			std::vector<BuiltinMethodDescriptor>({}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt32Ty( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::Long::Name,
			semantic::qualname::classes::Long::Package,
			semantic::qualname::classes::Int::Name,
			std::vector<BuiltinMethodDescriptor>({}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt64Ty( context );
			}
		});
	}
	
	void Builtin::registerSpecialTypes() {
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::nonetype::Name,
			semantic::qualname::classes::nonetype::Package,
			"",
			std::vector<BuiltinMethodDescriptor>({
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToString,
					semantic::qualname::classes::string::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateGlobalStringPtr( "None", "none.str" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::equatable::methods::Equals,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return llvm::ConstantInt::getTrue( context );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Hash,
					semantic::qualname::classes::i64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 0 );
					}
				}
			}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt1Ty( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::Void::Name,
			semantic::qualname::classes::Void::Package,
			"",
			std::vector<BuiltinMethodDescriptor>({
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToString,
					semantic::qualname::classes::string::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateGlobalStringPtr( "void", "void.str" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Hash,
					semantic::qualname::classes::i64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 0 );
					}
				}
			}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getVoidTy( context );
			}
		});
	}
	
	void Builtin::registerStringType() {
		std::vector<BuiltinMethodDescriptor> methods;
		methods.reserve( 8 );
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::object::methods::GetValue,
			semantic::qualname::classes::string::Name,
			std::vector<MethodParameter>({}),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				return self;
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::object::methods::ToString,
			semantic::qualname::classes::string::Name,
			std::vector<MethodParameter>({}),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				return self;
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::Length,
			semantic::qualname::classes::i64::Name,
			std::vector<MethodParameter>({}),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				llvm::FunctionType* strlenType = llvm::FunctionType::get( llvm::Type::getInt64Ty( context ), { llvm::PointerType::getUnqual( context ) }, false );
				llvm::FunctionCallee strlenFunction = builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction( "strlen", strlenType );
				return builder.CreateCall( strlenFunction, { self }, "strlen" );
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::IsEmpty,
			semantic::qualname::classes::boolean::Name,
			std::vector<MethodParameter>({}),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				llvm::FunctionType* strlenType = llvm::FunctionType::get( llvm::Type::getInt64Ty( context ), { llvm::PointerType::getUnqual( context ) }, false );
				llvm::FunctionCallee strlenFunction = builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction( "strlen", strlenType );
				llvm::Value* stringLength = builder.CreateCall( strlenFunction, { self }, "strlen" );
				return builder.CreateICmpEQ( stringLength, llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 0 ), "isempty" );
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::interfaces::equatable::methods::Equals,
			semantic::qualname::classes::boolean::Name,
			std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::string::Name } }),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				if( arguments.empty() == false ) {
					llvm::FunctionType* strcmpType = llvm::FunctionType::get( llvm::Type::getInt32Ty( context ), { llvm::PointerType::getUnqual( context ), llvm::PointerType::getUnqual( context ) }, false );
					llvm::FunctionCallee strcmpFunction = builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction( "strcmp", strcmpType );
					llvm::Value* comparisonResult = builder.CreateCall( strcmpFunction, { self, arguments[0] }, "strcmp" );
					return builder.CreateICmpEQ( comparisonResult, llvm::ConstantInt::get( llvm::Type::getInt32Ty( context ), 0 ), "streq" );
				}
				return nullptr;
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::Concat,
			semantic::qualname::classes::string::Name,
			std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::string::Name } }),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				if( arguments.empty() == false ) {
					llvm::Type* i8PtrType = llvm::PointerType::getUnqual( context );
					llvm::Type* i64Type = llvm::Type::getInt64Ty( context );
					llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
					llvm::FunctionType* strlenType = llvm::FunctionType::get( i64Type, { i8PtrType }, false );
					llvm::FunctionCallee strlenFunction = currentModule->getOrInsertFunction( "strlen", strlenType );
					llvm::Value* length1 = builder.CreateCall( strlenFunction, { self }, "len1" );
					llvm::Value* length2 = builder.CreateCall( strlenFunction, { arguments[0] }, "len2" );
					llvm::Value* totalLength = builder.CreateAdd( builder.CreateAdd( length1, length2, "total" ), llvm::ConstantInt::get( i64Type, 1 ), "total1" );
					llvm::FunctionType* mallocType = llvm::FunctionType::get( i8PtrType, { i64Type }, false );
					llvm::FunctionCallee mallocFunction = currentModule->getOrInsertFunction( "malloc", mallocType );
					llvm::Value* buffer = builder.CreateCall( mallocFunction, { totalLength }, "buf" );
					llvm::FunctionType* strcpyType = llvm::FunctionType::get( i8PtrType, { i8PtrType, i8PtrType }, false );
					llvm::FunctionCallee strcpyFunction = currentModule->getOrInsertFunction( "strcpy", strcpyType );
					builder.CreateCall( strcpyFunction, { buffer, self } );
					llvm::FunctionType* strcatType = llvm::FunctionType::get( i8PtrType, { i8PtrType, i8PtrType }, false );
					llvm::FunctionCallee strcatFunction = currentModule->getOrInsertFunction( "strcat", strcatType );
					builder.CreateCall( strcatFunction, { buffer, arguments[0] } );
					return buffer;
				}
				return nullptr;
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::StartsWith,
			semantic::qualname::classes::boolean::Name,
			std::vector<MethodParameter>({ MethodParameter { "prefix", semantic::qualname::classes::string::Name } }),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				if( arguments.empty() ) {
					return nullptr;
				}
				llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
				llvm::Type* i8PtrType = llvm::PointerType::getUnqual( context );
				llvm::Type* i64Type = llvm::Type::getInt64Ty( context );
				llvm::Type* i32Type = llvm::Type::getInt32Ty( context );
				llvm::FunctionCallee strlenFunction = currentModule->getOrInsertFunction( "strlen", llvm::FunctionType::get( i64Type, { i8PtrType }, false ) );
				llvm::FunctionCallee strncmpFunction = currentModule->getOrInsertFunction( "strncmp", llvm::FunctionType::get( i32Type, { i8PtrType, i8PtrType, i64Type }, false ) );
				llvm::Value* prefixLength = builder.CreateCall( strlenFunction, { arguments[0] }, "prefix.len" );
				llvm::Value* comparisonResult = builder.CreateCall( strncmpFunction, { self, arguments[0], prefixLength }, "strncmp" );
				return builder.CreateICmpEQ( comparisonResult, llvm::ConstantInt::get( i32Type, 0 ), "startswith" );
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::EndsWith,
			semantic::qualname::classes::boolean::Name,
			std::vector<MethodParameter>({ MethodParameter { "suffix", semantic::qualname::classes::string::Name } }),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				if( arguments.empty() ) {
					return nullptr;
				}
				llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
				llvm::Type* i8PtrType = llvm::PointerType::getUnqual( context );
				llvm::Type* i8Type = llvm::Type::getInt8Ty( context );
				llvm::Type* i64Type = llvm::Type::getInt64Ty( context );
				llvm::Type* i32Type = llvm::Type::getInt32Ty( context );
				llvm::FunctionCallee strlenFunction = currentModule->getOrInsertFunction( "strlen", llvm::FunctionType::get( i64Type, { i8PtrType }, false ) );
				llvm::FunctionCallee strcmpFunction = currentModule->getOrInsertFunction( "strcmp", llvm::FunctionType::get( i32Type, { i8PtrType, i8PtrType }, false ) );
				llvm::Value* selfLength = builder.CreateCall( strlenFunction, { self }, "self.len" );
				llvm::Value* suffixLength = builder.CreateCall( strlenFunction, { arguments[0] }, "suff.len" );
				llvm::Value* longEnough = builder.CreateICmpUGE( selfLength, suffixLength, "long.enough" );
				llvm::Value* offset = builder.CreateSub( selfLength, suffixLength, "offset" );
				llvm::Value* safeOffset = builder.CreateSelect( longEnough, offset, llvm::ConstantInt::get( i64Type, 0 ), "safe.offset" );
				llvm::Value* tailPointer = builder.CreateGEP( i8Type, self, safeOffset, "tail.ptr" );
				llvm::Value* comparisonResult = builder.CreateCall( strcmpFunction, { tailPointer, arguments[0] }, "strcmp" );
				llvm::Value* matches = builder.CreateICmpEQ( comparisonResult, llvm::ConstantInt::get( i32Type, 0 ), "matches" );
				return builder.CreateAnd( longEnough, matches, "endswith" );
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::Contains,
			semantic::qualname::classes::boolean::Name,
			std::vector<MethodParameter>({ MethodParameter { "substr", semantic::qualname::classes::string::Name } }),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				if( arguments.empty() ) {
					return nullptr;
				}
				llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
				llvm::Type* i8PtrType = llvm::PointerType::getUnqual( context );
				llvm::FunctionCallee strstrFunction = currentModule->getOrInsertFunction( "strstr", llvm::FunctionType::get( i8PtrType, { i8PtrType, i8PtrType }, false ) );
				llvm::Value* found = builder.CreateCall( strstrFunction, { self, arguments[0] }, "strstr" );
				return builder.CreateICmpNE( found, llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( i8PtrType ) ), "contains" );
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::ToUpper,
			semantic::qualname::classes::string::Name,
			std::vector<MethodParameter>({}),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
				llvm::Type* i8PtrType = llvm::PointerType::getUnqual( context );
				llvm::Type* i8Type = llvm::Type::getInt8Ty( context );
				llvm::Type* i64Type = llvm::Type::getInt64Ty( context );
				llvm::Function* helperFunction = currentModule->getFunction( "_uranite_str_toUpper" );
				if( helperFunction == nullptr ) {
					llvm::FunctionCallee strlenCallee = currentModule->getOrInsertFunction( "strlen", llvm::FunctionType::get( i64Type, { i8PtrType }, false ) );
					llvm::FunctionCallee mallocCallee = currentModule->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
					llvm::FunctionType* helperType = llvm::FunctionType::get( i8PtrType, { i8PtrType }, false );
					helperFunction = llvm::Function::Create( helperType, llvm::Function::InternalLinkage, "_uranite_str_toUpper", currentModule );
					llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create( context, "entry", helperFunction );
					llvm::BasicBlock* loopBlock = llvm::BasicBlock::Create( context, "loop", helperFunction );
					llvm::BasicBlock* bodyBlock = llvm::BasicBlock::Create( context, "body", helperFunction );
					llvm::BasicBlock* doneBlock = llvm::BasicBlock::Create( context, "done", helperFunction );
					llvm::IRBuilder<> tmpBuilder( context );
					tmpBuilder.SetInsertPoint( entryBlock );
					llvm::Value* str = helperFunction->getArg( 0 );
					llvm::Value* len = tmpBuilder.CreateCall( strlenCallee, { str }, "len" );
					llvm::Value* len1 = tmpBuilder.CreateAdd( len, llvm::ConstantInt::get( i64Type, 1 ), "len1" );
					llvm::Value* buf = tmpBuilder.CreateCall( mallocCallee, { len1 }, "buf" );
					tmpBuilder.CreateBr( loopBlock );
					tmpBuilder.SetInsertPoint( loopBlock );
					llvm::PHINode* idx = tmpBuilder.CreatePHI( i64Type, 2, "i" );
					idx->addIncoming( llvm::ConstantInt::get( i64Type, 0 ), entryBlock );
					llvm::Value* cond = tmpBuilder.CreateICmpULT( idx, len, "cond" );
					tmpBuilder.CreateCondBr( cond, bodyBlock, doneBlock );
					tmpBuilder.SetInsertPoint( bodyBlock );
					llvm::Value* srcPtr = tmpBuilder.CreateGEP( i8Type, str, idx, "src.ptr" );
					llvm::Value* ch = tmpBuilder.CreateLoad( i8Type, srcPtr, "ch" );
					llvm::Value* isGEa = tmpBuilder.CreateICmpUGE( ch, llvm::ConstantInt::get( i8Type, 'a' ), "ge.a" );
					llvm::Value* isLEz = tmpBuilder.CreateICmpULE( ch, llvm::ConstantInt::get( i8Type, 'z' ), "le.z" );
					llvm::Value* isLower = tmpBuilder.CreateAnd( isGEa, isLEz, "is.lower" );
					llvm::Value* upper = tmpBuilder.CreateSub( ch, llvm::ConstantInt::get( i8Type, 32 ), "upper" );
					llvm::Value* result = tmpBuilder.CreateSelect( isLower, upper, ch, "result" );
					llvm::Value* dstPtr = tmpBuilder.CreateGEP( i8Type, buf, idx, "dst.ptr" );
					tmpBuilder.CreateStore( result, dstPtr );
					llvm::Value* next = tmpBuilder.CreateAdd( idx, llvm::ConstantInt::get( i64Type, 1 ), "next" );
					idx->addIncoming( next, bodyBlock );
					tmpBuilder.CreateBr( loopBlock );
					tmpBuilder.SetInsertPoint( doneBlock );
					llvm::Value* nullPtr = tmpBuilder.CreateGEP( i8Type, buf, len, "null.ptr" );
					tmpBuilder.CreateStore( llvm::ConstantInt::get( i8Type, 0 ), nullPtr );
					tmpBuilder.CreateRet( buf );
				}
				return builder.CreateCall( helperFunction, { self }, "toupper.result" );
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::ToLower,
			semantic::qualname::classes::string::Name,
			std::vector<MethodParameter>({}),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
				llvm::Type* i8PtrType = llvm::PointerType::getUnqual( context );
				llvm::Type* i8Type = llvm::Type::getInt8Ty( context );
				llvm::Type* i64Type = llvm::Type::getInt64Ty( context );
				llvm::Function* helperFunction = currentModule->getFunction( "_uranite_str_toLower" );
				if( helperFunction == nullptr ) {
					llvm::FunctionCallee strlenCallee = currentModule->getOrInsertFunction( "strlen", llvm::FunctionType::get( i64Type, { i8PtrType }, false ) );
					llvm::FunctionCallee mallocCallee = currentModule->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
					llvm::FunctionType* helperType = llvm::FunctionType::get( i8PtrType, { i8PtrType }, false );
					helperFunction = llvm::Function::Create( helperType, llvm::Function::InternalLinkage, "_uranite_str_toLower", currentModule );
					llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create( context, "entry", helperFunction );
					llvm::BasicBlock* loopBlock = llvm::BasicBlock::Create( context, "loop", helperFunction );
					llvm::BasicBlock* bodyBlock = llvm::BasicBlock::Create( context, "body", helperFunction );
					llvm::BasicBlock* doneBlock = llvm::BasicBlock::Create( context, "done", helperFunction );
					llvm::IRBuilder<> tmpBuilder( context );
					tmpBuilder.SetInsertPoint( entryBlock );
					llvm::Value* str = helperFunction->getArg( 0 );
					llvm::Value* len = tmpBuilder.CreateCall( strlenCallee, { str }, "len" );
					llvm::Value* len1 = tmpBuilder.CreateAdd( len, llvm::ConstantInt::get( i64Type, 1 ), "len1" );
					llvm::Value* buf = tmpBuilder.CreateCall( mallocCallee, { len1 }, "buf" );
					tmpBuilder.CreateBr( loopBlock );
					tmpBuilder.SetInsertPoint( loopBlock );
					llvm::PHINode* idx = tmpBuilder.CreatePHI( i64Type, 2, "i" );
					idx->addIncoming( llvm::ConstantInt::get( i64Type, 0 ), entryBlock );
					llvm::Value* cond = tmpBuilder.CreateICmpULT( idx, len, "cond" );
					tmpBuilder.CreateCondBr( cond, bodyBlock, doneBlock );
					tmpBuilder.SetInsertPoint( bodyBlock );
					llvm::Value* srcPtr = tmpBuilder.CreateGEP( i8Type, str, idx, "src.ptr" );
					llvm::Value* ch = tmpBuilder.CreateLoad( i8Type, srcPtr, "ch" );
					llvm::Value* isGEA = tmpBuilder.CreateICmpUGE( ch, llvm::ConstantInt::get( i8Type, 'A' ), "ge.A" );
					llvm::Value* isLEZ = tmpBuilder.CreateICmpULE( ch, llvm::ConstantInt::get( i8Type, 'Z' ), "le.Z" );
					llvm::Value* isUpperChar = tmpBuilder.CreateAnd( isGEA, isLEZ, "is.upper" );
					llvm::Value* lower = tmpBuilder.CreateAdd( ch, llvm::ConstantInt::get( i8Type, 32 ), "lower" );
					llvm::Value* result = tmpBuilder.CreateSelect( isUpperChar, lower, ch, "result" );
					llvm::Value* dstPtr = tmpBuilder.CreateGEP( i8Type, buf, idx, "dst.ptr" );
					tmpBuilder.CreateStore( result, dstPtr );
					llvm::Value* next = tmpBuilder.CreateAdd( idx, llvm::ConstantInt::get( i64Type, 1 ), "next" );
					idx->addIncoming( next, bodyBlock );
					tmpBuilder.CreateBr( loopBlock );
					tmpBuilder.SetInsertPoint( doneBlock );
					llvm::Value* nullPtr = tmpBuilder.CreateGEP( i8Type, buf, len, "null.ptr" );
					tmpBuilder.CreateStore( llvm::ConstantInt::get( i8Type, 0 ), nullPtr );
					tmpBuilder.CreateRet( buf );
				}
				return builder.CreateCall( helperFunction, { self }, "tolower.result" );
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::Trim,
			semantic::qualname::classes::string::Name,
			std::vector<MethodParameter>({}),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
				llvm::Type* i8PtrType = llvm::PointerType::getUnqual( context );
				llvm::Type* i8Type = llvm::Type::getInt8Ty( context );
				llvm::Type* i64Type = llvm::Type::getInt64Ty( context );
				llvm::Function* helperFunction = currentModule->getFunction( "_uranite_str_trim" );
				if( helperFunction == nullptr ) {
					llvm::FunctionCallee strlenCallee = currentModule->getOrInsertFunction( "strlen", llvm::FunctionType::get( i64Type, { i8PtrType }, false ) );
					llvm::FunctionCallee mallocCallee = currentModule->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
					llvm::FunctionCallee memcpyCallee = currentModule->getOrInsertFunction( "memcpy", llvm::FunctionType::get( i8PtrType, { i8PtrType, i8PtrType, i64Type }, false ) );
					llvm::FunctionType* helperType = llvm::FunctionType::get( i8PtrType, { i8PtrType }, false );
					helperFunction = llvm::Function::Create( helperType, llvm::Function::InternalLinkage, "_uranite_str_trim", currentModule );
					llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create( context, "entry", helperFunction );
					llvm::BasicBlock* startLoopBlock = llvm::BasicBlock::Create( context, "start.loop", helperFunction );
					llvm::BasicBlock* startBodyBlock = llvm::BasicBlock::Create( context, "start.body", helperFunction );
					llvm::BasicBlock* endSetupBlock = llvm::BasicBlock::Create( context, "end.setup", helperFunction );
					llvm::BasicBlock* endLoopBlock = llvm::BasicBlock::Create( context, "end.loop", helperFunction );
					llvm::BasicBlock* endBodyBlock = llvm::BasicBlock::Create( context, "end.body", helperFunction );
					llvm::BasicBlock* copyBlock = llvm::BasicBlock::Create( context, "copy", helperFunction );
					llvm::IRBuilder<> tmpBuilder( context );
					tmpBuilder.SetInsertPoint( entryBlock );
					llvm::Value* str = helperFunction->getArg( 0 );
					llvm::Value* len = tmpBuilder.CreateCall( strlenCallee, { str }, "len" );
					tmpBuilder.CreateBr( startLoopBlock );
					tmpBuilder.SetInsertPoint( startLoopBlock );
					llvm::PHINode* startIdx = tmpBuilder.CreatePHI( i64Type, 2, "start" );
					startIdx->addIncoming( llvm::ConstantInt::get( i64Type, 0 ), entryBlock );
					llvm::Value* startCond = tmpBuilder.CreateICmpULT( startIdx, len, "start.cond" );
					tmpBuilder.CreateCondBr( startCond, startBodyBlock, endSetupBlock );
					tmpBuilder.SetInsertPoint( startBodyBlock );
					llvm::Value* startPtr = tmpBuilder.CreateGEP( i8Type, str, startIdx, "start.ptr" );
					llvm::Value* startCh = tmpBuilder.CreateLoad( i8Type, startPtr, "start.ch" );
					llvm::Value* isSpace = tmpBuilder.CreateICmpEQ( startCh, llvm::ConstantInt::get( i8Type, ' ' ), "is.space" );
					llvm::Value* isTab = tmpBuilder.CreateICmpEQ( startCh, llvm::ConstantInt::get( i8Type, '\t' ), "is.tab" );
					llvm::Value* isNewline = tmpBuilder.CreateICmpEQ( startCh, llvm::ConstantInt::get( i8Type, '\n' ), "is.nl" );
					llvm::Value* isReturn = tmpBuilder.CreateICmpEQ( startCh, llvm::ConstantInt::get( i8Type, '\r' ), "is.cr" );
					llvm::Value* isWhitespace = tmpBuilder.CreateOr( tmpBuilder.CreateOr( isSpace, isTab ), tmpBuilder.CreateOr( isNewline, isReturn ) );
					llvm::Value* nextStart = tmpBuilder.CreateAdd( startIdx, llvm::ConstantInt::get( i64Type, 1 ), "next.start" );
					startIdx->addIncoming( nextStart, startBodyBlock );
					tmpBuilder.CreateCondBr( isWhitespace, startLoopBlock, endSetupBlock );
					tmpBuilder.SetInsertPoint( endSetupBlock );
					llvm::PHINode* trimStart = tmpBuilder.CreatePHI( i64Type, 2, "trim.start" );
					trimStart->addIncoming( startIdx, startLoopBlock );
					trimStart->addIncoming( startIdx, startBodyBlock );
					tmpBuilder.CreateBr( endLoopBlock );
					tmpBuilder.SetInsertPoint( endLoopBlock );
					llvm::PHINode* endIdx = tmpBuilder.CreatePHI( i64Type, 2, "end" );
					endIdx->addIncoming( len, endSetupBlock );
					llvm::Value* endCond = tmpBuilder.CreateICmpUGT( endIdx, trimStart, "end.cond" );
					tmpBuilder.CreateCondBr( endCond, endBodyBlock, copyBlock );
					tmpBuilder.SetInsertPoint( endBodyBlock );
					llvm::Value* prevEnd = tmpBuilder.CreateSub( endIdx, llvm::ConstantInt::get( i64Type, 1 ), "prev.end" );
					llvm::Value* endPtr = tmpBuilder.CreateGEP( i8Type, str, prevEnd, "end.ptr" );
					llvm::Value* endCh = tmpBuilder.CreateLoad( i8Type, endPtr, "end.ch" );
					llvm::Value* endIsSpace = tmpBuilder.CreateICmpEQ( endCh, llvm::ConstantInt::get( i8Type, ' ' ), "e.space" );
					llvm::Value* endIsTab = tmpBuilder.CreateICmpEQ( endCh, llvm::ConstantInt::get( i8Type, '\t' ), "e.tab" );
					llvm::Value* endIsNewline = tmpBuilder.CreateICmpEQ( endCh, llvm::ConstantInt::get( i8Type, '\n' ), "e.nl" );
					llvm::Value* endIsReturn = tmpBuilder.CreateICmpEQ( endCh, llvm::ConstantInt::get( i8Type, '\r' ), "e.cr" );
					llvm::Value* endIsWhitespace = tmpBuilder.CreateOr( tmpBuilder.CreateOr( endIsSpace, endIsTab ), tmpBuilder.CreateOr( endIsNewline, endIsReturn ) );
					endIdx->addIncoming( prevEnd, endBodyBlock );
					tmpBuilder.CreateCondBr( endIsWhitespace, endLoopBlock, copyBlock );
					tmpBuilder.SetInsertPoint( copyBlock );
					llvm::PHINode* trimEnd = tmpBuilder.CreatePHI( i64Type, 2, "trim.end" );
					trimEnd->addIncoming( endIdx, endLoopBlock );
					trimEnd->addIncoming( endIdx, endBodyBlock );
					llvm::Value* trimLen = tmpBuilder.CreateSub( trimEnd, trimStart, "trim.len" );
					llvm::Value* trimLen1 = tmpBuilder.CreateAdd( trimLen, llvm::ConstantInt::get( i64Type, 1 ), "trim.len1" );
					llvm::Value* buf = tmpBuilder.CreateCall( mallocCallee, { trimLen1 }, "buf" );
					llvm::Value* srcStart = tmpBuilder.CreateGEP( i8Type, str, trimStart, "src.start" );
					tmpBuilder.CreateCall( memcpyCallee, { buf, srcStart, trimLen } );
					llvm::Value* nullPtr = tmpBuilder.CreateGEP( i8Type, buf, trimLen, "null.ptr" );
					tmpBuilder.CreateStore( llvm::ConstantInt::get( i8Type, 0 ), nullPtr );
					tmpBuilder.CreateRet( buf );
				}
				return builder.CreateCall( helperFunction, { self }, "trim.result" );
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::Replace,
			semantic::qualname::classes::string::Name,
			std::vector<MethodParameter>({ MethodParameter { "old", semantic::qualname::classes::string::Name }, MethodParameter { "replacement", semantic::qualname::classes::string::Name } }),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				if( arguments.size() < 2 ) {
					return nullptr;
				}
				llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
				llvm::Type* i8PtrType = llvm::PointerType::getUnqual( context );
				llvm::Type* i8Type = llvm::Type::getInt8Ty( context );
				llvm::Type* i64Type = llvm::Type::getInt64Ty( context );
				llvm::Function* helperFunction = currentModule->getFunction( "_uranite_str_replace" );
				if( helperFunction == nullptr ) {
					llvm::FunctionCallee strlenCallee = currentModule->getOrInsertFunction( "strlen", llvm::FunctionType::get( i64Type, { i8PtrType }, false ) );
					llvm::FunctionCallee mallocCallee = currentModule->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
					llvm::FunctionCallee strstrCallee = currentModule->getOrInsertFunction( "strstr", llvm::FunctionType::get( i8PtrType, { i8PtrType, i8PtrType }, false ) );
					llvm::FunctionCallee memcpyCallee = currentModule->getOrInsertFunction( "memcpy", llvm::FunctionType::get( i8PtrType, { i8PtrType, i8PtrType, i64Type }, false ) );
					llvm::FunctionType* helperType = llvm::FunctionType::get( i8PtrType, { i8PtrType, i8PtrType, i8PtrType }, false );
					helperFunction = llvm::Function::Create( helperType, llvm::Function::InternalLinkage, "_uranite_str_replace", currentModule );
					llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create( context, "entry", helperFunction );
					llvm::BasicBlock* countLoopBlock = llvm::BasicBlock::Create( context, "count.loop", helperFunction );
					llvm::BasicBlock* countFoundBlock = llvm::BasicBlock::Create( context, "count.found", helperFunction );
					llvm::BasicBlock* allocBlock = llvm::BasicBlock::Create( context, "alloc", helperFunction );
					llvm::BasicBlock* buildLoopBlock = llvm::BasicBlock::Create( context, "build.loop", helperFunction );
					llvm::BasicBlock* buildFoundBlock = llvm::BasicBlock::Create( context, "build.found", helperFunction );
					llvm::BasicBlock* buildDoneBlock = llvm::BasicBlock::Create( context, "build.done", helperFunction );
					llvm::IRBuilder<> tmpBuilder( context );
					tmpBuilder.SetInsertPoint( entryBlock );
					llvm::Value* str = helperFunction->getArg( 0 );
					llvm::Value* oldStr = helperFunction->getArg( 1 );
					llvm::Value* newStr = helperFunction->getArg( 2 );
					llvm::Value* strLen = tmpBuilder.CreateCall( strlenCallee, { str }, "str.len" );
					llvm::Value* oldLen = tmpBuilder.CreateCall( strlenCallee, { oldStr }, "old.len" );
					llvm::Value* newLen = tmpBuilder.CreateCall( strlenCallee, { newStr }, "new.len" );
					tmpBuilder.CreateBr( countLoopBlock );
					tmpBuilder.SetInsertPoint( countLoopBlock );
					llvm::PHINode* countCur = tmpBuilder.CreatePHI( i8PtrType, 2, "count.cur" );
					countCur->addIncoming( str, entryBlock );
					llvm::PHINode* count = tmpBuilder.CreatePHI( i64Type, 2, "count" );
					count->addIncoming( llvm::ConstantInt::get( i64Type, 0 ), entryBlock );
					llvm::Value* found = tmpBuilder.CreateCall( strstrCallee, { countCur, oldStr }, "found" );
					llvm::Value* isFound = tmpBuilder.CreateICmpNE( found, llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( i8PtrType ) ), "is.found" );
					tmpBuilder.CreateCondBr( isFound, countFoundBlock, allocBlock );
					tmpBuilder.SetInsertPoint( countFoundBlock );
					llvm::Value* nextCount = tmpBuilder.CreateAdd( count, llvm::ConstantInt::get( i64Type, 1 ), "next.count" );
					llvm::Value* afterMatch = tmpBuilder.CreateGEP( i8Type, found, oldLen, "after.match" );
					countCur->addIncoming( afterMatch, countFoundBlock );
					count->addIncoming( nextCount, countFoundBlock );
					tmpBuilder.CreateBr( countLoopBlock );
					tmpBuilder.SetInsertPoint( allocBlock );
					llvm::Value* diff = tmpBuilder.CreateSub( newLen, oldLen, "diff" );
					llvm::Value* delta = tmpBuilder.CreateMul( count, diff, "delta" );
					llvm::Value* resultLen = tmpBuilder.CreateAdd( strLen, delta, "result.len" );
					llvm::Value* resultLen1 = tmpBuilder.CreateAdd( resultLen, llvm::ConstantInt::get( i64Type, 1 ), "result.len1" );
					llvm::Value* buf = tmpBuilder.CreateCall( mallocCallee, { resultLen1 }, "buf" );
					tmpBuilder.CreateBr( buildLoopBlock );
					tmpBuilder.SetInsertPoint( buildLoopBlock );
					llvm::PHINode* buildSrc = tmpBuilder.CreatePHI( i8PtrType, 2, "build.src" );
					buildSrc->addIncoming( str, allocBlock );
					llvm::PHINode* buildDst = tmpBuilder.CreatePHI( i8PtrType, 2, "build.dst" );
					buildDst->addIncoming( buf, allocBlock );
					llvm::Value* buildFound = tmpBuilder.CreateCall( strstrCallee, { buildSrc, oldStr }, "build.found" );
					llvm::Value* buildIsFound = tmpBuilder.CreateICmpNE( buildFound, llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( i8PtrType ) ), "build.is.found" );
					tmpBuilder.CreateCondBr( buildIsFound, buildFoundBlock, buildDoneBlock );
					tmpBuilder.SetInsertPoint( buildFoundBlock );
					llvm::Value* chunkLen = tmpBuilder.CreatePtrDiff( i8Type, buildFound, buildSrc );
					llvm::Value* chunkLenI64 = tmpBuilder.CreateIntCast( chunkLen, i64Type, false, "chunk.len" );
					tmpBuilder.CreateCall( memcpyCallee, { buildDst, buildSrc, chunkLenI64 } );
					llvm::Value* dstAfterChunk = tmpBuilder.CreateGEP( i8Type, buildDst, chunkLenI64, "dst.after.chunk" );
					tmpBuilder.CreateCall( memcpyCallee, { dstAfterChunk, newStr, newLen } );
					llvm::Value* dstAfterReplace = tmpBuilder.CreateGEP( i8Type, dstAfterChunk, newLen, "dst.after.replace" );
					llvm::Value* srcAfterMatch = tmpBuilder.CreateGEP( i8Type, buildFound, oldLen, "src.after.match" );
					buildSrc->addIncoming( srcAfterMatch, buildFoundBlock );
					buildDst->addIncoming( dstAfterReplace, buildFoundBlock );
					tmpBuilder.CreateBr( buildLoopBlock );
					tmpBuilder.SetInsertPoint( buildDoneBlock );
					llvm::Value* remainLen = tmpBuilder.CreateCall( strlenCallee, { buildSrc }, "remain.len" );
					llvm::Value* remainLen1 = tmpBuilder.CreateAdd( remainLen, llvm::ConstantInt::get( i64Type, 1 ), "remain.len1" );
					tmpBuilder.CreateCall( memcpyCallee, { buildDst, buildSrc, remainLen1 } );
					tmpBuilder.CreateRet( buf );
				}
				return builder.CreateCall( helperFunction, { self, arguments[0], arguments[1] }, "replace.result" );
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::Split,
			semantic::qualname::classes::string::Name,
			std::vector<MethodParameter>({ MethodParameter { semantic::qualname::fields::Delimiter, semantic::qualname::classes::string::Name } }),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				return self;
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::object::methods::Hash,
			semantic::qualname::classes::i64::Name,
			std::vector<MethodParameter>({}),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				return builder.CreatePtrToInt( self, llvm::Type::getInt64Ty( context ), "strhash" );
			}
		});
		methods.push_back( BuiltinMethodDescriptor {
			semantic::qualname::classes::string::methods::Format,
			semantic::qualname::classes::string::Name,
			std::vector<MethodParameter>({}),
			[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
				llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
				llvm::Type* i8PtrType = llvm::PointerType::getUnqual( context );
				llvm::Type* i64Type = llvm::Type::getInt64Ty( context );
				llvm::FunctionCallee mallocFn = currentModule->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
				llvm::FunctionCallee snprintfFn = currentModule->getOrInsertFunction( "snprintf", llvm::FunctionType::get( llvm::Type::getInt32Ty( context ), { i8PtrType, i64Type, i8PtrType }, true ) );
				llvm::FunctionCallee strlenFn = currentModule->getOrInsertFunction( "strlen", llvm::FunctionType::get( i64Type, { i8PtrType }, false ) );
				llvm::FunctionCallee memcpyFn = currentModule->getOrInsertFunction( "memcpy", llvm::FunctionType::get( i8PtrType, { i8PtrType, i8PtrType, i64Type }, false ) );
				std::string fmtStr;
				if( extractStringLiteral( self, fmtStr ) ) {
					std::vector<FormatPlaceholder> placeholders = parseFormatString( fmtStr );
					if( placeholders.empty() ) {
						if( fmtStr.find( "{{" ) != std::string::npos || fmtStr.find( "}}" ) != std::string::npos ) {
							std::string unescaped;
							for( size_t ei = 0; ei < fmtStr.size(); ei++ ) {
								if( fmtStr[ei] == '{' && ei + 1 < fmtStr.size() && fmtStr[ei + 1] == '{' ) {
									unescaped+= '{';
									ei++;
								}
								else if( fmtStr[ei] == '}' && ei + 1 < fmtStr.size() && fmtStr[ei + 1] == '}' ) {
									unescaped+= '}';
									ei++;
								}
								else {
									unescaped+= fmtStr[ei];
								}
							}
							return builder.CreateGlobalStringPtr( unescaped, "fmt.unesc" );
						}
						return self;
					}
					bool hasBinary = false;
					bool hasCenterAlign = false;
					bool hasNamedPlaceholder = false;
					for( const FormatPlaceholder& ph : placeholders ) {
						if( ph.align == '^' ) {
							hasCenterAlign = true;
						}
						if( ph.name.empty() == false ) {
							hasNamedPlaceholder = true;
						}
						if( ph.type == 'b' ) {
							hasBinary = true;
						}
					}
					if( hasBinary == false && hasCenterAlign == false && hasNamedPlaceholder == false ) {
						std::vector<int> argOrder;
						std::string snprintfFmt = buildSnprintfFormat( fmtStr, placeholders, arguments, argOrder );
						size_t bufSize = estimateBufferSize( fmtStr, placeholders );
						for( size_t ai = 0; ai < argOrder.size(); ai++ ) {
							int idx = argOrder[ai];
							if( idx >= 0 && idx < static_cast<int>( arguments.size() ) ) {
								if( arguments[idx]->getType() == i8PtrType ) {
									llvm::Value* argLen = builder.CreateCall( strlenFn, { arguments[idx] }, "arg.len" );
									llvm::Value* argLenVal = builder.CreateAdd( argLen, llvm::ConstantInt::get( i64Type, 1 ) );
									bufSize+= 256;
									(void)argLenVal;
								}
							}
						}
						llvm::Value* bufSizeVal = llvm::ConstantInt::get( i64Type, bufSize );
						bool hasStringArgs = false;
						for( int idx : argOrder ) {
							if( idx >= 0 && idx < static_cast<int>( arguments.size() ) ) {
								llvm::Type* at = arguments[idx]->getType();
								if( at == i8PtrType || at->isPointerTy() ) {
									hasStringArgs = true;
									break;
								}
							}
						}
						if( hasStringArgs ) {
							llvm::Value* totalSize = llvm::ConstantInt::get( i64Type, fmtStr.size() + 64 );
							for( int idx : argOrder ) {
								if( idx >= 0 && idx < static_cast<int>( arguments.size() ) ) {
									if( arguments[idx]->getType() == i8PtrType ) {
										llvm::Value* argLen = builder.CreateCall( strlenFn, { arguments[idx] }, "sarg.len" );
										totalSize = builder.CreateAdd( totalSize, argLen, "accum.len" );
									}
									else {
										totalSize = builder.CreateAdd( totalSize, llvm::ConstantInt::get( i64Type, 32 ), "accum.len" );
									}
								}
							}
							totalSize = builder.CreateAdd( totalSize, llvm::ConstantInt::get( i64Type, 1 ), "total.len" );
							bufSizeVal = totalSize;
						}
						llvm::Value* buffer = builder.CreateCall( mallocFn, { bufSizeVal }, "fmt.buf" );
						llvm::Constant* fmtGlobal = builder.CreateGlobalStringPtr( snprintfFmt, "fmt.str" );
						std::vector<llvm::Value*> snprintfArgs;
						snprintfArgs.push_back( buffer );
						snprintfArgs.push_back( bufSizeVal );
						snprintfArgs.push_back( fmtGlobal );
						for( int idx : argOrder ) {
							if( idx >= 0 && idx < static_cast<int>( arguments.size() ) ) {
								llvm::Value* arg = arguments[idx];
								if( arg->getType()->isPointerTy() && getPointeeType( arg )->isStructTy() ) {
									arg = coerceToString( builder, context, arg );
								}
								else if( arg->getType()->isIntegerTy( 1 ) ) {
									llvm::Constant* trueStr = builder.CreateGlobalStringPtr( "True", "bool.true" );
									llvm::Constant* falseStr = builder.CreateGlobalStringPtr( "False", "bool.false" );
									arg = builder.CreateSelect( arg, trueStr, falseStr, "bool.str" );
								}
								else if( arg->getType()->isFloatTy() ) {
									arg = builder.CreateFPExt( arg, llvm::Type::getDoubleTy( context ), "f2d" );
								}
								else if( arg->getType()->isIntegerTy() && arg->getType() != i64Type && arg->getType()->isIntegerTy( 1 ) == false ) {
									arg = builder.CreateSExt( arg, i64Type, "iext" );
								}
								snprintfArgs.push_back( arg );
							}
						}
						builder.CreateCall( snprintfFn, snprintfArgs );
						return buffer;
					}
					std::vector<llvm::Value*> pieces;
					std::vector<llvm::Value*> pieceLens;
					size_t pos = 0;
					size_t phIdx = 0;
					size_t autoIdx = 0;
					while( pos < fmtStr.size() ) {
						if( phIdx < placeholders.size() && pos == placeholders[phIdx].startPos ) {
							const FormatPlaceholder& ph = placeholders[phIdx];
							llvm::Value* arg = nullptr;
							if( ph.name.empty() == false ) {
								for( std::pair<std::string, llvm::Value*>& kw : keywordArguments ) {
									if( kw.first == ph.name ) {
										arg = kw.second;
										break;
									}
								}
								if( arg == nullptr && arguments.empty() == false ) {
									arg = arguments[0];
								}
							}
							else {
								int idx = ph.argIndex;
								if( idx < 0 ) {
									idx = static_cast<int>( autoIdx++ );
								}
								if( idx >= 0 && idx < static_cast<int>( arguments.size() ) ) {
									arg = arguments[idx];
								}
							}
							if( arg != nullptr ) {
								llvm::Value* piece = nullptr;
								llvm::Value* pieceLen = nullptr;
								if( ph.type == 'b' || ( ph.type == 0 && false ) ) {
									llvm::Function* parentFn = builder.GetInsertBlock()->getParent();
									llvm::Value* val = arg;
									if( val->getType() != i64Type ) {
										val = builder.CreateSExt( val, i64Type, "bin.ext" );
									}
									llvm::Value* tmpBuf = builder.CreateCall( mallocFn, { llvm::ConstantInt::get( i64Type, 65 ) }, "bin.buf" );
									llvm::Value* idxVal = llvm::ConstantInt::get( i64Type, 63 );
									llvm::AllocaInst* idxAlloca = builder.CreateAlloca( i64Type, nullptr, "bin.idx" );
									builder.CreateStore( idxVal, idxAlloca );
									llvm::AllocaInst* valAlloca = builder.CreateAlloca( i64Type, nullptr, "bin.val" );
									builder.CreateStore( val, valAlloca );
									llvm::BasicBlock* zeroBB = llvm::BasicBlock::Create( context, "bin.zero", parentFn );
									llvm::BasicBlock* loopBB = llvm::BasicBlock::Create( context, "bin.loop", parentFn );
									llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create( context, "bin.body", parentFn );
									llvm::BasicBlock* doneBB = llvm::BasicBlock::Create( context, "bin.done", parentFn );
									llvm::Value* isZero = builder.CreateICmpEQ( val, llvm::ConstantInt::get( i64Type, 0 ), "is.zero" );
									builder.CreateCondBr( isZero, zeroBB, loopBB );
									builder.SetInsertPoint( zeroBB );
									llvm::Value* zeroChar = llvm::ConstantInt::get( llvm::Type::getInt8Ty( context ), '0' );
									builder.CreateStore( zeroChar, builder.CreateGEP( llvm::Type::getInt8Ty( context ), tmpBuf, llvm::ConstantInt::get( i64Type, 0 ) ) );
									builder.CreateStore( llvm::ConstantInt::get( llvm::Type::getInt8Ty( context ), 0 ), builder.CreateGEP( llvm::Type::getInt8Ty( context ), tmpBuf, llvm::ConstantInt::get( i64Type, 1 ) ) );
									builder.CreateBr( doneBB );
									builder.SetInsertPoint( loopBB );
									llvm::Value* curVal = builder.CreateLoad( i64Type, valAlloca, "cur.val" );
									llvm::Value* loopCond = builder.CreateICmpNE( curVal, llvm::ConstantInt::get( i64Type, 0 ), "loop.cond" );
									builder.CreateCondBr( loopCond, bodyBB, doneBB );
									builder.SetInsertPoint( bodyBB );
									llvm::Value* curVal2 = builder.CreateLoad( i64Type, valAlloca, "cur.val2" );
									llvm::Value* bit = builder.CreateAnd( curVal2, llvm::ConstantInt::get( i64Type, 1 ), "bit" );
									llvm::Value* digitChar = builder.CreateAdd( builder.CreateTrunc( bit, llvm::Type::getInt8Ty( context ) ), llvm::ConstantInt::get( llvm::Type::getInt8Ty( context ), '0' ), "digit" );
									llvm::Value* curIdx = builder.CreateLoad( i64Type, idxAlloca, "cur.idx" );
									llvm::Value* gep = builder.CreateGEP( llvm::Type::getInt8Ty( context ), tmpBuf, curIdx, "bin.gep" );
									builder.CreateStore( digitChar, gep );
									llvm::Value* nextIdx = builder.CreateSub( curIdx, llvm::ConstantInt::get( i64Type, 1 ), "next.idx" );
									builder.CreateStore( nextIdx, idxAlloca );
									llvm::Value* shifted = builder.CreateLShr( curVal2, llvm::ConstantInt::get( i64Type, 1 ), "shifted" );
									builder.CreateStore( shifted, valAlloca );
									builder.CreateBr( loopBB );
									builder.SetInsertPoint( doneBB );
									llvm::Value* finalIdx = builder.CreateLoad( i64Type, idxAlloca, "final.idx" );
									llvm::Value* startPos = builder.CreateAdd( finalIdx, llvm::ConstantInt::get( i64Type, 1 ), "start.pos" );
									llvm::Value* binLen = builder.CreateSub( llvm::ConstantInt::get( i64Type, 64 ), startPos, "bin.len" );
									llvm::Value* binStart = builder.CreateGEP( llvm::Type::getInt8Ty( context ), tmpBuf, startPos, "bin.start" );
									llvm::Value* wasBinZero = builder.CreateICmpEQ( val, llvm::ConstantInt::get( i64Type, 0 ), "was.zero" );
									piece = builder.CreateSelect( wasBinZero, tmpBuf, binStart, "bin.piece" );
									pieceLen = builder.CreateSelect( wasBinZero, llvm::ConstantInt::get( i64Type, 1 ), binLen, "bin.plen" );
								}
								else if( arg->getType()->isPointerTy() && getPointeeType( arg )->isStructTy() ) {
									piece = coerceToString( builder, context, arg );
									pieceLen = builder.CreateCall( strlenFn, { piece }, "obj.len" );
								}
								else if( arg->getType()->isPointerTy() ) {
									piece = arg;
									pieceLen = builder.CreateCall( strlenFn, { arg }, "s.len" );
								}
								else if( arg->getType()->isIntegerTy( 1 ) ) {
									llvm::Constant* trueStr = builder.CreateGlobalStringPtr( "True", "bool.t" );
									llvm::Constant* falseStr = builder.CreateGlobalStringPtr( "False", "bool.f" );
									piece = builder.CreateSelect( arg, trueStr, falseStr, "bool.sel" );
									llvm::Value* trueLen = llvm::ConstantInt::get( i64Type, 4 );
									llvm::Value* falseLen = llvm::ConstantInt::get( i64Type, 5 );
									pieceLen = builder.CreateSelect( arg, trueLen, falseLen, "bool.len" );
								}
								else {
									llvm::Value* numBuf = builder.CreateCall( mallocFn, { llvm::ConstantInt::get( i64Type, 48 ) }, "num.buf" );
									std::string numFmt;
									llvm::Value* numArg = arg;
									if( arg->getType()->isDoubleTy() || arg->getType()->isFloatTy() ) {
										if( arg->getType()->isFloatTy() ) {
											numArg = builder.CreateFPExt( arg, llvm::Type::getDoubleTy( context ), "f2d" );
										}
										if( ph.precision >= 0 ) {
											numFmt = "%." + std::to_string( ph.precision ) + "f";
										}
										else {
											numFmt = "%f";
										}
									}
									else {
										if( arg->getType() != i64Type ) {
											numArg = builder.CreateSExt( arg, i64Type, "iext" );
										}
										switch( ph.type ) {
											case 'x': numFmt = "%lx"; break;
											case 'X': numFmt = "%lX"; break;
											case 'o': numFmt = "%lo"; break;
											default: numFmt = "%ld"; break;
										}
									}
									llvm::Constant* numFmtGlobal = builder.CreateGlobalStringPtr( numFmt, "num.fmt" );
									builder.CreateCall( snprintfFn, { numBuf, llvm::ConstantInt::get( i64Type, 48 ), numFmtGlobal, numArg } );
									piece = numBuf;
									pieceLen = builder.CreateCall( strlenFn, { numBuf }, "num.len" );
								}
								if( ph.width > 0 ) {
									llvm::Value* widthVal = llvm::ConstantInt::get( i64Type, ph.width );
									llvm::Value* needsPad = builder.CreateICmpSLT( pieceLen, widthVal, "needs.pad" );
									llvm::Function* parentFn = builder.GetInsertBlock()->getParent();
									llvm::BasicBlock* padBB = llvm::BasicBlock::Create( context, "pad", parentFn );
									llvm::BasicBlock* noPadBB = llvm::BasicBlock::Create( context, "nopad", parentFn );
									llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create( context, "pad.merge", parentFn );
									builder.CreateCondBr( needsPad, padBB, noPadBB );
									builder.SetInsertPoint( padBB );
									llvm::Value* padBuf = builder.CreateCall( mallocFn, { builder.CreateAdd( widthVal, llvm::ConstantInt::get( i64Type, 1 ) ) }, "pad.buf" );
									llvm::FunctionCallee memsetFn = currentModule->getOrInsertFunction( "memset", llvm::FunctionType::get( i8PtrType, { i8PtrType, llvm::Type::getInt32Ty( context ), i64Type }, false ) );
									builder.CreateCall( memsetFn, { padBuf, llvm::ConstantInt::get( llvm::Type::getInt32Ty( context ), ph.fill ), widthVal } );
									llvm::Value* padAmount = builder.CreateSub( widthVal, pieceLen, "pad.amt" );
									llvm::Value* destPtr = nullptr;
									if( ph.align == '^' ) {
										llvm::Value* leftPad = builder.CreateUDiv( padAmount, llvm::ConstantInt::get( i64Type, 2 ), "lpad" );
										destPtr = builder.CreateGEP( llvm::Type::getInt8Ty( context ), padBuf, leftPad, "ctr.dest" );
									}
									else if( ph.align == '<' ) {
										destPtr = padBuf;
									}
									else {
										destPtr = builder.CreateGEP( llvm::Type::getInt8Ty( context ), padBuf, padAmount, "rpad.dest" );
									}
									builder.CreateCall( memcpyFn, { destPtr, piece, pieceLen } );
									builder.CreateStore( llvm::ConstantInt::get( llvm::Type::getInt8Ty( context ), 0 ), builder.CreateGEP( llvm::Type::getInt8Ty( context ), padBuf, widthVal ) );
									builder.CreateBr( mergeBB );
									builder.SetInsertPoint( noPadBB );
									builder.CreateBr( mergeBB );
									builder.SetInsertPoint( mergeBB );
									llvm::PHINode* mergedPiece = builder.CreatePHI( i8PtrType, 2, "merged.piece" );
									mergedPiece->addIncoming( padBuf, padBB );
									mergedPiece->addIncoming( piece, noPadBB );
									llvm::PHINode* mergedLen = builder.CreatePHI( i64Type, 2, "merged.len" );
									mergedLen->addIncoming( widthVal, padBB );
									mergedLen->addIncoming( pieceLen, noPadBB );
									piece = mergedPiece;
									pieceLen = mergedLen;
								}
								pieces.push_back( piece );
								pieceLens.push_back( pieceLen );
							}
							pos = ph.endPos + 1;
							phIdx++;
						}
						else {
							size_t nextPh = ( phIdx < placeholders.size() ) ? placeholders[phIdx].startPos : fmtStr.size();
							std::string literal;
							while( pos < nextPh && pos < fmtStr.size() ) {
								if( fmtStr[pos] == '{' && pos + 1 < fmtStr.size() && fmtStr[pos + 1] == '{' ) {
									literal+= '{';
									pos+= 2;
								}
								else if( fmtStr[pos] == '}' && pos + 1 < fmtStr.size() && fmtStr[pos + 1] == '}' ) {
									literal+= '}';
									pos+= 2;
								}
								else {
									literal+= fmtStr[pos];
									pos++;
								}
							}
							if( literal.empty() == false ) {
								llvm::Constant* litStr = builder.CreateGlobalStringPtr( literal, "lit.piece" );
								pieces.push_back( litStr );
								pieceLens.push_back( llvm::ConstantInt::get( i64Type, literal.size() ) );
							}
						}
					}
					if( pieces.empty() ) {
						return builder.CreateGlobalStringPtr( "", "empty.fmt" );
					}
					llvm::Value* totalLen = llvm::ConstantInt::get( i64Type, 0 );
					for( llvm::Value* len : pieceLens ) {
						totalLen = builder.CreateAdd( totalLen, len, "total" );
					}
					llvm::Value* allocSize = builder.CreateAdd( totalLen, llvm::ConstantInt::get( i64Type, 1 ), "alloc.sz" );
					llvm::Value* resultBuf = builder.CreateCall( mallocFn, { allocSize }, "result.buf" );
					llvm::Value* offset = llvm::ConstantInt::get( i64Type, 0 );
					for( size_t pi = 0; pi < pieces.size(); pi++ ) {
						llvm::Value* dest = builder.CreateGEP( llvm::Type::getInt8Ty( context ), resultBuf, offset, "dest" );
						builder.CreateCall( memcpyFn, { dest, pieces[pi], pieceLens[pi] } );
						offset = builder.CreateAdd( offset, pieceLens[pi], "off" );
					}
					builder.CreateStore( llvm::ConstantInt::get( llvm::Type::getInt8Ty( context ), 0 ), builder.CreateGEP( llvm::Type::getInt8Ty( context ), resultBuf, offset ) );
					return resultBuf;
				}
				return self;
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			false,
			semantic::qualname::classes::string::Name,
			semantic::qualname::classes::string::Package,
			"",
			std::move( methods ),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::PointerType::getUnqual( context );
			}
		});
	}
	
	void Builtin::registerType( BuiltinTypeDescriptor descriptor ) {
		this->types.insert_or_assign( descriptor.name, std::move( descriptor ) );
	}
	
	void Builtin::registerUIntTypes() {
		this->registerType( BuiltinTypeDescriptor {
			false,
			semantic::qualname::classes::uint::Name,
			semantic::qualname::classes::uint::Package,
			"",
			std::vector<BuiltinMethodDescriptor>({
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::GetValue,
					semantic::qualname::classes::u64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return self;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::addable::methods::Add,
					semantic::qualname::classes::uint::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::uint::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::Type* i64 = llvm::Type::getInt64Ty( context );
							llvm::Value* lhs = self->getType() == i64 ? self : builder.CreateZExt( self, i64, "lhs.ext" );
							llvm::Value* rhs = arguments[0]->getType() == i64 ? arguments[0] : builder.CreateZExt( arguments[0], i64, "rhs.ext" );
							return builder.CreateAdd( lhs, rhs, "addtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::subtractable::methods::Subtract,
					semantic::qualname::classes::uint::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::uint::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::Type* i64 = llvm::Type::getInt64Ty( context );
							llvm::Value* lhs = self->getType() == i64 ? self : builder.CreateZExt( self, i64, "lhs.ext" );
							llvm::Value* rhs = arguments[0]->getType() == i64 ? arguments[0] : builder.CreateZExt( arguments[0], i64, "rhs.ext" );
							return builder.CreateSub( lhs, rhs, "subtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::multipliable::methods::Multiply,
					semantic::qualname::classes::uint::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::uint::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::Type* i64 = llvm::Type::getInt64Ty( context );
							llvm::Value* lhs = self->getType() == i64 ? self : builder.CreateZExt( self, i64, "lhs.ext" );
							llvm::Value* rhs = arguments[0]->getType() == i64 ? arguments[0] : builder.CreateZExt( arguments[0], i64, "rhs.ext" );
							return builder.CreateMul( lhs, rhs, "multmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::dividable::methods::Divide,
					semantic::qualname::classes::uint::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::uint::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::Type* i64 = llvm::Type::getInt64Ty( context );
							llvm::Value* lhs = self->getType() == i64 ? self : builder.CreateZExt( self, i64, "lhs.ext" );
							llvm::Value* rhs = arguments[0]->getType() == i64 ? arguments[0] : builder.CreateZExt( arguments[0], i64, "rhs.ext" );
							return builder.CreateUDiv( lhs, rhs, "divtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::modulable::methods::Modulo,
					semantic::qualname::classes::uint::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::uint::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::Type* i64 = llvm::Type::getInt64Ty( context );
							llvm::Value* lhs = self->getType() == i64 ? self : builder.CreateZExt( self, i64, "lhs.ext" );
							llvm::Value* rhs = arguments[0]->getType() == i64 ? arguments[0] : builder.CreateZExt( arguments[0], i64, "rhs.ext" );
							return builder.CreateURem( lhs, rhs, "modtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::interfaces::equatable::methods::Equals,
					semantic::qualname::classes::boolean::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::uint::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							return builder.CreateICmpEQ( self, arguments[0], "eqtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::CompareTo,
					semantic::qualname::classes::i32::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::uint::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							llvm::IntegerType* integer32Type = llvm::Type::getInt32Ty( context );
							llvm::Value* isLessThan = builder.CreateICmpULT( self, arguments[0], "lt" );
							llvm::Value* isGreaterThan = builder.CreateICmpUGT( self, arguments[0], "gt" );
							llvm::Value* greaterThanValue = builder.CreateSelect( isGreaterThan, llvm::ConstantInt::get( integer32Type, 1 ), llvm::ConstantInt::get( integer32Type, 0 ), "gtsel" );
							return builder.CreateSelect( isLessThan, llvm::ConstantInt::get( integer32Type, -1, true ), greaterThanValue, "cmptmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::BitwiseAnd,
					semantic::qualname::classes::uint::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::uint::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							return builder.CreateAnd( self, arguments[0], "andtmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::BitwiseOr,
					semantic::qualname::classes::uint::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::uint::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							return builder.CreateOr( self, arguments[0], "ortmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::BitwiseXor,
					semantic::qualname::classes::uint::Name,
					std::vector<MethodParameter>({ MethodParameter { "other", semantic::qualname::classes::uint::Name } }),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						if( arguments.empty() == false ) {
							return builder.CreateXor( self, arguments[0], "xortmp" );
						}
						return nullptr;
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::Hash,
					semantic::qualname::classes::i64::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						return builder.CreateIntCast( self, llvm::Type::getInt64Ty( context ), false, "hash" );
					}
				},
				BuiltinMethodDescriptor {
					semantic::qualname::classes::object::methods::ToString,
					semantic::qualname::classes::string::Name,
					std::vector<MethodParameter>({}),
					[]( llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) -> llvm::Value* {
						llvm::Module* currentModule = builder.GetInsertBlock()->getParent()->getParent();
						llvm::Value* buffer = builder.CreateCall( getMalloc( currentModule, context ), { llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 24 ) }, "buf" );
						llvm::Constant* formatString = builder.CreateGlobalStringPtr( "%lu", "uint.fmt" );
						llvm::Value* unsignedValue = builder.CreateIntCast( self, llvm::Type::getInt64Ty( context ), false, "uval" );
						builder.CreateCall( getSnprintf( currentModule, context ), { buffer, llvm::ConstantInt::get( llvm::Type::getInt64Ty( context ), 24 ), formatString, unsignedValue } );
						return buffer;
					}
				}
			}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt64Ty( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::u8::Name,
			semantic::qualname::classes::u8::Package,
			semantic::qualname::classes::uint::Name,
			std::vector<BuiltinMethodDescriptor>({}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt8Ty( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::u16::Name,
			semantic::qualname::classes::u16::Package,
			semantic::qualname::classes::uint::Name,
			std::vector<BuiltinMethodDescriptor>({}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt16Ty( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::u32::Name,
			semantic::qualname::classes::u32::Package,
			semantic::qualname::classes::uint::Name,
			std::vector<BuiltinMethodDescriptor>({}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt32Ty( context );
			}
		});
		this->registerType( BuiltinTypeDescriptor {
			true,
			semantic::qualname::classes::u64::Name,
			semantic::qualname::classes::u64::Package,
			semantic::qualname::classes::uint::Name,
			std::vector<BuiltinMethodDescriptor>({}),
			[]( llvm::LLVMContext& context ) -> llvm::Type* {
				return llvm::Type::getInt64Ty( context );
			}
		});
	}
	
}
