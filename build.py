import json
import sys
import os
import subprocess
import pathlib
import hashlib
import time
import enum

from targets import targets

STATUS_FILE = pathlib.Path("status.json")

if STATUS_FILE.exists():
    with STATUS_FILE.open("r", encoding="utf-8") as r:
        status = json.load(r)
else:
    status = {}

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
    limit = 1
    state = State.default
    globalState = State.default
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
                Task.building -= 1
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
        print("building:",self)
        if self.state is not State.default:
            print("Trying to start already started task", self)
            return False
        Task.building += 1
        self.state = State.pending
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
    def sha(self):
        if self.__target.virtual:
            return None
        sha = b""
        source = [pathlib.Path(i) for i in [self.name, *self.__target.source]]
        for s in source:
            if s.exists():
                with s.open("rb") as f:
                    sha = hashlib.sha256(sha + f.read()).hexdigest().encode('utf-8')
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
        for r in self.__target.requirements:
            if not r():
                print("Not building",self,"due to missing dep")
                self.state = State.missing
                return self
        if self.__target.virtual:
            rebuild = bool(list(self.__target.cmd))
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
    if '-m' in argv:
        i = argv.index('-m')
        mode = argv[i + 1]
        argv.pop(i + 1)
        argv.pop(i)
    else:
        mode = "debug"
    if '-j' in argv:
        i = argv.index('-j')
        if len(argv) > i + 1 and argv[i + 1].isdigit():
            Task.limit = int(argv[i + 1])
            argv.pop(i + 1)
        else:
            Task.limit = os.cpu_count()
        argv.pop(i)
        
    if len(argv) < 2:
        build_targets = ["setup", "all"]
    else:
        build_targets = ["setup", *argv[1:]]

    ec = 0
    building = [Target(target).prebuild(mode) for target in build_targets]
    for b in building:
        b.wait()
        ec |= b.poll()

    Target.syncState()
        
    with STATUS_FILE.open("w", encoding="utf-8") as f:
        json.dump(obj=status, fp=f)
    raise SystemExit(ec)

        
if __name__ == "__main__":
    main()

