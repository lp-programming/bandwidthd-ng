from pybuild.library_search import LS, linker_dict, static, shared

def get_linker_settings(mode):
    # Set up default linker rules. We're GPL2, which means we can only statically link GPL2-compatible code. This limits
    # our portability options.
    default = linker_dict(
        default = shared, # if we don't know the license, opt for shared-only
        libsqlite3 = shared | static, # "Public Domain" dedication
        libz = shared | static, # zlib
        libpqxx = shared | static, # BSD-3
        libpq = shared | static, # POSTGRES (mit-like)
        libpgcommon = shared | static, # POSTGRES (mit-like)
        libpgport = shared | static, # POSTGRES (mit-like)
        libm = shared | static, # LGPL2
        libdl = shared | static, # LGPL-2
        libpthread = shared | static, # LGPL-2
        libssl = shared, # Apache2. Also, these shouldn't get out of date, so use the target system version
        libcrpyto = shared, # Apache2. Also, these shouldn't get out of date, so use the target system version
        libpcap = shared | static, # BSD
        libSQLiteCpp = shared, # GPL3
        libatomic = shared | static, # GPL3 with runtime exception
    )

    portable = default | dict(
        libsqlite3 = static,
        libpqxx = static,
        libpq = static,
        libpcap = static,
        libSQLiteCpp = None
    )

    c_abi = default | dict(
        libpqxx = static,
        libSQLiteCpp = None
    )

    linker_settings = dict(
        default = default,
        portable = portable,
        c_abi = c_abi
    )
    return linker_settings.get(mode, default)


