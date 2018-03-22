
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "modelgen.h"
#include "module.h"
#include "inspect.h"
#include "utilities.h"


static inline void _mgFail(MGModule *module, MGNode *node, const char *format, ...)
{
	fflush(stdout);

	if (module->filename)
		fprintf(stderr, "%s:", module->filename);
	if (node && node->tokenBegin)
		fprintf(stderr, "%u:%u: ", node->tokenBegin->begin.line, node->tokenBegin->begin.character);

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	putc('\n', stderr);
	fflush(stderr);

	exit(1);
}

#define MG_FAIL(...) _mgFail(module, node, __VA_ARGS__)


static MGValue* _mgDeepCopyValue(MGValue *value)
{
	MG_ASSERT(value);

	MGValue *copy = (MGValue*) malloc(sizeof(MGValue));
	memcpy(copy, value, sizeof(MGValue));

	switch (copy->type)
	{
	case MG_VALUE_STRING:
		copy->data.s = mgDuplicateString(value->data.s);
		break;
	case MG_VALUE_TUPLE:
	case MG_VALUE_LIST:
		if (value->data.a.length)
		{
			copy->data.a.items = (MGValue**) malloc(value->data.a.length * sizeof(MGValue*));
			for (size_t i = 0; i < value->data.a.length; ++i)
				copy->data.a.items[i] = _mgDeepCopyValue(value->data.a.items[i]);
		}
		break;
	default:
		break;
	}

	return copy;
}


static MGValue* _mgVisitNode(MGModule *module, MGNode *node);


static MGValue* _mgVisitChildren(MGModule *module, MGNode *node)
{
	MGValue *value = mgCreateValueTuple(node->childCount);

	for (size_t i = 0; i < node->childCount; ++i)
		mgTupleAdd(value, _mgVisitNode(module, node->children[i]));

	return value;
}


static inline MGValue* _mgVisitModule(MGModule *module, MGNode *node)
{
	return _mgVisitChildren(module, node);
}


static MGValue* _mgVisitCall(MGModule *module, MGNode *node)
{
	MG_ASSERT(node->childCount > 0);

	MGNode *nameNode = node->children[0];

	MGValue *func = NULL;
	char *name;

	if (nameNode->type == MG_NODE_PROCEDURE)
	{
		func = _mgVisitNode(module, nameNode);
		MG_ASSERT(func);

		name = mgDuplicateString("<anonymous>");
	}
	else
	{
		MG_ASSERT(nameNode->type == MG_NODE_IDENTIFIER);
		MG_ASSERT(nameNode->token);

		const size_t nameLength = nameNode->token->end.string - nameNode->token->begin.string;
		MG_ASSERT(nameLength > 0);
		name = mgDuplicateFixedString(nameNode->token->begin.string, nameLength);

		func = mgModuleGet(module, name);

		if (!func)
			MG_FAIL("Error: Undefined name \"%s\"", name);
	}

	if ((func->type != MG_VALUE_CFUNCTION) && (func->type != MG_VALUE_PROCEDURE))
		MG_FAIL("Error: \"%s\" is not callable", _MG_VALUE_TYPE_NAMES[func->type]);

	size_t argc = node->childCount - 1;
	MGValue **argv = (MGValue**) malloc(argc * sizeof(MGValue*));

	for (size_t i = 0; i < argc; ++i)
	{
		argv[i] = _mgVisitNode(module, node->children[i + 1]);
		MG_ASSERT(argv[i]);
	}

	MGValue *value = NULL;

	if (func->type == MG_VALUE_CFUNCTION)
	{
		value = func->data.cfunc(argc, argv);
	}
	else
	{
		MGNode *procNode = func->data.func;

		if (procNode)
		{
			MG_ASSERT(procNode->type == MG_NODE_PROCEDURE);
			MG_ASSERT((procNode->childCount == 2) || (procNode->childCount == 3));

			MGNode *procParametersNode = procNode->children[1];
			MG_ASSERT(procParametersNode->type == MG_NODE_TUPLE);

			if (procParametersNode->childCount < argc)
			{
				MGNode *procNameNode = procNode->children[0];
				MG_ASSERT(procNameNode->type == MG_NODE_IDENTIFIER);
				MG_ASSERT(procNameNode->token);

				const size_t procNameLength = procNameNode->token->end.string - procNameNode->token->begin.string;
				MG_ASSERT(procNameLength > 0);
				char *procName = mgDuplicateFixedString(procNameNode->token->begin.string, procNameLength);
				MG_ASSERT(procName);

				MG_FAIL("Error: %s expected at most %zu arguments, received %zu", procName, procParametersNode->childCount, argc);

				free(procName);
			}

			for (size_t i = 0; i < procParametersNode->childCount; ++i)
			{
				MGNode *procParameterNode = procParametersNode->children[i];
				MG_ASSERT((procParameterNode->type == MG_NODE_IDENTIFIER) || (procParameterNode->type == MG_NODE_BIN_OP));

				char *procParameterName = NULL;

				if (procParameterNode->type == MG_NODE_IDENTIFIER)
				{
					const size_t procParameterNameLength = procParameterNode->token->end.string - procParameterNode->token->begin.string;
					MG_ASSERT(procParameterNameLength > 0);
					procParameterName = mgDuplicateFixedString(procParameterNode->token->begin.string, procParameterNameLength);
				}
				else if (procParameterNode->type == MG_NODE_BIN_OP)
				{
					MG_ASSERT(procParameterNode->childCount == 2);
					MG_ASSERT(procParameterNode->token);

					const size_t opLength = procParameterNode->token->end.string - procParameterNode->token->begin.string;
					MG_ASSERT(opLength > 0);
					char *op = mgDuplicateFixedString(procParameterNode->token->begin.string, opLength);
					MG_ASSERT(op);

					MG_ASSERT(strcmp(op, "=") == 0);

					free(op);

					MGNode *procParameterNameNode = procParameterNode->children[0];
					MG_ASSERT(procParameterNameNode->type == MG_NODE_IDENTIFIER);
					MG_ASSERT(procParameterNameNode->token);

					const size_t procParameterNameLength = procParameterNameNode->token->end.string - procParameterNameNode->token->begin.string;
					MG_ASSERT(procParameterNameLength > 0);
					procParameterName = mgDuplicateFixedString(procParameterNameNode->token->begin.string, procParameterNameLength);
				}

				MG_ASSERT(procParameterName);

				if (i < argc)
					mgModuleSet(module, procParameterName, _mgDeepCopyValue(argv[i]));
				else
				{
					if (procParameterNode->type != MG_NODE_BIN_OP)
						MG_FAIL("Error: Expected argument \"%s\"", procParameterName);

					mgModuleSet(module, procParameterName, _mgVisitNode(module, procParameterNode->children[1]));
				}

				free(procParameterName);
			}

			if (procNode->childCount == 3)
				value = _mgVisitNode(module, procNode->children[2]);
		}
		else
		{
			value = mgCreateValue(MG_VALUE_INTEGER);
			value->data.i = 0;
		}
	}

	for (size_t i = 0; i < argc; ++i)
		mgDestroyValue(argv[i]);
	free(argv);

	free(name);

	return value;
}


static MGValue* _mgVisitFor(MGModule *module, MGNode *node)
{
	MG_ASSERT(node->childCount >= 2);

	MGNode *nameNode = node->children[0];
	MG_ASSERT(nameNode->type == MG_NODE_IDENTIFIER);
	MG_ASSERT(nameNode->token);

	const size_t nameLength = nameNode->token->end.string - nameNode->token->begin.string;
	MG_ASSERT(nameLength > 0);
	char *name = mgDuplicateFixedString(nameNode->token->begin.string, nameLength);
	MG_ASSERT(name);

	MGValue *test = _mgVisitNode(module, node->children[1]);
	MG_ASSERT(test);
	MG_ASSERT(test->type == MG_VALUE_TUPLE);

	int iterations = 0;

	for (size_t i = 0; i < test->data.a.length; ++i, ++iterations)
	{
		MGValue *value = test->data.a.items[i];
		MG_ASSERT(value);

		mgModuleSet(module, name, _mgDeepCopyValue(value));

		for (size_t j = 2; j < node->childCount; ++j)
		{
			MGValue *result = _mgVisitNode(module, node->children[j]);
			MG_ASSERT(result);
			mgDestroyValue(result);
		}
	}

	mgModuleSet(module, name, NULL);

	mgDestroyValue(test);

	free(name);

	MGValue *value = mgCreateValue(MG_VALUE_INTEGER);
	value->data.i = iterations;

	return value;
}


static MGValue* _mgVisitIf(MGModule *module, MGNode *node)
{
	MG_ASSERT(node->childCount >= 2);

	MGValue *condition = _mgVisitNode(module, node->children[0]);
	MG_ASSERT(condition);

	int _condition = 0;

	switch (condition->type)
	{
	case MG_VALUE_INTEGER:
		_condition = condition->data.i != 0;
		break;
	case MG_VALUE_FLOAT:
#define _MG_EPSILON 1E-6f
#define _MG_FEQUAL(x, y) ((((y) - _MG_EPSILON) < (x)) && ((x) < ((y) + _MG_EPSILON)))
		_condition = !_MG_FEQUAL(condition->data.f, 0.0f);
		break;
#undef _MG_EPSILON
#undef _MG_FEQUAL
	case MG_VALUE_STRING:
		_condition = condition->data.s ? (int) strlen(condition->data.s) : 0;
		break;
	case MG_VALUE_TUPLE:
		_condition = condition->data.a.length > 0;
		break;
	default:
		_condition = 1;
		break;
	}

	if (_condition)
		return _mgVisitNode(module, node->children[1]);
	else if (node->childCount > 2)
		return _mgVisitNode(module, node->children[2]);

	MGValue *value = mgCreateValue(MG_VALUE_INTEGER);
	value->data.i = _condition ? MG_TRUE : MG_FALSE;

	return value;
}


static MGValue* _mgVisitProcedure(MGModule *module, MGNode *node)
{
	MG_ASSERT((node->childCount == 2) || (node->childCount == 3));

	MGNode *nameNode = node->children[0];
	MG_ASSERT((nameNode->type == MG_NODE_INVALID) || (nameNode->type == MG_NODE_IDENTIFIER));

	MGValue *proc = mgCreateValue(MG_VALUE_PROCEDURE);
	proc->data.func = node;

	if (nameNode->type == MG_NODE_INVALID)
		return proc;

	MG_ASSERT(nameNode->type == MG_NODE_IDENTIFIER);
	MG_ASSERT(nameNode->token);

	const size_t nameLength = nameNode->token->end.string - nameNode->token->begin.string;
	MG_ASSERT(nameLength > 0);
	char *name = mgDuplicateFixedString(nameNode->token->begin.string, nameLength);
	MG_ASSERT(name);

	mgModuleSet(module, name, proc);

	free(name);

	return _mgDeepCopyValue(proc);
}


static MGValue* _mgVisitIdentifier(MGModule *module, MGNode *node)
{
	MG_ASSERT(node->token);

	const size_t nameLength = node->token->end.string - node->token->begin.string;
	MG_ASSERT(nameLength > 0);
	char *name = mgDuplicateFixedString(node->token->begin.string, nameLength);
	MG_ASSERT(name);

	MGValue *value = mgModuleGet(module, name);

	if (!value)
		MG_FAIL("Error: Undefined name \"%s\"", name);

	free(name);

	return _mgDeepCopyValue(value);
}


static MGValue* _mgVisitInteger(MGModule *module, MGNode *node)
{
	MG_ASSERT(node->token);

	const size_t _valueLength = node->token->end.string - node->token->begin.string;
	MG_ASSERT(_valueLength > 0);
	char *_value = mgDuplicateFixedString(node->token->begin.string, _valueLength);
	MG_ASSERT(_value);

	MGValue *value = mgCreateValueInteger(strtol(_value, NULL, 10));

	free(_value);

	return value;
}


static MGValue* _mgVisitFloat(MGModule *module, MGNode *node)
{
	MG_ASSERT(node->token);

	const size_t _valueLength = node->token->end.string - node->token->begin.string;
	MG_ASSERT(_valueLength > 0);
	char *_value = mgDuplicateFixedString(node->token->begin.string, _valueLength);
	MG_ASSERT(_value);

	MGValue *value = mgCreateValueFloat(strtof(_value, NULL));

	free(_value);

	return value;
}


static MGValue* _mgVisitString(MGModule *module, MGNode *node)
{
	MG_ASSERT(node->token);

	return mgCreateValueString(node->token->value.s);
}


static MGValue* _mgVisitTuple(MGModule *module, MGNode *node)
{
	MG_ASSERT(node->token);
	MG_ASSERT((node->type == MG_NODE_TUPLE) || (node->type == MG_NODE_LIST));

	MGValue *value = mgCreateValueTuple(node->childCount);
	value->type = (node->type == MG_NODE_TUPLE) ? MG_VALUE_TUPLE : MG_VALUE_LIST;

	for (size_t i = 0; i < node->childCount; ++i)
		mgTupleAdd(value, _mgVisitNode(module, node->children[i]));

	return value;
}


static MGValue* _mgVisitRange(MGModule *module, MGNode *node)
{
	MG_ASSERT((node->childCount == 2) || (node->childCount == 3));

	int range[3] = { 0, 0, 1 };

	for (size_t i = 0; i < node->childCount; ++i)
	{
		MGNode *child = node->children[i];

		MG_ASSERT(child);
		MG_ASSERT(child->type == MG_NODE_INTEGER);

		MG_ASSERT(child->token);

		const size_t _valueLength = child->token->end.string - child->token->begin.string;
		MG_ASSERT(_valueLength > 0);
		char *_value = mgDuplicateFixedString(child->token->begin.string, _valueLength);
		MG_ASSERT(_value);

		range[i] = strtol(_value, NULL, 10);

		free(_value);
	}

	MG_ASSERT(range[2] > 0);

	int length = (range[1] - range[0]) / range[2] + (((range[1] - range[0]) % range[2]) != 0);
	MG_ASSERT(length >= 0);

	MGValue *value = mgCreateValueTuple((size_t) length);
	for (int i = 0; i < length; ++i)
		mgTupleAdd(value, mgCreateValueInteger(range[0] + range[2] * i));

	return value;
}


static inline MGValue* _mgVisitAssignment(MGModule *module, MGNode *node)
{
	MG_ASSERT(node->childCount == 2);

	MGNode *nameNode = node->children[0];
	MG_ASSERT(nameNode->type == MG_NODE_IDENTIFIER);
	MG_ASSERT(nameNode->token);

	const size_t nameLength = nameNode->token->end.string - nameNode->token->begin.string;
	MG_ASSERT(nameLength > 0);
	char *name = mgDuplicateFixedString(nameNode->token->begin.string, nameLength);
	MG_ASSERT(name);

	MGValue *value = _mgVisitNode(module, node->children[1]);
	mgModuleSet(module, name, value);

	free(name);

	return _mgDeepCopyValue(value);
}


static MGValue* _mgVisitBinOp(MGModule *module, MGNode *node)
{
	MG_ASSERT(node->childCount == 2);
	MG_ASSERT(node->token);

	const size_t opLength = node->token->end.string - node->token->begin.string;
	MG_ASSERT(opLength > 0);
	char *op = mgDuplicateFixedString(node->token->begin.string, opLength);
	MG_ASSERT(op);

	MGValue *value = NULL;

	if (strcmp(op, "=") == 0)
		value = _mgVisitAssignment(module, node);
	else
		MG_FAIL("Error: Unknown BinOp \"%s\"", op);

	free(op);

	return value;
}


static MGValue* _mgVisitNode(MGModule *module, MGNode *node)
{
	switch (node->type)
	{
	case MG_NODE_MODULE:
		return _mgVisitModule(module, node);
	case MG_NODE_BLOCK:
		return _mgVisitChildren(module, node);
	case MG_NODE_IDENTIFIER:
		return _mgVisitIdentifier(module, node);
	case MG_NODE_INTEGER:
		return _mgVisitInteger(module, node);
	case MG_NODE_FLOAT:
		return _mgVisitFloat(module, node);
	case MG_NODE_STRING:
		return _mgVisitString(module, node);
	case MG_NODE_TUPLE:
	case MG_NODE_LIST:
		return _mgVisitTuple(module, node);
	case MG_NODE_RANGE:
		return _mgVisitRange(module, node);
	case MG_NODE_BIN_OP:
		return _mgVisitBinOp(module, node);
	case MG_NODE_CALL:
		return _mgVisitCall(module, node);
	case MG_NODE_FOR:
		return _mgVisitFor(module, node);
	case MG_NODE_IF:
		return _mgVisitIf(module, node);
	case MG_NODE_PROCEDURE:
		return _mgVisitProcedure(module, node);
	default:
		MG_FAIL("Error: Unknown node \"%s\"", _MG_NODE_NAMES[node->type]);
	}

	return NULL;
}


MGValue* mgRunFile(MGModule *module, const char *filename)
{
	MG_ASSERT(module);
	MG_ASSERT(filename);

	module->filename = filename;

	MGValue *result = NULL;

	MGParser parser;
	mgCreateParser(&parser);

	if (mgParseFile(&parser, filename))
		result = _mgVisitNode(module, parser.root);

	mgDestroyParser(&parser);

	return result;
}


MGValue* mgRunFileHandle(MGModule *module, FILE *file)
{
	MG_ASSERT(module);
	MG_ASSERT(file);

	MGValue *result = NULL;

	MGParser parser;
	mgCreateParser(&parser);

	if (mgParseFileHandle(&parser, file))
		result = _mgVisitNode(module, parser.root);

	mgDestroyParser(&parser);

	return result;
}


MGValue* mgRunString(MGModule *module, const char *string)
{
	MG_ASSERT(module);
	MG_ASSERT(string);

	MGValue *result = NULL;

	MGParser parser;
	mgCreateParser(&parser);

	if (mgParseString(&parser, string))
		result = _mgVisitNode(module, parser.root);

	mgDestroyParser(&parser);

	return result;
}
