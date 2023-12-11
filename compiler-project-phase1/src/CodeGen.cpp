#include "CodeGen.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// Define a visitor class for generating LLVM IR from the AST.
namespace
{
  class ToIRVisitor : public ASTVisitor
  {
    Module *M;
    IRBuilder<> Builder;
    Type *VoidTy;
    Type *Int32Ty;
    Type *Int8PtrTy;
    Type *Int8PtrPtrTy;
    Constant *Int32Zero;

    Value *V;
    StringMap<AllocaInst *> nameMap;

  public:
    // Constructor for the visitor class.
    ToIRVisitor(Module *M) : M(M), Builder(M->getContext())
    {
      // Initialize LLVM types and constants.
      VoidTy = Type::getVoidTy(M->getContext());
      Int32Ty = Type::getInt32Ty(M->getContext());
      Int8PtrTy = Type::getInt8PtrTy(M->getContext());
      Int8PtrPtrTy = Int8PtrTy->getPointerTo();
      Int32Zero = ConstantInt::get(Int32Ty, 0, true);
    }

    // Entry point for generating LLVM IR from the AST.
    void run(AST *Tree)
    {
      // Create the main function with the appropriate function type.
      FunctionType *MainFty = FunctionType::get(Int32Ty, {Int32Ty, Int8PtrPtrTy}, false);
      Function *MainFn = Function::Create(MainFty, GlobalValue::ExternalLinkage, "main", M);

      // Create a basic block for the entry point of the main function.
      BasicBlock *BB = BasicBlock::Create(M->getContext(), "entry", MainFn);
      Builder.SetInsertPoint(BB);

      // Visit the root node of the AST to generate IR.
      Tree->accept(*this);

      // Create a return instruction at the end of the main function.
      Builder.CreateRet(Int32Zero);
    }

    // Visit function for the GSM node in the AST.
    virtual void visit(Goal &Node) override
    {
      // Iterate over the children of the GSM node and visit each child.
      for (auto I = Node.begin(), E = Node.end(); I != E; ++I)
      {
        (*I)->accept(*this);
      }
    };

    virtual void visit(Assignment &Node) override
    {
      // Visit the right-hand side of the assignment and get its value.
      Node.getRight()->accept(*this);
      Value *val = V;

      // Get the name of the variable being assigned.
      auto varName = Node.getLeft()->getVal();

      // Create a store instruction to assign the value to the variable.
      Builder.CreateStore(val, nameMap[varName]);

      // Create a function type for the "gsm_write" function.
      FunctionType *CalcWriteFnTy = FunctionType::get(VoidTy, {Int32Ty}, false);

      // Create a function declaration for the "gsm_write" function.
      Function *CalcWriteFn = Function::Create(CalcWriteFnTy, GlobalValue::ExternalLinkage, "gsm_write", M);

      // Create a call instruction to invoke the "gsm_write" function with the value.
      CallInst *Call = Builder.CreateCall(CalcWriteFnTy, CalcWriteFn, {val});
    };

    virtual void visit(Factor &Node) override
    {
      if (Node.getKind() == Factor::Ident)
      {
        // If the factor is an identifier, load its value from memory.
        V = Builder.CreateLoad(Int32Ty, nameMap[Node.getVal()]);
      }
      else
      {
        // If the factor is a literal, convert it to an integer and create a constant.
        int intval;
        Node.getVal().getAsInteger(10, intval);
        V = ConstantInt::get(Int32Ty, intval, true);
      }
    };

    virtual void visit(BinaryOp &Node) override
    {
      // Visit the left-hand side of the binary operation and get its value.
      Node.getLeft()->accept(*this);
      Value *Left = V;

      // Visit the right-hand side of the binary operation and get its value.
      Node.getRight()->accept(*this);
      Value *Right = V;

      // Perform the binary operation based on the operator type and create the corresponding instruction.
      switch (Node.getOperator())
      {
      case BinaryOp::Plus:
        V = Builder.CreateNSWAdd(Left, Right);
        break;
      case BinaryOp::Minus:
        V = Builder.CreateNSWSub(Left, Right);
        break;
      case BinaryOp::Mul:
        V = Builder.CreateNSWMul(Left, Right);
        break;
      case BinaryOp::Div:
        V = Builder.CreateSDiv(Left, Right);
        break;
      case BinaryOp::Power:
        V = left;
        for (int i = 0; i < Right; i++)
        {
          V = Builder.CreateNSWMul(V, Left);
        }
        break;
      case BinaryOp::Remain:
        V = Builder.CreateSRem(Left, Right);
        break;
      case BinaryOp::Or:
        V = Builder.CreateOr(Left, Right);
        break;
      case BinaryOp::And:
        V = Builder.CreateAnd(Left, Right);
        break;
      case BinaryOp::Equal_equal:
        V = Builder.CreateICmpEQ(Left, Right);
        break;
      case BinaryOp::Not_equal:
        V = Builder.CreateICmpNE(Left, Right);
        break;
      case BinaryOp::More_equal:
        V = Builder.CreateICmpSGE(Left, Right);
        break;
      case BinaryOp::Less_equal:
        V = Builder.CreateICmpSLE(Left, Right);
        break;
      case BinaryOp::Less:
        V = Builder.CreateICmpSLT(Left, Right);
        break;
      case BinaryOp::More:
        V = Builder.CreateICmpSGT(Left, Right);
        break;
      }
    };

    virtual void visit(Declaration &Node) override
    {

      // Our code

      // Iterate over the variables declared in the declaration statement.
      for (auto I = Node.begin(), E = Node.end(), e_I = Node.begin_values(), e_E = Node.end_values(); I != E; ++I, ++e_I)
      {
        StringRef Var = *I;
        Value *val = nullptr;
        // Create an alloca instruction to allocate memory for the variable.
        nameMap[Var] = Builder.CreateAlloca(Int32Ty);

        if (*e_I)
        {
          (*e_I)->accept(*this);

          val = V;

          if (val != nullptr)
          {
            Builder.CreateStore(val, nameMap[Var]);
          }
        }
        else
        {
          Builder.CreateStore(0, nameMap[Var]);
        }
      }
    };

    virtual void visit(BE &Node) override
    {
      for (auto I = Node.begin(), E = Node.end(); I != E; ++I)
      {
        if (*I)
        {

          (Assignment *)I->accept(*this);
        }
      }
    };

    virtual void visit(Loop &Node) override
    {

      // Our code
      Value *val = nullptr;
      BE *be = (BE *)Node.getBE();
      if (Node.getExpr())
      {
        Node.getExpr()->accept(*this);
        val = V;

        if (val)
        {
          be->accept(*this);
        }
      }
    };

    virtual void visit(Condition &Node) override
    {

      // Our code

      int count_exprs = 0;
      int count_bes = 0;
      bool flag_has_been_true = false;
      for (auto I = Node.exprs_begin(), E = Node.exprs_end(); I != E; ++I)
      {
        count_exprs++;
      }
      for (auto I = Node.bes_begin(), E = Node.bes_end(); I != E; ++I)
      {
        count_bes++;
      }

      for (auto I = Node.exprs_begin(), E = Node.exprs_begin(), bes_I = Node.bes_begin(), bes_E = Node.bes_end(); I != E; ++I, ++bes_I)
      {
        if (*I)
        {
          (*I)->accept(*this);
          val = V;
          flag_has_been_true = true;
          if (val)
          {
            if (bes_I)
            {
              (*bes_I)->accept(*this);
            }

            break;
          }
        }
      }
      if (count_bes + 1 == count_exprs && !flag_has_been_true)
      {
        (*Node.bes_end())->accept(this *);
      }
    };
  };
}; // namespace

void CodeGen::compile(AST *Tree)
{
  // Create an LLVM context and a module.
  LLVMContext Ctx;
  Module *M = new Module("calc.expr", Ctx);

  // Create an instance of the ToIRVisitor and run it on the AST to generate LLVM IR.
  ToIRVisitor ToIR(M);
  ToIR.run(Tree);

  // Print the generated module to the standard output.
  M->print(outs(), nullptr);
}