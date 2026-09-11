# TP/LCD 비용 모델 구현 및 실험 보고서

코드 커밋: `568d27324e338882ad46639c68f0f59eb0138172`
게시 브랜치: `vplan-cost-adjustment`
구현 시작 revision: `ca7933e47d3a3451d81e72ac174dcb5aa28b59d1`
측정 자료: 저장소 루트의 `VF_IC1_SWP_OFF_REPORT.md`만 사용

이 보고서는 compiler가 출력한 후보 점수와 기존 측정값의 비교 결과다.
새 바이너리의 실측 성능을 주장하지 않는다. 핵심 7개와 BiCG/GEMVER를
분리 평가했으며, 모든 보정은 **validation-informed**다.
Saturn은 독립 software profile과 컴파일 검증만 수행했다.

## 결과

| 그룹 | 실제 자동 선택 regret GM / 최악 | Non-tie 정확도 | 상대 오차 GM / P90 |
|---|---:|---:|---:|
| 핵심 7개 | 1.015112 / 1.093117 | 94.8718% (37/39) | 1.118502 / 1.246743 |
| BiCG/GEMVER | 1.000000 / 1.000000 | 100% (11/11) | 1.106704 / 1.188672 |
| 목표 | ≤1.05 / ≤1.15 | ≥90% | ≤1.15 / ≤1.30 |

36개 forced 후보와 실제 expanded automatic 9개를 모두 컴파일했다.
각 automatic search가 scalable VF2/4/8/16을 모두 방문했음을 진단으로 확인했다.
Forced argmin과 실제 automatic 선택은 별도로 수집했으며 최종 결과에서 일치한다.

검증 결과: RISC-V cost-model/vectorizer lit **169개**, Vectorize unit
**80개**, Python **17개** 통과. 최종 코드의 compiler/source hash는
수집 provenance와 대조했다.

## 초기 TTI와 coefficient

수정 전 LLVM에는 이번 공통 profile의 beat/startup coefficient가 없었다.
예를 들어 `<vscale x 4 x float>`, 추정 vscale=2에서 다음과 같다.
각 셀은 throughput / latency다.

| 연산 | 수정 전 TTI | 새 profile 초기 | 최종 XiangShan |
|---|---:|---:|---:|
| Vector FAdd | 4 / 3 | 2 / 5 | 2 / 7 |
| 연속 vector load | 2 / 4 | 2 / 5 | 1 / 6 |
| FMA intrinsic | 2 / 2 | 2 / 5 | 2 / 7 |
| Vector splice | 4 / 4 | 4 / 10 | 22 / 32 |

새 profile은 공통 beat128과 latency startup3, 숫자 두 개로 시작했다.

| 항목 | 초기 | 최종 XiangShan | 최종 Saturn |
|---|---:|---:|---:|
| FP beat bits | 128 | 128 | 128 |
| Memory/reference beat bits | FP와 공유128 | 256 | 128 |
| 공통 latency startup | 3 | 5 | 3 |
| Slide startup | 별도 항 없음 | 8 | 0 |
| Slide quadratic coefficient | 선형 | 3 | 0: 선형 |
| Loaded-index startup | 없음 | 공통5 재사용 | 없음 |

최종 profile당 숫자 필드는5개다. FP width는 고정했고 나머지4개를 보정했다.
Beat당 기본 비용은1이다. 이 숫자는 software cost 가설이며 실제 하드웨어
폭이나 물리 cycle을 입증한 값이 아니다. LLVM 기본 동작은 model=off,
profile=none을 유지한다.

## 관찰에 따른 변경

| 단계 | 관찰과 조치 | 핵심 상대 오차 GM / P90 |
|---|---|---:|
| 초기 완전 coverage | 공통 beat128/startup3 | 1.42958 / 1.81490 |
| Memory width 분리 | s000 VF4/VF2 예측0.75 vs 측정0.38361 등 감소량 부족. Memory만128→256 | 1.26818 / 1.50262 |
| FMA accounting | VPlan multiply/add 두 개가 실제 assembly에서 FMA 하나로 결합됨. 조건을 증명한 pair만 native 비용 적용 | 1.25003 / 1.40005 |
| 비선형 slide | Jacobi VF16/VF8 예측0.9375 vs 측정1.45738. Spill/LCD 원인이 아니므로 공통 slide 가설 검토 | 1.20072 / 1.33144 |
| Startup3→5 | s311 accumulator recurrence의 상대 scale 잔차. 기존 latency coefficient 조정 | 1.14544 / 1.30340 |
| 모든 irregular access에 startup | affine strided store까지 포함해 오차 악화. 폐기 | 1.15322 / 1.30633 |
| Loaded-index만 startup | Load에서 유래한 GEP index에만 기존5 추가 | 1.11850 / 1.24674 |
| Full-EVL 증명 | 작은 VF의 automatic plan에 남은 EVL/merge 항 제거. GESUMMV8→4 | forced 수치 유지 |
| Scalar register 증명 | s1111 주소 체인과 scalar EVL의 과대 vector pressure 수정. VF16 선택 가능 | forced 수치 유지 |

Slide는 startup{4,8,12,16} × quadratic{1,2,3,4}의16개 점을 모두
평가했고8/3을 선택했다. Latency startup은{3,4,5,6}을 평가해5를 선택했다.
모든 trial과 실패·악화 결과를 보존했다. FMA/EVL/register 변경에는 새로운
숫자 coefficient를 추가하지 않았다.

## Compare 계산식

VF는 scalable coefficient다. 추정 runtime lane 수는 `R=2*VF`로,
VF2/4/8/16에서4/8/16/32다. VLEN128은 최소값이며 exact VLEN 가정이 아니다.

```text
FP beats     = ceil(32 * R / 128) = [1, 2, 4, 8]
Memory beats = ceil(32 * R / 256) = [1, 1, 2, 4]
FP TP        = FP beats
FP latency   = FP beats + 5
Memory TP    = Memory beats
Memory lat   = Memory beats + 5
Slide TP     = 8 + 3 * Memory beats^2       (primitive slide 하나)
Slide lat    = Slide TP + 5

M = 루프 전체 Memory throughput 합
C = 루프 전체 Compute throughput 합
O = Other 비용
TP = O + max(M, C)
LCD = max(닫힌 recurrence latency 합 / iteration distance)
compare = max(TP, LCD) / R                 (작을수록 유리)
```

연속 memory throughput에는 startup5를 더하지 않는다. Loaded-index
irregular memory throughput에만 기존 per-lane 비용에5를 더하며, VPlan
memory recipe의 주소 계산 비용도 남는다. 기타 integer/cast/control 비용은
기존 TTI/precompute 정책을 유지한다.

LCD는 distance1~3의 닫힌 cycle만 포함한다. Feed-forward chain은 LCD가
되지 않으며 독립 accumulator는 합이 아닌 최대값을 쓴다. 이번 후보에서
지배적인 recurrence distance는 모두1이다. 지원하지 않는 메모리 의존성,
recipe latency, nested/replicated region에는 명시적인 fallback이 있다.

## 벤치마크별 세부 항과 실제 비교값

M/C/O/TP/LCD는 vector iteration당 값이고 compare만 R로 나눈 값이다.
최종36개 domain/score tuple은 forced와 automatic search에서 일치한다.
선택 자격은 별도이며 GESUMMV16은 실제 register pressure로 제외된다.

| Benchmark | VF | M | C | O | TP | LCD | Compare | 실제 선택/제외 |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| s000 | 2 | 2 | 1 | 2 | 4 | 1 | 1 |  |
| s000 | 4 | 2 | 2 | 2 | 4 | 1 | 0.5 |  |
| s000 | 8 | 4 | 4 | 3 | 7 | 3 | 0.4375 |  |
| s000 | 16 | 8 | 8 | 3 | 11 | 3 | 0.34375 | 선택 |
| s125 | 2 | 4 | 1 | 2 | 6 | 1 | 1.5 |  |
| s125 | 4 | 4 | 2 | 2 | 6 | 1 | 0.75 |  |
| s125 | 8 | 8 | 4 | 3 | 11 | 3 | 0.6875 |  |
| s125 | 16 | 16 | 8 | 3 | 19 | 3 | 0.59375 | 선택 |
| s311 | 2 | 1 | 1 | 2 | 3 | 6 | 1.5 |  |
| s311 | 4 | 1 | 2 | 2 | 4 | 7 | 0.875 |  |
| s311 | 8 | 2 | 8 | 3 | 11 | 10 | 0.6875 |  |
| s311 | 16 | 4 | 16 | 3 | 19 | 14 | 0.59375 | 선택 |
| s1111 | 2 | 9 | 7 | 3 | 12 | 1 | 3 |  |
| s1111 | 4 | 15 | 14 | 5 | 20 | 1 | 2.5 |  |
| s1111 | 8 | 30 | 28 | 10 | 40 | 3 | 2.5 |  |
| s1111 | 16 | 60 | 56 | 18 | 78 | 3 | 2.4375 | 선택 |
| s4112 | 2 | 14 | 3 | 2 | 16 | 1 | 4 |  |
| s4112 | 4 | 20 | 6 | 2 | 22 | 1 | 2.75 |  |
| s4112 | 8 | 35 | 12 | 3 | 38 | 3 | 2.375 |  |
| s4112 | 16 | 65 | 25 | 3 | 68 | 3 | 2.125 | 선택 |
| jacobi-2d-imper-l00 | 2 | 5 | 27 | 3 | 30 | 1 | 7.5 |  |
| jacobi-2d-imper-l00 | 4 | 5 | 32 | 3 | 35 | 1 | 4.375 |  |
| jacobi-2d-imper-l00 | 8 | 10 | 60 | 4 | 64 | 3 | 4 | 선택 |
| jacobi-2d-imper-l00 | 16 | 20 | 152 | 4 | 156 | 3 | 4.875 |  |
| gesummv-l00 | 2 | 3 | 2 | 2 | 5 | 6 | 1.5 |  |
| gesummv-l00 | 4 | 3 | 4 | 2 | 6 | 7 | 0.875 | 선택 |
| gesummv-l00 | 8 | 6 | 16 | 3 | 19 | 10 | 1.1875 |  |
| gesummv-l00 | 16 | 12 | 32 | 3 | 35 | 14 | 1.09375 | register pressure 제외 |
| bicg-l01 | 2 | 4 | 2 | 2 | 6 | 6 | 1.5 |  |
| bicg-l01 | 4 | 4 | 4 | 2 | 6 | 7 | 0.875 |  |
| bicg-l01 | 8 | 8 | 12 | 3 | 15 | 10 | 0.9375 |  |
| bicg-l01 | 16 | 16 | 24 | 3 | 27 | 14 | 0.84375 | 선택 |
| gemver-l00 | 2 | 4 | 2 | 2 | 6 | 1 | 1.5 |  |
| gemver-l00 | 4 | 4 | 4 | 2 | 6 | 1 | 0.75 |  |
| gemver-l00 | 8 | 8 | 8 | 3 | 11 | 3 | 0.6875 |  |
| gemver-l00 | 16 | 16 | 16 | 3 | 19 | 3 | 0.59375 | 선택 |

### 선택된 후보의 recipe 구성

| Benchmark/VF | Memory | Compute | Other | LCD |
|---|---|---|---|---:|
| s000/16 | load4+store4=8 | FAdd8 | induction1+exit1+EVL1=3 | 3 |
| s125/16 | 3loads×4+store4=16 | FMA8 | 3 | 3 |
| s311/16 | load4 | FAdd8+vp.merge8=16 | 3 | 14 |
| s1111/16 | 3loads×4+strided-store recipe48=60 | 결합 후 FP40+integer shift16=56 | induction precompute16+exit1+EVL1=18 | 3 |
| s4112/16 | 3연속 access×4+indexed-load recipe53=65 | FMA8+index sext17=25 | 3 | 3 |
| Jacobi/8 | 4loads×2+store2=10 | FP5개×4+splice40=60 | induction1+exit1+EVL1+scalar 주소add1=4 | 3 |
| GESUMMV/4 | 3loads×1=3 | 2FMA×2=4 | induction1+exit1=2 | 7 |
| BiCG/16 | 3loads×4+store4=16 | 2FMA×8+vp.merge8=24 | 3 | 14 |
| GEMVER/16 | 3loads×4+store4=16 | 2FMA×8=16 | 3 | 3 |

- s311/GESUMMV/BiCG의 LCD는 VF2/4에서 FP latency6/7,
  VF8/16에서 FP latency9/13+merge latency1로10/14다.
- Jacobi splice는 primitive slide 두 개다. VF8에서2×(8+3×2²)=40,
  VF16에서2×(8+3×4²)=112다. 따라서 compare4와4.875로 VF8이 선택된다.
  Load-fed splice 자체를 closed LCD로 바꾸지 않았다.
- s4112의 indexed-load recipe53은 per-lane32+startup5+주소16이다.
  Sext17은 기존 TTI legalization 비용이다.
- s1111의 strided-store recipe48은 per-lane32+주소16이다. Scalar register
  보정은 pressure 판정에 적용했고 throughput의 기존 주소/induction 항을
  모두 scalar 명령 비용으로 교체하지는 않았다. 실제 물리 시간으로 해석하면
  안 되는, 보수적 비용 항이 남는다.
- GESUMMV automatic VF4에서 full-EVL 증명으로 EVL1+merge2+merge2=5를
  제거했다. 최종 LCD7이 TP6보다 커 compare7/8=0.875다.

## Baseline, ablation과 남은 한계

Pristine forced legacy argmin의 핵심 regret GM/worst는1.109637/1.457382,
relative GM/P90는1.570488/2.098064였다. Pristine 실제 expanded automatic
regret은1.085638/1.457382다. Default automatic baseline은 s1111이 fixed8을
선택해 측정 후보 집합 밖이므로 해당 aggregate가 undefined다.

최종 FMA-off는 non-tie89.7436%, relative1.15307/1.30633으로 악화된다.
Loaded-index-off는 relative1.14544/1.30340, full-EVL-off는 실제 최악
regret1.16108, scalar-register-off는1.22629다. 네 ablation 모두 전체45개
compiler job을 다시 실행했다. TP-only/LCD-only는 compiler가 출력한 실제
component를 이용한 evaluator ablation이며 별도 automatic 정책 실행은 아니다.

모든36개 frozen XiangShan forced assembly는 새 pristine forced assembly와
byte-identical하다. 실제 automatic은 s1111/Jacobi/GESUMMV의 선택이 달라졌다.
9개 중8개는 같은 VF의 forced function body와 동일하다. GESUMMV4는
벡터 연산 수는 같지만 스케줄과 counter가 다르고 scalar sub가 하나 더 있다.
이 automatic binary는 실측 미검증이다.

현재 GESUMMV16은 두 scalable stack slot과 vector spill/reload가 있다.
Historical report의 no-spill 코드와 다르며 실제 pressure exclusion을 유지한다.
현재 s4112 VF16의 loop vset 수는6으로 historical4와 다르다. Report에는
전체 historical assembly/ELF가 없으므로 pristine 일치가 historical binary
identity의 증명은 아니다.

| Adjacent gap | 예측 비율 | 측정 비율 |
|---|---:|---:|
| Jacobi4→8 | 0.914286 | 0.726043 |
| Jacobi8→16 | 1.218750 | 1.457382 |
| GESUMMV4→8 | 1.357143 | 1.062174 |
| GESUMMV8→16 | 0.921053 | 0.861267 |

지정된 aggregate 기준은 모두 통과했지만 개별 gap 크기는 정확하지 않다.
새 binary timing, Saturn 하드웨어/성능, absolute cycle 정확도는 검증하지 않았다.

## 산출물과 재현

- [전체 최종 기술 보고서](FINAL_REPORT.md), [완료 감사](completion-audit.json)
- [최종 coefficients](frozen-model.json), [초기 구조](initial-model-structure.json)
- [36개 후보](final-report/candidates.csv), [실제 선택](final-report/selections.csv)
- [모든 automatic 후보/제외](final-report/automatic-candidates.csv)
- [상대 오차](final-report/relative.csv), [pair](final-report/pairs.csv), [gap](final-report/gaps.csv)
- [모든 round와 ablation](final-report/rounds-and-ablations.csv), [작업 이력](WORK_LOG.md)
- [Code shape/allocator evidence](final-report/code-shape.csv)
- [최종 compiler/source provenance](frozen-xiangshan/provenance.json)
- [입력 hash](input-manifest.json), [header provenance](header-provenance.json)
- [최종 source manifest](final-source-manifest.json)

`frozen-xiangshan/`, `frozen-saturn/`, `frozen-none-observe/`는 각각45개 전체
산출물을 담는다. `baseline-v2/`는36forced+18automatic baseline이며, 이전
실패 collection과 모든 tuning trial도 보존했다. 원본 report와22개 embedded
input은 수정하지 않았다. 원본 입력에 포함된 라이선스와 provenance도 유지한다.

Git에는 scripts, 입력, JSON/CSV, logs, assembly/IR, workaround object 등을
포함한다. 기존 `.gitignore`의 `toolchain/`, `__pycache__/`는 로컬에만 남는다.
Compiler 실행 파일과 header package는 build/preparation scripts 및 hash로
재현하며 Git에 포함된 실행 파일로 오해하지 않아야 한다. 최초 실험 provenance의
absolute path와 starting SHA는 당시 기록으로 보존했다.

준비 과정은 [README](README.md)를 따른다. 새 호스트에서는 우선 시작 revision의
clean checkout에서 pristine compiler와 baseline을 생성하고, 코드 변경을 적용해
modified compiler를 build한다. 기존 원본 입력 디렉터리와 수집 결과는 덮어쓰지 않는다.
준비가 끝난 뒤 저장소 루트에서:

```sh
cmake --build build-tp-lcd-main --target clang opt VectorizeTests --parallel 3
python3 experiments/tp-lcd/collect_model.py --out NEW_XIANGSHAN_DIRECTORY
python3 experiments/tp-lcd/evaluate.py NEW_XIANGSHAN_DIRECTORY/scores.json --out NEW_EVALUATION_DIRECTORY
python3 experiments/tp-lcd/collect_model.py --profile saturn --out NEW_SATURN_DIRECTORY
python3 experiments/tp-lcd/collect_model.py --profile none --model observe --out NEW_NONE_DIRECTORY
build-tp-lcd-main/bin/llvm-lit -v llvm/test/Transforms/LoopVectorize/RISCV llvm/test/Analysis/CostModel/RISCV
build-tp-lcd-main/unittests/Transforms/Vectorize/VectorizeTests
python3 -m unittest discover -s experiments/tp-lcd -p 'test_*.py' -v
```

`final_report.py`는 보존된 collection으로 최종 CSV와 metric audit를 생성한다.
출력 `final-report/`가 이미 있으면 별도 경로로 보존한 뒤 실행한다.
새 호스트의 실행 경로는 새 baseline 수집에서 기록되며, 기존 호스트의 저장된
absolute command 경로를 그대로 실행하는 방식은 사용하지 않는다.
