#pragma once


// fixed point configuration
#define WORKLIST_INTERFERENCE false

// encoding configuration
#define MERGE_VALID_PTR true

// interference configuration
#define SKIP_NOOPS true // ad hoc
#define KILL_IS_NOOP true

// concretisation configuration
#define REPEAT_PRUNING true

// AST configuration
#define PRINT_ID true // prints statement ids (obfuscates code)

// DGLM precision
#define AD_HOC_PRECISION true
#define DGLM_HINT false