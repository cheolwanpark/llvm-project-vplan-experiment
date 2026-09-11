; ModuleID = '/Users/cheolwanpark/Documents/project/llvm-project/experiments/tp-lcd/inputs/application_kernels.c'
source_filename = "/Users/cheolwanpark/Documents/project/llvm-project/experiments/tp-lcd/inputs/application_kernels.c"
target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128"
target triple = "riscv64-unknown-unknown-elf"

@.str = private unnamed_addr constant [16 x i8] c"MB_ROI=%016llx\0A\00", align 1
@result_sink = internal global float 0.000000e+00, align 4
@a = internal unnamed_addr global [4098 x float] zeroinitializer, align 64
@b = internal unnamed_addr global [4098 x float] zeroinitializer, align 64
@c = internal unnamed_addr global [4098 x float] zeroinitializer, align 64
@output = internal unnamed_addr global [4096 x float] zeroinitializer, align 64

; Function Attrs: nounwind vscale_range(2,1024)
define dso_local signext range(i32 0, 2) i32 @builder_main(i32 noundef signext %argc, ptr noundef readnone captures(none) %argv) local_unnamed_addr #0 !dbg !14 {
entry:
  tail call fastcc void @initialise_arrays() #7, !dbg !18
  tail call void asm sideeffect "fence rw, rw", "~{memory}"() #8, !dbg !19, !srcloc !22
  %call.i = tail call i64 @builder_platform_cycle() #9, !dbg !23
  tail call void asm sideeffect "fence rw, rw", "~{memory}"() #8, !dbg !24, !srcloc !25
  br label %for.body, !dbg !26

for.cond.cleanup:                                 ; preds = %for.body
  tail call void asm sideeffect "fence rw, rw", "~{memory}"() #8, !dbg !27, !srcloc !22
  %call.i5 = tail call i64 @builder_platform_cycle() #9, !dbg !29
  tail call void asm sideeffect "fence rw, rw", "~{memory}"() #8, !dbg !30, !srcloc !25
  tail call fastcc void @consume_outputs() #7, !dbg !31
  %sub = sub i64 %call.i5, %call.i, !dbg !32
  %call2 = tail call signext i32 (ptr, ...) @printf(ptr noundef nonnull @.str, i64 noundef %sub) #9, !dbg !33
  %0 = load volatile float, ptr @result_sink, align 4, !dbg !34, !tbaa !35
  %cmp3 = fcmp fast oeq float %0, -1.000000e+00, !dbg !37
  %conv = zext i1 %cmp3 to i32, !dbg !37
  ret i32 %conv, !dbg !38

for.body:                                         ; preds = %entry, %for.body
  %repeat.06 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  tail call fastcc void @selected_kernel() #7, !dbg !39
  %inc = add nuw nsw i32 %repeat.06, 1, !dbg !40
  %exitcond.not = icmp eq i32 %inc, 32, !dbg !41
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body, !dbg !26, !llvm.loop !42
}

; Function Attrs: noinline nounwind optnone vscale_range(2,1024)
define internal fastcc void @initialise_arrays() unnamed_addr #1 !dbg !47 {
entry:
  %i = alloca i32, align 4
  %i9 = alloca i32, align 4
  call void @llvm.lifetime.start.p0(ptr %i) #8, !dbg !48
  store i32 0, ptr %i, align 4, !dbg !49, !tbaa !10
  br label %for.cond, !dbg !48

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, ptr %i, align 4, !dbg !50, !tbaa !10
  %cmp = icmp slt i32 %0, 4098, !dbg !51
  br i1 %cmp, label %for.body, label %for.cond.cleanup, !dbg !52

for.cond.cleanup:                                 ; preds = %for.cond
  call void @llvm.lifetime.end.p0(ptr %i) #8, !dbg !52
  br label %for.end

for.body:                                         ; preds = %for.cond
  %1 = load i32, ptr %i, align 4, !dbg !53, !tbaa !10
  %rem = srem i32 %1, 17, !dbg !54
  %conv = sitofp i32 %rem to float, !dbg !55
  %2 = call float @llvm.fmuladd.f32(float %conv, float 3.125000e-02, float 2.500000e-01), !dbg !56
  %3 = load i32, ptr %i, align 4, !dbg !57, !tbaa !10
  %idxprom = sext i32 %3 to i64, !dbg !58
  %arrayidx = getelementptr inbounds [4098 x float], ptr @a, i64 0, i64 %idxprom, !dbg !58
  store float %2, ptr %arrayidx, align 4, !dbg !59, !tbaa !35
  %4 = load i32, ptr %i, align 4, !dbg !60, !tbaa !10
  %rem1 = srem i32 %4, 13, !dbg !61
  %conv2 = sitofp i32 %rem1 to float, !dbg !62
  %5 = call float @llvm.fmuladd.f32(float %conv2, float 6.250000e-02, float -5.000000e-01), !dbg !63
  %6 = load i32, ptr %i, align 4, !dbg !64, !tbaa !10
  %idxprom3 = sext i32 %6 to i64, !dbg !65
  %arrayidx4 = getelementptr inbounds [4098 x float], ptr @b, i64 0, i64 %idxprom3, !dbg !65
  store float %5, ptr %arrayidx4, align 4, !dbg !66, !tbaa !35
  %7 = load i32, ptr %i, align 4, !dbg !67, !tbaa !10
  %rem5 = srem i32 %7, 19, !dbg !68
  %conv6 = sitofp i32 %rem5 to float, !dbg !69
  %8 = call float @llvm.fmuladd.f32(float %conv6, float 1.562500e-02, float 1.250000e-01), !dbg !70
  %9 = load i32, ptr %i, align 4, !dbg !71, !tbaa !10
  %idxprom7 = sext i32 %9 to i64, !dbg !72
  %arrayidx8 = getelementptr inbounds [4098 x float], ptr @c, i64 0, i64 %idxprom7, !dbg !72
  store float %8, ptr %arrayidx8, align 4, !dbg !73, !tbaa !35
  br label %for.inc, !dbg !74

for.inc:                                          ; preds = %for.body
  %10 = load i32, ptr %i, align 4, !dbg !75, !tbaa !10
  %inc = add nsw i32 %10, 1, !dbg !75
  store i32 %inc, ptr %i, align 4, !dbg !75, !tbaa !10
  br label %for.cond, !dbg !52, !llvm.loop !76

for.end:                                          ; preds = %for.cond.cleanup
  call void @llvm.lifetime.start.p0(ptr %i9) #8, !dbg !77
  store i32 0, ptr %i9, align 4, !dbg !78, !tbaa !10
  br label %for.cond10, !dbg !77

for.cond10:                                       ; preds = %for.inc23, %for.end
  %11 = load i32, ptr %i9, align 4, !dbg !79, !tbaa !10
  %cmp11 = icmp slt i32 %11, 4096, !dbg !80
  br i1 %cmp11, label %for.body14, label %for.cond.cleanup13, !dbg !81

for.cond.cleanup13:                               ; preds = %for.cond10
  call void @llvm.lifetime.end.p0(ptr %i9) #8, !dbg !81
  br label %for.end25

for.body14:                                       ; preds = %for.cond10
  %12 = load i32, ptr %i9, align 4, !dbg !82, !tbaa !10
  %rem19 = srem i32 %12, 7, !dbg !83
  %conv20 = sitofp i32 %rem19 to float, !dbg !84
  %13 = call float @llvm.fmuladd.f32(float %conv20, float 3.125000e-02, float 1.250000e-01), !dbg !85
  %14 = load i32, ptr %i9, align 4, !dbg !86, !tbaa !10
  %idxprom21 = sext i32 %14 to i64, !dbg !87
  %arrayidx22 = getelementptr inbounds [4096 x float], ptr @output, i64 0, i64 %idxprom21, !dbg !87
  store float %13, ptr %arrayidx22, align 4, !dbg !88, !tbaa !35
  br label %for.inc23, !dbg !89

for.inc23:                                        ; preds = %for.body14
  %15 = load i32, ptr %i9, align 4, !dbg !90, !tbaa !10
  %inc24 = add nsw i32 %15, 1, !dbg !90
  store i32 %inc24, ptr %i9, align 4, !dbg !90, !tbaa !10
  br label %for.cond10, !dbg !81, !llvm.loop !91

for.end25:                                        ; preds = %for.cond.cleanup13
  ret void, !dbg !92
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(ptr captures(none)) #2

; Function Attrs: noinline nounwind vscale_range(2,1024)
define internal fastcc void @selected_kernel() unnamed_addr #3 !dbg !93 {
entry:
  tail call void asm sideeffect "", "~{memory}"() #8, !dbg !94, !srcloc !95
  %.pre = load float, ptr getelementptr inbounds nuw (i8, ptr @a, i64 4), align 4, !dbg !96, !tbaa !35
  %0 = tail call i64 @llvm.vscale.i64()
  %1 = shl nuw nsw i64 %0, 1
  %2 = tail call i32 @llvm.vscale.i32(), !dbg !97
  %3 = shl nuw nsw i32 %2, 1, !dbg !97
  %4 = add nsw i32 %3, -1, !dbg !97
  %vector.recur.init = insertelement <vscale x 2 x float> poison, float %.pre, i32 %4, !dbg !97
  br label %vector.body, !dbg !97

vector.body:                                      ; preds = %vector.body, %entry
  %index = phi i64 [ 0, %entry ], [ %index.next, %vector.body ]
  %vector.recur = phi <vscale x 2 x float> [ %vector.recur.init, %entry ], [ %wide.load25, %vector.body ]
  %offset.idx = or disjoint i64 %index, 1
  %5 = getelementptr inbounds float, ptr @a, i64 %index, !dbg !98
  %wide.load = load <vscale x 2 x float>, ptr %5, align 8, !dbg !98, !tbaa !35
  %6 = getelementptr inbounds nuw float, ptr @a, i64 %index, !dbg !99
  %7 = getelementptr inbounds nuw i8, ptr %6, i64 8, !dbg !99
  %wide.load25 = load <vscale x 2 x float>, ptr %7, align 8, !dbg !99, !tbaa !35
  %8 = tail call <vscale x 2 x float> @llvm.vector.splice.right.nxv2f32(<vscale x 2 x float> %vector.recur, <vscale x 2 x float> %wide.load25, i32 1)
  %9 = fadd fast <vscale x 2 x float> %wide.load, %8, !dbg !100
  %10 = fadd fast <vscale x 2 x float> %9, %wide.load25, !dbg !101
  %11 = getelementptr inbounds nuw float, ptr @c, i64 %offset.idx, !dbg !102
  %wide.load26 = load <vscale x 2 x float>, ptr %11, align 4, !dbg !102, !tbaa !35
  %12 = fadd fast <vscale x 2 x float> %10, %wide.load26, !dbg !103
  %13 = getelementptr inbounds nuw float, ptr @b, i64 %offset.idx, !dbg !104
  %wide.load27 = load <vscale x 2 x float>, ptr %13, align 4, !dbg !104, !tbaa !35
  %14 = fadd fast <vscale x 2 x float> %12, %wide.load27, !dbg !105
  %15 = fmul fast <vscale x 2 x float> %14, splat (float 0x3FC99999A0000000), !dbg !106
  %16 = getelementptr inbounds float, ptr @output, i64 %index, !dbg !107
  store <vscale x 2 x float> %15, ptr %16, align 8, !dbg !108, !tbaa !35
  %index.next = add nuw i64 %index, %1
  %17 = icmp eq i64 %index.next, 4096, !dbg !97
  br i1 %17, label %for.cond.cleanup, label %vector.body, !dbg !97, !llvm.loop !109

for.cond.cleanup:                                 ; preds = %vector.body
  ret void, !dbg !112
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(ptr captures(none)) #2

; Function Attrs: noinline nounwind optnone vscale_range(2,1024)
define internal fastcc void @consume_outputs() unnamed_addr #1 !dbg !113 {
entry:
  %sum = alloca float, align 4
  %i = alloca i32, align 4
  call void @llvm.lifetime.start.p0(ptr %sum) #8, !dbg !114
  %add = fadd float 0.000000e+00, 0.000000e+00, !dbg !115
  store float %add, ptr %sum, align 4, !dbg !116, !tbaa !35
  call void @llvm.lifetime.start.p0(ptr %i) #8, !dbg !117
  store i32 0, ptr %i, align 4, !dbg !118, !tbaa !10
  br label %for.cond, !dbg !117

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, ptr %i, align 4, !dbg !119, !tbaa !10
  %cmp = icmp slt i32 %0, 4096, !dbg !120
  br i1 %cmp, label %for.body, label %for.cond.cleanup, !dbg !121

for.cond.cleanup:                                 ; preds = %for.cond
  call void @llvm.lifetime.end.p0(ptr %i) #8, !dbg !121
  br label %for.end

for.body:                                         ; preds = %for.cond
  %1 = load i32, ptr %i, align 4, !dbg !122, !tbaa !10
  %idxprom = sext i32 %1 to i64, !dbg !123
  %arrayidx = getelementptr inbounds [4096 x float], ptr @output, i64 0, i64 %idxprom, !dbg !123
  %2 = load float, ptr %arrayidx, align 4, !dbg !123, !tbaa !35
  %3 = load float, ptr %sum, align 4, !dbg !124, !tbaa !35
  %add1 = fadd float %3, %2, !dbg !124
  store float %add1, ptr %sum, align 4, !dbg !124, !tbaa !35
  br label %for.inc, !dbg !125

for.inc:                                          ; preds = %for.body
  %4 = load i32, ptr %i, align 4, !dbg !126, !tbaa !10
  %inc = add nsw i32 %4, 1, !dbg !126
  store i32 %inc, ptr %i, align 4, !dbg !126, !tbaa !10
  br label %for.cond, !dbg !121, !llvm.loop !127

for.end:                                          ; preds = %for.cond.cleanup
  %5 = load float, ptr %sum, align 4, !dbg !129, !tbaa !35
  store volatile float %5, ptr @result_sink, align 4, !dbg !130, !tbaa !35
  call void @llvm.lifetime.end.p0(ptr %sum) #8, !dbg !131
  ret void, !dbg !131
}

declare dso_local signext i32 @printf(ptr noundef, ...) local_unnamed_addr #4

; Function Attrs: mustprogress nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none)
declare float @llvm.fmuladd.f32(float, float, float) #5

declare dso_local i64 @builder_platform_cycle() local_unnamed_addr #4

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.vscale.i64() #6

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.vscale.i32() #6

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare <vscale x 2 x float> @llvm.vector.splice.right.nxv2f32(<vscale x 2 x float>, <vscale x 2 x float>, i32 immarg) #6

attributes #0 = { nounwind vscale_range(2,1024) "no-builtins" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="xiangshan-kunminghu" "target-features"="+64bit,+a,+b,+c,+d,+f,+i,+m,+v,+zaamo,+zalrsc,+zba,+zbb,+zbc,+zbs,+zca,+zcd,+zicbom,+zicboz,+zicsr,+zifencei,+zmmul,+zve32f,+zve32x,+zve64d,+zve64f,+zve64x,+zvl128b,+zvl32b,+zvl64b,-e,-experimental-p,-experimental-smpmpmt,-experimental-svukte,-experimental-xrivosvisni,-experimental-xrivosvizip,-experimental-xsfmclic,-experimental-xsfsclic,-experimental-zibi,-experimental-zicfilp,-experimental-zicfiss,-experimental-zvbc32e,-experimental-zvfbfa,-experimental-zvfofp8min,-experimental-zvkgs,-experimental-zvqdotq,-h,-q,-relax,-sdext,-sdtrig,-sha,-shcounterenw,-shgatpa,-shlcofideleg,-shtvala,-shvsatpa,-shvstvala,-shvstvecd,-smaia,-smcdeleg,-smcntrpmf,-smcsrind,-smctr,-smdbltrp,-smepmp,-smmpm,-smnpm,-smrnmi,-smstateen,-ssaia,-ssccfg,-ssccptr,-sscofpmf,-sscounterenw,-sscsrind,-ssctr,-ssdbltrp,-ssnpm,-sspm,-ssqosid,-ssstateen,-ssstrict,-sstc,-sstvala,-sstvecd,-ssu64xl,-supm,-svade,-svadu,-svbare,-svinval,-svnapot,-svpbmt,-svvptc,-xandesbfhcvt,-xandesperf,-xandesvbfhcvt,-xandesvdot,-xandesvpackfph,-xandesvsinth,-xandesvsintload,-xcvalu,-xcvbi,-xcvbitmanip,-xcvelw,-xcvmac,-xcvmem,-xcvsimd,-xmipscbop,-xmipscmov,-xmipsexectl,-xmipslsp,-xqccmp,-xqci,-xqcia,-xqciac,-xqcibi,-xqcibm,-xqcicli,-xqcicm,-xqcics,-xqcicsr,-xqciint,-xqciio,-xqcilb,-xqcili,-xqcilia,-xqcilo,-xqcilsm,-xqcisim,-xqcisls,-xqcisync,-xsfcease,-xsfmm128t,-xsfmm16t,-xsfmm32a16f,-xsfmm32a32f,-xsfmm32a8f,-xsfmm32a8i,-xsfmm32t,-xsfmm64a64f,-xsfmm64t,-xsfmmbase,-xsfvcp,-xsfvfbfexp16e,-xsfvfexp16e,-xsfvfexp32e,-xsfvfexpa,-xsfvfexpa64e,-xsfvfnrclipxfqf,-xsfvfwmaccqqq,-xsfvqmaccdod,-xsfvqmaccqoq,-xsifivecdiscarddlone,-xsifivecflushdlone,-xsmtvdot,-xtheadba,-xtheadbb,-xtheadbs,-xtheadcmo,-xtheadcondmov,-xtheadfmemidx,-xtheadmac,-xtheadmemidx,-xtheadmempair,-xtheadsync,-xtheadvdot,-xventanacondops,-xwchc,-za128rs,-za64rs,-zabha,-zacas,-zalasr,-zama16b,-zawrs,-zbkb,-zbkc,-zbkx,-zcb,-zce,-zcf,-zclsd,-zcmop,-zcmp,-zcmt,-zdinx,-zfa,-zfbfmin,-zfh,-zfhmin,-zfinx,-zhinx,-zhinxmin,-zic64b,-zicbop,-ziccamoa,-ziccamoc,-ziccif,-zicclsm,-ziccrse,-zicntr,-zicond,-zihintntl,-zihintpause,-zihpm,-zilsd,-zimop,-zk,-zkn,-zknd,-zkne,-zknh,-zkr,-zks,-zksed,-zksh,-zkt,-ztso,-zvbb,-zvbc,-zvfbfmin,-zvfbfwma,-zvfh,-zvfhmin,-zvkb,-zvkg,-zvkn,-zvknc,-zvkned,-zvkng,-zvknha,-zvknhb,-zvks,-zvksc,-zvksed,-zvksg,-zvksh,-zvkt,-zvl1024b,-zvl16384b,-zvl2048b,-zvl256b,-zvl32768b,-zvl4096b,-zvl512b,-zvl65536b,-zvl8192b" }
attributes #1 = { noinline nounwind optnone vscale_range(2,1024) "no-builtins" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="xiangshan-kunminghu" "target-features"="+64bit,+a,+b,+c,+d,+f,+i,+m,+v,+zaamo,+zalrsc,+zba,+zbb,+zbc,+zbs,+zca,+zcd,+zicbom,+zicboz,+zicsr,+zifencei,+zmmul,+zve32f,+zve32x,+zve64d,+zve64f,+zve64x,+zvl128b,+zvl32b,+zvl64b,-e,-experimental-p,-experimental-smpmpmt,-experimental-svukte,-experimental-xrivosvisni,-experimental-xrivosvizip,-experimental-xsfmclic,-experimental-xsfsclic,-experimental-zibi,-experimental-zicfilp,-experimental-zicfiss,-experimental-zvbc32e,-experimental-zvfbfa,-experimental-zvfofp8min,-experimental-zvkgs,-experimental-zvqdotq,-h,-q,-relax,-sdext,-sdtrig,-sha,-shcounterenw,-shgatpa,-shlcofideleg,-shtvala,-shvsatpa,-shvstvala,-shvstvecd,-smaia,-smcdeleg,-smcntrpmf,-smcsrind,-smctr,-smdbltrp,-smepmp,-smmpm,-smnpm,-smrnmi,-smstateen,-ssaia,-ssccfg,-ssccptr,-sscofpmf,-sscounterenw,-sscsrind,-ssctr,-ssdbltrp,-ssnpm,-sspm,-ssqosid,-ssstateen,-ssstrict,-sstc,-sstvala,-sstvecd,-ssu64xl,-supm,-svade,-svadu,-svbare,-svinval,-svnapot,-svpbmt,-svvptc,-xandesbfhcvt,-xandesperf,-xandesvbfhcvt,-xandesvdot,-xandesvpackfph,-xandesvsinth,-xandesvsintload,-xcvalu,-xcvbi,-xcvbitmanip,-xcvelw,-xcvmac,-xcvmem,-xcvsimd,-xmipscbop,-xmipscmov,-xmipsexectl,-xmipslsp,-xqccmp,-xqci,-xqcia,-xqciac,-xqcibi,-xqcibm,-xqcicli,-xqcicm,-xqcics,-xqcicsr,-xqciint,-xqciio,-xqcilb,-xqcili,-xqcilia,-xqcilo,-xqcilsm,-xqcisim,-xqcisls,-xqcisync,-xsfcease,-xsfmm128t,-xsfmm16t,-xsfmm32a16f,-xsfmm32a32f,-xsfmm32a8f,-xsfmm32a8i,-xsfmm32t,-xsfmm64a64f,-xsfmm64t,-xsfmmbase,-xsfvcp,-xsfvfbfexp16e,-xsfvfexp16e,-xsfvfexp32e,-xsfvfexpa,-xsfvfexpa64e,-xsfvfnrclipxfqf,-xsfvfwmaccqqq,-xsfvqmaccdod,-xsfvqmaccqoq,-xsifivecdiscarddlone,-xsifivecflushdlone,-xsmtvdot,-xtheadba,-xtheadbb,-xtheadbs,-xtheadcmo,-xtheadcondmov,-xtheadfmemidx,-xtheadmac,-xtheadmemidx,-xtheadmempair,-xtheadsync,-xtheadvdot,-xventanacondops,-xwchc,-za128rs,-za64rs,-zabha,-zacas,-zalasr,-zama16b,-zawrs,-zbkb,-zbkc,-zbkx,-zcb,-zce,-zcf,-zclsd,-zcmop,-zcmp,-zcmt,-zdinx,-zfa,-zfbfmin,-zfh,-zfhmin,-zfinx,-zhinx,-zhinxmin,-zic64b,-zicbop,-ziccamoa,-ziccamoc,-ziccif,-zicclsm,-ziccrse,-zicntr,-zicond,-zihintntl,-zihintpause,-zihpm,-zilsd,-zimop,-zk,-zkn,-zknd,-zkne,-zknh,-zkr,-zks,-zksed,-zksh,-zkt,-ztso,-zvbb,-zvbc,-zvfbfmin,-zvfbfwma,-zvfh,-zvfhmin,-zvkb,-zvkg,-zvkn,-zvknc,-zvkned,-zvkng,-zvknha,-zvknhb,-zvks,-zvksc,-zvksed,-zvksg,-zvksh,-zvkt,-zvl1024b,-zvl16384b,-zvl2048b,-zvl256b,-zvl32768b,-zvl4096b,-zvl512b,-zvl65536b,-zvl8192b" }
attributes #2 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #3 = { noinline nounwind vscale_range(2,1024) "no-builtins" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="xiangshan-kunminghu" "target-features"="+64bit,+a,+b,+c,+d,+f,+i,+m,+v,+zaamo,+zalrsc,+zba,+zbb,+zbc,+zbs,+zca,+zcd,+zicbom,+zicboz,+zicsr,+zifencei,+zmmul,+zve32f,+zve32x,+zve64d,+zve64f,+zve64x,+zvl128b,+zvl32b,+zvl64b,-e,-experimental-p,-experimental-smpmpmt,-experimental-svukte,-experimental-xrivosvisni,-experimental-xrivosvizip,-experimental-xsfmclic,-experimental-xsfsclic,-experimental-zibi,-experimental-zicfilp,-experimental-zicfiss,-experimental-zvbc32e,-experimental-zvfbfa,-experimental-zvfofp8min,-experimental-zvkgs,-experimental-zvqdotq,-h,-q,-relax,-sdext,-sdtrig,-sha,-shcounterenw,-shgatpa,-shlcofideleg,-shtvala,-shvsatpa,-shvstvala,-shvstvecd,-smaia,-smcdeleg,-smcntrpmf,-smcsrind,-smctr,-smdbltrp,-smepmp,-smmpm,-smnpm,-smrnmi,-smstateen,-ssaia,-ssccfg,-ssccptr,-sscofpmf,-sscounterenw,-sscsrind,-ssctr,-ssdbltrp,-ssnpm,-sspm,-ssqosid,-ssstateen,-ssstrict,-sstc,-sstvala,-sstvecd,-ssu64xl,-supm,-svade,-svadu,-svbare,-svinval,-svnapot,-svpbmt,-svvptc,-xandesbfhcvt,-xandesperf,-xandesvbfhcvt,-xandesvdot,-xandesvpackfph,-xandesvsinth,-xandesvsintload,-xcvalu,-xcvbi,-xcvbitmanip,-xcvelw,-xcvmac,-xcvmem,-xcvsimd,-xmipscbop,-xmipscmov,-xmipsexectl,-xmipslsp,-xqccmp,-xqci,-xqcia,-xqciac,-xqcibi,-xqcibm,-xqcicli,-xqcicm,-xqcics,-xqcicsr,-xqciint,-xqciio,-xqcilb,-xqcili,-xqcilia,-xqcilo,-xqcilsm,-xqcisim,-xqcisls,-xqcisync,-xsfcease,-xsfmm128t,-xsfmm16t,-xsfmm32a16f,-xsfmm32a32f,-xsfmm32a8f,-xsfmm32a8i,-xsfmm32t,-xsfmm64a64f,-xsfmm64t,-xsfmmbase,-xsfvcp,-xsfvfbfexp16e,-xsfvfexp16e,-xsfvfexp32e,-xsfvfexpa,-xsfvfexpa64e,-xsfvfnrclipxfqf,-xsfvfwmaccqqq,-xsfvqmaccdod,-xsfvqmaccqoq,-xsifivecdiscarddlone,-xsifivecflushdlone,-xsmtvdot,-xtheadba,-xtheadbb,-xtheadbs,-xtheadcmo,-xtheadcondmov,-xtheadfmemidx,-xtheadmac,-xtheadmemidx,-xtheadmempair,-xtheadsync,-xtheadvdot,-xventanacondops,-xwchc,-za128rs,-za64rs,-zabha,-zacas,-zalasr,-zama16b,-zawrs,-zbkb,-zbkc,-zbkx,-zcb,-zce,-zcf,-zclsd,-zcmop,-zcmp,-zcmt,-zdinx,-zfa,-zfbfmin,-zfh,-zfhmin,-zfinx,-zhinx,-zhinxmin,-zic64b,-zicbop,-ziccamoa,-ziccamoc,-ziccif,-zicclsm,-ziccrse,-zicntr,-zicond,-zihintntl,-zihintpause,-zihpm,-zilsd,-zimop,-zk,-zkn,-zknd,-zkne,-zknh,-zkr,-zks,-zksed,-zksh,-zkt,-ztso,-zvbb,-zvbc,-zvfbfmin,-zvfbfwma,-zvfh,-zvfhmin,-zvkb,-zvkg,-zvkn,-zvknc,-zvkned,-zvkng,-zvknha,-zvknhb,-zvks,-zvksc,-zvksed,-zvksg,-zvksh,-zvkt,-zvl1024b,-zvl16384b,-zvl2048b,-zvl256b,-zvl32768b,-zvl4096b,-zvl512b,-zvl65536b,-zvl8192b" }
attributes #4 = { "no-builtins" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="xiangshan-kunminghu" "target-features"="+64bit,+a,+b,+c,+d,+f,+i,+m,+v,+zaamo,+zalrsc,+zba,+zbb,+zbc,+zbs,+zca,+zcd,+zicbom,+zicboz,+zicsr,+zifencei,+zmmul,+zve32f,+zve32x,+zve64d,+zve64f,+zve64x,+zvl128b,+zvl32b,+zvl64b,-e,-experimental-p,-experimental-smpmpmt,-experimental-svukte,-experimental-xrivosvisni,-experimental-xrivosvizip,-experimental-xsfmclic,-experimental-xsfsclic,-experimental-zibi,-experimental-zicfilp,-experimental-zicfiss,-experimental-zvbc32e,-experimental-zvfbfa,-experimental-zvfofp8min,-experimental-zvkgs,-experimental-zvqdotq,-h,-q,-relax,-sdext,-sdtrig,-sha,-shcounterenw,-shgatpa,-shlcofideleg,-shtvala,-shvsatpa,-shvstvala,-shvstvecd,-smaia,-smcdeleg,-smcntrpmf,-smcsrind,-smctr,-smdbltrp,-smepmp,-smmpm,-smnpm,-smrnmi,-smstateen,-ssaia,-ssccfg,-ssccptr,-sscofpmf,-sscounterenw,-sscsrind,-ssctr,-ssdbltrp,-ssnpm,-sspm,-ssqosid,-ssstateen,-ssstrict,-sstc,-sstvala,-sstvecd,-ssu64xl,-supm,-svade,-svadu,-svbare,-svinval,-svnapot,-svpbmt,-svvptc,-xandesbfhcvt,-xandesperf,-xandesvbfhcvt,-xandesvdot,-xandesvpackfph,-xandesvsinth,-xandesvsintload,-xcvalu,-xcvbi,-xcvbitmanip,-xcvelw,-xcvmac,-xcvmem,-xcvsimd,-xmipscbop,-xmipscmov,-xmipsexectl,-xmipslsp,-xqccmp,-xqci,-xqcia,-xqciac,-xqcibi,-xqcibm,-xqcicli,-xqcicm,-xqcics,-xqcicsr,-xqciint,-xqciio,-xqcilb,-xqcili,-xqcilia,-xqcilo,-xqcilsm,-xqcisim,-xqcisls,-xqcisync,-xsfcease,-xsfmm128t,-xsfmm16t,-xsfmm32a16f,-xsfmm32a32f,-xsfmm32a8f,-xsfmm32a8i,-xsfmm32t,-xsfmm64a64f,-xsfmm64t,-xsfmmbase,-xsfvcp,-xsfvfbfexp16e,-xsfvfexp16e,-xsfvfexp32e,-xsfvfexpa,-xsfvfexpa64e,-xsfvfnrclipxfqf,-xsfvfwmaccqqq,-xsfvqmaccdod,-xsfvqmaccqoq,-xsifivecdiscarddlone,-xsifivecflushdlone,-xsmtvdot,-xtheadba,-xtheadbb,-xtheadbs,-xtheadcmo,-xtheadcondmov,-xtheadfmemidx,-xtheadmac,-xtheadmemidx,-xtheadmempair,-xtheadsync,-xtheadvdot,-xventanacondops,-xwchc,-za128rs,-za64rs,-zabha,-zacas,-zalasr,-zama16b,-zawrs,-zbkb,-zbkc,-zbkx,-zcb,-zce,-zcf,-zclsd,-zcmop,-zcmp,-zcmt,-zdinx,-zfa,-zfbfmin,-zfh,-zfhmin,-zfinx,-zhinx,-zhinxmin,-zic64b,-zicbop,-ziccamoa,-ziccamoc,-ziccif,-zicclsm,-ziccrse,-zicntr,-zicond,-zihintntl,-zihintpause,-zihpm,-zilsd,-zimop,-zk,-zkn,-zknd,-zkne,-zknh,-zkr,-zks,-zksed,-zksh,-zkt,-ztso,-zvbb,-zvbc,-zvfbfmin,-zvfbfwma,-zvfh,-zvfhmin,-zvkb,-zvkg,-zvkn,-zvknc,-zvkned,-zvkng,-zvknha,-zvknhb,-zvks,-zvksc,-zvksed,-zvksg,-zvksh,-zvkt,-zvl1024b,-zvl16384b,-zvl2048b,-zvl256b,-zvl32768b,-zvl4096b,-zvl512b,-zvl65536b,-zvl8192b" }
attributes #5 = { mustprogress nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none) }
attributes #6 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #7 = { nobuiltin "no-builtins" }
attributes #8 = { nounwind }
attributes #9 = { nobuiltin nounwind "no-builtins" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !7, !8}
!llvm.ident = !{!9}
!llvm.errno.tbaa = !{!10}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang version 22.1.8 (https://github.com/llvm/llvm-project.git ca7933e47d3a3451d81e72ac174dcb5aa28b59d1)", isOptimized: true, runtimeVersion: 0, emissionKind: NoDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/Users/cheolwanpark/Documents/project/llvm-project/experiments/tp-lcd/inputs/application_kernels.c", directory: "/Users/cheolwanpark/Documents/project/llvm-project")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = !{i32 1, !"wchar_size", i32 4}
!4 = !{i32 1, !"target-abi", !"lp64d"}
!5 = !{i32 6, !"riscv-isa", !6}
!6 = !{!"rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_b1p0_v1p0_zicbom1p0_zicboz1p0_zicsr2p0_zifencei2p0_zmmul1p0_zaamo1p0_zalrsc1p0_zca1p0_zcd1p0_zba1p0_zbb1p0_zbc1p0_zbs1p0_zve32f1p0_zve32x1p0_zve64d1p0_zve64f1p0_zve64x1p0_zvl128b1p0_zvl32b1p0_zvl64b1p0"}
!7 = !{i32 1, !"Code Model", i32 3}
!8 = !{i32 8, !"SmallDataLimit", i32 0}
!9 = !{!"clang version 22.1.8 (https://github.com/llvm/llvm-project.git ca7933e47d3a3451d81e72ac174dcb5aa28b59d1)"}
!10 = !{!11, !11, i64 0}
!11 = !{!"int", !12, i64 0}
!12 = !{!"omnipotent char", !13, i64 0}
!13 = !{!"Simple C/C++ TBAA"}
!14 = distinct !DISubprogram(name: "builder_main", scope: !15, file: !15, line: 138, type: !16, scopeLine: 138, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!15 = !DIFile(filename: "experiments/tp-lcd/inputs/application_kernels.c", directory: "/Users/cheolwanpark/Documents/project/llvm-project")
!16 = !DISubroutineType(types: !17)
!17 = !{}
!18 = !DILocation(line: 141, column: 5, scope: !14)
!19 = !DILocation(line: 129, column: 5, scope: !20, inlinedAt: !21)
!20 = distinct !DISubprogram(name: "read_cycle", scope: !15, file: !15, line: 127, type: !16, scopeLine: 127, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!21 = distinct !DILocation(line: 142, column: 22, scope: !14)
!22 = !{i64 4291}
!23 = !DILocation(line: 131, column: 22, scope: !20, inlinedAt: !21)
!24 = !DILocation(line: 133, column: 5, scope: !20, inlinedAt: !21)
!25 = !{i64 4411}
!26 = !DILocation(line: 144, column: 5, scope: !14)
!27 = !DILocation(line: 129, column: 5, scope: !20, inlinedAt: !28)
!28 = distinct !DILocation(line: 146, column: 20, scope: !14)
!29 = !DILocation(line: 131, column: 22, scope: !20, inlinedAt: !28)
!30 = !DILocation(line: 133, column: 5, scope: !20, inlinedAt: !28)
!31 = !DILocation(line: 147, column: 5, scope: !14)
!32 = !DILocation(line: 148, column: 57, scope: !14)
!33 = !DILocation(line: 148, column: 5, scope: !14)
!34 = !DILocation(line: 149, column: 12, scope: !14)
!35 = !{!36, !36, i64 0}
!36 = !{!"float", !12, i64 0}
!37 = !DILocation(line: 149, column: 24, scope: !14)
!38 = !DILocation(line: 149, column: 5, scope: !14)
!39 = !DILocation(line: 145, column: 9, scope: !14)
!40 = !DILocation(line: 144, column: 48, scope: !14)
!41 = !DILocation(line: 144, column: 33, scope: !14)
!42 = distinct !{!42, !26, !43, !44, !45, !46}
!43 = !DILocation(line: 145, column: 25, scope: !14)
!44 = !{!"llvm.loop.mustprogress"}
!45 = !{!"llvm.loop.unroll.disable"}
!46 = !{!"llvm.loop.vectorize.width", i32 1}
!47 = distinct !DISubprogram(name: "initialise_arrays", scope: !15, file: !15, line: 42, type: !16, scopeLine: 42, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!48 = !DILocation(line: 43, column: 10, scope: !47)
!49 = !DILocation(line: 43, column: 14, scope: !47)
!50 = !DILocation(line: 43, column: 21, scope: !47)
!51 = !DILocation(line: 43, column: 23, scope: !47)
!52 = !DILocation(line: 43, column: 5, scope: !47)
!53 = !DILocation(line: 44, column: 32, scope: !47)
!54 = !DILocation(line: 44, column: 34, scope: !47)
!55 = !DILocation(line: 44, column: 24, scope: !47)
!56 = !DILocation(line: 44, column: 22, scope: !47)
!57 = !DILocation(line: 44, column: 11, scope: !47)
!58 = !DILocation(line: 44, column: 9, scope: !47)
!59 = !DILocation(line: 44, column: 14, scope: !47)
!60 = !DILocation(line: 45, column: 32, scope: !47)
!61 = !DILocation(line: 45, column: 34, scope: !47)
!62 = !DILocation(line: 45, column: 24, scope: !47)
!63 = !DILocation(line: 45, column: 22, scope: !47)
!64 = !DILocation(line: 45, column: 11, scope: !47)
!65 = !DILocation(line: 45, column: 9, scope: !47)
!66 = !DILocation(line: 45, column: 14, scope: !47)
!67 = !DILocation(line: 46, column: 33, scope: !47)
!68 = !DILocation(line: 46, column: 35, scope: !47)
!69 = !DILocation(line: 46, column: 25, scope: !47)
!70 = !DILocation(line: 46, column: 23, scope: !47)
!71 = !DILocation(line: 46, column: 11, scope: !47)
!72 = !DILocation(line: 46, column: 9, scope: !47)
!73 = !DILocation(line: 46, column: 14, scope: !47)
!74 = !DILocation(line: 47, column: 5, scope: !47)
!75 = !DILocation(line: 43, column: 44, scope: !47)
!76 = distinct !{!76, !52, !74, !44, !45}
!77 = !DILocation(line: 48, column: 10, scope: !47)
!78 = !DILocation(line: 48, column: 14, scope: !47)
!79 = !DILocation(line: 48, column: 21, scope: !47)
!80 = !DILocation(line: 48, column: 23, scope: !47)
!81 = !DILocation(line: 48, column: 5, scope: !47)
!82 = !DILocation(line: 50, column: 38, scope: !47)
!83 = !DILocation(line: 50, column: 40, scope: !47)
!84 = !DILocation(line: 50, column: 30, scope: !47)
!85 = !DILocation(line: 50, column: 28, scope: !47)
!86 = !DILocation(line: 50, column: 16, scope: !47)
!87 = !DILocation(line: 50, column: 9, scope: !47)
!88 = !DILocation(line: 50, column: 19, scope: !47)
!89 = !DILocation(line: 51, column: 5, scope: !47)
!90 = !DILocation(line: 48, column: 40, scope: !47)
!91 = distinct !{!91, !81, !89, !44, !45}
!92 = !DILocation(line: 59, column: 1, scope: !47)
!93 = distinct !DISubprogram(name: "selected_kernel", scope: !15, file: !15, line: 62, type: !16, scopeLine: 62, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!94 = !DILocation(line: 63, column: 5, scope: !93)
!95 = !{i64 2116}
!96 = !DILocation(line: 68, column: 33, scope: !93)
!97 = !DILocation(line: 67, column: 5, scope: !93)
!98 = !DILocation(line: 68, column: 40, scope: !93)
!99 = !DILocation(line: 68, column: 51, scope: !93)
!100 = !DILocation(line: 68, column: 38, scope: !93)
!101 = !DILocation(line: 68, column: 49, scope: !93)
!102 = !DILocation(line: 68, column: 62, scope: !93)
!103 = !DILocation(line: 68, column: 60, scope: !93)
!104 = !DILocation(line: 68, column: 69, scope: !93)
!105 = !DILocation(line: 68, column: 67, scope: !93)
!106 = !DILocation(line: 68, column: 30, scope: !93)
!107 = !DILocation(line: 68, column: 9, scope: !93)
!108 = !DILocation(line: 68, column: 23, scope: !93)
!109 = distinct !{!109, !44, !45, !110, !111}
!110 = !{!"llvm.loop.isvectorized", i32 1}
!111 = !{!"llvm.loop.unroll.runtime.disable"}
!112 = !DILocation(line: 114, column: 1, scope: !93)
!113 = distinct !DISubprogram(name: "consume_outputs", scope: !15, file: !15, line: 118, type: !16, scopeLine: 118, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!114 = !DILocation(line: 119, column: 5, scope: !113)
!115 = !DILocation(line: 119, column: 28, scope: !113)
!116 = !DILocation(line: 119, column: 11, scope: !113)
!117 = !DILocation(line: 120, column: 10, scope: !113)
!118 = !DILocation(line: 120, column: 14, scope: !113)
!119 = !DILocation(line: 120, column: 21, scope: !113)
!120 = !DILocation(line: 120, column: 23, scope: !113)
!121 = !DILocation(line: 120, column: 5, scope: !113)
!122 = !DILocation(line: 121, column: 23, scope: !113)
!123 = !DILocation(line: 121, column: 16, scope: !113)
!124 = !DILocation(line: 121, column: 13, scope: !113)
!125 = !DILocation(line: 121, column: 9, scope: !113)
!126 = !DILocation(line: 120, column: 40, scope: !113)
!127 = distinct !{!127, !121, !128, !44, !45}
!128 = !DILocation(line: 121, column: 24, scope: !113)
!129 = !DILocation(line: 122, column: 19, scope: !113)
!130 = !DILocation(line: 122, column: 17, scope: !113)
!131 = !DILocation(line: 123, column: 1, scope: !113)
