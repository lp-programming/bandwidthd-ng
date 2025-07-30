import json
import sys
import os
import subprocess
import pathlib
import hashlib
import time

from targets import targets

STATUS_FILE = pathlib.Path("status.json")

if STATUS_FILE.exists():
    with STATUS_FILE.open("r", encoding="utf-8") as r:
        status = json.load(r)
else:
    status = {}

def system(args):
    return subprocess.Popen(args, stdin=subprocess.PIPE)

class State:
    default = "default"
    pending = "pending"
    rebuilt = "rebuilt"
    skipped = "skipped"
    failure = "failure"

class Task:
    building = 0
    limit = 1
    state = State.default
    def __init__(self, t):
        self.proc = None
        self.target = t
    def poll(self):
        if self.state is State.failure:
            return 1
        if self.state is State.rebuilt:
            return 0
        if self.proc is None:
            if not self.maybeStart():
                return None
        r = self.proc.poll()
        if r is not None:
            if self.state is State.pending:
                Task.building -= 1
            if r:
                print("Failure running",self)
                print(self.args)
                self.state = State.failure
            else:
                self.state = State.rebuilt
        return r
    def maybeStart(self):
        if Task.building < Task.limit:
            self.start()
            return True
        return False
    def start(self):
        print("Building",self)
        if self.state is not State.default:
            print("Trying to start already started task", self)
            return
        Task.building += 1
        self.state = State.pending
        args = list(self.target.getArgs())
        self.args = args
        self.proc = system(args)
        if self.target.name == "clean":
            status.clear()
    def wait(self, errorShutdown=False):
        if self.proc is None:
            if errorShutdown:
                self.state = State.failure
                return
            self.start()
        if self.state is not State.pending:
            print("Awaiting already resolved task", self)
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
        if self.state is State.failure:
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
            return
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
    def wait(self, errorShutdown=False):
        if self.state is not State.pending:
            return
        for p in self.pending:
            if self.poll():
                self.state = State.failure
                return
            p.wait(errorShutdown)
        if self.poll():
           self.state = State.failure
           return
        if self.task:
            self.task.wait(errorShutdown)
            if self.task.poll():
                self.state = State.failure
            else:
                self.state = State.rebuilt
                status[self.name] = self.sha
    def prebuild(self, mode="debug"):
        if self.state is not State.default:
            return self
        self.mode = mode
        print("prebuild:", self)
        rebuild = self.__target.virtual or (status.get(self.name, None) != self.sha)
        self.pending = []
        for d in self.__target.deps:
            dep = Target(d)
            dep.prebuild(mode)
            if dep.state is not State.skipped:
                rebuild = True
                self.pending.append(dep)
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
        
    with STATUS_FILE.open("w", encoding="utf-8") as f:
        json.dump(obj=status, fp=f)
    raise SystemExit(ec)

        
if __name__ == "__main__":
    main()

