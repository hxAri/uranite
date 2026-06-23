
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

#include "uranite/semantic/scope.hpp"

namespace uranite::semantic {
	
	Scope::Scope( Kind kind, ScopeSharedPointer parent ) : kindT( kind ), parentT( std::move( parent ) ) {
	}
	
	bool Scope::define( const std::string& name, SymbolSharedPointer symbol ) {
		std::unordered_map<std::string, std::vector<SymbolSharedPointer>>::iterator existingIterator = this->symbolsT.find( name );
		if( existingIterator != this->symbolsT.end() ) {
			if( symbol->kind == Symbol::Kind::Field || symbol->kind == Symbol::Kind::Variable ||
				symbol->kind == Symbol::Kind::Parameter || symbol->kind == Symbol::Kind::Type ||
				symbol->kind == Symbol::Kind::EnumVariant || symbol->kind == Symbol::Kind::Module ) {
				return false;
			}
			for( size_t existingIndex = 0; existingIndex < existingIterator->second.size(); existingIndex++ ) {
				const SymbolSharedPointer& existingSymbol = existingIterator->second[existingIndex];
				if( existingSymbol->source != nullptr && symbol->source != nullptr &&
					existingSymbol->source->filename == symbol->source->filename &&
					existingSymbol->source->location != nullptr && symbol->source->location != nullptr &&
					existingSymbol->source->location->line == symbol->source->location->line ) {
					existingIterator->second[existingIndex] = symbol;
					return true;
				}
				if( existingSymbol->typeref != nullptr && symbol->typeref != nullptr &&
					existingSymbol->typeref->kind == Type::Kind::Function && symbol->typeref->kind == Type::Kind::Function ) {
					FunctionTypeSharedPointer existingFunctionType = std::static_pointer_cast<FunctionType>( existingSymbol->typeref );
					FunctionTypeSharedPointer newFunctionType = std::static_pointer_cast<FunctionType>( symbol->typeref );
					if( newFunctionType->parameterTypes.size() == existingFunctionType->parameterTypes.size() ) {
						bool signatureMatch = true;
						for( size_t paramIndex = 0; paramIndex < newFunctionType->parameterTypes.size(); paramIndex++ ) {
							if( newFunctionType->parameterTypes[paramIndex]->toString() != existingFunctionType->parameterTypes[paramIndex]->toString() ) {
								signatureMatch = false;
								break;
							}
						}
						if( signatureMatch ) {
							bool crossModuleDuplicate = ( existingSymbol->source != nullptr && symbol->source != nullptr &&
								existingSymbol->source->filename != symbol->source->filename );
							if( crossModuleDuplicate ) {
								return true;
							}
							return false;
						}
					}
				}
			}
			existingIterator->second.push_back( symbol );
			return true;
		}
		this->symbolsT[name] = { symbol };
		return true;
	}
	
	bool Scope::isInsideClass() const {
		if( this->kindT == Scope::Kind::Class ) {
			return true;
		}
		if( this->parentT ) {
			return this->parentT->isInsideClass();
		}
		return false;
	}
	
	bool Scope::isInsideFunction() const {
		if( this->kindT == Scope::Kind::Function ) {
			return true;
		}
		if( this->parentT ) {
			return this->parentT->isInsideFunction();
		}
		return false;
	}
	
	bool Scope::isInsideLoop() const {
		if( this->kindT == Scope::Kind::Loop || this->kindT == Scope::Kind::Switch ) {
			return true;
		}
		if( this->parentT ) {
			return this->parentT->isInsideLoop();
		}
		return false;
	}
	
	bool Scope::isInsideUnsafe() const {
		if( this->kindT == Scope::Kind::Unsafe ) {
			return true;
		}
		if( this->parentT ) {
			return this->parentT->isInsideUnsafe();
		}
		return false;
	}
	
	SymbolSharedPointer Scope::lookup( const std::string& name ) const {
		std::unordered_map<std::string, std::vector<SymbolSharedPointer>>::const_iterator symbolIterator = this->symbolsT.find( name );
		if( symbolIterator != this->symbolsT.end() && symbolIterator->second.empty() == false ) {
			return symbolIterator->second.front();
		}
		if( this->parentT ) {
			return this->parentT->lookup( name );
		}
		return nullptr;
	}

	SymbolSharedPointer Scope::lookupLocal( const std::string& name ) const {
		std::unordered_map<std::string, std::vector<SymbolSharedPointer>>::const_iterator symbolIterator = this->symbolsT.find( name );
		if( symbolIterator != this->symbolsT.end() && symbolIterator->second.empty() == false ) {
			return symbolIterator->second.front();
		}
		return nullptr;
	}

	std::vector<SymbolSharedPointer> Scope::lookupAll( const std::string& name ) const {
		std::unordered_map<std::string, std::vector<SymbolSharedPointer>>::const_iterator symbolIterator = this->symbolsT.find( name );
		if( symbolIterator != this->symbolsT.end() ) {
			return symbolIterator->second;
		}
		if( this->parentT ) {
			return this->parentT->lookupAll( name );
		}
		return {};
	}

	std::vector<SymbolSharedPointer> Scope::lookupAllLocal( const std::string& name ) const {
		std::unordered_map<std::string, std::vector<SymbolSharedPointer>>::const_iterator symbolIterator = this->symbolsT.find( name );
		if( symbolIterator != this->symbolsT.end() ) {
			return symbolIterator->second;
		}
		return {};
	}
	
}
