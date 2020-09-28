#if defined(__x86_64)
__attribute__((target_clones("avx","sse4.2","sse3","sse2","default")))
#endif
void ifunc_test() {}
int main(int argc, char** argv) {}