from pybuild import target, cppm, cppms, find_cppms, cpp, func, create_module_file, system_headers, proc
from library_search import find_python, find_pqxx, find_sqlite

import colorama
import pathlib
import time
import os
import sys
import shlex

target.pymodules.append(sys.modules['project'])
 


target.modes['release'] = [
    "-O3",
]

target.modes['debug'] = [
    "-O2",
    "-ggdb3",
]

target.modes['asan'] = target.modes['debug'] + [
  "-fsanitize=address,undefined",
  "-fno-omit-frame-pointer"
]

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
    "-march=native",
    "-std=c++26",
    "-stdlib=libc++",
    "-fPIC",
    "-fimplicit-modules",
    "-fbuiltin-module-map",
    "-fimplicit-module-maps",
    "-fmodule-map-file=/usr/include/c++/v1/module.modulemap",
    *[f"-fmodule-map-file={target.project/i}" for i in target.module_maps],
    f"-fmodules-cache-path={target.build}/modules",
    "-flto",
    "-Wold-style-cast",
    "-Wall",
    "-Wextra",
    "-Wfloat-conversion",
    "-Wsign-conversion",
    "-Wsign-compare",
    "-Wpedantic",
    *CXXFLAGS,
]

find_cppms(__file__)

def write_module_map(_):
    mm = create_module_file(True)
    if (target.project / target.module_maps[0]).exists():
        with open(target.project / target.module_maps[0], 'r') as f:
            if f.read().strip() == mm.strip():
                return True
    with open(str(target.project / target.module_maps[0]), 'w') as f:
        print(mm, file=f)
    print(colorama.Fore.RED,
          colorama.Back.BLACK,
          "modules.map rebuilt. You probably need to run clean.",
          colorama.Style.RESET_ALL)
    return True

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
    
