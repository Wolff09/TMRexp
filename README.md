TMRexp
======


This project resembles a model checker for concurrent heap-manipulating singly linked data structures.
It checks for the well-known correctness criterion of linearisability [1].
The checker is able to establish correctness for an unbounded number of threads.
The analysis is conducted along the lines of [2].
That is, we integrated
   - Wolper's data independence argument
   - Observer automata
   - Shape Analysis
   - Thread-Modularity

Moreover, this tool supports checking for subtle programming bugs due to low-level memory operations.
We are able to detect the ABA problem in presence of explicit memory management like in C.

The main novelty of this model checker is that it implements the so-called ownership-respecting memory semantics introduced by [3].
It allows to conduct a thread-modular analysis in the style of an garbage collected analysis.
However, the results carry over to the memory managed semantics.
As a result we experience a speed-up of up to two orders of magnitude (compared to an analysis for explicit memory management).

The implementation of this tool was part of my Master's Thesis.


[1] M. Herlihy and N. Shavit. The art of multiprocessor programming. Morgan Kaufmann, 2008.
[2] P. A. Abdulla, F. Haziza, L. Holík, B. Jonsson, and A. Rezine. An inte- grated specification and verification technique for highly concurrent data structures. In N. Piterman and S. A. Smolka, editors, Tools and Algorithms for the Construction and Analysis of Systems - 19th International Confer- ence, TACAS 2013, Held as Part of the European Joint Conferences on Theory and Practice of Software, ETAPS 2013, Rome, Italy, March 16- 24, 2013. Proceedings, volume 7795 of Lecture Notes in Computer Science, pages 324–338. Springer, 2013.
[3] F. Haziza, L. Holík, R. Meyer, and S. Wolff. Pointer race freedom. 2015. Accepted for VMCAI 2016.


Library
-------

The code for the analysis can be found in `src/`.


Experiments
-----------

Test cases can be found in `test/`.
There are three binaries for showcasing the tool:

   1. Coarse/CoarseQueue
   2. Stack/TreibersStack
   3. ~~Queue/MSQueue~~ (one should use the branch next-age instead)

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
