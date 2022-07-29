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
   rand850a = 4 * 5 * 32771 ^ 53 * 65537 ^ 3 + 1
   rand850c = 65537 ^ 53
   rand850m = 10 ^ 256
define rand850 () {
   t = scale
   scale = 0
   rand850s = (rand850a * rand850s + rand850c) % rand850m
   scale = t
   return rand850s / rand850m
}

sum = 0
for (i = 0; i < top; i++) {
   sum = sum + rand850()
}

sum
quit
