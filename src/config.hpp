#pragma once


// fixed point configuration
#define WORKLIST_INTERFERENCE false
#define REPLACE_INTERFERENCE_WITH_SUMMARY false
#define MERGE_VALID_PTR false

// summary configurations
#define USE_MODIFIED_FIXEDPOINT true
#define SUMMARY_OPTIMIZATION true
#define SUMMARY_CHKMIMIC true // required for soundness

// interference configuration
#define SKIP_NOOPS true // ad hoc
#define KILL_IS_NOOP true
#define INTERFERENCE_OPTIMIZATION false // ad hoc

// memory configuration
#define CAS_OVERAPPROXIMATE_AGE_ASSIGNMENT true
#define CAS_OVERAPPROXIMATE_AGE_PROPAGATION true
#define PRF_REALLOCATION true

// AST configuration
#define PRINT_ID true // prints statement ids (obfuscates code)


// allow to prune shape relations based on hints provided by the program
#define HINTING true