## RCRL (Read-Compile-Run-Loop) - a tiny embeddable REPL analog for C++

[![Windows status](https://ci.appveyor.com/api/projects/status/fp0sqit57eorgswb/branch/master?svg=true)](https://ci.appveyor.com/project/onqtam/rcrl/branch/master)
[![Linux Status](https://travis-ci.org/onqtam/rcrl.svg?branch=master)](https://travis-ci.org/onqtam/rcrl)
[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![License](http://img.shields.io/badge/license-MIT-blue.svg)](http://opensource.org/licenses/MIT)

RCRL is a tiny engine for interactive C++ compilation and execution (implemented in just a few source files) and works on any platform with any toolchain - the main focus is easy integration. It supports:
- interacting with the host application through dll-exported symbols
- mixing includes, type/function definitions, persistent variable definitions and statements with side effects that are meant to reside in function scope

It is an elegant alternative to [cling](https://github.com/root-project/cling) and everything that [uses it](https://github.com/inspector-repl/inspector).

Checkout this [blog post](http://onqtam.com/programming/2018-02-12-read-compile-run-loop-a-tiny-repl-for-cpp/) if you are curious how this works and watch the video.

[![youtube video showcase](http://onqtam.com/assets/images/rcrl_youtube_thumbnail.png)](https://www.youtube.com/watch?v=HscxAzFc2QY)

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
