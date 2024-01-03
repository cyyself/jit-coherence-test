# JIT-Coherence-Test

Understand microarchitecture features of I-D Coherence.

## How does it work?

This program will generate jitted instructions with a random immediate number and store the number to a pointer which points to a global variable. Then create a separate thread to execute the jitted instructions continuously and controlled by a shared variable as start and stop signals. When the thread raised a finished signal with a different result value, the JIT coherence is corrupted.

You can also try some ISA-dependent options to see what will happen if the default cache clean procedures change and then violate JIT coherence.

## Usage:

```console
./test <number_of_loops=100> <ISA-dependent options>
```

To see what will happen if force enables DIC on aarch64, you can:

```console
./test 10000 -fdic
```

## Supported ISA and ISA-dependent options

- aarch64
    - `-fidc`: Do not explicitly clear D-Cache
    - `-fdic`: Do not explicitly clear I-Cache
- riscv64
    - `-flocal`: Not use `__clear_cache` from libc and only executes `fence.i` on writer hart
