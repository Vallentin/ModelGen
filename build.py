#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os
from os.path import join
from itertools import repeat
from subprocess import run
import shutil
import re
from typing import List


_cflags = ["-std=c99" if os.name == "nt" else "-std=gnu99", "-Wall"]
_cflags.append("-DMG_ANSI_COLORS")
# _cflags.append("-DMG_DEBUG_SHOW_RANGE")
debug_cflags = _cflags + ["-DDEBUG", "-g", "-O0", "-Wno-unused-function", "-Wno-unused-variable", "-Wno-unused-but-set-variable"]
release_cflags = _cflags + ["-O3"]
cflags = release_cflags
ldflags = ["-lm"]


modelgen_dir = os.path.abspath(os.path.dirname(__file__))
modelgen_src_dir = join(modelgen_dir, "src")
modelgen_tests_dir = join(modelgen_dir, "tests")
modelgen_modules_dir = join(modelgen_dir, "modules")
modelgen_bin_dir = join(modelgen_dir, "bin")


def src_to_obj(src, bin_dir, build_name=""):
	relative = os.path.relpath(src, os.path.commonpath((src, bin_dir)))
	obj = join(bin_dir, build_name, relative)
	return os.path.splitext(obj)[0] + ".o"


def iter_files(dirname, filename_filter=lambda f: True):
	for dirpath, dirnames, filenames in os.walk(dirname):
		yield from filter(filename_filter, (join(dirpath, filename) for filename in sorted(filenames)))


def iter_c_files(*dirnames):
	for dirname in dirnames:
		yield from iter_files(dirname, lambda f: f.endswith(".c"))


def iter_h_files(*dirnames):
	for dirname in dirnames:
		yield from iter_files(dirname, lambda f: f.endswith(".h"))


def get_files(dirname, filename_filter=lambda f: True):
	return list(iter_files(dirname, filename_filter))


def get_c_files(*dirnames):
	return list(iter_c_files(*dirnames))


def get_h_files(*dirnames):
	return list(iter_h_files(*dirnames))


def resolve_include(filename: str, include_directories: List[str]):
	assert len(include_directories) > 0
	resolved_filename = filename
	for include_directory in include_directories:
		resolved_filename = join(include_directory, filename)
		if os.path.isfile(resolved_filename):
			return resolved_filename
	raise FileNotFoundError(resolved_filename)


def get_includes(filename: str, include_directories=[]):
	include_directories = [os.path.dirname(filename), *include_directories] # type: List[str]
	re_include = re.compile(r"^[ \t]*#[ \t]*include[ \t]*\"([^\"]+)\"[ \t]*$")
	includes = set()
	with open(filename) as f:
		for line in (line.rstrip() for line in f):
			m = re_include.match(line)
			if m:
				includes.add(m.group(1))
	return [resolve_include(include, include_directories) for include in sorted(includes)]


def has_changed(src, obj=None, include_directories=None):
	assert os.path.exists(src)

	last_modified = os.path.getmtime(src)
	if obj:
		if not os.path.exists(obj):
			return True
		obj_last_modified = os.path.getmtime(obj)
		if last_modified >= obj_last_modified:
			return True
		last_modified = obj_last_modified

	if not include_directories:
		return False

	checked_files = set()

	def _has_changed(src, include_directories):
		assert os.path.exists(src)
		if src in checked_files:
			return False
		checked_files.add(src)
		if last_modified <= os.path.getmtime(src):
			return True
		include_directories = [os.path.dirname(src), *include_directories]
		return any(map(_has_changed, get_includes(src, include_directories), repeat([os.path.dirname(src), *include_directories])))

	include_directories = [os.path.dirname(src), *include_directories]
	return any(map(_has_changed, get_includes(src, include_directories), repeat(include_directories)))


def has_entry_point(filename):
	re_main = re.compile(r"^[ \t]*(?:void|int)[ \t]+main[ \t]*\(")
	with open(filename) as f:
		for line in (line.rstrip() for line in f):
			if re_main.match(line):
				return True
	return False


def clean(dirname):
	try:
		print("Cleaning:", os.path.relpath(dirname))
		shutil.rmtree(dirname)
	except FileNotFoundError:
		pass


def gcc(*args):
	result = run(["gcc", *args], cwd=modelgen_dir)
	if result.returncode != 0:
		exit(result.returncode)


def compile(src, obj, cflags=cflags, include_directories=None):
	assert os.path.isfile(src)
	if has_changed(src, obj, include_directories):
		print(f"Compiling: {os.path.relpath(src)} -> {os.path.relpath(obj)}", flush=True)
		os.makedirs(os.path.dirname(obj), exist_ok=True)
		assert os.path.isdir(os.path.dirname(obj))
		if include_directories:
			gcc(*cflags, *[f"-I{include_directory}" for include_directory in include_directories], "-c", src, "-o", obj)
		else:
			gcc(*cflags, "-c", src, "-o", obj)


def link(out, objects, ldflags=ldflags):
	assert len(objects) > 0
	if not os.path.isfile(out) or max(os.path.getmtime(obj) for obj in objects) >= os.path.getmtime(out):
		print("Linking:", os.path.relpath(out), flush=True)
		os.makedirs(os.path.dirname(out), exist_ok=True)
		assert os.path.isdir(os.path.dirname(out))
		gcc(*objects, *ldflags, "-o", out)


def get_object_files(name, bin_dir, entry, c_files, dependencies):
	_src_to_obj = lambda f: src_to_obj(f, bin_dir, "." + name)
	objects = []
	if entry:
		assert os.path.isfile(entry)
		objects.append(_src_to_obj(entry))
	objects.extend(_src_to_obj(f) for f in c_files if not has_entry_point(f))
	if dependencies:
		for dependency in dependencies:
			config = build_configurations[dependency]
			objects.extend(get_object_files(config["name"], config["bin_dir"], None, config["c_files"], config.get("dependencies")))
	return list(set(objects))


def build(name, bin_dir, entry, c_files, cflags=cflags, ldflags=ldflags, include_directories=None, dependencies=None, *, clean_build=False):
	_src_to_obj = lambda f: src_to_obj(f, bin_dir, "." + name)
	out = join(bin_dir, name) + (".exe" if os.name == "nt" else "")
	if clean_build:
		clean(bin_dir)
	if dependencies:
		for dependency in dependencies:
			config = build_configurations[dependency]
			build_config(config, as_dependency=True)
	print("Building:", os.path.relpath(out) if entry else name, flush=True)
	c_files = [f for f in c_files if not has_entry_point(f)]
	for src in c_files:
		compile(src, _src_to_obj(src), cflags, include_directories)
	if entry:
		assert os.path.isfile(entry)
		entry_obj = _src_to_obj(entry)
		compile(entry, entry_obj, cflags, include_directories)
		objects = get_object_files(name, bin_dir, entry, c_files, dependencies)
		link(out, objects, ldflags)
		assert os.path.isfile(out)
	print("Finished:", os.path.relpath(out) if entry else name, flush=True)
	return out if entry else None


build_configurations = {
	"modules-debug": {
		"name": "modelgen-modules-debug",
		"bin_dir": modelgen_bin_dir,
		"c_files": get_c_files(modelgen_modules_dir),
		"cflags": debug_cflags,
		"ldflags": ldflags,
		"include_directories": [modelgen_src_dir],
	},
	"modules-debug-x64": {
		"name": "modelgen-modules-debug-x64",
		"bin_dir": modelgen_bin_dir,
		"c_files": get_c_files(modelgen_modules_dir),
		"cflags": debug_cflags + ["-m64"],
		"ldflags": ldflags + ["-m64"],
		"include_directories": [modelgen_src_dir],
	},
	"modules": {
		"name": "modelgen-modules",
		"bin_dir": modelgen_bin_dir,
		"c_files": get_c_files(modelgen_modules_dir),
		"cflags": release_cflags,
		"ldflags": ldflags,
		"include_directories": [modelgen_src_dir],
	},
	"modules-x64": {
		"name": "modelgen-modules-x64",
		"bin_dir": modelgen_bin_dir,
		"c_files": get_c_files(modelgen_modules_dir),
		"cflags": release_cflags + ["-m64"],
		"ldflags": ldflags + ["-m64"],
		"include_directories": [modelgen_src_dir],
	},
	"debug": {
		"name": "modelgen-debug",
		"bin_dir": modelgen_bin_dir,
		"entry": join(modelgen_src_dir, "modelgen.c"),
		"c_files": get_c_files(modelgen_src_dir),
		"cflags": debug_cflags,
		"ldflags": ldflags,
		"include_directories": [modelgen_src_dir],
		"dependencies": ["modules-debug"],
	},
	"debug-x64": {
		"name": "modelgen-debug-x64",
		"bin_dir": modelgen_bin_dir,
		"entry": join(modelgen_src_dir, "modelgen.c"),
		"c_files": get_c_files(modelgen_src_dir),
		"cflags": debug_cflags + ["-m64"],
		"ldflags": ldflags + ["-m64"],
		"include_directories": [modelgen_src_dir],
		"dependencies": ["modules-debug-x64"],
	},
	"release": {
		"name": "modelgen-release",
		"bin_dir": modelgen_bin_dir,
		"entry": join(modelgen_src_dir, "modelgen.c"),
		"c_files": get_c_files(modelgen_src_dir),
		"cflags": release_cflags,
		"ldflags": ldflags,
		"include_directories": [modelgen_src_dir],
		"dependencies": ["modules"],
	},
	"release-x64": {
		"name": "modelgen-release-x64",
		"bin_dir": modelgen_bin_dir,
		"entry": join(modelgen_src_dir, "modelgen.c"),
		"c_files": get_c_files(modelgen_src_dir),
		"cflags": release_cflags + ["-m64"],
		"ldflags": ldflags + ["-m64"],
		"include_directories": [modelgen_src_dir],
		"dependencies": ["modules-x64"],
	},
	"test": {
		"name": "modelgen-test",
		"bin_dir": modelgen_bin_dir,
		"entry": join(modelgen_tests_dir, "test.c"),
		"c_files": get_c_files(modelgen_tests_dir),
		"cflags": debug_cflags,
		"ldflags": ldflags,
		"include_directories": [modelgen_src_dir],
		"dependencies": ["debug"],
	},
	"test-x64": {
		"name": "modelgen-test-x64",
		"bin_dir": modelgen_bin_dir,
		"entry": join(modelgen_tests_dir, "test.c"),
		"c_files": get_c_files(modelgen_tests_dir),
		"cflags": debug_cflags + ["-m64"],
		"ldflags": ldflags + ["-m64"],
		"include_directories": [modelgen_src_dir],
		"dependencies": ["debug-x64"],
	},
}


def build_config(config, *, as_dependency=False, clean_build=False):
	if isinstance(config, str):
		config = build_configurations[config]
	return build(
		config["name"],
		config["bin_dir"],
		None if as_dependency else config.get("entry", None),
		config["c_files"],
		config.get("cflags", cflags),
		config.get("ldflags", ldflags),
		config.get("include_directories", None),
		config.get("dependencies", None),
		clean_build=clean_build)


if __name__ == "__main__":
	bin_filename = None
	argv = sys.argv[1:]
	for i, arg in enumerate(argv):
		if arg == "--":
			break
		elif arg == "clean":
			clean(modelgen_bin_dir)
		else:
			bin_filename = build_config(arg)
			bin_basename = os.path.basename(bin_filename)
			if "debug" in bin_basename or "release" in bin_basename:
				shutil.copyfile(bin_filename, join(os.path.dirname(bin_filename), os.path.basename(os.path.abspath(modelgen_dir)).lower() + os.path.splitext(bin_filename)[1]))

	if bin_filename is not None:
		try:
			argv = argv[argv.index("--")+1:]
			if os.path.exists(bin_filename):
				result = run([bin_filename, *argv], shell=True)
				exit(result.returncode)
		except ValueError:
			pass
