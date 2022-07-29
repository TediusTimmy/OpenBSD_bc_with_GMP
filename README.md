OpenBSD bc with GMP
===================

I decided to do what I did with GNU bc, and rewire the OpenBSD version of bc to use GMP. This was much easier than GNU bc, as OpenBSD's bc already used the BIGNUM library from OpenSSL to handle math. The main big difference is that GMP doesn't report errors from operations. It either fails to allocate memory, which causes it to abort, or it succeeds. There is also a slight difference in the division routines and how one gets quotient and remainder that caused me to crash at first.

It should compile with Cygwin. I didn't change it to compile under the cross-compiler.


Speed Races
-----------

I thought that it would be interesting to compare the before and after speeds, and also to compare GNU bc with GMP. In addition, I added the FreeBSD version of bc (https://github.com/gavinhoward/bc). I recycled an LCG PRNG that I wrote years ago called rand850. It uses constants that are 256 decimal digits in size, and has a 256 decimal digit state (which is approximately 850 bits). It returns a number uniformly distributed in [0,1). So, you have multiplications that produce a 512 digit answer and divisions of 256+ digits. It's pretty numerically intensive.

The benchmark program starts with a seed of zero and adds two hundred thousand results from the random number generator, at seventy-five digits of scale. It was designed for GNU bc to take about a minute on my computer for a run. There are three versions: the first is the naive approach that recomputes the value of the constants each call; the second precomputes the constants and stores them in variables; and the third has the value of the constants in the source and sets them every call. The third was designed to give "GNU bc with GMP" a handicap, as GNU bc stores the number in the bytecode as digits, and has to reconstitute the number every call from its constituent digits. This is actually rather slow, and the performance really does suffer because of it.

| Program | Time 1 | Times faster than GNU bc | Time 2 | Times faster than GNU bc | Time 3 | Times faster than GNU bc |
| ------------------- | ----- | ---- | ----- | ---- | ----- | ---- |
| GNU bc              | 55.06 |  1   | 44.50 |  1   | 44.83 |  1   |
| GNU bc with GMP     |  1.80 | 30.6 |   .59 | 74.9 |  2.17 | 20.7 |
| FreeBSD bc          |  3.83 | 14.4 |  1.90 | 23.4 |  1.98 | 22.7 |
| OpenBSD bc          |  3.25 | 16.9 |  1.57 | 28.3 |  4.87 |  9.2 |
| OpenBSD bc with GMP |  1.73 | 31.9 |   .79 | 56.6 |  3.08 | 14.5 |

Interestingly, for the users of a binary arbitrary-precision number library, it is faster to reconstitute the constants from their prime factorization than to read the full number in decimal form.

Notes: GNU bc was configured with readline support. FreeBSD bc was configured with editline support and "-msse4 -flto -O3".
