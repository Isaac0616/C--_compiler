#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "header.h"
#include "symbolTable.h"
#include "offsetInAR.h"
#include "myRegister.h"
#include "printSourceFile.h"

FILE* g_codeGenOutputFp = NULL;
char* g_currentFunctionName = NULL;

int getLabelNumber();
int codeGenConstantLabel(C_type constantType, void* valuePtr);
void codeGenGetBoolOfFloat(int boolRegIndex, int floatRegIndex);
void codeGenPrepareRegister(ProcessorType processorType, int regIndex, int needToBeLoaded, int workRegIndexIfPseudo, char** regName);
void codeGenSaveToMemoryIfPsuedoRegister(ProcessorType processorType, int regIndex, char* regName);
void codeGenFloatCompInstruction(char *instruction, int dstRegIndex, int srcReg1Index, int srcReg2Index);
void codeGenLogicalInstruction(ProcessorType processorType, char *instruction, int dstRegIndex, int srcReg1Index, int srcReg2Index);
//reg1 is dst
void codeGen2RegInstruction(ProcessorType processorType, char* instruction, int reg1Index, int reg2Index);
//reg1 is dst
void codeGen3RegInstruction(ProcessorType processorType, char* instruction, int reg1Index, int reg2Index, int reg3Index);
void codeGen2Reg1ImmInstruction(ProcessorType processorType, char* instruction, int reg1Index, int reg2Index, void* imm);
int codeGenConvertFromIntToFloat(int intRegIndex);
int codeGenConvertFromFloatToInt(int floatRegIndex);
//*************************

void codeGenProgramNode(AST_NODE *programNode);
void codeGenGlobalVariable(AST_NODE *varaibleDeclListNode);
void codeGenFunctionDeclaration(AST_NODE *functionDeclNode);
void codeGenGeneralNode(AST_NODE* node);
void codeGenStmtNode(AST_NODE* stmtNode);
void codeGenBlockNode(AST_NODE* blockNode);
void codeGenWhileStmt(AST_NODE* whileStmtNode);
void codeGenForStmt(AST_NODE* forStmtNode);
void codeGenIfStmt(AST_NODE* ifStmtNode);
void codeGenReturnStmt(AST_NODE* returnStmtNode);
void codeGenAssignOrExpr(AST_NODE* testNode);
void codeGenAssignmentStmt(AST_NODE* assignmentStmtNode);
void codeGenExprRelatedNode(AST_NODE* exprRelatedNode);
void codeGenExprNode(AST_NODE* exprNode);
void codeGenFunctionCall(AST_NODE* functionCallNode);
void codeGenVariableReference(AST_NODE* idNode);
void codeGenConstantReference(AST_NODE* constantNode);
int codeGenCalcArrayElemenetAddress(AST_NODE* idNode);
void codeGenStoreArgument(AST_NODE *traverseParameter, Parameter* formalParameter);
int codeGenCalcArrayPointerAddress(AST_NODE *idNode);

int getLabelNumber()
{
    static int labelNumber = 0;
    return labelNumber++;
}


int codeGenConstantLabel(C_type constantType, void* valuePtr)
{
    int labelNumber = getLabelNumber();
    
    fprintf(g_codeGenOutputFp, ".data\n");

    if(constantType == INTEGERC)
    {
        int* val = (int*)valuePtr;
        fprintf(g_codeGenOutputFp, "_CONSTANT_%d: .word %d\n", labelNumber, *val);
    }
    else if(constantType == FLOATC)
    {
        float* val = (float*)valuePtr;
        fprintf(g_codeGenOutputFp, "_CONSTANT_%d: .float %f\n", labelNumber, *val);
    }
    else if(constantType == STRINGC)
    {
        char* val = (char*)valuePtr;
        fprintf(g_codeGenOutputFp, "_CONSTANT_%d: .asciiz %s\n", labelNumber, val);
        fprintf(g_codeGenOutputFp, ".align 2\n");
    }

    fprintf(g_codeGenOutputFp, ".text\n");

    return labelNumber;
}


void codeGenGetBoolOfFloat(int boolRegIndex, int floatRegIndex)
{
    float zero = 0.0f;
    int constantZeroLabelNumber = codeGenConstantLabel(FLOATC, &zero);

    char* tmpZeroRegName = floatWorkRegisterName[0];
    fprintf(g_codeGenOutputFp, "l.s %s, _CONSTANT_%d\n", tmpZeroRegName, constantZeroLabelNumber);
    char* origFloatRegName = NULL;
    codeGenPrepareRegister(FLOAT_REG, floatRegIndex, 1, 1, &origFloatRegName);
    fprintf(g_codeGenOutputFp, "c.eq.s %s, %s\n", tmpZeroRegName, origFloatRegName);
    
    char* boolRegName = NULL;
    codeGenPrepareRegister(INT_REG, boolRegIndex, 0, 0, &boolRegName);
    int tmpLabelIndex = getLabelNumber();
    fprintf(g_codeGenOutputFp, "bc1t _setFalse_%d\n", tmpLabelIndex);
    fprintf(g_codeGenOutputFp, "li %s, %d\n", boolRegName, 1);
    fprintf(g_codeGenOutputFp, "j _setBoolEnd_%d\n", tmpLabelIndex);
    fprintf(g_codeGenOutputFp, "_setFalse_%d:\n", tmpLabelIndex);
    fprintf(g_codeGenOutputFp, "li %s, %d\n", boolRegName, 0);
    fprintf(g_codeGenOutputFp, "_setBoolEnd_%d:\n", tmpLabelIndex);

    codeGenSaveToMemoryIfPsuedoRegister(INT_REG, boolRegIndex, boolRegName);
}


void codeGenPrepareRegister(ProcessorType processorType, int regIndex, int needToBeLoaded, int workRegIndexIfPseudo, char** regName)
{
    int realRegisterCount = (processorType == INT_REG) ? INT_REGISTER_COUNT : FLOAT_REGISTER_COUNT;
    char** realRegisterName = (processorType == INT_REG) ? intRegisterName : floatRegisterName;
    char** workRegisterName = (processorType == INT_REG) ? intWorkRegisterName : floatWorkRegisterName;
    char* loadInstruction = (processorType == INT_REG) ? "lw" : "l.s";

    if(regIndex >= realRegisterCount)
    {
        //pseudo register
        int pseudoIndex = regIndex - realRegisterCount;
        *regName = workRegisterName[workRegIndexIfPseudo];
        if(needToBeLoaded)
        {
            fprintf(g_codeGenOutputFp, "%s %s, %d($fp)\n", loadInstruction, *regName, getPseudoRegisterCorrespondingOffset(pseudoIndex));
        }
    }
    else
    {
        *regName = realRegisterName[regIndex];
    }
}


void codeGenSaveToMemoryIfPsuedoRegister(ProcessorType processorType, int regIndex, char* regName)
{
    int realRegisterCount = (processorType == INT_REG) ? INT_REGISTER_COUNT : FLOAT_REGISTER_COUNT;
    char* saveInstruction = (processorType == INT_REG) ? "sw" : "s.s";

    if(regIndex >= realRegisterCount)
    {
        //pseudo register
        int pseudoIndex = regIndex - realRegisterCount;
        fprintf(g_codeGenOutputFp, "%s %s, %d($fp)\n", saveInstruction, regName, getPseudoRegisterCorrespondingOffset(pseudoIndex));
    }
}


void codeGenFloatCompInstruction(char *notRealInstruction, int dstRegIndex, int srcReg1Index, int srcReg2Index)
{
    char* srcReg1Name = NULL;
    codeGenPrepareRegister(FLOAT_REG, srcReg1Index, 1, 0, &srcReg1Name);

    char* srcReg2Name = NULL;
    codeGenPrepareRegister(FLOAT_REG, srcReg2Index, 1, 1, &srcReg2Name);

    char* dstRegName = NULL;
    codeGenPrepareRegister(INT_REG, dstRegIndex, 0, 0, &dstRegName);

    char* realInstruction = notRealInstruction;
    int resultOfCompareTrue = 1;
    if(strcmp(notRealInstruction, "c.ne.s") == 0)
    {
        realInstruction = "c.eq.s";
        resultOfCompareTrue = 0;
    }
    else if(strcmp(notRealInstruction, "c.ge.s") == 0)
    {
        realInstruction = "c.lt.s";
        resultOfCompareTrue = 0;
    }
    else if(strcmp(notRealInstruction, "c.gt.s") == 0)
    {
        realInstruction = "c.le.s";
        resultOfCompareTrue = 0;
    }
    
    int tmpLabelIndex = getLabelNumber();
    fprintf(g_codeGenOutputFp, "%s %s, %s\n", realInstruction, srcReg1Name, srcReg2Name);
    fprintf(g_codeGenOutputFp, "bc1f _compareFalse_%d\n", tmpLabelIndex);
    fprintf(g_codeGenOutputFp, "li %s, %d\n", dstRegName, resultOfCompareTrue);
    fprintf(g_codeGenOutputFp, "j _compareEnd_%d\n", tmpLabelIndex);
    fprintf(g_codeGenOutputFp, "_compareFalse_%d:\n", tmpLabelIndex);
    fprintf(g_codeGenOutputFp, "li %s, %d\n", dstRegName, !resultOfCompareTrue);
    fprintf(g_codeGenOutputFp, "_compareEnd_%d:\n", tmpLabelIndex);
    
    codeGenSaveToMemoryIfPsuedoRegister(INT_REG, dstRegIndex, dstRegName);
}


void codeGenLogicalInstruction(ProcessorType processorType, char *instruction, int dstRegIndex, int srcReg1Index, int srcReg2Index)
{
    int boolReg1Index = -1;
    int boolReg2Index = -1;

    if(processorType == FLOAT_REG)
    {
        boolReg1Index = getRegister(INT_REG);
        boolReg2Index = getRegister(INT_REG);
        codeGenGetBoolOfFloat(boolReg1Index, srcReg1Index);
        codeGenGetBoolOfFloat(boolReg2Index, srcReg2Index);
    }
    else if(processorType == INT_REG)
    {
        int zero = 0;
        boolReg1Index = srcReg1Index;
        boolReg2Index = srcReg2Index;
        codeGen2Reg1ImmInstruction(INT_REG, "sne", boolReg1Index, srcReg1Index, &zero);
        codeGen2Reg1ImmInstruction(INT_REG, "sne", boolReg2Index, srcReg2Index, &zero);
    }

    codeGen3RegInstruction(INT_REG, instruction, dstRegIndex, boolReg1Index, boolReg2Index);
    
    if(processorType == FLOAT_REG)
    {
        freeRegister(INT_REG, boolReg1Index);
        freeRegister(INT_REG, boolReg2Index);
    }
}


void codeGen2RegInstruction(ProcessorType processorType, char* instruction, int reg1Index, int reg2Index)
{
    char* reg1Name = NULL;
    codeGenPrepareRegister(processorType, reg1Index, 0, 0, &reg1Name);
    
    char* reg2Name = NULL;
    codeGenPrepareRegister(processorType, reg2Index, 1, 1, &reg2Name);
    
    fprintf(g_codeGenOutputFp, "%s %s, %s\n", instruction, reg1Name, reg2Name);

    codeGenSaveToMemoryIfPsuedoRegister(processorType, reg1Index, reg1Name);
}


void codeGen3RegInstruction(ProcessorType processorType, char* instruction, int reg1Index, int reg2Index, int reg3Index)
{
    char* reg1Name = NULL;
    codeGenPrepareRegister(processorType, reg1Index, 0, 0, &reg1Name);
    
    char* reg2Name = NULL;
    codeGenPrepareRegister(processorType, reg2Index, 1, 0, &reg2Name);
    
    char* reg3Name = NULL;
    codeGenPrepareRegister(processorType, reg3Index, 1, 1, &reg3Name);
    
    fprintf(g_codeGenOutputFp, "%s %s, %s, %s\n", instruction, reg1Name, reg2Name, reg3Name);

    codeGenSaveToMemoryIfPsuedoRegister(processorType, reg1Index, reg1Name);
}


void codeGen2Reg1ImmInstruction(ProcessorType processorType, char* instruction, int reg1Index, int reg2Index, void* imm)
{
    char* reg1Name = NULL;
    codeGenPrepareRegister(processorType, reg1Index, 0, 0, &reg1Name);
    
    char* reg2Name = NULL;
    codeGenPrepareRegister(processorType, reg2Index, 1, 0, &reg2Name);
    
    if(processorType == INT_REG)
    {
        int* val = (int*)imm;
        fprintf(g_codeGenOutputFp, "%s %s, %s, %d\n", instruction, reg1Name, reg2Name, *val);
    }
    else if(processorType == FLOAT_REG)
    {
        float* val = (float*)imm;
        fprintf(g_codeGenOutputFp, "%s %s, %s, %f\n", instruction, reg1Name, reg2Name, *val);
    }

    codeGenSaveToMemoryIfPsuedoRegister(processorType, reg1Index, reg1Name);
}


int codeGenConvertFromIntToFloat(int intRegIndex)
{
    /*TODO*/
    char *intRegisterName, *floatRegisterName;
    int floatRegisterIndex = getRegister(FLOAT_REG);
    codeGenPrepareRegister(INT_REG, intRegIndex, 1, 0, &intRegisterName);
    codeGenPrepareRegister(FLOAT_REG, floatRegisterIndex, 0, 0, &floatRegisterName);

    fprintf(g_codeGenOutputFp, "mtc1 %s, %s\n", intRegisterName, floatRegisterName);
    fprintf(g_codeGenOutputFp, "cvt.s.w %s, %s\n", floatRegisterName, floatRegisterName);

    freeRegister(INT_REG, intRegIndex);

    return floatRegisterIndex;
}


int codeGenConvertFromFloatToInt(int floatRegIndex)
{
    /*TODO*/
    char *intRegisterName, *floatRegisterName;
    int intRegisterIndex = getRegister(INT_REG);
    codeGenPrepareRegister(INT_REG, intRegisterIndex, 1, 0, &intRegisterName);
    codeGenPrepareRegister(FLOAT_REG, floatRegIndex, 0, 0, &floatRegisterName);

    fprintf(g_codeGenOutputFp, "cvt.w.s %s, %s\n", floatRegisterName, floatRegisterName);
    fprintf(g_codeGenOutputFp, "mfc1 %s, %s\n", intRegisterName, floatRegisterName);

    freeRegister(FLOAT_REG, floatRegIndex);

    return intRegisterIndex;
}


void codeGenerate(AST_NODE *root)
{
    char* outputfileName = "output.s";
    g_codeGenOutputFp = fopen(outputfileName, "w");
    if(!g_codeGenOutputFp)
    {
        printf("Cannot open file \"%s\"", outputfileName);
        exit(EXIT_FAILURE);
    }

    codeGenProgramNode(root);
}


void codeGenProgramNode(AST_NODE *programNode)
{
    AST_NODE *traverseDeclaration = programNode->child;
    while(traverseDeclaration)
    {
        if(traverseDeclaration->nodeType == VARIABLE_DECL_LIST_NODE)
        {
            fprintf(g_codeGenOutputFp, ".data\n");
            codeGenGlobalVariable(traverseDeclaration);
            fprintf(g_codeGenOutputFp, ".text\n");
        }
        else if(traverseDeclaration->nodeType == DECLARATION_NODE)
        {
            codeGenFunctionDeclaration(traverseDeclaration);
        }
        traverseDeclaration = traverseDeclaration->rightSibling;
    }
    return;
}


void codeGenGlobalVariable(AST_NODE* varaibleDeclListNode)
{
    AST_NODE *traverseDeclaration = varaibleDeclListNode->child;
    while(traverseDeclaration)
    {
        if(traverseDeclaration->semantic_value.declSemanticValue.kind == VARIABLE_DECL)
        {
            AST_NODE *typeNode = traverseDeclaration->child;
            AST_NODE *idNode = typeNode->rightSibling;
            while(idNode)
            {
                /*TODO initial*/
                void *val;
                int intZero = 0;
                float floatZero = 0.0f;
                if(idNode->semantic_value.identifierSemanticValue.kind == WITH_INIT_ID) {
                    AST_NODE *constValueNode = idNode->child;
                    if(typeNode->dataType == INT_TYPE) 
                        val = &constValueNode->semantic_value.exprSemanticValue.constEvalValue.iValue;
                    else if(typeNode->dataType == FLOAT_TYPE) 
                        val = &constValueNode->semantic_value.exprSemanticValue.constEvalValue.fValue;
                }
                else {
                    if(typeNode->dataType == INT_TYPE) 
                        val = &intZero;
                    else if(typeNode->dataType == FLOAT_TYPE) 
                        val = &floatZero;
                }

                SymbolTableEntry* idSymbolTableEntry = idNode->semantic_value.identifierSemanticValue.symbolTableEntry;
                TypeDescriptor* idTypeDescriptor = idSymbolTableEntry->attribute->attr.typeDescriptor;
                if(idTypeDescriptor->kind == SCALAR_TYPE_DESCRIPTOR)
                {
                    if(idTypeDescriptor->properties.dataType == INT_TYPE)
                    {
                        fprintf(g_codeGenOutputFp, "_g_%s: .word %d\n", idSymbolTableEntry->name, *(int*)val);
                    }
                    else if(idTypeDescriptor->properties.dataType == FLOAT_TYPE)
                    {
                        fprintf(g_codeGenOutputFp, "_g_%s: .float %f\n", idSymbolTableEntry->name, *(float*)val);
                    }
                }
                else if(idTypeDescriptor->kind == ARRAY_TYPE_DESCRIPTOR)
                {
                    int variableSize = getVariableSize(idTypeDescriptor);
                    fprintf(g_codeGenOutputFp, "_g_%s: .space %d\n", idSymbolTableEntry->name, variableSize);
                }
                idNode = idNode->rightSibling;
            }
        }
        traverseDeclaration = traverseDeclaration->rightSibling;
    }
    return;
}


void codeGenFunctionDeclaration(AST_NODE *functionDeclNode)
{
    AST_NODE* functionIdNode = functionDeclNode->child->rightSibling;
    
    g_currentFunctionName = functionIdNode->semantic_value.identifierSemanticValue.identifierName;

    fprintf(g_codeGenOutputFp, ".text\n");
    if (strcmp(functionIdNode->semantic_value.identifierSemanticValue.identifierName, "main") != 0) {
        fprintf(g_codeGenOutputFp, "_start_%s:\n", functionIdNode->semantic_value.identifierSemanticValue.identifierName);
    } else {
        fprintf(g_codeGenOutputFp, "%s:\n", functionIdNode->semantic_value.identifierSemanticValue.identifierName);
    }
    
    //prologue
    fprintf(g_codeGenOutputFp, "sw $ra, 0($sp)\n");
    fprintf(g_codeGenOutputFp, "sw $fp, -4($sp)\n");
    fprintf(g_codeGenOutputFp, "add $fp, $sp, -4\n");
    fprintf(g_codeGenOutputFp, "add $sp, $sp, -8\n");
    fprintf(g_codeGenOutputFp, "lw $2, _frameSize_%s\n", functionIdNode->semantic_value.identifierSemanticValue.identifierName);
    fprintf(g_codeGenOutputFp, "sub $sp, $sp, $2\n");
    printStoreRegister(g_codeGenOutputFp);

    resetRegisterTable(functionIdNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->offsetInAR);

    AST_NODE* blockNode = functionIdNode->rightSibling->rightSibling;
    AST_NODE *traverseListNode = blockNode->child;
    while(traverseListNode)
    {
        codeGenGeneralNode(traverseListNode);
        traverseListNode = traverseListNode->rightSibling;
    }

    //epilogue
    fprintf(g_codeGenOutputFp, "_end_%s:\n", g_currentFunctionName);
    printRestoreRegister(g_codeGenOutputFp);
    fprintf(g_codeGenOutputFp, "lw $ra, 4($fp)\n");
    fprintf(g_codeGenOutputFp, "add $sp, $fp, 4\n");
    fprintf(g_codeGenOutputFp, "lw $fp, 0($fp)\n");
    if (strcmp(functionIdNode->semantic_value.identifierSemanticValue.identifierName, "main") == 0)
    {
        fprintf(g_codeGenOutputFp, "li $v0, 10\n");
        fprintf(g_codeGenOutputFp, "syscall\n");
    }
    else
    {
        fprintf(g_codeGenOutputFp, "jr $ra\n");
    }
    fprintf(g_codeGenOutputFp, ".data\n");
    int frameSize = abs(functionIdNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->offsetInAR) + 
        (INT_REGISTER_COUNT + INT_WORK_REGISTER_COUNT + FLOAT_REGISTER_COUNT + FLOAT_WORK_REGISTER_COUNT) * 4 +
        g_pseudoRegisterTable.isAllocatedVector->size * 4;
    fprintf(g_codeGenOutputFp, "_frameSize_%s: .word %d\n", functionIdNode->semantic_value.identifierSemanticValue.identifierName, frameSize);
    return;
}


void codeGenBlockNode(AST_NODE* blockNode)
{
    AST_NODE *traverseListNode = blockNode->child;
    while(traverseListNode)
    {
        codeGenGeneralNode(traverseListNode);
        traverseListNode = traverseListNode->rightSibling;
    }
}


void codeGenExprNode(AST_NODE* exprNode)
{
    if(exprNode->semantic_value.exprSemanticValue.kind == BINARY_OPERATION &&
       exprNode->semantic_value.exprSemanticValue.op.binaryOp != BINARY_OP_AND &&
       exprNode->semantic_value.exprSemanticValue.op.binaryOp != BINARY_OP_OR)
    {
        AST_NODE* leftOp = exprNode->child;
        AST_NODE* rightOp = leftOp->rightSibling;
        codeGenExprRelatedNode(leftOp);
        codeGenExprRelatedNode(rightOp);
        if(leftOp->dataType == FLOAT_TYPE || rightOp->dataType == FLOAT_TYPE)
        {
            if(leftOp->dataType == INT_TYPE)
            {
                leftOp->registerIndex = codeGenConvertFromIntToFloat(leftOp->registerIndex);
            }
            //else if
            if(rightOp->dataType == INT_TYPE)
            {
                rightOp->registerIndex = codeGenConvertFromIntToFloat(rightOp->registerIndex);
            }
            
            switch(exprNode->semantic_value.exprSemanticValue.op.binaryOp)
            {
            case BINARY_OP_ADD:
                exprNode->registerIndex = leftOp->registerIndex;
                codeGen3RegInstruction(FLOAT_REG, "add.s", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_SUB:
                exprNode->registerIndex = leftOp->registerIndex;
                codeGen3RegInstruction(FLOAT_REG, "sub.s", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_MUL:
                exprNode->registerIndex = leftOp->registerIndex;
                codeGen3RegInstruction(FLOAT_REG, "mul.s", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_DIV:
                exprNode->registerIndex = leftOp->registerIndex;
                codeGen3RegInstruction(FLOAT_REG, "div.s", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_EQ:
                exprNode->registerIndex = getRegister(INT_REG);
                codeGenFloatCompInstruction("c.eq.s", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                freeRegister(FLOAT_REG, leftOp->registerIndex);
                break;
            case BINARY_OP_GE:
                exprNode->registerIndex = getRegister(INT_REG);
                codeGenFloatCompInstruction("c.ge.s", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                freeRegister(FLOAT_REG, leftOp->registerIndex);
                break;
            case BINARY_OP_LE:
                exprNode->registerIndex = getRegister(INT_REG);
                codeGenFloatCompInstruction("c.le.s", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                freeRegister(FLOAT_REG, leftOp->registerIndex);
                break;
            case BINARY_OP_NE:
                exprNode->registerIndex = getRegister(INT_REG);
                codeGenFloatCompInstruction("c.ne.s", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                freeRegister(FLOAT_REG, leftOp->registerIndex);
                break;
            case BINARY_OP_GT:
                exprNode->registerIndex = getRegister(INT_REG);
                codeGenFloatCompInstruction("c.gt.s", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                freeRegister(FLOAT_REG, leftOp->registerIndex);
                break;
            case BINARY_OP_LT:
                exprNode->registerIndex = getRegister(INT_REG);
                codeGenFloatCompInstruction("c.lt.s", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                freeRegister(FLOAT_REG, leftOp->registerIndex);
                break;
            default:
                printf("Unhandled case in void evaluateExprValue(AST_NODE* exprNode)\n");
                break;
            }

            freeRegister(FLOAT_REG, rightOp->registerIndex);
        }//endif at least one float operand
        else if(exprNode->dataType == INT_TYPE)
        {
            exprNode->registerIndex = leftOp->registerIndex;
            switch(exprNode->semantic_value.exprSemanticValue.op.binaryOp)
            {
            case BINARY_OP_ADD:
                codeGen3RegInstruction(INT_REG, "add", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_SUB:
                codeGen3RegInstruction(INT_REG, "sub", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_MUL:
                codeGen3RegInstruction(INT_REG, "mul", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_DIV:
                codeGen3RegInstruction(INT_REG, "div", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_EQ:
                codeGen3RegInstruction(INT_REG, "seq", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_GE:
                codeGen3RegInstruction(INT_REG, "sge", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_LE:
                codeGen3RegInstruction(INT_REG, "sle", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_NE:
                codeGen3RegInstruction(INT_REG, "sne", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_GT:
                codeGen3RegInstruction(INT_REG, "sgt", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            case BINARY_OP_LT:
                codeGen3RegInstruction(INT_REG, "slt", exprNode->registerIndex, leftOp->registerIndex, rightOp->registerIndex);
                break;
            default:
                printf("Unhandled case in void evaluateExprValue(AST_NODE* exprNode)\n");
                break;
            }

            freeRegister(INT_REG, rightOp->registerIndex);
        }//endif 2 int operands
    }//endif BINARY_OPERATION
    else if(exprNode->semantic_value.exprSemanticValue.kind == BINARY_OPERATION) {
        AST_NODE* leftOp = exprNode->child;
        AST_NODE* rightOp = leftOp->rightSibling;
        int labelNumber = getLabelNumber();
        char *leftOpRegName, *rightOpRegName;

        if(leftOp->dataType == FLOAT_TYPE || rightOp->dataType == FLOAT_TYPE)
        {
            float zero = 0.0f;
            int constantZeroLabelNumber = codeGenConstantLabel(FLOATC, &zero);
            int zeroIndex = getRegister(FLOAT_REG);
            char *zeroRegName;
            codeGenPrepareRegister(FLOAT_REG, zeroIndex, 0, 0, &zeroRegName);
            fprintf(g_codeGenOutputFp, "l.s %s, _CONSTANT_%d\n", zeroRegName, constantZeroLabelNumber);

            char *exprRegName;
            exprNode->registerIndex = getRegister(INT_REG);
            codeGenPrepareRegister(INT_REG, exprNode->registerIndex, 0, 0, &exprRegName);
            
            switch(exprNode->semantic_value.exprSemanticValue.op.binaryOp)
            {
            case BINARY_OP_AND:
                codeGenExprRelatedNode(leftOp);
                if(leftOp->dataType == INT_TYPE)
                    leftOp->registerIndex = codeGenConvertFromIntToFloat(leftOp->registerIndex);
                codeGenPrepareRegister(FLOAT_REG, leftOp->registerIndex, 1, 1, &leftOpRegName);
                fprintf(g_codeGenOutputFp, "c.eq.s %s, %s\n", leftOpRegName, zeroRegName);
                fprintf(g_codeGenOutputFp, "bc1t _booleanFalse%d\n", labelNumber);
                codeGenExprRelatedNode(rightOp);
                if(rightOp->dataType == INT_TYPE)
                    rightOp->registerIndex = codeGenConvertFromIntToFloat(rightOp->registerIndex);
                codeGenPrepareRegister(FLOAT_REG, rightOp->registerIndex, 1, 1, &rightOpRegName);
                fprintf(g_codeGenOutputFp, "c.eq.s %s, %s\n", rightOpRegName, zeroRegName);
                fprintf(g_codeGenOutputFp, "bc1t _booleanFalse%d\n", labelNumber);
                fprintf(g_codeGenOutputFp, "_booleanTrue%d:\n", labelNumber);
                fprintf(g_codeGenOutputFp, "li %s, %d\n", exprRegName, 1);
                fprintf(g_codeGenOutputFp, "j _booleanExit%d\n", labelNumber);
                fprintf(g_codeGenOutputFp, "_booleanFalse%d:\n", labelNumber);
                fprintf(g_codeGenOutputFp, "li %s, %d\n", exprRegName, 0);
                fprintf(g_codeGenOutputFp, "_booleanExit%d:\n", labelNumber);
                break;
            case BINARY_OP_OR:
                codeGenExprRelatedNode(leftOp);
                if(leftOp->dataType == INT_TYPE)
                    leftOp->registerIndex = codeGenConvertFromIntToFloat(leftOp->registerIndex);
                codeGenPrepareRegister(FLOAT_REG, leftOp->registerIndex, 1, 1, &leftOpRegName);
                fprintf(g_codeGenOutputFp, "c.eq.s %s, %s\n", leftOpRegName, zeroRegName);
                fprintf(g_codeGenOutputFp, "bc1f _booleanTrue%d\n", labelNumber);
                codeGenExprRelatedNode(rightOp);
                if(rightOp->dataType == INT_TYPE)
                    rightOp->registerIndex = codeGenConvertFromIntToFloat(rightOp->registerIndex);
                codeGenPrepareRegister(FLOAT_REG, rightOp->registerIndex, 1, 1, &rightOpRegName);
                fprintf(g_codeGenOutputFp, "c.eq.s %s, %s\n", rightOpRegName, zeroRegName);
                fprintf(g_codeGenOutputFp, "bc1f _booleanTrue%d\n", labelNumber);
                fprintf(g_codeGenOutputFp, "_booleanFalse%d:\n", labelNumber);
                fprintf(g_codeGenOutputFp, "li %s, %d\n", exprRegName, 0);
                fprintf(g_codeGenOutputFp, "j _booleanExit%d\n", labelNumber);
                fprintf(g_codeGenOutputFp, "_booleanTrue%d:\n", labelNumber);
                fprintf(g_codeGenOutputFp, "li %s, %d\n", exprRegName, 1);
                fprintf(g_codeGenOutputFp, "_booleanExit%d:\n", labelNumber);
                break;
            default:
                printf("Unhandled case in void evaluateExprValue(AST_NODE* exprNode)\n");
                break;
            }

            freeRegister(FLOAT_REG, zeroIndex);
            freeRegister(FLOAT_REG, leftOp->registerIndex);
            freeRegister(FLOAT_REG, rightOp->registerIndex);
        }//endif at least one float operand
        else if(exprNode->dataType == INT_TYPE)
        {
            switch(exprNode->semantic_value.exprSemanticValue.op.binaryOp)
            {
            case BINARY_OP_AND:
                codeGenExprRelatedNode(leftOp);
                codeGenPrepareRegister(INT_REG, leftOp->registerIndex, 1, 0, &leftOpRegName);
                fprintf(g_codeGenOutputFp, "beqz %s, _booleanFalse%d\n", leftOpRegName, labelNumber);
                codeGenExprRelatedNode(rightOp);
                codeGenPrepareRegister(INT_REG, rightOp->registerIndex, 1, 0, &rightOpRegName);
                fprintf(g_codeGenOutputFp, "beqz %s, _booleanFalse%d\n", rightOpRegName, labelNumber);
                fprintf(g_codeGenOutputFp, "_booleanTrue%d:\n", labelNumber);
                fprintf(g_codeGenOutputFp, "li %s, %d\n", leftOpRegName, 1);
                fprintf(g_codeGenOutputFp, "j _booleanExit%d\n", labelNumber);
                fprintf(g_codeGenOutputFp, "_booleanFalse%d:\n", labelNumber);
                fprintf(g_codeGenOutputFp, "li %s, %d\n", leftOpRegName, 0);
                fprintf(g_codeGenOutputFp, "_booleanExit%d:\n", labelNumber);
                break;
            case BINARY_OP_OR:
                codeGenExprRelatedNode(leftOp);
                codeGenPrepareRegister(INT_REG, leftOp->registerIndex, 1, 0, &leftOpRegName);
                fprintf(g_codeGenOutputFp, "bnez %s, _booleanTrue%d\n", leftOpRegName, labelNumber);
                codeGenExprRelatedNode(rightOp);
                codeGenPrepareRegister(INT_REG, rightOp->registerIndex, 1, 0, &rightOpRegName);
                fprintf(g_codeGenOutputFp, "bnez %s, _booleanTrue%d\n", rightOpRegName, labelNumber);
                fprintf(g_codeGenOutputFp, "_booleanFalse%d:\n", labelNumber);
                fprintf(g_codeGenOutputFp, "li %s, %d\n", leftOpRegName, 0);
                fprintf(g_codeGenOutputFp, "j _booleanExit%d\n", labelNumber);
                fprintf(g_codeGenOutputFp, "_booleanTrue%d:\n", labelNumber);
                fprintf(g_codeGenOutputFp, "li %s, %d\n", leftOpRegName, 1);
                fprintf(g_codeGenOutputFp, "_booleanExit%d:\n", labelNumber);
                break;
            default:
                printf("Unhandled case in void evaluateExprValue(AST_NODE* exprNode)\n");
                break;
            }
            exprNode->registerIndex = leftOp->registerIndex;

            freeRegister(INT_REG, rightOp->registerIndex);
        }//endif 2 int operands
    }
    else if(exprNode->semantic_value.exprSemanticValue.kind == UNARY_OPERATION)
    {
        int tmpZero = 0;
        AST_NODE* operand = exprNode->child;
        codeGenExprRelatedNode(operand);
        if(operand->dataType == FLOAT_TYPE)
        {
            switch(exprNode->semantic_value.exprSemanticValue.op.unaryOp)
            {
            case UNARY_OP_POSITIVE:
                exprNode->registerIndex = operand->registerIndex;
                break;
            case UNARY_OP_NEGATIVE:
                exprNode->registerIndex = operand->registerIndex;
                codeGen2RegInstruction(FLOAT_REG, "neg.s", exprNode->registerIndex, exprNode->registerIndex);
                break;
            case UNARY_OP_LOGICAL_NEGATION:
                exprNode->registerIndex = getRegister(INT_REG);
                codeGenGetBoolOfFloat(exprNode->registerIndex, operand->registerIndex);
                codeGen2Reg1ImmInstruction(INT_REG, "seq", exprNode->registerIndex, exprNode->registerIndex, &tmpZero);
                freeRegister(FLOAT_REG, operand->registerIndex);
                break;
            default:
                printf("Unhandled case in void evaluateExprValue(AST_NODE* exprNode)\n");
                break;
            }
        }
        else if(operand->dataType == INT_TYPE)
        {
            switch(exprNode->semantic_value.exprSemanticValue.op.unaryOp)
            {
            case UNARY_OP_POSITIVE:
                exprNode->registerIndex = operand->registerIndex;
                break;
            case UNARY_OP_NEGATIVE:
                exprNode->registerIndex = operand->registerIndex;
                codeGen2RegInstruction(INT_REG, "neg", exprNode->registerIndex, exprNode->registerIndex);
                break;
            case UNARY_OP_LOGICAL_NEGATION:
                exprNode->registerIndex = operand->registerIndex;
                codeGen2Reg1ImmInstruction(INT_REG, "seq", exprNode->registerIndex, exprNode->registerIndex, &tmpZero);
                break;
            default:
                printf("Unhandled case in void evaluateExprValue(AST_NODE* exprNode)\n");
                break;
            }
        }
    }
}


int codeGenCalcArrayPointerAddress(AST_NODE *idNode) {
    AST_NODE* traverseDim = idNode->child;
    int* sizeInEachDimension = idNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.typeDescriptor->properties.arrayProperties.sizeInEachDimension;
    int dimension = idNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.typeDescriptor->properties.arrayProperties.dimension;
            
    codeGenExprRelatedNode(traverseDim);
    int linearIdxRegisterIndex = traverseDim->registerIndex;
    traverseDim = traverseDim->rightSibling;

    int dimIndex = 1;
    int dimRegIndex = getRegister(INT_REG);
    char *totalRegName, *oneDimRegName, *dimConstRegName;
    /*TODO multiple dimensions*/
    while(traverseDim)
    {
        codeGenPrepareRegister(INT_REG, linearIdxRegisterIndex, 1, 0, &totalRegName);
        codeGenPrepareRegister(INT_REG, dimRegIndex, 0, 1, &dimConstRegName);
        fprintf(g_codeGenOutputFp, "li %s, %d\n", dimConstRegName, sizeInEachDimension[dimIndex]);
        fprintf(g_codeGenOutputFp, "mul %s, %s, %s\n", totalRegName, totalRegName, dimConstRegName);
        codeGenExprRelatedNode(traverseDim);
        codeGenPrepareRegister(INT_REG, traverseDim->registerIndex, 1, 1, &oneDimRegName);
        fprintf(g_codeGenOutputFp, "add %s, %s, %s\n", totalRegName, totalRegName, oneDimRegName);

        freeRegister(INT_REG, traverseDim->registerIndex);
        dimIndex++;
        traverseDim = traverseDim->rightSibling;
    }
    while(dimIndex < dimension) {
        codeGenPrepareRegister(INT_REG, linearIdxRegisterIndex, 1, 0, &totalRegName);
        codeGenPrepareRegister(INT_REG, dimRegIndex, 0, 1, &dimConstRegName);
        fprintf(g_codeGenOutputFp, "li %s, %d\n", dimConstRegName, sizeInEachDimension[dimIndex]);
        fprintf(g_codeGenOutputFp, "mul %s, %s, %s\n", totalRegName, totalRegName, dimConstRegName);

        dimIndex++;
    }
    freeRegister(INT_REG, dimRegIndex);
    
    int shiftLeftTwoBits = 2;
    codeGen2Reg1ImmInstruction(INT_REG, "sll", linearIdxRegisterIndex, linearIdxRegisterIndex, &shiftLeftTwoBits);
    
    char* linearOffsetRegName = NULL;
    if(!isGlobalVariable(idNode->semantic_value.identifierSemanticValue.symbolTableEntry))
    {
        int baseOffset = idNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->offsetInAR;
        codeGen2Reg1ImmInstruction(INT_REG, "add", linearIdxRegisterIndex, linearIdxRegisterIndex, &baseOffset);
        codeGenPrepareRegister(INT_REG, linearIdxRegisterIndex, 1, 0, &linearOffsetRegName);
        fprintf(g_codeGenOutputFp, "add %s, %s, $fp\n", linearOffsetRegName, linearOffsetRegName);
    }
    else
    {
        fprintf(g_codeGenOutputFp, "la %s, _g_%s\n", intWorkRegisterName[0], idNode->semantic_value.identifierSemanticValue.identifierName);
        codeGenPrepareRegister(INT_REG, linearIdxRegisterIndex, 1, 1, &linearOffsetRegName);
        fprintf(g_codeGenOutputFp, "add %s, %s, %s\n", linearOffsetRegName, linearOffsetRegName, intWorkRegisterName[0]);
    }

    codeGenSaveToMemoryIfPsuedoRegister(INT_REG, linearIdxRegisterIndex, linearOffsetRegName);

    return linearIdxRegisterIndex;
}


void codeGenStoreArgument(AST_NODE *traverseParameter, Parameter* formalParameter) {
    if(traverseParameter->rightSibling)
        codeGenStoreArgument(traverseParameter->rightSibling, formalParameter->next);

    char* parameterRegName = NULL;
    if(traverseParameter->dataType == INT_TYPE) {
        codeGenExprRelatedNode(traverseParameter);

        if(formalParameter->type->properties.dataType == FLOAT_TYPE) {
            int floatIndex = codeGenConvertFromIntToFloat(traverseParameter->registerIndex);
            codeGenPrepareRegister(FLOAT_REG, floatIndex, 1, 0, &parameterRegName);
            fprintf(g_codeGenOutputFp, "s.s %s, 0($sp)\n", parameterRegName);
            freeRegister(FLOAT_REG, floatIndex);
        }
        else {
            codeGenPrepareRegister(INT_REG, traverseParameter->registerIndex, 1, 0, &parameterRegName);
            fprintf(g_codeGenOutputFp, "sw %s, 0($sp)\n", parameterRegName);
            freeRegister(INT_REG, traverseParameter->registerIndex);
        }
    }
    else if(traverseParameter->dataType == FLOAT_TYPE) {
        codeGenExprRelatedNode(traverseParameter);

        if(formalParameter->type->properties.dataType == INT_TYPE) {
            int intIndex = codeGenConvertFromFloatToInt(traverseParameter->registerIndex);
            codeGenPrepareRegister(INT_REG, intIndex, 1, 0, &parameterRegName);
            fprintf(g_codeGenOutputFp, "sw %s, 0($sp)\n", parameterRegName);
            freeRegister(INT_REG, intIndex);
        }
        else {
            codeGenPrepareRegister(FLOAT_REG, traverseParameter->registerIndex, 1, 0, &parameterRegName);
            fprintf(g_codeGenOutputFp, "s.s %s, 0($sp)\n", parameterRegName);
            freeRegister(FLOAT_REG, traverseParameter->registerIndex);
        }
    }
    else if(traverseParameter->dataType == INT_PTR_TYPE || traverseParameter->dataType == FLOAT_PTR_TYPE) {
        int offsetIndex;
        char *offsetRegName;
        if(traverseParameter->semantic_value.identifierSemanticValue.kind == ARRAY_ID) {
            offsetIndex = codeGenCalcArrayPointerAddress(traverseParameter);
            codeGenPrepareRegister(INT_REG, offsetIndex, 0, 0, &offsetRegName);
        }
        else {
            offsetIndex = getRegister(INT_REG);
            codeGenPrepareRegister(INT_REG, offsetIndex, 0, 0, &offsetRegName);

            if(!isGlobalVariable(traverseParameter->semantic_value.identifierSemanticValue.symbolTableEntry)) {
                fprintf(g_codeGenOutputFp, "li %s, %d\n", offsetRegName, traverseParameter->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->offsetInAR);
                fprintf(g_codeGenOutputFp, "add %s, %s, $fp\n", offsetRegName, offsetRegName);
            }
            else {
                fprintf(g_codeGenOutputFp, "la %s, _g_%s\n", offsetRegName, traverseParameter->semantic_value.identifierSemanticValue.identifierName);
            }
        }
        fprintf(g_codeGenOutputFp, "sw %s, 0($sp)\n", offsetRegName);

        freeRegister(INT_REG, offsetIndex);
    }
    fprintf(g_codeGenOutputFp, "addi $sp, $sp, -4\n");
}


void codeGenFunctionCall(AST_NODE* functionCallNode)
{
    AST_NODE* functionIdNode = functionCallNode->child;
    AST_NODE* parameterList = functionIdNode->rightSibling;
    if(strcmp(functionIdNode->semantic_value.identifierSemanticValue.identifierName, "write") == 0)
    {
        AST_NODE* firstParameter = parameterList->child;
        codeGenExprRelatedNode(firstParameter);
        char* parameterRegName = NULL;
        switch(firstParameter->dataType)
        {
        case INT_TYPE:
            fprintf(g_codeGenOutputFp, "li $v0, 1\n");
            codeGenPrepareRegister(INT_REG, firstParameter->registerIndex, 1, 0, &parameterRegName);
            fprintf(g_codeGenOutputFp, "move $a0, %s\n", parameterRegName);
            freeRegister(INT_REG, firstParameter->registerIndex);
            break;
        case FLOAT_TYPE:
            fprintf(g_codeGenOutputFp, "li $v0, 2\n");
            codeGenPrepareRegister(FLOAT_REG, firstParameter->registerIndex, 1, 0, &parameterRegName);
            fprintf(g_codeGenOutputFp, "mov.s $f12, %s\n", parameterRegName);
            freeRegister(FLOAT_REG, firstParameter->registerIndex);
            break;
        case CONST_STRING_TYPE:
            fprintf(g_codeGenOutputFp, "li $v0, 4\n");
            codeGenPrepareRegister(INT_REG, firstParameter->registerIndex, 1, 0, &parameterRegName);
            fprintf(g_codeGenOutputFp, "move $a0, %s\n", parameterRegName);
            freeRegister(INT_REG, firstParameter->registerIndex);
            break;
        default:
            printf("Unhandled case in void codeGenFunctionCall(AST_NODE* functionCallNode)\n");
            printf("firstParameter->registerIndex was not free\n");
            break;
        }
        fprintf(g_codeGenOutputFp, "syscall\n");
        return;
    }


    if(strcmp(functionIdNode->semantic_value.identifierSemanticValue.identifierName, "read") == 0)
    {
        fprintf(g_codeGenOutputFp, "li $v0, 5\n");
        fprintf(g_codeGenOutputFp, "syscall\n");
    }
    else if(strcmp(functionIdNode->semantic_value.identifierSemanticValue.identifierName, "fread") == 0)
    {
        fprintf(g_codeGenOutputFp, "li $v0, 6\n");
        fprintf(g_codeGenOutputFp, "syscall\n");
    }
    else
    {
        if (strcmp(functionIdNode->semantic_value.identifierSemanticValue.identifierName, "main") != 0) {
            AST_NODE* traverseParameter = parameterList->child;
            if(traverseParameter) {
                codeGenStoreArgument(traverseParameter, functionIdNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.functionSignature->parameterList);
            }
            fprintf(g_codeGenOutputFp, "jal _start_%s\n", functionIdNode->semantic_value.identifierSemanticValue.identifierName);
            while(traverseParameter) {
                fprintf(g_codeGenOutputFp, "addi $sp, $sp, 4\n");
                traverseParameter = traverseParameter->rightSibling;
            }
        } else {
            fprintf(g_codeGenOutputFp, "jal %s\n", functionIdNode->semantic_value.identifierSemanticValue.identifierName);
        }
    }




    if (functionIdNode->semantic_value.identifierSemanticValue.symbolTableEntry) {
        if(functionIdNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.functionSignature->returnType == INT_TYPE)
        {
            functionCallNode->registerIndex = getRegister(INT_REG);
            char* returnIntRegName = NULL;
            codeGenPrepareRegister(INT_REG, functionCallNode->registerIndex, 0, 0, &returnIntRegName);

            fprintf(g_codeGenOutputFp, "move %s, $v0\n", returnIntRegName);

            codeGenSaveToMemoryIfPsuedoRegister(INT_REG, functionCallNode->registerIndex, returnIntRegName);
        }
        else if(functionIdNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.functionSignature->returnType == FLOAT_TYPE)
        {
            functionCallNode->registerIndex = getRegister(FLOAT_REG);
            char* returnfloatRegName = NULL;
            codeGenPrepareRegister(FLOAT_REG, functionCallNode->registerIndex, 0, 0, &returnfloatRegName);

            fprintf(g_codeGenOutputFp, "mov.s %s, $f0\n", returnfloatRegName);

            codeGenSaveToMemoryIfPsuedoRegister(INT_REG, functionCallNode->registerIndex, returnfloatRegName);
        }
    }
}


int codeGenCalcArrayElemenetAddress(AST_NODE* idNode)
{
    AST_NODE* traverseDim = idNode->child;
    int* sizeInEachDimension = idNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.typeDescriptor->properties.arrayProperties.sizeInEachDimension;
            
    codeGenExprRelatedNode(traverseDim);
    int linearIdxRegisterIndex = traverseDim->registerIndex;
    traverseDim = traverseDim->rightSibling;

    int dimIndex = 1;
    int dimRegIndex = getRegister(INT_REG);
    char *totalRegName, *oneDimRegName, *dimConstRegName;
    /*TODO multiple dimensions*/
    while(traverseDim)
    {
        codeGenPrepareRegister(INT_REG, linearIdxRegisterIndex, 1, 0, &totalRegName);
        codeGenPrepareRegister(INT_REG, dimRegIndex, 0, 1, &dimConstRegName);
        fprintf(g_codeGenOutputFp, "li %s, %d\n", dimConstRegName, sizeInEachDimension[dimIndex]);
        fprintf(g_codeGenOutputFp, "mul %s, %s, %s\n", totalRegName, totalRegName, dimConstRegName);
        codeGenExprRelatedNode(traverseDim);
        codeGenPrepareRegister(INT_REG, traverseDim->registerIndex, 1, 1, &oneDimRegName);
        fprintf(g_codeGenOutputFp, "add %s, %s, %s\n", totalRegName, totalRegName, oneDimRegName);

        freeRegister(INT_REG, traverseDim->registerIndex);
        dimIndex++;
        traverseDim = traverseDim->rightSibling;
    }
    freeRegister(INT_REG, dimRegIndex);
    
    int shiftLeftTwoBits = 2;
    codeGen2Reg1ImmInstruction(INT_REG, "sll", linearIdxRegisterIndex, linearIdxRegisterIndex, &shiftLeftTwoBits);
    
    char* linearOffsetRegName = NULL;
    if(!isGlobalVariable(idNode->semantic_value.identifierSemanticValue.symbolTableEntry))
    {
        int baseOffset = idNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->offsetInAR;
        if(baseOffset > 0) {
            int realBaseOffsetIndex = getRegister(INT_REG);
            char *realBaseOffsetRegName;
            codeGenPrepareRegister(INT_REG, realBaseOffsetIndex, 1, 0, &realBaseOffsetRegName);
            codeGenPrepareRegister(INT_REG, linearIdxRegisterIndex, 1, 1, &linearOffsetRegName);
            fprintf(g_codeGenOutputFp, "lw %s, %d($fp)\n", realBaseOffsetRegName, baseOffset);
            fprintf(g_codeGenOutputFp, "add %s, %s, %s\n", linearOffsetRegName, linearOffsetRegName, realBaseOffsetRegName);
        }
        else {
            codeGen2Reg1ImmInstruction(INT_REG, "add", linearIdxRegisterIndex, linearIdxRegisterIndex, &baseOffset);
            codeGenPrepareRegister(INT_REG, linearIdxRegisterIndex, 1, 0, &linearOffsetRegName);
            fprintf(g_codeGenOutputFp, "add %s, %s, $fp\n", linearOffsetRegName, linearOffsetRegName);
        }
    }
    else
    {
        fprintf(g_codeGenOutputFp, "la %s, _g_%s\n", intWorkRegisterName[0], idNode->semantic_value.identifierSemanticValue.identifierName);
        codeGenPrepareRegister(INT_REG, linearIdxRegisterIndex, 1, 1, &linearOffsetRegName);
        fprintf(g_codeGenOutputFp, "add %s, %s, %s\n", linearOffsetRegName, linearOffsetRegName, intWorkRegisterName[0]);
    }

    codeGenSaveToMemoryIfPsuedoRegister(INT_REG, linearIdxRegisterIndex, linearOffsetRegName);

    return linearIdxRegisterIndex;
}


void codeGenVariableReference(AST_NODE* idNode)
{
    SymbolAttribute *idAttribute = idNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute;
    if(idNode->semantic_value.identifierSemanticValue.kind == NORMAL_ID)
    {
        if(idNode->dataType == INT_TYPE)
        {
            idNode->registerIndex = getRegister(INT_REG);
            char* loadRegName = NULL;
            if(!isGlobalVariable(idNode->semantic_value.identifierSemanticValue.symbolTableEntry))
            {
                codeGenPrepareRegister(INT_REG, idNode->registerIndex, 0, 0, &loadRegName);
                fprintf(g_codeGenOutputFp, "lw %s, %d($fp)\n", loadRegName, idAttribute->offsetInAR);
            }
            else
            {
                fprintf(g_codeGenOutputFp, "la %s, _g_%s\n", intWorkRegisterName[0], idNode->semantic_value.identifierSemanticValue.identifierName);
                codeGenPrepareRegister(INT_REG, idNode->registerIndex, 0, 1, &loadRegName);
                fprintf(g_codeGenOutputFp, "lw %s, 0(%s)\n", loadRegName, intWorkRegisterName[0]);
            }
            codeGenSaveToMemoryIfPsuedoRegister(INT_REG, idNode->registerIndex, loadRegName);
        }
        else if(idNode->dataType == FLOAT_TYPE)
        {
            idNode->registerIndex = getRegister(FLOAT_REG);
            char* loadRegName = NULL;
            if(!isGlobalVariable(idNode->semantic_value.identifierSemanticValue.symbolTableEntry))
            {
                codeGenPrepareRegister(FLOAT_REG, idNode->registerIndex, 0, 0, &loadRegName);
                fprintf(g_codeGenOutputFp, "l.s %s, %d($fp)\n", loadRegName, idAttribute->offsetInAR);
            }
            else
            {
                fprintf(g_codeGenOutputFp, "la %s, _g_%s\n", intWorkRegisterName[0], idNode->semantic_value.identifierSemanticValue.identifierName);
                codeGenPrepareRegister(FLOAT_REG, idNode->registerIndex, 0, 0, &loadRegName);
                fprintf(g_codeGenOutputFp, "l.s %s, 0(%s)\n", loadRegName, intWorkRegisterName[0]);
            }
            codeGenSaveToMemoryIfPsuedoRegister(FLOAT_REG, idNode->registerIndex, loadRegName);
        }
    }
    else if(idNode->semantic_value.identifierSemanticValue.kind == ARRAY_ID)
    {
        if(idNode->dataType == INT_TYPE || idNode->dataType == FLOAT_TYPE)
        {
            int elementAddressRegIndex = codeGenCalcArrayElemenetAddress(idNode);
            char* elementAddressRegName = NULL;
            codeGenPrepareRegister(INT_REG, elementAddressRegIndex, 1, 0, &elementAddressRegName);
            
            if(idNode->dataType == INT_TYPE)
            {
                idNode->registerIndex = elementAddressRegIndex;
                fprintf(g_codeGenOutputFp, "lw %s, 0(%s)\n", elementAddressRegName, elementAddressRegName);
                codeGenSaveToMemoryIfPsuedoRegister(INT_REG, idNode->registerIndex, elementAddressRegName);
            }
            else if(idNode->dataType == FLOAT_TYPE)
            {
                idNode->registerIndex = getRegister(FLOAT_REG);
                char* dstRegName = NULL;
                codeGenPrepareRegister(FLOAT_REG, idNode->registerIndex, 0, 0, &dstRegName);
                
                char* elementAddressRegName = NULL;
                codeGenPrepareRegister(INT_REG, elementAddressRegIndex, 1, 0, &elementAddressRegName);
            
                fprintf(g_codeGenOutputFp, "l.s %s, 0(%s)\n", dstRegName, elementAddressRegName);
                codeGenSaveToMemoryIfPsuedoRegister(FLOAT_REG, idNode->registerIndex, dstRegName);
            
                freeRegister(INT_REG, elementAddressRegIndex);
            }
        }
    }
}


void codeGenConstantReference(AST_NODE* constantNode)
{
    C_type cType = constantNode->semantic_value.const1->const_type;
    if(cType == INTEGERC)
    {
        int tmpInt = constantNode->semantic_value.const1->const_u.intval;
        int constantLabelNumber = codeGenConstantLabel(INTEGERC, &tmpInt);
        constantNode->registerIndex = getRegister(INT_REG);
        char* regName = NULL;
        codeGenPrepareRegister(INT_REG, constantNode->registerIndex, 0, 0, &regName);
        fprintf(g_codeGenOutputFp, "lw %s, _CONSTANT_%d\n", regName, constantLabelNumber);
        codeGenSaveToMemoryIfPsuedoRegister(INT_REG, constantNode->registerIndex, regName);
    }
    else if(cType == FLOATC)
    {
        float tmpFloat = constantNode->semantic_value.const1->const_u.fval;
        int constantLabelNumber = codeGenConstantLabel(FLOATC, &tmpFloat);
        constantNode->registerIndex = getRegister(FLOAT_REG);
        char* regName = NULL;
        codeGenPrepareRegister(FLOAT_REG, constantNode->registerIndex, 0, 0, &regName);
        fprintf(g_codeGenOutputFp, "l.s %s, _CONSTANT_%d\n", regName, constantLabelNumber);
        codeGenSaveToMemoryIfPsuedoRegister(FLOAT_REG, constantNode->registerIndex, regName);
    }
    else if(cType == STRINGC)
    {
        char* tmpCharPtr = constantNode->semantic_value.const1->const_u.sc;
        int constantLabelNumber = codeGenConstantLabel(STRINGC, tmpCharPtr);
        constantNode->registerIndex = getRegister(INT_REG);
        char* regName = NULL;
        codeGenPrepareRegister(INT_REG, constantNode->registerIndex, 0, 0, &regName);
        fprintf(g_codeGenOutputFp, "la %s, _CONSTANT_%d\n", regName, constantLabelNumber);
        codeGenSaveToMemoryIfPsuedoRegister(INT_REG, constantNode->registerIndex, regName);
    }
}


void codeGenExprRelatedNode(AST_NODE* exprRelatedNode)
{
    switch(exprRelatedNode->nodeType)
    {
    case EXPR_NODE:
        codeGenExprNode(exprRelatedNode);
        break;
    case STMT_NODE:
        codeGenFunctionCall(exprRelatedNode);
        break;
    case IDENTIFIER_NODE:
        codeGenVariableReference(exprRelatedNode);
        break;
    case CONST_VALUE_NODE:
        codeGenConstantReference(exprRelatedNode);
        break;
    default:
        printf("Unhandle case in void processExprRelatedNode(AST_NODE* exprRelatedNode)\n");
        exprRelatedNode->dataType = ERROR_TYPE;
        break;
    }
}


void codeGenAssignmentStmt(AST_NODE* assignmentStmtNode)
{
    AST_NODE* leftOp = assignmentStmtNode->child;
    AST_NODE* rightOp = leftOp->rightSibling;
    codeGenExprRelatedNode(rightOp);

    /* TODO type conversion */
    if(leftOp->dataType == FLOAT_TYPE && rightOp->dataType == INT_TYPE)
    {
        rightOp->registerIndex = codeGenConvertFromIntToFloat(rightOp->registerIndex);
    }
    else if(leftOp->dataType == INT_TYPE && rightOp->dataType == FLOAT_TYPE)
    {
        rightOp->registerIndex = codeGenConvertFromFloatToInt(rightOp->registerIndex);
    }

    if(leftOp->semantic_value.identifierSemanticValue.kind == NORMAL_ID)
    {
        if(leftOp->dataType == INT_TYPE)
        {
            char* rightOpRegName = NULL;
            codeGenPrepareRegister(INT_REG, rightOp->registerIndex, 1, 0, &rightOpRegName);
            if(!isGlobalVariable(leftOp->semantic_value.identifierSemanticValue.symbolTableEntry))
            {
                fprintf(g_codeGenOutputFp, "sw %s, %d($fp)\n", rightOpRegName, leftOp->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->offsetInAR);
            }
            else
            {
                fprintf(g_codeGenOutputFp, "sw %s, _g_%s\n", rightOpRegName, leftOp->semantic_value.identifierSemanticValue.identifierName);
            }
            leftOp->registerIndex = rightOp->registerIndex;
        }
        else if(leftOp->dataType == FLOAT_TYPE)
        {
            char* rightOpRegName = NULL;
            codeGenPrepareRegister(FLOAT_REG, rightOp->registerIndex, 1, 0, &rightOpRegName);
            if(!isGlobalVariable(leftOp->semantic_value.identifierSemanticValue.symbolTableEntry))
            {
                fprintf(g_codeGenOutputFp, "s.s %s, %d($fp)\n", rightOpRegName, leftOp->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->offsetInAR);
            }
            else
            {
                fprintf(g_codeGenOutputFp, "s.s %s, _g_%s\n", rightOpRegName, leftOp->semantic_value.identifierSemanticValue.identifierName);
            }
            leftOp->registerIndex = rightOp->registerIndex;
        }
    }
    else if(leftOp->semantic_value.identifierSemanticValue.kind == ARRAY_ID)
    {
        int elementAddressRegIndex = codeGenCalcArrayElemenetAddress(leftOp);

        char* elementAddressRegName = NULL;
        codeGenPrepareRegister(INT_REG, elementAddressRegIndex, 1, 0, &elementAddressRegName);
        if(leftOp->dataType == INT_TYPE)
        {
            char* rightOpRegName = NULL;
            codeGenPrepareRegister(INT_REG, rightOp->registerIndex, 1, 1, &rightOpRegName);
            fprintf(g_codeGenOutputFp, "sw %s, 0(%s)\n", rightOpRegName, elementAddressRegName);
            
            leftOp->registerIndex = rightOp->registerIndex;
        }
        else if(leftOp->dataType == FLOAT_TYPE)
        {
            char* rightOpRegName = NULL;
            codeGenPrepareRegister(FLOAT_REG, rightOp->registerIndex, 1, 0, &rightOpRegName);
            
            fprintf(g_codeGenOutputFp, "s.s %s, 0(%s)\n", rightOpRegName, elementAddressRegName);

            leftOp->registerIndex = rightOp->registerIndex;
        }

        freeRegister(INT_REG, elementAddressRegIndex);
    }
}


void codeGenAssignOrExpr(AST_NODE* testNode)
{
    if(testNode->nodeType == STMT_NODE)
    {
        if(testNode->semantic_value.stmtSemanticValue.kind == ASSIGN_STMT)
        {
            codeGenAssignmentStmt(testNode);
        }
        else if(testNode->semantic_value.stmtSemanticValue.kind == FUNCTION_CALL_STMT)
        {
            codeGenFunctionCall(testNode);
        }
    }
    else
    {
        codeGenExprRelatedNode(testNode);
    }
}


void codeGenWhileStmt(AST_NODE* whileStmtNode)
{
    AST_NODE* boolExpression = whileStmtNode->child;

    int constantZeroLabelNumber = -1;
    if(boolExpression->dataType == FLOAT_TYPE)
    {
        float zero = 0.0f;
        constantZeroLabelNumber = codeGenConstantLabel(FLOATC, &zero);
    }

    int labelNumber = getLabelNumber();
    fprintf(g_codeGenOutputFp, "_whileTestLabel_%d:\n", labelNumber);
    
    codeGenAssignOrExpr(boolExpression);

    if(boolExpression->dataType == INT_TYPE)
    {
        char* boolRegName = NULL;
        codeGenPrepareRegister(INT_REG, boolExpression->registerIndex, 1, 0, &boolRegName);
        fprintf(g_codeGenOutputFp, "beqz %s, _whileExitLabel_%d\n", boolRegName, labelNumber);
        freeRegister(INT_REG, boolExpression->registerIndex);
    }
    else if(boolExpression->dataType == FLOAT_TYPE)
    {
        fprintf(g_codeGenOutputFp, "l.s %s, _CONSTANT_%d\n", floatWorkRegisterName[0], constantZeroLabelNumber);
        char* boolRegName = NULL;
        codeGenPrepareRegister(FLOAT_REG, boolExpression->registerIndex, 1, 1, &boolRegName);
        fprintf(g_codeGenOutputFp, "c.eq.s %s, %s\n", boolRegName, floatWorkRegisterName[0]);
        fprintf(g_codeGenOutputFp, "bc1t _whileExitLabel_%d\n", labelNumber);
        freeRegister(FLOAT_REG, boolExpression->registerIndex);
    }
    
    AST_NODE* bodyNode = boolExpression->rightSibling;
    codeGenStmtNode(bodyNode);

    fprintf(g_codeGenOutputFp, "j _whileTestLabel_%d\n", labelNumber);
    fprintf(g_codeGenOutputFp, "_whileExitLabel_%d:\n", labelNumber);
}


void codeGenForStmt(AST_NODE* forStmtNode)
{
    /*TODO*/
    AST_NODE* initialNode = forStmtNode->child;
    AST_NODE* conditionNode = initialNode->rightSibling;
    AST_NODE* incrementNode = conditionNode->rightSibling;
    AST_NODE* bodyNode = incrementNode->rightSibling;

    int constantZeroLabelNumber = -1;
    if(conditionNode->dataType == FLOAT_TYPE)
    {
        float zero = 0.0f;
        constantZeroLabelNumber = codeGenConstantLabel(FLOATC, &zero);
    }

    codeGenGeneralNode(initialNode);
    if(initialNode->dataType == INT_TYPE)
        freeRegister(INT_REG, initialNode->registerIndex);
    else if(initialNode->dataType == FLOAT_TYPE)
        freeRegister(FLOAT_REG, initialNode->registerIndex);

    int labelNumber = getLabelNumber();
    fprintf(g_codeGenOutputFp, "_forTestLabel_%d:\n", labelNumber);

    codeGenGeneralNode(conditionNode);

    if(conditionNode->dataType == INT_TYPE)
    {
        char* boolRegName = NULL;
        codeGenPrepareRegister(INT_REG, conditionNode->registerIndex, 1, 0, &boolRegName);
        fprintf(g_codeGenOutputFp, "beqz %s, _forExitLabel_%d\n", boolRegName, labelNumber);
        freeRegister(INT_REG, conditionNode->registerIndex);
    }
    else if(conditionNode->dataType == FLOAT_TYPE)
    {
        fprintf(g_codeGenOutputFp, "l.s %s, _CONSTANT_%d\n", floatWorkRegisterName[0], constantZeroLabelNumber);
        char* boolRegName = NULL;
        codeGenPrepareRegister(FLOAT_REG, conditionNode->registerIndex, 1, 1, &boolRegName);
        fprintf(g_codeGenOutputFp, "c.eq.s %s, %s\n", boolRegName, floatWorkRegisterName[0]);
        fprintf(g_codeGenOutputFp, "bc1t _forExitLabel_%d\n", labelNumber);
        freeRegister(FLOAT_REG, conditionNode->registerIndex);
    }

    fprintf(g_codeGenOutputFp, "j _forBodyLabel_%d\n", labelNumber);
    fprintf(g_codeGenOutputFp, "_forIncLabel_%d:\n", labelNumber);

    codeGenGeneralNode(incrementNode);
    if(incrementNode->dataType == INT_TYPE)
        freeRegister(INT_REG, incrementNode->registerIndex);
    else if(incrementNode->dataType == FLOAT_TYPE)
        freeRegister(FLOAT_REG, incrementNode->registerIndex);

    fprintf(g_codeGenOutputFp, "j _forTestLabel_%d\n", labelNumber);
    fprintf(g_codeGenOutputFp, "_forBodyLabel_%d:\n", labelNumber);

    codeGenStmtNode(bodyNode);

    fprintf(g_codeGenOutputFp, "j _forIncLabel_%d\n", labelNumber);
    fprintf(g_codeGenOutputFp, "_forExitLabel_%d:\n", labelNumber);
}


void codeGenIfStmt(AST_NODE* ifStmtNode)
{
    AST_NODE* boolExpression = ifStmtNode->child;

    int constantZeroLabelNumber = -1;
    if(boolExpression->dataType == FLOAT_TYPE)
    {
        float zero = 0.0f;
        constantZeroLabelNumber = codeGenConstantLabel(FLOATC, &zero);
    }

    int labelNumber = getLabelNumber();

    codeGenAssignOrExpr(boolExpression);

    if(boolExpression->dataType == INT_TYPE)
    {
        char* boolRegName = NULL;
        codeGenPrepareRegister(INT_REG, boolExpression->registerIndex, 1, 0, &boolRegName);
        fprintf(g_codeGenOutputFp, "beqz %s, _elseLabel_%d\n", boolRegName, labelNumber);
        freeRegister(INT_REG, boolExpression->registerIndex);
    }
    else if(boolExpression->dataType == FLOAT_TYPE)
    {
        fprintf(g_codeGenOutputFp, "l.s %s, _CONSTANT_%d\n", floatWorkRegisterName[0], constantZeroLabelNumber);
        char* boolRegName = NULL;
        codeGenPrepareRegister(FLOAT_REG, boolExpression->registerIndex, 1, 1, &boolRegName);
        fprintf(g_codeGenOutputFp, "c.eq.s %s, %s\n", boolRegName, floatWorkRegisterName[0]);
        fprintf(g_codeGenOutputFp, "bc1t _elseLabel_%d\n", labelNumber);
        freeRegister(FLOAT_REG, boolExpression->registerIndex);
    }

    AST_NODE* ifBodyNode = boolExpression->rightSibling;
    codeGenStmtNode(ifBodyNode);
    
    fprintf(g_codeGenOutputFp, "j _ifExitLabel_%d\n", labelNumber);
    fprintf(g_codeGenOutputFp, "_elseLabel_%d:\n", labelNumber);
    AST_NODE* elsePartNode = ifBodyNode->rightSibling;
    codeGenStmtNode(elsePartNode);
    fprintf(g_codeGenOutputFp, "_ifExitLabel_%d:\n", labelNumber);
}


void codeGenReturnStmt(AST_NODE* returnStmtNode)
{
    AST_NODE* returnVal = returnStmtNode->child;
    if(returnVal->nodeType != NUL_NODE)
    {
        codeGenExprRelatedNode(returnVal);
        /* TODO type conversion */
        if(returnStmtNode->dataType == FLOAT_TYPE && returnVal->dataType == INT_TYPE)
        {
            returnVal->registerIndex = codeGenConvertFromIntToFloat(returnVal->registerIndex);
        }
        else if(returnStmtNode->dataType == INT_TYPE && returnVal->dataType == FLOAT_TYPE)
        {
            returnVal->registerIndex = codeGenConvertFromFloatToInt(returnVal->registerIndex);
        }

        char* returnValRegName = NULL;
        /*if (returnVal->dataType == INT_TYPE)*/
        if (returnStmtNode->dataType == INT_TYPE)
        {
            codeGenPrepareRegister(INT_REG, returnVal->registerIndex, 1, 0, &returnValRegName);
            fprintf(g_codeGenOutputFp, "move $v0, %s\n", returnValRegName);
            freeRegister(INT_REG, returnVal->registerIndex);
        }
        /*else if(returnVal->dataType == FLOAT_TYPE)*/
        else if(returnStmtNode->dataType == FLOAT_TYPE)
        {
            codeGenPrepareRegister(FLOAT_REG, returnVal->registerIndex, 1, 0, &returnValRegName);
            fprintf(g_codeGenOutputFp, "mov.s $f0, %s\n", returnValRegName);
            freeRegister(FLOAT_REG, returnVal->registerIndex);
        }
    }
    fprintf(g_codeGenOutputFp, "j _end_%s\n", g_currentFunctionName); 
}


void codeGenStmtNode(AST_NODE* stmtNode)
{
    printSourceFile(g_codeGenOutputFp, stmtNode->linenumber);

    if(stmtNode->nodeType == NUL_NODE)
    {
        return;
    }
    else if(stmtNode->nodeType == BLOCK_NODE)
    {
        codeGenBlockNode(stmtNode);
    }
    else
    {
        switch(stmtNode->semantic_value.stmtSemanticValue.kind)
        {
        case WHILE_STMT:
            codeGenWhileStmt(stmtNode);
            break;
        case FOR_STMT:
            codeGenForStmt(stmtNode);
            break;
        case ASSIGN_STMT:
            codeGenAssignmentStmt(stmtNode);
            if(stmtNode->child->dataType == INT_TYPE)
            {
                freeRegister(INT_REG, stmtNode->child->registerIndex);
            }
            else if(stmtNode->child->dataType == FLOAT_TYPE)
            {
                freeRegister(FLOAT_REG, stmtNode->child->registerIndex);
            }
            break;
        case IF_STMT:
            codeGenIfStmt(stmtNode);
            break;
        case FUNCTION_CALL_STMT:
            codeGenFunctionCall(stmtNode);
            if(stmtNode->registerIndex != -1)
            {
                if(stmtNode->dataType == INT_TYPE)
                {
                    freeRegister(INT_REG, stmtNode->registerIndex);
                }
                else if(stmtNode->dataType == FLOAT_TYPE)
                {
                    freeRegister(FLOAT_REG, stmtNode->registerIndex);
                }
            }
            break;
        case RETURN_STMT:
            codeGenReturnStmt(stmtNode);
            break;
        default:
            printf("Unhandle case in void processStmtNode(AST_NODE* stmtNode)\n");
            break;
        }
    }
}


void codeGenGeneralNode(AST_NODE* node)
{
    AST_NODE *traverseChildren = node->child;
    AST_NODE *lastChild = NULL;
    switch(node->nodeType)
    {
    case VARIABLE_DECL_LIST_NODE:
        /*TODO initial*/
        while(traverseChildren)
        {
            AST_NODE *typeNode = traverseChildren->child;
            AST_NODE *idNode = typeNode->rightSibling;

            while(idNode) {
                if(idNode->semantic_value.identifierSemanticValue.kind == WITH_INIT_ID) {
                    AST_NODE *constValueNode = idNode->child;

                    if(typeNode->dataType == INT_TYPE) {
                        int constValueIndex = getRegister(INT_REG);
                        char *constValueRegName;
                        int constantLabelNumber = codeGenConstantLabel(INTEGERC, &constValueNode->semantic_value.exprSemanticValue.constEvalValue.iValue);

                        codeGenPrepareRegister(INT_REG, constValueIndex, 0, 0, &constValueRegName);
                        fprintf(g_codeGenOutputFp, "lw %s, _CONSTANT_%d\n", constValueRegName, constantLabelNumber);
                        fprintf(g_codeGenOutputFp, "sw %s, %d($fp)\n", constValueRegName, idNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->offsetInAR);
                        freeRegister(INT_REG, constValueIndex);
                    }
                    else if(typeNode->dataType == FLOAT_TYPE) {
                        int constValueIndex = getRegister(FLOAT_REG);
                        char *constValueRegName;
                        int constantLabelNumber = codeGenConstantLabel(FLOATC, &constValueNode->semantic_value.exprSemanticValue.constEvalValue.fValue);

                        codeGenPrepareRegister(FLOAT_REG, constValueIndex, 0, 0, &constValueRegName);
                        fprintf(g_codeGenOutputFp, "l.s %s, _CONSTANT_%d\n", constValueRegName, constantLabelNumber);
                        fprintf(g_codeGenOutputFp, "s.s %s, %d($fp)\n", constValueRegName, idNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->offsetInAR);
                        freeRegister(FLOAT_REG, constValueIndex);
                    }
                }
                idNode = idNode->rightSibling;
            }
            traverseChildren = traverseChildren->rightSibling;
        }
        break;
    case STMT_LIST_NODE:
        while(traverseChildren)
        {
            codeGenStmtNode(traverseChildren);
            traverseChildren = traverseChildren->rightSibling;
        }
        break;
    case NONEMPTY_ASSIGN_EXPR_LIST_NODE:
        while(traverseChildren)
        {
            codeGenAssignOrExpr(traverseChildren);
            if(traverseChildren->rightSibling)
            {
                if(traverseChildren->dataType == INT_TYPE)
                {
                    freeRegister(INT_REG, traverseChildren->registerIndex);
                }
                else if(traverseChildren->dataType == FLOAT_TYPE)
                {
                    freeRegister(FLOAT_REG, traverseChildren->registerIndex);
                }
            }
            lastChild = traverseChildren;
            traverseChildren = traverseChildren->rightSibling;
        }
        if (lastChild) {
            node->registerIndex = lastChild->registerIndex;
        }
        break;
    case NONEMPTY_RELOP_EXPR_LIST_NODE:
        while(traverseChildren)
        {
            codeGenExprRelatedNode(traverseChildren);
            if(traverseChildren->rightSibling)
            {
                if(traverseChildren->dataType == INT_TYPE)
                {
                    freeRegister(INT_REG, traverseChildren->registerIndex);
                }
                else if(traverseChildren->dataType == FLOAT_TYPE)
                {
                    freeRegister(FLOAT_REG, traverseChildren->registerIndex);
                }
            }
            lastChild = traverseChildren;
            traverseChildren = traverseChildren->rightSibling;
        }
        if (lastChild) {
            node->registerIndex = lastChild->registerIndex;
        }
        break;
    case NUL_NODE:
        break;
    default:
        printf("Unhandle case in void processGeneralNode(AST_NODE *node)\n");
        node->dataType = ERROR_TYPE;
        break;
    }
}
