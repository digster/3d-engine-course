"""Dump a .glb's JSON structure: nodes, skins, animations, accessor types."""
import json, struct, sys
def load(path):
    b = open(path, 'rb').read()
    magic, ver, length = struct.unpack_from('<III', b, 0)
    jl, jt = struct.unpack_from('<II', b, 12)
    j = json.loads(b[20:20+jl])
    bl, bt = struct.unpack_from('<II', b, 20+jl)
    return j, b[28+jl:28+jl+bl]
j, bin_ = load(sys.argv[1])
print('generator:', j['asset'].get('generator'))
print('scene roots:', j['scenes'][j.get('scene',0)]['nodes'])
for i,n in enumerate(j['nodes']):
    print(i, n.get('name'), {k:v for k,v in n.items() if k!='name'})
for s in j.get('skins',[]):
    print('skin', s.get('name'), 'joints', s['joints'], 'ibm acc', s.get('inverseBindMatrices'), 'skeleton', s.get('skeleton'))
for m in j['meshes']:
    for p in m['primitives']:
        print('prim attrs', {k:(v, j['accessors'][v]['componentType'], j['accessors'][v]['type'], j['accessors'][v].get('normalized'), j['accessors'][v]['count']) for k,v in p['attributes'].items()}, 'mat', p.get('material'))
for a in j.get('animations',[]):
    interp = {}
    for s in a['samplers']: interp[s.get('interpolation','LINEAR')] = interp.get(s.get('interpolation','LINEAR'),0)+1
    ins = set(s['input'] for s in a['samplers'])
    print('anim', a.get('name'), 'channels', len(a['channels']), 'samplers', len(a['samplers']), interp, 'inputs', [(i, j['accessors'][i]['count'], j['accessors'][i].get('min'), j['accessors'][i].get('max')) for i in sorted(ins)][:4])
    tg = {}
    for c in a['channels']:
        t=c['target']; tg.setdefault(t['path'],[]).append(t.get('node'))
    print('   targets', tg)
