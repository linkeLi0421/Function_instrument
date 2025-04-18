#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Module.h"


using namespace llvm;

namespace {
struct Print_trace : PassInfoMixin<Print_trace> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    // Skip function declarations (external functions)
    if (F.isDeclaration())
      return PreservedAnalyses::all();

    Module *M = F.getParent();
    LLVMContext &Ctx = F.getContext();

    // Look up or create the external runtime function 'print_func_name'
    // which should have the signature: void print_func_name(const char*)
    Function *printFunc = M->getFunction("__print_func_name");
    if (!printFunc) {
      FunctionType *FT = FunctionType::get(
        Type::getVoidTy(Ctx),
        { Type::getInt8Ty(Ctx)->getPointerTo() },
        false);
      printFunc = Function::Create(FT, Function::ExternalLinkage, "__print_func_name", M);
    }
  
    // Insert before first non-PHI instruction
    BasicBlock &Entry = F.getEntryBlock();
    Instruction *FirstInst = &Entry.front();
    IRBuilder<> Builder(FirstInst);
    
    if (isa<PHINode>(FirstInst)) {
      Builder.SetInsertPoint(Entry.getFirstNonPHI());
    } else {
      Builder.SetInsertPoint(&*Entry.begin());
    }

    // Create a global string for the function's own name.
    Value *funcName = Builder.CreateGlobalStringPtr(F.getName());
    // Insert a call to print_func_name with the function name.
    Builder.CreateCall(printFunc, {funcName});
  
      // Since we modified the IR, we do not preserve any analyses.
      return PreservedAnalyses::none();
    }
  
    // Ensure the pass runs even for functions marked with optnone.
    static bool isRequired() { return true; }
};
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo Print_tracePluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Print_trace", LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      // Register once at the end of the optimization pipeline
      PB.registerOptimizerLastEPCallback(
        [](ModulePassManager &MPM, OptimizationLevel OL) {
          MPM.addPass(createModuleToFunctionPassAdaptor(Print_trace()));
        });
    }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return Print_tracePluginInfo();
}