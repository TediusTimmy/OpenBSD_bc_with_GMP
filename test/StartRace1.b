/*
   These two control the timing:
      top is the number of iterations
      scale controls the number of digits
*/
top = 200000
scale = 75

/*
   Rand850 is an LCG PRNG by Thomas DiModica.
   It's period is 10^256, which is approximately 2^850.
*/
rand850s = 0
define rand850 () {
   a = 4 * 5 * 32771 ^ 53 * 65537 ^ 3 + 1
   c = 65537 ^ 53
   m = 10 ^ 256
   t = scale
   scale = 0
   rand850s = (a * rand850s + c) % m
   scale = t
   return rand850s / m
}

sum = 0
for (i = 0; i < top; i++) {
   sum = sum + rand850()
}

sum
quit
