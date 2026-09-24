.syntax new
.init_vf_all
.init_vi_all
.name vsmDeadIntegerSourceReuse

--enter
--endenter

iaddiu vi01, vi00, 16
ilw.w vi01, 0(vi01)
iaddiu adjusted, vi01, 7
mfir.w vf01, adjusted
fcand vi01, 0x3ffff
iaddiu vi02, vi00, 32
sq.w vf01, 0(vi02)

--exit
--endexit
