
# TinyOS v.3

TinyOS is a very small operating system, built on top of a simple-minded virtual machine, whose purpose is
purely educational. It is not related in any way to the well-known operating system for wireless sensors,
but since it was first conceived in 2003, there was a name collision that I have not yet resolved.
This code (in its long history) has been used for many years to teach the Operating Systems course
at the Technical University of Crete.

In its current incarnation, tinyos supports a multicore preemptive scheduler, serial terminal devices, and a
unix like process model. It does not support (yet) memory management, block devices, or network devices. These
extensions are planned for the future.

## Quick start

After downloading the code, just build it.
```
$ make
```
If all goes well, the code should build without warnings. Then, you can run your first instance of tinyos,
a simulation of Dijkstra's Dining Philosophers.
```
$ ./mtask 1 0 5 5
FMIN = 27    FMAX = 37
*** Booting TinyOS
[T] .  .  .  .      0 has arrived
[E] .  .  .  .      0 is eating
[T] .  .  .  .      0 is thinking
[E] .  .  .  .      0 is eating
 E [T] .  .  .      1 has arrived
 E [H] .  .  .      1 waits hungry
 E  H [T] .  .      2 has arrived
< more lines deleted >
```

Then, you are ready to start reading the documentation (you will need `doxygen` to build it)
```
make doc
```
Point your browser at file  `doc/html/index.html`.  Happy reading!


### Build dependencies

Tinyos is developed, and will probably only run on Linux (its bios.c file uses Linux-specific system 
calls, in particular signal streams). Any recent (last few years) version of Linux should be sufficient.

Working with the code, at the basic level, requires a recent GCC compiler (with support for C11). The
standard packages `doxygen` and `valgrind` with their dependencies (e.g., `graphviz`) are also needed 
for anything serious, as well as the GDB debugger.

### Regression testing

A regression test wrapper of `validate_api` is provided in this implementation. Simply use the following command:

```
regression/run.sh {suite} -c {numCores} -t {num}
```

where `{suite}` is one of the suites provided by `validate_api`. `-c` is option used for the number of cores, 
and `-t` for the number of terminals. 
If a difference is found between the "golden" result and the current one, the user is prompted to decide whether to replace 
the golden file with the current results.

Note that `colordiff` is required for this wrapper. If someone does not wish to install it, it can easily be replaced 
by `diff` without a loss in functionality.

Finally, the following command allows you to to rerun all tests with golden files automatically:

```
regression/rerunAll.sh
```