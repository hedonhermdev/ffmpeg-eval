%define USE_TRAMPOLINE
%define USE_ATOMIC
%define USE_SIMD
%define USE_REGWIPE
%define USE_PASSING_BUFFER
%define USE_WRPKRU
%define USE_CHECK_CALLER
%define USE_CHECK_RAX
%include "user-trampoline/trampoline.S"

make_trampoline_MPK_loader_with split, _split, T, A;
make_trampoline_MPK_loader_with extract, _extract, T, B;
make_trampoline_MPK_loader_with shared_allocator, _shared_allocator, A, T;
