#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <memory>
#include <optional>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory MulToShiftCategory("mul-to-shift options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("\nReplaces integer multiplication by power of two with left shift.\n");

bool isPowerOfTwo(uint64_t n) {
    return n && !(n & (n - 1));
}

unsigned getLog2(uint64_t n) {
    unsigned count = 0;
    while (n > 1) {
        n >>= 1;
        ++count;
    }
    return count;
}

class MulToShiftVisitor : public RecursiveASTVisitor<MulToShiftVisitor> {
public:
    explicit MulToShiftVisitor(ASTContext *Context, Rewriter &R)
        : Context(Context), Rewrite(R) {}

    bool VisitBinaryOperator(BinaryOperator *BO) {
        if (BO->getOpcode() != BO_Mul)
            return true;

        Expr *LHS = BO->getLHS();
        Expr *RHS = BO->getRHS();

        if (!LHS->getType()->isIntegerType() || !RHS->getType()->isIntegerType())
            return true;

        auto LHSConst = LHS->isIntegerConstantExpr(*Context);
        auto RHSConst = RHS->isIntegerConstantExpr(*Context);

        Expr *NonConstExpr = nullptr;
        unsigned shiftAmount = 0;
        bool found = false;

        if (RHSConst && isPowerOfTwo(RHSConst->getZExtValue())) {
            NonConstExpr = LHS;
            shiftAmount = getLog2(RHSConst->getZExtValue());
            found = true;
        } else if (LHSConst && isPowerOfTwo(LHSConst->getZExtValue())) {
            NonConstExpr = RHS;
            shiftAmount = getLog2(LHSConst->getZExtValue());
            found = true;
        }

        if (!found)
            return true;

        // Получаем текст неконстантного операнда
        SourceRange nonConstRange = NonConstExpr->getSourceRange();
        std::string nonConstText;
        if (Rewrite.getRewrittenText(nonConstRange).size() > 0) {
            nonConstText = Rewrite.getRewrittenText(nonConstRange).str();
        } else {
            SourceManager &SM = Context->getSourceManager();
            const char *Start = SM.getCharacterData(nonConstRange.getBegin());
            const char *End = SM.getCharacterData(nonConstRange.getEnd());
            nonConstText = std::string(Start, End - Start + 1);
        }

        std::string replacement = "(" + nonConstText + " << " + std::to_string(shiftAmount) + ")";
        Rewrite.ReplaceText(BO->getSourceRange(), replacement);

        return true;
    }

private:
    ASTContext *Context;
    Rewriter &Rewrite;
};

class MulToShiftConsumer : public ASTConsumer {
public:
    explicit MulToShiftConsumer(ASTContext *Context, Rewriter &R)
        : Visitor(Context, R), Rewrite(R) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
        Rewrite.getEditBuffer(Context.getSourceManager().getMainFileID())
            .write(outs());
    }

private:
    MulToShiftVisitor Visitor;
    Rewriter &Rewrite;  // ссылка, инициализируется в конструкторе
};

class MulToShiftAction : public ASTFrontendAction {
public:
    void EndSourceFileAction() override {}

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   StringRef File) override {
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<MulToShiftConsumer>(&CI.getASTContext(), TheRewriter);
    }

private:
    Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, MulToShiftCategory);
    if (!ExpectedParser) {
        errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(),
                   OptionsParser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<MulToShiftAction>().get());
}
