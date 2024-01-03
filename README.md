# JIT-Coherence-Test

Understand microarchitecture features of I-D Coherence.

## How does it work?

This program will generate jitted instructions with a random immediate number and store the number to a pointer which points to a global variable. Then create a separate thread to execute the jitted instructions continuously and controlled by a shared variable as start and stop signals. When the thread raised a finished signal with a different result value, the JIT coherence is corrupted.

You can also try some ISA-dependent options to see what will happen if the default cache clean procedures change and then violate JIT coherence.

## Usage:

```console
make
./test <number_of_loops=100> <ISA-dependent options>
```

To see what will happen if force enables DIC on aarch64, you can:

```console
./test 10000 -fdic
```

If there is no output and the return value is zero, then no JIT coherence violation happens.

## Supported ISA and ISA-dependent options

- aarch64
    - `-fidc`: Do not explicitly clear D-Cache
    - `-fdic`: Do not explicitly clear I-Cache
- riscv64
    - `-flocal`: Not use `__clear_cache` from libc and only executes `fence.i` on writer hart

## Some interesting findings:

### AArch64

#### Apple M1

`CTR_EL0.IDC = 1`

`CTR_EL0.DIC = 0`

`-fdic` will fail. Specifically, `ic, ivau [cacheline_addr]` and `dsb ish` must both exist to make cache coherence.

#### Phytium D2000

`CTR_EL0.IDC = 0`

`CTR_EL0.DIC = 0`

`-fidc` will fail sometime.

`-fdic` will fail. Specifically, `ic, ivau [cacheline_addr]` must exist, `dsb ish` seems not required.

### RISC-V 64

#### HiFive Unmatched (Sifive Freedom U740)

`-flocal` will fail.