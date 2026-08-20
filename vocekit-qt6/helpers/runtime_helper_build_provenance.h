#pragma once

#include <iostream>

#ifndef VOCEKIT_HELPER_SOURCE_COMMIT
#error VOCEKIT_HELPER_SOURCE_COMMIT must be supplied by the trusted runtime helper build.
#endif

#ifndef VOCEKIT_HELPER_SOURCE_TREE_CLEAN
#error VOCEKIT_HELPER_SOURCE_TREE_CLEAN must be supplied by the trusted runtime helper build.
#endif

#ifndef VOCEKIT_HELPER_CONFIGURATION
#error VOCEKIT_HELPER_CONFIGURATION must be supplied by the trusted runtime helper build.
#endif

inline int writeRuntimeHelperBuildProvenance(const char *helperName)
{
    // All interpolated values are trusted build-time ASCII constants: helper
    // names are fixed below, the commit is validated hexadecimal, and the
    // configuration is exactly Release. Avoiding WinRT here also keeps this
    // probe safe before the helpers initialize their normal runtime.
    std::cout
        << "{\"schema_version\":1,"
        << "\"kind\":\"vocekit-runtime-helper-build-provenance\","
        << "\"helper_name\":\"" << helperName << "\","
        << "\"source_commit\":\"" << VOCEKIT_HELPER_SOURCE_COMMIT << "\","
        << "\"source_tree_clean\":"
        << (VOCEKIT_HELPER_SOURCE_TREE_CLEAN != 0 ? "true" : "false") << ","
        << "\"configuration\":\"" << VOCEKIT_HELPER_CONFIGURATION << "\"}"
        << std::endl;
    return 0;
}
