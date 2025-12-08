from pybuild import target, cppm, cppms, find_cppms, cpp, system_headers, func, pkg, proc, write_module_map, colorama, stubs, CXX
from pybuild.library_search import find_python, Library, Package, ABIS, static, shared
from linker_settings import get_linker_settings
from dataclasses import dataclass, field

import subprocess
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

target.modes['c_abi'] = [
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

target.linker_args['c_abi'] = target.linker_args['portable'] = [
    "-nostdlib++",
    "-Wl,-Bstatic",
    "-lc++",
    "-lc++abi",
    "-Wl,-Bdynamic",
]



def linker_args(mode):
    return target.linker_args.get(mode, [])


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
    "-pthread",
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

@dataclass
class PackageDescription(pkg):
    name: str
    pkgconf_flags:tuple[str] = ()
    libs: tuple[str] = ()
    Libs: tuple[str] = ()
    ld_flags: tuple[str] = ()
    c_flags: tuple[str] = ()
    abis: tuple[ABIS] = (ABIS.C, ABIS.libcxx)
    package: {str:Package} = field(default_factory=dict)

    def validate(self, mode):
        return self.getPackage(mode).validate(mode)

    def getCFlags(self, mode):
        return self.getPackage(mode).getCFlags(mode)
    def getLDFlags(self, mode):
        p = self.getPackage(mode)
        
        return p.getLDFlags(mode, link_mode=get_linker_settings(mode).get("default", shared))

    def getPackage(self, mode="debug"):
        package = self.package.get(mode, None)
        if package:
            return package
        ls = get_linker_settings(mode)
        if mode in ["portable", "c_abi"]:
            strategy = static
        else:
            strategy = shared
        package = Package.find_package(self.name, self.pkgconf_flags, self.abis, link_mode=strategy, link_mode_map=ls)
        if package.found:
            if mode == "portable" and self.name == "libpqxx":
                l = Library("-lpgcommon_shlib", search_path, [], [ABIS.C], link_mode=static)
                print(l)
                package.libs.append(l)
            self.package[mode] = package
            return package
        else:
            print("Could not find", self.name, "via pkgconf, falling back to pseudo-package")
            found = True
            libs = []
            for lib in self.libs:
                library = Library(lib, self.Libs, self.ld_flags, self.abis, link_mode_map = ls)
                if not library.found:
                    found = False
                libs.append(library)
            package = self.package[mode] = Package(
                found=found,
                name=self.name,
                cflags=self.c_flags,
                ldflags=[*self.Libs, *self.ld_flags],
                libraries=libs,
                link_mode_map=ls)
            return package

search_path = [f"-L{p}" for p in shlex.split(
    subprocess.run([CXX, "--print-search-dirs"], stdout=subprocess.PIPE).stdout.decode('utf-8'))[-1][1:].split(':')]

pqxx = PackageDescription("libpqxx",
                          libs=("-lpqxx", "-lpq"),
                          Libs=search_path)
pcap = PackageDescription("libpcap",
                          libs=("-lpcap"),
                          Libs=search_path)
sqlitecpp = PackageDescription("libsqlitecpp",
                          libs=("-lSQLiteCpp", "-lsqlite3"),
                          Libs=search_path)

def verify_sqlite(mode):
    if sqlitecpp.validate(mode):
        return []
    else:
        print("sqlite not available")
        targets.pop(targets['Sqlite']['deps'][0])
        targets.update(stubs['Sqlite'])
        cppms.update(stubs['Sqlite'])
        return stubs['Sqlite']

 
