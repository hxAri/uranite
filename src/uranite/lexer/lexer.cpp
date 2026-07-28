
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

#include "uranite/lexer/lexer.hpp"

namespace uranite::lexer {
	
	Lexer::Lexer( 
		const std::string& source, 
		const std::string& filename, 
		diagnostic::Engine& diagnostic 
	) : diagnostic( diagnostic ),
		filename( filename ),
		source( source ) {
		this->filepath = std::filesystem::path( filename );
		this->filepath = this->filepath.parent_path();
		this->indentStack.push( 0 );
	}
	
	char Lexer::advance() {
		char processedChar = this->source[this->position++];
		if( processedChar == '\n' ) {
			this->line++;
			this->column = 1;
		}
		else {
			this->column++;
		}
		return processedChar;
	}
	
	char Lexer::current() const {
		if( this->isAtEnd() ) {
			return '\0';
		}
		return this->source[this->position];
	}
	
	lookup::SourceSharedPointer Lexer::currentSource() const {
		return std::make_shared<lookup::Source>(
			this->filename,
			this->filepath.string(),
			std::make_shared<lookup::Source::Location>(
				this->column,
				this->line
			),
			std::make_shared<lookup::Source::Range>(
			)
		);
	}
	
	void Lexer::handleIndentation() {
		int spaceCount = 0;
		while( this->isAtEnd() == false && ( this->current() == ' ' || this->current() == '\t' ) ) {
			if( this->current() == '\t' ) {
				spaceCount+= 4;
			}
			else {
				spaceCount++;
			}
			this->advance();
		}
		if( this->isAtEnd() || this->current() == '\n' ) {
			return;
		}
		if( this->current() == '#' ) {
			this->skipLineComment();
			return;
		}
		int previousIndent = this->indentStack.top();
		if( spaceCount > previousIndent ) {
			this->indentStack.push( spaceCount );
			this->tokens.push_back( token::Token( token::Type::Indent, this->currentSource(), "INDENT" ) );
		}
		else {
			while( spaceCount < this->indentStack.top() ) {
				this->indentStack.pop();
				this->tokens.push_back( token::Token( token::Type::Dedent, this->currentSource(), "DEDENT" ) );
			}
			if( spaceCount != this->indentStack.top() ) {
				this->diagnostic.error( this->currentSource(), "inconsistent indentation", "indentation does not match any outer level" );
			}
		}
		this->atLineStart = false;
	}
	
	bool Lexer::isAtEnd() const {
		return this->position >= this->source.size();
	}
	
	token::Token Lexer::makeToken( token::Type tokenKind, const std::string& tokenValue ) {
		return token::Token( tokenKind, this->currentSource(), tokenValue );
	}
	
	token::Token Lexer::makeToken( token::Type tokenKind ) {
		return token::Token( tokenKind, this->currentSource(), std::string( token::toString( tokenKind ) ) );
	}
	
	bool Lexer::match( char expectedChar ) {
		if( this->isAtEnd() || this->source[this->position] != expectedChar ) {
			return false;
		}
		this->advance();
		return true;
	}
	
	char Lexer::peek( int indexOffset ) const {
		size_t targetIndex = this->position + indexOffset;
		if( targetIndex >= this->source.size() ) {
			return '\0';
		}
		return this->source[targetIndex];
	}
	
	token::Token Lexer::readChar() {
		lookup::SourceSharedPointer characterSource = this->currentSource();
		this->advance();
		std::string characterValue;
		if( this->current() == '\\' ) {
			this->advance();
			char escapeCharacter = this->current();
			switch( escapeCharacter ) {
				case '\'':
					characterValue = "'";
					this->advance();
					break;
				case '0':
					characterValue = std::string( 1, '\0' );
					this->advance();
					break;
				case '\\':
					characterValue = "\\";
					this->advance();
					break;
				case 'n':
					characterValue = "\n";
					this->advance();
					break;
				case 'r':
					characterValue = "\r";
					this->advance();
					break;
				case 't':
					characterValue = "\t";
					this->advance();
					break;
				case 'x': {
					this->advance();
					std::string hexByteValue;
					for( int i = 0; i < 2 && this->isAtEnd() == false && std::isxdigit( this->current() ); i++ ) {
						hexByteValue+= this->current();
						this->advance();
					}
					if( hexByteValue.empty() == false ) {
						characterValue = std::string( 1, static_cast<char>( std::stoi( hexByteValue, nullptr, 16 ) ) );
					}
					else {
						this->diagnostic.error( this->currentSource(), "expected hex digits after \\x in character literal" );
						characterValue = "x";
					}
					break;
				}
				default:
					characterValue = std::string( 1, this->current() );
					this->advance();
					break;
			}
		}
		else {
			characterValue = std::string( 1, this->current() );
			this->advance();
		}
		if( this->current() != '\'' ) {
			this->diagnostic.error( this->currentSource(), "unterminated character literal" );
			return token::Token( token::Type::Error, characterSource, characterValue );
		}
		this->advance();
		return token::Token( token::Type::LiteralChar, characterSource, characterValue );
	}
	
	token::Token Lexer::readIdentifierOrKeyword() {
		lookup::SourceSharedPointer identifierSource = this->currentSource();
		std::string identifierValue;
		while( this->isAtEnd() == false && ( std::isalnum( this->current() ) || this->current() == '_' ) ) {
			identifierValue+= this->current();
			this->advance();
		}
		const std::unordered_map<std::string,token::Type>& keywordRegistry = token::keymaps();
		const std::unordered_map<std::string,token::Type>::const_iterator keywordIterator = keywordRegistry.find( identifierValue );
		if( keywordIterator != keywordRegistry.end() ) {
			return token::Token( keywordIterator->second, identifierSource, identifierValue );
		}
		return token::Token( token::Type::Identifier, identifierSource, identifierValue );
	}
	
	token::Token Lexer::readNumber() {
		bool isFloatingPoint = false;
		lookup::SourceSharedPointer numberSource = this->currentSource();
		std::string numberValue;
		if( this->current() == '0' && ( this->peek() == 'B' || this->peek() == 'b' ) ) {
			numberValue+= this->current();
			this->advance();
			numberValue+= this->current();
			this->advance();
			while( this->isAtEnd() == false && ( this->current() == '0' || this->current() == '1' || this->current() == '_' ) ) {
				if( this->current() != '_' ) {
					numberValue+= this->current();
				}
				this->advance();
			}
			return token::Token( token::Type::LiteralInteger, numberSource, numberValue );
		}
		if( this->current() == '0' && ( this->peek() == 'O' || this->peek() == 'o' ) ) {
			numberValue+= this->current();
			this->advance();
			numberValue+= this->current();
			this->advance();
			while( this->isAtEnd() == false && ( ( this->current() >= '0' && this->current() <= '7' ) || this->current() == '_' ) ) {
				if( this->current() != '_' ) {
					numberValue+= this->current();
				}
				this->advance();
			}
			return token::Token( token::Type::LiteralInteger, numberSource, numberValue );
		}
		if( this->current() == '0' && ( this->peek() == 'X' || this->peek() == 'x' ) ) {
			numberValue+= this->current();
			this->advance();
			numberValue+= this->current();
			this->advance();
			while( this->isAtEnd() == false && ( std::isxdigit( this->current() ) || this->current() == '_' ) ) {
				if( this->current() != '_' ) {
					numberValue+= this->current();
				}
				this->advance();
			}
			return token::Token( token::Type::LiteralInteger, numberSource, numberValue );
		}
		while( this->isAtEnd() == false && ( std::isdigit( this->current() ) || this->current() == '_' ) ) {
			if( this->current() != '_' ) {
				numberValue+= this->current();
			}
			this->advance();
		}
		if( this->current() == '.' && std::isdigit( this->peek() ) ) {
			isFloatingPoint = true;
			numberValue+= this->current();
			this->advance();
			while( this->isAtEnd() == false && ( std::isdigit( this->current() ) || this->current() == '_' ) ) {
				if( this->current() != '_' ) {
					numberValue+= this->current();
				}
				this->advance();
			}
		}
		if( this->current() == 'E' || this->current() == 'e' ) {
			isFloatingPoint = true;
			numberValue+= this->current();
			this->advance();
			if( this->current() == '+' || this->current() == '-' ) {
				numberValue+= this->current();
				this->advance();
			}
			while( this->isAtEnd() == false && std::isdigit( this->current() ) ) {
				numberValue+= this->current();
				this->advance();
			}
		}
		if( std::isalpha( this->current() ) ) {
			std::string numberSuffix;
			( void )this->position;
			while( this->isAtEnd() == false && std::isalnum( this->current() ) ) {
				numberSuffix+= this->current();
				this->advance();
			}
			numberValue+= numberSuffix;
		}
		token::Type numericTokenType = token::Type::LiteralInteger;
		if( isFloatingPoint ) {
			numericTokenType = token::Type::LiteralFloat;
		}
		return token::Token( numericTokenType, numberSource, numberValue );
	}
	
	token::Token Lexer::readString() {
		lookup::SourceSharedPointer stringSource = this->currentSource();
		char quoteCharacter = this->advance();
		std::string stringValue;
		while( this->isAtEnd() == false ) {
			char currentChar = this->current();
			if( currentChar == quoteCharacter ) {
				this->advance();
				return token::Token( token::Type::LiteralString, stringSource, stringValue );
			}
			if( currentChar == '\n' ) {
				this->diagnostic.error( this->currentSource(), "unterminated string literal" );
				return token::Token( token::Type::Error, stringSource, stringValue );
			}
			if( currentChar == '\\' ) {
				this->advance();
				char escapeChar = this->current();
				switch( escapeChar ) {
					case '"':
						stringValue+= '"';
						break;
					case '\'':
						stringValue+= '\'';
						break;
					case '0':
						stringValue+= '\0';
						break;
					case '\\':
						stringValue+= '\\';
						break;
					case 'n':
						stringValue+= '\n';
						break;
					case 'r':
						stringValue+= '\r';
						break;
					case 't':
						stringValue+= '\t';
						break;
					case 'x': {
						this->advance();
						std::string hexSequence;
						for( int i = 0; i < 2 && this->isAtEnd() == false && std::isxdigit( this->current() ); i++ ) {
							hexSequence+= this->current();
							this->advance();
						}
						stringValue+= static_cast<char>( std::stoi( hexSequence, nullptr, 16 ) );
						continue;
					}
					default: {
						std::string warningMessage = fmt::format( "unknown escape sequence '\\{}'", std::string( 1, escapeChar ) );
						this->diagnostic.warning( this->currentSource(), warningMessage );
						stringValue+= escapeChar;
						break;
					}
				}
			}
			else {
				stringValue+= currentChar;
			}
			this->advance();
		}
		this->diagnostic.error( stringSource, "unterminated string literal" );
		return token::Token( token::Type::Error, stringSource, stringValue );
	}
	
	void Lexer::skipBlockComment() {
		this->advance();
		int nestingDepth = 1;
		while( this->isAtEnd() == false && nestingDepth > 0 ) {
			if( this->current() == '#' && this->peek() == '{' ) {
				nestingDepth++;
				this->advance();
				this->advance();
			}
			else if( this->current() == '}' && this->peek() == '#' ) {
				nestingDepth--;
				this->advance();
				this->advance();
			}
			else {
				this->advance();
			}
		}
	}
	
	void Lexer::skipLineComment() {
		while( this->isAtEnd() == false && this->current() != '\n' ) {
			this->advance();
		}
	}
	
	void Lexer::skipTripleQuoteComment( char quoteCharacter ) {
		this->advance();
		this->advance();
		this->advance();
		while( this->isAtEnd() == false ) {
			if( this->current() == quoteCharacter && this->peek() == quoteCharacter && this->peek( 2 ) == quoteCharacter ) {
				this->advance();
				this->advance();
				this->advance();
				return;
			}
			this->advance();
		}
	}
	
	std::vector<token::Token> Lexer::tokenize() {
		while( this->isAtEnd() == false ) {
			if( this->atLineStart ) {
				if( this->current() == '\n' ) {
					this->advance();
					continue;
				}
				if( this->parenDepth == 0 ) {
					this->handleIndentation();
				}
				else {
					while( this->isAtEnd() == false && ( this->current() == ' ' || this->current() == '\t' ) ) {
						this->advance();
					}
					this->atLineStart = false;
				}
				continue;
			}
			char character = this->current();
			if( character == '\t' || character == '\r' || character == ' ' ) {
				this->advance();
				continue;
			}
			if( character == '\\' && this->peek() == '\n' ) {
				this->advance();
				this->advance();
				continue;
			}
			if( character == '\n' ) {
				if( this->parenDepth == 0 ) {
					if( this->tokens.empty() == false ) {
						token::Type lastTokenKind = this->tokens.back().type;
						if( lastTokenKind != token::Type::Dedent &&
							lastTokenKind != token::Type::Indent &&
							lastTokenKind != token::Type::Newline ) {
							this->tokens.push_back( token::Token( token::Type::Newline, this->currentSource(), "\\n" ) );
						}
					}
				}
				this->advance();
				this->atLineStart = true;
				continue;
			}
			if( character == '#' ) {
				if( this->peek() == '{' ) {
					this->advance();
					this->skipBlockComment();
				}
				else {
					this->skipLineComment();
				}
				continue;
			}
			if( ( character == '\'' && this->peek() == '\'' && this->peek( 2 ) == '\'' ) ||
				( character == '"' && this->peek() == '"' && this->peek( 2 ) == '"' ) ) {
				this->skipTripleQuoteComment( character );
				continue;
			}
			if( character == '"' ) {
				this->tokens.push_back( this->readString() );
				continue;
			}
			if( character == '\'' ) {
				this->tokens.push_back( this->readChar() );
				continue;
			}
			if( std::isdigit( character ) ) {
				this->tokens.push_back( this->readNumber() );
				continue;
			}
			if( std::isalpha( character ) || character == '_' ) {
				this->tokens.push_back( this->readIdentifierOrKeyword() );
				continue;
			}
			lookup::SourceSharedPointer operatorSource = this->currentSource();
			this->advance();
			switch( character ) {
				case '!': {
					if( this->match( '=' ) ) {
						this->tokens.push_back( token::Token( token::Type::NotEqual, operatorSource, "!=" ) );
					}
					else {
						this->tokens.push_back( token::Token( token::Type::Bang, operatorSource, "!" ) );
					}
					break;
				}
				case '%': {
					if( this->match( '=' ) ) {
						this->tokens.push_back( token::Token( token::Type::PercentAssignment, operatorSource, "%=" ) );
					}
					else {
						this->tokens.push_back( token::Token( token::Type::Percent, operatorSource, "%" ) );
					}
					break;
				}
				case '&': {
					if( this->match( '=' ) ) {
						this->tokens.push_back( token::Token( token::Type::AmpersandAssignment, operatorSource, "&=" ) );
					}
					else {
						this->tokens.push_back( token::Token( token::Type::Ampersand, operatorSource, "&" ) );
					}
					break;
				}
				case '(':
					this->parenDepth++;
					this->tokens.push_back( token::Token( token::Type::LeftParenthesis, operatorSource, "(" ) );
					break;
				case ')':
					this->parenDepth--;
					this->tokens.push_back( token::Token( token::Type::RightParenthesis, operatorSource, ")" ) );
					break;
				case '*': {
					if( this->match( '*' ) ) {
						this->tokens.push_back( token::Token( token::Type::Power, operatorSource, "**" ) );
					}
					else if( this->match( '=' ) ) {
						this->tokens.push_back( token::Token( token::Type::StarAssignment, operatorSource, "*=" ) );
					}
					else {
						this->tokens.push_back( token::Token( token::Type::Star, operatorSource, "*" ) );
					}
					break;
				}
				case '+': {
					if( this->match( '+' ) ) { 
						this->tokens.push_back( token::Token( token::Type::Increment, operatorSource, "++" ) );
					}
					else if( this->match( '=' ) ) { 
						this->tokens.push_back( token::Token( token::Type::PlusAssignment, operatorSource, "+=" ) );
					}
					else {
						this->tokens.push_back( token::Token( token::Type::Plus, operatorSource, "+" ) );
					}
					break;
				}
				case ',':
					this->tokens.push_back( token::Token( token::Type::Comma, operatorSource, "," ) );
					break;
				case '-': {
					if( this->match( '-' ) ) {
						this->tokens.push_back( token::Token( token::Type::Decrement, operatorSource, "--" ) );
					}
					else if( this->match( '>' ) ) {
						this->tokens.push_back( token::Token( token::Type::Arrow, operatorSource, "->" ) );
					}
					else if( this->match( '=' ) ) {
						this->tokens.push_back( token::Token( token::Type::MinusAssignment, operatorSource, "-=" ) );
					}
					else { this->tokens.push_back( token::Token( token::Type::Minus, operatorSource, "-" ) ); }
					break;
				}
				case '.':
					if( this->match( '.' ) ) {
						if( this->match( '.' ) ) { this->tokens.push_back( token::Token( token::Type::Ellipsis, operatorSource, "..." ) ); }
						else { this->tokens.push_back( token::Token( token::Type::DoubleDot, operatorSource, ".." ) ); }
					}
					else { this->tokens.push_back( token::Token( token::Type::Dot, operatorSource, "." ) ); }
					break;
				case '/': {
					if( this->match( '=' ) ) {
						this->tokens.push_back( token::Token( token::Type::SlashAssignment, operatorSource, "/=" ) );
					}
					else {
						bool isRegex = false;
						if( this->tokens.empty() ) {
							isRegex = true;
						}
						else {
							token::Type prevType = this->tokens.back().type;
							if( prevType != token::Type::Identifier &&
								prevType != token::Type::LiteralInteger &&
								prevType != token::Type::LiteralFloat &&
								prevType != token::Type::LiteralString &&
								prevType != token::Type::LiteralChar &&
								prevType != token::Type::RightParenthesis &&
								prevType != token::Type::RightBracket &&
								prevType != token::Type::KeywordSelf &&
								prevType != token::Type::KeywordTrue &&
								prevType != token::Type::KeywordFalse &&
								prevType != token::Type::KeywordNone ) {
								isRegex = true;
							}
						}
						if( isRegex && this->position < this->source.size() && this->source[this->position] != ' ' && this->source[this->position] != '\n' ) {
							std::string regexPattern;
							bool escaped = false;
							bool inCharClass = false;
							while( this->position < this->source.size() ) {
								char rc = this->source[this->position];
								if( escaped ) {
									regexPattern+= rc;
									escaped = false;
									this->position++;
									continue;
								}
								if( rc == '\\' ) {
									regexPattern+= rc;
									escaped = true;
									this->position++;
									continue;
								}
								if( rc == '[' ) { inCharClass = true; }
								if( rc == ']' ) { inCharClass = false; }
								if( rc == '/' && inCharClass == false ) {
									this->position++;
									break;
								}
								if( rc == '\n' ) {
									break;
								}
								regexPattern+= rc;
								this->position++;
							}
							this->tokens.push_back( token::Token( token::Type::LiteralRegex, operatorSource, regexPattern ) );
						}
						else {
							this->tokens.push_back( token::Token( token::Type::Slash, operatorSource, "/" ) );
						}
					}
					break;
				}
				case ':':
					if( this->match( ':' ) ) { this->tokens.push_back( token::Token( token::Type::DoubleColon, operatorSource, "::" ) ); }
					else { this->tokens.push_back( token::Token( token::Type::Colon, operatorSource, ":" ) ); }
					break;
				case ';':
					this->tokens.push_back( token::Token( token::Type::Semicolon, operatorSource, ";" ) );
					break;
				case '<': {
					if( this->match( '<' ) ) {
						if( this->match( '=' ) ) {
							this->tokens.push_back( token::Token( token::Type::ShiftLeftAssignment, operatorSource, "<<=" ) );
						}
						else {
							this->tokens.push_back( token::Token( token::Type::ShiftLeft, operatorSource, "<<" ) );
						}
					}
					else if( this->match( '=' ) ) {
						this->tokens.push_back( token::Token( token::Type::LessThanEqual, operatorSource, "<=" ) );
					}
					else {
						this->tokens.push_back( token::Token( token::Type::LessThan, operatorSource, "<" ) );
					}
					break;
				}
				case '=': {
					if( this->match( '=' ) ) {
						this->tokens.push_back( token::Token( token::Type::Equal, operatorSource, "==" ) );
					}
					else if( this->match( '>' ) ) {
						this->tokens.push_back( token::Token( token::Type::FatArrow, operatorSource, "=>" ) );
					}
					else {
						this->tokens.push_back( token::Token( token::Type::Assignment, operatorSource, "=" ) );
					}
					break;
				}
				case '>': {
					if( this->match( '>' ) ) {
						if( this->match( '=' ) ) {
							this->tokens.push_back( token::Token( token::Type::ShiftRightAssignment, operatorSource, ">>=" ) );
						}
						else {
							this->tokens.push_back( token::Token( token::Type::ShiftRight, operatorSource, ">>" ) );
						}
					}
					else if( this->match( '=' ) ) {
						this->tokens.push_back( token::Token( token::Type::GreaterThanEqual, operatorSource, ">=" ) );
					}
					else { 
						this->tokens.push_back( token::Token( token::Type::GreaterThan, operatorSource, ">" ) );
					}
					break;
				}
				case '?':
					this->tokens.push_back( token::Token( token::Type::Question, operatorSource, "?" ) );
					break;
				case '@':
					this->tokens.push_back( token::Token( token::Type::At, operatorSource, "@" ) );
					break;
				case '[':
					this->parenDepth++;
					this->tokens.push_back( token::Token( token::Type::LeftBracket, operatorSource, "[" ) );
					break;
				case ']':
					this->parenDepth--;
					this->tokens.push_back( token::Token( token::Type::RightBracket, operatorSource, "]" ) );
					break;
				case '^': {
					if( this->match( '=' ) ) {
						this->tokens.push_back( token::Token( token::Type::CaretAssignment, operatorSource, "^=" ) );
					}
					else { 
						this->tokens.push_back( token::Token( token::Type::Caret, operatorSource, "^" ) );
					}
					break;
				}
				case '{':
					this->parenDepth++;
					this->tokens.push_back( token::Token( token::Type::LeftBrace, operatorSource, "{" ) );
					break;
				case '|': {
					if( this->match( '=' ) ) {
						this->tokens.push_back( token::Token( token::Type::PipeAssignment, operatorSource, "|=" ) );
					}
					else {
						this->tokens.push_back( token::Token( token::Type::Pipe, operatorSource, "|" ) );
					}
					break;
				}
				case '}':
					this->parenDepth--;
					this->tokens.push_back( token::Token( token::Type::RightBrace, operatorSource, "}" ) );
					break;
				case '~':
					this->tokens.push_back( token::Token( token::Type::Tilde, operatorSource, "~" ) );
					break;
				default: {
					std::string errorMessage = fmt::format( "unexpected character '{}'", std::string( 1, character ) );
					this->diagnostic.error( operatorSource, errorMessage );
					this->tokens.push_back( token::Token( token::Type::Error, operatorSource, std::string( 1, character ) ) );
					break;
				}
			}
		}
		while( this->indentStack.size() > 1 ) {
			this->indentStack.pop();
			this->tokens.push_back( token::Token( token::Type::Dedent, this->currentSource(), "DEDENT" ) );
		}
		if( this->tokens.empty() == false && this->tokens.back().type != token::Type::Newline ) {
			this->tokens.push_back( token::Token( token::Type::Newline, this->currentSource(), "\\n" ) );
		}
		this->tokens.push_back( token::Token( token::Type::Eof, this->currentSource(), "" ) );
		return this->tokens;
	}
	
}
