#if defined(__x86_64)
#if defined(__has_attribute)
#if __has_attribute(target_clones)
#define OWRX_CONNECTOR_TARGET_CLONES __attribute__((target_clones("avx","sse4.2","sse3","sse2","default")))
#endif
#endif
#endif

#ifndef OWRX_CONNECTOR_TARGET_CLONES
#define OWRX_CONNECTOR_TARGET_CLONES
#endif