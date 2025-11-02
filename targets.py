"""
This is a procedural config file. 

"""
from project import *
from functools import partial
PYLIB = f"lib/PyBandwidthD{find_python() or '.so'}"

find_pqxx = partial(find_library, "libpq", ("-lpq", "-lpqxx"), "-lpqxx")
find_sqlite = partial(find_library, "SQLiteCpp", ("-lsqlite3", "-lSQLiteCpp"), pkgconfig=False)

targets.update( cppms | {
    "all": target({
        "doc": "Default meta target - builds everything we can",
        "deps": ["core", "bin/demo", "bin/live"],
        "targets": ["classic", "python", "bin/graph", "bin/psqlsensor"],
        "virtual": True
    }),
    "core": target({
        "doc": "The unspecialized library files",
        "deps": [cppm.module("core/network/BandwidthD.cppm")],
        "virtual": True
    }),
    "bin/demo": cpp(
        doc="The simplest version, which just prints status messages to stdout",
        path="demo/Demo.cpp",
        out="bin/demo",
        args=["-lpcap"]
    ),
    "bin/live": cpp(
        doc="This version draws live graphs",
        path="live/Live.cpp",
        out="bin/live",
        args=["-lpcap"]
    ),
    "bin/graph": cpp(
        doc="This version draws graphs from postgres",
        path="graph/Graph.cpp",
        out="bin/graph",
        requirements=[find_pqxx],
        args=["-lpcap", func(find_pqxx)]
    ),
    "bin/psqlsensor": cpp(
        doc="This is a minimal sensor that only can send data to postgres",
        path="psqlsensor/PsqlSensor.cpp",
        out="bin/psqlsensor",
        requirements=[find_pqxx],
        args=["-lpcap", func(find_pqxx)]
    ),
    "bin/bandwidthd": cpp(
        doc="This is the classic version, which does everything in one binary",
        path="classic/Classic.cpp",
        out="bin/bandwidthd",
        requirements=[find_pqxx, find_sqlite],
        args=["-lpcap", func(find_pqxx), func(find_sqlite)]
    ),
    "classic": target({
        "doc":"As close to the original bandwidthd as we can get",
        "virtual": True,
        "deps": ["bin/bandwidthd"]
    }),    
    "python": target({
        "doc":"Core functionality exposed to python for extensions",
        "virtual": True,
        "deps": [PYLIB]
    }),
    PYLIB: cpp(
        doc="This version exposes basic functionality through python",
        path="python/PyBandwidthd.cpp",
        out=PYLIB,
        requirements=[find_pqxx, find_python],
        args=["-lpcap", func(find_pqxx), proc("python3-config", "--includes", "--ldflags", "--embed"), "-Wno-old-style-cast", "-shared"]
    ),
    "setup": target({
        "virtual": True,
        "deps": ["module-map"]
    }),
    "module-map": target({
        "doc": "Rebuild the module map for the local system",
        "hash": str.join(" ", sorted(system_headers)),
        "function": write_module_map        
    }),
})

targets[next(targets["SubnetIdentifier"].deps)]['args'].append('-Wno-zero-length-array')


