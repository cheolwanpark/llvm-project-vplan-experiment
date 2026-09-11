	.attribute	4, 16
	.attribute	5, "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_b1p0_v1p0_zicbom1p0_zicboz1p0_zicsr2p0_zifencei2p0_zmmul1p0_zaamo1p0_zalrsc1p0_zca1p0_zcd1p0_zba1p0_zbb1p0_zbc1p0_zbs1p0_zve32f1p0_zve32x1p0_zve64d1p0_zve64f1p0_zve64x1p0_zvl128b1p0_zvl32b1p0_zvl64b1p0"
	.file	"tsvc_kernels.c"
	.section	.text.builder_main,"ax",@progbits
	.globl	builder_main                    # -- Begin function builder_main
	.p2align	1
	.type	builder_main,@function
builder_main:                           # @builder_main
# %bb.0:                                # %entry
	addi	sp, sp, -32
	sd	ra, 24(sp)                      # 8-byte Folded Spill
	sd	s0, 16(sp)                      # 8-byte Folded Spill
	sd	s1, 8(sp)                       # 8-byte Folded Spill
	fsd	fs0, 0(sp)                      # 8-byte Folded Spill
	call	initialise_arrays
	#APP
	fence	rw, rw
	#NO_APP
	call	builder_platform_cycle
	mv	s0, a0
	#APP
	fence	rw, rw
	#NO_APP
	fmv.w.x	fs0, zero
	li	s1, 32
.LBB0_1:                                # %for.body
                                        # =>This Inner Loop Header: Depth=1
	call	selected_kernel
	addiw	s1, s1, -1
	fadd.s	fs0, fa0, fs0
	bnez	s1, .LBB0_1
# %bb.2:                                # %for.cond.cleanup
	#APP
	fence	rw, rw
	#NO_APP
	call	builder_platform_cycle
	#APP
	fence	rw, rw
	#NO_APP
.Lpcrel_hi0:
	auipc	s1, %pcrel_hi(result_sink)
	fsw	fs0, %pcrel_lo(.Lpcrel_hi0)(s1)
	sub	a1, a0, s0
.Lpcrel_hi1:
	auipc	a0, %pcrel_hi(.L.str)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi1)
	call	printf
	flw	fa5, %pcrel_lo(.Lpcrel_hi0)(s1)
	lui	a0, 784384
	fmv.w.x	fa4, a0
	feq.s	a0, fa5, fa4
	ld	ra, 24(sp)                      # 8-byte Folded Reload
	ld	s0, 16(sp)                      # 8-byte Folded Reload
	ld	s1, 8(sp)                       # 8-byte Folded Reload
	fld	fs0, 0(sp)                      # 8-byte Folded Reload
	addi	sp, sp, 32
	ret
.Lfunc_end0:
	.size	builder_main, .Lfunc_end0-builder_main
                                        # -- End function
	.section	.text.initialise_arrays,"ax",@progbits
	.p2align	1                               # -- Begin function initialise_arrays
	.type	initialise_arrays,@function
initialise_arrays:                      # @initialise_arrays
# %bb.0:                                # %entry
	addi	sp, sp, -32
	sw	zero, 28(sp)
	j	.LBB1_1
.LBB1_1:                                # %for.cond
                                        # =>This Inner Loop Header: Depth=1
	lw	a0, 28(sp)
	lui	a1, 2
	blt	a0, a1, .LBB1_3
	j	.LBB1_2
.LBB1_2:                                # %for.cond.cleanup
	j	.LBB1_5
.LBB1_3:                                # %for.body
                                        #   in Loop: Header=BB1_1 Depth=1
	lw	a0, 28(sp)
	andi	a1, a0, 7
	fcvt.s.wu	fa5, a1
	lui	a1, 260096
	fmv.w.x	fa4, a1
	lui	a1, 246333
	addi	a1, a1, 1802
	fmv.w.x	fa3, a1
	fmadd.s	fa5, fa5, fa3, fa4
.Lpcrel_hi2:
	auipc	a1, %pcrel_hi(a)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi2)
	sh2add	a0, a0, a1
	fsw	fa5, 0(a0)
	j	.LBB1_4
.LBB1_4:                                # %for.inc
                                        #   in Loop: Header=BB1_1 Depth=1
	lw	a0, 28(sp)
	addiw	a0, a0, 1
	sw	a0, 28(sp)
	j	.LBB1_1
.LBB1_5:                                # %for.end
	sw	zero, 24(sp)
	j	.LBB1_6
.LBB1_6:                                # %for.cond2
                                        # =>This Inner Loop Header: Depth=1
	lw	a0, 24(sp)
	lui	a1, 1
	blt	a0, a1, .LBB1_8
	j	.LBB1_7
.LBB1_7:                                # %for.cond.cleanup5
	j	.LBB1_10
.LBB1_8:                                # %for.body6
                                        #   in Loop: Header=BB1_6 Depth=1
	lw	a0, 24(sp)
	andi	a1, a0, 3
	fcvt.s.wu	fa5, a1
	lui	a1, 262144
	fmv.w.x	fa4, a1
	lui	a1, 248381
	addi	a1, a1, 1802
	fmv.w.x	fa3, a1
	fmadd.s	fa5, fa5, fa3, fa4
.Lpcrel_hi3:
	auipc	a1, %pcrel_hi(b)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi3)
	sh2add	a0, a0, a1
	fsw	fa5, 0(a0)
	lw	a0, 24(sp)
	andi	a1, a0, 15
	fcvt.s.wu	fa5, a1
	lui	a1, 263168
	fmv.w.x	fa4, a1
	lui	a1, 246333
	addi	a1, a1, 1802
	fmv.w.x	fa3, a1
	fmadd.s	fa5, fa5, fa3, fa4
.Lpcrel_hi4:
	auipc	a1, %pcrel_hi(c)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi4)
	sh2add	a0, a0, a1
	fsw	fa5, 0(a0)
	lw	a0, 24(sp)
	andi	a1, a0, 7
	fcvt.s.wu	fa5, a1
	lui	a1, 264192
	fmv.w.x	fa4, a1
	lui	a1, 249692
	addi	a1, a1, 655
	fmv.w.x	fa3, a1
	fmadd.s	fa5, fa5, fa3, fa4
.Lpcrel_hi5:
	auipc	a1, %pcrel_hi(d)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi5)
	sh2add	a0, a0, a1
	fsw	fa5, 0(a0)
	j	.LBB1_9
.LBB1_9:                                # %for.inc22
                                        #   in Loop: Header=BB1_6 Depth=1
	lw	a0, 24(sp)
	addiw	a0, a0, 1
	sw	a0, 24(sp)
	j	.LBB1_6
.LBB1_10:                               # %for.end24
	sw	zero, 20(sp)
	j	.LBB1_11
.LBB1_11:                               # %for.cond26
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB1_14 Depth 2
	lw	a0, 20(sp)
	li	a1, 64
	blt	a0, a1, .LBB1_13
	j	.LBB1_12
.LBB1_12:                               # %for.cond.cleanup29
	li	a0, 8
	sw	a0, 16(sp)
	j	.LBB1_20
.LBB1_13:                               # %for.body30
                                        #   in Loop: Header=BB1_11 Depth=1
	sw	zero, 12(sp)
	j	.LBB1_14
.LBB1_14:                               # %for.cond31
                                        #   Parent Loop BB1_11 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	lw	a0, 12(sp)
	li	a1, 64
	blt	a0, a1, .LBB1_16
	j	.LBB1_15
.LBB1_15:                               # %for.cond.cleanup34
                                        #   in Loop: Header=BB1_11 Depth=1
	li	a0, 11
	sw	a0, 16(sp)
	j	.LBB1_18
.LBB1_16:                               # %for.body35
                                        #   in Loop: Header=BB1_14 Depth=2
	j	.LBB1_17
.LBB1_17:                               # %for.inc59
                                        #   in Loop: Header=BB1_14 Depth=2
	lw	a0, 12(sp)
	addiw	a0, a0, 1
	sw	a0, 12(sp)
	j	.LBB1_14
.LBB1_18:                               # %for.end61
                                        #   in Loop: Header=BB1_11 Depth=1
	j	.LBB1_19
.LBB1_19:                               # %for.inc62
                                        #   in Loop: Header=BB1_11 Depth=1
	lw	a0, 20(sp)
	addiw	a0, a0, 1
	sw	a0, 20(sp)
	j	.LBB1_11
.LBB1_20:                               # %for.end64
	addi	sp, sp, 32
	ret
.Lfunc_end1:
	.size	initialise_arrays, .Lfunc_end1-initialise_arrays
                                        # -- End function
	.section	.text.selected_kernel,"ax",@progbits
	.p2align	1                               # -- Begin function selected_kernel
	.type	selected_kernel,@function
selected_kernel:                        # @selected_kernel
# %bb.0:                                # %entry
	#APP
	#NO_APP
	csrr	a0, vlenb
	lui	a1, 1
.Lpcrel_hi6:
	auipc	a2, %pcrel_hi(d)
	srli	a3, a0, 2
	sub	a1, a1, a3
	divu	a7, a1, a3
.Lpcrel_hi7:
	auipc	a3, %pcrel_hi(b)
.Lpcrel_hi8:
	auipc	a4, %pcrel_hi(c)
.Lpcrel_hi9:
	auipc	a5, %pcrel_hi(a)
	srli	a1, a0, 3
	slli	a6, a0, 1
	addi	a2, a2, %pcrel_lo(.Lpcrel_hi6)
	addi	a3, a3, %pcrel_lo(.Lpcrel_hi7)
	addi	a4, a4, %pcrel_lo(.Lpcrel_hi8)
	addi	a5, a5, %pcrel_lo(.Lpcrel_hi9)
	slli	a7, a7, 3
	addi	a7, a7, 8
	mul	a1, a7, a1
	add	t0, a2, a1
	li	a7, 8
	vsetvli	a1, zero, e32, m1, ta, ma
.LBB2_1:                                # %vector.body
                                        # =>This Inner Loop Header: Depth=1
	vl1re32.v	v8, (a4)
	vl1re32.v	v9, (a3)
	vl1re32.v	v10, (a2)
	add	a2, a2, a0
	add	a3, a3, a0
	add	a4, a4, a0
	vfadd.vv	v11, v10, v10
	vfmul.vv	v11, v9, v11
	vfadd.vv	v9, v9, v8
	vfadd.vv	v9, v9, v10
	vfmadd.vv	v9, v8, v11
	vsse32.v	v9, (a5), a7
	add	a5, a5, a6
	bne	a2, t0, .LBB2_1
# %bb.2:                                # %for.cond.cleanup
.Lpcrel_hi10:
	auipc	a0, %pcrel_hi(a+32760)
	flw	fa0, %pcrel_lo(.Lpcrel_hi10)(a0)
	ret
.Lfunc_end2:
	.size	selected_kernel, .Lfunc_end2-selected_kernel
                                        # -- End function
	.type	result_sink,@object             # @result_sink
	.section	.bss.result_sink,"aw",@nobits
	.p2align	2, 0x0
result_sink:
	.word	0x00000000                      # float 0
	.size	result_sink, 4

	.type	.L.str,@object                  # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"MB_ROI=%016llx\n"
	.size	.L.str, 16

	.type	a,@object                       # @a
	.section	.bss.a,"aw",@nobits
	.p2align	6, 0x0
a:
	.zero	32768
	.size	a, 32768

	.type	b,@object                       # @b
	.section	.bss.b,"aw",@nobits
	.p2align	6, 0x0
b:
	.zero	16384
	.size	b, 16384

	.type	c,@object                       # @c
	.section	.bss.c,"aw",@nobits
	.p2align	6, 0x0
c:
	.zero	16384
	.size	c, 16384

	.type	d,@object                       # @d
	.section	.bss.d,"aw",@nobits
	.p2align	6, 0x0
d:
	.zero	16384
	.size	d, 16384

	.ident	"clang version 22.1.8 (https://github.com/llvm/llvm-project.git ca7933e47d3a3451d81e72ac174dcb5aa28b59d1)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym result_sink
