from pybuild import target, cppm, cppms, find_cppms, cpp, system_headers, func, link, proc, write_module_map, colorama,stubs
from pybuild.library_search import find_library, find_python, check_abi, ABIS

import pathlib
import time
import os
import sys
import shlex

target.pymodules.append(sys.modules['project'])

target.module_maps[0] = target.build / "modules.map"


target.modes['release'] = [
    "-O3",
    "-march=native"
]

target.modes['portable'] = [
    "-O3",
    "-march=core2"
]

target.modes['debug'] = [
    "-O2",
    "-ggdb3",
    "-march=native"
]

target.modes['asan'] = target.modes['debug'] + [
  "-fsanitize=address,undefined",
  "-fno-omit-frame-pointer"
]

target.linker_args['portable'] = [
    "-nostdlib++",
    "-Wl,-Bstatic",
    "-lc++",
    "-lc++abi",
    "-Wl,-Bdynamic",
]


def portable(*libs):
    statics = []
    others = []
    for l in libs:
        abi, static = check_abi(l)
        if abi == ABIS.C:
            if static:
                statics.append(l)
            else:
                others.append(l)
        else:
            if not static:
                raise RuntimeError("Cannot make a portable executable because C++ library", l, "is not static linkable")
            statics.append(l)
    if statics:
        return [
            *others,
            "-Wl,-Bstatic",
            *statics,
            "-Wl,-Bdynamic"
        ]
    else:
        return others


target.linker['portable'] = portable

CXXFLAGS = os.environ.get('CXXFLAGS', os.environ.get('CFLAGS', []))
if CXXFLAGS:
    CXXFLAGS = shlex.split(CXXFLAGS)

CXX = os.environ.get('CXX', "clang++")
if "clang++" not in CXX:
    print(colorama.Fore.RED,
          colorama.Back.BLACK,
          "This project uses clang's implicit modules system. Compilers other than clang++ are unlikely to work.",
          colorama.Style.RESET_ALL)


    
target.common_args = [
    CXX,
    "-D_LIBCPP_DISABLE_DEPRECATION_WARNINGS=true",
    "-std=c++26",
    "-stdlib=libc++",
    "-fPIC",
    "-fimplicit-modules",
    "-fbuiltin-module-map",
    "-fimplicit-module-maps",
    "-fmodule-map-file=/usr/include/c++/v1/module.modulemap",
    *[f"-fmodule-map-file={target.project/i}" for i in target.module_maps],
    f"-fmodules-cache-path={target.build}/modules",
    "-flto=thin",
    "-Wold-style-cast",
    "-Wall",
    "-Wextra",
    "-Wfloat-conversion",
    "-Wsign-conversion",
    "-Wsign-compare",
    "-Wpedantic",
    "-Wno-zero-length-array",
    *CXXFLAGS,
]

find_cppms(__file__)


def dist_clean(t):
    module_clean(t)
    for p, _, fs in reversed(list(target.build.walk())):
        for f in fs:
            (p/f).unlink()
        p.rmdir()
    return True

def module_clean(t):
    for p, _, fs in reversed(list((target.build / 'modules').walk())):
        for f in fs:
            (p/f).unlink()
        p.rmdir()
    return True
    

def clean(t):
    print("cleaning")
    for i in targets.values():
        if a := getattr(i, "out", None):
            p = pathlib.Path(a)
            if p.exists():
                print("removing",p)
                p.unlink()
    return True

targets = {
    "clean": target({
        "function": clean,
        "virtual": True
    }),
    "moduleclean": target({
        "deps": ["clean"],
        "function": module_clean,
        "virtual": True
    }),
    "distclean": target({
        "deps": ["moduleclean"],
        "function": dist_clean,
        "virtual": True
    }),
}
    
def check_pqxx(mode="debug"):
    if not find_pqxx(mode=mode):
        targets.pop(targets['Postgres']['deps'][0])
        targets.update(stubs['Postgres'])
        cppms.update(stubs['Postgres'])
        return stubs['Postgres']
    return []

def check_sqlite(mode="debug"):
    if not find_sqlite(mode):
        print("sqlite not available")
        targets.pop(targets['Sqlite']['deps'][0])
        targets.update(stubs['Sqlite'])
        cppms.update(stubs['Sqlite'])
        return stubs['Sqlite']
    return []

def find_sqlite(mode="debug", res = {}):
    r = res.get(mode, None)
    if r is not None:
        return r
    r = find_library("SQLiteCpp", ("-lsqlite3", "-lSQLiteCpp"), pkgconfig=False)
    if mode == "portable" and r:
        abi, static = check_abi("-lSQLiteCpp")
        if not static:
            print("cannot use shared sqlite in portable build, disabling sqlite")
            r = []
    res[mode] = r
    return r

def find_pqxx(mode="debug", res = {}):
    r = res.get(mode, None)
    if r is not None:
        return r
    r = find_library("libpq", ("-lpq", "-lpqxx"), "-lpqxx")
    if mode == "portable" and r:
        abi, static = check_abi("-lpqxx")
        if not static:
            print("cannot use shared pqxx in portable build, disabling psql")
            r = []
    res[mode] = r
    return r
