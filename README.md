# rcrl
Read-Compile-Run-Loop - a REPL variant for C++ (much lighter than cling)

[![Windows status](https://ci.appveyor.com/api/projects/status/fp0sqit57eorgswb/branch/master?svg=true)](https://ci.appveyor.com/project/onqtam/rcrl/branch/master)
[![Linux Status](https://travis-ci.org/onqtam/rcrl.svg?branch=master)](https://travis-ci.org/onqtam/rcrl)
[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![License](http://img.shields.io/badge/license-MIT-blue.svg)](http://opensource.org/licenses/MIT)

TODO:

- get the 3 sections working
    - buttons in the GUI
    - tabbing between them with the keyboard
    - constructing the text from them
- implement cleanup
- parameterize which headers should be included
- think about removing the asserts and throwing exceptions instead...
- document the API - like any specifics for each call
- something more interesting should happen in the background
- redirect the output temporarily while loading the lib?

- test under linux/osx (currently only compiles)
- add a PCH
- read up on 'lexer programming'
- http://cppnow.org/2018-conference/announcements/2017/12/03/call-for-submission.html
