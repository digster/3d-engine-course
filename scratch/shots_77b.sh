#!/bin/sh
# scratch/shots_77b.sh — the renders Lesson 7.7b's figures are drawn from.
#
# Run from the repository root after `cmake --build build --target mannequin`.
# Each shot is a headless `mannequin --shot`; each is then cropped to the region
# its figure uses and box-averaged to half resolution by scratch/crop_ppm.py, and
# only the crop is kept (scratch/l77b_render_*.ppm, tracked) — the full frames are
# 1.5 MB each and are output, not source.
set -e
cd "$(dirname "$0")/.."
cmake -E copy_directory assets build/demos/assets   # POST_BUILD copies only on relink
d=build/demos
( cd $d && ./mannequin --time 0.25 --shot ../../scratch/_shot_walk.ppm )
( cd $d && ./mannequin --time 0.25 --order-file --shot ../../scratch/_shot_order.ppm )
( cd $d && ./mannequin --time 0.25 --xyzw --wide --shot ../../scratch/_shot_xyzw.ppm )
( cd $d && ./mannequin --clip wave --time 1.0 --shot ../../scratch/_shot_wave.ppm )
python3 scratch/crop_ppm.py scratch/_shot_walk.ppm  scratch/l77b_render_walk.ppm  280 0 680 540
python3 scratch/crop_ppm.py scratch/_shot_order.ppm scratch/l77b_render_order.ppm 280 0 680 540
python3 scratch/crop_ppm.py scratch/_shot_xyzw.ppm  scratch/l77b_render_xyzw.ppm  300 60 660 450
python3 scratch/crop_ppm.py scratch/_shot_wave.ppm  scratch/l77b_render_wave.ppm  280 0 680 540
rm -f scratch/_shot_*.ppm
