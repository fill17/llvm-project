#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <string>
#include <memory>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory VarRenamerCategory("var-renamer options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("\nRenames variables with prefixes based on their category.\n");

class VarRenamerVisitor : public RecursiveASTVisitor<VarRenamerVisitor> {
public:
  explicit VarRenamerVisitor(ASTContext *Context, Rewriter &R,
                             std::map<const VarDecl *, std::string> &RenameMap)
      : Context(Context), Rewrite(R), RenameMap(RenameMap) {}

  bool VisitVarDecl(VarDecl *VD) {
    if (!Context->getSourceManager().isInMainFile(VD->getLocation()))
      return true;

    std::string NewName;
    if (isa<ParmVarDecl>(VD)) {
      NewName = "param_" + VD->getNameAsString();
    } else if (VD->isFileVarDecl()) {
      NewName = "global_" + VD->getNameAsString();
    } else if (VD->isStaticLocal()) {
      NewName = "static_" + VD->getNameAsString();
    } else {
      NewName = "local_" + VD->getNameAsString();
    }
    RenameMap[VD] = NewName;
    return true;
  }

private:
  ASTContext *Context;
  Rewriter &Rewrite;
  std::map<const VarDecl *, std::string> &RenameMap;
};

class VarRenamerConsumer : public ASTConsumer {
public:
  explicit VarRenamerConsumer(ASTContext *Context, Rewriter &R)
      : Visitor(Context, R, RenameMap), Rewrite(R) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    // Первый проход: собираем все объявления переменных
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());

    // Второй проход: заменяем имена в объявлениях
    for (auto &Pair : RenameMap) {
      const VarDecl *VD = Pair.first;
      const std::string &NewName = Pair.second;
      SourceLocation Loc = VD->getLocation();
      if (Loc.isValid()) {
        unsigned OldLen = VD->getNameAsString().length();
        Rewrite.ReplaceText(Loc, OldLen, NewName);
      }
    }

    // Третий проход: заменяем все использования переменных
    struct RefVisitor : public RecursiveASTVisitor<RefVisitor> {
      RefVisitor(ASTContext &C, Rewriter &R, std::map<const VarDecl *, std::string> &Map)
          : Context(C), Rewrite(R), RenameMap(Map) {}

      bool VisitDeclRefExpr(DeclRefExpr *DRE) {
        if (auto *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
          auto It = RenameMap.find(VD);
          if (It != RenameMap.end()) {
            SourceLocation Loc = DRE->getLocation();
            if (Loc.isValid()) {
              unsigned OldLen = VD->getNameAsString().length();
              Rewrite.ReplaceText(Loc, OldLen, It->second);
            }
          }
        }
        return true;
      }

      ASTContext &Context;
      Rewriter &Rewrite;
      std::map<const VarDecl *, std::string> &RenameMap;
    };

    RefVisitor RefVis(Context, Rewrite, RenameMap);
    RefVis.TraverseDecl(Context.getTranslationUnitDecl());

    // Вывод изменённого файла
    Rewrite.getEditBuffer(Context.getSourceManager().getMainFileID())
        .write(outs());
  }

private:
  VarRenamerVisitor Visitor;
  Rewriter &Rewrite; 
  std::map<const VarDecl *, std::string> RenameMap;
};

class VarRenamerAction : public ASTFrontendAction {
public:
  void EndSourceFileAction() override {}

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef File) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<VarRenamerConsumer>(&CI.getASTContext(), TheRewriter);
  }

private:
  Rewriter TheRewriter; 
};

int main(int argc, const char **argv) {
  auto ExpectedParser = CommonOptionsParser::create(argc, argv, VarRenamerCategory);
  if (!ExpectedParser) {
    errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser &OptionsParser = ExpectedParser.get();
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  return Tool.run(newFrontendActionFactory<VarRenamerAction>().get());
}
