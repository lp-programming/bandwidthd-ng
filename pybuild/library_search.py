import subprocess

libs = {}

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

def find_library(name, alt, *extra, pkgconfig=True):
    result = libs.get(name, None)
    if result:
        return result
    if pkgconfig:
        print("Checking for", name, "via pkgconf")
        p = subprocess.run(['pkgconf', name, '--libs'], stdout=subprocess.PIPE)
        if p.returncode:
            print("Failed to find", name, "via pkgconf, looking globally")
            args = list(alt)
        else:
            args = [*p.stdout.decode('utf-8').split(), *extra]
    else:
        args = list(alt)
    p = subprocess.run(['ld', '-shared', '-o', '/dev/null', *args], stdout=subprocess.PIPE)
    if p.returncode:
        print(name, "not found")
        libs[name] = False
        return False
    libs[name] = args
    return args



__all__ = ["find_python", "find_library"]
