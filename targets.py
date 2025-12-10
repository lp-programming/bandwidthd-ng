"""
This is a procedural config file. 

"""
from project import *


def use_targets():
    for k, v in dict(classic="classic",
                     python="python",
                     live="bin/live",
                     graph="bin/graph",
                     psqlsensor="bin/psqlsensor").items():
        if target.use_flags[k] > -1:
            yield v

def not_portable(mode):
    if mode == "portable":
        print("Disabling component in portable build")
    return mode != "portable"

bin_bandwidthd = cpp(
    doc="This is the classic version, which does everything in one binary",
    path="classic/Classic.cpp",
    out="bin/bandwidthd",
    requirements=[pcap.validate],
    optionals=[],
    args=[func(linker_args), pcap, pqxx, sqlitecpp, ]
)

match target.use_flags['sqlite']:
    case 0:
        bin_bandwidthd['optionals'].append(verify_sqlite)
    case 1:
        bin_bandwidthd['requirements'].append(sqlitecpp.validate)
match target.use_flags['postgres']:
    case 0:
        bin_bandwidthd['optionals'].append(verify_postgres)
    case 1:
        bin_bandwidthd['requirements'].append(pqxx.validate)

targets.update( cppms | {
    "all": target({
        "doc": "Default meta target - builds everything we can",
        "deps": ["core", "bin/demo", "bin/live"],
        "targets": [*use_targets()],
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
        requirements=[pcap.validate],
        args=[func(linker_args), pcap]
    ),
    "bin/live": cpp(
        doc="This version draws live graphs",
        path="live/Live.cpp",
        out="bin/live",
        requirements=[pcap.validate],
        args=[func(linker_args), pcap]
    ),
    "bin/graph": cpp(
        doc="This version draws graphs from postgres",
        path="graph/Graph.cpp",
        out="bin/graph",
        requirements=[pqxx.validate],
        args=[func(linker_args), pcap, pqxx]
    ),
    "bin/psqlsensor": cpp(
        doc="This is a minimal sensor that only can send data to postgres",
        path="psqlsensor/PsqlSensor.cpp",
        out="bin/psqlsensor",
        requirements=[pqxx.validate, pcap.validate],
        args=[func(linker_args), pcap, pqxx]
    ),
    "bin/bandwidthd": bin_bandwidthd,
    "classic": target({
        "doc":"As close to the original bandwidthd as we can get",
        "virtual": True,
        "deps": ["bin/bandwidthd"]
    }),    
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




if target.use_flags['python'] > -1:
    PYLIB = f"lib/PyBandwidthD{find_python() or '.so'}"
    targets.update({
        "python": target({
            "doc":"Core functionality exposed to python for extensions",
            "virtual": True,
            "deps": [PYLIB]
        }),
        PYLIB: cpp(
            doc="This version exposes basic functionality through python",
            path="python/PyBandwidthd.cpp",
            out=PYLIB,
            requirements=[pqxx.validate, not_portable],
            args=[func(linker_args), pcap, pqxx, proc("python3-config", "--includes", "--ldflags", "--embed"), "-Wno-old-style-cast", "-shared"]
        ),
    })
