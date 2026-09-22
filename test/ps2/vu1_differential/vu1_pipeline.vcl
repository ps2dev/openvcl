	.syntax new
	.init_vf_all
	.init_vi_all
	.name vsmDifferential

	--enter
	--endenter

	iaddiu vi01, vi00, 0
	iaddiu vi02, vi00, 32
	iaddiu vi03, vi00, 4
	move.xyz vf10, vf00
	move.xyz vf11, vf00
	move.xyz vf12, vf00
	move.xyz vf13, vf00
	move.xyz vf14, vf00
	move.xyz vf15, vf00

loop_lid:	--LoopCS 1, 1
	lq vf01, 0(vi01)
	div q, vf00[w], vf01[w]
	mulq.xyz vf02, vf01, q
	add.xyz vf10, vf10, vf00
	add.xyz vf11, vf11, vf00
	add.xyz vf12, vf12, vf00
	add.xyz vf13, vf13, vf00
	add.xyz vf14, vf14, vf00
	add.xyz vf15, vf15, vf00
	sq.xyz vf02, 0(vi02)
	iaddiu vi01, vi01, 1
	iaddiu vi02, vi02, 1
	isubiu vi03, vi03, 1
	ibne vi03, vi00, loop_lid

	--exit
	--endexit
