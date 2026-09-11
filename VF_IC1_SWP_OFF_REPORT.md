# LLVM RVV vectorizer cost model 검토: VF sweep, IC=1 / SWP=off

이 문서는 `microbench/suite-vf-ic-sweep`의 실제 커널 소스, 생성 어셈블리와 측정 결과를 바탕으로 작성했다. 별도의 benchmark 코드 배포를 가정하지 않는다. 아래에 **측정에 사용된 두 C 파일 원문, reduction postprocessor 원문, bare-metal runtime 원문, 독립 빌드·실행 스크립트**를 포함한다. 파일 추출 후 원래 code-lab 저장소 없이 benchmark를 빌드할 수 있다. LLVM, C/C++ 라이브러리와 RTL simulator는 외부 도구로 필요하다.

**핵심 추천은 7개다: `s000`, `s125`, `s311`, `s1111`, `s4112`, Jacobi, GESUMMV.** Jacobi의 VF=16 퇴보, gather의 index-width legalization 경계, GESUMMV의 VF=4→8 퇴보를 우선 확인하고 나머지를 대표 대조군으로 사용한다. 추가 검증용으로 BiCG와 GEMVER도 보존한다.

## 1. 측정 범위와 재현 범위

| 항목 | 기록된 조건 |
|---|---|
| Target | XiangShan, `-mcpu=xiangshan-kunminghu` |
| RTL 로그 revision | `75775862a6`, dirty=0; 36개 로그 모두 동일 |
| LLVM repository | `https://github.com/cheolwanpark/llvm-project.git` |
| LLVM revision | `89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb` |
| Data type | f32; gather index 원본은 signed `int` |
| Scalable VF | 2 / 4 / 8 / 16 |
| f32 LMUL | m1 / m2 / m4 / m8 |
| VLEN | XiangShan 설정 128 bits; 컴파일에서는 minimum=128만 지정 |
| IC / SWP | 1 / off |
| Trip count | 호출당 4096 iterations |
| ROI | `selected_kernel` 32회 호출; 총 131,072 useful iterations |
| 주요 최적화 옵션 | `-O2 -ffast-math -fno-unroll-loops` |
| 측정값 | guest `rdcycle` 차이, stdout `MB_ROI=<hex>` |
| Run seed / limit | 1 / 5,000,000 simulator cycles |
| 측정 반복의 의미 | 설정당 저장된 run 1개; 32회는 독립 표본이 아닌 ROI 내부 반복 |
| Compiler spill/reload | IC=1/SWP=off의 36개 모두 0/0; 수동 workaround의 stack save/restore는 별도 |

`raw_roi_cycles`만 사용한다. `KC`, simulator 전체 `cycleCnt`, host execution time과 혼동하지 않는다. 초기화와 최종 출력 소비는 ROI 밖이다. ROI에는 함수 호출·반복 제어, TSVC의 반환값 누적, reduction epilogue 및 해당 workaround 비용이 포함된다. 다른 커널끼리 FLOP 수가 같지는 않다. `cycles_per_element = raw_roi_cycles / 131072`는 useful loop iteration당 비용이다.

LLVM RVV의 `vscale=VLEN/64` 정의상 VLEN=128이면 vscale=2이다. 따라서 표의 VF=2/4/8/16은 실제 최대 4/8/16/32 f32 elements를 처리한다. VF=2를 scalar baseline으로 해석하면 안 된다. [LLVM RVV type mapping](https://llvm.org/docs/RISCV/RISCVVectorExtension.html#mapping-to-llvm-ir-types)

**재현 가능성의 한계:** 커널·runtime·postprocessor 원문과 컴파일 옵션은 이 문서에 포함했다. 그러나 원래 builder/runner image digest, 라이브러리 전체 package lock, RTL 생성 옵션과 simulator `init` 입력의 전체 archive는 측정 summary에 없다. 아래 절차는 동일한 benchmark workload와 compilation policy를 재구성하는 절차이며, 숫자가 bit-for-bit 동일하게 재측정된다고 보장하지 않는다. cycle 값의 엄밀한 재현에는 원래 RTL binary·reference model·실행 입력 및 toolchain 환경이 필요하다. QEMU의 cycle은 XiangShan 측정값의 대체물이 아니다.

## 2. Measured results

| Kernel | VF=2 / m1 | VF=4 / m2 | VF=8 / m4 | VF=16 / m8 | Best VF | Speedup: VF=2 cycles / best cycles |
|---|---:|---:|---:|---:|---:|---:|
| s000 | 471,934 | 181,039 | 174,586 | **159,331** | 16 | 2.962× |
| s1111 | 1,368,994 | 1,377,183 | 1,160,981 | **1,123,047** | 16 | 1.219× |
| s125 | 684,788 | 388,692 | 376,793 | **354,097** | 16 | 1.934× |
| s311 | 198,467 | 100,355 | 101,871 | **78,423** | 16 | 2.531× |
| s4112 | 1,254,522 | 746,233 | **706,672** | 718,042 | 8 | 1.775× |
| jacobi-2d-imper-l00 | 1,200,885 | 729,082 | **529,345** | 771,458 | 8 | 2.269× |
| gesummv-l00 | 507,862 | 369,351 | 392,315 | **337,888** | 16 | 1.503× |
| bicg-l01 | 606,973 | 350,537 | 341,400 | **328,917** | 16 | 1.845× |
| gemver-l00 | 650,222 | 363,914 | 335,283 | **322,317** | 16 | 2.017× |

각 row의 최솟값은 **굵게** 표시했다. 기준 speedup은 같은 커널의 VF=2 대비 최적 VF의 speedup이다. 수치는 `summary.csv`, 개별 `result.json`, stdout의 `MB_ROI`를 대조했다. 원래 run ID를 포함하는 machine-readable `measured.csv`도 아래에 내장했다.

## 3. Cost model 업데이트에서 반드시 확인할 사례

### 3.1 공통 경계: VF=4→8에서 tail 처리와 명령 형태가 바뀐다

대부분의 커널에서 VF=2/4는 루프 밖 VLMAX 설정과 whole-register load/store를 사용하고, VF=8/16은 루프 안에서 remaining count로 VL을 설정하며 element load/store를 사용한다. 어셈블리의 loop iteration당 `vsetvli` 수는 다음과 같다.

| Kernel | VF=2 | VF=4 | VF=8 | VF=16 |
|---|---:|---:|---:|---:|
| s000 | 0 | 0 | 1 | 1 |
| s311 | 0 | 0 | 2 | 2 |
| s4112 | 0 | 0 | 1 | 4 |
| GESUMMV | 0 | 0 | 4 | 4 |
| Jacobi | 1 | 1 | 1 | 1 |

이 변화에는 memory instruction selection, 주소 계산, `ta/tu` 전환과 tail folding이 함께 들어간다. 실제 VLEN=128에서 N=4096은 모든 VF의 처리 폭으로 나누어떨어져도, compiler가 VLEN 상한을 정확히 아는 것은 아니다. 해당 LLVM의 LoopVectorize는 최대 가능한 scalable VF까지 고려하여 tail folding 생략 여부를 판단한다. 따라서 이 데이터를 LMUL throughput만의 실험으로 fitting하면 안 된다.

**후속 확인:** 현재 옵션을 유지한 결과에 더해 `-mllvm -riscv-v-vector-bits-max=128`을 추가한 별도 sweep을 비교한다. 이는 새 실험 조건이며 이 문서의 measured table을 대체하지 않는다. source가 constant trip count이므로 generic-VLEN과 exact-VLEN의 코드 형태 차이를 분리하기 좋다.

### 3.2 Jacobi: spill 없는 VF=16의 큰 퇴보

VF=8→16에서 **529,345 → 771,458 cycles, 45.7% 악화**한다. VF=16은 VF=4보다도 느리다. 이 suite에서 “가장 큰 VF가 최선”이라는 선택 규칙에 대한 가장 강한 반례다.

원본 계산은 `0.2f * (a[j] + a[j-1] + a[j+1] + c[j] + b[j])`이다. 생성된 루프는 4 vector loads, `vslidedown.vx`, `vslideup.vi`, 4 FP adds, scalar-vector multiply와 store를 사용한다. 이전 iteration의 vector가 다음 iteration의 slide 입력으로 이어진다. 단순한 5-load map과 달리 shuffle과 loop-carried dependency가 포함된다. `a+8`, `b+4`, `c+4`의 어긋난 주소 접근도 확인 대상이다.

현재 LLVM의 `getShuffleCost(SK_Splice)`는 slide 두 개로 계산하고, `getVSlideVXCost`/`getVSlideVICost`는 `getLMULCost`를 반환한다. **slide의 LMUL별 처리 비용, dependency latency, 메모리 접근과의 상호작용**을 우선 검증한다. cycle만으로 slide latency나 실행 자원 병목 하나를 원인으로 확정할 수는 없다.

### 3.3 s4112: f32 data m8 + i64 index의 legalization 경계

VF=8→16에서 **706,672 → 718,042 cycles, 1.6% 악화**한다. 차이는 작지만 생성 코드의 구조적 변화는 명확하다.

- VF≤8: `vwmulsu.vx`로 signed i32 index를 64-bit byte offset으로 widening하고 `vluxei64.v` 한 개로 load한다.
- VF=16: widening과 gather가 각각 두 개로 분할된다. 루프의 `vsetvli`도 1개에서 4개로 늘고, `minu/maxu/sub`로 부분 VL을 계산한다.

f32 data가 m8이면 같은 원소 수의 i64 index에 필요한 EMUL은 16으로 한 register group에 들어가지 않는다. **data vector만의 legalization 비용으로는 놓치기 쉬운 경계**다. 이 연산은 memory gather이며, vector-register shuffle인 `vrgather`와 구분해야 한다.

현재 `getGatherScatterOpCost`는 예상 element 수에 비례하는 비용을 반환한다. address/GEP/cast 비용에 이미 반영된 항목과 중복되지 않도록, index 폭·분할·추가 VL 제어가 전체 plan 비용에 포함되는지 확인한다. 초기 index는 `(i*5)&4095`인 규칙적인 permutation이다. 임의의 sparse/random index 전체를 대표하지는 않는다.

### 3.4 s311 / GESUMMV / BiCG: 누적 연산과 종료 reduction을 구분한다

`s311`은 단일 sum accumulator, GESUMMV는 `x`를 공유하는 두 개의 독립적인 dot-product accumulator다. 두 커널 모두 hot loop에서는 `vfadd` 또는 `vfmacc`로 누적하고, horizontal `vfredusum`은 loop exit에서 수행한다.

- s311: VF=4→8에서 100,355 → 101,871 cycles, 1.5% 악화. VF=16에서는 78,423으로 개선.
- GESUMMV: VF=4→8에서 369,351 → 392,315 cycles, 6.2% 악화. VF=16에서는 337,888로 개선.
- GESUMMV의 VF≥8에서는 `ta↔tu` 전환을 포함한 `vsetvli` 4개가 매 iteration 실행된다.

`getArithmeticReductionCost`만 조정하지 말고 **accumulator dependency, 독립 accumulator 수, 일반 FP 연산 비용, tail 정책과 loop-exit 비용**을 함께 확인한다. 종료 reduction 비용을 매 iteration 비용으로 중복 계산해서는 안 된다.

| Kernel | 호출당 workaround 적용 수: VF=2/4/8/16 | 결과 처리 |
|---|---|---|
| s311 | 1 / 1 / 1 / 1 | terminal scalar extraction |
| GESUMMV | 2 / 2 / 2 / 2 | 두 scalar extractions, VL/vtype와 GPR 보존 |
| BiCG | 0 / 0 / 0 / 0 | reduction 결과를 VL=1 vector store로 저장 |

XiangShan workaround는 e32 extraction을 e64 `vmv.x.s` + `fmv.w.x`로 바꾼다. 보존 모드는 stack save/restore와 vector-state 복구도 한다. 비용은 ROI에 포함된다. 적용 횟수가 같다고 실제 overhead cycle까지 동일하다고 단정할 수 없으며, 이 비용을 보편적인 reduction 비용에 그대로 흡수하지 않는다. state 복구에는 immediate-form `vsetvli`를 사용한다. 과거 register-form `vsetvl`은 기록된 RTL에서 stall을 일으켰다. 내장한 postprocessor를 생략하거나 임의로 단순화하면 측정 조건이 달라진다.

### 3.5 s1111: strided store의 제한적인 VF scaling

VF=2→4에서 1,368,994 → 1,377,183 cycles로 개선이 없으며, VF=2→16 전체 speedup도 **1.22×**다. s000의 2.96×, s125의 1.93×와 대비된다.

루프는 3 loads, 재결합된 FP arithmetic과 8-byte stride `vsse32.v`를 사용한다. `-ffast-math`에 의해 source expression이 재결합되므로 소스상의 연산 개수로 비용을 세면 안 된다. unit-stride memory와 strided store의 scaling을 구별하고, 현재 `getStridedMemoryOpCost`의 예상 element 수에 비례하는 비용이 전체 VF 순위에 어떻게 작용하는지 확인한다. 연산 dependency와 store 비용이 섞여 있으므로 순수 strided-store instruction latency 측정으로 취급하지 않는다.

## 4. Representative subset와 업데이트 검증

| 구분 | Kernel | 대표하는 성질 |
|---|---|---|
| 핵심 | s000 | 1 load + scalar-vector add + 1 store, 작은 VF의 overhead와 이후 포화 |
| 핵심 | s125 | 3 loads + 1 FMA + 1 store, 연속 접근 다중 stream. 실제 loop는 flattened 1D |
| 핵심 | s311 | 단일 accumulator, 종료 FP reduction |
| 핵심 | s1111 | stride-2 store + FP dependency |
| 핵심 | s4112 | indexed memory, widening 및 m8 legalization |
| 핵심 | Jacobi | stencil에서 생성되는 splice/shuffle recurrence, spill 없는 m8 퇴보 |
| 핵심 | GESUMMV | shared input을 가진 두 reductions, vtype 전환 |
| 추가 검증 | BiCG | map + reduction의 결합, 이 조건에서는 extraction workaround 없음 |
| 추가 검증 | GEMVER | 연속 접근 read-modify-write와 두 scalar-vector FMAs |

이 suite의 application kernel들은 원래 프로그램의 선택된 inner computation만 실행하는 f32 adaptation이다. 전체 matrix workload가 아니다. BiCG와 GEMVER는 32회 호출 사이 output update를 유지한다. RoPE는 원래 source의 APP_CASE=3으로 보존했지만 강제 VF/IC vectorization 검증에 실패하여 측정 대상에서 제외했다. 이 문서의 builder도 그 case를 실행하지 않는다.

권장 검증 순서는 다음과 같다.

1. IC=1/SWP=off에서 기존·수정 cost model의 자동 선택 VF, 후보별 estimated cost 및 code shape를 기록한다. 현재 표는 forced sweep이므로 기존 자동 선택이 틀렸다는 사실까지 증명하지 않는다.
2. `selection regret = cycles(selected VF) / min(cycles(VF=2,4,8,16))`를 비교한다. 최대 VF 고정은 Jacobi에서 약 1.457의 regret을 만든다.
3. generic-VLEN과 exact-VLEN, loop-body와 exit/workaround 비용을 분리하는 대조 실험으로 설명을 검증한다.
4. VF별 순위뿐 아니라 VF=4→8, VF=8→16 경계의 비용 차이를 확인한다. 모든 벡터 비용을 하나의 LMUL 계수로 조정하지 않는다.
5. 작은 차이는 재측정한다. 특히 s4112의 1.6%, s311의 1.5%를 곧바로 강한 선택 규칙으로 만들지 않는다. PMU/trace가 없으므로 port pressure, early split, 실행 window 포화는 아직 원인 가설이다.

현재 데이터는 f32, N=4096, 정렬된 정적 배열, 한 target에 한정된다. scalar 대비 vectorization 여부, short trip count, irregular tail, alias checks, 다른 element width, 실제 spill 경계와 SWP/IC 효과는 별도 coverage가 필요하다.

## 5. 이 Markdown에서 파일 추출

아래 명령의 첫 번째 인수에 이 Markdown 파일 경로를 넣는다. `file:` marker가 붙은 코드 블록만 추출하며 SHA-256을 확인한다. 기존 디렉터리를 덮어쓰지 않는다.

```bash
python3 - VF_IC1_SWP_OFF_REPORT.md vf-repro <<'PY'
import hashlib
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text()
destination = Path(sys.argv[2]).resolve()
pattern = r'<!-- file: ([^\n]+) sha256: ([0-9a-f]{64}) -->\n```[^\n]*\n(.*?)\n```'
blocks = re.findall(pattern, text, re.S)
if not blocks:
    raise SystemExit('No embedded files found')
destination.mkdir(parents=True, exist_ok=False)
seen = set()
for name, expected, body in blocks:
    path = (destination / name).resolve()
    if not path.is_relative_to(destination) or name in seen:
        raise SystemExit('Invalid or duplicate filename: ' + name)
    seen.add(name)
    content = (body + '\n').encode()
    if hashlib.sha256(content).hexdigest() != expected:
        raise SystemExit('Hash mismatch: ' + name)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
print('Extracted', len(blocks), 'files to', destination)
PY
```

## 6. 빌드와 실행

### Toolchain과 36-cell build

Python 3.10 이상을 사용한다. 내장 `Dockerfile`은 기록된 LLVM SHA를 source-build하고 Ubuntu 24.04의 RISC-V GCC/picolibc와 libc++ 22 headers를 설치한다. LLVM 빌드는 시간·메모리·네트워크가 필요하다. 이 Dockerfile과 `build.py`/`run.py`는 문서용 재현 도구로 새로 작성했으며, 원래 측정에 사용된 파일이라고 주장하지 않는다. kernel/runtime/postprocessor 파일은 원문이다.

```bash
cd vf-repro
docker build --build-arg LLVM_JOBS=2 -t vf-cost-repro .
docker run --rm -v "$PWD:/work" -w /work vf-cost-repro python3 build.py
```

원래 환경의 `/opt/llvm-main/bin/clang`과 picolibc가 이미 있으면 추출 디렉터리에서 `python3 build.py`를 실행해도 된다. compiler 경로는 `--cc`/`--cxx`, picolibc root는 `--picolibc`로 바꿀 수 있다. GCC runtime library는 `riscv64-unknown-elf-gcc -march=rv64iafd -mabi=lp64d -print-libgcc-file-name`으로 찾는다. 전체 ELF build에는 bare-metal용 libc++ header configuration도 필요하므로 일반 desktop clang++ header 설정을 그대로 사용하지 않는다.

```bash
# 외부 명령을 실행하지 않고 compile/assemble/link command 확인
python3 build.py --dry-run

# 해당 toolchain이 설치된 환경에서 assembly + remarks + workaround까지만 생성
python3 build.py --assembly-only --out asm-only
```

`out/<kernel>/vf<VF>-ic1-swp-off/`에 assembly, 수정 전 assembly, remarks, ELF와 command metadata가 생성된다. `build.py`는 VF/IC remark와 `selected_kernel`의 e32 LMUL, workaround 적용 수를 확인한다. `s311` 1회, GESUMMV 2회, BiCG 0회라는 검증은 기록된 revision의 이 36개 설정을 위한 것이다. 다른 compiler에서 extraction shape가 달라지면 실패를 조사하고 검증 기대값을 갱신해야 한다.

`-force-vector-width`/`-force-vector-interleave`는 실제 compiler flags다. kernel code를 수작업 RVV intrinsic으로 대체하거나 array/global 배치, `noinline`, barrier, 반복 횟수를 바꾸지 않는다. kernel 함수만 떼어서 compile하면 여기 기록된 결과와 compilation context가 달라진다.

### XiangShan에서 ROI 측정

준비된 원래 XiangShan `emu`, NEMU reference shared object, simulator working directory를 지정한다. 이 세 경로는 외부 실행 환경의 경로다. `EMU_WORKDIR`에는 원래 runner에서 사용한 `init`/설정 입력이 있어야 한다. 새로운 RTL build에서 같은 source SHA만 맞추는 것으로 동일한 microarchitecture/configuration이 보장되지는 않는다.

```bash
python3 run.py --build out --results rerun \
  --emu /absolute/path/to/XiangShan/build/emu \
  --ref /absolute/path/to/riscv64-nemu-interpreter-so \
  --cwd /absolute/path/to/EMU_WORKDIR
```

실제 명령은 각 ELF에 대해 `emu -i <elf> --diff <ref> --max-cycles 5000000 --seed 1`이다. wrapper는 exit status, 유일한 `MB_ROI`, `HIT GOOD TRAP`을 확인하고 `rerun/summary.csv`에 새 결과를 기록한다. 기존 results 디렉터리를 덮어쓰지 않는다. 원래 `measured.csv`와 새 측정은 구분한다. 원본 kernel의 `result_sink == -1.0f` 검사는 전체 numerical correctness 검증이 아니며, GOOD TRAP만으로 변경된 알고리즘의 정확성이 증명되는 것도 아니다.

## 7. 실제 소스와 재현 파일

두 kernel C 파일은 측정 manifest의 source SHA-256과 일치한다. runtime 및 postprocessor도 현재 저장소 파일 원문을 포함한다. 각 파일의 marker에 SHA-256을 기록했다. 원문의 상대 경로 license 주석은 원래 저장소 배치를 가리킨다. 재배포용 attribution/license 자료는 아래 `licenses/` 파일로 함께 제공한다.

### 7.1. `tsvc_kernels.c`

<!-- file: tsvc_kernels.c sha256: 4aa2e36fa8d4c857db6177579c26737640de72e1c39a0c7bfd22fc13312d1ae3 -->
```c
/*
 * Copyright (c) 2011 University of Illinois at Urbana-Champaign.
 * TSVC license: ../../benchmarks/TSVC-2/upstream/license.txt
 *
 * Small, freestanding adaptations of representative UoB TSVC-2 loops.
 * Original loop identifiers are kept in each kernel name.  Array sizes are
 * deliberately reduced for RTL simulation; loop bodies retain TSVC shape.
 */
#include <stdint.h>
#include <stdio.h>

#define LOOP_SIZE 4096
#define STRIDED_OUTPUT_SIZE (2 * LOOP_SIZE)
#define LEN_2D 64
#define REPEATS 32
#define ALIGNMENT 64

#ifndef TSVC_CASE
#error "TSVC_CASE must select one kernel"
#endif

static float a[STRIDED_OUTPUT_SIZE] __attribute__((aligned(ALIGNMENT)));
static float b[LOOP_SIZE] __attribute__((aligned(ALIGNMENT)));
static float c[LOOP_SIZE] __attribute__((aligned(ALIGNMENT)));
static float d[LOOP_SIZE] __attribute__((aligned(ALIGNMENT)));
static int index_array[LOOP_SIZE] __attribute__((aligned(ALIGNMENT)));
static float flat_2d_array[LEN_2D * LEN_2D] __attribute__((aligned(ALIGNMENT)));
static float aa[LEN_2D][LEN_2D] __attribute__((aligned(ALIGNMENT)));
static float bb[LEN_2D][LEN_2D] __attribute__((aligned(ALIGNMENT)));
static float cc[LEN_2D][LEN_2D] __attribute__((aligned(ALIGNMENT)));
static volatile float result_sink;

extern uint64_t builder_platform_cycle(void);

__attribute__((noinline, optnone))
static void initialise_arrays(void) {
    for (int i = 0; i < STRIDED_OUTPUT_SIZE; ++i)
        a[i] = 1.0f + (float)(i & 7) * 0.01f;
    for (int i = 0; i < LOOP_SIZE; ++i) {
        b[i] = 2.0f + (float)(i & 3) * 0.02f;
        c[i] = 3.0f + (float)(i & 15) * 0.01f;
        d[i] = 4.0f + (float)(i & 7) * 0.03f;
        index_array[i] = (i * 5) & (LOOP_SIZE - 1);
    }
    for (int i = 0; i < LEN_2D; ++i) {
        for (int j = 0; j < LEN_2D; ++j) {
            aa[i][j] = 1.0f + (float)i * 0.01f;
            bb[i][j] = 2.0f + (float)j * 0.01f;
            cc[i][j] = 0.5f + (float)((i + j) & 7) * 0.02f;
            flat_2d_array[i * LEN_2D + j] = 0.0f;
        }
    }
}

static inline uint64_t read_cycle(void) {
    __asm__ volatile("fence rw, rw" ::: "memory");
    uint64_t value = builder_platform_cycle();
    __asm__ volatile("fence rw, rw" ::: "memory");
    return value;
}

#if TSVC_CASE == 0
/* TSVC-2 s000: unit-stride map. */
__attribute__((noinline))
static float selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
    for (int i = 0; i < LOOP_SIZE; ++i)
        a[i] = b[i] + 1.0f;
    return a[LOOP_SIZE - 1];
}
#elif TSVC_CASE == 1
/* TSVC-2 s1111: mixed arithmetic with a strided store. */
__attribute__((noinline))
static float selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
    for (int i = 0; i < LOOP_SIZE; ++i)
        a[2 * i] = c[i] * b[i] + d[i] * b[i] + c[i] * c[i]
                 + d[i] * b[i] + d[i] * c[i];
    return a[STRIDED_OUTPUT_SIZE - 2];
}
#elif TSVC_CASE == 2
/* TSVC-2 s125: collapsed-index 2-D map, 64 x 64 = 4096 elements. */
__attribute__((noinline))
static float selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
    float *aa_flat = &aa[0][0];
    float *bb_flat = &bb[0][0];
    float *cc_flat = &cc[0][0];
    for (int k = 0; k < LEN_2D * LEN_2D; ++k)
        flat_2d_array[k] = aa_flat[k] + bb_flat[k] * cc_flat[k];
    return flat_2d_array[LEN_2D * LEN_2D - 1];
}
#elif TSVC_CASE == 3
/* TSVC-2 s311: floating-point sum reduction. */
__attribute__((noinline))
static float selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
    float sum = 0.0f;
    for (int i = 0; i < LOOP_SIZE; ++i)
        sum += a[i];
    return sum;
}
#elif TSVC_CASE == 4
/* TSVC-2 s4112: sparse SAXPY requiring a gather. */
__attribute__((noinline))
static float selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
    const float scale = 0.5f;
    for (int i = 0; i < LOOP_SIZE; ++i)
        a[i] += b[index_array[i]] * scale;
    return a[LOOP_SIZE - 1];
}
#else
#error "unknown TSVC_CASE"
#endif

int builder_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    initialise_arrays();

    float result = 0.0f;
    uint64_t begin = read_cycle();
    for (int repeat = 0; repeat < REPEATS; ++repeat)
        result += selected_kernel();
    uint64_t end = read_cycle();

    result_sink = result;
    printf("MB_ROI=%016llx\n", (unsigned long long)(end - begin));
    return result_sink == -1.0f;
}
```

### 7.2. `application_kernels.c`

<!-- file: application_kernels.c sha256: 3f64e3fdfb2a1c5a3350dc6b52c9d5483585749c0a88e6261d1ae1aa854f9a14 -->
```c
/*
 * Freestanding, single-row adaptations of PolyBench/C 3.2 and ONNX Runtime.
 * PolyBench: Copyright (c) 2011-2012 the Ohio State University.
 * PolyBench authors: Louis-Noel Pouchet and Uday Bondugula.
 * PolyBench sources: ../../benchmarks/Polybench/upstream/ (see README.md).
 * RoPE: Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License; see licenses/onnxruntime-MIT.txt.
 * ORT revision: 1bc68c1d25b374e13bbf9a0649441af1ad9f1501.
 *
 * Each call executes one original inner loop, with no matrix allocation,
 * copy-back, dispatch, or other benchmark phases in the measured region.
 */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef APP_LOOP_SIZE
#error "APP_LOOP_SIZE must be supplied by suite.py"
#endif
#if APP_LOOP_SIZE <= 0 || APP_LOOP_SIZE % 2 != 0
#error "APP_LOOP_SIZE must be a positive even number"
#endif
#ifndef APP_CASE
#error "APP_CASE must select one kernel"
#endif
#ifndef APP_REPEATS
#error "APP_REPEATS must be supplied by suite.py"
#endif

#define ALIGNED __attribute__((aligned(64)))
static float a[APP_LOOP_SIZE + 2] ALIGNED;
static float b[APP_LOOP_SIZE + 2] ALIGNED;
static float c[APP_LOOP_SIZE + 2] ALIGNED;
static float x[APP_LOOP_SIZE] ALIGNED;
static float output[APP_LOOP_SIZE] ALIGNED;
static float scalars[2] ALIGNED;
static volatile float result_sink;

__attribute__((noinline, optnone))
static void initialise_arrays(void) {
    for (int i = 0; i < APP_LOOP_SIZE + 2; ++i) {
        a[i] = 0.25f + (float)(i % 17) * 0.03125f;
        b[i] = -0.5f + (float)(i % 13) * 0.0625f;
        c[i] = 0.125f + (float)(i % 19) * 0.015625f;
    }
    for (int i = 0; i < APP_LOOP_SIZE; ++i) {
        x[i] = -0.25f + (float)(i % 11) * 0.0625f;
        output[i] = 0.125f + (float)(i % 7) * 0.03125f;
    }
#if APP_CASE == 3
    for (int i = 0; i < APP_LOOP_SIZE / 2; ++i) {
        b[i] = sinf((float)i * 0.01f);
        c[i] = cosf((float)i * 0.01f);
    }
#endif
    scalars[0] = scalars[1] = 0.0f;
}

__attribute__((noinline))
static void selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
#if APP_CASE == 0
    /* Jacobi: a = middle row, b = upper row, c = lower row; both halos kept. */
    /* Target: jacobi-2d-imper-l00 */
    for (int j = 1; j <= APP_LOOP_SIZE; ++j)
        output[j - 1] = 0.2f * (a[j] + a[j - 1] + a[j + 1] + c[j] + b[j]);
#elif APP_CASE == 1
    /* GESUMMV: a/b = A/B row, x shared by both dot products. */
    float tmp = 0.0f;
    float y = 0.0f;
    /* Target: gesummv-l00 */
    for (int j = 0; j < APP_LOOP_SIZE; ++j) {
        tmp = a[j] * x[j] + tmp;
        y = b[j] * x[j] + y;
    }
    scalars[0] = tmp;
    scalars[1] = 1.5f * tmp + 1.2f * y;
#elif APP_CASE == 2
    /* BiCG: a = A row, x = p, output = s; r[i] = 0.75. */
    float q = 0.0f;
    /* Target: bicg-l01 */
    for (int j = 0; j < APP_LOOP_SIZE; ++j) {
        output[j] = output[j] + 0.75f * a[j];
        q = q + a[j] * x[j];
    }
    scalars[0] = q;
#elif APP_CASE == 3
    /* ORT MlasRotaryEmbedOneRow_FallBack<float>, interleaved = false.
     * a = input, b = sin_data, c = cos_data. Preserve the scalar indexing.
     */
    const size_t half = APP_LOOP_SIZE / 2;
    /* Target: rope-f32-noninterleaved */
    for (size_t i = 0; i < APP_LOOP_SIZE; ++i) {
        size_t cache_idx = i % half;
        bool sign = i >= half;
        size_t j = (i + half) % APP_LOOP_SIZE;
        float value = a[i] * c[cache_idx];
        if (sign)
            value += a[j] * b[cache_idx];
        else
            value -= a[j] * b[cache_idx];
        output[i] = value;
    }
#elif APP_CASE == 4
    /* GEMVER: output = A row, b/c = v1/v2; u1[i]/u2[i] = 0.5/0.75. */
    /* Target: gemver-l00 */
    for (int j = 0; j < APP_LOOP_SIZE; ++j)
        output[j] = output[j] + 0.5f * b[j] + 0.75f * c[j];
#else
#error "unknown APP_CASE"
#endif
}

/* Consume every output outside the ROI, without creating vector candidates. */
__attribute__((noinline, optnone))
static void consume_outputs(void) {
    float sum = scalars[0] + scalars[1];
    for (int i = 0; i < APP_LOOP_SIZE; ++i)
        sum += output[i];
    result_sink = sum;
}

extern uint64_t builder_platform_cycle(void);

static inline uint64_t read_cycle(void) {
#ifdef __riscv
    __asm__ volatile("fence rw, rw" ::: "memory");
#endif
    uint64_t value = builder_platform_cycle();
#ifdef __riscv
    __asm__ volatile("fence rw, rw" ::: "memory");
#endif
    return value;
}

int builder_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    initialise_arrays();
    uint64_t begin = read_cycle();
#pragma clang loop vectorize(disable) unroll(disable)
    for (int repeat = 0; repeat < APP_REPEATS; ++repeat)
        selected_kernel();
    uint64_t end = read_cycle();
    consume_outputs();
    printf("MB_ROI=%016llx\n", (unsigned long long)(end - begin));
    return result_sink == -1.0f;
}
```

### 7.3. `xiangshan_reduction_workaround.py`

<!-- file: xiangshan_reduction_workaround.py sha256: 168cb386c05391a57de441fcdf1af099eb6e8f68615a1ead5720fa03543a7f5b -->
```python
#!/usr/bin/env python3
"""Patch e32 floating-point reduction extraction for XiangShan."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import tempfile


LMULS = ("m1", "m2", "m4", "m8")
WORKAROUND_NAME = "xiangshan-f32-reduction-extract-via-gpr-e64-v1"
MARKER = f"# {WORKAROUND_NAME}"
SUPPORTED_REDUCTIONS = {
    "vfredosum.vs",
    "vfredusum.vs",
    "vfredmin.vs",
    "vfredmax.vs",
}
WIDENING_REDUCTIONS = {"vfwredosum.vs", "vfwredusum.vs"}

FUNCTION_RE = re.compile(
    r"(?ms)^[ \t]*\.type[ \t]+(?P<name>[A-Za-z_.$][\w.$]*),[ \t]*@function[^\n]*\n"
    r"(?P<body>.*?)"
    r"(?=^[ \t]*\.size[ \t]+(?P=name)[ \t]*,)"
)
INSTRUCTION_RE = re.compile(
    r"(?m)^(?P<indent>[ \t]*)(?P<opcode>[A-Za-z][A-Za-z0-9_.]*)"
    r"(?:[ \t]+(?P<operands>[^#\n]*?))?[ \t]*(?:#.*)?(?P<newline>\n|$)"
)
IGNORABLE_RE = re.compile(r"(?m)^[ \t]*(?:#[^\n]*)?(?:\n|$)")
VECTOR_CONFIGS = {"vsetvli", "vsetivli", "vsetvl"}


class WorkaroundError(ValueError):
    """Assembly does not match a safe workaround shape."""


def _operands(instruction: re.Match[str]) -> list[str]:
    return [operand.strip() for operand in (instruction["operands"] or "").split(",")]


def _only_space_or_comments(text: str) -> bool:
    return not IGNORABLE_RE.sub("", text)


def _active_sew(instructions: list[re.Match[str]], reduction_index: int) -> str | None:
    for instruction in reversed(instructions[:reduction_index]):
        if instruction["opcode"].lower() not in VECTOR_CONFIGS:
            continue
        match = re.search(r"(?:^|,)\s*(e(?:8|16|32|64))\b", instruction["operands"] or "")
        return match.group(1) if match else None
    return None


def _active_vtype(instructions: list[re.Match[str]], reduction_index: int) -> str:
    """Recover the immediate vtype; never restore through XiangShan's vsetvl path."""
    for instruction in reversed(instructions[:reduction_index]):
        opcode = instruction["opcode"].lower()
        if opcode not in VECTOR_CONFIGS:
            continue
        if opcode != "vsetvl":
            suffix = ", ".join(_operands(instruction)[2:])
            if re.fullmatch(r"e32, m(?:f[248]|[1248]), t[au], m[au]", suffix):
                return suffix
        break
    raise WorkaroundError("state-preserving extraction requires a known immediate e32 vtype")


def _is_vector_instruction(instruction: re.Match[str]) -> bool:
    return instruction["opcode"].lower().startswith("v")


def _paired_reductions(body: str) -> list[tuple[list[re.Match[str]], int]]:
    instructions = list(INSTRUCTION_RE.finditer(body))
    pairs: list[tuple[list[re.Match[str]], int]] = []
    reductions = SUPPORTED_REDUCTIONS | WIDENING_REDUCTIONS
    for index, instruction in enumerate(instructions):
        if instruction["opcode"].lower() not in reductions or index + 1 == len(instructions):
            continue
        extraction = instructions[index + 1]
        between = body[instruction.end() : extraction.start()]
        if (
            extraction["opcode"].lower() == "vfmv.f.s"
            and _only_space_or_comments(between)
        ):
            pairs.append((instructions, index))
    return pairs


def remaining_extraction_count(text: str) -> int:
    """Return terminal or unsafe reduction/extraction pairs still present."""

    return sum(len(_paired_reductions(match["body"])) for match in FUNCTION_RE.finditer(text))


def _rewrite_function(body: str, lmul: str, preserve_state: bool) -> tuple[str, int]:
    replacements: list[tuple[int, int, str]] = []
    for instructions, reduction_index in _paired_reductions(body):
        reduction = instructions[reduction_index]
        extraction = instructions[reduction_index + 1]
        opcode = reduction["opcode"].lower()
        if opcode in WIDENING_REDUCTIONS:
            raise WorkaroundError("widening floating-point reduction extraction is unsupported")

        reduction_operands = _operands(reduction)
        extraction_operands = _operands(extraction)
        if len(reduction_operands) < 2 or not re.fullmatch(r"v\d+", reduction_operands[0]):
            raise WorkaroundError("floating-point reduction has an invalid destination register")
        if len(extraction_operands) != 2:
            raise WorkaroundError("vfmv.f.s extraction has invalid operands")
        fp_destination = extraction_operands[0]
        if not re.fullmatch(r"(?:f(?:[0-9]|[12][0-9]|3[01])|fa[0-7]|ft(?:[0-9]|1[01])|fs(?:[0-9]|1[01]))", fp_destination):
            raise WorkaroundError("vfmv.f.s extraction has an invalid floating-point destination")
        if not preserve_state and fp_destination != "fa0":
            raise WorkaroundError("floating-point reduction extraction destination is not fa0")
        vd = reduction_operands[0]
        if extraction_operands[1] != vd:
            raise WorkaroundError(
                f"floating-point reduction destination {vd} does not match extraction source "
                f"{extraction_operands[1]}"
            )
        sew = _active_sew(instructions, reduction_index)
        if sew != "e32":
            raise WorkaroundError(
                "floating-point reduction extraction is not known to use e32 "
                f"(active SEW: {sew or 'unknown'})"
            )
        if not preserve_state and any(_is_vector_instruction(item) for item in instructions[reduction_index + 2 :]):
            raise WorkaroundError("vector instruction follows floating-point reduction extraction")

        indent = reduction["indent"]
        reduction_line = body[reduction.start() : reduction.end()].rstrip("\n")
        between = body[reduction.end() : extraction.start()]
        newline = extraction["newline"] or "\n"
        prefix = [f"{indent}{MARKER}"]
        if preserve_state:
            # No liveness assumptions: keep the ABI-aligned stack and all scratch
            # GPRs intact. Save runtime VL, but recover vtype from the assembly:
            # register-form vsetvl can stall the deployed XiangShan RTL after
            # committing, even though the extraction itself completed correctly.
            restore_vtype = _active_vtype(instructions, reduction_index)
            prefix += [
                f"{indent}addi\tsp, sp, -16",
                f"{indent}sd\tt0, 0(sp)",
                f"{indent}sd\tt1, 8(sp)",
                f"{indent}csrr\tt1, vl",
            ]
        prefix += [
            f"{indent}vsetvli\tzero, zero, e32, {lmul}, tu, ma",
            reduction_line,
        ]
        replacement = "\n".join(prefix)
        replacement += "\n" + between
        scratch = "t0" if preserve_state else "a0"
        suffix = [
            f"{indent}vsetivli\tzero, 1, e64, m1, ta, ma",
            f"{indent}vmv.x.s\t{scratch}, {vd}",
            f"{indent}fmv.w.x\t{fp_destination}, {scratch}",
        ]
        if preserve_state:
            suffix += [
                f"{indent}vsetvli\tzero, t1, {restore_vtype}",
                f"{indent}ld\tt0, 0(sp)",
                f"{indent}ld\tt1, 8(sp)",
                f"{indent}addi\tsp, sp, 16",
            ]
        replacement += "\n".join(suffix)
        replacement += newline
        replacements.append((reduction.start(), extraction.end(), replacement))

    for start, end, replacement in reversed(replacements):
        body = body[:start] + replacement + body[end:]
    return body, len(replacements)


def rewrite_assembly(text: str, lmul: str, *, preserve_state: bool = False) -> tuple[str, int]:
    """Rewrite safe reduction extractions and return text plus applied count."""

    if lmul not in LMULS:
        raise ValueError(f"unsupported LMUL: {lmul}")

    replacements: list[tuple[int, int, str]] = []
    count = 0
    for function in FUNCTION_RE.finditer(text):
        body, applied = _rewrite_function(function["body"], lmul, preserve_state)
        if applied:
            replacements.append((function.start("body"), function.end("body"), body))
            count += applied
    for start, end, replacement in reversed(replacements):
        text = text[:start] + replacement + text[end:]
    return text, count


def patch_file(path: str | Path, lmul: str, *, preserve_state: bool = False) -> int:
    """Atomically rewrite an assembly file and return applied count."""

    assembly = Path(path)
    original = assembly.read_text()
    rewritten, count = rewrite_assembly(original, lmul, preserve_state=preserve_state)
    if rewritten == original:
        return count

    mode = assembly.stat().st_mode
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{assembly.name}.", suffix=".tmp", dir=assembly.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w") as output:
            output.write(rewritten)
            output.flush()
            os.fsync(output.fileno())
        os.chmod(temporary, mode)
        os.replace(temporary, assembly)
    finally:
        if temporary.exists():
            temporary.unlink()
    return count


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("assembly", type=Path, metavar="ASM")
    parser.add_argument("--lmul", choices=LMULS, required=True)
    parser.add_argument("--preserve-state", action="store_true",
                        help="preserve scratch GPRs, runtime vl and immediate vtype; allow any FP destination and later vector operations")
    args = parser.parse_args(argv)
    try:
        count = patch_file(args.assembly, args.lmul, preserve_state=args.preserve_state)
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")
    print(f"applied {count} {WORKAROUND_NAME} workaround(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

### 7.4. `build.py`

<!-- file: build.py sha256: 2cedd48e7bfa9d97d6035bd27908c7a449acf86cd8d435b1898b64dd0ff9ea5e -->
```python
#!/usr/bin/env python3
"""Build the nine original kernels at IC=1, SWP=off, scalable VF=2/4/8/16."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parent
CASES = [
    ("s000", "tsvc_kernels.c", 0),
    ("s1111", "tsvc_kernels.c", 1),
    ("s125", "tsvc_kernels.c", 2),
    ("s311", "tsvc_kernels.c", 3),
    ("s4112", "tsvc_kernels.c", 4),
    ("jacobi-2d-imper-l00", "application_kernels.c", 0),
    ("gesummv-l00", "application_kernels.c", 1),
    ("bicg-l01", "application_kernels.c", 2),
    ("gemver-l00", "application_kernels.c", 4),
]
TARGET = [
    "--target=riscv64-unknown-elf",
    "-march=rv64gcv_zba_zbb_zbc_zbs_zicbom_zicboz_zvl128b",
    "-mcpu=xiangshan-kunminghu", "-mabi=lp64d", "-mcmodel=medany",
    "-mno-relax", "-mllvm", "-riscv-v-vector-bits-min=128",
]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="/opt/llvm-main/bin/clang")
    parser.add_argument("--cxx", default="/opt/llvm-main/bin/clang++")
    parser.add_argument("--gcc", default="riscv64-unknown-elf-gcc")
    parser.add_argument("--picolibc", type=Path,
                        default=Path("/usr/lib/picolibc/riscv64-unknown-elf"))
    parser.add_argument("--out", type=Path, default=ROOT / "out")
    parser.add_argument("--assembly-only", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    out = args.out.resolve()

    def run(command, capture=False):
        print(shlex.join(map(str, command)), flush=True)
        if args.dry_run:
            return None
        return subprocess.run(list(map(str, command)), check=True,
                              text=True, capture_output=capture)

    if not args.dry_run:
        out.mkdir(parents=True, exist_ok=True)
        (out / "compiler-version.txt").write_text(
            run([args.cc, "--version"], True).stdout)
    headers = ["-idirafter", str(args.picolibc / "include")]
    runtime_objects = []
    gcc_lib = "<resolved-by-riscv64-unknown-elf-gcc>"
    if not args.assembly_only:
        runtime = out / "runtime"
        if not args.dry_run:
            runtime.mkdir(exist_ok=True)
        runtime_flags = TARGET + [
            "-O2", "-ffreestanding", "-fno-builtin", "-ffunction-sections",
            "-fdata-sections", "-D_DEFAULT_SOURCE=1", "-D_POSIX_C_SOURCE=200112L",
        ] + headers
        for name, source, language in [
            ("startup", "xiangshan/startup.S", "assembler-with-cpp"),
            ("start", "common/start.c", "c"),
            ("syscalls", "common/syscalls.c", "c"),
            ("stdio", "common/stdio.c", "c"),
            ("cxx", "common/cxx.cpp", "c++"),
            ("platform", "xiangshan/platform.c", "c"),
        ]:
            obj = runtime / (name + ".o")
            flags = list(runtime_flags)
            if language == "c++":
                flags += ["-std=c++20", "-stdlib=libc++", "-fno-exceptions", "-fno-rtti"]
            run([args.cxx if language == "c++" else args.cc] + flags +
                ["-x", language, "-c", ROOT / "runtime" / source, "-o", obj])
            runtime_objects.append(str(obj))
        result = run([args.gcc, "-march=rv64iafd", "-mabi=lp64d",
                      "-print-libgcc-file-name"], True)
        if result:
            gcc_lib = result.stdout.strip()
            if not Path(gcc_lib).is_file():
                raise RuntimeError("libgcc.a was not found")

    if not args.dry_run:
        from xiangshan_reduction_workaround import rewrite_assembly, remaining_extraction_count
    manifest = []
    for kernel, source, case in CASES:
        for vf, lmul in [(2, "m1"), (4, "m2"), (8, "m4"), (16, "m8")]:
            directory = out / kernel / f"vf{vf}-ic1-swp-off"
            asm = directory / "xiangshan.s"
            obj = directory / "xiangshan.o"
            elf = directory / "xiangshan.elf"
            defines = ([f"-DTSVC_CASE={case}"] if source == "tsvc_kernels.c" else
                       [f"-DAPP_CASE={case}", "-DAPP_LOOP_SIZE=4096", "-DAPP_REPEATS=32"])
            flags = TARGET + [
                "-std=c11", "-ffreestanding", "-fno-builtin", "-ffunction-sections",
                "-fdata-sections", "-O2", "-ffast-math", "-fno-unroll-loops",
                "-mllvm", "-scalable-vectorization=on",
                "-mllvm", f"-force-vector-width={vf}",
                "-mllvm", "-force-vector-interleave=1",
                "-mllvm", "-riscv-enable-pipeliner=false", "-Rpass=loop-vectorize",
                "-I", str(ROOT / "include"),
            ] + headers + defines
            compile_command = [args.cc] + flags + ["-S", str(ROOT / source), "-o", str(asm)]
            if not args.dry_run:
                directory.mkdir(parents=True, exist_ok=True)
            result = run(compile_command, True)
            applied = 0
            if not args.dry_run:
                (directory / "xiangshan.remarks.txt").write_text(result.stderr)
                expected = f"vectorization width: vscale x {vf}, interleaved count: 1"
                if expected not in result.stderr:
                    raise RuntimeError(f"{kernel}/{vf}: missing forced VF/IC remark")
                original = asm.read_text()
                (directory / "xiangshan.unpatched.s").write_text(original)
                stateful = kernel in ("gesummv-l00", "bicg-l01")
                if kernel == "s311" or stateful:
                    patched, applied = rewrite_assembly(original, lmul, preserve_state=stateful)
                    expected_count = 1 if kernel == "s311" else 2 if kernel == "gesummv-l00" else 0
                    if applied != expected_count or remaining_extraction_count(patched):
                        raise RuntimeError(f"{kernel}/{vf}: unexpected extraction shape/count {applied}")
                    asm.write_text(patched)
                body = re.search(r"(?ms)^selected_kernel:.*?^\s*\.size\s+selected_kernel,", asm.read_text())
                if not body or not re.search(rf"e32,\s*{lmul}\b", body.group()):
                    raise RuntimeError(f"{kernel}/{vf}: missing target-loop LMUL")
            assemble_command = [args.cc] + TARGET + ["-c", str(asm), "-o", str(obj)]
            link_command = [args.cc] + TARGET + [
                "-nostdlib", "-nostartfiles", "-static", "-fuse-ld=lld", "-Wl,--gc-sections",
                "-Wl,-T," + str(ROOT / "runtime/xiangshan/link.ld"),
            ] + runtime_objects + [str(obj), "-L", str(args.picolibc / "lib/rv64iafd/lp64d"),
                                  "-Wl,--start-group", "-lc", "-lm", gcc_lib,
                                  "-Wl,--end-group", "-o", str(elf)]
            if not args.assembly_only:
                run(assemble_command)
                run(link_command)
            if not args.dry_run:
                record = dict(kernel=kernel, vf=vf, lmul=lmul, interleave_count=1,
                              software_pipelining="off", useful_elements=4096 * 32,
                              source_sha256=sha(ROOT / source), assembly_sha256=sha(asm),
                              workaround_applied=applied, compile=compile_command,
                              assemble=assemble_command, link=link_command)
                if not args.assembly_only:
                    record.update(elf=str(elf), elf_sha256=sha(elf))
                (directory / "command.json").write_text(json.dumps(record, indent=2) + "\n")
                manifest.append(record)
    if not args.dry_run:
        (out / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

if __name__ == "__main__":
    main()
```

### 7.5. `run.py`

<!-- file: run.py sha256: f163f1797315498c4762423271c7444479cc5bbc0a86d0415e023e43a3a55656 -->
```python
#!/usr/bin/env python3
"""Measure rebuilt ELFs using an externally supplied XiangShan emulator."""
import argparse
import csv
import json
from pathlib import Path
import re
import subprocess

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", type=Path, default=Path("out"))
    p.add_argument("--results", type=Path, default=Path("rerun"))
    p.add_argument("--emu", type=Path, required=True)
    p.add_argument("--ref", type=Path, required=True)
    p.add_argument("--cwd", type=Path, required=True,
                   help="Original emulator working directory, including its init/config inputs")
    args = p.parse_args()
    # A fresh directory prevents accidental mixing with the historical results.
    args.results.mkdir(parents=True, exist_ok=False)
    rows = json.loads((args.build / "build-manifest.json").read_text())
    fields = ["kernel", "vf", "lmul", "interleave_count", "software_pipelining",
              "raw_roi_cycles", "cycles_per_element", "rtl_revision", "seed"]
    with (args.results / "summary.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            directory = args.results / row["kernel"] / f"vf{row['vf']}"
            directory.mkdir(parents=True)
            elf = args.build / row["kernel"] / f"vf{row['vf']}-ic1-swp-off" / "xiangshan.elf"
            command = [str(args.emu.resolve()), "-i", str(elf.resolve()),
                       "--diff", str(args.ref.resolve()), "--max-cycles", "5000000", "--seed", "1"]
            (directory / "command.json").write_text(json.dumps(command, indent=2) + "\n")
            with (directory / "stdout.log").open("w") as stdout, (directory / "stderr.log").open("w") as stderr:
                result = subprocess.run(command, cwd=args.cwd, stdout=stdout, stderr=stderr)
            log = (directory / "stdout.log").read_text()
            markers = re.findall(r"MB_ROI=([0-9a-fA-F]+)", log)
            if result.returncode or len(markers) != 1 or "HIT GOOD TRAP" not in log:
                raise RuntimeError(f"failed run: {directory}")
            cycles = int(markers[0], 16)
            if cycles <= 0:
                raise RuntimeError(f"invalid ROI cycles: {directory}")
            revision = re.search(r"Commit SHA is: ([0-9a-f]+)", log)
            output = {key: row[key] for key in fields[:5]}
            output.update(raw_roi_cycles=cycles, cycles_per_element=cycles / 131072,
                          rtl_revision=revision[1] if revision else "unknown", seed=1)
            writer.writerow(output)
            f.flush()
            print(output, flush=True)

if __name__ == "__main__":
    main()
```

### 7.6. `Dockerfile`

<!-- file: Dockerfile sha256: 064cb5bf0f93e6ecec8984775de31702166c947c4fa205c7d49260bb5febf107 -->
```dockerfile
# Build with Docker BuildKit. External packages are not an archived image digest.
FROM ubuntu:24.04 AS llvm-build
ARG DEBIAN_FRONTEND=noninteractive
ARG LLVM_JOBS=2
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates build-essential clang cmake git lld ninja-build python3 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
RUN git init llvm-project \
    && git -C llvm-project remote add origin https://github.com/cheolwanpark/llvm-project.git \
    && git -C llvm-project fetch --depth 1 origin 89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb \
    && git -C llvm-project checkout --detach FETCH_HEAD \
    && test "$(git -C llvm-project rev-parse HEAD)" = 89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb
RUN cmake -S llvm-project/llvm -B llvm-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" \
    -DCMAKE_INSTALL_PREFIX=/opt/llvm-main -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_DUMP=ON \
    -DLLVM_ENABLE_PROJECTS="clang;lld" -DLLVM_TARGETS_TO_BUILD=RISCV \
    -DLLVM_USE_LINKER=lld -DLLVM_PARALLEL_LINK_JOBS=1 \
    -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_TESTS=OFF -DLLVM_BUILD_TESTS=OFF \
    -DLLVM_ENABLE_LIBXML2=OFF -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF \
    && cmake --build llvm-build --target install-clang install-clang-resource-headers \
       install-lld install-opt install-llc install-llvm-objdump -j "${LLVM_JOBS}" \
    && ln -sfn clang /opt/llvm-main/bin/clang++ \
    && ln -sfn lld /opt/llvm-main/bin/ld.lld

FROM ubuntu:24.04
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates wget python3 gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf \
    && wget -qO /usr/share/keyrings/apt.llvm.org.asc https://apt.llvm.org/llvm-snapshot.gpg.key \
    && echo "deb [signed-by=/usr/share/keyrings/apt.llvm.org.asc] http://apt.llvm.org/noble/ llvm-toolchain-noble-22 main" \
       > /etc/apt/sources.list.d/llvm.list \
    && apt-get update && apt-get install -y --no-install-recommends libc++-22-dev \
    && rm -rf /var/lib/apt/lists/*
COPY --from=llvm-build /opt/llvm-main /opt/llvm-main
COPY --from=llvm-build /workspace/llvm-project/llvm/LICENSE.TXT /opt/llvm-main/LICENSE.TXT
RUN mkdir -p /opt/llvm-main/include \
    && ln -sfn /usr/lib/llvm-22/include/c++ /opt/llvm-main/include/c++ \
    && for config in /usr/lib/llvm-22/include/c++/v1/__config_site /usr/include/c++/v1/__config_site; do \
      if test -f "$config"; then \
        sed -i \
          -e 's/#define _LIBCPP_HAS_THREADS 1/#define _LIBCPP_HAS_THREADS 0/' \
          -e 's/#define _LIBCPP_HAS_MONOTONIC_CLOCK 1/#define _LIBCPP_HAS_MONOTONIC_CLOCK 0/' \
          -e 's/#define _LIBCPP_HAS_THREAD_API_PTHREAD 1/#define _LIBCPP_HAS_THREAD_API_PTHREAD 0/' \
          -e 's/#define _LIBCPP_HAS_TERMINAL 1/#define _LIBCPP_HAS_TERMINAL 0/' \
          -e 's/#define _LIBCPP_HAS_FILESYSTEM 1/#define _LIBCPP_HAS_FILESYSTEM 0/' \
          -e 's/#define _LIBCPP_HAS_RANDOM_DEVICE 1/#define _LIBCPP_HAS_RANDOM_DEVICE 0/' \
          -e 's/#define _LIBCPP_HAS_LOCALIZATION 1/#define _LIBCPP_HAS_LOCALIZATION 0/' \
          -e 's/#define _LIBCPP_HAS_UNICODE 1/#define _LIBCPP_HAS_UNICODE 0/' \
          -e 's/#define _LIBCPP_HAS_WIDE_CHARACTERS 1/#define _LIBCPP_HAS_WIDE_CHARACTERS 0/' \
          -e 's/#define _LIBCPP_HAS_TIME_ZONE_DATABASE 1/#define _LIBCPP_HAS_TIME_ZONE_DATABASE 0/' \
          -e 's/#define _LIBCPP_LIBC_PICOLIBC 0/#define _LIBCPP_LIBC_PICOLIBC 1/' "$config"; \
      fi; \
    done
ENV PATH=/opt/llvm-main/bin:${PATH}
WORKDIR /work
```

### 7.7. `measured.csv`

<!-- file: measured.csv sha256: d52dd2b5c489ea1bbf7aef3f33052694521fc7952d83342331a0af6548b45b3c -->
```csv
kernel,vf,lmul,interleave_count,software_pipelining,raw_roi_cycles,cycles_per_element,speedup_vs_vf2,run_id,seed,rtl_revision
s000,2,m1,1,off,471934,3.600570679,1.0,d727d32e2224bec693ca36468ecc8fd5,1,75775862a6
s000,4,m2,1,off,181039,1.381217957,2.6068084777313176,80bf69e74c1c21757f3cd909c4de4dec,1,75775862a6
s000,8,m4,1,off,174586,1.331985474,2.7031606199809834,38fb3cc8113b480f9fe04b44767de712,1,75775862a6
s000,16,m8,1,off,159331,1.215599060,2.961972246455492,d338484c093eefd4619cbe56ac7047b9,1,75775862a6
s1111,2,m1,1,off,1368994,10.444595337,1.0,9fd114e6160919a4bb95202e84191103,1,75775862a6
s1111,4,m2,1,off,1377183,10.507072449,0.9940538040333057,ae0cce70b96bc197d4fbf569b40786e4,1,75775862a6
s1111,8,m4,1,off,1160981,8.857582092,1.1791700294836867,70bb5eab5b4de97cfd4999ef6328ef8d,1,75775862a6
s1111,16,m8,1,off,1123047,8.568168640,1.2189997391026377,0d0d6c35029407c67b4ec181ba57a0a7,1,75775862a6
s125,2,m1,1,off,684788,5.224517822,1.0,14f77eddae3c94713e56c3250eaf3991,1,75775862a6
s125,4,m2,1,off,388692,2.965484619,1.761775390283309,cf40605beb3a95d065cbdda81745eadf,1,75775862a6
s125,8,m4,1,off,376793,2.874702454,1.8174116822764754,ff73fd6b27e001f0ecb9f438d9fc0f4d,1,75775862a6
s125,16,m8,1,off,354097,2.701545715,1.9338994682248085,a821d01535f82ead21f7a08deedea6c4,1,75775862a6
s311,2,m1,1,off,198467,1.514183044,1.0,3d63c73a14cdd84f44a642dc23e98ecc,1,75775862a6
s311,4,m2,1,off,100355,0.765647888,1.977649344825868,11e4ab542666f03cc971d54ffacb00b0,1,75775862a6
s311,8,m4,1,off,101871,0.777214050,1.9482188257698463,c1d0e72149171bffffd5090aed8411b4,1,75775862a6
s311,16,m8,1,off,78423,0.598320007,2.5307244048302158,8d3c887e559e68c4b32580e2d89ed7d9,1,75775862a6
s4112,2,m1,1,off,1254522,9.571243286,1.0,34b1e90a6c64a858117834895ef52594,1,75775862a6
s4112,4,m2,1,off,746233,5.693305969,1.6811398048598762,9a457d36903ec3ddc8589960264795f3,1,75775862a6
s4112,8,m4,1,off,706672,5.391479492,1.7752535829918266,f149ff7bdf10a95c5884fa22678fadde,1,75775862a6
s4112,16,m8,1,off,718042,5.478225708,1.7471429247871295,0539054f5131007a76643121deff139f,1,75775862a6
jacobi-2d-imper-l00,2,m1,1,off,1200885,9.162025452,1.0,0e26c8d42f0cd47cf4700f717306687d,1,75775862a6
jacobi-2d-imper-l00,4,m2,1,off,729082,5.562454224,1.6471192540756732,83ca3a52631bb9b9b6060908546ff112,1,75775862a6
jacobi-2d-imper-l00,8,m4,1,off,529345,4.038581848,2.268624432081157,0c0feee1c9641a4a56711c6b050b5a6b,1,75775862a6
jacobi-2d-imper-l00,16,m8,1,off,771458,5.885757446,1.5566433947149423,c1ec0ff205376d89add68d697e139887,1,75775862a6
gesummv-l00,2,m1,1,off,507862,3.874679565,1.0,cb2927dc11203421adf656f4fd69b8fc,1,75775862a6
gesummv-l00,4,m2,1,off,369351,2.817924500,1.3750118451012723,ecf0d629cb3caa70c0b66d8b19e82b57,1,75775862a6
gesummv-l00,8,m4,1,off,392315,2.993125916,1.2945260823572895,0a1684a75bb1a930e934a7ef2182b8ef,1,75775862a6
gesummv-l00,16,m8,1,off,337888,2.577880859,1.5030483473813807,325213d5aeabb6b9267dffec5af673f6,1,75775862a6
bicg-l01,2,m1,1,off,606973,4.630836487,1.0,e13a6d35914a590244be8a97aa943363,1,75775862a6
bicg-l01,4,m2,1,off,350537,2.674385071,1.7315518761214936,9c985b3f2a9b61bcb36b2e1ce47a4081,1,75775862a6
bicg-l01,8,m4,1,off,341400,2.604675293,1.7778939660222612,ca9ac7e7ccaf0699d034d32e8d26575b,1,75775862a6
bicg-l01,16,m8,1,off,328917,2.509437561,1.8453682844000159,d3da13b46bdcf9c57acfbce621e9d503,1,75775862a6
gemver-l00,2,m1,1,off,650222,4.960800171,1.0,317753327983755188d5d304d0750e9a,1,75775862a6
gemver-l00,4,m2,1,off,363914,2.776443481,1.786746319185302,7014f1a50dd63ab991f14b17b3c8ea2b,1,75775862a6
gemver-l00,8,m4,1,off,335283,2.558006287,1.9393229003558188,058ea062bc28cdc1f0782f630fc4f5df,1,75775862a6
gemver-l00,16,m8,1,off,322317,2.459083557,2.0173369695051764,5eb4c26ff69773212f5fcb62bc13c3d1,1,75775862a6
```

### 7.8. `runtime/common/start.c`

<!-- file: runtime/common/start.c sha256: d13f712199e30b111fa3d218cd38b38678645da44ebd85b52eec28e264abc61a -->
```c
#include <stddef.h>

extern int builder_main(int argc, char **argv);
extern char __bss_start[], __bss_end[], __tbss_start[], __tbss_end[];
extern void builder_platform_init(void);
extern void builder_platform_finish(int code) __attribute__((noreturn));

typedef void (*init_function)(void);
extern init_function __preinit_array_start[], __preinit_array_end[];
extern init_function __init_array_start[], __init_array_end[];

static char argument_zero[] = "benchmark";
static char *arguments[] = {argument_zero, NULL};

void __builder_start(void) {
    for (char *byte = __bss_start; byte < __bss_end; ++byte) {
        *byte = 0;
    }
    for (char *byte = __tbss_start; byte < __tbss_end; ++byte) {
        *byte = 0;
    }
    builder_platform_init();
    for (init_function *function = __preinit_array_start;
         function < __preinit_array_end; ++function) {
        (*function)();
    }
    for (init_function *function = __init_array_start;
         function < __init_array_end; ++function) {
        (*function)();
    }
    builder_platform_finish(builder_main(1, arguments));
}
```

### 7.9. `runtime/common/syscalls.c`

<!-- file: runtime/common/syscalls.c sha256: 7a6c796c22099c54526a325885b7e64c2ad04c53c46970835db8b0d086d8ca39 -->
```c
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>

extern char __heap_start[], __heap_end[];
extern long builder_platform_write(const void *buffer, unsigned long count);
extern uint64_t builder_platform_cycle(void);
extern void builder_platform_finish(int code) __attribute__((noreturn));

struct _reent;
static char *heap_current = __heap_start;

void *_sbrk(ptrdiff_t increment) {
    char *previous = heap_current;
    char *next = heap_current + increment;
    if (next < __heap_start || next > __heap_end) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_current = next;
    return previous;
}

void *sbrk(ptrdiff_t increment) { return _sbrk(increment); }
void *_sbrk_r(struct _reent *reent, ptrdiff_t increment) {
    (void)reent;
    return _sbrk(increment);
}

long _write(int fd, const void *buffer, unsigned long count) {
    (void)fd;
    return builder_platform_write(buffer, count);
}
long write(int fd, const void *buffer, unsigned long count) { return _write(fd, buffer, count); }
long _write_r(struct _reent *reent, int fd, const void *buffer, unsigned long count) {
    (void)reent;
    return _write(fd, buffer, count);
}

long _read(int fd, void *buffer, unsigned long count) {
    (void)fd; (void)buffer; (void)count;
    return 0;
}
long read(int fd, void *buffer, unsigned long count) { return _read(fd, buffer, count); }
long _read_r(struct _reent *reent, int fd, void *buffer, unsigned long count) {
    (void)reent;
    return _read(fd, buffer, count);
}

int _close(int fd) { (void)fd; return 0; }
int close(int fd) { return _close(fd); }
int _close_r(struct _reent *reent, int fd) { (void)reent; return _close(fd); }

int _fstat(int fd, struct stat *status) {
    (void)fd;
    if (status) status->st_mode = S_IFCHR;
    return 0;
}
int fstat(int fd, struct stat *status) { return _fstat(fd, status); }
int _fstat_r(struct _reent *reent, int fd, struct stat *status) {
    (void)reent;
    return _fstat(fd, status);
}

int _isatty(int fd) { (void)fd; return 1; }
int isatty(int fd) { return _isatty(fd); }
int _isatty_r(struct _reent *reent, int fd) { (void)reent; return _isatty(fd); }

long _lseek(int fd, long offset, int whence) {
    (void)fd; (void)offset; (void)whence;
    return 0;
}
long lseek(int fd, long offset, int whence) { return _lseek(fd, offset, whence); }
long _lseek_r(struct _reent *reent, int fd, long offset, int whence) {
    (void)reent;
    return _lseek(fd, offset, whence);
}

int _getpid(void) { return 1; }
int getpid(void) { return _getpid(); }
int _getpid_r(struct _reent *reent) { (void)reent; return _getpid(); }
int _kill(int pid, int signal) { (void)pid; (void)signal; errno = EINVAL; return -1; }
int kill(int pid, int signal) { return _kill(pid, signal); }
int _kill_r(struct _reent *reent, int pid, int signal) {
    (void)reent;
    return _kill(pid, signal);
}

int gettimeofday(struct timeval *value, void *zone) {
    (void)zone;
    uint64_t cycles = builder_platform_cycle();
    if (value) {
        value->tv_sec = (long)(cycles / 1000000ULL);
        value->tv_usec = (long)(cycles % 1000000ULL);
    }
    return 0;
}
int _gettimeofday(struct timeval *value, void *zone) { return gettimeofday(value, zone); }
int _gettimeofday_r(struct _reent *reent, struct timeval *value, void *zone) {
    (void)reent;
    return gettimeofday(value, zone);
}

clock_t clock(void) { return (clock_t)builder_platform_cycle(); }
time_t time(time_t *result) {
    time_t value = (time_t)(builder_platform_cycle() / 1000000ULL);
    if (result) *result = value;
    return value;
}
clock_t times(struct tms *value) {
    clock_t now = clock();
    if (value) {
        value->tms_utime = now;
        value->tms_stime = value->tms_cutime = value->tms_cstime = 0;
    }
    return now;
}

void _exit(int code) { builder_platform_finish(code); }
void exit(int code) { builder_platform_finish(code); }
void abort(void) { builder_platform_finish(134); }
```

### 7.10. `runtime/common/stdio.c`

<!-- file: runtime/common/stdio.c sha256: 7a15ed4690169573fef52cde75a786546803dcc2326b9214c286ebdc16bd3ed6 -->
```c
#include <stdio.h>

extern long builder_platform_write(const void *buffer, unsigned long count);

static int builder_stdio_put(char character, FILE *stream) {
    (void)stream;
    return builder_platform_write(&character, 1) == 1 ? 0 : EOF;
}

static int builder_stdio_get(FILE *stream) {
    (void)stream;
    return EOF;
}

static int builder_stdio_flush(FILE *stream) {
    (void)stream;
    return 0;
}

static FILE builder_stdin = FDEV_SETUP_STREAM(
    NULL, builder_stdio_get, builder_stdio_flush, _FDEV_SETUP_READ);
static FILE builder_stdout = FDEV_SETUP_STREAM(
    builder_stdio_put, NULL, builder_stdio_flush, _FDEV_SETUP_WRITE);
static FILE builder_stderr = FDEV_SETUP_STREAM(
    builder_stdio_put, NULL, builder_stdio_flush, _FDEV_SETUP_WRITE);

FILE *const stdin = &builder_stdin;
FILE *const stdout = &builder_stdout;
FILE *const stderr = &builder_stderr;
```

### 7.11. `runtime/common/cxx.cpp`

<!-- file: runtime/common/cxx.cpp sha256: 237bb43c419fd3173ffc71ae1172b44de026c74fce603ebb2a5dc9d60e624cf5 -->
```cpp
#include <cstddef>
#include <cstdlib>
#include <new>

extern "C" void *memalign(std::size_t alignment, std::size_t size);

#define BUILDER_WEAK __attribute__((weak))

[[noreturn]] static void allocation_failed() { std::abort(); }

BUILDER_WEAK void *operator new(std::size_t size) {
    if (void *pointer = std::malloc(size)) return pointer;
    allocation_failed();
}
BUILDER_WEAK void *operator new[](std::size_t size) { return ::operator new(size); }
BUILDER_WEAK void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    return std::malloc(size);
}
BUILDER_WEAK void *operator new[](std::size_t size, const std::nothrow_t &value) noexcept {
    return ::operator new(size, value);
}
BUILDER_WEAK void *operator new(std::size_t size, std::align_val_t alignment) {
    if (void *pointer = memalign(static_cast<std::size_t>(alignment), size)) return pointer;
    allocation_failed();
}
BUILDER_WEAK void *operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}
BUILDER_WEAK void *operator new(
    std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return memalign(static_cast<std::size_t>(alignment), size);
}
BUILDER_WEAK void *operator new[](
    std::size_t size, std::align_val_t alignment, const std::nothrow_t &value) noexcept {
    return ::operator new(size, alignment, value);
}
BUILDER_WEAK void operator delete(void *pointer) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete[](void *pointer) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete(void *pointer, std::size_t) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete[](void *pointer, std::size_t) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete(void *pointer, std::align_val_t) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete[](void *pointer, std::align_val_t) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete(void *pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}
BUILDER_WEAK void operator delete[](void *pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}

extern "C" {
BUILDER_WEAK void *__dso_handle = &__dso_handle;
BUILDER_WEAK int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
BUILDER_WEAK int __cxa_thread_atexit(void (*)(void *), void *, void *) { return 0; }
BUILDER_WEAK int __cxa_guard_acquire(unsigned long long *guard) {
    return *reinterpret_cast<unsigned char *>(guard) == 0;
}
BUILDER_WEAK void __cxa_guard_release(unsigned long long *guard) {
    *reinterpret_cast<unsigned char *>(guard) = 1;
}
BUILDER_WEAK void __cxa_guard_abort(unsigned long long *) {}
BUILDER_WEAK void __cxa_finalize(void *) {}
BUILDER_WEAK void __cxa_pure_virtual(void) { allocation_failed(); }
}

namespace std {
inline namespace __1 {
[[noreturn]] BUILDER_WEAK void __libcpp_verbose_abort(char const *, ...) noexcept {
    allocation_failed();
}
[[noreturn]] BUILDER_WEAK void terminate() noexcept { allocation_failed(); }
}
}
```

### 7.12. `runtime/xiangshan/startup.S`

<!-- file: runtime/xiangshan/startup.S sha256: 0dfad36809a3a9b5729a2b90bfc2f752d3001068ad8ff25e80c07e39df4551b1 -->
```asm
    .section .text.init
    .globl _start
    .type _start, @function
_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    csrw mie, zero
    csrw mip, zero
    la t0, 1f
    csrw mtvec, t0
    li t0, 0x00006600
    csrs mstatus, t0
    csrwi fcsr, 0
    csrwi vcsr, 0
    la sp, __stack_top
    la tp, __tls_base
    call __builder_start
1:  j 1b
```

### 7.13. `runtime/xiangshan/platform.c`

<!-- file: runtime/xiangshan/platform.c sha256: d80be7d71458d9f8bf65bd84449526ebb674bb86af05d322291f4fe6bf1a3012 -->
```c
#include <stdint.h>

#define UART_TX (*(volatile unsigned char *)0x40600004UL)
static uint64_t run_start_cycle;

uint64_t builder_platform_cycle(void) {
    uint64_t value;
    __asm__ volatile("rdcycle %0" : "=r"(value));
    return value;
}
void builder_platform_init(void) { run_start_cycle = builder_platform_cycle(); }
long builder_platform_write(const void *buffer, unsigned long count) {
    const unsigned char *bytes = buffer;
    for (unsigned long index = 0; index < count; ++index) UART_TX = bytes[index];
    return (long)count;
}
static void write_text(const char *text) {
    while (*text) UART_TX = (unsigned char)*text++;
}
static void write_decimal(uint64_t value) {
    char buffer[32];
    unsigned int position = sizeof(buffer);
    do {
        buffer[--position] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    builder_platform_write(buffer + position, sizeof(buffer) - position);
}
void builder_platform_finish(int code) {
    write_text("KC=");
    write_decimal(builder_platform_cycle() - run_start_cycle);
    write_text(code == 0 ? "\nPASSED\n" : "\nFAILED\n");
    register uint64_t status __asm__("a0") = (uint64_t)(unsigned int)code;
    __asm__ volatile(".word 0x0000006b" : : "r"(status));
    for (;;) {}
}
```

### 7.14. `runtime/xiangshan/link.ld`

<!-- file: runtime/xiangshan/link.ld sha256: b0439f320b09b234dca24a8721f0c385cd822a3c8de5cd31c8353f4840bde536 -->
```ld
OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS {
    . = 0x80000000;
    .text.init : { KEEP(*(.text.init)) }
    .tohost ALIGN(0x1000) : { *(.tohost) }
    .text ALIGN(0x1000) : { *(.text .text.*) }
    .rodata ALIGN(0x1000) : { *(.rodata .rodata.*) *(.srodata .srodata.*) }
    .preinit_array ALIGN(16) : {
        __preinit_array_start = .; KEEP(*(.preinit_array .preinit_array.*)); __preinit_array_end = .;
    }
    .init_array ALIGN(16) : {
        __init_array_start = .; KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) .init_array)); __init_array_end = .;
    }
    .fini_array ALIGN(16) : {
        __fini_array_start = .; KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) .fini_array)); __fini_array_end = .;
    }
    .data.rel.ro ALIGN(16) : { *(.data.rel.ro .data.rel.ro.*) }
    .got ALIGN(16) : { *(.got .got.*) }
    PROVIDE(__global_pointer$ = . + 0x800);
    .data ALIGN(0x1000) : { *(.data .data.*) *(.sdata .sdata.*) }
    .bss ALIGN(0x1000) (NOLOAD) : {
        __bss_start = .; *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON); __bss_end = .;
    }
    .tdata ALIGN(16) : { __tls_base = .; *(.tdata .tdata.*) }
    .tbss ALIGN(16) (NOLOAD) : { __tbss_start = .; *(.tbss .tbss.*) *(.tcommon); __tbss_end = .; }
    . = ALIGN(16);
    __heap_start = .;
    __heap_end = 0x87f00000;
    __stack_top = 0x88000000;
    _end = .;
    PROVIDE(end = .);
}
```

### 7.15. `include/sched.h`

<!-- file: include/sched.h sha256: 6641fc2fcb0149698032a9f9924552123ce2e0ae72eced1ce1e743326a3440e4 -->
```text
#ifndef CONTAINER_IMAGES_BUILDER_SCHED_H
#define CONTAINER_IMAGES_BUILDER_SCHED_H
#define SCHED_OTHER 0
#define SCHED_FIFO 1
struct sched_param { int sched_priority; };
static inline int sched_get_priority_max(int policy) { (void)policy; return 0; }
static inline int sched_setscheduler(int pid, int policy, const struct sched_param *parameter) {
    (void)pid; (void)policy; (void)parameter; return 0;
}
#endif
```

### 7.16. `include/sys/resource.h`

<!-- file: include/sys/resource.h sha256: 4dfd6740ced46b1fb7494d3728d9f751b9109be8665fe97e869efa51fd415dc6 -->
```text
#ifndef CONTAINER_IMAGES_BUILDER_SYS_RESOURCE_H
#define CONTAINER_IMAGES_BUILDER_SYS_RESOURCE_H
#define RUSAGE_SELF 0
struct rusage { long ru_utime; long ru_stime; };
static inline int getrusage(int who, struct rusage *usage) { (void)who; (void)usage; return 0; }
#endif
```

### 7.17. `include/malloc.h`

<!-- file: include/malloc.h sha256: 4c01b81d793f1f947e830525d8f745168521ae077519abbde9d1733324257b38 -->
```text
#ifndef CONTAINER_IMAGES_BUILDER_MALLOC_H
#define CONTAINER_IMAGES_BUILDER_MALLOC_H
#include <stddef.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
void *memalign(size_t alignment, size_t size);
#ifdef __cplusplus
}
#endif
#endif
```

### 7.18. `licenses/TSVC.txt`

<!-- file: licenses/TSVC.txt sha256: 19a1ed5a00c18e907cedcaa7b89cf68b46711d039f49ad30136675656b466d78 -->
```text
Copyright (c) 2011 University of Illinois at Urbana-Champaign.  All rights reserved.



Developed by: Polaris Research Group

              University of Illinois at Urbana-Champaign

              http://polaris.cs.uiuc.edu



Permission is hereby granted, free of charge, to any person obtaining a copy

of this software and associated documentation files (the "Software"), to

deal with the Software without restriction, including without limitation the

rights to use, copy, modify, merge, publish, distribute, sublicense, and/or

sell copies of the Software, and to permit persons to whom the Software is

furnished to do so, subject to the following conditions:

  1. Redistributions of source code must retain the above copyright notice,

     this list of conditions and the following disclaimers.

  2. Redistributions in binary form must reproduce the above copyright

     notice, this list of conditions and the following disclaimers in the

     documentation and/or other materials provided with the distribution.

  3. Neither the names of Polaris Research Group, University of Illinois at

     Urbana-Champaign, nor the names of its contributors may be used to endorse

     or promote products derived from this Software without specific prior

     written permission.



THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR

IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,

FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE

CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER

LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING

FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS

WITH THE SOFTWARE.

```

### 7.19. `licenses/PolyBench-README.txt`

<!-- file: licenses/PolyBench-README.txt sha256: 5660cbc4b885688d3f063699a6b9f4e4ac7eea54924092635830671036b398ff -->
```text
* * * * * * * * * *
* PolyBench/C 3.2 *
* * * * * * * * * *

Copyright (c) 2011-2012 the Ohio State University.
Contact: Louis-Noel Pouchet <pouchet@cse.ohio-state.edu>


-------------
* New in 3.2:
-------------

- Rename the package to PolyBench/C, to prepare for the upcoming
  PolyBench/Fortran and PolyBench/GPU.
- Fixed a typo in polybench.h, causing compilation problems for 5D arrays.
- Fixed minor typos in correlation, atax, cholesky, fdtd-2d.
- Added an option to build the test suite with constant loop bounds
  (default is parametric loop bounds)


-------------
* New in 3.1:
-------------

- Fixed a typo in polybench.h, causing compilation problems for 3D arrays.
- Set by default heap arrays, stack arrays are now optional.


-------------
* New in 3.0:
-------------

- Multiple dataset sizes are predefined. Each file comes now with a .h
  header file defining the dataset.
- Support of heap-allocated arrays. It uses a single malloc for the
  entire array region, the data allocated is cast into a C99
  multidimensional array.
- One benchmark is out: gauss_filter
- One benchmark is in: floyd-warshall
- PAPI support has been greatly improved; it also can report the
  counters on a specific core to be set by the user.



----------------
* Mailing lists:
----------------

** polybench-announces@lists.sourceforge.net:
---------------------------------------------

Announces about releases of PolyBench.

** polybench-discussion@lists.sourceforge.net:
----------------------------------------------

General discussions reg. PolyBench.



-----------------------
* Available benchmarks:
-----------------------

::linear-algebra::
linear-algebra/kernels:
linear-algebra/kernels/2mm/2mm.c
linear-algebra/kernels/3mm/3mm.c
linear-algebra/kernels/atax/atax.c
linear-algebra/kernels/bicg/bicg.c
linear-algebra/kernels/cholesky/cholesky.c
linear-algebra/kernels/doitgen/doitgen.c
linear-algebra/kernels/gemm/gemm.c
linear-algebra/kernels/gemver/gemver.c
linear-algebra/kernels/gesummv/gesummv.c
linear-algebra/kernels/mvt/mvt.c
linear-algebra/kernels/symm/symm.c
linear-algebra/kernels/syr2k/syr2k.c
linear-algebra/kernels/syrk/syrk.c
linear-algebra/kernels/trisolv/trisolv.c
linear-algebra/kernels/trmm/trmm.c

linear-algebra/solvers:
linear-algebra/solvers/durbin/durbin.c
linear-algebra/solvers/dynprog/dynprog.c
linear-algebra/solvers/gramschmidt/gramschmidt.c
linear-algebra/solvers/lu/lu.c
linear-algebra/solvers/ludcmp/ludcmp.c

::datamining::
datamining/correlation/correlation.c
datamining/covariance/covariance.c

::medley::
medley/floyd-warshall/floyd-warshall.c
medley/reg_detect/reg_detect.c

::stencils::
stencils/adi/adi.c
stencils/fdtd-2d/fdtd-2d.c
stencils/fdtd-apml/fdtd-apml.c
stencils/jacobi-1d-imper/jacobi-1d-imper.c
stencils/jacobi-2d-imper/jacobi-2d-imper.c
stencils/seidel-2d/seidel-2d.c



------------------------------
* Sample compilation commands:
------------------------------


** To compile a benchmark without any monitoring:
-------------------------------------------------

$> gcc -I utilities -I linear-algebra/kernels/atax utilities/polybench.c linear-algebra/kernels/atax/atax.c -o atax_base


** To compile a benchmark with execution time reporting:
--------------------------------------------------------

$> gcc -O3 -I utilities -I linear-algebra/kernels/atax utilities/polybench.c linear-algebra/kernels/atax/atax.c -DPOLYBENCH_TIME -o atax_time


** To generate the reference output of a benchmark:
---------------------------------------------------

$> gcc -O0 -I utilities -I linear-algebra/kernels/atax utilities/polybench.c linear-algebra/kernels/atax/atax.c -DPOLYBENCH_DUMP_ARRAYS -o atax_ref
$> ./atax_ref 2>atax_ref.out




-------------------------
* Some available options:
-------------------------

They are all passed as macro definitions during compilation time (e.g,
-Dname_of_the_option).

- POLYBENCH_TIME: output execution time (gettimeofday) [default: off]

- POLYBENCH_NO_FLUSH_CACHE: don't flush the cache before calling the
  timer [default: flush the cache]

- POLYBENCH_LINUX_FIFO_SCHEDULER: use FIFO real-time scheduler for the
  kernel execution, the program must be run as root, under linux only,
  and compiled with -lc [default: off]

- POLYBENCH_CACHE_SIZE_KB: cache size to flush, in kB [default: 33MB]

- POLYBENCH_STACK_ARRAYS: use stack allocation instead of malloc [default: off]

- POLYBENCH_DUMP_ARRAYS: dump all live-out arrays on stderr [default: off]

- POLYBENCH_CYCLE_ACCURATE_TIMER: Use Time Stamp Counter to monitor
  the execution time of the kernel [default: off]

- POLYBENCH_PAPI: turn on papi timing (see below).

- MINI_DATASET, SMALL_DATASET, STANDARD_DATASET, LARGE_DATASET,
  EXTRALARGE_DATASET: set the dataset size to be used
  [default: STANDARD_DATASET]

- POLYBENCH_USE_C99_PROTO: Use standard C99 prototype for the functions.

- POLYBENCH_USE_SCALAR_LB: Use scalar loop bounds instead of parametric ones.



---------------
* PAPI support:
---------------

** To compile a benchmark with PAPI support:
--------------------------------------------

$> gcc -O3 -I utilities -I linear-algebra/kernels/atax utilities/polybench.c linear-algebra/kernels/atax/atax.c -DPOLYBENCH_PAPI -lpapi -o atax_papi


** To specify which counter(s) to monitor:
------------------------------------------

Edit utilities/papi_counters.list, and add 1 line per event to
monitor. Each line (including the last one) must finish with a ',' and
both native and standard events are supported.

The whole kernel is run one time per counter (no multiplexing) and
there is no sampling being used for the counter value.



------------------------------
* Accurate performance timing:
------------------------------

With kernels that have an execution time in the orders of a few tens
of milliseconds, it is critical to validate any performance number by
repeating several times the experiment. A companion script is
available to perform reasonable performance measurement of a PolyBench.

$> gcc -O3 -I utilities -I linear-algebra/kernels/atax utilities/polybench.c linear-algebra/kernels/atax/atax.c -DPOLYBENCH_TIME -o atax_time
$> ./utilities/time_benchmark.sh ./atax_time

This script will run five times the benchmark (that must be a
PolyBench compiled with -DPOLYBENCH_TIME), eliminate the two extremal
times, and check that the deviation of the three remaining does not
exceed a given threshold, set to 5%.

It is also possible to use POLYBENCH_CYCLE_ACCURATE_TIMER to use the
Time Stamp Counter instead of gettimeofday() to monitor the number of
elapsed cycles.




----------------------------------------
* Generating macro-free benchmark suite:
----------------------------------------

(from the root of the archive:)
$> PARGS="-I utilities -DPOLYBENCH_TIME";
$> for i in `cat utilities/benchmark_list`; do create_cpped_version.sh $i "$PARGS"; done

This create for each benchmark file 'xxx.c' a new file
'xxx.preproc.c'. The PARGS variable in the above example can be set to
the desired configuration, for instance to create a full C99 version
(parametric arrays):

$> PARGS="-I utilities -DPOLYBENCH_USE_C99_PROTO";
$> for i in `cat utilities/benchmark_list`; do ./utilities/create_cpped_version.sh "$i" "$PARGS"; done


```

### 7.20. `licenses/PolyBench-AUTHORS.txt`

<!-- file: licenses/PolyBench-AUTHORS.txt sha256: 068b9b4e966141725cfff58147ac4b0ccc11789506fb6d9bfed86f4ffd961798 -->
```text
* * * * * * * * * * * * *
* Authors of PolyBench  *
* * * * * * * * * * * * *


* Louis-Noel Pouchet <pouchet@cse.ohio-state.edu>
  Who provided packaging and harmonization of all test files,
  the PolyBench infrastructure and machinery, and several
  reference C files.

* Uday Bondugula <uday@csa.iisc.ernet.in>
  Who provided many of the original reference C files, including
  Fortran to C translation.

```

### 7.21. `licenses/onnxruntime-MIT.txt`

<!-- file: licenses/onnxruntime-MIT.txt sha256: 2f07c72751aed99790b8a4869cf2311df85a860b22ded05fa22803587a48922c -->
```text
MIT License

Copyright (c) Microsoft Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### 7.22. `licenses/XiangShan-MulanPSL-2.0.txt`

<!-- file: licenses/XiangShan-MulanPSL-2.0.txt sha256: 0c62f4cddf9b93d11a065fd736584f6711d5a7862fc68696a8a1634b9a275bf7 -->
```text
Copyright (c) 2020-2021 Institute of Computing Technology,
Chinese Academy of Sciences.

XiangShan is licensed under Mulan PSL v2. You can use this software according
to the terms and conditions of the Mulan PSL v2. You may obtain a copy at:

    https://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See Mulan PSL v2 for details.
```


## 8. LLVM 소스 근거와 검증 기록

분석한 로컬 LLVM HEAD는 측정 manifest와 같은 SHA이며 작업 트리 변경이 없었다. 아래 링크는 그 revision에 고정되어 있다.

- [LoopVectorize.cpp: maximum scalable VF와 tail folding 결정](https://github.com/cheolwanpark/llvm-project/blob/89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb/llvm/lib/Transforms/Vectorize/LoopVectorize.cpp#L2998)
- [RISCVTargetTransformInfo.cpp: SK_Splice 비용](https://github.com/cheolwanpark/llvm-project/blob/89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb/llvm/lib/Target/RISCV/RISCVTargetTransformInfo.cpp#L993)
- [RISCVISelLowering.cpp: LMUL/slide 비용](https://github.com/cheolwanpark/llvm-project/blob/89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb/llvm/lib/Target/RISCV/RISCVISelLowering.cpp#L3543)
- [RISCVTargetTransformInfo.cpp: gather와 strided-memory 비용](https://github.com/cheolwanpark/llvm-project/blob/89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb/llvm/lib/Target/RISCV/RISCVTargetTransformInfo.cpp#L1303)
- [RISCVTargetTransformInfo.cpp: arithmetic reduction 비용](https://github.com/cheolwanpark/llvm-project/blob/89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb/llvm/lib/Target/RISCV/RISCVTargetTransformInfo.cpp#L2227)

문서 작성·검증일: 2026-09-10.

- 36개 historical measurement에 대해 `summary.csv`, 개별 `result.json`, stdout `MB_ROI`의 cycle 값이 일치함을 확인했다. 같은 로그에서 RTL revision과 seed도 확인했다.
- kernel C 원문 두 개의 SHA-256이 측정 manifest와 일치함을 확인했다.
- 문서에 적힌 추출 명령을 실제로 실행하여 22개 파일을 복원했고, 모든 파일이 내장 전 입력과 byte-identical함을 확인했다.
- 추출한 `build.py --dry-run`의 36개 kernel compilation 명령을 원래 `xiangshan.command.json`과 비교했다. source/output/include의 로컬 경로를 제외한 compiler flags가 모두 일치했다.
- 추출한 Python 스크립트 세 개의 문법을 검사했고, host Clang 22.1.8로 TSVC 5개와 application 5개 case의 C syntax를 검사했다. RoPE syntax도 확인했지만 측정 대상에는 포함하지 않는다.
- 추출한 `measured.csv`의 36개 cycle 값과 run ID를 원래 summary와 다시 대조했다.

Docker daemon이 실행 중이지 않아 새 Dockerfile의 image build, 새 wrapper를 통한 ELF link와 RTL 재실행은 이 문서 작성 중 수행하지 않았다. 위 검증은 source/metadata/command 및 문법 검증이며, 새 재현 환경의 end-to-end 실행 성공을 주장하지 않는다. 표의 숫자는 기존에 완료된 실제 RTL 측정값이다.

