	.syntax new
	.init_vf_all
	.init_vi_all
	.name vsmForwardExit

	--enter
	--endenter

	iaddiu vi01, vi00, 0
	iaddiu vi02, vi00, 4
	iaddiu vi03, vi00, 32
	iaddiu vi07, vi00, 0x20
	iaddiu vi08, vi00, 0xff

forward_lid:
	ilw.w vi05, 0(vi01)
	iand vi06, vi05, vi07
	ibeq vi06, vi07, forward_done_lid
	iand vi05, vi05, vi08
	isw.w vi05, 0(vi03)
	iaddiu vi01, vi01, 1
	iaddiu vi03, vi03, 1
	ibne vi01, vi02, forward_lid

forward_done_lid:
	isw.w vi01, 80(vi00)

	iaddiu vi09, vi00, 48
	iaddiu vi10, vi00, 51
	move.xyzw vf03, vf00

pipe_lid: --LoopCS 1,1
	lq.xyzw vf01, 0(vi09)
	div q, vf00[w], vf01[w]
	mulq.xyz vf02, vf03, q
	add.xyz vf10, vf00, vf00
	add.xyz vf11, vf00, vf00
	add.xyz vf12, vf00, vf00
	add.xyz vf13, vf00, vf00
	add.xyz vf14, vf00, vf00
	add.xyz vf15, vf00, vf00
	iaddiu vi09, vi09, 1
	ibne vi09, vi10, pipe_lid

	--exit
	--endexit
