
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

#ifndef _URANITE_IR_MIR_PRINTER_HPP_
#define _URANITE_IR_MIR_PRINTER_HPP_

#include <string>
#include <sstream>

#include "uranite/ir/mir.hpp"

namespace uranite::ir::mir {
	
	/** @brief Pretty-prints an MIR module to a human-readable string representation. */
	class MIRPrinter {
	public:
		
		/** @brief Prints the entire MIR module. */
		std::string print( const MIRModuleDefinition& mirModule );
	
	private:
		
		void printFunction( const MIRFunctionDefinition& functionDefinition );
		void printBasicBlock( const MIRBasicBlock& basicBlock );
		void printInstruction( const MIRInstruction& instruction );
		std::string instructionKindToString( MIRInstructionKind instructionKind );
		void indent( int indentLevel );
		
		std::ostringstream outputStream;
	
	};

} // namespace uranite::ir::mir

#endif // end _URANITE_IR_MIR_PRINTER_HPP_
