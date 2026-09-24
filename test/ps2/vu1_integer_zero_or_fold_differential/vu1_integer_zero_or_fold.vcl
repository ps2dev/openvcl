.syntax new
.init_vf_all
.init_vi_all
.name vsmIntegerZeroOrFold

--enter
--endenter

iaddiu vi01, vi00, 16
ilw.w vi02, 0(vi01)
ior vi03, vi02, vi00
iaddiu vi03, vi03, 7
mfir.w vf01, vi03
iaddiu vi04, vi00, 32
sq.w vf01, 0(vi04)

--exit
--endexit
