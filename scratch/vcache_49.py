#!/usr/bin/env python3
"""Exercise 4.9.6, checked: a FIFO post-transform cache over torus.obj, shipped vs shuffled,
and Tom Forsyth's linear-speed optimiser (2006 constants). Run from the repository root.
Reproduces Lesson 4.9 §3.6 (2,400 at cache 8/16/32, 1,225 at 64) before trusting the rest."""
# Vertex-cache simulation for Exercise 4.9.6: FIFO cache over torus.obj's shipped index order,
# a shuffled order, and Tom Forsyth's linear-speed optimiser applied to both.
import random, math, sys
tris=[]; keys={}
for line in open("assets/torus.obj"):
    if line.startswith("f "):
        # a vertex is the whole v/vt/vn tuple, as the loader welds it (Lesson 3.5)
        tris.append(tuple(keys.setdefault(t, len(keys)) for t in line.split()[1:4]))
nv=len(keys)
def fifo(order, size):
    cache=[]; inv=0
    for t in order:
        for v in t:
            if v not in cache:
                inv+=1; cache.append(v)
                if len(cache)>size: cache.pop(0)
    return inv
# Forsyth (2006), constants from his article: cache 32, decay 1.5, last-tri 0.75, valence 2.0/0.5
CS=32
def vscore(pos, remaining):
    if remaining==0: return -1.0
    s=0.0
    if pos>=0:
        if pos<3: s=0.75
        else: s=(1.0-(pos-3)/(CS-3))**1.5
    return s+2.0*remaining**-0.5
def forsyth(order):
    vt=[[] for _ in range(nv)]
    for i,t in enumerate(order):
        for v in t: vt[v].append(i)
    rem=[len(x) for x in vt]; pos=[-1]*nv
    vs=[vscore(-1,rem[v]) for v in range(nv)]
    ts=[sum(vs[v] for v in t) for t in order]; done=[False]*len(order)
    cache=[]; out=[]; nxt=0
    best=max(range(len(order)), key=lambda i: ts[i])
    while best is not None:
        t=order[best]; done[best]=True; out.append(t)
        for v in t:
            rem[v]-=1; vt[v].remove(best)
            if v in cache: cache.remove(v)
            cache.insert(0,v)
        evicted=cache[CS:]; cache=cache[:CS]
        for v in evicted: pos[v]=-1
        touched=set(cache)|set(evicted)
        for i,v in enumerate(cache): pos[v]=i
        for v in touched: vs[v]=vscore(pos[v],rem[v])
        cand=set()
        for v in touched:
            for ti in vt[v]: cand.add(ti)
        best=None; bs=-1
        for ti in cand:
            ts[ti]=sum(vs[v] for v in order[ti])
            if ts[ti]>bs: bs=ts[ti]; best=ti
        if best is None:
            while nxt<len(order) and done[nxt]: nxt+=1
            if nxt<len(order): best=nxt
    return out
shipped=tris
sh=tris[:]; random.Random(49).shuffle(sh)
print("vertices",nv,"triangles",len(tris))
for name,o in [("shipped",shipped),("shuffled",sh),("forsyth(shipped)",forsyth(shipped)),("forsyth(shuffled)",forsyth(sh))]:
    print(f"{name:20s}", " ".join(f"c{c}:{fifo(o,c)}" for c in (8,16,32,64)))
