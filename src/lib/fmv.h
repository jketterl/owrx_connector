#ifndef FMV_H

#ifdef OWRX_CONNECTOR_FMV
#if defined(__has_attribute)
#if __has_attribute(target_clones)
#if defined(__x86_64)
#define OWRX_CONNECTOR_TARGET_CLONES __attribute__((target_clones("avx","sse4.2","sse3","sse2","default")))
#endif
#endif
#endif
#endif

#ifndef OWRX_CONNECTOR_TARGET_CLONES
#define OWRX_CONNECTOR_TARGET_CLONES
#endif

#endif