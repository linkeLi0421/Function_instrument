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
  
      // Insert instrumentation at the beginning of the function (the entry block).
      BasicBlock &Entry = F.getEntryBlock();
      IRBuilder<> Builder(&*Entry.getFirstInsertionPt());
  
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
            // Register for opt -passes=Print_trace
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "Print_trace") {
                    FPM.addPass(Print_trace());
                    return true;
                  }
                  return false;
                });
                
            // Register for clang frontend action - early in the optimization pipeline
            PB.registerOptimizerEarlyEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel OL) {
                  MPM.addPass(createModuleToFunctionPassAdaptor(Print_trace()));
                });
                
            // Additionally, register for scalar optimizer (middle of pipeline)
            PB.registerScalarOptimizerLateEPCallback(
                [](FunctionPassManager &FPM, OptimizationLevel OL) {
                  FPM.addPass(Print_trace());
                });
                
            // Register as peephole optimization (late in pipeline)
            PB.registerPeepholeEPCallback(
                [](FunctionPassManager &FPM, OptimizationLevel OL) {
                  FPM.addPass(Print_trace());
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return Print_tracePluginInfo();
}