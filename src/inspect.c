
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "inspect.h"
#include "utilities.h"


#define _MG_FILENAME_PADDING 4
#define _MG_NODE_PADDING 40

#define _MG_NODE_INDENT            "|- "
#define _MG_NODE_INDENT_LAST       "`- "
#define _MG_NODE_CHILD_INDENT      "|  "
#define _MG_NODE_CHILD_INDENT_LAST "   "
#define _MG_NODE_INDENT_LENGTH     3


void mgInspectToken(const MGToken *token, const char *filename, MGbool justify)
{
	const unsigned int len = token->end.string - token->begin.string;

	char *string2 = NULL;

	if (token->type == MG_TOKEN_STRING)
	{
		if (token->value.s)
		{
			string2 = (char*) malloc((mgInlineRepresentationLength(token->value.s, NULL) + 1) * sizeof(char));
			mgInlineRepresentation(string2, token->value.s, NULL);
		}
	}
	else if (len)
	{
		string2 = (char*) malloc((mgInlineRepresentationLength(token->begin.string, token->end.string) + 1) * sizeof(char));
		mgInlineRepresentation(string2, token->begin.string, token->end.string);
	}

	if (filename)
		printf("%s:", filename);

	if (justify)
	{
		int padding = _MG_INT_COUNT_DIGITS(token->begin.line) + _MG_INT_COUNT_DIGITS(token->begin.character);
		padding = (padding > _MG_FILENAME_PADDING) ? 0 : (_MG_FILENAME_PADDING + 1 - padding);

		printf("%u:%u:%*s %-*s \"%s\"\n",
		       token->begin.line, token->begin.character,
		       padding, "",
		       _MG_LONGEST_TOKEN_NAME_LENGTH, _MG_TOKEN_NAMES[token->type],
		       string2 ? string2 : "");
	}
	else
	{
		printf("%u:%u: %s \"%s\"\n",
		       token->begin.line, token->begin.character,
		       _MG_TOKEN_NAMES[token->type],
		       string2 ? string2 : "");
	}

	free(string2);
}


static void _mgInspectNode(const MGNode *node, char *prefix, char *prefixEnd, MGbool isLast)
{
	int width = 0;

	if (isLast)
		width += printf("%s" _MG_NODE_INDENT_LAST, prefix);
	else
		width += printf("%s" _MG_NODE_INDENT, prefix);

	width += printf("%s", _MG_NODE_NAMES[node->type]);

	if (node->token)
	{
		switch (node->token->type)
		{
		case MG_TOKEN_IDENTIFIER:
		case MG_TOKEN_INTEGER:
		case MG_TOKEN_FLOAT:
		case MG_TOKEN_STRING:
		case MG_TOKEN_ADD:
		case MG_TOKEN_SUB:
		case MG_TOKEN_MUL:
		case MG_TOKEN_DIV:
		case MG_TOKEN_MOD:
		case MG_TOKEN_ASSIGN:
		case MG_TOKEN_ADD_ASSIGN:
		case MG_TOKEN_SUB_ASSIGN:
		case MG_TOKEN_MUL_ASSIGN:
		case MG_TOKEN_MOD_ASSIGN:
		case MG_TOKEN_EQUAL:
		case MG_TOKEN_NOT_EQUAL:
		case MG_TOKEN_LESS:
		case MG_TOKEN_LESS_EQUAL:
		case MG_TOKEN_GREATER:
		case MG_TOKEN_GREATER_EQUAL:
		case MG_TOKEN_NOT:
		case MG_TOKEN_AND:
		case MG_TOKEN_OR:
			width += printf(" %.*s", (int) (node->token->end.string - node->token->begin.string), node->token->begin.string);
			break;
		default:
			break;
		}
	}

	if (isLast)
		strcpy(prefixEnd, _MG_NODE_CHILD_INDENT_LAST);
	else
		strcpy(prefixEnd, _MG_NODE_CHILD_INDENT);

#if MG_ANSI_COLORS
	fputs("\e[90m", stdout);
#endif

	if (width < _MG_NODE_PADDING)
		printf("%*s", _MG_NODE_PADDING - width, "");

	if (node->token && (node->tokenBegin == node->tokenEnd))
		mgInspectToken(node->token, NULL, MG_FALSE);
	else if (node->tokenBegin && node->tokenEnd)
		printf("%u:%u->%u:%u\n",
		       node->tokenBegin->begin.line, node->tokenBegin->begin.character,
		       node->tokenEnd->end.line, node->tokenEnd->end.character);
	else
		putchar('\n');

#if MG_DEBUG_SHOW_RANGE
	if (node->tokenBegin != node->tokenEnd)
	{
		printf("%s" _MG_NODE_INDENT, prefix);
		mgDebugInspectToken(node->tokenBegin, NULL, MG_FALSE);
		printf("%s%s", prefix, node->childCount ? _MG_NODE_CHILD_INDENT : _MG_NODE_INDENT_LAST);
		mgDebugInspectToken(node->tokenEnd, NULL, MG_FALSE);
	}
#endif

#if MG_ANSI_COLORS
	fputs("\e[0m", stdout);
#endif

	for (size_t i = 0; i < node->childCount; ++i)
	{
		if (isLast)
			strcpy(prefixEnd, _MG_NODE_CHILD_INDENT_LAST);
		else
			strcpy(prefixEnd, _MG_NODE_CHILD_INDENT);

		_mgInspectNode(node->children[i], prefix, prefixEnd + _MG_NODE_INDENT_LENGTH, (MGbool) (i == (node->childCount - 1)));
	}
}


void mgInspectNode(const MGNode *node)
{
	// Warning: If the height exceeds 341 nodes then we're in a world of trouble
	// TODO: Check the node's height and allocate ((height * _MG_NODE_INDENT_LENGTH + 1) * sizeof(char))
	char prefix[1024];
	prefix[0] = '\0';

	_mgInspectNode(node, prefix, prefix, MG_TRUE);
}


void _mgInspectValue(const MGValue *value)
{
	switch (value->type)
	{
	case MG_VALUE_INTEGER:
		printf("%d", value->data.i);
		break;
	case MG_VALUE_FLOAT:
		printf("%f", value->data.f);
		break;
	case MG_VALUE_STRING:
	{
		char *str = (char*) malloc((mgInlineRepresentationLength(value->data.s, NULL) + 1) * sizeof(char));
		mgInlineRepresentation(str, value->data.s, NULL);
		printf("\"%s\"", str);
		free(str);
		break;
	}
	case MG_VALUE_TUPLE:
		putchar('(');
		for (size_t i = 0; i < value->data.a.length; ++i)
		{
			if (i > 0)
				fputs(", ", stdout);
			_mgInspectValue(value->data.a.items[i]);
		}
		if (value->data.a.length == 1)
			putchar(',');
		putchar(')');
		break;
	case MG_VALUE_CFUNCTION:
		printf("%p", value->data.cfunc);
		break;
	default:
		break;
	}
}


void mgInspectValue(const MGValue *value)
{
	_mgInspectValue(value);
	putchar('\n');
}


static void _mgInspectName(MGName *name)
{
	printf("%s: %s = ", name->name, _MG_VALUE_TYPE_NAMES[name->value->type]);
	mgInspectValue(name->value);
}


void mgInspectModule(const MGModule *module)
{
	if (module->filename)
		printf("Module [%zu:%zu] \"%s\"\n",
		       module->length, module->capacity, mgBasename(module->filename));
	else
		printf("Module [%zu:%zu]\n",
		       module->length, module->capacity);

	for (size_t i = 0; i < module->length; ++i)
		_mgInspectName(&module->names[i]);
}


void _mgDebugReadPrint(const char *filename, char *str, size_t len)
{
	if (str)
	{
		printf("Reading: %s\n", filename);

		unsigned int lineCount = 0;

		if (len)
		{
			++lineCount;

			for (char *c = str; *c; ++c)
				if (*c == '\n')
					++lineCount;
		}

		printf("Length: %zu\n", len);
		printf("Lines: %u\n", lineCount);

		if (lineCount > 0)
		{
			char *currentLine = str;
			unsigned int line = 1;

			while (currentLine)
			{
				char *nextLine = strchr(currentLine, '\n');

				if (nextLine)
					*nextLine = '\0';

				printf("%*u: %s\n", _MG_INT_COUNT_DIGITS(lineCount), line, currentLine);

				if (nextLine)
					*nextLine = '\n';

				currentLine = nextLine ? (nextLine + 1) : NULL;
				++line;
			}
		}
	}
	else
	{
		fprintf(stderr, "Failed Reading: %s\n", filename);
	}
}


void _mgDebugTokenizePrint(const char *filename, char *str, size_t len)
{
	if (str)
	{
		printf("Tokenizing: %s\n", filename);

		MGToken token;
		mgTokenReset(str, &token);

		filename = mgBasename(filename);

		do
		{
			mgTokenizeNext(&token);
			mgInspectToken(&token, filename, MG_TRUE);
		}
		while (token.type != MG_TOKEN_EOF);
	}
	else
	{
		fprintf(stderr, "Failed Reading: %s\n", filename);
	}
}


MGbool mgDebugRead(const char *filename)
{
	size_t len;
	char *str = mgReadFile(filename, &len);

	_mgDebugReadPrint(filename, str, len);

	free(str);

	return str ? MG_TRUE : MG_FALSE;
}


MGbool mgDebugReadHandle(FILE *file, const char *filename)
{
	size_t len;
	char *str = mgReadFileHandle(file, &len);

	_mgDebugReadPrint(filename, str, len);

	free(str);

	return str ? MG_TRUE : MG_FALSE;
}


MGbool mgDebugTokenize(const char *filename)
{
	size_t len;
	char *str = mgReadFile(filename, &len);

	_mgDebugTokenizePrint(filename, str, len);

	free(str);

	return str ? MG_TRUE : MG_FALSE;
}


MGbool mgDebugTokenizeHandle(FILE *file, const char *filename)
{
	size_t len;
	char *str = mgReadFileHandle(file, &len);

	_mgDebugTokenizePrint(filename, str, len);

	free(str);

	return str ? MG_TRUE : MG_FALSE;
}
