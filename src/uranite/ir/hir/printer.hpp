
//
// @author hxAri (hxari)
// @create 13-06-2026
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

#ifndef _URANITE_IR_HIR_PRINTER_HPP_
#define _URANITE_IR_HIR_PRINTER_HPP_

#include <string>
#include <sstream>

#include "uranite/ir/hir.hpp"

namespace uranite::ir::hir {
	
	/** @brief Pretty-prints an HIR module to a human-readable string representation. */
	class HIRPrinter {
	public:
		
		/** @brief Prints the entire HIR module tree. */
		std::string print( const HIRModule& hirModule );
	
	private:
		
		void printNode( const HIRNodeSharedPointer& node, int indentLevel );
		void printBlock( const HIRBlock& block, int indentLevel );
		void printFunctionDefinition( const HIRFunctionDefinition& function, int indentLevel );
		void printClassDefinition( const HIRClassDefinition& classDefinition, int indentLevel );
		void printStructDefinition( const HIRStructDefinition& structDefinition, int indentLevel );
		void printEnumDefinition( const HIREnumDefinition& enumDefinition, int indentLevel );
		void printInterfaceDefinition( const HIRInterfaceDefinition& interfaceDefinition, int indentLevel );
		
		void indent( int indentLevel );
		std::string nodeKindToString( HIRNodeKind nodeKind );
		
		std::ostringstream outputStream;
	
	};

} // namespace uranite::ir::hir

#endif // end _URANITE_IR_HIR_PRINTER_HPP_
