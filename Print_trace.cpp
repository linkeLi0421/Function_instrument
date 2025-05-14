#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Module.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/Support/raw_ostream.h"


using namespace llvm;

namespace {
struct Print_trace : PassInfoMixin<Print_trace> {

  static StringRef buildPrettySignature(Function &F) {
    // Create the string as a static local variable to ensure its lifetime
    static std::string PrettySignature;
    PrettySignature.clear();
    
    std::string DemName = demangle(F.getName().str());
    FunctionType *FT = F.getFunctionType();
    raw_string_ostream OS(PrettySignature);
    FT->getReturnType()->print(OS);
    OS << " " << DemName << "(";
    for (unsigned i = 0, e = FT->getNumParams(); i != e; ++i) {
      FT->getParamType(i)->print(OS);
      if (i + 1 != e) OS << ", ";
    }
    OS << ")";
    OS.flush();
    return PrettySignature;
  }


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

    // Get function line number from debug info if available
    unsigned lineNumber = 0;
    unsigned columnNumber = 0;
    StringRef filename = "";
    if (DISubprogram *SP = F.getSubprogram()) {
      lineNumber = SP->getLine();
      if (const DILocation *Loc = SP->getScopeLine() > 0 ? 
          DILocation::get(Ctx, SP->getScopeLine(), 1, SP->getScope()) : nullptr) {
        columnNumber = Loc->getColumn();
      }
      filename = SP->getFilename();
    }

    // Prepare the parameters: function name and line number
    Value *funcName = Builder.CreateGlobalStringPtr(F.getName());
    Value *filenameVal = Builder.CreateGlobalStringPtr(filename);
    Value *lineVal = Builder.getInt32(lineNumber);
    Value *colVal = Builder.getInt32(columnNumber);
    // Insert a call to print_func_name with the function name.
    Value* args[] = {funcName, filenameVal, lineVal, colVal};
    Builder.CreateCall(printFunc, args);
  
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