#ifndef MODELGEN_H
#define MODELGEN_H

#include <stddef.h>

#include "debug.h"

#include "tokens.h"
#include "ast.h"
#include "value.h"
#include "frame.h"
#include "collections.h"

#include <stdio.h>

#define MG_MAJOR_VERSION 0
#define MG_MINOR_VERSION 1
#define MG_PATCH_VERSION 0

#define MG_VERSION "0.1.0"

#define MG_FALSE 0
#define MG_TRUE 1

typedef unsigned char MGbool;

typedef struct MGToken {
	MGTokenType type;
	struct {
		const char *string;
		unsigned int line;
		unsigned int character;
	} begin;
	struct {
		const char *string;
		unsigned int line;
		unsigned int character;
	} end;
	union {
		int i;
		float f;
		char *s;
	} value;
} MGToken;

typedef struct MGTokenizer {
	const char *filename;
	char *string;
	_MGList(MGToken) tokens;
} MGTokenizer;

typedef struct MGNode MGNode;

typedef struct MGNode {
	MGNodeType type;
	MGToken *token;
	MGToken *tokenBegin;
	MGToken *tokenEnd;
	_MGList(MGNode*) children;
	MGNode *parent;
} MGNode;

typedef struct MGParser {
	MGTokenizer tokenizer;
	MGNode *root;
} MGParser;

typedef struct MGValue MGValue;

typedef _MGList(MGValue*) MGValueList;

typedef _MGPair(char*, MGValue*) MGValueMapPair;
typedef _MGList(MGValueMapPair) MGValueMap;

typedef struct MGModule MGModule;

typedef MGValue* (*MGCFunction)(MGModule *module, size_t argc, MGValue **argv);

typedef struct MGValue {
	MGValueType type;
	size_t refCount;
	union {
		int i;
		float f;
		char *s;
		MGCFunction cfunc;
		MGValueList a;
		MGValueMap m;
		MGNode *func;
	} data;
} MGValue;

typedef struct MGInstance MGInstance;

typedef struct MGModule {
	MGInstance *instance;
	MGParser parser;
	char *filename;
	MGValue *globals;
} MGModule;

typedef struct MGStackFrame MGStackFrame;

typedef struct MGStackFrame {
	MGStackFrameState state;
	MGStackFrame *last;
	MGStackFrame *next;
	const MGModule *module;
	const MGNode *caller;
	const char *callerName;
	MGValue *value;
} MGStackFrame;

typedef _MGPair(char*, MGModule) MGNameModule;

typedef float MGVertex[3];

typedef struct MGInstance {
	_MGList(MGNameModule) modules;
	MGStackFrame *callStackTop;
	_MGList(MGVertex) vertices;
} MGInstance;

char* mgReadFile(const char *filename, size_t *length);
char* mgReadFileHandle(FILE *file, size_t *length);

void mgTokenReset(const char *string, MGToken *token);
void mgTokenizeNext(MGToken *token);

void mgCreateTokenizer(MGTokenizer *tokenizer);
void mgDestroyTokenizer(MGTokenizer *tokenizer);

MGToken* mgTokenizeFile(MGTokenizer *tokenizer, const char *filename, size_t *tokenCount);
MGToken* mgTokenizeFileHandle(MGTokenizer *tokenizer, FILE *file, size_t *tokenCount);
MGToken* mgTokenizeString(MGTokenizer *tokenizer, const char *string, size_t *tokenCount);

void mgCreateParser(MGParser *parser);
void mgDestroyParser(MGParser *parser);

MGNode* mgCreateNode(MGToken *token);
void mgDestroyNode(MGNode *node);

MGNode* mgParse(MGParser *parser);
MGNode* mgParseFile(MGParser *parser, const char *filename);
MGNode* mgParseFileHandle(MGParser *parser, FILE *file);
MGNode* mgParseString(MGParser *parser, const char *string);

MGValue* mgInterpret(MGModule *module);
MGValue* mgInterpretFile(MGModule *module, const char *filename);
MGValue* mgInterpretFileHandle(MGModule *module, FILE *file, const char *filename);
MGValue* mgInterpretString(MGModule *module, const char *string, const char *filename);

void mgCreateInstance(MGInstance *instance);
void mgDestroyInstance(MGInstance *instance);

void mgCreateStackFrame(MGStackFrame *frame);
void mgDestroyStackFrame(MGStackFrame *frame);

void mgPushStackFrame(MGInstance *instance, MGStackFrame *frame);
void mgPopStackFrame(MGInstance *instance, MGStackFrame *frame);

void mgRunFile(MGInstance *instance, const char *filename, const char *name);
void mgRunFileHandle(MGInstance *instance, FILE *file, const char *name);
void mgRunString(MGInstance *instance, const char *string, const char *name);

void mgImportModule(MGInstance *instance, const char *name);

#endif