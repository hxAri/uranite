
//
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
//

#ifndef _URANITE_ERRORS_HPP_
#define _URANITE_ERRORS_HPP_

#include <exception>
#include <string>
#include <vector>

namespace uranite::errors {
	
	/**
	 * @brief
	 *  A custom exception class that extends std::exception to provide enhanced error handling
	 *  This class supports multiple causes, error codes, and stack traces for better debugging
	 * 
	 * @author Ari Setiawan (hxAri)
	 * @class Throwable
	 * @namespace uranite
	 */
	class Throwable: public std::exception {
		
		protected:
			
			/** @brief Multiple pointer to the original exception cause (if any). */
			std::vector<std::exception_ptr> causes;
			
			/** @brief Numeric error code associated with the exception. */
			int code;
			
			/** @brief Error message describing the exception. */
			std::string message;
			
			/** @brief Captured stack trace at the moment of exception creation. */
			std::vector<std::string> stackTrace;
			
			/**
			 * @brief
			 *  Captures the current stack trace
			 *  Uses backtrace() to retrieve the call stack and stores it in stackTrace
			 * 
			 */
			void captureStackTrace();
		
		public:
			
			/** @brief Construct method of class Throwable. */
			explicit Throwable();
			virtual ~Throwable() noexcept = default;
			
			/**
			 * @brief
			 *  Construct method of class Throwable
			 * 
			 * @param message
			 *  The error message
			 */
			explicit Throwable( const std::string &message );
			
			/**
			 * @brief
			 *  Construct method of class Throwable
			 * 
			 * @param message
			 *  The error message
			 * @param code
			 *  The error code
			 */
			explicit Throwable( const std::string &message, int code );
			
			/**
			 * @brief
			 *  Construct method of class Throwable
			 * 
			 * @param message
			 *  The error message
			 * @param causes
			 *  An optional pointer to the original exception cause
			 */
			explicit Throwable( const std::string& message, std::vector<std::exception_ptr> causes );
			
			/**
			 * @brief
			 *  Construct method of class Throwable
			 * 
			 * @param message
			 *  The error message
			 * @param code
			 *  The error code
			 * @param causes
			 *  An optional pointer to the original exception cause
			 */
			explicit Throwable( const std::string& message, int code, std::vector<std::exception_ptr> causes );
			
			/** @brief Return the numeric error code. */
			int getCode();
			
			/** @brief Return the error message. */
			std::string getMessage();
			
			/** @brief Return formatted string containing the error code, message, and cause. */
			std::string getMessageAll();
			
			/** @brief Return the error stack traces */
			std::vector<std::string> getStackTrace();
			
			/** @brief Prints the captured stack trace to standard error */
			void printStackTrace() const;
			
			/** @brief Returns C-style string containing the error message. */
			const char* what() const noexcept override;
		
	};
	
	class BaseException: public Throwable {
		using Throwable::Throwable;
	};
	class Exception: public BaseException {
		using BaseException::BaseException;
	};
	class Error: public BaseException {
		using BaseException::BaseException;
	};
	
} // namespace uranite

#endif // end _URANITE_ERRORS_HPP_
