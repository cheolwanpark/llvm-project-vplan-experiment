# TP/LCD cost model — 소스 이관 없이 clean local main에서 새로 구현할 Goal 프롬프트

새 환경의 **clean worktree, 현재 checkout된 로컬 `main`의 HEAD**에서 아래 목표를 Goal로 등록하고 구현·평가·보정을 진행해줘. 이미 같은 Goal이 활성화되어 있으면 중복 등록하지 않는다. 별도 token budget은 지정하지 않는다. 로컬 `main`의 실제 파일과 commit을 권위 있는 출발점으로 삼는다. **이전 작업의 수정 소스, patch, 실험 디렉터리, scripts, build 결과는 전혀 이관되지 않는다. 이 프롬프트와 `VF_IC1_SWP_OFF_REPORT.md` 두 문서만 제공된다.** 현재 소스의 API를 읽고 필요한 구현·실험 도구를 새로 작성하라. 과거 파일을 찾거나 이관을 요청하지 마라.

## 시작 revision과 branch

- Goal 시작 시 `git branch --show-current`는 `main`이고, `git rev-parse HEAD`와 `git rev-parse refs/heads/main`은 같아야 한다. 시작 상태는 clean이라고 가정하되 `git status --short`로 확인하고 예상하지 못한 변경은 보존한다.
- 이번 환경 준비에 사용할 remote commit은 `ca7933e47d3a3451d81e72ac174dcb5aa28b59d1` (`ca7933e4`)이다. 이 commit을 가리키는 remote branch가 없어도 아래 명령으로 **로컬 `main`을 해당 commit에 정확히 위치**시킬 수 있다.
- 이 명령은 clean destination worktree에서 Goal을 시작하기 전에 실행한다. `switch -C`는 기존 로컬 `main`의 위치를 지정 commit으로 바꾼다. remote ref를 push하거나 수정하지 않는다.

```sh
git fetch origin ca7933e47d3a3451d81e72ac174dcb5aa28b59d1 &&
git switch -C main FETCH_HEAD

git branch --show-current
git rev-parse HEAD
```

- SHA를 가져오는 작업이므로 branch를 따라가는 `git pull` 대신 `fetch`를 사용한다. `origin/main`이 이 commit을 가리킨다고 가정하거나 이후 임의의 `git pull`로 다른 revision으로 이동하지 마라.
- 준비가 끝난 뒤 Goal은 **그 시점의 로컬 `main` HEAD**를 기준으로 작업한다. 실제 시작 SHA를 provenance에 기록하고, 자동으로 다른 branch를 만들거나 detached HEAD/과거 revision으로 전환하지 마라.
- 보고서의 historical compiler revision `89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb`는 측정 provenance다. 현재 구현의 시작 revision과 다르며, 이 SHA로 주 worktree를 되돌리는 지시가 아니다.

## 사용자가 확정한 우선순위와 현재 범위

- **정확한 absolute cycle 예측은 목표가 아니다.** VF/LMUL 후보의 ranking, 실제 선택의 regret, 같은 kernel 안에서의 상대 scale과 후보 사이 gap이 중요하다. absolute cycle 오차를 줄이기 위해 작업을 확장하거나 파라미터를 늘리지 마라.
- benchmark·measurement 자료는 **주어진 `VF_IC1_SWP_OFF_REPORT.md`만** 사용한다. 보고서에서 추출한 입력과 이 입력으로 생성한 compiler 결과는 사용할 수 있다. 인접 저장소, 기존 `historical/` 복사본, 다른 측정 자료를 새 근거로 사용하지 마라.
- 현재 단계에서는 simulator/RTL endpoint 탐색, SSH tunnel 조사, Saturn 측정 확보, RTL/QEMU 실행을 하지 마라. 필요한 compiler/build 도구와 header/sysroot 설치는 이 제한과 별개다.
- XiangShan과 Saturn의 소프트웨어 tuning profile은 독립적으로 선택되도록 구현한다. 보고서에 없는 Saturn 실측 보정·검증은 보류하고, 미검증임을 명시한다. 이를 이유로 가능한 XiangShan 구현·평가를 중단하지 마라.
- BiCG/GEMVER의 최초 freeze 후 검증은 이미 수행되었고, 이후 그 결과를 본 상태에서 FMA accounting 수정이 시작되었다. 환경을 바꾸어도 이 이력은 초기화되지 않는다. 이후 결과를 untouched/blind validation이라고 부르지 마라.

## 목표

`VF_IC1_SWP_OFF_REPORT.md`의 benchmark와 measured cycles를 기준으로, LLVM Loop Vectorizer에 가장 단순한 TP/LCD 기반 ranking cost model을 구현하라. XiangShan과 Saturn의 RISC-V TTI tuning profile을 구분하고, 소수의 설명 가능한 타깃별 비용 파라미터를 조정하여 VF/LMUL 후보의 ranking과 상대 cycle 비율을 유용한 수준으로 예측하는 것이 목표다.

정확한 cycle simulator를 만들려는 작업이 아니다. 공통 middle-end 모델은 작게 유지하고, 타깃 차이는 가능한 한 기존 TTI의 `TCK_RecipThroughput` / `TCK_Latency` 구현과 타깃별 tuning 값으로 표현하라. benchmark별 예외로 숫자를 맞추지 마라.

## 기준 데이터와 실험 조건

- 먼저 저장소 지침과 보고서를 읽고 현재 소스, build 도구, 보고서 내 측정 자료를 확인하라. 이전 실험 산출물이 없는 것은 정상적인 시작 조건이다. 사용자의 기존 파일과 변경을 보존하라. 이전 환경의 실행 파일·절대 경로·process handle을 새 환경에서 재사용하지 마라.
- 보고서의 내장 파일을 별도 실험 디렉터리에 추출하고 SHA-256을 검증하라. 원본 보고서와 historical measurements는 수정하지 마라.
- XiangShan 기준은 `xiangshan-kunminghu`, IC=1, SWP=off, f32, scalable VF={2,4,8,16}, 실제 VLEN=128이다. 이 조건에서 vscale=2, 실제 처리 폭은 {4,8,16,32}, f32 LMUL은 {m1,m2,m4,m8}이다. VF=2는 scalar가 아니다.
- N=4096, ROI 내부 호출 32회, 배열 배치, 전체 translation-unit compilation context, IC=1/SWP=off 및 reduction workaround를 유지하라. 보고서의 원래 compiler revision·옵션은 historical provenance로 보존하고, 새 compiler는 로컬 `main`의 시작 HEAD와 그 위의 변경으로 build하라. 지원하지 않는 옵션/API가 있으면 동등한 정책을 소스에서 확인하고 차이를 기록하라. min-VLEN 조건과 exact-VLEN 실험을 혼합하지 마라.
- 정답은 `raw_roi_cycles`이며 `KC`, simulator 전체 cycle, host 실행 시간을 대신 사용하지 마라. useful iteration당 cycle의 분모는 131072이다. ROI의 32회 호출은 독립 측정 32개가 아니다.
- 핵심 7개는 s000, s125, s311, s1111, s4112, jacobi-2d-imper-l00, gesummv-l00이다. bicg-l01과 gemver-l00은 별도 평가 그룹으로 유지하고 핵심 7개 결과와 구분하라. 원래 tuning에서 제외하고 freeze 후 검증했지만, 현재는 검증 결과를 본 상태다. 이후 공통 모델 수정·재튜닝은 validation-informed로 표시하라.
- 현재 보고서에는 XiangShan 36개 측정만 있다. 현재 범위에서 Saturn의 실제 VLEN, datapath, host core, 실행 구성은 미확인이다. 소프트웨어 profile의 가정값을 실제 hardware configuration으로 표시하지 마라. 사용자가 나중에 자료 범위를 넓힐 때만 Saturn 실측 평가를 재개하라.
- XiangShan cycle을 Saturn label로 재사용하거나 추정값을 실측으로 표시하지 마라. Saturn 보정·실측 검증이 완료됐다고 주장하지 마라.

## 가장 단순한 공통 모델

벡터 루프 한 iteration에 대해 다음을 계산하고, 후보 ranking에는 useful element당 점수를 사용하라.

```text
TP(VF) = OtherTP(VF) + max_d sum_{recipe in d} ThroughputCost(recipe, VF)
LCD(VF) = 반복 dependency cycle의 latency / iteration distance 근사
LoopScore(VF) = max(TP(VF), LCD(VF))
RankScore(VF) = LoopScore(VF) / EstimatedRuntimeVF
```

### TP

- 도메인은 처음에는 Memory / Compute처럼 적게 시작하라. Integer/FP/Permute 분리는 실제 공통 오차를 설명하고 검증 결과를 개선할 때만 추가하라. 물리 실행 포트를 재현하지 마라.
- recipe 종류와 opcode로 분류하라. `fcmp`처럼 결과 타입만으로 잘못 분류되는 경우와 scalar로 실행하는 recipe를 구분하라.
- 기존 recipe/TTI throughput 비용을 최대한 재사용하라. 복합 recipe의 분해가 어려우면 OtherTP로 더하되 분류 coverage를 출력하라.
- 도메인 비용은 전체 vector loop region에서 합산한 뒤 한 번만 max를 취하라. preheader/exit의 validity 검사, legacy precompute, skip 처리 때문에 누락·중복되지 않게 하라.
- 기존 reciprocal-throughput 비용은 capacity를 이미 반영할 수 있다. 최초 capacity는 1로 간주하고 타깃별 처리량은 TTI에 반영하라. 별도 capacity/weight가 꼭 필요하면 의미와 단위를 명시하고 같은 효과를 TTI와 두 번 보정하지 마라.

### LCD

- recipe/VPValue의 def-use와 header PHI의 backedge로 분석용 dependency graph를 구성하라. 실제 VPlan 복제나 code-generation unroll은 하지 마라.
- 최대 3 iteration 거리를 탐색하는 간단한 longest-path 분석으로 시작하라. 같은 iteration의 operand edge와 이전 iteration의 backedge edge를 구분하고, 같은 recurrence 값으로 돌아오는 경로에 대해 latency / iteration distance를 계산하라.
- `CP(3)/3` 같은 전체 unrolled critical-path 평균을 LCD로 쓰지 마라. iteration 간 독립적인 긴 체인을 LCD로 오인하지 않아야 한다. 3회보다 긴 recurrence를 놓칠 수 있다는 제한을 기록하라.
- benchmark에 필요한 reduction accumulator, first-order recurrence/splice, 기본 induction을 지원하라. widened induction은 내부 backedge를 생성하며 canonical IV도 일반 PHI와 다를 수 있으므로 단순 accessor 일괄 호출을 피하라.
- 일반 SSA def-use로 memory-carried dependence가 해결됐다고 가정하지 마라. 분석 지원 범위와 검출 불가능한 경우를 명시하라. 지원하지 않는 recurrence를 LCD=0으로 조용히 처리하지 말고 기존 모델 fallback과 이유를 출력하라.
- throughput의 skip/precompute 상태를 latency 분석에 재사용하지 마라. 작은 latency helper에서 기존 TTI API에 `TCK_Latency`를 전달하고, 필요한 recipe만 명시적으로 처리하라. 기존 legacy fallback이 다른 cost kind를 쓰는지 확인하라.
- latency 하나로 복합 recipe의 operand-to-result 경로를 근사할 수는 있으나, 그 가정을 기록하라. Jacobi의 splice가 핵심이므로 이를 전부 unsupported로 남겨놓고 모델 구현이 완료됐다고 하지 마라.
- loop 내부 accumulator update와 loop exit의 horizontal reduction을 구분하라. exit reduction과 workaround를 매 iteration LCD에 넣지 마라.
- 두 독립 accumulator의 recurrence latency는 단순 합산하지 마라. 자원 경쟁은 TP, 각 recurrence 제약은 LCD가 담당하게 하라.

### 비용 단위와 연결

- `TCK_RecipThroughput`과 `TCK_Latency` 반환값의 비교 가능한 단위를 먼저 확인하라. 실제 XiangShan/Saturn 경로가 generic fallback인지 조사하라. 필요하면 기존 TTI 구현에서 두 종류를 공통 cycle-like 단위로 정규화하라.
- 두 타깃의 알고리즘은 공유하라. target CPU/tune feature 또는 작은 명시적 experimental tuning profile로 파라미터를 선택하라. Saturn을 ISA 속성만 보고 추측하거나 알려진 XiangShan CPU 이름으로 위장하지 마라. 다른 CPU의 기본 TTI 동작을 보존하라.
- 새 공통 TTI API, scheduling simulator, 범용 graph framework, MachineInstr lowering pipeline을 도입하지 않는 것을 우선하라. 정말 작은 hook 하나가 전체 수정량을 줄인다면 이유와 대안을 기록하고 최소한으로 추가할 수 있다.
- 최초 범위는 IC=1/SWP=off의 main-loop VF ranking이다. scalar vectorization profitability와 epilogue 선택까지 무관하게 재설계하지 마라. 기존 수익성 판정과 새 ranking을 분리할 수 있다.
- 평가에서는 VF=2/4/8/16을 모두 score할 수 있어야 한다. forced VF가 기존 costing을 우회하면 진단 경로를 추가하라. 후보를 제외해서 점수가 좋아 보이게 하지 말고, automatic selection이 이 후보들을 실제 탐색하는지도 확인하라.
- 새 cost kind를 code-size 최적화에 적용하지 마라. invalid cost, 기존 legality, spill 처리를 유지하라.

## TTI tuning 원칙

우선 조정할 후보는 연속 memory throughput, strided/indexed memory scaling, FP add/FMA throughput와 dependency latency, slide/splice의 LMUL scaling, index-width legalization 비용이다. 모든 항목을 처음부터 새로 구현하지 말고 residual 분석으로 필요한 것만 수정하라.

- 타깃별 작은 parameter table과 공통 함수를 선호하라. latency는 고정 startup과 LMUL/datapath에 따른 증가처럼 설명 가능한 형태를 우선하라.
- kernel 이름, function 이름, source 위치, benchmark ID, measured cycle lookup, kernel별 계수 및 kernel별 VF 보정은 금지한다.
- 연산 종류·타입·LMUL/EMUL·legalization 경계·mask/tail 형태에 따른 공통 규칙은 허용한다. measured table의 각 VF cell에 자유 파라미터를 하나씩 대응시키지 마라.
- cost-only 표현으로 얻을 수 없는 backend 상태 전환을 모두 새로 시뮬레이션하지 마라. 필요한 경우 VPlan에서 관측 가능한 구조에 근거한 작은 공통 overhead 근사만 추가하고, TTI primitive 비용에 원인 불명의 overhead를 숨기지 마라.
- s000/s125의 scaling, s1111의 strided-store 포화, s4112의 i64 index EMUL 경계, Jacobi의 m8 퇴보, GESUMMV의 VF=4→8 퇴보를 구분해서 설명하라.
- VF=4→8에서 tail 정책·memory 명령·vsetvli 구성이 바뀐다. 이를 LMUL throughput 하나로 fitting하지 마라. vsetvli 비용이나 split 비용을 다른 항목과 중복 계산하지 마라.
- 36개 원래 측정은 compiler spill/reload가 없다. Jacobi 퇴보를 근거 없는 spill penalty로 맞추지 마라.
- absolute ROI overhead, exit reduction, XiangShan workaround 비용은 별도 residual로 설명하라. 필요 시 호출당 amortization을 사용하되 함수별 자유 상수로 맞추지 마라.

## 평가 지표와 초기 목표

코드를 튜닝하기 전에 baseline과 지표를 저장하고 아래 기준을 고정하라. 기준 변경이 필요하면 변경 이유와 이전 기준에서의 결과를 함께 남겨라.

1. **Selection regret:** 실제 선택 VF의 measured cycles / 해당 kernel의 최소 measured cycles. 타깃별 geometric mean과 worst case를 보고한다. 초기 목표는 geometric mean ≤1.05, worst ≤1.15다.
2. **Ranking:** kernel 내 6개 VF pair의 방향 일치율. measured 차이 3% 이내는 near-tie로 제외한 값을 주지표로 삼고, 전체 pair 결과도 함께 보고한다. near-tie는 측정 노이즈의 추정 신뢰구간이 아니라 사전에 정한 평가 tolerance다. 초기 목표는 non-tie pairwise accuracy ≥90%다.
3. **Relative scale:** kernel 내 VF=2를 기준으로 `pred_rel = RankScore(VF)/RankScore(2)`, `meas_rel = cycles(VF)/cycles(2)`를 비교한다. VF=4/8/16에 대해 `exp(mean(abs(log(pred_rel/meas_rel))))`와 개별 오차를 보고한다. 초기 목표는 평균 multiplicative error ≤1.15, 90th-percentile multiplicative error ≤1.30이다.
4. **중요 경계와 gap:** VF=2→4, 4→8, 8→16 각각의 predicted/measured ratio와 변화율을 보고한다. Jacobi의 VF=8→16 큰 퇴보와 GESUMMV의 VF=4→8 퇴보는 방향뿐 아니라 gap의 크기도 별도로 비교한다. 순서가 맞았다는 이유만으로 상대 차이까지 정확하다고 주장하지 마라. 반대로 1~2% 차이나 exact cycle을 맞추기 위한 복잡도는 추가하지 마라.
5. **Baseline/ablation:** 기존 LLVM 비용 및 automatic selection, largest-VF 선택, TP-only, LCD-only, max(TP,LCD)를 같은 표로 비교하라. 단순한 baseline보다 좋아졌는지와 LCD 추가의 기여를 확인하라.
6. **일반화:** 이 문서에 요약된 과거 7개 tuning / BiCG·GEMVER freeze 후 평가 이력을 새 실험 기록에 명시하라. 원본 실험 파일을 복원하려고 하지 마라. 현재는 그 결과를 본 상태이므로, 이후 변경은 핵심 7개와 BiCG/GEMVER를 별도로 재평가하고 validation-informed임을 명시하라. 새 freeze를 만들 수는 있지만 과거 데이터 노출을 없앨 수는 없다. 가능하면 kernel family를 통째로 제외하는 평가를 추가하되, 모델 구조가 전체 suite를 보고 선택되었다면 그 한계도 표시하라.

relative scale의 주목표는 같은 kernel 안에서 VF에 따른 비율과 gap이다. 서로 다른 kernel 간 cycle scale과 absolute cycle 환산은 선택적인 보조 분석이며 성공 조건으로 추가하지 마라. 이를 보고한다면 kernel별 정규화만으로 cross-kernel 예측까지 성공했다고 주장하지 말고, absolute 환산에는 calibration 데이터에서 정한 타깃당 공통 scale만 허용하라.

## 구현·평가·보정 반복

1. 수정 전 로컬 `main` HEAD의 compiler/profile로 36-cell candidate scoring과 automatic selection baseline을 확보한다. 이 job에서 baseline을 새로 생성하고 보존한다. 보고서 compiler나 이전 수정 compiler의 결과를 이 baseline으로 바꾸어 부르지 마라. 어셈블리/IR/VPlan으로 code shape와 VF/IC/SWP를 확인하고 historical 측정 조건과의 차이를 기록한다.
2. 최소 TP/LCD 모델과 타깃 profile 분리를 구현한다. 모델 구조를 먼저 고정하고 모든 타깃 parameter, 기본값, 자유도 수를 기록한다.
3. 각 candidate에 `target, kernel, VF, estimated runtime VF, legacy cost, domain totals, OtherTP, TP, LCD, dominant recurrence, final score, fallback reason`을 machine-readable 형태로 출력한다. kernel 이름은 평가 데이터 결합에만 사용하고 compiler 모델 입력으로 사용하지 마라.
4. 실제 compiler가 출력한 점수와 measured cycles를 결합해 지표·오차·ablation을 계산한다. Python 평가기의 복제 모델만 맞추고 실제 LLVM 동작을 검증하지 않는 방식은 피하라.
5. 가장 큰 공통 residual에 대해 원인 가설을 하나 세우고, 관련 TTI parameter 또는 작은 공통 규칙만 수정한다. 초기에는 제한된 grid/coordinate search도 가능하지만 자유도와 탐색 범위를 기록하라.
6. 매 round에서 전체 calibration suite를 다시 평가하고 기존 best와 비교한다. 개선이 없는 복잡도는 되돌리되 사용자의 변경은 보존한다. parameter 변경으로 다른 TTI 사용자나 code shape에 미치는 영향도 확인한다.
7. parameter를 고정하고 BiCG/GEMVER를 별도 평가한다. 현재는 validation-informed임을 표시한다. 다른 target은 software profile 분리·기본 동작 보존 테스트를 수행하되, Saturn 실측 성능 평가는 자료가 없어 보류임을 명시한다.
8. 목표 달성까지 반복하되, 여러 round 개선이 없으면 자유 파라미터를 계속 늘리지 말고 graph coverage, latency 단위, tail/VL 코드 형태, 측정 대응 관계를 재점검한다. 단순 모델의 한계라면 미달 지표와 재현 가능한 근거를 남기고, 그 상태를 목표 달성으로 표시하지 마라.

historical cycles와 예측값의 비교에서는 generated code shape의 대응이 핵심이다. TTI 변경으로 forced-VF assembly까지 바뀌면 기존 measured cycles가 새 binary의 정답이라고 가정하지 마라. historical calibration과 새 binary 검증을 분리하고, 현재 report-only 범위에서는 바뀐 후보를 실측 미검증으로 표시하라. 자동 선택된 코드도 같은 VF의 forced binary와 다른 경우 차이를 조사하라. 보고서에 full historical assembly/ELF가 없으므로 reconstructed baseline과 같다는 사실을 historical binary identity의 증명으로 바꾸지 마라.

현재 단계에서는 보고서의 기존 데이터와 compiler 출력으로 반복 평가하며 RTL/QEMU 측정을 수행하지 않는다. 새 환경에서도 simulator 확보를 선행 조건으로 만들지 마라. 실제 측정이 없는 결과를 QEMU cycle, host 시간, 분석 추정치로 채우지 마라.

## 완료 산출물과 검증

- 작은 공통 TP/LCD 구현, 독립적으로 선택되는 XiangShan/Saturn TTI profile, 변경 이유를 담은 diff.
- 재현 가능한 추출·build·candidate scoring·평가 명령과 스크립트. compiler revision, target 구성, flags, 입력 hash와 measurement provenance 포함.
- baseline/best parameter 및 각 tuning round의 변경·지표를 보존한 기록.
- 타깃·kernel·VF별 predicted/measured 상대비율과 순위, 선택 VF, regret, ablation, fallback coverage를 담은 CSV와 최종 보고서.
- 독립적인 긴 체인이 LCD가 되지 않는 경우, 단일/상호 PHI recurrence, 독립 accumulator 두 개, splice recurrence, 도메인 전체 합산, invalid/fallback, target profile 분리를 검증하는 의미 있는 테스트.
- 관련 LLVM 빌드와 Vectorizer/RISC-V TTI 테스트를 실행하라. 필요한 체크 통과 후 근거 없이 전체 빌드를 반복하지 마라.
- 최종 보고서에는 달성한 기준과 미달 기준, 실제 측정과 추정, historical calibration과 새 코드 검증, Saturn 검증 유무를 분명히 구분하라. 일부 좋은 사례만으로 전체 목표를 완료했다고 하지 마라.

일상적인 구현·로컬 실험·되돌릴 수 있는 수정은 반복 확인 없이 진행하라. 누락된 필수 정보는 필요한 이유를 구체적으로 질문하면서 독립적인 작업을 계속하라. 이 프롬프트는 구현과 로컬 평가를 승인하며 원격 게시·외부 유료 자원 구매를 요청하지 않는다. commit을 작성할 경우 먼저 history를 확인하고 저장소 메시지 패턴을 따르라.

---

## 소스 이관 없는 새 구현의 실행 절차

### 1. 입력과 출발점

새 환경에는 다음만 있다고 가정한다:

- 위 절차로 준비한 clean LLVM repository의 로컬 `main` HEAD.
- 이 프롬프트와 `VF_IC1_SWP_OFF_REPORT.md`.

이전 TP/LCD 수정 소스, custom flags, tests, Python evaluator, JSON/CSV, snapshot, compiler binary, sysroot 및 build cache는 없다. 이 부재를 blocker로 삼거나 사용자가 가져오기를 기다리지 마라. `ca7933e4`의 실제 VPlan/TTI 구조를 읽고, 요구 기능과 실험 도구를 새로 구현한다. 예전 구현의 함수명·클래스명·파일 구조를 그대로 재현할 의무는 없다.

두 문서가 repository 밖에서 제공되면 그 경로에서 읽는다. 문서를 실험 디렉터리에 복사해 생긴 untracked 파일은 알려진 작업 입력이지 복구해야 할 이전 소스 변경이 아니다. 작업을 진행하면서 생성한 파일을 clean 상태로 돌리려고 삭제하지 마라.

### 2. 보고서 내장 입력 추출

보고서에는 kernel C source, build/run 도구, reduction workaround, runtime, measured CSV 및 license 자료가 내장되어 있다. 별도의 이전 experiment directory가 필요하지 않다. 보고서의 추출 절차를 사용하거나 다음과 같은 새 추출 도구를 작성한다:

```sh
python3 - VF_IC1_SWP_OFF_REPORT.md experiments/tp-lcd/inputs <<'EXTRACT'
import hashlib
import json
from pathlib import Path
import re
import sys

report = Path(sys.argv[1])
destination = Path(sys.argv[2]).resolve()
blocks = re.findall(
    r'<!-- file: ([^\n]+) sha256: ([0-9a-f]{64}) -->\n```[^\n]*\n(.*?)\n```',
    report.read_text(), re.S)
if not blocks:
    raise SystemExit('No embedded files found')
files = {}
for name, expected, body in blocks:
    path = (destination / name).resolve()
    content = (body + '\n').encode()
    if not path.is_relative_to(destination) or name in files:
        raise SystemExit('Invalid or duplicate path: ' + name)
    if hashlib.sha256(content).hexdigest() != expected:
        raise SystemExit('Hash mismatch: ' + name)
    files[name] = (content, expected)
destination.mkdir(parents=True, exist_ok=False)
for name, (content, _) in files.items():
    path = destination / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
manifest = {
    'report_sha256': hashlib.sha256(report.read_bytes()).hexdigest(),
    'files': {name: item[1] for name, item in files.items()},
}
(destination.parent / 'input-manifest.json').write_text(
    json.dumps(manifest, indent=2) + '\n')
print('Verified and extracted', len(files), 'files')
EXTRACT
```

입력 문서 경로와 새 출력 디렉터리는 실제 환경에 맞게 지정한다. 기존 출력 디렉터리는 덮어쓰지 않는다. 추출한 원본 입력도 수정하지 말고, 새 harness와 결과는 그 밖에 둔다.

추출된 build 도구는 historical compiler를 전제로 할 수 있다. compile 옵션을 먼저 확인하고 현재 main에서 지원하는지 조사한다. 예를 들어 SWP-off 옵션이 없으면 그 버전의 RISC-V backend에 해당 pass가 없는 것인지 소스로 확인하고 동등한 정책과 차이를 기록한다. 알 수 없는 옵션을 이유 없이 제거하거나 다른 정책을 같은 조건이라고 부르지 마라.

### 3. 수정 전 main의 compiler와 baseline

먼저 아래와 같이 시작 SHA를 기록하고 pristine compiler를 build한다. 명령은 repository root 기준이며, 병렬 수는 새 host의 자원에 맞춘다:

```sh
TP_LCD_START_SHA=$(git rev-parse refs/heads/main)
TP_LCD_BUILD=build-tp-lcd-main
TP_LCD_JOBS=4

cmake -S llvm -B "$TP_LCD_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_TARGETS_TO_BUILD=RISCV -DLLVM_ENABLE_PROJECTS=clang \
  -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_ZLIB=OFF
cmake --build "$TP_LCD_BUILD" --target clang opt llc FileCheck \
  llvm-as llvm-dis llvm-link llvm-config llvm-readobj count not \
  --parallel "$TP_LCD_JOBS"
```

Python 3.10+, CMake, Ninja, host C++ compiler 및 RISC-V용 headers를 새 환경에서 준비한다. Tool/header 설치는 허용된다. 보고서의 Dockerfile과 build 설명을 참고하되 새 환경에서 이용 가능한 방식으로 구성한다. 새 header/sysroot의 버전·hash를 기록하고, 이를 복원된 historical package lock이라고 주장하지 마라. Assembly/scoring 단계에 불필요한 simulator나 bare-metal runtime 실행 환경을 먼저 만들지 마라.

구현을 시작하기 전에 pristine compiler 및 필요한 resource files, compile commands, 36개 forced 후보의 assembly/cost와 automatic 결과를 보존한다. 이후 source 수정에 따라 baseline executable을 재build하여 덮어쓰지 않는다. 필요한 비용 진단만 추가했다면 관측 기능이 생성 코드를 바꾸지 않는지 확인한다.

현재 main에는 이전 custom flags가 없으므로 그 이름으로 실행하지 마라. 먼저 이 버전의 forced-VF costing·candidate enumeration·debug/remark 경로를 조사한다. 기존 진단으로 충분하지 않으면 작은 observation 경로를 구현한다. Assertion build가 필요한 debug 출력에 의존한다면 이를 build 설정에 반영한다.

### 4. 새로 작성할 실험 도구

예를 들어 `experiments/tp-lcd/` 아래에 도구와 기록을 만들 수 있다. 아래는 새로 만들 기능의 요구사항이며, 이미 존재하는 script나 파일을 지칭하지 않는다:

- **입력·provenance 관리:** 시작 main SHA, 수정 diff/hash, compiler hash/version, report와 embedded input hash, tool/header 버전, 전체 flags를 저장한다.
- **Baseline/candidate scoring:** 원래 전체 C translation unit을 유지하여 VF=2/4/8/16, IC=1, SWP=off를 compile한다. 함수만 분리하거나 수작업 intrinsic으로 대체하지 않는다. Forced 후보와 실제 automatic search를 별도로 기록한다.
- **실제 compiler 진단 수집:** function/loop 식별자를 평가용 kernel ID와 연결한다. target/profile, VF, runtime VF, legacy cost, domain totals, OtherTP, TP, LCD, dominant recurrence, final score, fallback, spill 및 별도 구조적 overhead를 기록한다. Function 이름을 compiler 모델의 수치 선택 입력으로 쓰지 않는다.
- **평가기:** 위 공식대로 regret, pairwise ranking, VF=2 기준 상대비율, adjacent-VF gap, ablation, coverage를 계산한다. Cost model 자체를 Python에 복제하여 그 복제 모델만 fitting하지 않는다.
- **Code-shape 비교:** 같은 새 환경에서 만든 pristine-main baseline과 수정 compiler 결과를 비교한다. 보고서에 기술된 LMUL, memory 형태, vsetvli, reduction workaround 및 spill 유무도 확인한다. Historical binary 전체가 보고서에 없다는 한계를 유지한다.
- **Round 관리:** 파라미터·규칙·탐색 범위를 실행 전에 기록하고, 각 round의 전체 결과와 실패·악화된 trial도 보존한다. Directory 이름은 새로 정하며 이전 round 파일을 찾지 않는다.
- **테스트:** 현재 LLVM 버전에 맞는 Vectorizer/RISC-V TTI regression과 필요한 unit tests, evaluator의 누락 후보·tie·분모·선택 VF 처리 검사를 새로 작성하고 실행한다.

평가기 세부 규칙도 tuning 전에 고정한다:

- Near-tie는 `max(cycles_a, cycles_b) / min(cycles_a, cycles_b) <= 1.03`으로 정의한다.
- Measured non-tie에 대해 predicted tie는 방향 일치로 계산하지 않는다. 동일 score의 선택 tie-break는 작은 VF로 한다. 실제 automatic 선택은 별도로 기록한다.
- P90은 정렬된 표본의 `(n-1) * 0.9` 위치에서 선형 보간한다.
- 누락된 VF/kernel, invalid cost, 실제 선택이 실측 후보 집합 밖인 경우를 임의로 제외하지 않는다. Coverage와 계산 불가능한 지표를 명시한다.
- LCD-only의 기준 score가 0이면 상대 scale은 undefined로 표시한다. 작은 양수를 넣어 지표를 좋게 만들지 않는다.
- 핵심 7개와 BiCG/GEMVER의 지표를 구분하고, 전체 9개 집계가 필요하면 함께 추가한다. 두 그룹을 섞어 한 그룹의 실패를 숨기지 않는다.

Automatic search가 실제로 네 scalable 후보를 방문하도록 현재 버전의 적절한 target tuning 설정을 사용한다. 일부 버전의 `-riscv-v-register-bit-width-lmul=8`은 참고할 설정이지만, 존재 여부와 실제 탐색 결과를 확인해야 한다. Forced score 최소 VF를 automatic selection이라고 대신 보고하지 않는다.

### 5. 이전 시도에서 얻은 참고 지식 — 복원 대상이나 검증 증거가 아님

아래는 소스 이관 없이 전달하는 설계상 주의점이다. 새 main의 구현에 같은 문제가 있는지 먼저 확인하고, 필요한 것만 가장 작게 구현한다. 전부 미리 추가할 기능 목록이 아니다.

- 이전 RISC-V 경로에서는 throughput이 target/LMUL 기반인 반면 latency가 generic fallback을 쓰는 경우가 있었다. 같은 단위인지 확인하지 않은 채 max를 취하면 LCD가 의미 없어질 수 있다.
- Legacy precompute가 vector induction 비용을 남겼지만, 최종 VPlan은 scalar counter만 사용하는 경우가 있었다. 실제 VPlan의 scalar-use 증거로 확인하고 중복·과대 계산을 피한다.
- `vp.merge`가 target에서 무료로 fold되고 accumulator의 사용 관계가 허용하면 old accumulator, update result, merge result가 같은 register group을 쓸 수 있다. 이를 별도 live values로 세어 phantom spill을 만들지 마라. 반대로 추가 live use가 있거나 실제 pressure가 남으면 coalescing/penalty 생략은 부당하다. Profitability와 ranking의 spill 처리 변경 범위를 명확히 하라.
- Synthetic EVL cast는 lowering 시 없어질 수 있다. 이를 증명할 target 문맥을 기존 TTI API로 전달할 방법을 우선 검토한다. Live-in에는 defining recipe가 없을 수 있으므로 null-safe하게 분석한다.
- Tail-agnostic memory와 tail-undisturbed accumulator update 사이의 VL policy 변경은 필요 시 VPlan 구조에 근거한 별도 overhead로 근사할 수 있다. 이미 계산한 초기 VL setup을 다시 더하거나 무조건 accumulator 수에 비례시키지 마라.
- Jacobi의 load-fed splice는 지원해야 하지만 그 feed-forward 체인을 closed-cycle LCD로 바꾸면 안 된다. Slide의 비선형 scaling은 검토할 공통 opcode/type 기반 가설이지 확인된 물리 원인이 아니다.
- 같은 basic block에서 contract가 허용된 single-use multiply/add가 native FMA로 합쳐지면 별도 연산 두 개로 계산하는 모델이 과대평가할 수 있다. 현재 target의 legality, 실제 VPlan 사용 관계, lowering/code shape를 확인한다. FMA 한 명령의 throughput을 무조건 add 한 개와 같다고 가정하지도 마라.
- Memory와 FP의 effective beat width가 같아야 할 이유는 없다. 다만 근거 없는 datapath/계수를 늘리지 말고 공통 residual과 전체 평가 개선으로 필요성을 입증한다.

### 6. 과거 결과와 validation 노출 이력

이전 구현은 historical compiler revision `89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb`를 바탕으로 했다. 아래 숫자는 이전 시도의 참고 요약일 뿐이다. 원본 실험 파일은 제공되지 않으며, 현재 main의 baseline·성능·테스트 결과를 대신하지 않는다.

| 과거 단계 | 참고 결과 |
|---|---|
| 최초 freeze 전 calibration best | 핵심 7개 non-tie 100%; actual expanded automatic regret GM 1.002283 / worst 1.016090; relative error GM 1.144632 / P90 1.293405 |
| 최초 freeze 후 BiCG/GEMVER 평가 | 둘 다 expanded automatic VF=16, regret 1.0, non-tie 100%; relative error GM 1.466562 / P90 1.512126로 gap/scale 목표 미달 |
| Validation을 본 뒤 FMA accounting 수정 시도 | 핵심 7개 ranking·regret 유지; relative error GM 1.157394 / P90 1.303404. 수정 뒤 BiCG/GEMVER 평가는 수행되지 않은 상태로 중단 |

따라서 새 구현에서 validation-informed임을 명시한다. 새 parameter freeze와 별도 그룹 평가는 수행하되, 소스·파일이 없다는 이유로 과거 데이터 노출을 초기화하거나 untouched/blind validation이라고 부르지 마라. 이전 테스트 통과 수나 calibration 지표를 현재 Goal의 완료 증거로 사용하지 않는다.

이전 best의 비용 파라미터는 memory/reference beat width 256 bits, FP beat width 128 bits, memory beat cost 1, memory startup 4, FP beat cost 1, FP startup 4, slide beat cost 3, slide startup 8이었다. Slide에는 `startup + beats² × slide_beat` 가설, indexed access에는 memory startup을 더하는 가설을 사용했다. **이 값이나 8개 필드를 새 모델의 필수 구조·정답·실제 hardware configuration으로 고정하지 마라.** 새 main에서 더 적은 설명 가능한 파라미터로 시작하고, 필요한 경우에만 이 가설을 비교한다. 과거 값만 넣으면 위 숫자가 재현된다고 보장하지 않는다.

### 7. 작업 시작 및 완료 감사

1. 로컬 main HEAD와 실제 API를 확인하고 시작 SHA를 기록한다. 주 main을 과거 측정 revision으로 옮기거나 이전 branch를 복원하지 않는다.
2. 보고서 입력을 추출·검증하고 도구를 준비한다. 현재 main의 pristine baseline 36개 후보와 automatic 결과를 새로 얻는다.
3. 공통 최소 TP/LCD 분석, target profile 분리와 진단을 구현하고 필요한 테스트를 만든다. 이전 custom flags·scripts가 없어서 실패하는 명령을 반복하지 않는다.
4. Ranking, regret, relative scale/gap을 중심으로 전체 suite를 반복 평가한다. 큰 공통 residual 하나씩 수정하며, 실패한 trial도 기록한다. Exact cycle fitting으로 방향을 바꾸지 않는다.
5. 새 parameters/model을 고정하여 핵심 7개와 BiCG/GEMVER를 각각 평가하고 validation-informed임을 명시한다. Code-shape가 다른 후보는 실측 미검증으로 남긴다.
6. 새 소스 diff, 재현 도구, CSV, tests 및 최종 보고서로 요구사항별 증거를 확인한다. Historical 숫자나 일부 좋은 순위만으로 완료를 선언하지 않는다. 상대 gap의 미달과 Saturn 실측 보류도 분명히 보고한다.

