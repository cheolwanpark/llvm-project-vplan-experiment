add_subdirectory(SandboxVectorizer)

set(LLVM_LINK_COMPONENTS
  Analysis
  Core
  Vectorize
  AsmParser
  TargetParser
  )

add_llvm_unittest(VectorizeTests
  VPlanTest.cpp
  VPlanCostModelTest.cpp
  VPDomTreeTest.cpp
  VPlanHCFGTest.cpp
  VPlanPatternMatchTest.cpp
  VPlanRecurrenceTest.cpp
  VPlanSlpTest.cpp
  VPlanUncountableExitTest.cpp
  VPlanVerifierTest.cpp
  )
