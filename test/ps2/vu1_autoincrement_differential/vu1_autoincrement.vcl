	.syntax new
	.init_vf_all
	.init_vi_all
	.name vsmAutoIncrement

	--enter
	--endenter

	iaddiu vi01, vi00, 0
	iaddiu vi02, vi00, 32
	iaddiu vi03, vi00, 12
	lq vf19, 64(vi00)
	lq vf12, 65(vi00)
	lq vf14, 66(vi00)
	lq vf08, 67(vi00)
	lq vf23, 68(vi00)
	lq vf17, 69(vi00)
	lq vf13, 70(vi00)
	lq vf06, 71(vi00)

loop_lid: --LoopCS 1,3
	lq.xyz vf31, 1(vi01)
	mul.xyz vf30, vf19, vf31
	adday.z acc, vf30, vf30
	maddx.z vf30, vf12, vf30
	maxx.z vf30, vf30, vf00
	mulz.xyz vf29, vf14, vf30
	mula.xyz acc, vf29, vf08
	mul.xyz vf28, vf23, vf31
	mr32.xyw vf28, vf28
	addax.w acc, vf28, vf28
	maddy.w vf31, vf00, vf28
	maxx.w vf31, vf31, vf00
	mul.w vf31, vf31, vf31
	mul.w vf31, vf31, vf31
	mul.w vf31, vf31, vf31
	mul.w vf31, vf31, vf31
	mul.w vf31, vf31, vf31
	maddaw.xyz acc, vf17, vf31
	madd.xyz vf16, vf13, vf06
	lq.xyz vf21, 0(vi02)
	add.xyz vf16, vf21, vf16
	sqi.xyz vf16, (vi02++)
	iaddiu vi01, vi01, 3
	ibne vi01, vi03, loop_lid
	nop

	isw.x vi02, 80(vi00)

	--exit
	--endexit
