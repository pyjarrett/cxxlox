#pragma once

#include "scanner.hpp"

namespace cxxlox {

// Use enum value auto-incrementing to decide precedence levels.
enum Precedence
{
	PREC_NONE,
	PREC_ASSIGNMENT, // =
	PREC_OR,         // or
	PREC_AND,        // and
	PREC_EQUALITY,   // ==
	PREC_COMPARISON, // < > <= >=
	PREC_TERM,       // + -
	PREC_FACTOR,     // * /
	PREC_UNARY,      // ! -
	PREC_CALL,       // . ()
	PREC_PRIMARY
};

using ParseFn = void (*)(bool canAssign);

// Pratt parsing rule.
struct ParseRule {
	// Function to use when encountering the key's token type as a prefix.
	ParseFn prefix;

	// Function to use when encountering the key's token type as a infix operator.
	ParseFn infix;

	Precedence precedence;
};

// C++ doesn't have C99 array designated initializers.  Emulate this.  This also
// hides that TokenType can't be directly converted to an index without a cast.
struct PrattRuleMap {
	struct PrattRuleRow {
		TokenType type;
		ParseRule rule;
	};

	PrattRuleMap(std::initializer_list<PrattRuleRow> items);

	~PrattRuleMap() { delete[] rules_; }

	PrattRuleMap(const PrattRuleMap&) = delete;
	PrattRuleMap& operator=(const PrattRuleMap&) = delete;

	PrattRuleMap(PrattRuleMap&&) = delete;
	PrattRuleMap& operator=(PrattRuleMap&&) = delete;

	const ParseRule* operator[](TokenType token) const { return &rules_[static_cast<int>(token)]; }

private:
	ParseRule* rules_ = nullptr;
};

} // namespace cxxlox
