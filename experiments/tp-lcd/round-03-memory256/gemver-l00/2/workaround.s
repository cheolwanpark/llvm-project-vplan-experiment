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
	auipc	a0, %pcrel_hi(result_sink)
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
.Lpcrel_hi2:
	auipc	a1, %pcrel_hi(b)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi2)
	sh2add	a0, a0, a1
	fsw	fa5, 0(a0)
	lw	a0, 12(sp)
	lui	a1, 441506
	addi	a1, a1, -1293
	mul	a1, a0, a1
	srli	a2, a1, 63
	srai	a1, a1, 35
	addw	a1, a1, a2
	sh3add	a2, a1, a1
	sh1add	a1, a2, a1
	subw	a1, a0, a1
	fcvt.s.w	fa5, a1
	lui	a1, 253952
	fmv.w.x	fa4, a1
	lui	a1, 247808
	fmv.w.x	fa3, a1
	fmadd.s	fa5, fa5, fa3, fa4
.Lpcrel_hi3:
	auipc	a1, %pcrel_hi(c)
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
.Lpcrel_hi4:
	auipc	a1, %pcrel_hi(output)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi4)
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
	csrr	a0, vlenb
	lui	a1, 1
	srli	a2, a0, 2
	sub	a1, a1, a2
	divu	a1, a1, a2
	#APP
	#NO_APP
.Lpcrel_hi5:
	auipc	a2, %pcrel_hi(c)
.Lpcrel_hi6:
	auipc	a3, %pcrel_hi(b)
.Lpcrel_hi7:
	auipc	a4, %pcrel_hi(output)
	lui	a5, 258048
	fmv.w.x	fa5, a5
	srli	a5, a0, 3
	slli	a1, a1, 3
	addi	a1, a1, 8
	mul	a5, a1, a5
	lui	a6, 259072
	addi	a1, a2, %pcrel_lo(.Lpcrel_hi5)
	addi	a2, a3, %pcrel_lo(.Lpcrel_hi6)
	addi	a3, a4, %pcrel_lo(.Lpcrel_hi7)
	add	a4, a1, a5
	fmv.w.x	fa4, a6
	vsetvli	a5, zero, e32, m1, ta, ma
.LBB2_1:                                # %vector.body
                                        # =>This Inner Loop Header: Depth=1
	vl1re32.v	v8, (a3)
	vl1re32.v	v9, (a2)
	vl1re32.v	v10, (a1)
	vfmadd.vf	v9, fa5, v8
	add	a1, a1, a0
	add	a2, a2, a0
	vfmadd.vf	v10, fa4, v9
	vs1r.v	v10, (a3)
	add	a3, a3, a0
	bne	a1, a4, .LBB2_1
# %bb.2:                                # %for.cond.cleanup
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
	sw	zero, 12(sp)
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
.Lpcrel_hi8:
	auipc	a1, %pcrel_hi(output)
	addi	a1, a1, %pcrel_lo(.Lpcrel_hi8)
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
.Lpcrel_hi9:
	auipc	a0, %pcrel_hi(result_sink)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi9)
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

	.type	result_sink,@object             # @result_sink
	.section	.bss.result_sink,"aw",@nobits
	.p2align	2, 0x0
result_sink:
	.word	0x00000000                      # float 0
	.size	result_sink, 4

	.type	b,@object                       # @b
	.section	.bss.b,"aw",@nobits
	.p2align	6, 0x0
b:
	.zero	16392
	.size	b, 16392

	.type	c,@object                       # @c
	.section	.bss.c,"aw",@nobits
	.p2align	6, 0x0
c:
	.zero	16392
	.size	c, 16392

	.type	output,@object                  # @output
	.section	.bss.output,"aw",@nobits
	.p2align	6, 0x0
output:
	.zero	16384
	.size	output, 16384

	.ident	"clang version 22.1.8 (https://github.com/llvm/llvm-project.git ca7933e47d3a3451d81e72ac174dcb5aa28b59d1)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym result_sink
