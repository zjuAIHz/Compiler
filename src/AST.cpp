/**
 * @file AST.cpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-05-17
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <cstdio>
#include <iostream>
#include <vector>
#include "AST.hpp"
#include "IRGenerator.hpp"


/**
 * @brief 考虑到逻辑运算中需要用到true or false的判断
 * 通过该函数把类型转换成bool型（即1bit）
 * 
 */
llvm::Value* ToBoolType(llvm::Value* value, IRGenerator& IRContext) {
	auto IRBuilder = IRContext.IRBuilder; 
	if (value->getType() == IRBuilder->getInt1Ty()) return value;
	else if (value->getType()->isIntegerTy()){
		auto v2 = llvm::ConstantInt::get((llvm::IntegerType*)value->getType(), 0, true);
		return IRBuilder->CreateICmpNE(value, v2);
	}else{
		return NULL;
	}
}

/**
 * @brief 涉及到类型转换
 * 目前支持：
 * 1. 类型相同
 * 2. 转换成bool
 * 
 */
llvm::Value* CastType(llvm::Value* value, llvm::Type* type, IRGenerator& IRContext){
	auto IRBuilder = IRContext.IRBuilder;
	if(value->getType() == type){
		return value;
	}else if(type == IRBuilder->getInt1Ty()){
		return ToBoolType(value, IRContext);
	}
	// else if(value=>get){

	// }
}
/**
 * @brief 
 * 
 * 
 */

VarType::VarType(std::string name) {
	if (name == "int") type = Int; 
	else if (name == "char") type = Char; 
	else if (name == "short") type = Short;
} 

llvm::Type* VarType::ToLLVMType(IRGenerator& IRContext) {
	auto IRBuilder = IRContext.IRBuilder;
	switch(this->type) {
		case Int: return IRBuilder->getInt32Ty(); 
		case Char: return IRBuilder->getInt8Ty(); 
		case Short: return IRBuilder->getInt16Ty(); 
	}
}

llvm::Type* ArrayType::ToLLVMType(IRGenerator& IRContext) {
	auto IRBuilder = IRContext.IRBuilder;
	llvm::Type* elemType = this->elemType_->ToLLVMType(IRContext);
	return llvm::ArrayType::get(elemType, this->size_);
}

/**
 * @brief 
 * 
 * 
 */


llvm::Value* ProgramAST::IRGen(IRGenerator& IRContext) {
    std::cout << "ProgramAST" << std::endl;

	for (auto compUnit : *(this->compUnit_)){
		if(compUnit){
			compUnit->IRGen(IRContext);
		}
	}

	return NULL;
}

llvm::Value* VarDeclAST::IRGen(IRGenerator& IRContext) {
	std::cout << "VarDeclAST" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 

	//创建变量
	auto AllocMem = IRBuilder->CreateAlloca(this->type_.ToLLVMType(IRContext), 0, this->varDef_->varName_);
	
	// llvm::Value* initVal = CastType(this->, IRContext)

	// initialize
	llvm::Value* value = this->varDef_->IRGen(IRContext);

	// store will always align to 4, even for char, which is because we need a type cast for 'value'
	IRBuilder->CreateStore(value, AllocMem);

	IRContext.CreateVar(this->type_, this->varDef_->varName_, AllocMem);

	return NULL;
}

llvm::Value* VarDefAST::IRGen(IRGenerator& IRContext) {
	std::cout << "VarDefAST" << std::endl;
	
	if (this->initValue_) {
		return this->initValue_->IRGen(IRContext);
	}
	else {
		auto IRBuilder = IRContext.IRBuilder; 
		return IRBuilder->getInt8(0);
	}
}

llvm::Value* FuncDefAST::IRGen(IRGenerator& IRContext) {
    //Get return type
    std::cout << "FunctionAST" << std::endl;

    auto IRBuilder = IRContext.IRBuilder; 
    llvm::Type* ReturnType; 
    if (this->type_.GetType() == Int)
        ReturnType = IRBuilder->getInt32Ty();

    std::vector<llvm::Type*> ArgTypes; 

    //Get function type
    llvm::FunctionType* FuncType = llvm::FunctionType::get(ReturnType, ArgTypes, false);
    //Create function
    llvm::Function* Func = llvm::Function::Create(FuncType, llvm::Function::ExternalLinkage, this->funcName_, IRContext.Module);

	IRContext.SetCurFunc(Func);
	IRContext.ClearPreBrSignal();

    this->block_->IRGen(IRContext);

	IRContext.SetBasicBlock(NULL); 
	IRContext.SetCurFunc(NULL); 
    return NULL;
}

llvm::Value* BlockAST::IRGen(IRGenerator& IRContext) {
    std::cout << "BlockAST" << std::endl;

	// // Empty Block
	// if (this->stmts_.size() == 0) {
	// 	IRContext.SetPreBrSignal();
	// 	return NULL; 
	// }

	auto IRBuilder = IRContext.IRBuilder; 

	llvm::Function* Func = IRContext.GetCurFunc();
	llvm::BasicBlock* newBlock = llvm::BasicBlock::Create(*(IRContext.Context), "BBEntry", Func);
	auto prevBB = IRContext.GetBasicBlock(); 

	bool isConn = IRContext.ClearPreBrSignal(); 
	if (isConn) {
		// maybe go into a function, thus not insert Br
		IRBuilder->CreateBr(newBlock);
	}
	IRContext.SetPreBrSignal();

	IRContext.SetBasicBlock(this); 
    IRBuilder->SetInsertPoint(newBlock);

	for (auto stmt : *(this->stmts_)){
		if(stmt){
			stmt->IRGen(IRContext);
		}
	}

	IRContext.DiscardVar();

	IRContext.SetBasicBlock(prevBB); 
	if (isConn) {
		llvm::BasicBlock* outBlock = llvm::BasicBlock::Create(*(IRContext.Context), "BBExit", Func);
		IRBuilder->CreateBr(outBlock);
		IRBuilder->SetInsertPoint(outBlock);
	}

    return newBlock; 
}

llvm::Value* ReturnStmtAST::IRGen(IRGenerator& IRContext) {
    std::cout << "ReturnAST" << std::endl;
    auto IRBuilder = IRContext.IRBuilder; 
    IRBuilder->CreateRet(this->RetVal_->IRGen(IRContext));
    return NULL; 
}

llvm::Value* IfElseStmtAST::IRGen(IRGenerator& IRContext) {
	std::cout << "IfElseStmtAST" << std::endl;

	auto IRBuilder = IRContext.IRBuilder; 

	auto CondExpr = this->cond_->IRGen(IRContext);
	llvm::BasicBlock* CondBlock = IRBuilder->GetInsertBlock();

	IRContext.ClearPreBrSignal();
	llvm::BasicBlock* IfBlock = (llvm::BasicBlock*)this->ifBlock_->IRGen(IRContext);
	llvm::BasicBlock* IfOutBlock = IRBuilder->GetInsertBlock(); 
	llvm::BasicBlock* ElseBlock, *ElseOutBlock; 
	if (this->elseBlock_) {
		IRContext.ClearPreBrSignal();
		ElseBlock = (llvm::BasicBlock*)this->elseBlock_->IRGen(IRContext);
		ElseOutBlock = IRBuilder->GetInsertBlock(); 
	}

	// set exit 
	llvm::Function* Func = IRContext.GetCurFunc();
	llvm::BasicBlock* OutBlock = llvm::BasicBlock::Create(*(IRContext.Context), "BBExit", Func);
	IRBuilder->SetInsertPoint(IfOutBlock);
	IRBuilder->CreateBr(OutBlock);
	if (this->elseBlock_) {
		IRBuilder->SetInsertPoint(ElseOutBlock);
		IRBuilder->CreateBr(OutBlock);
	}

	// set conditional branch
	IRBuilder->SetInsertPoint(CondBlock);
	IRBuilder->CreateCondBr(CondExpr, IfBlock, this->elseBlock_?ElseBlock:OutBlock);

	IRBuilder->SetInsertPoint(OutBlock);

	return NULL;
}

llvm::Value* ForStmtAST::IRGen(IRGenerator& IRContext) {
	std::cout << "ForStmtAST" << std::endl;

	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Function* Func = IRContext.GetCurFunc();

	// init generate
	if (this->initStmt_)
		this->initStmt_->IRGen(IRContext); 
	llvm::BasicBlock* cmpBlock = llvm::BasicBlock::Create(*(IRContext.Context), "ForCmp", Func);
	IRBuilder->CreateBr(cmpBlock);

	// condition generate
	IRBuilder->SetInsertPoint(cmpBlock);
	auto cmpRes = (this->condExpr_)?this->condExpr_->IRGen(IRContext):IRBuilder->getInt1(true); 

	// body generate
	IRContext.ClearPreBrSignal();
	llvm::BasicBlock* bodyBlock = (llvm::BasicBlock*)this->forBody_->IRGen(IRContext);
	llvm::BasicBlock* bodyOutBlock = IRBuilder->GetInsertBlock();
	IRBuilder->SetInsertPoint(bodyOutBlock);
	// iteration generate
	if (this->iterStmt_) this->iterStmt_->IRGen(IRContext);
	IRBuilder->CreateBr(cmpBlock);

	// exit generate
	llvm::BasicBlock* exitBlock = llvm::BasicBlock::Create(*(IRContext.Context), "ForExit", Func);

	IRBuilder->SetInsertPoint(cmpBlock);
	IRBuilder->CreateCondBr(cmpRes, bodyBlock, exitBlock);
	IRBuilder->SetInsertPoint(exitBlock);

	return NULL;
}


/**
 * @brief 算术运算
 * 
 */
llvm::Value* MoncPlus::IRGen(IRGenerator& IRContext) {
	std::cout << "ExprAST +1" << std::endl;
	llvm::Value* val = this->RHS_->IRGen(IRContext);
	return val;

}

llvm::Value* MoncMinus::IRGen(IRGenerator& IRContext) {
	std::cout << "ExprAST -1" << std::endl;
	llvm::Value* val = this->RHS_->IRGen(IRContext);
	auto IRBuilder = IRContext.IRBuilder; 
	return IRBuilder->CreateNeg(val);

}

llvm::Value* Addition::IRGen(IRGenerator& IRContext) {
    std::cout << "ExprAST +" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = this->LHS_->IRGen(IRContext);
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	return IRBuilder->CreateAdd(LHS, RHS);
}

llvm::Value* Subtraction::IRGen(IRGenerator& IRContext) {
    std::cout << "ExprAST -" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = this->LHS_->IRGen(IRContext);
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	return IRBuilder->CreateSub(LHS, RHS);
}

llvm::Value* Multiplication::IRGen(IRGenerator& IRContext) {
    std::cout << "ExprAST *" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = this->LHS_->IRGen(IRContext);
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	return IRBuilder->CreateMul(LHS, RHS);
}

llvm::Value* Division::IRGen(IRGenerator& IRContext) {
    std::cout << "ExprAST /" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = this->LHS_->IRGen(IRContext);
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	return IRBuilder->CreateSDiv(LHS, RHS);
}

llvm::Value* Modulation::IRGen(IRGenerator& IRContext) {
    std::cout << "ExprAST %" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = this->LHS_->IRGen(IRContext);
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	return IRBuilder->CreateSRem(LHS, RHS);
}


/**
 * @brief 逻辑运算
 * 
 */
llvm::Value* LogicNot::IRGen(IRGenerator& IRContext) {
	std::cout << "ExprAST !" << std::endl;
	llvm::Value* val = this->RHS_->IRGen(IRContext);
	auto IRBuilder = IRContext.IRBuilder;
	return IRBuilder->CreateICmpEQ(ToBoolType(val, IRContext), IRBuilder->getInt1(false));
}

llvm::Value* AndOp::IRGen(IRGenerator& IRContext) {
	std::cout << "ExprAST &&" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = ToBoolType(this->LHS_->IRGen(IRContext), IRContext);
	llvm::Value* RHS = ToBoolType(this->RHS_->IRGen(IRContext), IRContext);
	return IRBuilder->CreateLogicalAnd(LHS, RHS);
}

llvm::Value* OrOp::IRGen(IRGenerator& IRContext) {
	std::cout << "ExprAST ||" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = ToBoolType(this->LHS_->IRGen(IRContext), IRContext);
	llvm::Value* RHS = ToBoolType(this->RHS_->IRGen(IRContext), IRContext);
	return IRBuilder->CreateLogicalOr(LHS, RHS);
}

llvm::Value* Equal::IRGen(IRGenerator& IRContext) {
	std::cout << "ExprAST ==" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = this->LHS_->IRGen(IRContext);
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	return IRBuilder->CreateICmpEQ(LHS, RHS);
}

llvm::Value* NotEqual::IRGen(IRGenerator& IRContext) {
	std::cout << "ExprAST !=" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = this->LHS_->IRGen(IRContext);
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	return IRBuilder->CreateICmpNE(LHS, RHS);
}

llvm::Value* GreThan::IRGen(IRGenerator& IRContext) {
	std::cout << "ExprAST >" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = this->LHS_->IRGen(IRContext);
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	return IRBuilder->CreateICmpSGT(LHS, RHS);
}

llvm::Value* LessThan::IRGen(IRGenerator& IRContext) {
	std::cout << "ExprAST >" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = this->LHS_->IRGen(IRContext);
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	return IRBuilder->CreateICmpSLT(LHS, RHS);
}

llvm::Value* GreEqu::IRGen(IRGenerator& IRContext) {
	std::cout << "ExprAST >" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = this->LHS_->IRGen(IRContext);
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	return IRBuilder->CreateICmpSGE(LHS, RHS);
}

llvm::Value* LessEqu::IRGen(IRGenerator& IRContext) {
	std::cout << "ExprAST >" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	llvm::Value* LHS = this->LHS_->IRGen(IRContext);
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	return IRBuilder->CreateICmpSLE(LHS, RHS);
}

llvm::Value* AssignAST::IRGen(IRGenerator& IRContext){
	std::cout << "Assign" << std::endl;
	auto IRBuilder = IRContext.IRBuilder;
	llvm::Value* RHS = this->RHS_->IRGen(IRContext);
	llvm::Value* LHSPtr = this->LHS_->IRGenPtr(IRContext);

	//赋值
	IRBuilder->CreateStore(RHS, LHSPtr);
	return RHS; 

}

llvm::Value* Constant::IRGen(IRGenerator& IRContext) {
	std::cout << "Constant" << std::endl;
	auto IRBuilder = IRContext.IRBuilder; 
	return IRBuilder->getInt32(this->int_);
}

llvm::Value* LeftValAST::IRGen(IRGenerator& IRContext) {
	std::cout << "LeftVal" << std::endl;
	auto IRBuilder = IRContext.IRBuilder;
	llvm::Value* VarPtr = IRContext.FindVar(this->name_);
	llvm::Type* type = VarPtr->getType()->getNonOpaquePointerElementType();
	llvm::Value* val = IRBuilder->CreateLoad(type,VarPtr);
	return val;
}

llvm::Value* LeftValAST::IRGenPtr(IRGenerator& IRContext) {
	std::cout << "LeftValPtr" << std::endl;
	auto IRBuilder = IRContext.IRBuilder;
	llvm::Value* VarPtr = IRContext.FindVar(this->name_);
	return VarPtr;
}