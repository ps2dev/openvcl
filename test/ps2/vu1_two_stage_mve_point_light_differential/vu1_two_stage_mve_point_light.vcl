	.init_vf_all
	.init_vi_all
	.name vsmTwoStageMvePointLight

	--enter
	--endenter

	iaddiu vi03, vi00, 160
	iaddiu vi13, vi00, 32
	ilw.x vi04, 8(vi00)
	lq vf06, 16(vi00)
	lq vf08, 17(vi00)
	lq vf09, 18(vi00)
	lq vf10, 19(vi00)
	lq vf11, 20(vi00)
	lq vf12, 21(vi00)
	lq vf13, 22(vi00)

pt_light_vert_loop_lid:
	--LoopCS 1,3
	--LoopExtra 5
	lq.xyz vf14, 1(vi13)
	lq.xyz vf15, 0(vi13)
	sub.xyz vf16, vf13, vf15
	mul.xyz vf18, vf16, vf16
	adday.z acc, vf18, vf18y
	maddx.z vf18, vf09, vf18x
	sqrt q, vf18z
	addw.x vf18, vf00, vf00w
	addq.y vf18, vf00, q
	div q, vf00w, vf18y
	mulq.xyz vf17, vf16, q
	mul.xyz vf19, vf18, vf12
	mulax.w acc, vf00, vf19x
	madday.w acc, vf00, vf19y
	maddz.w vf18, vf00, vf19z
	mul.xyz vf20, vf17, vf14
	mulax.w acc, vf00, vf20x
	madday.w acc, vf00, vf20y
	maddz.w vf21, vf00, vf20z
	maxx.w vf22, vf21, vf00x
	mulw.xyz vf23, vf11, vf22w
	mula.xyz acc, vf23, vf08
	madd.xyz vf24, vf10, vf06
	div q, vf00w, vf18w
	mulq.xyz vf25, vf24, q
	lq.xyz vf26, 1(vi03)
	add.xyz vf27, vf26, vf25
	sq.xyz vf27, 1(vi03)
	iaddiu vi13, vi13, 3
	iaddiu vi03, vi03, 3
	ibne vi13, vi04, pt_light_vert_loop_lid
	nop

	isw.x vi13, 300(vi00)
	isw.y vi03, 300(vi00)
	--exit
	--endexit
