#!/usr/bin/python3

copyright = '''
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Source: <https://github.com/lp-programming/bandwidthd-ng>
Upstream-Name: bandwidthd-ng
Upstream-Contact: logan@bandwidthd.lp-programming.com

Files:
 *
Copyright:
 <2025> LP Programming L.L.C.
 <2025> Jeroen T. Vermeulen (libcxx-static)
License: GPL2
 This package is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License.
 .
 This package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this package. If not, see <https://www.gnu.org/licenses/>.
Comment:
'''

control = '''
Source: bandwidthd-ng
Version: 0.1.1
Section: admin
Priority: optional
Maintainer: <logan@bandwidthd.lp-programming.com>
Rules-Requires-Root: no
Build-Depends:
 libc++-19-dev, clang-19, postgresql-common-dev, libpcap0.8-dev
Standards-Version: 4.7.2
Homepage: https://github.com/lp-programming/bandwidthd-ng
Package: bandwidthd-ng
Architecture: amd64
Depends: libpcap0.8-dev, postgresql-client-common, libc++-19-dev
Description: bandwidthd-ng
 bandwidthd-ng, a drop-in replacement for the classic bandwidthd
'''


import sys
import os
import jumbo
from targets import *

os.mkdir("target")
os.mkdir("target/DEBIAN")
with open("target/DEBIAN/control", "w") as f:
    print(control, file=f)
with open("target/DEBIAN/copyright", "w") as f:
    print(copyright, file=f)

os.system("git clone https://github.com/jtv/libpqxx libpqxx")
os.chdir("libpqxx")
os.system("git checkout 156d1f8")
os.chdir("..")
os.mkdir("libpqxx_build")
os.chdir("libpqxx_build")
    
assert not os.system('CC=clang-19 CXX=clang++-19 CXXFLAGS=" -stdlib=libc++ " cmake ../libpqxx -DBUILD_TEST=OFF -DBUILD_SHARED_LIBS=off  -DCMAKE_CXX_STANDARD=26 -DCMAKE_INSTALL_PREFIX=`pwd`/static_pqxx/')
assert not os.system('CC=clang-19 CXX=clang++-19 CXXFLAGS=" -stdlib=libc++ " make -j `nproc`')
assert not os.system('CC=clang-19 CXX=clang++-19 CXXFLAGS=" -stdlib=libc++ " make install')
os.chdir("..")
pqxx.pkgconf_flags = (f"--with-path={os.getcwd()}/libpqxx_build/static_pqxx/lib/pkgconfig")
pqxx.package['release'] = Package.find_package("libpqxx", ["--with-path=libpqxx_build/static_pqxx/lib/pkgconfig"], link_mode=static | shared)
pq = Package.find_package("libpq", [], link_mode=shared)
pqxx.package['release'].libs.extend(pq.libs)

assert not jumbo.main((f"bin/live", "release"))
os.environ['CXXFLAGS'] = "-lpqxx"

for f in ["bandwidthd", "graph", "psqlsensor"]:
    assert not jumbo.main((f"bin/{f}", "release"))

os.mkdir("target/usr")
os.mkdir("target/usr/bin")

for k,v in dict(live="-live", bandwidthd="", graph="-graph", psqlsensor="-psql").items():
    os.rename(f"build/bin/{k}", "target/usr/bin/bandwidthd{v}")

os.chdir("target")
os.system("dpkg-deb --root-owner-group  --build . ../bandwidthd-ng_0.1.1.deb")

