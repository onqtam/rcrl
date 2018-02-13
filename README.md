## RCRL (Read-Compile-Run-Loop) - a tiny embeddable REPL analog for C++

[![Windows status](https://ci.appveyor.com/api/projects/status/fp0sqit57eorgswb/branch/master?svg=true)](https://ci.appveyor.com/project/onqtam/rcrl/branch/master)
[![Linux Status](https://travis-ci.org/onqtam/rcrl.svg?branch=master)](https://travis-ci.org/onqtam/rcrl)
[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![License](http://img.shields.io/badge/license-MIT-blue.svg)](http://opensource.org/licenses/MIT)

RCRL is a tiny engine for interactive C++ compilation and execution (implemented in just a few source files - [```src/rcrl```](src/rcrl)) and works on any platform with any toolchain - the main focus is easy integration. It supports:
- mixing includes, type/function definitions, persistent variable definitions and statements meant only for function scope
- interacting with the host application through dll-exported symbols (after linking to it)

It is an elegant alternative to [cling](https://github.com/root-project/cling) (and [other projects](https://github.com/inspector-repl/inspector) that build on top of it).

This repository is a demo project with GUI but the RCRL engine can be integrated in any way with host applications - code may be submitted even from external editors with full syntax highlighting and code completion! The goal was not to make a one-size-fits-all solution because that is hardly possible - but to demonstrate how the core of RCRL can be integrated.

Checkout this [blog post](http://onqtam.com/programming/2018-02-12-read-compile-run-loop-a-tiny-repl-for-cpp/) if you are curious how to use it, how it works and how to integrate it. And watch this video:

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
