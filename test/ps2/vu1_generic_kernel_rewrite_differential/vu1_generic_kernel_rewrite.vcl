	.syntax new
	.init_vf_all
	.init_vi_all
	.name vsmGenericKernelRewrite

	--enter
	--endenter

	iaddiu next_index, vi00, 16
	ilw.x last_index, 301(vi00)
	iaddiu first_index_mask, vi00, 0xff
	iaddiu input_start, vi00, 64
	iaddiu color_start, vi00, 160
	iaddiu next_output, vi00, 224
	iaddiu do_clipping, vi00, 0
	iaddiu matrix_start, vi00, 304
	loi 3.0
	maxi.z index_constants, vf00, i
	loi 255.0
	maxi.y index_constants, vf00, i
	loi 253.0
	maxi.w index_constants, vf00, i
	loi 2047.5
	addi.xyz gs_offsets, vf00, i
	loi 255.0
	maxi.w max_color_val, vf00, i
	maxi.w vert_color, vf00, i
	loi 1.0
	maxi.xyz clip_scales, vf00, i
	maxi.w clip_scales, vf00, i
	lq vert_xform_0, 0(matrix_start)
	lq vert_xform_1, 1(matrix_start)
	lq vert_xform_2, 2(matrix_start)
	lq vert_xform_3, 3(matrix_start)
xform_loop_lid: --LoopCS 1,3
 ; get first index
 ilw.w first_index, 0(next_index)
 iand first_index, first_index, first_index_mask
 iadd first_offset, first_index, first_index
 iadd first_offset, first_offset, first_index
 ; get second index
 ; the first and second indices are in the form 0x3f80ssff
 lqi.w indices, (next_index++)
 addy.w second_ind, indices, index_constants[y]
 mtir second_index, second_ind[w]
 mulz.w second_off, indices, index_constants[z]
 add.w second_off, second_off, index_constants[w]
 mtir second_offset, second_off[w]
 ; ---------- first vert -----------------
 iadd next_input, first_offset, input_start
 iadd next_color, first_index, color_start
 ; xform/clip vertex
 lq.xyz vert, 0(next_input)
 mulax acc, vert_xform_0, vert
 madday acc, vert_xform_1, vert
 maddaz acc, vert_xform_2, vert
 maddw xformed_vert, vert_xform_3, vf00
 div q, vf00[w], xformed_vert[w]
 mulq.xyz xformed_vert, xformed_vert, q
 ; FIXME: visible vertices are now in range (+-320, +-112, +-2^24-1)
 ; add screen offsets to xformed vertex
 add.xyz gs_vert, xformed_vert, gs_offsets
 ; convert to 4-bit fixed-point
 ftoi4.xyz gs_vert, gs_vert
 ; load_strip_adc strip_adc
 ; bfc_strip_vert xformed_vert, vi00
 mul.xyz clip_vert65, xformed_vert, clip_scales
 clipw.xyz clip_vert65, clip_scales[w]
 fcand vi01, 0x003ffff
 iand vi01, vi01, do_clipping
 ; set_adc_fbs gs_vert, vi00
 ; clip and face culling flags
 ior new_adc66, vi01, vi00
 iaddiu new_adc66, new_adc66, 0x7fff
 mfir.w gs_vert, new_adc66
 sq gs_vert, 2+0(next_output)
 ; color
 lq.xyz vert_color, (next_color)
 miniw.xyz vert_color, vert_color, max_color_val[w]
 ftoi0.xyz vert_color, vert_color
 sq vert_color, 1+0(next_output)
 ; texture coords
 lq.xyz tex_stq, 2+0(next_input)
 ; normalize stq
 mulq.xyz tex_stq, tex_stq, q
 sq.xyz tex_stq, 0+0(next_output)
 ; ---------- second vert -----------------
 iadd next_input, second_offset, input_start
 iadd next_color, second_index, color_start
 iaddiu next_output, next_output, 3
 ; xform/clip vertex
 lq.xyz vert, 0(next_input)
 mulax acc, vert_xform_0, vert
 madday acc, vert_xform_1, vert
 maddaz acc, vert_xform_2, vert
 maddw xformed_vert, vert_xform_3, vf00
 div q, vf00[w], xformed_vert[w]
 mulq.xyz xformed_vert, xformed_vert, q
 ; FIXME: visible vertices are now in range (+-320, +-112, +-2^24-1)
 ; add screen offsets to xformed vertex
 add.xyz gs_vert, xformed_vert, gs_offsets
 ; convert to 4-bit fixed-point
 ftoi4.xyz gs_vert, gs_vert
 ; load_strip_adc strip_adc
 ; bfc_strip_vert xformed_vert, vi00
 mul.xyz clip_vert77, xformed_vert, clip_scales
 clipw.xyz clip_vert77, clip_scales[w]
 fcand vi01, 0x003ffff
 iand vi01, vi01, do_clipping
 ; set_adc_fbs gs_vert, vi00
 ; clip and face culling flags
 ior new_adc78, vi01, vi00
 iaddiu new_adc78, new_adc78, 0x7fff
 mfir.w gs_vert, new_adc78
 sq gs_vert, 2+0(next_output)
 ; color
 lq.xyz vert_color, (next_color)
 miniw.xyz vert_color, vert_color, max_color_val[w]
 ftoi0.xyz vert_color, vert_color
 sq vert_color, 1+0(next_output)
 ; texture coords
 lq.xyz tex_stq, 2+0(next_input)
 ; normalize stq
 mulq.xyz tex_stq, tex_stq, q
 sq.xyz tex_stq, 0+0(next_output)
 ; --------- end loop ---------------
	 iaddiu next_output, next_output, 3
	 ibne next_index, last_index, xform_loop_lid
	 nop
		isw.x next_index, 300(vi00)
	isw.y next_output, 300(vi00)
	--exit
	--endexit
