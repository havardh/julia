#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

struct ArrayToPointerPass : public FunctionPass {

  static char ID;
  ArrayToPointerPass() : FunctionPass(ID) {}

  virtual bool runOnFunction(Function &F) {

    for (ilist_iterator<Argument> argument = F.getArgumentList().begin();
         argument != F.getArgumentList().end(); ++argument) 
      {
        if (argument->getType()->isPointerTy()) {

          for (Value::use_iterator use = argument->use_begin();
               use != argument->use_end();
               ++use) 
            {

              // Get Instructions decoding the jl_value_t
              Instruction *GetArray = cast<Instruction>(*use);
              Instruction *LoadArray = cast<Instruction>(*GetArray->use_begin());
              GetElementPtrInst *LoadValue = cast<GetElementPtrInst>(*LoadArray->use_begin());
              Instruction *Bitcast = cast<Instruction>(*LoadValue->use_begin());
              Instruction *Use = cast<Instruction>(*Bitcast->use_begin());

              std::vector<llvm::Value *> putsArgs(LoadValue->idx_begin(), LoadValue->idx_end());
              llvm::ArrayRef<llvm::Value*>  argsRef(putsArgs);
              Instruction *GetElementPtr = GetElementPtrInst::Create(
                argument,
                argsRef,
                Twine(""),
                Use
              );

              Bitcast->replaceAllUsesWith(GetElementPtr);

              Bitcast->eraseFromParent();
              LoadValue->eraseFromParent();
              LoadArray->eraseFromParent();
              // Fix: Should be GetArray->eraseFromParent(), but this segfaults
              GetArray->removeFromParent();
              
            }

        }
      }

    return false;

  }

};

char ArrayToPointerPass::ID = 0;
static RegisterPass<ArrayToPointerPass> X("ArrayToPointerPass", "Julia Array to C pointer pass", false, false);

Pass * createArrayToPointerPass() {
  return new ArrayToPointerPass();
}

}
