.syntax new
.init_vf_all
.init_vi_all
.name vsmNestedLoop

--enter
--endenter

iaddiu vi01, vi00, 2
iaddiu vi02, vi00, 1
iaddiu vi03, vi00, 32
ilw.x vi04, 0(vi02)

outer_lid:
isw.w vi04, 0(vi03)
iaddiu vi03, vi03, 1
iaddiu vi05, vi00, 0
iaddiu vi06, vi00, 2

inner_lid: --LoopCS 1,1
iaddiu vi05, vi05, 1
ibne vi05, vi06, inner_lid

iaddiu vi02, vi02, 1
ilw.x vi04, 0(vi02)
isubiu vi01, vi01, 1
ibne vi01, vi00, outer_lid

isw.w vi02, 80(vi00)
isw.w vi04, 81(vi00)

--exit
--endexit
