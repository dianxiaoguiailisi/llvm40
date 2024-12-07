//===- CollectIBranchHints.cpp - 收集间接分支提示信息 ------------===//
#include "llvm/Pass.h"
#include <string>
#include <map> // 修复了map未定义的问题
#include "llvm/Analysis/CFG.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "collect-ibranch-hints"

using namespace llvm;

typedef std::map<std::string, int> FunctionMap;

namespace {
struct CollectIBranchHints : public FunctionPass {
  static char ID;
  static FunctionMap *fMap;
  static int FID;

  CollectIBranchHints() : FunctionPass(ID) {}
  bool runOnFunction(Function &F) override;
  bool instrumentIBranch(Instruction *I, int fid, int count);
};
} // end of namespace

bool CollectIBranchHints::runOnFunction(Function &F) {
  bool modified = false;
  std::string name = F.getName().str();
  int fid;
  int count = 0;

  // 检查是否已经访问过这个函数
  if (fMap->find(name) != fMap->end()) {
    (*fMap)[name] = ++FID; 
  } else {
    (*fMap)[name] = FID++;  // 如果没有访问过，初始化该函数ID
  }

  fid = (*fMap)[name];

  // 遍历函数中的基本块，查找间接分支指令
  for (auto &BB : F) {
    if (auto *IBI = dyn_cast<IndirectBrInst>(BB.getTerminator())) {  
      modified |= instrumentIBranch(IBI, fid, count);
      count++;
    }
  }

  return modified;
}

bool CollectIBranchHints::instrumentIBranch(Instruction *I, int fid, int count) {
  IRBuilder<> B(I);
  Module *M = B.GetInsertBlock()->getModule();
  Type *VoidTy = B.getVoidTy();
  Type *I64Ty = B.getInt64Ty();
  
  llvm::Constant* constFid, *constCount;
  Value *target;
  Value *castVal;

  errs() << __func__ << " : " << *I << "\n";
  errs() << __func__ << " fid : " << fid << " count: " << count << "\n";

  if (auto *indirectBranchInst = dyn_cast<IndirectBrInst>(I)) {
    target = indirectBranchInst->getAddress();
  } else {
    errs() << __func__ << "错误: 未知的间接分支指令: " << *I << "\n";
    return false;
  }

  assert(target != nullptr);

  // 使用 getOrInsertFunction 来插入或获取函数
  FunctionCallee ConstCollectIBranchHints = M->getOrInsertFunction(
      "__collect_ibranch_hints", VoidTy, I64Ty, I64Ty, I64Ty, nullptr);

  // 获取 Function 并转换为 Constant* 类型
  Function *FuncCollectIBranchHints = cast<Function>(ConstCollectIBranchHints.getCallee());

  // 创建函数ID和计数的常量
  constFid = ConstantInt::get(I64Ty, fid);  // 注意这里返回的是 Constant* 类型
  constCount = ConstantInt::get(I64Ty, count);  // 同上
  
  // 将目标地址指针转换为整数
  castVal = CastInst::Create(Instruction::PtrToInt, target, I64Ty, "ptrtoint", I);

  // 创建调用指令
  B.CreateCall(FuncCollectIBranchHints, {constFid, constCount, castVal});

  return true;
}

char CollectIBranchHints::ID = 0;
int CollectIBranchHints::FID = 0;
FunctionMap *CollectIBranchHints::fMap = new FunctionMap();  // 初始化map

// 使用LLVM的Pass管理器注册此Pass
static RegisterPass<CollectIBranchHints> X("collect-ibranch-hints-pass", "收集间接分支提示信息", false, false);
