# Exercise 7.4.2: how far from orthonormal is mat3_from_quat's output, in float32?
# Columns are rotate(q, e_i) with rotate = v + t*w + cross(q.v, t), t = 2*cross(q.v, v).
import struct, random, math
F=lambda x: struct.unpack('f',struct.pack('f',x))[0]
def cross(a,b): return (F(F(a[1]*b[2])-F(a[2]*b[1])),F(F(a[2]*b[0])-F(a[0]*b[2])),F(F(a[0]*b[1])-F(a[1]*b[0])))
def rot(q,v):
    w,qv=q[0],q[1:]
    t=tuple(F(c*2.0) for c in cross(qv,v)); u=cross(qv,t)
    return tuple(F(F(v[i]+F(t[i]*w))+u[i]) for i in range(3))
r=random.Random(742); worst_o=0; worst_d=0
for _ in range(20000):
    g=[r.gauss(0,1) for _ in range(4)]; n=math.sqrt(sum(x*x for x in g))
    q=tuple(F(x/n) for x in g)             # a float unit quaternion, as the engine would hold it
    cols=[rot(q,e) for e in ((1,0,0),(0,1,0),(0,0,1))]
    for i in range(3):
        for j in range(3):
            d=sum(cols[i][k]*cols[j][k] for k in range(3))   # (M^T M)_ij, in double
            worst_o=max(worst_o,abs(d-(1.0 if i==j else 0.0)))
    a,b,c=cols
    det=a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0])
    worst_d=max(worst_d,abs(det-1))
print("worst |M^T M - I| = %.3e   worst |det - 1| = %.3e"%(worst_o,worst_d))
