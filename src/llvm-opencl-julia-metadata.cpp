#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/LLVMContext.h"

#include <utility>

#define FOR_EACH_FUNCTION_AS_I(M, code) for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) code
    


namespace llvm {



struct RemoveJuliaMetadataPass : public ModulePass {
	static char ID;
	RemoveJuliaMetadataPass() : ModulePass(ID) {}

	virtual bool runOnModule(Module &M) {


    // Remove all Julia Metadata
    FOR_EACH_FUNCTION_AS_I(M, {
      removeJuliaMetadata(I);
    })

		return false;
	}

  void removeJuliaMetadata(Function* F) {

		for (ilist_iterator<BasicBlock> bb = F->getBasicBlockList().begin();
			bb != F->getBasicBlockList().end(); ++bb)
		{
			for (ilist_iterator<Instruction> inst = bb->getInstList().begin();
				inst != bb->getInstList().end(); ++inst)
			{
				inst->setMetadata(LLVMContext::MD_dbg, NULL);
        inst->setMetadata(LLVMContext::MD_tbaa, NULL);
			}
		}
    
  }

};



char RemoveJuliaMetadataPass::ID = 0;
static RegisterPass<RemoveJuliaMetadataPass> X("RemoveJuliaMetadataPass", "Remove Julia Metadata Pass", false, false);

Pass* createRemoveJuliaMetadataPass() {
	return new RemoveJuliaMetadataPass();
}

}
