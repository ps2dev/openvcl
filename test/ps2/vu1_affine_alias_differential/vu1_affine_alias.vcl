.syntax new
.init_vf_all
.init_vi_all
.name vsmAffineAlias

--enter
--endenter

iaddiu vi01, vi00, 16
iaddiu vi02, vi01, 4
iaddiu vi03, vi00, 32
lq.w vf01, 64(vi00)
sq.w vf01, 0(vi01)
--barrier
lq.w vf02, 0(vi02)
sq.w vf02, 0(vi03)

--exit
--endexit
