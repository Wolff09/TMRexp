#pragma once

// fixed point configuration
#define REPLACE_INTERFERENCE_WITH_SUMMARY true

// summary configurations
#define USE_MODIFIED_FIXEDPOINT true
#define SUMMARY_OPTIMIZATION true
#define SUMMARY_CHKMIMIC true // required for soundness

// interference configuration
#define SKIP_NOOPS true // ad hoc
#define KILL_IS_NOOP true
#define INTERFERENCE_OPTIMIZATION false // ad hoc

// AST configuration
#define PRINT_ID true // prints statement ids (obfuscates code)
