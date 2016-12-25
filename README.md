TMRexp
======


This tool is a model checker for for singly-linked, lock-free data structures.
It checks whether the implementation under scrutiny is linearizable in the presence of unboundedly many client threads.
The analysis is conducted along the lines of [2].
That is, we integrated:
   - Wolper's data independence argument
   - Observer automata
   - Shape Analysis
   - Thread-Modularity

Our tool supports checking for subtle programming bugs due to low-level memory operations.
We are able to detect the ABA problem in presence of explicit memory management like in C.

On top of that we integrated several advancements.
The main novelty of this model checker is that it implements the ownership-respecting memory semantics introduced in [3].
It allows to conduct a thread-modular analysis in the style of an garbage collected analysis.
However, the results carry over to the memory managed semantics.
As a result we experience a speed-up of up to two orders of magnitude (compared to an analysis for explicit memory management).

The implementation of this tool was part of my Master's Thesis.
During my recent research as a Ph.D. student I continued to work on this tool.

We **overhauled** the analysis from above to be truly scalable.
Therefore, we replaced the interference steps of the thread-modular framework with *thread-summaries*.
This novel approach of ours allows to reduce the problem of verifying a system with an unbounded number of concurrent client threads to the problem of verifying a system with two concurrent threads only.
This new system consists of a *regular* thread and a *summary* thread which is able to mimic the effect of an unbounded number of threads on the shared heap.
Since this is work in progress we do not have a paper on the theoretical foundations yet.
An elevator pitch, however, can be found in [4].


[1] M. Herlihy and N. Shavit. The art of multiprocessor programming. Morgan Kaufmann, 2008.
[2] P. A. Abdulla, F. Haziza, L. Holík, B. Jonsson, and A. Rezine. An integrated specification and verification technique for highly concurrent data structures. In N. Piterman and S. A. Smolka, editors, Tools and Algorithms for the Construction and Analysis of Systems - 19th International Conference, TACAS 2013, Held as Part of the European Joint Conferences on Theory and Practice of Software, ETAPS 2013, Rome, Italy, March 16- 24, 2013. Proceedings, volume 7795 of Lecture Notes in Computer Science, pages 324–338. Springer, 2013.
[3] F. Haziza, L. Holík, R. Meyer, and S. Wolff. Pointer race freedom. In VMCAI, volume 9583 of Lecture Notes in Computer Science, pages 393–412. Springer, 2016.
[4] S. Wolff. Thread Summaries for Lock-Free Data Structures. In Proceedings of the 28th Nordic Workshop on Programming Theory (NWPT'16), 2016.


Library
-------

The code for the analysis can be found in `src/`.

The configuration file can be found at `src/config.hpp`.
You can change between the two analyses (classical thread-modular reasoning vs. thread summaries), select the fixed point engine to be used in summary mode, and turn on/off some optimizations.


Experiments
-----------

Test cases can be found in `test/`.
There are three binaries for showcasing the tool:

   1. Coarse/CoarseQueue
   2. Stack/TreibersStack
   3. Queue/MSQueue

Each binary comes with a short help page when called like so:
```
path/to/binary help
```


Compiling
---------

To compile this project one needs CMake and an C++11 compiler.
Compilation was tested with Apple LLVM version 7.0.0 (clang-700.0.57.2),
Apple LLVM version 8.0.0 (clang-800.0.42.1),
and Clang version 3.8.0.

To build the project, simply type `make` in the root directory.
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
