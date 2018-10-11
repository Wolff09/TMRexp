#pragma once


// fixed point configuration
#define WORKLIST_INTERFERENCE false

// encoding configuration
#define MERGE_VALID_PTR true

// interference configuration
#define SKIP_NOOPS true
#define KILL_IS_NOOP true
#define MAX_PRUNE_ITERATIONS 2

// concretisation configuration
#define REPEAT_PRUNING true

// AST configuration
#define PRINT_ID true // prints statement ids (obfuscates code)

// making DGLM work
#define DGLM_HINT false // set to: true for DGLM, false otherwise
#define DGLM_PRECISION false // not required