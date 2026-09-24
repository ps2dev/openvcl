.syntax new
.init_vf_all
.init_vi_all
.name vsmMemoryBaseUpdateCoalesce

--enter
--endenter

lq vf01, 16(vi00)
lq vf02, 17(vi00)
iaddiu vi03, vi00, 32
add.xyz vf04, vf00, vf00
iaddiu vi03, vi03, 3
sq vf01, 0(vi03)
sq vf02, 1(vi03)
iaddiu vi03, vi03, 3
mfir.w vf03, vi03
sq.w vf03, 40(vi00)

--exit
--endexit
