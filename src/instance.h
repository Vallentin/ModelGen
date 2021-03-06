#ifndef MODELGEN_INSTANCE_H
#define MODELGEN_INSTANCE_H

#include "value.h"
#include "frame.h"

typedef float MGVertex[3 + 3];

typedef struct MGInstance {
	MGStackFrame *callStackTop;
	_MGList(char*) path;
	MGValue *modules;
	MGValue *staticModules;
	const MGValue *base;
	MGValue *uniforms;
	_MGList(MGVertex) vertices;
	struct {
		unsigned int position : 3;
		unsigned int uv : 2;
		unsigned int normal : 3;
		unsigned int color : 3;
	} vertexSize;
} MGInstance;

#define mgInstanceGetVertexSize(instance) ((instance)->vertexSize.position + (instance)->vertexSize.uv + (instance)->vertexSize.normal + (instance)->vertexSize.color)

void mgCreateInstance(MGInstance *instance);
void mgDestroyInstance(MGInstance *instance);

void mgPushStackFrame(MGInstance *instance, MGStackFrame *frame);
void mgPopStackFrame(MGInstance *instance, MGStackFrame *frame);

void mgRunFile(MGInstance *instance, const char *filename, const char *name);
void mgRunFileHandle(MGInstance *instance, FILE *file, const char *name);
void mgRunString(MGInstance *instance, const char *string, const char *name);

MGValue* mgImportModule(MGInstance *instance, const char *name);

#endif