.syntax new
.init_vf_all
.init_vi_all
.name vsmAliasInduction

--enter
--endenter

iaddiu vi01, vi00, 0
iaddiu vi02, vi00, 4
iaddiu vi03, vi00, 32
iaddiu vi04, vi00, 0
lq.xyz vf21, 64(vi00)

loop_lid: --LoopCS 1,1
lq.xyz vf20, 0(vi04)
div q, vf20[x], vf21[x]
mulq.xyz vf02, vf20, q
add.xyz vf10, vf20, vf00
add.xyz vf11, vf20, vf00
div q, vf20[y], vf21[y]
add.xyz vf12, vf20, vf00
add.xyz vf13, vf20, vf00
mulq.xyz vf05, vf20, q
sq.xyz vf05, 0(vi03)
iaddiu vi03, vi03, 1
sq.xyz vf02, 0(vi03)
lq.xyz vf24, 8(vi04)
add.xyz vf05, vf24, vf00
iaddiu vi04, vi04, 1
iaddiu vi03, vi03, 1
iaddiu vi01, vi01, 1
ibne vi01, vi02, loop_lid
nop

isw.x vi03, 80(vi00)
--exit
--endexit
