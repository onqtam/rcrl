# rcrl
Read-Compile-Run-Loop - a REPL variant for C++ (much lighter than cling) - WILL WORK ON IT IN THE FOLLOWING DAYS

[![Windows status](https://ci.appveyor.com/api/projects/status/fp0sqit57eorgswb/branch/master?svg=true)](https://ci.appveyor.com/project/onqtam/rcrl/branch/master)
[![Linux Status](https://travis-ci.org/onqtam/rcrl.svg?branch=master)](https://travis-ci.org/onqtam/rcrl)
[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![License](http://img.shields.io/badge/license-MIT-blue.svg)](http://opensource.org/licenses/MIT)

TODO:

- get the 3 sections working
- implement cleanup
- use a process library to get the output from the compiler
- make the execution non-blocking
    - synchronization
    - objects spinning in the background
- redirect the output temporarily while loading the lib?
- factor it in a few easy-to-use functions

- make it work under linux
- add a PCH
- read up on 'lexer programming'
- http://cppnow.org/2018-conference/announcements/2017/12/03/call-for-submission.html
