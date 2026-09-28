import math, random
def enc(x,y,z):
    s=abs(x)+abs(y)+abs(z); x/=s; y/=s
    if z<0:
        x,y=((1-abs(y))*(1 if x>=0 else -1),(1-abs(x))*(1 if y>=0 else -1))
    return x,y
def dec(u,v):
    z=1-abs(u)-abs(v)
    if z<0:
        u,v=((1-abs(v))*(1 if u>=0 else -1),(1-abs(u))*(1 if v>=0 else -1))
    l=math.sqrt(u*u+v*v+z*z); return u/l,v/l,z/l
def q(a,bits):
    m=(1<<(bits-1))-1
    return round(max(-1,min(1,a))*m)/m
def err(bx,by,N=400000,seed=1):
    r=random.Random(seed); worst=0; tot=0
    for _ in range(N):
        z=r.uniform(-1,1); t=r.uniform(0,2*math.pi); s=math.sqrt(1-z*z)
        x,y=s*math.cos(t),s*math.sin(t)
        u,v=enc(x,y,z); a,b,c=dec(q(u,bx),q(v,by))
        d=max(-1,min(1,a*x+b*y+c*z)); e=math.degrees(math.acos(d)) if d<1 else 0.0
        # acos is blind near 1 (the course's own 0.036 deg floor, in float; here double) - use chord
        e=math.degrees(2*math.asin(min(1,math.sqrt((a-x)**2+(b-y)**2+(c-z)**2)/2)))
        worst=max(worst,e); tot+=e
    return worst, tot/N
for bx,by in [(8,8),(16,16),(16,15)]:
    w,m=err(bx,by); print(f"oct {bx}+{by} bits: max {w:.5f} deg, mean {m:.5f} deg")
