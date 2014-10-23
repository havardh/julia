#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/LLVMContext.h"

#include <utility>


namespace llvm {



struct RemoveJuliaMetadataPass : public FunctionPass {
	static char ID;
	RemoveJuliaMetadataPass() : FunctionPass(ID) {}

	virtual bool runOnFunction(Function &F) {

	 	F.setName(Twine("at"));

		for (ilist_iterator<BasicBlock> bb = F.getBasicBlockList().begin();
			bb != F.getBasicBlockList().end(); ++bb)
		{
			for (ilist_iterator<Instruction> inst = bb->getInstList().begin();
				inst != bb->getInstList().end(); ++inst)
			{
				inst->setMetadata(LLVMContext::MD_dbg, NULL);
				inst->setMetadata(LLVMContext::MD_tbaa, NULL);
			}
		}

		return false;
	}

};



char RemoveJuliaMetadataPass::ID = 0;
static RegisterPass<RemoveJuliaMetadataPass> X("RemoveJuliaMetadataPass", "Remove Julia Metadata Pass", false, false);

Pass* createRemoveJuliaMetadataPass() {
	return new RemoveJuliaMetadataPass();
}

}
