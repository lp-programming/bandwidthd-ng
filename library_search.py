import subprocess

def find_python(result = []):
    if result:
        return result[0]
    args = subprocess.run(['python3-config', '--ldflags', '--embed'], stdout=subprocess.PIPE).stdout.decode('utf-8').split()
    p = subprocess.run(['ld', '-shared', '-o', '/dev/null', *args], stdout=subprocess.PIPE)
    if p.returncode:
        print("python not found or not linkable, check that python3-config works")
        result.append(False)
        return False
    r = subprocess.run(['python3-config', '--extension-suffix'], stdout=subprocess.PIPE).stdout.decode('utf-8').strip()
    result.append(r)
    return r

def find_pqxx(result = []):
    if result:
        return result[0]
    print("Checking for postgres via pkgconf")
    p = subprocess.run(['pkgconf', 'libpq', '--libs'], stdout=subprocess.PIPE)
    if p.returncode:
        print("Failed to find postgres via pkgconf, looking globally")
        args = ["-lpq", "-lpqxx"]
    else:
        args = [*p.stdout.decode('utf-8').split(), "-lpqxx"]
    p = subprocess.run(['ld', '-shared', '-o', '/dev/null', *args], stdout=subprocess.PIPE)
    if p.returncode:
        print("postgres libpq or libpqxx not found")
        result.append(False)
        return False
    result.append(args)
    return args

def find_sqlite(result = []):
    if result:
        return result[0]
    print("Checking for SQLiteCpp via pkgconf")
    p = subprocess.run(['pkgconf', 'SQLiteCpp', '--libs'], stdout=subprocess.PIPE)
    if p.returncode:
        print("Failed to find SQLiteCpp via pkgconf, looking globally")
        args = ["-lsqlite3", "-lSQLiteCpp"]
    else:
        args = [*p.stdout.decode('utf-8').split(), "-lSQLiteCpp"]
    p = subprocess.run(['ld', '-shared', '-o', '/dev/null', *args], stdout=subprocess.PIPE)
    if p.returncode:
        print("sqlite libsqlite3 or libSQLiteCpp not found")
        result.append(False)
        return False
    result.append(args)
    return args



__all__ = ["find_python", "find_pqxx", "find_sqlite"]
