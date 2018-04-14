
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>

#include "../modelgen.h"
#include "../module.h"
#include "../inspect.h"
#include "../utilities.h"


static inline void _mgFail(const char *format, ...)
{
	fflush(stdout);

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	putc('\n', stderr);
	fflush(stderr);

	exit(1);
}

#define MG_FAIL(...) _mgFail(__VA_ARGS__)


static MGValue* mg_print(MGInstance *instance, size_t argc, MGValue **argv)
{
	for (size_t i = 0; i < argc; ++i)
	{
		if (i > 0)
			putchar(' ');

		const MGValue *value = argv[i];

		if (value->type != MG_VALUE_STRING)
			mgInspectValueEx(argv[i], MG_FALSE);
		else if (value->data.str.s)
			fputs(value->data.str.s, stdout);
	}

	putchar('\n');

	return mgCreateValueVoid();
}


MGValue* _mg_rangei(int start, int stop, int step)
{
	int difference = stop - start;

	if (difference == 0)
		return mgCreateValueTuple(0);

	if (step == 0)
		step = (difference > 0) - (difference < 0);

	int length = difference / step + ((difference % step) != 0);

	if ((difference ^ step) < 0)
		return mgCreateValueTuple(0);

	MGValue *value = mgCreateValueTuple((size_t) length);
	for (int i = 0; i < length; ++i)
		mgTupleAdd(value, mgCreateValueInteger(start + step * i));

	return value;
}


static MGValue* mg_range(MGInstance *instance, size_t argc, MGValue **argv)
{
	if (argc < 1)
		MG_FAIL("Error: range expected at least 1 argument, received %zu", argc);
	else if (argc > 3)
		MG_FAIL("Error: range expected at most 3 arguments, received %zu", argc);

	for (size_t i = 0; i < argc; ++i)
		if (argv[i]->type != MG_VALUE_INTEGER)
			MG_FAIL("Error: rangef expected argument %zu as \"%s\", received \"%s\"",
			        i + 1, _MG_VALUE_TYPE_NAMES[MG_VALUE_INTEGER], _MG_VALUE_TYPE_NAMES[argv[i]->type]);

	int range[3] = { 0, 0, 0 };

	if (argc > 1)
		for (size_t i = 0; i < argc; ++i)
			range[i] = argv[i]->data.i;
	else
		range[1] = argv[0]->data.i;

	if ((argc > 2) && (range[2] == 0))
		MG_FAIL("Error: step cannot be 0");

	return _mg_rangei(range[0], range[1], range[2]);
}


static MGValue* mg_type(MGInstance *instance, size_t argc, MGValue **argv)
{
	if (argc != 1)
		MG_FAIL("Error: type expects exactly 1 argument, received %zu", argc);

	return mgCreateValueString(_MG_VALUE_TYPE_NAMES[argv[0]->type]);
}


static MGValue* mg_len(MGInstance *instance, size_t argc, MGValue **argv)
{
	if (argc != 1)
		MG_FAIL("Error: len expects exactly 1 argument, received %zu", argc);

	switch (argv[0]->type)
	{
	case MG_VALUE_TUPLE:
	case MG_VALUE_LIST:
		return mgCreateValueInteger((int) mgListLength(argv[0]));
	case MG_VALUE_MAP:
		return mgCreateValueInteger((int) mgMapSize(argv[0]));
	case MG_VALUE_STRING:
		return mgCreateValueInteger((int) mgStringLength(argv[0]));
	default:
		MG_FAIL("Error: \"%s\" has no length", _MG_VALUE_TYPE_NAMES[argv[0]->type]);
	}

	return mgCreateValueVoid();
}


static MGValue* mg_traceback(MGInstance *instance, size_t argc, MGValue **argv)
{
	MG_ASSERT(instance);
	MG_ASSERT(instance->callStackTop);

	const MGStackFrame *frame = instance->callStackTop;

	while (frame->last)
		frame = frame->last;

	size_t depth = 0;

	while (frame)
	{
		printf("%zu: ", depth);
		mgInspectStackFrame(frame);
		putchar('\n');

		frame = frame->next;
		++depth;
	}

	return mgCreateValueVoid();
}


static MGValue* mg_globals(MGInstance *instance, size_t argc, MGValue **argv)
{
	MG_ASSERT(instance);
	MG_ASSERT(instance->callStackTop);
	MG_ASSERT(instance->callStackTop->module);
	MG_ASSERT(instance->callStackTop->module->type == MG_VALUE_MODULE);
	MG_ASSERT(instance->callStackTop->module->data.module.globals);

	return mgReferenceValue(instance->callStackTop->module->data.module.globals);
}


static MGValue* mg_locals(MGInstance *instance, size_t argc, MGValue **argv)
{
	MG_ASSERT(instance);
	MG_ASSERT(instance->callStackTop);
	MG_ASSERT(instance->callStackTop->last);
	MG_ASSERT(instance->callStackTop->last->locals);

	return mgReferenceValue(instance->callStackTop->last->locals);
}


static MGValue* mg_import(MGInstance *instance, size_t argc, MGValue **argv)
{
	MG_ASSERT(instance);

	MGValue *modules = mgCreateValueTuple(argc);

	for (size_t i = 0; i < argc; ++i)
	{
		if (argv[i]->type != MG_VALUE_STRING)
			MG_FAIL("Error: import expected argument %zu as \"%s\", received \"%s\"",
			        i + 1, _MG_VALUE_TYPE_NAMES[MG_VALUE_STRING], _MG_VALUE_TYPE_NAMES[argv[i]->type]);

		mgTupleAdd(modules, mgImportModule(instance, argv[i]->data.str.s));
	}

	if (mgTupleLength(modules) == 1)
	{
		MGValue *importedModule = mgReferenceValue(mgTupleGet(modules, 0));
		mgDestroyValue(modules);
		return importedModule;
	}

	return modules;
}


MGValue* mgCreateBaseLib(void)
{
	MGValue *module = mgCreateValueModule();

	MG_ASSERT(module);
	MG_ASSERT(module->type == MG_VALUE_MODULE);

	mgModuleSetInteger(module, "false", 0);
	mgModuleSetInteger(module, "true", 1);

	MGValue *version = mgCreateValueTuple(3);
	mgTupleAdd(version, mgCreateValueInteger(MG_MAJOR_VERSION));
	mgTupleAdd(version, mgCreateValueInteger(MG_MINOR_VERSION));
	mgTupleAdd(version, mgCreateValueInteger(MG_PATCH_VERSION));
	mgModuleSet(module, "version", version);

	mgModuleSetCFunction(module, "print", mg_print);
	mgModuleSetCFunction(module, "range", mg_range);
	mgModuleSetCFunction(module, "type", mg_type);
	mgModuleSetCFunction(module, "len", mg_len);
	mgModuleSetCFunction(module, "traceback", mg_traceback);
	mgModuleSetCFunction(module, "globals", mg_globals);
	mgModuleSetCFunction(module, "locals", mg_locals);

	mgModuleSetCFunction(module, "__import", mg_import);

	return module;
}
