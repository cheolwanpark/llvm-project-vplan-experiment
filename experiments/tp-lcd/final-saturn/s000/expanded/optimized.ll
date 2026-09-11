; ModuleID = '/Users/cheolwanpark/Documents/project/llvm-project/experiments/tp-lcd/inputs/tsvc_kernels.c'
source_filename = "/Users/cheolwanpark/Documents/project/llvm-project/experiments/tp-lcd/inputs/tsvc_kernels.c"
target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128"
target triple = "riscv64-unknown-unknown-elf"

@result_sink = internal global float 0.000000e+00, align 4
@.str = private unnamed_addr constant [16 x i8] c"MB_ROI=%016llx\0A\00", align 1
@a = internal unnamed_addr global [8192 x float] zeroinitializer, align 64
@b = internal unnamed_addr global [4096 x float] zeroinitializer, align 64

; Function Attrs: nounwind vscale_range(2,1024)
define dso_local signext range(i32 0, 2) i32 @builder_main(i32 noundef signext %argc, ptr noundef readnone captures(none) %argv) local_unnamed_addr #0 !dbg !14 {
entry:
  tail call fastcc void @initialise_arrays() #9, !dbg !18
  tail call void asm sideeffect "fence rw, rw", "~{memory}"() #10, !dbg !19, !srcloc !22
  %call.i = tail call i64 @builder_platform_cycle() #11, !dbg !23
  tail call void asm sideeffect "fence rw, rw", "~{memory}"() #10, !dbg !24, !srcloc !25
  br label %for.body, !dbg !26

for.cond.cleanup:                                 ; preds = %for.body
  tail call void asm sideeffect "fence rw, rw", "~{memory}"() #10, !dbg !27, !srcloc !22
  %call.i7 = tail call i64 @builder_platform_cycle() #11, !dbg !29
  tail call void asm sideeffect "fence rw, rw", "~{memory}"() #10, !dbg !30, !srcloc !25
  store volatile float %add, ptr @result_sink, align 4, !dbg !31, !tbaa !32
  %sub = sub i64 %call.i7, %call.i, !dbg !34
  %call3 = tail call signext i32 (ptr, ...) @printf(ptr noundef nonnull @.str, i64 noundef %sub) #11, !dbg !35
  %0 = load volatile float, ptr @result_sink, align 4, !dbg !36, !tbaa !32
  %cmp4 = fcmp fast oeq float %0, -1.000000e+00, !dbg !37
  %conv = zext i1 %cmp4 to i32, !dbg !37
  ret i32 %conv, !dbg !38

for.body:                                         ; preds = %entry, %for.body
  %result.09 = phi float [ 0.000000e+00, %entry ], [ %add, %for.body ]
  %repeat.08 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %call1 = tail call fast fastcc nofpclass(nan inf) float @selected_kernel() #9, !dbg !39
  %add = fadd fast float %call1, %result.09, !dbg !40
  %inc = add nuw nsw i32 %repeat.08, 1, !dbg !41
  %exitcond.not = icmp eq i32 %inc, 32, !dbg !42
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body, !dbg !26, !llvm.loop !43
}

; Function Attrs: noinline nounwind optnone vscale_range(2,1024)
define internal fastcc void @initialise_arrays() unnamed_addr #1 !dbg !47 {
entry:
  %i = alloca i32, align 4
  %i1 = alloca i32, align 4
  %i25 = alloca i32, align 4
  %cleanup.dest.slot = alloca i32, align 4
  %j = alloca i32, align 4
  call void @llvm.lifetime.start.p0(ptr %i) #10, !dbg !48
  store i32 0, ptr %i, align 4, !dbg !49, !tbaa !10
  br label %for.cond, !dbg !48

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, ptr %i, align 4, !dbg !50, !tbaa !10
  %cmp = icmp slt i32 %0, 8192, !dbg !51
  br i1 %cmp, label %for.body, label %for.cond.cleanup, !dbg !52

for.cond.cleanup:                                 ; preds = %for.cond
  call void @llvm.lifetime.end.p0(ptr %i) #10, !dbg !52
  br label %for.end

for.body:                                         ; preds = %for.cond
  %1 = load i32, ptr %i, align 4, !dbg !53, !tbaa !10
  %and = and i32 %1, 7, !dbg !54
  %conv = uitofp nneg i32 %and to float, !dbg !55
  %2 = call float @llvm.fmuladd.f32(float %conv, float 0x3F847AE140000000, float 1.000000e+00), !dbg !56
  %3 = load i32, ptr %i, align 4, !dbg !57, !tbaa !10
  %idxprom = sext i32 %3 to i64, !dbg !58
  %arrayidx = getelementptr inbounds [8192 x float], ptr @a, i64 0, i64 %idxprom, !dbg !58
  store float %2, ptr %arrayidx, align 4, !dbg !59, !tbaa !32
  br label %for.inc, !dbg !58

for.inc:                                          ; preds = %for.body
  %4 = load i32, ptr %i, align 4, !dbg !60, !tbaa !10
  %inc = add nsw i32 %4, 1, !dbg !60
  store i32 %inc, ptr %i, align 4, !dbg !60, !tbaa !10
  br label %for.cond, !dbg !52, !llvm.loop !61

for.end:                                          ; preds = %for.cond.cleanup
  call void @llvm.lifetime.start.p0(ptr %i1) #10, !dbg !63
  store i32 0, ptr %i1, align 4, !dbg !64, !tbaa !10
  br label %for.cond2, !dbg !63

for.cond2:                                        ; preds = %for.inc22, %for.end
  %5 = load i32, ptr %i1, align 4, !dbg !65, !tbaa !10
  %cmp3 = icmp slt i32 %5, 4096, !dbg !66
  br i1 %cmp3, label %for.body6, label %for.cond.cleanup5, !dbg !67

for.cond.cleanup5:                                ; preds = %for.cond2
  call void @llvm.lifetime.end.p0(ptr %i1) #10, !dbg !67
  br label %for.end24

for.body6:                                        ; preds = %for.cond2
  %6 = load i32, ptr %i1, align 4, !dbg !68, !tbaa !10
  %and7 = and i32 %6, 3, !dbg !69
  %conv8 = uitofp nneg i32 %and7 to float, !dbg !70
  %7 = call float @llvm.fmuladd.f32(float %conv8, float 0x3F947AE140000000, float 2.000000e+00), !dbg !71
  %8 = load i32, ptr %i1, align 4, !dbg !72, !tbaa !10
  %idxprom9 = sext i32 %8 to i64, !dbg !73
  %arrayidx10 = getelementptr inbounds [4096 x float], ptr @b, i64 0, i64 %idxprom9, !dbg !73
  store float %7, ptr %arrayidx10, align 4, !dbg !74, !tbaa !32
  br label %for.inc22, !dbg !75

for.inc22:                                        ; preds = %for.body6
  %9 = load i32, ptr %i1, align 4, !dbg !76, !tbaa !10
  %inc23 = add nsw i32 %9, 1, !dbg !76
  store i32 %inc23, ptr %i1, align 4, !dbg !76, !tbaa !10
  br label %for.cond2, !dbg !67, !llvm.loop !77

for.end24:                                        ; preds = %for.cond.cleanup5
  call void @llvm.lifetime.start.p0(ptr %i25) #10, !dbg !78
  store i32 0, ptr %i25, align 4, !dbg !79, !tbaa !10
  br label %for.cond26, !dbg !78

for.cond26:                                       ; preds = %for.inc62, %for.end24
  %10 = load i32, ptr %i25, align 4, !dbg !80, !tbaa !10
  %cmp27 = icmp slt i32 %10, 64, !dbg !81
  br i1 %cmp27, label %for.body30, label %for.cond.cleanup29, !dbg !82

for.cond.cleanup29:                               ; preds = %for.cond26
  store i32 8, ptr %cleanup.dest.slot, align 4
  call void @llvm.lifetime.end.p0(ptr %i25) #10, !dbg !82
  br label %for.end64

for.body30:                                       ; preds = %for.cond26
  call void @llvm.lifetime.start.p0(ptr %j) #10, !dbg !83
  store i32 0, ptr %j, align 4, !dbg !84, !tbaa !10
  br label %for.cond31, !dbg !83

for.cond31:                                       ; preds = %for.inc59, %for.body30
  %11 = load i32, ptr %j, align 4, !dbg !85, !tbaa !10
  %cmp32 = icmp slt i32 %11, 64, !dbg !86
  br i1 %cmp32, label %for.body35, label %for.cond.cleanup34, !dbg !87

for.cond.cleanup34:                               ; preds = %for.cond31
  store i32 11, ptr %cleanup.dest.slot, align 4
  call void @llvm.lifetime.end.p0(ptr %j) #10, !dbg !87
  br label %for.end61

for.body35:                                       ; preds = %for.cond31
  br label %for.inc59, !dbg !88

for.inc59:                                        ; preds = %for.body35
  %12 = load i32, ptr %j, align 4, !dbg !89, !tbaa !10
  %inc60 = add nsw i32 %12, 1, !dbg !89
  store i32 %inc60, ptr %j, align 4, !dbg !89, !tbaa !10
  br label %for.cond31, !dbg !87, !llvm.loop !90

for.end61:                                        ; preds = %for.cond.cleanup34
  br label %for.inc62, !dbg !91

for.inc62:                                        ; preds = %for.end61
  %13 = load i32, ptr %i25, align 4, !dbg !92, !tbaa !10
  %inc63 = add nsw i32 %13, 1, !dbg !92
  store i32 %inc63, ptr %i25, align 4, !dbg !92, !tbaa !10
  br label %for.cond26, !dbg !82, !llvm.loop !93

for.end64:                                        ; preds = %for.cond.cleanup29
  ret void, !dbg !94
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(ptr captures(none)) #2

; Function Attrs: noinline nounwind vscale_range(2,1024)
define internal fastcc nofpclass(nan inf) float @selected_kernel() unnamed_addr #3 !dbg !95 {
entry:
  tail call void asm sideeffect "", "~{memory}"() #10, !dbg !96, !srcloc !97
  br label %vector.body, !dbg !98

vector.body:                                      ; preds = %vector.body, %entry
  %evl.based.iv = phi i64 [ 0, %entry ], [ %index.evl.next, %vector.body ]
  %avl = phi i64 [ 4096, %entry ], [ %avl.next, %vector.body ]
  %0 = tail call i32 @llvm.experimental.get.vector.length.i64(i64 %avl, i32 16, i1 true)
  %1 = getelementptr inbounds nuw float, ptr @b, i64 %evl.based.iv, !dbg !99
  %vp.op.load = tail call <vscale x 16 x float> @llvm.vp.load.nxv16f32.p0(ptr nonnull align 4 %1, <vscale x 16 x i1> splat (i1 true), i32 %0), !dbg !99, !tbaa !32
  %2 = fadd fast <vscale x 16 x float> %vp.op.load, splat (float 1.000000e+00), !dbg !100
  %3 = getelementptr inbounds nuw float, ptr @a, i64 %evl.based.iv, !dbg !101
  tail call void @llvm.vp.store.nxv16f32.p0(<vscale x 16 x float> %2, ptr nonnull align 4 %3, <vscale x 16 x i1> splat (i1 true), i32 %0), !dbg !102, !tbaa !32
  %4 = zext i32 %0 to i64, !dbg !103
  %index.evl.next = add nuw i64 %evl.based.iv, %4, !dbg !103
  %avl.next = sub nuw i64 %avl, %4
  %5 = icmp eq i64 %avl.next, 0
  br i1 %5, label %for.cond.cleanup, label %vector.body, !dbg !98, !llvm.loop !104

for.cond.cleanup:                                 ; preds = %vector.body
  %6 = load float, ptr getelementptr inbounds nuw (i8, ptr @a, i64 16380), align 4, !dbg !107, !tbaa !32
  ret float %6, !dbg !108
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(ptr captures(none)) #2

declare dso_local signext i32 @printf(ptr noundef, ...) local_unnamed_addr #4

; Function Attrs: mustprogress nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none)
declare float @llvm.fmuladd.f32(float, float, float) #5

declare dso_local i64 @builder_platform_cycle() local_unnamed_addr #4

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.experimental.get.vector.length.i64(i64, i32 immarg, i1 immarg) #6

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: read)
declare <vscale x 16 x float> @llvm.vp.load.nxv16f32.p0(ptr captures(none), <vscale x 16 x i1>, i32) #7

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: write)
declare void @llvm.vp.store.nxv16f32.p0(<vscale x 16 x float>, ptr captures(none), <vscale x 16 x i1>, i32) #8

attributes #0 = { nounwind vscale_range(2,1024) "no-builtins" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic-rv64" "target-features"="+64bit,+a,+b,+c,+d,+f,+i,+m,+v,+zaamo,+zalrsc,+zba,+zbb,+zbc,+zbs,+zca,+zcd,+zicbom,+zicboz,+zicsr,+zifencei,+zmmul,+zve32f,+zve32x,+zve64d,+zve64f,+zve64x,+zvl128b,+zvl32b,+zvl64b,-e,-experimental-p,-experimental-smpmpmt,-experimental-svukte,-experimental-xrivosvisni,-experimental-xrivosvizip,-experimental-xsfmclic,-experimental-xsfsclic,-experimental-zibi,-experimental-zicfilp,-experimental-zicfiss,-experimental-zvbc32e,-experimental-zvfbfa,-experimental-zvfofp8min,-experimental-zvkgs,-experimental-zvqdotq,-h,-q,-relax,-sdext,-sdtrig,-sha,-shcounterenw,-shgatpa,-shlcofideleg,-shtvala,-shvsatpa,-shvstvala,-shvstvecd,-smaia,-smcdeleg,-smcntrpmf,-smcsrind,-smctr,-smdbltrp,-smepmp,-smmpm,-smnpm,-smrnmi,-smstateen,-ssaia,-ssccfg,-ssccptr,-sscofpmf,-sscounterenw,-sscsrind,-ssctr,-ssdbltrp,-ssnpm,-sspm,-ssqosid,-ssstateen,-ssstrict,-sstc,-sstvala,-sstvecd,-ssu64xl,-supm,-svade,-svadu,-svbare,-svinval,-svnapot,-svpbmt,-svvptc,-xandesbfhcvt,-xandesperf,-xandesvbfhcvt,-xandesvdot,-xandesvpackfph,-xandesvsinth,-xandesvsintload,-xcvalu,-xcvbi,-xcvbitmanip,-xcvelw,-xcvmac,-xcvmem,-xcvsimd,-xmipscbop,-xmipscmov,-xmipsexectl,-xmipslsp,-xqccmp,-xqci,-xqcia,-xqciac,-xqcibi,-xqcibm,-xqcicli,-xqcicm,-xqcics,-xqcicsr,-xqciint,-xqciio,-xqcilb,-xqcili,-xqcilia,-xqcilo,-xqcilsm,-xqcisim,-xqcisls,-xqcisync,-xsfcease,-xsfmm128t,-xsfmm16t,-xsfmm32a16f,-xsfmm32a32f,-xsfmm32a8f,-xsfmm32a8i,-xsfmm32t,-xsfmm64a64f,-xsfmm64t,-xsfmmbase,-xsfvcp,-xsfvfbfexp16e,-xsfvfexp16e,-xsfvfexp32e,-xsfvfexpa,-xsfvfexpa64e,-xsfvfnrclipxfqf,-xsfvfwmaccqqq,-xsfvqmaccdod,-xsfvqmaccqoq,-xsifivecdiscarddlone,-xsifivecflushdlone,-xsmtvdot,-xtheadba,-xtheadbb,-xtheadbs,-xtheadcmo,-xtheadcondmov,-xtheadfmemidx,-xtheadmac,-xtheadmemidx,-xtheadmempair,-xtheadsync,-xtheadvdot,-xventanacondops,-xwchc,-za128rs,-za64rs,-zabha,-zacas,-zalasr,-zama16b,-zawrs,-zbkb,-zbkc,-zbkx,-zcb,-zce,-zcf,-zclsd,-zcmop,-zcmp,-zcmt,-zdinx,-zfa,-zfbfmin,-zfh,-zfhmin,-zfinx,-zhinx,-zhinxmin,-zic64b,-zicbop,-ziccamoa,-ziccamoc,-ziccif,-zicclsm,-ziccrse,-zicntr,-zicond,-zihintntl,-zihintpause,-zihpm,-zilsd,-zimop,-zk,-zkn,-zknd,-zkne,-zknh,-zkr,-zks,-zksed,-zksh,-zkt,-ztso,-zvbb,-zvbc,-zvfbfmin,-zvfbfwma,-zvfh,-zvfhmin,-zvkb,-zvkg,-zvkn,-zvknc,-zvkned,-zvkng,-zvknha,-zvknhb,-zvks,-zvksc,-zvksed,-zvksg,-zvksh,-zvkt,-zvl1024b,-zvl16384b,-zvl2048b,-zvl256b,-zvl32768b,-zvl4096b,-zvl512b,-zvl65536b,-zvl8192b" }
attributes #1 = { noinline nounwind optnone vscale_range(2,1024) "no-builtins" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic-rv64" "target-features"="+64bit,+a,+b,+c,+d,+f,+i,+m,+v,+zaamo,+zalrsc,+zba,+zbb,+zbc,+zbs,+zca,+zcd,+zicbom,+zicboz,+zicsr,+zifencei,+zmmul,+zve32f,+zve32x,+zve64d,+zve64f,+zve64x,+zvl128b,+zvl32b,+zvl64b,-e,-experimental-p,-experimental-smpmpmt,-experimental-svukte,-experimental-xrivosvisni,-experimental-xrivosvizip,-experimental-xsfmclic,-experimental-xsfsclic,-experimental-zibi,-experimental-zicfilp,-experimental-zicfiss,-experimental-zvbc32e,-experimental-zvfbfa,-experimental-zvfofp8min,-experimental-zvkgs,-experimental-zvqdotq,-h,-q,-relax,-sdext,-sdtrig,-sha,-shcounterenw,-shgatpa,-shlcofideleg,-shtvala,-shvsatpa,-shvstvala,-shvstvecd,-smaia,-smcdeleg,-smcntrpmf,-smcsrind,-smctr,-smdbltrp,-smepmp,-smmpm,-smnpm,-smrnmi,-smstateen,-ssaia,-ssccfg,-ssccptr,-sscofpmf,-sscounterenw,-sscsrind,-ssctr,-ssdbltrp,-ssnpm,-sspm,-ssqosid,-ssstateen,-ssstrict,-sstc,-sstvala,-sstvecd,-ssu64xl,-supm,-svade,-svadu,-svbare,-svinval,-svnapot,-svpbmt,-svvptc,-xandesbfhcvt,-xandesperf,-xandesvbfhcvt,-xandesvdot,-xandesvpackfph,-xandesvsinth,-xandesvsintload,-xcvalu,-xcvbi,-xcvbitmanip,-xcvelw,-xcvmac,-xcvmem,-xcvsimd,-xmipscbop,-xmipscmov,-xmipsexectl,-xmipslsp,-xqccmp,-xqci,-xqcia,-xqciac,-xqcibi,-xqcibm,-xqcicli,-xqcicm,-xqcics,-xqcicsr,-xqciint,-xqciio,-xqcilb,-xqcili,-xqcilia,-xqcilo,-xqcilsm,-xqcisim,-xqcisls,-xqcisync,-xsfcease,-xsfmm128t,-xsfmm16t,-xsfmm32a16f,-xsfmm32a32f,-xsfmm32a8f,-xsfmm32a8i,-xsfmm32t,-xsfmm64a64f,-xsfmm64t,-xsfmmbase,-xsfvcp,-xsfvfbfexp16e,-xsfvfexp16e,-xsfvfexp32e,-xsfvfexpa,-xsfvfexpa64e,-xsfvfnrclipxfqf,-xsfvfwmaccqqq,-xsfvqmaccdod,-xsfvqmaccqoq,-xsifivecdiscarddlone,-xsifivecflushdlone,-xsmtvdot,-xtheadba,-xtheadbb,-xtheadbs,-xtheadcmo,-xtheadcondmov,-xtheadfmemidx,-xtheadmac,-xtheadmemidx,-xtheadmempair,-xtheadsync,-xtheadvdot,-xventanacondops,-xwchc,-za128rs,-za64rs,-zabha,-zacas,-zalasr,-zama16b,-zawrs,-zbkb,-zbkc,-zbkx,-zcb,-zce,-zcf,-zclsd,-zcmop,-zcmp,-zcmt,-zdinx,-zfa,-zfbfmin,-zfh,-zfhmin,-zfinx,-zhinx,-zhinxmin,-zic64b,-zicbop,-ziccamoa,-ziccamoc,-ziccif,-zicclsm,-ziccrse,-zicntr,-zicond,-zihintntl,-zihintpause,-zihpm,-zilsd,-zimop,-zk,-zkn,-zknd,-zkne,-zknh,-zkr,-zks,-zksed,-zksh,-zkt,-ztso,-zvbb,-zvbc,-zvfbfmin,-zvfbfwma,-zvfh,-zvfhmin,-zvkb,-zvkg,-zvkn,-zvknc,-zvkned,-zvkng,-zvknha,-zvknhb,-zvks,-zvksc,-zvksed,-zvksg,-zvksh,-zvkt,-zvl1024b,-zvl16384b,-zvl2048b,-zvl256b,-zvl32768b,-zvl4096b,-zvl512b,-zvl65536b,-zvl8192b" }
attributes #2 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #3 = { noinline nounwind vscale_range(2,1024) "no-builtins" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic-rv64" "target-features"="+64bit,+a,+b,+c,+d,+f,+i,+m,+v,+zaamo,+zalrsc,+zba,+zbb,+zbc,+zbs,+zca,+zcd,+zicbom,+zicboz,+zicsr,+zifencei,+zmmul,+zve32f,+zve32x,+zve64d,+zve64f,+zve64x,+zvl128b,+zvl32b,+zvl64b,-e,-experimental-p,-experimental-smpmpmt,-experimental-svukte,-experimental-xrivosvisni,-experimental-xrivosvizip,-experimental-xsfmclic,-experimental-xsfsclic,-experimental-zibi,-experimental-zicfilp,-experimental-zicfiss,-experimental-zvbc32e,-experimental-zvfbfa,-experimental-zvfofp8min,-experimental-zvkgs,-experimental-zvqdotq,-h,-q,-relax,-sdext,-sdtrig,-sha,-shcounterenw,-shgatpa,-shlcofideleg,-shtvala,-shvsatpa,-shvstvala,-shvstvecd,-smaia,-smcdeleg,-smcntrpmf,-smcsrind,-smctr,-smdbltrp,-smepmp,-smmpm,-smnpm,-smrnmi,-smstateen,-ssaia,-ssccfg,-ssccptr,-sscofpmf,-sscounterenw,-sscsrind,-ssctr,-ssdbltrp,-ssnpm,-sspm,-ssqosid,-ssstateen,-ssstrict,-sstc,-sstvala,-sstvecd,-ssu64xl,-supm,-svade,-svadu,-svbare,-svinval,-svnapot,-svpbmt,-svvptc,-xandesbfhcvt,-xandesperf,-xandesvbfhcvt,-xandesvdot,-xandesvpackfph,-xandesvsinth,-xandesvsintload,-xcvalu,-xcvbi,-xcvbitmanip,-xcvelw,-xcvmac,-xcvmem,-xcvsimd,-xmipscbop,-xmipscmov,-xmipsexectl,-xmipslsp,-xqccmp,-xqci,-xqcia,-xqciac,-xqcibi,-xqcibm,-xqcicli,-xqcicm,-xqcics,-xqcicsr,-xqciint,-xqciio,-xqcilb,-xqcili,-xqcilia,-xqcilo,-xqcilsm,-xqcisim,-xqcisls,-xqcisync,-xsfcease,-xsfmm128t,-xsfmm16t,-xsfmm32a16f,-xsfmm32a32f,-xsfmm32a8f,-xsfmm32a8i,-xsfmm32t,-xsfmm64a64f,-xsfmm64t,-xsfmmbase,-xsfvcp,-xsfvfbfexp16e,-xsfvfexp16e,-xsfvfexp32e,-xsfvfexpa,-xsfvfexpa64e,-xsfvfnrclipxfqf,-xsfvfwmaccqqq,-xsfvqmaccdod,-xsfvqmaccqoq,-xsifivecdiscarddlone,-xsifivecflushdlone,-xsmtvdot,-xtheadba,-xtheadbb,-xtheadbs,-xtheadcmo,-xtheadcondmov,-xtheadfmemidx,-xtheadmac,-xtheadmemidx,-xtheadmempair,-xtheadsync,-xtheadvdot,-xventanacondops,-xwchc,-za128rs,-za64rs,-zabha,-zacas,-zalasr,-zama16b,-zawrs,-zbkb,-zbkc,-zbkx,-zcb,-zce,-zcf,-zclsd,-zcmop,-zcmp,-zcmt,-zdinx,-zfa,-zfbfmin,-zfh,-zfhmin,-zfinx,-zhinx,-zhinxmin,-zic64b,-zicbop,-ziccamoa,-ziccamoc,-ziccif,-zicclsm,-ziccrse,-zicntr,-zicond,-zihintntl,-zihintpause,-zihpm,-zilsd,-zimop,-zk,-zkn,-zknd,-zkne,-zknh,-zkr,-zks,-zksed,-zksh,-zkt,-ztso,-zvbb,-zvbc,-zvfbfmin,-zvfbfwma,-zvfh,-zvfhmin,-zvkb,-zvkg,-zvkn,-zvknc,-zvkned,-zvkng,-zvknha,-zvknhb,-zvks,-zvksc,-zvksed,-zvksg,-zvksh,-zvkt,-zvl1024b,-zvl16384b,-zvl2048b,-zvl256b,-zvl32768b,-zvl4096b,-zvl512b,-zvl65536b,-zvl8192b" }
attributes #4 = { "no-builtins" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic-rv64" "target-features"="+64bit,+a,+b,+c,+d,+f,+i,+m,+v,+zaamo,+zalrsc,+zba,+zbb,+zbc,+zbs,+zca,+zcd,+zicbom,+zicboz,+zicsr,+zifencei,+zmmul,+zve32f,+zve32x,+zve64d,+zve64f,+zve64x,+zvl128b,+zvl32b,+zvl64b,-e,-experimental-p,-experimental-smpmpmt,-experimental-svukte,-experimental-xrivosvisni,-experimental-xrivosvizip,-experimental-xsfmclic,-experimental-xsfsclic,-experimental-zibi,-experimental-zicfilp,-experimental-zicfiss,-experimental-zvbc32e,-experimental-zvfbfa,-experimental-zvfofp8min,-experimental-zvkgs,-experimental-zvqdotq,-h,-q,-relax,-sdext,-sdtrig,-sha,-shcounterenw,-shgatpa,-shlcofideleg,-shtvala,-shvsatpa,-shvstvala,-shvstvecd,-smaia,-smcdeleg,-smcntrpmf,-smcsrind,-smctr,-smdbltrp,-smepmp,-smmpm,-smnpm,-smrnmi,-smstateen,-ssaia,-ssccfg,-ssccptr,-sscofpmf,-sscounterenw,-sscsrind,-ssctr,-ssdbltrp,-ssnpm,-sspm,-ssqosid,-ssstateen,-ssstrict,-sstc,-sstvala,-sstvecd,-ssu64xl,-supm,-svade,-svadu,-svbare,-svinval,-svnapot,-svpbmt,-svvptc,-xandesbfhcvt,-xandesperf,-xandesvbfhcvt,-xandesvdot,-xandesvpackfph,-xandesvsinth,-xandesvsintload,-xcvalu,-xcvbi,-xcvbitmanip,-xcvelw,-xcvmac,-xcvmem,-xcvsimd,-xmipscbop,-xmipscmov,-xmipsexectl,-xmipslsp,-xqccmp,-xqci,-xqcia,-xqciac,-xqcibi,-xqcibm,-xqcicli,-xqcicm,-xqcics,-xqcicsr,-xqciint,-xqciio,-xqcilb,-xqcili,-xqcilia,-xqcilo,-xqcilsm,-xqcisim,-xqcisls,-xqcisync,-xsfcease,-xsfmm128t,-xsfmm16t,-xsfmm32a16f,-xsfmm32a32f,-xsfmm32a8f,-xsfmm32a8i,-xsfmm32t,-xsfmm64a64f,-xsfmm64t,-xsfmmbase,-xsfvcp,-xsfvfbfexp16e,-xsfvfexp16e,-xsfvfexp32e,-xsfvfexpa,-xsfvfexpa64e,-xsfvfnrclipxfqf,-xsfvfwmaccqqq,-xsfvqmaccdod,-xsfvqmaccqoq,-xsifivecdiscarddlone,-xsifivecflushdlone,-xsmtvdot,-xtheadba,-xtheadbb,-xtheadbs,-xtheadcmo,-xtheadcondmov,-xtheadfmemidx,-xtheadmac,-xtheadmemidx,-xtheadmempair,-xtheadsync,-xtheadvdot,-xventanacondops,-xwchc,-za128rs,-za64rs,-zabha,-zacas,-zalasr,-zama16b,-zawrs,-zbkb,-zbkc,-zbkx,-zcb,-zce,-zcf,-zclsd,-zcmop,-zcmp,-zcmt,-zdinx,-zfa,-zfbfmin,-zfh,-zfhmin,-zfinx,-zhinx,-zhinxmin,-zic64b,-zicbop,-ziccamoa,-ziccamoc,-ziccif,-zicclsm,-ziccrse,-zicntr,-zicond,-zihintntl,-zihintpause,-zihpm,-zilsd,-zimop,-zk,-zkn,-zknd,-zkne,-zknh,-zkr,-zks,-zksed,-zksh,-zkt,-ztso,-zvbb,-zvbc,-zvfbfmin,-zvfbfwma,-zvfh,-zvfhmin,-zvkb,-zvkg,-zvkn,-zvknc,-zvkned,-zvkng,-zvknha,-zvknhb,-zvks,-zvksc,-zvksed,-zvksg,-zvksh,-zvkt,-zvl1024b,-zvl16384b,-zvl2048b,-zvl256b,-zvl32768b,-zvl4096b,-zvl512b,-zvl65536b,-zvl8192b" }
attributes #5 = { mustprogress nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none) }
attributes #6 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #7 = { nocallback nofree nosync nounwind willreturn memory(argmem: read) }
attributes #8 = { nocallback nofree nosync nounwind willreturn memory(argmem: write) }
attributes #9 = { nobuiltin "no-builtins" }
attributes #10 = { nounwind }
attributes #11 = { nobuiltin nounwind "no-builtins" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !7, !8}
!llvm.ident = !{!9}
!llvm.errno.tbaa = !{!10}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang version 22.1.8 (https://github.com/llvm/llvm-project.git ca7933e47d3a3451d81e72ac174dcb5aa28b59d1)", isOptimized: true, runtimeVersion: 0, emissionKind: NoDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/Users/cheolwanpark/Documents/project/llvm-project/experiments/tp-lcd/inputs/tsvc_kernels.c", directory: "/Users/cheolwanpark/Documents/project/llvm-project")
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
!14 = distinct !DISubprogram(name: "builder_main", scope: !15, file: !15, line: 117, type: !16, scopeLine: 117, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!15 = !DIFile(filename: "experiments/tp-lcd/inputs/tsvc_kernels.c", directory: "/Users/cheolwanpark/Documents/project/llvm-project")
!16 = !DISubroutineType(types: !17)
!17 = !{}
!18 = !DILocation(line: 120, column: 5, scope: !14)
!19 = !DILocation(line: 56, column: 5, scope: !20, inlinedAt: !21)
!20 = distinct !DISubprogram(name: "read_cycle", scope: !15, file: !15, line: 55, type: !16, scopeLine: 55, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!21 = distinct !DILocation(line: 123, column: 22, scope: !14)
!22 = !{i64 2078}
!23 = !DILocation(line: 57, column: 22, scope: !20, inlinedAt: !21)
!24 = !DILocation(line: 58, column: 5, scope: !20, inlinedAt: !21)
!25 = !{i64 2176}
!26 = !DILocation(line: 124, column: 5, scope: !14)
!27 = !DILocation(line: 56, column: 5, scope: !20, inlinedAt: !28)
!28 = distinct !DILocation(line: 126, column: 20, scope: !14)
!29 = !DILocation(line: 57, column: 22, scope: !20, inlinedAt: !28)
!30 = !DILocation(line: 58, column: 5, scope: !20, inlinedAt: !28)
!31 = !DILocation(line: 128, column: 17, scope: !14)
!32 = !{!33, !33, i64 0}
!33 = !{!"float", !12, i64 0}
!34 = !DILocation(line: 129, column: 57, scope: !14)
!35 = !DILocation(line: 129, column: 5, scope: !14)
!36 = !DILocation(line: 130, column: 12, scope: !14)
!37 = !DILocation(line: 130, column: 24, scope: !14)
!38 = !DILocation(line: 130, column: 5, scope: !14)
!39 = !DILocation(line: 125, column: 19, scope: !14)
!40 = !DILocation(line: 125, column: 16, scope: !14)
!41 = !DILocation(line: 124, column: 44, scope: !14)
!42 = !DILocation(line: 124, column: 33, scope: !14)
!43 = distinct !{!43, !26, !44, !45, !46}
!44 = !DILocation(line: 125, column: 35, scope: !14)
!45 = !{!"llvm.loop.mustprogress"}
!46 = !{!"llvm.loop.unroll.disable"}
!47 = distinct !DISubprogram(name: "initialise_arrays", scope: !15, file: !15, line: 36, type: !16, scopeLine: 36, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!48 = !DILocation(line: 37, column: 10, scope: !47)
!49 = !DILocation(line: 37, column: 14, scope: !47)
!50 = !DILocation(line: 37, column: 21, scope: !47)
!51 = !DILocation(line: 37, column: 23, scope: !47)
!52 = !DILocation(line: 37, column: 5, scope: !47)
!53 = !DILocation(line: 38, column: 31, scope: !47)
!54 = !DILocation(line: 38, column: 33, scope: !47)
!55 = !DILocation(line: 38, column: 23, scope: !47)
!56 = !DILocation(line: 38, column: 21, scope: !47)
!57 = !DILocation(line: 38, column: 11, scope: !47)
!58 = !DILocation(line: 38, column: 9, scope: !47)
!59 = !DILocation(line: 38, column: 14, scope: !47)
!60 = !DILocation(line: 37, column: 46, scope: !47)
!61 = distinct !{!61, !52, !62, !45, !46}
!62 = !DILocation(line: 38, column: 40, scope: !47)
!63 = !DILocation(line: 39, column: 10, scope: !47)
!64 = !DILocation(line: 39, column: 14, scope: !47)
!65 = !DILocation(line: 39, column: 21, scope: !47)
!66 = !DILocation(line: 39, column: 23, scope: !47)
!67 = !DILocation(line: 39, column: 5, scope: !47)
!68 = !DILocation(line: 40, column: 31, scope: !47)
!69 = !DILocation(line: 40, column: 33, scope: !47)
!70 = !DILocation(line: 40, column: 23, scope: !47)
!71 = !DILocation(line: 40, column: 21, scope: !47)
!72 = !DILocation(line: 40, column: 11, scope: !47)
!73 = !DILocation(line: 40, column: 9, scope: !47)
!74 = !DILocation(line: 40, column: 14, scope: !47)
!75 = !DILocation(line: 44, column: 5, scope: !47)
!76 = !DILocation(line: 39, column: 36, scope: !47)
!77 = distinct !{!77, !67, !75, !45, !46}
!78 = !DILocation(line: 45, column: 10, scope: !47)
!79 = !DILocation(line: 45, column: 14, scope: !47)
!80 = !DILocation(line: 45, column: 21, scope: !47)
!81 = !DILocation(line: 45, column: 23, scope: !47)
!82 = !DILocation(line: 45, column: 5, scope: !47)
!83 = !DILocation(line: 46, column: 14, scope: !47)
!84 = !DILocation(line: 46, column: 18, scope: !47)
!85 = !DILocation(line: 46, column: 25, scope: !47)
!86 = !DILocation(line: 46, column: 27, scope: !47)
!87 = !DILocation(line: 46, column: 9, scope: !47)
!88 = !DILocation(line: 51, column: 9, scope: !47)
!89 = !DILocation(line: 46, column: 37, scope: !47)
!90 = distinct !{!90, !87, !88, !45, !46}
!91 = !DILocation(line: 52, column: 5, scope: !47)
!92 = !DILocation(line: 45, column: 33, scope: !47)
!93 = distinct !{!93, !82, !91, !45, !46}
!94 = !DILocation(line: 53, column: 1, scope: !47)
!95 = distinct !DISubprogram(name: "selected_kernel", scope: !15, file: !15, line: 65, type: !16, scopeLine: 65, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!96 = !DILocation(line: 66, column: 5, scope: !95)
!97 = !{i64 2366}
!98 = !DILocation(line: 67, column: 5, scope: !95)
!99 = !DILocation(line: 68, column: 16, scope: !95)
!100 = !DILocation(line: 68, column: 21, scope: !95)
!101 = !DILocation(line: 68, column: 9, scope: !95)
!102 = !DILocation(line: 68, column: 14, scope: !95)
!103 = !DILocation(line: 67, column: 36, scope: !95)
!104 = distinct !{!104, !45, !46, !105, !106}
!105 = !{!"llvm.loop.isvectorized", i32 1}
!106 = !{!"llvm.loop.unroll.runtime.disable"}
!107 = !DILocation(line: 69, column: 12, scope: !95)
!108 = !DILocation(line: 69, column: 5, scope: !95)
