#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

  
bool is_js_value_t(Type* type) {
  if (type->isPointerTy()) {
    Type *elemType = type->getPointerElementType();
    if (elemType->isStructTy()) {
      if (elemType->getStructName() == StringRef("jl_value_t")) {
        return true;
      }
    }
  }
  return false;
}


struct OpenCLArgumentPass : public ModulePass {

  static char ID;
  OpenCLArgumentPass() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M) {

    Function *NewF;
    std::vector<llvm::Function*> functions;
    for (Module::iterator F = M.begin(); F != M.end(); ++F) {
      functions.push_back(F);
    }

    for(std::vector<Function*>::iterator F = functions.begin(), E = functions.end(); F != E; ++F) {

      Function *OldF = (*F);
      
      std::vector<Type*> ArgTypes;
      for (Function::const_arg_iterator I = OldF->arg_begin(), E = OldF->arg_end(); I != E; ++I) {

        Type* type = I->getType();
        
        if (is_js_value_t(type)) {
          ArgTypes.push_back(PointerType::get(IntegerType::get(M.getContext(), 64), 0));
        } else {
          ArgTypes.push_back(I->getType());
        }

      }

      FunctionType *FTy = FunctionType::get(
        OldF->getFunctionType()->getReturnType(),
        ArgTypes,
        OldF->getFunctionType()->isVarArg()
      );

      NewF = Function::Create(
        FTy, 
        OldF->getLinkage(),
        OldF->getName()
      );

      ValueToValueMapTy VMap;
      Function::arg_iterator DestI = NewF->arg_begin();
      for (Function::const_arg_iterator I = OldF->arg_begin(), E = OldF->arg_end(); I != E; ++I) {
        VMap[I] = DestI++;
      }

      
      
      SmallVector<ReturnInst*, 5> Returns;
      CloneFunctionInto(NewF, OldF, VMap, false, Returns, "", 0);
      OldF->eraseFromParent();

    }

    M.getFunctionList().push_back(NewF);
    
    return false;
  }

};

char OpenCLArgumentPass::ID = 0;

static RegisterPass<OpenCLArgumentPass> X("OpenCLArgumentPass", "Julia Array arguments to C pointer pass", false, false);

Pass* createOpenCLArgumentPass() {
  return new OpenCLArgumentPass();
}

}
