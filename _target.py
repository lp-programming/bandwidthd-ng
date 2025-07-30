import pathlib
import subprocess
class glob:
    def __init__(self, p, g):
        self.path = p
        self.glob = g

class proc:
    def __init__(self, *args):
        self.args = args

class target(dict):
    common_args = []
    modes = dict(
        debug = [],
        release = []
    )
    @property
    def virtual(self):
        return self.get("virtual", False)
    @property
    def source(self):
        yield "build.py"
        yield "_target.py"
        yield "targets.py"
        yield from self.get("source")
    @property
    def deps(self):
        for i in self.get('deps'):
            yield i
    def getArgs(self, mode="debug"):
        if 'cmd' in self:
            yield from self.cmd
            return
        if self.virtual:
            yield "true"
            return
        for i in self.common_args:
            yield from self.expand(i)
        for i in self.modes[mode]:
            yield from self.expand(i)
        for i in self.get("args"):
            yield from self.expand(i)
    @property
    def cmd(self):
        for i in self.get("cmd", []):
            yield from self.expand(i)
    def expand(self, i):
        if isinstance(i, glob):
            for p in pathlib.Path(i.path).glob(i.glob):
                yield str(p)
        elif isinstance(i, proc):
            p = subprocess.run(i.args, stdout = subprocess.PIPE, stdin = subprocess.PIPE)
            for o in p.stdout.decode('utf-8').split():
                o = o.strip()
                if o:
                    yield o
        else:
            yield i

                
