TMRexp
======


This tool is an experimental model checker for verifying linearizability [1] of lock-free data structures with safe memory reclamation (SMR) against an unbounded number of client threads.
The tool is able to handle singly-linked, data independent data structures such as Treiber's stack [2], Michael&Scott's queue [3a,3b], and the DGLM queue [4] using epoch-based reclamation [5] and hazard pointers [6].

The tool integrates:
   - The basic analysis from [7] which uses
      - Wolper's data independence argument
      - Observer automata to detect non-linearizable executions
      - Shape Analysis
      - Thread-Modularity
   - The concept of 'valid pointers' from [6] which
      - allows to rely on 1-thread views (not 2-thread views like [7]) resulting in better performance/scalability
      - increases precision of interference

The main purpose of this implementation is to showcase the usefulness of the results from [9]:
   - compositional verification of the data structures against an SMR specification rather than the actual SMR implementation
   - a simpler analysis which
      - considers reallocations of a single cell only, and
      - perform an ABA check on top of the fixed point to guarantee soundness

For more details visit: https://wolff09.github.io/TMRexp/


[1] M. Herlihy and N. Shavit. The art of multiprocessor programming. Morgan Kaufmann, 2008.  
[2] R. Treiber. Systems programming: coping with parallelism. Technical Report RJ 5118. IBM, 1986.  
[3a] M. Michael and M. Scott. Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms. In PODC. ACM, 1996.  
[3b] M. Michael. Safe memory reclamation for dynamic lock-free objects using atomic reads and writes. In PODC. ACM, 2002.  
[4] S. Doherty, L. Groves, V. Luchangco, and M. Moir. Formal Verification of a Practical Lock-Free Queue Algorithm. In FORTE. Springer, 2004.  
[5] K. Fraser. Practical lock-freedom. Ph.D. Dissertation. University of Cambridge, UK. 2004.  
[6] M. Michael. Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects. In IEEE Transactions on Parallel and Distributed Systems. 2002.  
[7] P. A. Abdulla, F. Haziza, L. Holík, B. Jonsson, and A. Rezine. An integrated specification and verification technique for highly concurrent data structures. In TACAS. Springer, 2013.  
[8] F. Haziza, L. Holík, R. Meyer, and S. Wolff. Pointer race freedom. In VMCAI. Springer, 2016.  
[9] M. Meyer and S. Wolff. Decoupling Lock-Free Data Structures from Memory Reclamation for Static Analysis. Accepted for POPL'19.  


Code
----

The are various versions of our model checker which can be found on the following branches.
   - `master` Documentation/Webpage only, no code
   - `chkdsd` Implementation for verifying lock-free data structures relative to an SMR specification (for POPL'19)
   - `chkdsmr` Implementation for verifying an SMR implementation against its specification (for POPL'19)
   - `summary` Implementation with novel fixed point for verifying lock-free data structures without SMR (for SAS'17)
   - `summary-gc` Implementation with novel fixed point for verifying lock-free data structures relying on GC (for SAS'17)


Compiling
---------

For build instructions consider corresponding branch.
