
//
// @author hxAri (hxari)
// @create 2026-06-18 16:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "uranite-lsp/lsp-document.hpp"
#include "uranite-lsp/lsp-protocol.hpp"
#include "uranite-lsp/lsp-workspace.hpp"

// =====================================================================
// Document: Construction & Tree-sitter Parsing
// =====================================================================

class LspDocumentTest : public ::testing::Test {
	protected:
		std::string simpleSource =
			"package testing\n"
			"\n"
			"public function main() -> I32:\n"
			"    return 0\n";

		std::string classSource =
			"package testing\n"
			"\n"
			"public class Point:\n"
			"    public I64 x\n"
			"    public I64 y\n"
			"\n"
			"    public function Point( self, I64 x, I64 y ) -> Void:\n"
			"        self.x = x\n"
			"        self.y = y\n"
			"\n"
			"    public function sum( self ) -> I64:\n"
			"        return self.x + self.y\n";
};

TEST_F( LspDocumentTest, ConstructionStoresUriAndContent ) {
	uranite::lsp::Document document( "file:///test.urn", this->simpleSource, 1 );
	EXPECT_EQ( document.uri(), "file:///test.urn" );
	EXPECT_EQ( document.content(), this->simpleSource );
	EXPECT_EQ( document.version(), 1 );
}

TEST_F( LspDocumentTest, ParsesValidSourceWithoutErrors ) {
	uranite::lsp::Document document( "file:///test.urn", this->simpleSource, 1 );
	std::vector<uranite::lsp::Diagnostic> diagnostics = document.collectTreeErrors();
	EXPECT_TRUE( diagnostics.empty() );
}

TEST_F( LspDocumentTest, ParsesClassSourceWithoutErrors ) {
	uranite::lsp::Document document( "file:///test.urn", this->classSource, 1 );
	std::vector<uranite::lsp::Diagnostic> diagnostics = document.collectTreeErrors();
	EXPECT_TRUE( diagnostics.empty() );
}

TEST_F( LspDocumentTest, DetectsSyntaxErrors ) {
	std::string brokenSource = "package testing\n\npublic function\n";
	uranite::lsp::Document document( "file:///bad.urn", brokenSource, 1 );
	std::vector<uranite::lsp::Diagnostic> diagnostics = document.collectTreeErrors();
	EXPECT_FALSE( diagnostics.empty() );
}

TEST_F( LspDocumentTest, EmptySourceDoesNotCrash ) {
	uranite::lsp::Document document( "file:///empty.urn", "", 1 );
	std::vector<uranite::lsp::Diagnostic> diagnostics = document.collectTreeErrors();
	TSNode rootNode = document.rootNode();
	(void)rootNode;
}

TEST_F( LspDocumentTest, SourceLinesPopulated ) {
	uranite::lsp::Document document( "file:///test.urn", this->simpleSource, 1 );
	EXPECT_EQ( document.sourceLines().size(), 4u );
	EXPECT_EQ( document.sourceLines()[0], "package testing" );
}

// =====================================================================
// Document: Incremental Content Changes
// =====================================================================

TEST_F( LspDocumentTest, FullContentReplace ) {
	uranite::lsp::Document document( "file:///test.urn", "hello world\n", 1 );

	std::vector<uranite::lsp::ContentChangeEvent> changes;
	uranite::lsp::ContentChangeEvent fullReplace;
	fullReplace.hasRange = false;
	fullReplace.text = "package updated\n";
	changes.push_back( fullReplace );

	document.applyContentChanges( changes, 2 );
	EXPECT_EQ( document.content(), "package updated\n" );
	EXPECT_EQ( document.version(), 2 );
}

TEST_F( LspDocumentTest, IncrementalRangeReplace ) {
	uranite::lsp::Document document( "file:///test.urn", "hello world\n", 1 );

	std::vector<uranite::lsp::ContentChangeEvent> changes;
	uranite::lsp::ContentChangeEvent rangeReplace;
	rangeReplace.hasRange = true;
	rangeReplace.range.start.line = 0;
	rangeReplace.range.start.character = 6;
	rangeReplace.range.end.line = 0;
	rangeReplace.range.end.character = 11;
	rangeReplace.text = "Uranite";
	changes.push_back( rangeReplace );

	document.applyContentChanges( changes, 2 );
	EXPECT_EQ( document.content(), "hello Uranite\n" );
}

TEST_F( LspDocumentTest, IncrementalChangeUpdatesTree ) {
	uranite::lsp::Document document( "file:///test.urn", this->simpleSource, 1 );
	EXPECT_TRUE( document.collectTreeErrors().empty() );

	std::vector<uranite::lsp::ContentChangeEvent> changes;
	uranite::lsp::ContentChangeEvent fullReplace;
	fullReplace.hasRange = false;
	fullReplace.text = "package testing\n\npublic function\n";
	changes.push_back( fullReplace );

	document.applyContentChanges( changes, 2 );
	EXPECT_FALSE( document.collectTreeErrors().empty() );
}

// =====================================================================
// Document: Symbol Collection
// =====================================================================

TEST_F( LspDocumentTest, CollectsDocumentSymbols ) {
	uranite::lsp::Document document( "file:///test.urn", this->simpleSource, 1 );
	std::vector<uranite::lsp::DocumentSymbol> symbols = document.collectDocumentSymbols();

	bool foundMain = false;
	for( const uranite::lsp::DocumentSymbol& symbol : symbols ) {
		if( symbol.name == "main" && symbol.kind == uranite::lsp::SymbolKind::FUNCTION ) {
			foundMain = true;
		}
	}
	EXPECT_TRUE( foundMain );
}

TEST_F( LspDocumentTest, CollectsClassSymbol ) {
	uranite::lsp::Document document( "file:///test.urn", this->classSource, 1 );
	std::vector<uranite::lsp::DocumentSymbol> symbols = document.collectDocumentSymbols();

	bool foundPoint = false;
	for( const uranite::lsp::DocumentSymbol& symbol : symbols ) {
		if( symbol.name == "Point" && symbol.kind == uranite::lsp::SymbolKind::CLASS ) {
			foundPoint = true;
		}
	}
	EXPECT_TRUE( foundPoint );
}

TEST_F( LspDocumentTest, CollectsPackageSymbol ) {
	uranite::lsp::Document document( "file:///test.urn", this->simpleSource, 1 );
	std::vector<uranite::lsp::DocumentSymbol> symbols = document.collectDocumentSymbols();

	bool foundPackage = false;
	for( const uranite::lsp::DocumentSymbol& symbol : symbols ) {
		if( symbol.kind == uranite::lsp::SymbolKind::PACKAGE ) {
			foundPackage = true;
		}
	}
	EXPECT_TRUE( foundPackage );
}

// =====================================================================
// Document: Folding Ranges
// =====================================================================

TEST_F( LspDocumentTest, CollectsFoldingRanges ) {
	uranite::lsp::Document document( "file:///test.urn", this->simpleSource, 1 );
	std::vector<uranite::lsp::FoldingRange> foldingRanges = document.collectFoldingRanges();
	EXPECT_FALSE( foldingRanges.empty() );
}

TEST_F( LspDocumentTest, FoldingRangesSpanMultipleLines ) {
	uranite::lsp::Document document( "file:///test.urn", this->classSource, 1 );
	std::vector<uranite::lsp::FoldingRange> foldingRanges = document.collectFoldingRanges();
	for( const uranite::lsp::FoldingRange& foldRange : foldingRanges ) {
		EXPECT_GT( foldRange.endLine, foldRange.startLine );
	}
}

// =====================================================================
// Document: Semantic Tokens
// =====================================================================

TEST_F( LspDocumentTest, CollectsSemanticTokens ) {
	uranite::lsp::Document document( "file:///test.urn", this->simpleSource, 1 );
	std::vector<uranite::lsp::SemanticToken> semanticTokens = document.collectSemanticTokens();
	EXPECT_FALSE( semanticTokens.empty() );
}

TEST_F( LspDocumentTest, SemanticTokensForClassSource ) {
	uranite::lsp::Document document( "file:///test.urn", this->classSource, 1 );
	std::vector<uranite::lsp::SemanticToken> semanticTokens = document.collectSemanticTokens();
	EXPECT_FALSE( semanticTokens.empty() );
}

TEST_F( LspDocumentTest, EmptySourceProducesNoSemanticTokens ) {
	uranite::lsp::Document document( "file:///empty.urn", "", 1 );
	std::vector<uranite::lsp::SemanticToken> semanticTokens = document.collectSemanticTokens();
	EXPECT_TRUE( semanticTokens.empty() );
}

// =====================================================================
// Document: Node Lookup
// =====================================================================

TEST_F( LspDocumentTest, FindsNodeAtPosition ) {
	uranite::lsp::Document document( "file:///test.urn", this->simpleSource, 1 );
	uranite::lsp::Position position;
	position.line = 2;
	position.character = 16;
	TSNode foundNode = document.findNodeAtPosition( position );
	EXPECT_FALSE( ts_node_is_null( foundNode ) );
	std::string nodeText = document.nodeText( foundNode );
	EXPECT_EQ( nodeText, "main" );
}

TEST_F( LspDocumentTest, NodeTextExtractionWorks ) {
	uranite::lsp::Document document( "file:///test.urn", this->simpleSource, 1 );
	TSNode rootNode = document.rootNode();
	EXPECT_FALSE( ts_node_is_null( rootNode ) );
	std::string fullText = document.nodeText( rootNode );
	EXPECT_EQ( fullText, this->simpleSource );
}

// =====================================================================
// Workspace: Document Management
// =====================================================================

TEST( LspWorkspaceTest, OpenAndGetDocument ) {
	uranite::lsp::Workspace workspace( "file:///project" );
	uranite::lsp::Document* openedDocument = workspace.openDocument(
		"file:///test.urn",
		"package testing\n",
		1
	);
	ASSERT_NE( openedDocument, nullptr );
	EXPECT_EQ( openedDocument->uri(), "file:///test.urn" );

	uranite::lsp::Document* retrievedDocument = workspace.getDocument( "file:///test.urn" );
	ASSERT_NE( retrievedDocument, nullptr );
	EXPECT_EQ( retrievedDocument, openedDocument );
}

TEST( LspWorkspaceTest, CloseDocumentRemovesIt ) {
	uranite::lsp::Workspace workspace( "file:///project" );
	workspace.openDocument( "file:///test.urn", "package testing\n", 1 );
	workspace.closeDocument( "file:///test.urn" );
	EXPECT_EQ( workspace.getDocument( "file:///test.urn" ), nullptr );
}

TEST( LspWorkspaceTest, GetNonexistentDocumentReturnsNull ) {
	uranite::lsp::Workspace workspace( "file:///project" );
	EXPECT_EQ( workspace.getDocument( "file:///nonexistent.urn" ), nullptr );
}

TEST( LspWorkspaceTest, SearchSymbolsAcrossDocuments ) {
	uranite::lsp::Workspace workspace( "file:///project" );
	workspace.openDocument(
		"file:///alpha.urn",
		"package testing\n\npublic class Alpha:\n    public I64 value\n",
		1
	);
	workspace.openDocument(
		"file:///beta.urn",
		"package testing\n\npublic class Beta:\n    public I64 count\n",
		1
	);

	std::vector<uranite::lsp::WorkspaceSymbol> allSymbols = workspace.searchSymbols( "" );
	EXPECT_GE( allSymbols.size(), 2u );

	std::vector<uranite::lsp::WorkspaceSymbol> alphaSymbols = workspace.searchSymbols( "Alpha" );
	bool foundAlpha = false;
	for( const uranite::lsp::WorkspaceSymbol& symbol : alphaSymbols ) {
		if( symbol.symbolName == "Alpha" ) foundAlpha = true;
	}
	EXPECT_TRUE( foundAlpha );
}

// =====================================================================
// Workspace: URI Conversion
// =====================================================================

TEST( LspWorkspaceTest, UriToFilePath ) {
	uranite::lsp::Workspace workspace( "file:///project" );
	EXPECT_EQ( workspace.uriToFilePath( "file:///home/user/test.urn" ), "/home/user/test.urn" );
	EXPECT_EQ( workspace.uriToFilePath( "/home/user/test.urn" ), "/home/user/test.urn" );
}

TEST( LspWorkspaceTest, FilePathToUri ) {
	uranite::lsp::Workspace workspace( "file:///project" );
	std::string generatedUri = workspace.filePathToUri( "/home/user/test.urn" );
	EXPECT_EQ( generatedUri, "file:///home/user/test.urn" );
}

TEST( LspWorkspaceTest, UriPercentDecoding ) {
	uranite::lsp::Workspace workspace( "file:///project" );
	EXPECT_EQ( workspace.uriToFilePath( "file:///home/my%20project/test.urn" ), "/home/my project/test.urn" );
}
