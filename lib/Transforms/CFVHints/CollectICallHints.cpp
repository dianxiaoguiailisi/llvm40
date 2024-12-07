//===- CollectICallHints.cpp - Collect Indirect Call hints ----------------===//
//
#include "llvm/Pass.h"
#include <string>
#include <map>
#include "llvm/Analysis/CFG.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "collect-icall-hints"

using namespace llvm;

typedef std::map<std::string, int> FunctionMap;

namespace {
struct CollectICallHints : public ModulePass {
  static char ID;
  static FunctionMap *fMap;
  static int FID;

  CollectICallHints() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;
  bool processFunction(Function &);
  bool instrumentICall(Instruction *I, int fid, int count);
};
} // end of namespace

bool CollectICallHints::runOnModule(Module &M) {
  bool modified = false;
  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    if (F.hasFnAttribute(Attribute::OptimizeNone)) 
      continue;
    modified |= CollectICallHints::processFunction(F);
  }

  return modified;
}

bool CollectICallHints::processFunction(Function &F) {
  bool modified = false;
  std::string name = F.getName().str();
  int fid;
  int count = 0;

  // Check whether we have visited this function before
  if (fMap->find(name) != fMap->end()) {
    (*fMap)[name] = ++FID; 
  }

  fid = (*fMap)[name];

  errs() << "Processing function: " << F.getName() << "\n";

  // Iterate over each instruction in the function to find indirect calls
  for (auto &I : F) {
    if (auto *callInst = dyn_cast<CallInst>(&I)) {
      if (callInst->getCalledFunction() == nullptr) {  // Indirect call
        modified |= instrumentICall(callInst, fid, count);
        count++;
      }
    }
  }

  return modified;
}

bool CollectICallHints::instrumentICall(Instruction *I, int fid, int count) {
  IRBuilder<> B(I);
  Module *M = B.GetInsertBlock()->getModule();
  Type *VoidTy = B.getVoidTy();
  Type *I64Ty = B.getInt64Ty();
  Constant *constFid, *constCount;
  Value *targetFunc;
  Value *castVal;

  errs() << __func__ << " : "<< *I<< "\n";
  errs() << __func__ << " fid : "<< fid << " count: " << count << "\n";

  if (auto *callInst = dyn_cast<CallInst>(I)) {
    if (callInst->getCalledFunction() == nullptr) {  // Indirect call
      targetFunc = callInst->getCalledOperand();  // Replace getCalledValue with getCalledOperand
    }
  } else if (auto *invokeInst = dyn_cast<InvokeInst>(I)) {
    if (invokeInst->getCalledFunction() == nullptr) {  // Indirect invoke
      targetFunc = invokeInst->getCalledOperand();  // Replace getCalledValue with getCalledOperand
    }
  } else {
    errs() << __func__ << " ERROR: unknown call inst: "<< *I<< "\n";
    return false;
  }

  assert(targetFunc != nullptr);

  // 使用 llvm::FunctionCallee 来存储返回的插入函数
  FunctionCallee ConstCollectICallHints = M->getOrInsertFunction("__collect_icall_hints", 
                                                                VoidTy, I64Ty, I64Ty, I64Ty, nullptr);

  // 获取 Function* 类型
  Function *FuncCollectICallHints = cast<Function>(ConstCollectICallHints.getCallee());
  
  constFid = ConstantInt::get(I64Ty, fid);  // Fix: using ConstantInt::get correctly
  constCount = ConstantInt::get(I64Ty, count);  // Fix: using ConstantInt::get correctly
  
  // 使用 PtrToInt 转换目标函数指针为整数
  castVal = CastInst::Create(Instruction::PtrToInt, targetFunc, I64Ty, "ptrtoint", I);

  // Create the call instruction for __collect_icall_hints
  B.CreateCall(FuncCollectICallHints, {constFid, constCount, castVal});

  return true;
}

char CollectICallHints::ID = 0;
int CollectICallHints::FID = 0;
FunctionMap *CollectICallHints::fMap = new FunctionMap();

static RegisterPass<CollectICallHints> X("collect-icall-hints-pass", 
                                           "Collect Indirect Call Hints Info", 
                                           false, false);
