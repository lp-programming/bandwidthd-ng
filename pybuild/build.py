import json
import sys
import os
import subprocess
import pathlib
import hashlib
import time
import enum
import argparse

sys.dont_write_bytecode = True

from ._target import *


STATUS_FILE = pathlib.Path("status.json")

if STATUS_FILE.exists():
    with STATUS_FILE.open("r", encoding="utf-8") as r:
        meta_status = json.load(r)
else:
    meta_status = {}

def system(args):
    return subprocess.Popen(args, stdin=subprocess.PIPE)

class State(enum.Enum):
    default = 0
    pending = 1
    rebuilt = 2
    skipped = 3
    failure = 4
    missing = 5

class Task:
    building = 0
    maxParallel = 0
    totalBuilt = 0
    limit = 1
    state = State.default
    globalState = State.default
    @classmethod
    def markStarted(cls):
        cls.building += 1
        cls.maxParallel = max(cls.building, cls.maxParallel)
    @classmethod
    def markCompleted(cls):
        cls.building -= 1
        cls.totalBuilt += 1
    def __init__(self, t):
        self.proc = None
        self.target = t
    def poll(self):
        if self.state is State.failure:
            return 1
        if self.state is State.rebuilt:
            return 0
        if self.proc is None:
            if Task.globalState is State.failure:
                return 1
            if not self.maybeStart():
                return None
        r = self.proc.poll()
        if r is not None:
            if self.state is State.pending:
                self.markCompleted()
            if r:
                print("Failure running",self)
                print(str.join(' ', [repr(i) for i in self.args]))
                self.state = State.failure
                Task.globalState = State.failure
            else:
                self.state = State.rebuilt
        return r
    def maybeStart(self):
        if Task.building < Task.limit:
            self.start()
            return True
        return False
    def start(self):
        if self.globalState is State.failure:
            self.state = State.failure
            return False
        print("building:", self)
        if self.state is not State.default:
            print("Trying to start already started task", self)
            return False
        self.markStarted()
        self.state = State.pending
        if self.target.function:
            if self.target.function(self.target):
                args = ['true']
            else:
                args = ['false']
        else:
            args = list(self.target.getArgs())
        self.args = args
        self.proc = system(args)
        if self.target.name == "clean":
            status.clear()
        return True
    def wait(self):
        if self.proc is None:
            if not self.start():
                return
        self.proc.wait()
        self.poll()
    def __repr__(self):
        return f"<Task: {self.target}>"
    __str__=__repr__
    
class Target:
    __used = {}
    state = State.default
    task = None
    mode = "debug"
    @classmethod
    def syncState(cls):
        for k,v in cls.__used.items():
            if v.state is State.rebuilt:
                status[k] = v.sha
            if v.state is State.failure:
                status[k] = None
    @staticmethod
    def __new__(cls, tname):
        t = cls.__used.get(tname, None)
        if t:
            return t
        t = super().__new__(cls)
        cls.__used[tname] = t
        return t
    def __init__(self, tname):
        self.__target = targets[tname]
        self.name = tname
    def __str__(self):
        return f"<Target: {self.name}, {self.state}>"
    __repr__ = __str__
    @property
    def function(self):
        return self.__target.get('function', None)
    @property
    def sha(self):
        if self.__target.virtual:
            return None
        sha = b""
        source = [pathlib.Path(i) for i in [self.name, *self.__target.source]]
        for s in source:
            if s.exists():
                with s.open("rb") as f:
                    sha = hashlib.sha256(sha + f.read()).hexdigest().encode('utf-8')
        if not self.function:
            args = str.join(' ', self.getArgs()).encode('utf-8')
            sha = hashlib.sha256(sha + args).hexdigest().encode('utf-8')
        if h := self.__target.get('hash'):
            sha = hashlib.sha256(sha + h.encode('utf-8')).hexdigest().encode('utf-8')
        return sha.decode('utf-8')
    def poll(self):
        if self.state is State.skipped or self.state is State.rebuilt:
            return 0
        if self.state is State.failure or self.state is State.missing:
            return 1
        waiting = False
        for p in self.pending:
            ec = p.poll()
            if ec is None:
                waiting = True
            if ec:
                self.state = State.failure
                return ec
        if waiting:
            return None
        if not self.task:
            self.state = State.rebuilt
            return 0
        ec = self.task.poll()
        if ec is not None:
            if ec:
                self.state = State.failure
            else:
                self.state = State.rebuilt
        return ec
    def wait(self):
        if self.state is not State.pending:
            return
        for p in self.pending:
            if self.poll():
                self.state = State.failure
                return
            p.wait()
        if self.poll():
           self.state = State.failure
           return
        if self.task:
            self.task.wait()
            if self.task.poll():
                self.state = State.failure
            else:
                self.state = State.rebuilt
    def prebuild(self, mode="debug"):
        if self.state is not State.default:
            return self
        self.mode = mode
        print("prebuild:", self)
        if su := getattr(self.__target, "setup", None):
            su()
        
        for r in self.__target.requirements:
            if not r():
                print("Not building",self,"due to missing dep")
                self.state = State.missing
                return self
        if self.__target.virtual:
            rebuild = bool(list(self.__target.cmd)) or bool(self.__target.get('function', None))
        else:
            rebuild = status.get(self.name, None) != self.sha
        self.pending = []
        for d in self.__target.deps:
            dep = Target(d)
            dep.prebuild(mode)
            if dep.state is not State.skipped:
                if dep.state is State.missing:
                    self.state = State.missing
                    self.pending.clear()
                    return self
                rebuild = True
                self.pending.append(dep)
        for t in self.__target.targets:
            target = Target(t)
            target.prebuild(mode)
            if target.state is State.skipped or target.state is State.missing:
                continue
            rebuild = True
            self.pending.append(target)
        if rebuild:
            self.state = State.pending
            self.task = Task(self)
        else:
            self.state = State.skipped
        return self
    def getArgs(self):
        return self.__target.getArgs(self.mode)

        
def main(argv = sys.argv):
    global targets
    if "-j" in argv:
        jidx = argv.index('-j')
        if jidx < len(argv) - 1:
            if not argv[jidx + 1].isdigit():
                argv.insert(jidx + 1, str(os.cpu_count()))
    if '--' in argv:
        argv.remove('--')

    if "help" in argv:
        argv.append("--help")
        argv.append("-q")

    argv = [(('--'+i) if '=' in i and not i.startswith('--') else i )for i in argv]

    parser = argparse.ArgumentParser(prog="pybuild", description="Run the pybuild build tool")
    parser.add_argument("-j", "--jobs", type=int, nargs='?', default=1, const=os.cpu_count(),
                        help="number of parallel jobs to use")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="enable verbose output")
    parser.add_argument("-m", "--mode", default="debug",
                    help="build mode (default: debug)")
    parser.add_argument("--prefix", type=pathlib.Path, default=pathlib.Path("/usr/local/"), help="installation prefix")
    parser.add_argument("--build", type=pathlib.Path, default=pathlib.Path("build"), help="build directory")
    parser.add_argument("--project", type=str, default=".", help="project directory")
    parser.add_argument("-p", "--print", action="store_true", help="enable printing")
    parser.add_argument("-q", action="store_true", dest="quit", help="quit without building")
    parser.add_argument("--jobserver-auth", help=argparse.SUPPRESS)

    args, rest = parser.parse_known_intermixed_args(argv[1:])
    
    mode = args.mode

    project = args.project
    
    Task.limit = args.jobs

    target.prefix = args.prefix

    target.build = args.build
    
    target.project = pathlib.Path(args.project)

    sys.path.insert(0, project)
    from targets import targets
    sys.path.pop(0)
    if "targets" in rest or "tasks" in rest:
        a = targets["all"]
        print("The following top level targets are defined: \n\n")
        print("all: ", a.get("doc"), "\n")
        for d in a['deps']:
            print(d, targets[d].get("doc"), sep=': ', end="\n\n")
            
        for d in a['targets']:
            print(d, targets[d].get("doc"), sep=': ', end="\n\n")
        print("clean: remove most build artifacts\n")
        print("moduleclean: remove module cache, too\n")
        print("distclean: remove everything not tracked by git")
        return 0;

    if args.print:
        from pprint import pprint
        pprint(targets)
    if args.quit:
        raise SystemExit(1)

    if "setup" in targets:
        setup = ["setup"]
    else:
        setup = []
        
    if len(rest) < 1:
        build_targets = [*setup, "all"]
    else:
        build_targets = [*setup, *rest]

    global status
    if (mstatus := meta_status.get(str(target.build), None)) is None:
        meta_status[str(target.build)] = mstatus = {}
        
    if (status := mstatus.get(mode, None)) is None:
        mstatus[mode] = status = {}

    ec = 0
    building = [Target(target).prebuild(mode) for target in build_targets]
    for b in building:
        b.wait()
        ec |= b.poll()

    Target.syncState()


    json_status = json.dumps(meta_status)
    with STATUS_FILE.open("w", encoding="utf-8") as f:
        print(json_status, file=f)

    print(f"done building {Task.totalBuilt} jobs, using max", Task.maxParallel, "workers")
        
    return ec

        
if __name__ == "__main__":
    main()

