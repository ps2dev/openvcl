.syntax new
.init_vf_all
.init_vi_all
.name vsmPostIncrementPrefix

--enter
--endenter

iaddiu vi03, vi00, 16
ilw.x vi04, 66(vi00)
iaddiu vi05, vi00, 32
lq vf20, 64(vi00)
lq vf21, 65(vi00)

loop_lid: --LoopCS 1,1
lqi.w vf10, (vi03++)
div q, vf10[w], vf20[w]
mulq.w vf11, vf10, q
add.xyz vf12, vf00, vf00
add.xyz vf13, vf00, vf00
add.xyz vf14, vf00, vf00
add.xyz vf15, vf00, vf00
div q, vf10[w], vf21[w]
mulq.w vf12, vf10, q
add.xyz vf16, vf00, vf00
add.xyz vf17, vf00, vf00
add.xyz vf18, vf00, vf00
add.xyz vf19, vf00, vf00
sq.w vf11, 0(vi05)
sq.w vf12, 1(vi05)
iaddiu vi05, vi05, 2
ibne vi03, vi04, loop_lid
nop

isw.x vi03, 80(vi00)
--exit
--endexit
