
#include <stdlib.h>
#include <string.h>

#include "modelgen.h"
#include "debug.h"


static inline void _mgAssert(const char *expression, const char *file, int line)
{
	fprintf(stderr, "%s:%d: Assertion Failed: %s\n", file, line, expression);
	exit(1);
}


#define MG_ASSERT(expression) ((expression) ? ((void)0) : _mgAssert(#expression, __FILE__, __LINE__))


#define _MG_TOKEN_SCAN_LINE(token)  for (; (token->type == MG_TOKEN_WHITESPACE) || (token->type == MG_TOKEN_COMMENT); ++token)
#define _MG_TOKEN_SCAN_LINES(token) for (; (token->type == MG_TOKEN_NEWLINE) || (token->type == MG_TOKEN_WHITESPACE) || (token->type == MG_TOKEN_COMMENT); ++token)


#define _MG_OPERATOR_PRECEDENCE_LEVELS 7
#define _MG_OPERATOR_PRECEDENCE_LONGEST_LEVEL 6

static const int _MG_OPERATOR_PRECEDENCE_TYPE_COUNT[_MG_OPERATOR_PRECEDENCE_LEVELS] = { 6, 1, 1, 2, 4, 2, 3 };

static const MGTokenType _MG_OPERATOR_PRECEDENCE_TYPES[_MG_OPERATOR_PRECEDENCE_LEVELS][_MG_OPERATOR_PRECEDENCE_LONGEST_LEVEL] = {
	/* Highest Precedence */
	{ MG_TOKEN_ASSIGN, MG_TOKEN_ADD_ASSIGN, MG_TOKEN_SUB_ASSIGN, MG_TOKEN_MUL_ASSIGN, MG_TOKEN_DIV_ASSIGN, MG_TOKEN_MOD_ASSIGN },
	{ MG_TOKEN_OR },
	{ MG_TOKEN_AND },
	{ MG_TOKEN_EQUAL, MG_TOKEN_NOT_EQUAL },
	{ MG_TOKEN_LESS, MG_TOKEN_LESS_EQUAL, MG_TOKEN_GREATER, MG_TOKEN_GREATER_EQUAL },
	{ MG_TOKEN_ADD, MG_TOKEN_SUB },
	{ MG_TOKEN_MUL, MG_TOKEN_DIV, MG_TOKEN_MOD }
	/* Lowest Precedence */
};


void mgCreateParser(MGParser *parser)
{
	memset(parser, 0, sizeof(MGParser));

	mgCreateTokenizer(&parser->tokenizer);
}


void mgDestroyParser(MGParser *parser)
{
	mgDestroyTokenizer(&parser->tokenizer);

	if (parser->root)
		mgDestroyNode(parser->root);
}


MGNode* mgCreateNode(MGToken *token)
{
	MGNode *node = (MGNode*) calloc(1, sizeof(MGNode));

	node->token = token;
	node->tokenBegin = token;

	return node;
}


void mgDestroyNode(MGNode *node)
{
	for (size_t i = 0; i < node->childCount; ++i)
		if (node->children[i])
			mgDestroyNode(node->children[i]);
	free(node->children);

	free(node);
}


static void _mgAddChild(MGNode *parent, MGNode *child)
{
	if (parent->childCapacity == parent->childCount)
	{
		parent->childCapacity = parent->childCapacity ? parent->childCapacity << 1 : 2;
		parent->children = (MGNode**) realloc(parent->children, parent->childCapacity * sizeof(MGNode**));
	}

	parent->tokenEnd = child->tokenEnd;
	parent->children[parent->childCount++] = child;
	child->parent = parent;
}


static inline MGNode* _mgWrapNode(MGToken *token, MGNode *node)
{
	MGNode *parent = mgCreateNode(node->tokenBegin);
	parent->token = token;

	_mgAddChild(parent, node);

	return parent;
}


static MGNode* _mgParseExpression(MGParser *parser, MGToken *token);


static MGNode* _mgParseExpressionList(MGParser *parser, MGToken *token, MGNode *node, MGTokenType end)
{
	do
	{
		_MG_TOKEN_SCAN_LINES(token);

		if (token->type == end)
			break;

		MG_ASSERT(token->type != MG_TOKEN_EOF);

		MGNode *child = _mgParseExpression(parser, token);
		MG_ASSERT(child);
		_mgAddChild(node, child);

		token = child->tokenEnd + 1;

		_MG_TOKEN_SCAN_LINES(token);

		if (token->type == end)
			break;

		MG_ASSERT(token->type == MG_TOKEN_COMMA);

		node->tokenEnd = ++token;
	}
	while (token->type != end);

	MG_ASSERT(token->type == end);

	node->tokenEnd = token;

	return node;
}


static MGNode* _mgParseSubexpression(MGParser *parser, MGToken *token)
{
	_MG_TOKEN_SCAN_LINES(token);

	MGNode *node = NULL;

	if ((token->type == MG_TOKEN_INTEGER) ||
		(token->type == MG_TOKEN_FLOAT))
	{
		node = mgCreateNode(token);
		node->type = (token->type == MG_TOKEN_INTEGER) ? MG_NODE_INTEGER : MG_NODE_FLOAT;
		node->tokenEnd = token++;
	}
	else if (token->type == MG_TOKEN_IDENTIFIER)
	{
		node = mgCreateNode(token);
		node->type = MG_NODE_IDENTIFIER;
		node->tokenEnd = token++;
	}
	else if (token->type == MG_TOKEN_STRING)
	{
		node = mgCreateNode(token);
		node->type = MG_NODE_STRING;
		node->tokenEnd = token++;
	}
	else if ((token->type == MG_TOKEN_SUB) ||
	         (token->type == MG_TOKEN_ADD) ||
	         (token->type == MG_TOKEN_NOT))
	{
		node = mgCreateNode(token);
		node->type = MG_NODE_UNARY_OP;
		++token;

		_mgAddChild(node, _mgParseSubexpression(parser, token));

		MG_ASSERT(node->childCount == 1);

		token = node->tokenEnd + 1;
	}
	else if (token->type == MG_TOKEN_LPAREN)
	{
		node = mgCreateNode(token);
		node->type = MG_NODE_TUPLE;
		++token;

		_mgParseExpressionList(parser, token, node, MG_TOKEN_RPAREN);

		token = node->tokenEnd + 1;
	}
	else if (token->type == MG_TOKEN_LSQUARE)
	{
		node = mgCreateNode(token);
		node->type = MG_NODE_TUPLE;
		++token;

		_mgParseExpressionList(parser, token, node, MG_TOKEN_RSQUARE);

		token = node->tokenEnd + 1;
	}

	MG_ASSERT(node);

	for (;;)
	{
		_MG_TOKEN_SCAN_LINE(token);

		if (token->type == MG_TOKEN_LPAREN)
		{
			node = _mgWrapNode(token, node);
			node->type = MG_NODE_CALL;
			++token;

			_mgParseExpressionList(parser, token, node, MG_TOKEN_RPAREN);

			token = node->tokenEnd + 1;
		}
		else if (token->type == MG_TOKEN_LSQUARE)
		{
			node = _mgWrapNode(token, node);
			node->type = MG_NODE_INDEX;
			++token;

			_mgParseExpressionList(parser, token, node, MG_TOKEN_RSQUARE);

			token = node->tokenEnd + 1;
		}
		else
			break;
	}

	return node;
}


static MGNode* _mgParseBinaryOperation(MGParser *parser, MGToken *token, int level)
{
	MGNode *node = (level < (_MG_OPERATOR_PRECEDENCE_LEVELS - 1)) ?
	               _mgParseBinaryOperation(parser, token, level + 1) :
	               _mgParseSubexpression(parser, token);
	MG_ASSERT(node);

	for (;;)
	{
		token = node->tokenEnd + 1;
		_MG_TOKEN_SCAN_LINE(token);

		MGbool match = MG_FALSE;

		for (int i = 0; i < _MG_OPERATOR_PRECEDENCE_TYPE_COUNT[level]; ++i)
		{
			if (token->type == _MG_OPERATOR_PRECEDENCE_TYPES[level][i])
			{
				match = MG_TRUE;
				break;
			}
		}

		if (!match)
			break;

		node = _mgWrapNode(token, node);
		node->type = MG_NODE_BIN_OP;
		node->tokenEnd = token;

		++token;

		MGNode *child = (level < (_MG_OPERATOR_PRECEDENCE_LEVELS - 1)) ?
		                _mgParseBinaryOperation(parser, token, level + 1) :
		                _mgParseSubexpression(parser, token);
		MG_ASSERT(child);
		_mgAddChild(node, child);

		MG_ASSERT(node->childCount == 2);
	}

	return node;
}


static inline MGNode* _mgParseExpression(MGParser *parser, MGToken *token)
{
	return _mgParseBinaryOperation(parser, token, 0);
}


static MGNode* _mgParseModule(MGParser *parser, MGToken *token)
{
	MGNode *node = mgCreateNode(token);
	node->type = MG_NODE_MODULE;

	_MG_TOKEN_SCAN_LINES(token);

	while (token->type != MG_TOKEN_EOF)
	{
		MGNode *child = _mgParseExpression(parser, token);
		MG_ASSERT(child);

		_mgAddChild(node, child);

		token = node->tokenEnd + 1;
		_MG_TOKEN_SCAN_LINES(token);
	}

	node->tokenEnd = token;

	if (token->type != MG_TOKEN_EOF)
	{
#if MG_ANSI_COLORS
		fputs("\e[90m", stdout);
#endif

		printf("Error: Unexpected token, expected %s\n", _MG_TOKEN_NAMES[MG_TOKEN_EOF]);
		mgDebugInspectToken(token, parser->tokenizer.filename, MG_FALSE);

#if MG_ANSI_COLORS
		fputs("\e[0m", stdout);
#endif

		MG_ASSERT(token->type != MG_TOKEN_EOF);
	}

	return node;
}


static inline MGNode* _mgParse(MGParser *parser)
{
	parser->root = _mgParseModule(parser, parser->tokenizer.tokens);

	return parser->root;
}


MGNode* mgParseFile(MGParser *parser, const char *filename)
{
	if (!mgTokenizeFile(&parser->tokenizer, filename, NULL))
		return NULL;

	return _mgParse(parser);
}


MGNode* mgParseFileHandle(MGParser *parser, FILE *file)
{
	if (!mgTokenizeFileHandle(&parser->tokenizer, file, NULL))
		return NULL;

	return _mgParse(parser);
}


MGNode* mgParseString(MGParser *parser, const char *string)
{
	if (!mgTokenizeString(&parser->tokenizer, string, NULL))
		return NULL;

	return _mgParse(parser);
}
