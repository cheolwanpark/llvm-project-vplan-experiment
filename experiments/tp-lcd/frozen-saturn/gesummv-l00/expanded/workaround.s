	.attribute	4, 16
	.attribute	5, "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_b1p0_v1p0_zicbom1p0_zicboz1p0_zicsr2p0_zifencei2p0_zmmul1p0_zaamo1p0_zalrsc1p0_zca1p0_zcd1p0_zba1p0_zbb1p0_zbc1p0_zbs1p0_zve32f1p0_zve32x1p0_zve64d1p0_zve64f1p0_zve64x1p0_zvl128b1p0_zvl32b1p0_zvl64b1p0"
	.file	"application_kernels.c"
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
	call	initialise_arrays
	#APP
	fence	rw, rw
	#NO_APP
	call	builder_platform_cycle
	mv	s0, a0
	#APP
	fence	rw, rw
	#NO_APP
	li	s1, 32
.LBB0_1:                                # %for.body
                                        # =>This Inner Loop Header: Depth=1
	call	selected_kernel
	addiw	s1, s1, -1
	bnez	s1, .LBB0_1
# %bb.2:                                # %for.cond.cleanup
	#APP
	fence	rw, rw
	#NO_APP
	call	builder_platform_cycle
	#APP
	fence	rw, rw
	#NO_APP
	mv	s1, a0
	call	consume_outputs
	sub	a1, s1, s0
.Lpcrel_hi0:
	auipc	a0, %pcrel_hi(.L.str)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi0)
	call	printf
.Lpcrel_hi1:
	auipc	a0, %pcrel_hi(.L_MergedGlobals)
	flw	fa5, %pcrel_lo(.Lpcrel_hi1)(a0)
	lui	a0, 784384
	fmv.w.x	fa4, a0
	feq.s	a0, fa5, fa4
	ld	ra, 24(sp)                      # 8-byte Folded Reload
	ld	s0, 16(sp)                      # 8-byte Folded Reload
	ld	s1, 8(sp)                       # 8-byte Folded Reload
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
	addi	sp, sp, -16
	sw	zero, 12(sp)
	j	.LBB1_1
.LBB1_1:                                # %for.cond
                                        # =>This Inner Loop Header: Depth=1
	lw	a0, 12(sp)
	lui	a1, 1
	addi	a1, a1, 2
	blt	a0, a1, .LBB1_3
	j	.LBB1_2
.LBB1_2:                                # %for.cond.cleanup
	j	.LBB1_5
.LBB1_3:                                # %for.body
                                        #   in Loop: Header=BB1_1 Depth=1
	lw	a0, 12(sp)
	lui	a1, 493448
	addi	a1, a1, -1927
	mul	a1, a0, a1
	srli	a2, a1, 63
	srai	a1, a1, 35
	addw	a1, a1, a2
	slliw	a2, a1, 4
	subw	a1, a0, a1
	subw	a1, a1, a2
	fcvt.s.w	fa5, a1
	lui	a1, 256000
	fmv.w.x	fa4, a1
	lui	a1, 249856
	fmv.w.x	fa3, a1
	fmadd.s	fa5, fa5, fa3, fa4
.Lpcrel_hi2:
	auipc	a1, %pcrel_hi(a)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi2)
	sh2add	a0, a0, a1
	fsw	fa5, 0(a0)
	lw	a0, 12(sp)
	lui	a1, 322639
	addi	a1, a1, -945
	mul	a1, a0, a1
	srli	a2, a1, 63
	srai	a1, a1, 34
	addw	a1, a1, a2
	sh1add	a2, a1, a1
	sh2add	a1, a2, a1
	subw	a1, a0, a1
	fcvt.s.w	fa5, a1
	lui	a1, 782336
	fmv.w.x	fa4, a1
	lui	a1, 251904
	fmv.w.x	fa3, a1
	fmadd.s	fa5, fa5, fa3, fa4
.Lpcrel_hi3:
	auipc	a1, %pcrel_hi(b)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi3)
	sh2add	a0, a0, a1
	fsw	fa5, 0(a0)
	j	.LBB1_4
.LBB1_4:                                # %for.inc
                                        #   in Loop: Header=BB1_1 Depth=1
	lw	a0, 12(sp)
	addiw	a0, a0, 1
	sw	a0, 12(sp)
	j	.LBB1_1
.LBB1_5:                                # %for.end
	sw	zero, 8(sp)
	j	.LBB1_6
.LBB1_6:                                # %for.cond10
                                        # =>This Inner Loop Header: Depth=1
	lw	a0, 8(sp)
	lui	a1, 1
	blt	a0, a1, .LBB1_8
	j	.LBB1_7
.LBB1_7:                                # %for.cond.cleanup13
	j	.LBB1_10
.LBB1_8:                                # %for.body14
                                        #   in Loop: Header=BB1_6 Depth=1
	lw	a0, 8(sp)
	lui	a1, 190650
	addi	a1, a1, 745
	mul	a1, a0, a1
	srli	a2, a1, 63
	srai	a1, a1, 32
	srli	a1, a1, 1
	addw	a1, a1, a2
	sh2add	a2, a1, a1
	sh1add	a1, a2, a1
	subw	a1, a0, a1
	fcvt.s.w	fa5, a1
	lui	a1, 780288
	fmv.w.x	fa4, a1
	lui	a1, 251904
	fmv.w.x	fa3, a1
	fmadd.s	fa5, fa5, fa3, fa4
.Lpcrel_hi4:
	auipc	a1, %pcrel_hi(x)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi4)
	sh2add	a0, a0, a1
	fsw	fa5, 0(a0)
	lw	a0, 8(sp)
	lui	a1, 599186
	addi	a1, a1, 1171
	mul	a1, a0, a1
	srli	a1, a1, 32
	addw	a1, a1, a0
	srliw	a2, a1, 31
	sraiw	a1, a1, 2
	addw	a1, a1, a2
	slliw	a2, a1, 3
	addw	a1, a1, a0
	subw	a1, a1, a2
	fcvt.s.w	fa5, a1
	lui	a1, 253952
	fmv.w.x	fa4, a1
	lui	a1, 249856
	fmv.w.x	fa3, a1
	fmadd.s	fa5, fa5, fa3, fa4
.Lpcrel_hi5:
	auipc	a1, %pcrel_hi(output)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi5)
	sh2add	a0, a0, a1
	fsw	fa5, 0(a0)
	j	.LBB1_9
.LBB1_9:                                # %for.inc23
                                        #   in Loop: Header=BB1_6 Depth=1
	lw	a0, 8(sp)
	addiw	a0, a0, 1
	sw	a0, 8(sp)
	j	.LBB1_6
.LBB1_10:                               # %for.end25
.Lpcrel_hi6:
	auipc	a0, %pcrel_hi(.L_MergedGlobals)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi6)
	sw	zero, 68(a0)
	sw	zero, 64(a0)
	addi	sp, sp, 16
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
	csrr	a2, vlenb
	lui	a1, 1
	vsetvli	a0, zero, e32, m2, ta, ma
	vmv.v.i	v8, 0
.Lpcrel_hi7:
	auipc	a3, %pcrel_hi(b)
.Lpcrel_hi8:
	auipc	a4, %pcrel_hi(x)
	srli	a0, a2, 1
	addi	a1, a1, -1
	add	a1, a1, a0
	neg	a5, a0
	and	a1, a1, a5
.Lpcrel_hi9:
	auipc	a5, %pcrel_hi(a)
	slli	a2, a2, 1
	addi	a3, a3, %pcrel_lo(.Lpcrel_hi7)
	addi	a4, a4, %pcrel_lo(.Lpcrel_hi8)
	addi	a5, a5, %pcrel_lo(.Lpcrel_hi9)
	vmv.v.i	v10, 0
.LBB2_1:                                # %vector.body
                                        # =>This Inner Loop Header: Depth=1
	vl2re32.v	v12, (a5)
	vl2re32.v	v14, (a4)
	vfmacc.vv	v10, v14, v12
	vl2re32.v	v12, (a3)
	sub	a1, a1, a0
	add	a3, a3, a2
	add	a4, a4, a2
	vfmacc.vv	v8, v12, v14
	add	a5, a5, a2
	bnez	a1, .LBB2_1
# %bb.2:                                # %for.cond.cleanup
	vmv.s.x	v12, zero
	lui	a0, 261120
	lui	a1, 260506
	# xiangshan-f32-reduction-extract-via-gpr-e64-v1
	addi	sp, sp, -16
	sd	t0, 0(sp)
	sd	t1, 8(sp)
	csrr	t1, vl
	vsetvli	zero, zero, e32, m2, tu, ma
	vfredusum.vs	v10, v10, v12
	vsetivli	zero, 1, e64, m1, ta, ma
	vmv.x.s	t0, v10
	fmv.w.x	fa4, t0
	vsetvli	zero, t1, e32, m2, ta, ma
	ld	t0, 0(sp)
	ld	t1, 8(sp)
	addi	sp, sp, 16
	# xiangshan-f32-reduction-extract-via-gpr-e64-v1
	addi	sp, sp, -16
	sd	t0, 0(sp)
	sd	t1, 8(sp)
	csrr	t1, vl
	vsetvli	zero, zero, e32, m2, tu, ma
	vfredusum.vs	v8, v8, v12
	vsetivli	zero, 1, e64, m1, ta, ma
	vmv.x.s	t0, v8
	fmv.w.x	fa3, t0
	vsetvli	zero, t1, e32, m2, ta, ma
	ld	t0, 0(sp)
	ld	t1, 8(sp)
	addi	sp, sp, 16
	fmv.w.x	fa5, a0
	addi	a0, a1, -1638
	fmv.w.x	fa2, a0
	fmul.s	fa5, fa4, fa5
	fmadd.s	fa5, fa3, fa2, fa5
.Lpcrel_hi10:
	auipc	a0, %pcrel_hi(.L_MergedGlobals)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi10)
	fsw	fa4, 64(a0)
	fsw	fa5, 68(a0)
	ret
.Lfunc_end2:
	.size	selected_kernel, .Lfunc_end2-selected_kernel
                                        # -- End function
	.section	.text.consume_outputs,"ax",@progbits
	.p2align	1                               # -- Begin function consume_outputs
	.type	consume_outputs,@function
consume_outputs:                        # @consume_outputs
# %bb.0:                                # %entry
	addi	sp, sp, -16
.Lpcrel_hi11:
	auipc	a0, %pcrel_hi(.L_MergedGlobals)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi11)
	flw	fa5, 64(a0)
	flw	fa4, 68(a0)
	fadd.s	fa5, fa5, fa4
	fsw	fa5, 12(sp)
	sw	zero, 8(sp)
	j	.LBB3_1
.LBB3_1:                                # %for.cond
                                        # =>This Inner Loop Header: Depth=1
	lw	a0, 8(sp)
	lui	a1, 1
	blt	a0, a1, .LBB3_3
	j	.LBB3_2
.LBB3_2:                                # %for.cond.cleanup
	j	.LBB3_5
.LBB3_3:                                # %for.body
                                        #   in Loop: Header=BB3_1 Depth=1
	lw	a0, 8(sp)
.Lpcrel_hi12:
	auipc	a1, %pcrel_hi(output)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi12)
	sh2add	a0, a0, a1
	flw	fa5, 0(a0)
	flw	fa4, 12(sp)
	fadd.s	fa5, fa4, fa5
	fsw	fa5, 12(sp)
	j	.LBB3_4
.LBB3_4:                                # %for.inc
                                        #   in Loop: Header=BB3_1 Depth=1
	lw	a0, 8(sp)
	addiw	a0, a0, 1
	sw	a0, 8(sp)
	j	.LBB3_1
.LBB3_5:                                # %for.end
	flw	fa5, 12(sp)
.Lpcrel_hi13:
	auipc	a0, %pcrel_hi(.L_MergedGlobals)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi13)
	fsw	fa5, 0(a0)
	addi	sp, sp, 16
	ret
.Lfunc_end3:
	.size	consume_outputs, .Lfunc_end3-consume_outputs
                                        # -- End function
	.type	.L.str,@object                  # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"MB_ROI=%016llx\n"
	.size	.L.str, 16

	.type	a,@object                       # @a
	.section	.bss.a,"aw",@nobits
	.p2align	6, 0x0
a:
	.zero	16392
	.size	a, 16392

	.type	b,@object                       # @b
	.section	.bss.b,"aw",@nobits
	.p2align	6, 0x0
b:
	.zero	16392
	.size	b, 16392

	.type	x,@object                       # @x
	.section	.bss.x,"aw",@nobits
	.p2align	6, 0x0
x:
	.zero	16384
	.size	x, 16384

	.type	output,@object                  # @output
	.section	.bss.output,"aw",@nobits
	.p2align	6, 0x0
output:
	.zero	16384
	.size	output, 16384

	.type	.L_MergedGlobals,@object        # @_MergedGlobals
	.section	.bss..L_MergedGlobals,"aw",@nobits
	.p2align	6, 0x0
.L_MergedGlobals:
	.zero	72
	.size	.L_MergedGlobals, 72

result_sink = .L_MergedGlobals
	.size	result_sink, 4
scalars.0 = .L_MergedGlobals+64
	.size	scalars.0, 4
scalars.1 = .L_MergedGlobals+68
	.size	scalars.1, 4
	.ident	"clang version 22.1.8 (https://github.com/llvm/llvm-project.git ca7933e47d3a3451d81e72ac174dcb5aa28b59d1)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym .L_MergedGlobals
