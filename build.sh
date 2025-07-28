#!/bin/sh

export CFLAGS="-D_LIBCPP_DISABLE_DEPRECATION_WARNINGS=true -Wno-zero-length-array  "
clang++ $CFLAGS -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic  --precompile -c HeaderView.cppm -o HeaderView.pcm

clang++ $CFLAGS -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic  --precompile -c net_integer.cppm -o net_integer.pcm

clang++ $CFLAGS -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic  --precompile -c EnumFlags.cppm -o EnumFlags.pcm

clang++ $CFLAGS -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic  --precompile -c util.cppm -o util.pcm

clang++ $CFLAGS -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic  --precompile -c EnumFlags.cppm -o EnumFlags.pcm

clang++ $CFLAGS -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic  --precompile -c Types.cppm -o Types.pcm

clang++ $CFLAGS -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic  --precompile -c BandwidthD.cppm -o BandwidthD.pcm

clang++ $CFLAGS -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic  --precompile -c Database.cppm -o Database.pcm

clang++ $CFLAGS -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic  --precompile -c Postgres.cppm -o Postgres.pcm

clang++ $CFLAGS -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic Demo.cpp -o Demo *.pcm -lpcap -lpqxx -lpq


clang++ $CFLAGS -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic Simple.cpp -o Simple *.pcm -lpcap /usr/lib64/libpqxx.so /usr/lib64/libpq.so 

clang++ $CFLAGS -shared -O3 -g -march=native -std=c++26 -stdlib=libc++ -fPIC -fimplicit-modules -fbuiltin-module-map -fmodule-map-file=/usr/include/c++/v1/module.modulemap -fmodule-map-file=./modules.map -fprebuilt-module-path=./ -fmodules-cache-path=./modules -flto -Wold-style-cast -Wall -Wextra -Wpedantic $(python-config --includes) PyBandwidthd.cppm -o PyBandwidthD.so *.pcm -lpcap /usr/lib64/libpqxx.so /usr/lib64/libpq.so $(python-config --ldflags)
