Library
-------

The code for the analysis can be found in `src/`.


Experiments
-----------

Test cases can be found in `test/`.
There are three binaries for showcasing the tool:

   1. ~~Coarse/CoarseQueue~~ (one should use the master branch instead)
   2. ~~Stack/TreibersStack~~ (one should use the master branch instead)
   3. Queue/MSQueue

Each binary comes with a short help page when called like so:
```
path/to/binary help
```


Compiling
---------

To compile this project one needs CMake and an C++11 compiler.
Compilation was tested with Apple LLVM version 7.0.0 (clang-700.0.57.2)
and Ubuntu clang version 3.4-1 (based on LLVM 3.4).
Other compilers have not been tested so far.

To make the project, simply type `make` in the root directory.
This creates a `build/` folder where all binaries are added to.
`make` additionally executes the CTest test suite mentioned above.
However, beware that the tests may take some time.

Alternatively, you can do
```
mkdir build
cd build
cmake ..
make
```