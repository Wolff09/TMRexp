#pragma once

// fixed point configuration
#define REPLACE_INTERFERENCE_WITH_SUMMARY true

// summary configurations
#define USE_MODIFIED_FIXEDPOINT true
#define SUMMARY_OPTIMIZATION true

// interference configuration
#define SKIP_NOOPS true
#define KILL_IS_NOOP true
#define INTERFERENCE_OPTIMIZATION true // ad hoc

// AST configuration
#define PRINT_ID false // prints statement ids (obfuscates code)