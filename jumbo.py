from pybuild import build, _target
import os

if 'USE' in os.environ:
  _target.UseFlags.default = -1

import targets
import sys
import subprocess
import shlex

def main(argv=sys.argv[1:]):
  target = argv[0]
  mode = argv[1]

  class OrderedSet(dict):
    def add(self, o):
      self[o] = True
    def __repr__(self):
      return str(tuple(self))

  def getdeps(n):
    t = build.Target(n)
    s = str(list(t._Target__target.source)[-1])
    for d in t._Target__target.deps:
      yield from getdeps(d)
    if s.endswith(".cpp") or s.endswith(".cppm"):
      yield s

  build.targets = targets.targets
  targets.system_headers = _target.system_headers = OrderedSet()

  build.status={}
  target = build.Target(target)
  target.prebuild(mode)

  args = target.getArgs()
  args = [i for i in args if not i.endswith('.pcm') and not i.endswith(".cpp")]
  args = [i for i in args if not "modu" in i]
#args = [i for i in args if not "lto" in i]

  args.extend([
    "-x",
    "c++",
    "-",
    "-DSKIP_CONSTEXPR_TESTS"
   ])

  CXXFLAGS = os.environ.get('CXXFLAGS', os.environ.get('CFLAGS', []))
  if CXXFLAGS:
    CXXFLAGS = shlex.split(CXXFLAGS)

  args.extend(CXXFLAGS)

  sources = list({i: True for i in getdeps(target.name)})

  for s in sources:
    _target.scan_file(open(s), s)
  headers = list(targets.system_headers)


  jumbo = []
  for h in headers:
    jumbo.append(f"#include <{h}>")

  for a in target._Target__target.get("args"):
    if isinstance(a, targets.pkg):
      for l in a.getPackage(mode).libs:
        if l.link_mode is targets.static:
          l.license


  p = subprocess.Popen(args, stdout=-1, stdin=-1)
  for line in jumbo:
    p.stdin.write((line+'\n').encode('utf-8'))
  for s in sources:
    with open(s) as f:
      st = f.readlines()
      for line in st:
        line = line.replace("export", " ").strip()
        if line.startswith("module") or line.startswith("import"):
          continue
        p.stdin.write((line+"\n").encode('utf-8'))

  p.stdin.close()

  p.wait()


  if not p.poll():
    print("Done building")
    licenses = {}
    for a in target._Target__target.get("args"):
      if isinstance(a, targets.pkg):
        for l in a.getPackage(mode).libs:
          if l.link_mode is targets.static:
            licenses[l.name] = l.license
    if licenses:
      print("Redistributing the generated object file requires complying with the following licenses")
      for k,v in licenses.items():
        print(f"{k}: {v}")

  return p.poll()

if __name__=='__main__':
    raise SystemExit(main())
