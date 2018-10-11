TMRexp
======


This tool is a model checker for singly-linked, lock-free data structures.
It checks whether the implementation under scrutiny is linearizable [1] in the presence of unboundedly many client threads.
We integrated:
   - The basic analysis from [2] which uses
      - Wolper's data independence argument
      - Observer automata to detect non-linearizable executions
      - Shape Analysis
      - Thread-Modularity
   - The concept of 'valid pointers' from [3] which
      - allows to rely on 1-thread views (not 2-thread views like [2]) resulting in better performance/scalability
      - increases precision of interference
   - A novel reduction that allows to consider reallocations of a single cell only [4]

The main novelty of this model checker is that it allows programs to use *Hazard Pointers* [5] for safe memory reclamation.
We assume that the program under consideration uses a hazard pointer implementation through an API.
We specify the behavior of that implementation using observer automata.


[1] M. Herlihy and N. Shavit. The art of multiprocessor programming. Morgan Kaufmann, 2008.  
[2] P. A. Abdulla, F. Haziza, L. Holík, B. Jonsson, and A. Rezine. An integrated specification and verification technique for highly concurrent data structures. In N. Piterman and S. A. Smolka, editors, Tools and Algorithms for the Construction and Analysis of Systems - 19th International Conference, TACAS 2013, Held as Part of the European Joint Conferences on Theory and Practice of Software, ETAPS 2013, Rome, Italy, March 16- 24, 2013. Proceedings, volume 7795 of Lecture Notes in Computer Science, pages 324–338. Springer, 2013.  
[3] F. Haziza, L. Holík, R. Meyer, and S. Wolff. Pointer race freedom. In VMCAI, volume 9583 of Lecture Notes in Computer Science, pages 393–412. Springer, 2016.  
[4] *under submission*
[5] M. Michael. 2002. Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects. In IEEE Transactions on Parallel and Distributed Systems. 2002.



Library
-------

The code for the analysis can be found in `src/`.

The configuration file can be found at `src/config.hpp`.


Experiments
-----------

Test cases can be found in `test/`.
There are several binaries for showcasing the tool.


Compiling
---------

To compile this project one needs CMake, an C++17 compiler, and Boost `multi_array`.
Compilation was tested with Apple LLVM version 9.1.0 (clang-902.0.39.1),
and GNU g++ version 7.3.0.

To build the project, simply type `make` in the root directory.
This creates a `build/` folder where all binaries are added to.

Alternatively, you can do
```
mkdir build
cd build
cmake ..
make
```
