#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/IPO/InlinerPass.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Regex.h"
#include "llvm/Linker.h"

namespace llvm {

Pass* createLowerJuliaArrayFunctionPass();
  
bool is_jl_array_type(Type* type) {
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


  
StringRef unmangleName(StringRef name) {

  StringRef mangledName = name.drop_front(6);

  SmallVector<StringRef, 3> Matches;
  Regex regex("([A-Za-z_+]*)");
  
  if (regex.match(mangledName, &Matches)) {
    return Matches[0];
  } else {
    return mangledName;
  }
  
}

struct LowerJuliaArrayPass : public ModulePass {

  static char ID;
  LowerJuliaArrayPass() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M) {
    
    FunctionPassManager fpm(&M);
    fpm.add(createLowerJuliaArrayFunctionPass());
    
    std::vector<Function*> Fs = functions(M);
    
    linkLibrary(M);

    for (std::vector<Function*>::iterator I = Fs.begin(), E = Fs.end(); I != E; ++I) {

           
      Function *OldFunc = (*I);

      Function *NewFunc = copyFunctionWithLoweredJuliaArrayArguments(OldFunc);
        
      OldFunc->eraseFromParent();
      M.getFunctionList().push_back(NewFunc);
      
      generateFunctionMetadata(M, NewFunc);
    
      fpm.run(*NewFunc);
    }
    return false;
  }

  std::vector<Function*> functions(Module &M) {
    std::vector<Function*> functions;

    for (Module::iterator F = M.begin(); F != M.end(); ++F) {

      F->setName(unmangleName(F->getName()));
      functions.push_back(F);
    }
    
    return functions;
  }

  void generateFunctionMetadata(Module &M, Function *F) {

    generateOpenCLKernelMetadata(M, F);
    generateNVVMKernelMetadata(M, F);
    
    
  }

  void generateOpenCLKernelMetadata(Module &M, Function *F) {

    SmallVector <llvm::Value*, 5> kernelMDArgs;
    kernelMDArgs.push_back(F);

    MDNode *kernelMDNode = MDNode::get(M.getContext(), kernelMDArgs);
    
    NamedMDNode* OpenCLKernels = M.getOrInsertNamedMetadata("opencl.kernels");
    OpenCLKernels->addOperand(kernelMDNode);
  }

  void generateNVVMKernelMetadata(Module &M, Function *F) {

    LLVMContext &Ctx = M.getContext();
    
    SmallVector <llvm::Value*, 5> kernelMDArgs;
    kernelMDArgs.push_back(F);
    kernelMDArgs.push_back(MDString::get(Ctx, "kernel"));
    kernelMDArgs.push_back(ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1));

    MDNode *kernelMDNode = MDNode::get(Ctx, kernelMDArgs);

    NamedMDNode* NvvmAnnotations = M.getOrInsertNamedMetadata("nvvm.annotations");
    NvvmAnnotations->addOperand(kernelMDNode);

  
  }

  void linkLibrary(Module &M) {
    Module* Library;
    SMDiagnostic error;
    Library = ParseIRFile("lowered-julia-array.bc", error, M.getContext());

    SmallVector<Type*, 1> args;
    args.push_back(Type::getInt32Ty(Library->getContext()));
    FunctionType *ft = FunctionType::get(Type::getInt64Ty(Library->getContext()), args, false);
    Library->getOrInsertFunction("get_global_id", ft);
    
    Linker::LinkModules(&M, Library, 0, 0);
  }

  Function* copyFunctionWithLoweredJuliaArrayArguments(Function *OldFunc) {

    std::vector<Type*> ArgTypes = lowerJuliaArrayArguments(OldFunc);

    FunctionType *functionType = buildLoweredFunctionType(OldFunc, ArgTypes);

    Function* NewFunc = Function::Create(
      functionType,
      OldFunc->getLinkage(),
      OldFunc->getName()
    );

    ValueToValueMapTy VMap;

    Function::arg_iterator DestI = NewFunc->arg_begin();
    for (Function::const_arg_iterator I = OldFunc->arg_begin(), E = OldFunc->arg_end(); I != E; ++I) {
      VMap[I] = DestI++;
    }


    SmallVector<ReturnInst*, 5> Returns;
    
    CloneFunctionInto(NewFunc, OldFunc, VMap, false, Returns, "", 0);

    return NewFunc;
  }

  std::vector<Type*> lowerJuliaArrayArguments(Function *OldFunc) {

    std::vector<Type*> ArgTypes;
    for (Function::const_arg_iterator I = OldFunc->arg_begin(), E = OldFunc->arg_end(); I != E; ++I) {

      Type* argType = I->getType();

      if (is_jl_array_type(argType)) {
        // Should figure out actual type from meta?
        // This is hardcoded i64*
        ArgTypes.push_back(PointerType::get(IntegerType::get(OldFunc->getContext(), 64), 1));
      } else {
        ArgTypes.push_back(I->getType());
      }
      
    }

    return ArgTypes;
    
  }

  FunctionType* buildLoweredFunctionType(Function *F, std::vector<Type*> ArgTypes) {
    return FunctionType::get(
      Type::getVoidTy(F->getContext()),
      ArgTypes,
      F->getFunctionType()->isVarArg()
    );
  }
  
};

char LowerJuliaArrayPass::ID = 0;

static RegisterPass<LowerJuliaArrayPass> X("LowerJuliaArrayPass", "Lower Julia Array usage to c arrays", false, false);

Pass* createLowerJuliaArrayPass() {
  return new LowerJuliaArrayPass();
}


struct LowerJuliaArrayFunctionPass : public FunctionPass {
  
  static char ID;
  LowerJuliaArrayFunctionPass() : FunctionPass(ID) {}

  virtual bool runOnFunction(Function &F) {
    
    std::vector<Instruction*> Is;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      Is.push_back(&*I);
    }

    for (std::vector<Instruction*>::iterator I = Is.begin(), E = Is.end(); I != E; ++I) {

      if (CallInst* call = dyn_cast<CallInst>(*I)) {

        //errs() << *call << "\n"; 
        
        StringRef name = unmangleName(call->getCalledFunction()->getName());
        //errs() << name << "\n";
        Function *function = F.getParent()->getFunction(name);


        if (function) {
          //errs() << function->getName() << "\n";

          std::vector<Value*> args;
          for(int i=0; i<call->getNumArgOperands(); i++) {
            args.push_back(call->getArgOperand(i));
          }
          
          ArrayRef<Value*> Args(args);
          CallInst *newCall = CallInst::Create(function, Args);

          ReplaceInstWithInst(call, newCall);
            
        } else {
          errs() << "Did not find function: " << name << "\n";
        }
      } else if (ReturnInst* ret = dyn_cast<ReturnInst>(*I)) {

        ReplaceInstWithInst(ret, ReturnInst::Create(F.getContext()));
        
      }
    }
      
        
    return false;
  }
  
};

char LowerJuliaArrayFunctionPass::ID = 0;

static RegisterPass<LowerJuliaArrayFunctionPass> Y("LowerJuliaArrayFunctionPass", "Lower Julia Array usage to c arrays", false, false);

Pass* createLowerJuliaArrayFunctionPass() {
  return new LowerJuliaArrayFunctionPass();
}  
  
}
  
