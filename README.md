# RCRL (Read-Compile-Run-Loop) - a tiny embeddable REPL analog for C++

[![Windows status](https://ci.appveyor.com/api/projects/status/fp0sqit57eorgswb/branch/master?svg=true)](https://ci.appveyor.com/project/onqtam/rcrl/branch/master)
[![Linux Status](https://travis-ci.org/onqtam/rcrl.svg?branch=master)](https://travis-ci.org/onqtam/rcrl)
[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![License](http://img.shields.io/badge/license-MIT-blue.svg)](http://opensource.org/licenses/MIT)

Checkout this [blog post](http://onqtam.com/programming/2018-02-12-read-compile-run-loop-a-tiny-repl-for-cpp/) if you are curious how this works.

## Building

The demo is tested on Windows/Linux/MacOS and uses OpenGL 2.

You will need:
- CMake 3.0 or newer
- A C++14 capable compiler (tested with VS 2015+, GCC 5+, Clang 3.6+)

The repository makes use of a few third party libraries and they are setup as submodules of the repo (in ```src/third_party/```). Here are the steps you'll need to setup, build and run the project:

- ```git submodule update --init``` - checks out the submodules
- ```cmake path/to/repo``` - call cmake to generate the build files
- ```cmake --build .``` - compiles the project
- the resulting binary is ```host_app``` in ```bin``` of the build folder
