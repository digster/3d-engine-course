#!/usr/bin/env python3
"""Exercise 8.2.2, checked: which float masses fail a = (g * (1/w)) * w, w = 1/m, g = -9.81f —
the force route's three roundings, as Lesson 8.2 §5.3 describes them. Every operation is
rounded to float32. It reproduces the lesson's 16% before trusting anything else."""
import struct
F = lambda x: struct.unpack('f', struct.pack('f', x))[0]
BITS = lambda x: struct.unpack('I', struct.pack('f', x))[0]
FROM = lambda b: struct.unpack('f', struct.pack('I', b))[0]
g = F(-9.81)
def fails(m):
    w = F(1.0 / m)
    return F(F(g * F(1.0 / w)) * w) != g
N = 200001
bad = sum(fails(F(10 ** (-3 + 6 * i / (N - 1)))) for i in range(N))
print("log-uniform sweep, 1 g .. 1000 t: %.2f%% fail (the lesson: 15.99%%)" % (100.0 * bad / N))
b = BITS(F(10.0)) - 1
while not fails(FROM(b)):
    b -= 1
print("largest failing mass below 10 kg: %.9g kg  (%s)" % (FROM(b), FROM(b).hex()))
# structure: doubling a mass halves w exactly, so the failure set repeats every binade
for lo in (1.0, 2.0, 4.0, 8.0):
    b0, b1 = BITS(F(lo)), BITS(F(lo * 2))
    n = f = 0
    for bb in range(b0, b1, 61):
        n += 1; f += fails(FROM(bb))
    print("  [%g, %g): %.2f%% fail" % (lo, 2 * lo, 100.0 * f / n))
# where in the binade: failure fraction per tenth of [1, 2)
b0 = BITS(1.0)
cells = [0] * 10; tot = [0] * 10
for bb in range(b0, BITS(2.0), 17):
    m = FROM(bb); k = min(9, int((m - 1.0) * 10))
    tot[k] += 1; cells[k] += fails(m)
print("  by tenth of [1,2):", " ".join("%.0f%%" % (100.0 * c / t) for c, t in zip(cells, tot)))
