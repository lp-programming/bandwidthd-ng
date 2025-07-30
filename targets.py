"""
This is a procedural config file. 
See _target.py for details on the target class

"""
from _target import *

target.modes['release'] = [
    "-O3",
]

target.modes['debug'] = [
    "-O0",
    "-g",
]

target.common_args = [
    "clang++",
    "-Wno-zero-length-array",
    "-D_LIBCPP_DISABLE_DEPRECATION_WARNINGS=true",
    "-march=native",
    "-std=c++26",
    "-stdlib=libc++",
    "-fPIC",
    "-fimplicit-modules",
    "-fbuiltin-module-map",
    "-fmodule-map-file=/usr/include/c++/v1/module.modulemap",
    "-fmodule-map-file=./modules.map",
    "-fprebuilt-module-path=./build",
    "-fmodules-cache-path=./modules",
    "-flto",
    "-Wold-style-cast",
    "-Wall",
    "-Wextra",
    "-Wpedantic"]

targets = {
    "all": target({
        "source": None,
        "args": [],
        "deps": ["core", "demo", "classic", "python"],
        "virtual": True
    }),
    "setup": target({
        "source": None,
        "args": [],
        "deps": [],
        "cmd": ["mkdir", "-p", "bin", "lib", "build/databases"],
        "virtual": True
    }),
    "clean": target({
        "source": None,
        "args": [],
        "deps": [],
        "cmd": ["rm", glob("build", "*.pcm"), glob("build/databases", "*.pcm"), glob("lib", "*.so"), glob("bin", "*"), "status.json"],
        "virtual": True
    }),
    "moduleclean": target({
        "source": None,
        "args": [],
        "deps": ["clean"],
        "cmd": ["rm", "modules/modules.timestamp", glob("modules", "*/modules.idx"), glob("modules", "*/*.pcm")],
        "virtual": True
    }),
    "distclean": target({
        "source": None,
        "args": [],
        "deps": ["clean", "moduleclean"],
        "cmd": ["rmdir", "build/databases", "build", glob("modules", "*/"), "modules", "lib", "bin"],
        "virtual": True
    }),
    "python": target({
        "source": None,
        "args": [],
        "deps": ["lib/PyBandwidthD.so"],
        "virtual": True
    }),
    "lib/PyBandwidthD.so": target({
        "source": ["python/PyBandwidthd.cppm"],
        "args": ["-Wno-old-style-cast", "python/PyBandwidthd.cppm", "-o", "lib/PyBandwidthD.so", "-shared", glob("build/", "*.pcm"), "-lpcap", proc("python-config", "--includes"), proc("python-config", "--ldflags")],
        "deps": ["core"]
    }),
    "classic": target({
        "source": None,
        "args": [],
        "deps": ["bin/bandwidthd"],
        "virtual": True
    }),
    "bin/bandwidthd": target({
        "source": ["classic/Classic.cpp"],
        "args": ["-fprebuilt-module-path=./build/databases", "classic/Classic.cpp", "-o", "bin/bandwidthd", glob("build/", "*.pcm"), "build/databases/Postgres.pcm", "-lpcap", "/usr/lib64/libpqxx.so", "/usr/lib64/libpq.so"],
        "deps": ["core", "databases"]
    }),
    "databases": target({
        "source": None,
        "args": [],
        "deps": ["build/databases/Postgres.pcm"],
        "virtual": True
    }),
    "build/databases/Postgres.pcm": target({
        "source": ["databases/Postgres.cppm"],
        "args": ["--precompile", "-o", "build/databases/Postgres.pcm", "databases/Postgres.cppm"],
        "deps": ["core"],
    }),
    "demo": target({
        "source": None,
        "args": [],
        "deps": ["bin/demo"],
        "virtual": True
    }),
    "bin/demo": target({
        "source": ["demo/Demo.cpp"],
        "args": ["demo/Demo.cpp", "-o", "bin/demo", glob("build/", "*.pcm"), "-lpcap"],
        "deps": ["core", "build/Types.pcm", "build/BandwidthD.pcm"]
    }),
    "core": target({
        "source": None,
        "args": [],
        "deps": ["build/BandwidthD.pcm", "build/Database.pcm"],
        "virtual": True
    }),
    "build/BandwidthD.pcm": target({
        "source": ["core/BandwidthD.cppm"],
        "args": ["--precompile", "-o", "build/BandwidthD.pcm", "core/BandwidthD.cppm"],
        "deps": ["build/EnumFlags.pcm",
                 "build/HeaderView.pcm",
                 "build/Types.pcm",
                 "build/util.pcm",
                 "build/net_integer.pcm",
                 "build/SyscallHelper.pcm"
                ]
    }),
    "build/Database.pcm": target({
        "source": ["core/Database.cppm"],
        "args": ["--precompile", "-o", "build/Database.pcm", "core/Database.cppm"],
        "deps": ["build/BandwidthD.pcm", "build/Types.pcm"]
    }),
    "build/EnumFlags.pcm": target({
        "source": ["core/EnumFlags.cppm"],
        "args": ["--precompile", "-o", "build/EnumFlags.pcm", "core/EnumFlags.cppm"],
        "deps": []
    }),
    "build/HeaderView.pcm": target({
        "source": ["core/HeaderView.cppm"],
        "args": ["--precompile", "-o", "build/HeaderView.pcm", "core/HeaderView.cppm"],
        "deps": []
    }),
    "build/net_integer.pcm": target({
        "source": ["core/net_integer.cppm"],
        "args": ["-Wno-zero-length-array", "--precompile", "-o", "build/net_integer.pcm", "core/net_integer.cppm"],
        "deps": []
    }),
    "build/SyscallHelper.pcm": target({
        "source": ["core/SyscallHelper.cppm"],
        "args": ["--precompile", "-o", "build/SyscallHelper.pcm", "core/SyscallHelper.cppm"],
        "deps": []
    }),
    "build/Types.pcm": target({
        "source": ["core/Types.cppm"],
        "args": ["--precompile", "-o", "build/Types.pcm", "core/Types.cppm"],
        "deps": ["build/EnumFlags.pcm", "build/util.pcm", "build/net_integer.pcm"]
    }),
    "build/util.pcm": target({
        "source": ["core/util.cppm"],
        "args": ["--precompile", "-o", "build/util.pcm", "core/util.cppm"],
        "deps": ["build/net_integer.pcm"]
    })
}

