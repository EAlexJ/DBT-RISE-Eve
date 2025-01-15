//////////////////////////////////////////////////////////////////////////////////////////
//
// Assembler for the EveCore Architecture
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// Author List:
//   Harry H. Porter
//   ...your name...    - INSERT A LINE ABOVE THIS PLACEHOLDER    <<<<<<< UPDATE THIS
//
// Revision History:
//   6 March 2024       - Harry - Development of this code begins
//   11 March 2024      - Harry - Initial version completed
//   2 April 2024       - Harry - Changes for Windows; added HEX2 output
//   29 April 2024      - Harry - Misc cleanup
//   ...DATE-TODAY...   - INSERT A LINE ABOVE THIS PLACEHOLDER    <<<<<<< UPDATE THIS
//
// Date of current version:
//
//              "=====     <spacing template>    ====="
#define VERSION "=====       29 April 2024       =====" //  <<<<<<< UPDATE THIS
//              "=====      ^^  ^  ^  ^  ^^      ====="
//              "=====      ^^  ^  ^^  ^  ^^     ====="
//////////////////////////////////////////////////////////////////////////////////////////
//
// This program can be compiled (Mac OSX, Windows) with a line such as this:
//     gcc -std=c99 -Wall -O2 asm8.c -o asm8
//
// On Windows 10 with MinGW-w64, the above line worked for me. However, it gave
// warnings about "dereferencing a type-punned pointer", which I am ignoring.
//

#ifdef HOST_IS_BIG_ENDIAN
// This should be defined iff the host machine uses the Big Endian
// byte ordering.  Typically, this is done within the file "makefile",
// when this program is compiled with the C compiler.
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long long int64_t;
typedef int int32_t;
typedef unsigned int uint32_t;

// SWAP_BYTES_32 (int32_t)  -->  int32_t
//
// This macro is used to swap the bytes in a 32-bit int.
//
#ifdef HOST_IS_BIG_ENDIAN
#define SWAP_BYTES_32(x) (x)
#else
#define SWAP_BYTES_32(x)                                                       \
  ((int32_t)((((int32_t)(x)&0xff000000) >> 24) |                               \
             (((int32_t)(x)&0x00ff0000) >> 8) |                                \
             (((int32_t)(x)&0x0000ff00) << 8) |                                \
             (((int32_t)(x)&0x000000ff) << 24)))
#endif

typedef struct Expression Expression;
typedef struct Instruction Instruction;
typedef struct TableEntry TableEntry;

// *************************  Primary data structures  ********************

union tokenValue { // The current token is represented by "nextToken" and a "tokenValue".
  TableEntry *tableEntry;
  int32_t ivalue;
} tokenValue, tokenValue2;

struct Expression {
  int op; // ID, INTEGER, or an operator (PLUS, MINUS, STAR, ...)
  Expression *expr1;
  Expression *expr2;
  TableEntry *tableEntry; // This field is used iff op == ID
  int32_t value;          // The value of this expression
  int computed; // 1 = value is correct and final; 0 = don't have result yet
};

struct Instruction {
  int op; // Code indicating the operation category (ALU, ADD16, FLASH, ...)
  int lineNum;         // In source file, for error reporting
  unsigned char byte1; // The complete opcode byte, including RegS, RegD, etc.
  char byte2;          // The second byte of the instruction, if any.
  char byte3;          // The third byte of the instruction, if any.
  Expression *expr;
  TableEntry *myLabel; // Used for label, .equ, and .string
  int32_t myLC;        // Address of this instruction
  int actualSize;      // In bytes
  Instruction *next;   // Instructions are in a linked list
};

//
// The lexical analyzer uses a temporary buffer to hold the characters
// of STRINGS and IDENTIFIERS as they are scanned from the input.
// Later, a chunk memory of the correct size is allocated and the characters
// are copied.  Therefore, there is only one buffer in existence.
// The size of this buffer creates an arbitrary limit on the length of
// all STRINGs and all SYMBOLs, including LABELs.
//
// If necessary, this constant can be safely increased.  Nothing else needs
// to be modified.
//
#define MAX_STR_LEN 10000 // Strings are limited to this many characters.

// *************************  Symbol Table  *************************
//
// Everything that has an associated spelling will be represented as a "symbol".
// This includes:
//    Keywords                (if, goto, push, call, ...)
//    Pseudo-ops              (.flash, .equ, ...)
//    User defined symbols    (MyLabel, MyEquSymbol, MyVar, ...)
//    Strings                 ID
//
// All symbols will be represented with an object of type "TableEntry".
// All symbols are stored in a single symbol table.
//
// The symbol table is organized as an array, where each array element points
// to a linked list of TableEntrys. To locate a symbol given its string,
// we compute the hash of the string, then use that as an index into the array,
// then perform a linear search of the linked list.
//
// Each symbol has a "type", which will be an integer code. (See the giant enum
// of codes.) Here are some examples:
//    Keywords                GOTO, CMP, ...
//    Instruction categories  ALU, BRANCH, ...
//    Pseudo-ops              FLASH, EQU, ...
//    User defined symbols    ID (e.g., MyLabel)
//    Strings                 ID (e.g., "hello\n"); a string is just an undefined user symbol
//
// The characters in the symbol are stored in "stringLength" and "stringChars".
// For symbols that represent user strings, there may be unprintable characters;
// otherwise, all characters will be printable.
//
// The same symbol can be used both as a string (i.e., for an .string operand) and
// as a user defined symbol. These two uses don't conflict and only a single symbol
// will be used. In fact, .string strings ("hello") are indistinguishable from
// user symbols that have simply not been given values.
//
// Symbols may be given values. This only happens for user defined symbols. The value
// is stored in the field "value". There are 2 ways a symbol can be defined:
//      Label      The symbol is equated the address of some byte in this file.
//      .equ       The symbol is equated to an expression.
// The value is initially unknown, which is indicated by resolved == 0. Once the
// value is computed, we set resolved == 1.
//
struct TableEntry {
  TableEntry *next; // Linked list of TableEntry's
  int type;         // ID, or some keyword like ADD, SUB,...
  int resolved;     // 1 = value is correct and final; 0 = don't have result yet
  int32_t value;    // The value of this symbol
  int lineNumber;   // Line number where defined
  int stringLength; // Number of characters
  char stringChars[0]; // The characters
};

#define SYMBOL_TABLE_HASH_SIZE                                                 \
  30011 // Size of hash table for symbol table (a prime number)
// #define SYMBOL_TABLE_HASH_SIZE 43    // **** Value used for testing

#define SYMBOL_COUNT_LIMIT (SYMBOL_TABLE_HASH_SIZE * 4)
static TableEntry *symbolTableIndex[SYMBOL_TABLE_HASH_SIZE];

//*************************  Global variables *************************

#define VERILOG 1 // print style... verilog output
#define HEX2 2    //                hex2 output
#define LISTING 3 //                most human readable

#define FLASH_SEGMENT 1
#define RAM_SEGMENT 2

int nextToken;  // The current token
int nextToken2; // The token after that
int currentLine = 1;
int errorsDetected = 0;
int warningsDetected = 0;
Instruction *instrList;
Instruction *lastInstr;
int currentSegment;       // -1 = not in a segment; 1 = FLASH; 2 = RAM
int32_t currentLC;        // Location Counter: the address of instruction
int32_t flashLC;          // FLASH address in which to place next thing
int32_t ramLC;            // RAM address in which to place next thing
int numberOfSymbols = -1; // Symbols are numbered 1,2,...
int countOfSymbols = 0;   // Count of symbols as added to symbol table

int unresolvedEquates;  // True if we need to re-process equates
int anyEquatesResolved; // True if anything change this iteration

Expression *zeroExpression; // An expression for "0"

int commandOptionS = 0; // True: print the symbol table
int commandOptionD = 0; // True: print the instruction list
int commandOptionX = 0; // True: run the lexer only, then stop
int commandOptionL =
    0; // True: produce output in the style: most human readable
int commandOptionSV = 0;   // True: produce output as Verilog statements
int commandOptionHex1 = 0; // True: produce output in the style: hex1 format
int commandOptionHex2 = 0; // True: produce output in the style: hex2 format
int commandOptionW = 0;    // True: Suppress warning messages

char *commandInFileName = NULL;  // The .s filename, if provided
char *commandOutFileName = NULL; // The .o filename
char *hexDelim = NULL; // The -delim delimiter string for printing HEX2 files
char *varName = NULL;  // The -var string for printing Verilog output
FILE *inputFile;       // The .s input file
FILE *outputFile;      // The .o output file
int nextLineNumberToPrint; // The source line number, used when printing output

char *simpleTokenBuffer = NULL; // Used for user input (testing PerformMultiply)

char uniBuffer[5] = {0, 0, 0, 0, 0}; // Used for UTF-8 checking
int uniBufferLength = 0;             // .
int uniExpectedSize = 0;             // .

// *****************************  Function Prototypes  *********************************

char *StringForCode(int i);
void InitKeywords();
int CategoryOf(int tokType);
int IsReg8(int tokType);
void PrintWarning(char *msg);
void PrintWarningWithID(char *msg);
void PrintError(char *msg);
void PrintError2(char *msg, char *msg2);
void PrintErrorWithToken(char *msg);
void FatalError(const char *msg);
void ProgramLogicError(char *msg);
void ErrorExit();
void CheckForAbort();
void PrintWarningCount();

int main(int argc, char **argv);
void ProcessCommandLine(int argc, char **argv);
void PrintHelp();
void PrintHelp2();

void TestLexer();
void Scan();
int GetToken(void);
int IsAlpha(char ch);
int IsDigit(char ch);
int ScanEscape();
int IsEscape(char ch);
TableEntry *LookupAndAdd(char *givenStr, int newType);
TableEntry *LookupAndAdd2(char *givenStr, int length, int newType);
void PrintSymbolTable();
void PrintString(TableEntry *);
void ProcessExpressions();
void AddInstrToList(Instruction *, int instrSize);
void PrintInstructionList();
void PrintExpr(Expression *expr);
void PrintInstructionSummary(char *message, Instruction *instrPtr);
void DumpSymbolTable();
void DumpSymbol(char *message, TableEntry *p);
void DumpStringInWidth(TableEntry *tableEntry, int width);
void DumpExpr(char *message, Expression *p);
Instruction *NewInstr(int opcode);
Expression *NewExpression(int op);
Expression *NegateExpression(Expression *sub);
void GetOneInstruction();
void ScanRestOfLine();
int GetRegD(Instruction *p);
int GetRegS(Instruction *p);
int NextIsNot(int tokType, char *message);
int NextIsNotOne(char *message);
Expression *GetExpr();
Expression *ParseExpr0();
Expression *ParseExpr1();
Expression *ParseExpr2();
Expression *ParseExpr3();
Expression *ParseExpr4();
Expression *ParseExpr5();
Expression *ParseExpr6();
int NotInAnySegment();

void EvalExpr(Expression *expr, int PrintErrors);
void ProcessAllEquates();
void ResolveOneEquate(Instruction *instr, int PrintErrors);

void FinalCheck();

void printOutput(int style); // style: VERILOG, HEX2, LISTING
void printHex1Output();      // style: HEX1
void PrintOneLine(char *separator);
void PrintAddress(int addr, int style);

int hexCharToInt(char ch);
char intToHexChar(int i);
int isAllHex(char *str);

int performAdd(int32_t x, int32_t y, int32_t *ansPtr);
int performSubtract(int32_t x, int32_t y, int32_t *ansPtr);
int performMultiply(int32_t x, int32_t y, int32_t *ansPtr);

int addUTF8Char(int c, int wantPrintingForOK);
int resetUTF8Buffer();
int64_t FromUTF8(const char *ptr);
int ToUTF8(char *ptr, int codepoint);

unsigned ComputeHash(const char *str, int length);
int BytesEqual(const char *p, const char *q, int length);

// **********************************  OPCODES  ****************************************

#define OP_MOV 0x40
#define OP_MOVI 0xa0
#define OP_LD 0x80
#define OP_ST 0x88
#define OP_PUSH 0x90
#define OP_POP 0x98
#define OP_ADD16 0xa8
#define OP_AND 0xb0
#define OP_OR 0xb1
#define OP_XOR 0xb2
#define OP_NOT 0xb3
#define OP_ADD 0xb4
#define OP_ADDC 0xb5
#define OP_SUB 0xb6
#define OP_SUBC 0xb7
#define OP_NEG 0xb8
#define OP_CLR 0xb9
#define OP_SHL 0xba
#define OP_SHLC 0xbb
#define OP_SHR 0xbc
#define OP_SHRC 0xbd
#define OP_INCR 0xbe
#define OP_DECR 0xbf
#define OP_LDSP 0xc0
#define OP_LDCODE 0xc1
#define OP_GOTOXY 0xc2
#define OP_CALL 0xc3
#define OP_RET 0xc4
#define OP_IN 0xc5
#define OP_OUT 0xc6
#define OP_LDM 0xc7
#define OP_STM 0xc8
#define OP_CMP 0xc9
#define OP_BRANCH 0xd0
#define OP_NOP 0x00

// Branch Conditions
#define COND_EQ 0x00
#define COND_NE 0x01
#define COND_LT 0x02
#define COND_LE 0x03
#define COND_GT 0x04
#define COND_GE 0x05
#define COND_LTU 0x06
#define COND_LEU 0x07
#define COND_GTU 0x08
#define COND_GEU 0x09
#define COND_SN0 0x0a
#define COND_SN1 0x0b
#define COND_OV0 0x0c
#define COND_OV1 0x0d
#define COND_TRUE 0x0e
#define COND_UNDEF 0x0f

// ************************  INTERNAL CODE NUMBERS  ************************
//
// We use these codes to identify what sort of thing we are dealing with.
//
// Although the codes are used for different sorts of "thing", we have
// a single, unified set of codes.
//
// Note that there is a difference between these "codes"
// and the "symbols" appearing in the source file.

enum {

  // Assembler directives...

  BYTE,
  HALFWORD,
  WORD,
  STRINGOP,
  SKIP,
  EQU,
  FLASH,
  RAM,

  // Registers...

  A,
  B,
  C,
  D,
  X,
  Y,
  M1,
  M2,
  XY,
  M,
  SP,
  CY,
  SN,
  ZE,
  OV,

  // Keywords...

  IF,
  GOTO,
  PUSH,
  POP,
  CODE,
  IN,
  OUT,
  CALL,
  RET,
  NOP,
  CMP,

  // Instruction categories

  MOV,
  MOVI,
  LD,
  ST,
  ADD16,
  ALU,
  LDSP,
  LDCODE,
  GOTOXY,
  LDM,
  STM,
  BRANCH,

  // Additional token types...

  EOL,
  LABEL,
  ID,
  INTEGER,
  STRING,
  COMMA,
  PLUS,
  MINUS,
  EQUAL,
  EQUALEQUAL,
  NOTEQUAL,
  COLON,
  STAR,
  SLASH,
  PERCENT,
  LPAREN,
  RPAREN,
  LBRACKET,
  RBRACKET,
  AMPERSAND,
  BAR,
  CARET,
  BANG,
  LT,
  LTU,
  LE,
  LEU,
  LTLT,
  LTLTLT,
  GT,
  GTU,
  GE,
  GEU,
  GTGT,
  GTGTGT

  // NOTE: EOF elsewhere is defined as -1.

};

// ********************************************************************
//
// StringForCode (code) --> string
//
// This function prints the given code in human readable form.
//
char *StringForCode(int i) {
  switch (i) {

    // Assembler directives...

  case BYTE:
    return "BYTE";
  case HALFWORD:
    return "HALFWRD";
  case WORD:
    return "WORD";
  case STRINGOP:
    return "STRING";
  case SKIP:
    return "SKIP";
  case EQU:
    return "EQU";
  case FLASH:
    return "FLASH";
  case RAM:
    return "RAM";

    // Registers...

  case A:
    return "A";
  case B:
    return "B";
  case C:
    return "C";
  case D:
    return "D";
  case X:
    return "X";
  case Y:
    return "Y";
  case M1:
    return "M1";
  case M2:
    return "M2";
  case XY:
    return "XY";
  case M:
    return "M";
  case SP:
    return "SP";
  case CY:
    return "CY";
  case SN:
    return "SN";
  case ZE:
    return "ZE";
  case OV:
    return "OV";

    // Keywords...

  case IF:
    return "IF";
  case GOTO:
    return "GOTO";
  case PUSH:
    return "PUSH";
  case POP:
    return "POP";
  case CODE:
    return "CODE";
  case IN:
    return "IN";
  case OUT:
    return "OUT";
  case CALL:
    return "CALL";
  case RET:
    return "RET";
  case NOP:
    return "NOP";
  case CMP:
    return "CMP";

    // Instruction categories...

  case MOV:
    return "MOV";
  case MOVI:
    return "MOVI";
  case LD:
    return "LD";
  case ST:
    return "ST";
  case ADD16:
    return "ADD16";
  case ALU:
    return "ALU";
  case LDSP:
    return "LDSP";
  case LDCODE:
    return "LDCODE";
  case GOTOXY:
    return "GOTOXY";
  case LDM:
    return "LDM";
  case STM:
    return "STM";
  case BRANCH:
    return "BRANCH";

    // Additional token types...

  case EOL:
    return "EOL";
  case LABEL:
    return "LABEL";
  case ID:
    return "ID";
  case INTEGER:
    return "INTEGER";
  case STRING:
    return "STRING";
  case COMMA:
    return "COMMA";
  case PLUS:
    return "PLUS";
  case MINUS:
    return "MINUS";
  case EQUAL:
    return "EQUAL";
  case EQUALEQUAL:
    return "EQUALEQUAL";
  case NOTEQUAL:
    return "NOTEQUAL";
  case COLON:
    return "COLON";
  case STAR:
    return "STAR";
  case SLASH:
    return "SLASH";
  case PERCENT:
    return "PERCENT";
  case LPAREN:
    return "LPAREN";
  case RPAREN:
    return "RPAREN";
  case LBRACKET:
    return "LBRACKET";
  case RBRACKET:
    return "RBRACKET";
  case AMPERSAND:
    return "AMPERSAND";
  case BAR:
    return "BAR";
  case CARET:
    return "CARET";
  case BANG:
    return "BANG";
  case LT:
    return "LT";
  case LTU:
    return "LTU";
  case LE:
    return "LE";
  case LEU:
    return "LEU";
  case LTLT:
    return "LTLT";
  case LTLTLT:
    return "LTLTLT";
  case GT:
    return "GT";
  case GTU:
    return "GTU";
  case GE:
    return "GE";
  case GEU:
    return "GEU";
  case GTGT:
    return "GTGT";
  case GTGTGT:
    return "GTGTGT";

    // EOF...

  case EOF:
    return "EOF";

  default:
    return "*****  unknown symbol  *****";
  }
}

// ************************************************************
//
// InitKeywords ()
//
// This function adds each keyword to the symbol table
// with the corresponding type code.
//
void InitKeywords() {

  LookupAndAdd(".byte", BYTE);
  LookupAndAdd(".halfword", HALFWORD);
  LookupAndAdd(".word", WORD);
  LookupAndAdd(".string", STRINGOP);
  LookupAndAdd(".skip", SKIP);
  LookupAndAdd(".equ", EQU);
  LookupAndAdd(".flash", FLASH);
  LookupAndAdd(".ram", RAM);

  LookupAndAdd("A", A);
  LookupAndAdd("B", B);
  LookupAndAdd("C", C);
  LookupAndAdd("D", D);
  LookupAndAdd("X", X);
  LookupAndAdd("Y", Y);
  LookupAndAdd("M1", M1);
  LookupAndAdd("M2", M2);
  LookupAndAdd("XY", XY);
  LookupAndAdd("M", M);
  LookupAndAdd("SP", SP);
  LookupAndAdd("CY", CY);
  LookupAndAdd("SN", SN);
  LookupAndAdd("ZE", ZE);
  LookupAndAdd("OV", OV);

  LookupAndAdd("if", IF);
  LookupAndAdd("goto", GOTO);
  LookupAndAdd("push", PUSH);
  LookupAndAdd("pop", POP);
  LookupAndAdd("code", CODE);
  LookupAndAdd("in", IN);
  LookupAndAdd("out", OUT);
  LookupAndAdd("call", CALL);
  LookupAndAdd("ret", RET);
  LookupAndAdd("nop", NOP);
  LookupAndAdd("cmp", CMP);
}

// ********************************************************************************
//
// IsReg8 (tokType)
//
// This function returns an integer in 0..7 if this token is A,B,C,D,X,Y,M1, or M2.
// Returns -1 if not.
//
int IsReg8(int tokType) {
  switch (tokType) {
  case A:
    return 0;
  case B:
    return 1;
  case C:
    return 2;
  case D:
    return 3;
  case X:
    return 4;
  case Y:
    return 5;
  case M1:
    return 6;
  case M2:
    return 7;
  default:
    return -1;
  }
}

// ********************************************************************************
//
// PrintWarning (msg)
//
// This function is called to print a warning message and the current line
// number.  It returns.
//
void PrintWarning(char *msg) {
  if (!commandOptionW) {
    fprintf(stderr, "Warning on line %d: %s\n", currentLine, msg);
    warningsDetected++;
  }
}

// ********************************************************************************
//
// PrintWarningWithID (msg)
//
// This function is called to print a warning message and the current line
// number.  It returns.
//
void PrintWarningWithID(char *msg) {
  char *str;
  if (!commandOptionW) {
    if (nextToken == ID) {
      str = tokenValue.tableEntry->stringChars;
      fprintf(stderr, "Warning on line %d: %s [ID: \"%s\"]\n", currentLine, msg,
              str);
    } else {
      fprintf(stderr, "Warning on line %d: %s\n", currentLine, msg);
    }
    warningsDetected++;
  }
}

// ********************************************************************************
//
// PrintError (msg)
//
// This function is called to print an error message and the current line
// number.  It returns.
//
void PrintError(char *msg) {
  fprintf(stderr, "Error on line %d: %s\n", currentLine, msg);
  errorsDetected++;
}

// ********************************************************************************
//
// PrintError2 (msg, msg2)
//
// This function is called to print an error message and the current line
// number.  It returns.
//
void PrintError2(char *msg, char *msg2) {
  fprintf(stderr, "Error on line %d: %s%s\n", currentLine, msg, msg2);
  errorsDetected++;
}

// ********************************************************************************
//
// PrintErrorWithToken (msg)
//
// This function is called to print an error message, the current line
// number, and the current token.  It returns.
//
void PrintErrorWithToken(char *msg) {
  char *str;
  if (nextToken == ID) {
    str = tokenValue.tableEntry->stringChars;
    fprintf(stderr, "Error on line %d: %s [Location of error: \"%s\"]\n",
            currentLine, msg, str);
  } else if (nextToken == INTEGER) {
    fprintf(stderr,
            "Error on line %d: %s [Location of error: INTEGER (%d = 0x%x)]\n",
            currentLine, msg, tokenValue.ivalue, tokenValue.ivalue);
  } else {
    str = StringForCode(nextToken);
    fprintf(stderr, "Error on line %d: %s [Location of error: %s]\n",
            currentLine, msg, str);
  }
  errorsDetected++;
}

// ********************************************************************************
//
// FatalError (msg)
//
// This function prints the given error message on stderr and terminates the
// emulator by calling ErrorExit().
//
void FatalError(const char *msg) {
  fprintf(stderr, "\n***** Assembler Error: %s *****\n\n", msg);
  ErrorExit();
}

// ********************************************************************************
//
// ProgramLogicError (msg)
//
// Print a message and terminate immediately.
//
void ProgramLogicError(char *msg) {
  fprintf(stderr, "\n***** PROGRAM LOGIC ERROR on line %d: %s *****\n\n",
          currentLine, msg);
  ErrorExit();
}

// ********************************************************************************
//
// ErrorExit ()
//
// This function removes the output (.o) file and calls exit().
//
void ErrorExit() {
  remove(commandOutFileName); // Errors (e.g., no such file) are ignored.
  exit(EXIT_FAILURE);         // EXIT_FAILURE = 1
}

// ********************************************************************************
//
// CheckForAbort ()
//
// If any errors have been detected, then print messages and terminate.
//
void CheckForAbort() {
  if (errorsDetected) {
    PrintWarningCount();
    if (errorsDetected == 1) {
      fprintf(stderr, "\n***** 1 error was detected! *****\n\n");
    } else {
      fprintf(stderr, "\n***** %d errors were detected! *****\n\n",
              errorsDetected);
    }
    ErrorExit();
  }
}

// ********************************************************************************
//
// PrintWarningCount ()
//
// If there were any warnings printed, then print the count.
//
void PrintWarningCount() {
  if (warningsDetected == 1) {
    fprintf(stderr, "\n***** 1 warning was detected! *****\n");
  } else if (warningsDetected) {
    fprintf(stderr, "\n***** %d warnings were detected! *****\n",
            warningsDetected);
  }
}

// ********************************************************************************
//
// main ()
//
// Initialize, read the input file, perform processing, and write the output file.
//
int main(int argc, char **argv) {
  int i;

  char c = 0xff;
  if (c >= 0) {
    FatalError("This program only runs on computers where \"char\" values are "
               "signed.");
  }

  // Initialize the symbol table...
  for (i = 0; i < SYMBOL_TABLE_HASH_SIZE; i++) {
    symbolTableIndex[i] = NULL;
  }
  InitKeywords();

  zeroExpression = NewExpression(INTEGER);
  zeroExpression->value = 0;
  zeroExpression->computed = 1;

  ProcessCommandLine(argc, argv);

  // Test lexer, if option -x was used
  if (commandOptionX) {
    TestLexer();
    exit(EXIT_SUCCESS);
  }

  instrList = NULL;
  lastInstr = NULL;
  currentLC = 0;
  flashLC = 0;
  ramLC = 0;
  currentSegment = -1;
  unresolvedEquates = 0;
  anyEquatesResolved = 0;

  currentLine = 1;
  Scan(); // read nextToken and nextToken2
  Scan();

  // printf ("========== About to Scan and parse the input file ==========\n");
  // Read through the source file and build the list of instructions.
  while (nextToken != EOF) {
    GetOneInstruction();
  }

  // Save currentLC from previous segment. (flashLC & ramLC will be printed in output.)
  if (currentSegment == FLASH_SEGMENT) {
    flashLC = currentLC;
  } else if (currentSegment == RAM_SEGMENT) {
    ramLC = currentLC;
  }

  if (instrList == NULL) {
    PrintError("No legal instructions encountered");
    CheckForAbort();
  }

  // printf ("========== About to call ProcessAllEquates ==========\n");
  ProcessAllEquates();

  // printf ("========== About to call ProcessExpressions ==========\n");
  // Evaluate all expressions and save the value in byte2 and byte3.
  ProcessExpressions();

  // Print the values of all symbols, if option -s was specified.
  if (commandOptionS)
    PrintSymbolTable();

  // Print the instruction list, if option -d was specified.
  if (commandOptionD)
    PrintInstructionList();

  // If errors, terminate. If warnings, keep going.
  CheckForAbort();

  // Run through the instruction list check everything for consistency.
  FinalCheck();

  if (errorsDetected > 0)
    ProgramLogicError("FinalCheck does not produce errors");

  // printf ("========== About to produce output file ==========\n");
  if (commandOptionL) {
    printOutput(LISTING);
  } else if (commandOptionHex2) {
    printOutput(HEX2);
  } else if (commandOptionHex1) {
    printHex1Output();
  } else if (commandOptionSV) {
    printOutput(VERILOG);
  } else {
    ProgramLogicError("no other output options");
  }

  // Close the output file properly.
  errno = 0;
  if (fclose(outputFile)) {
    if (errno)
      perror("Host error from fclose");
    FatalError("Problems closing the output file");
  }

  PrintWarningCount();
  exit(EXIT_SUCCESS);
}

// ********************************************************************************
//
// ProcessCommandLine (argc, argv)
//
// This function processes the command line options.
//
void ProcessCommandLine(int argc, char **argv) {
  int argCount;
  int len;
  for (argc--, argv++; argc > 0; argc -= argCount, argv += argCount) {
    argCount = 1;

    // Scan the -h option (for printing help info)
    if (!strcmp(*argv, "-h")) {
      PrintHelp();
      exit(EXIT_FAILURE);

      // Scan the -h2 option (for syntax info)
    } else if (!strcmp(*argv, "-h2")) {
      PrintHelp2();
      exit(EXIT_FAILURE);

      // Scan the -s option (for printing the symbol table)
    } else if (!strcmp(*argv, "-s")) {
      if (commandOptionS) {
        FatalError("Multiple occurrences of the -s command line option.");
      }
      commandOptionS = 1;

      // Scan the -l option (for printing a listing to the output)
    } else if (!strcmp(*argv, "-l")) {
      if (commandOptionL) {
        FatalError("Multiple occurrences of the -l command line option.");
      }
      commandOptionL = 1;

      // Scan the -sv option (produce output in the Verilog format)
    } else if (!strcmp(*argv, "-sv")) {
      if (commandOptionSV) {
        FatalError("Multiple occurrences of the -sv command line option.");
      }
      commandOptionSV = 1;

      // Scan the -hex1 option (produce output in the hex1 format)
    } else if (!strcmp(*argv, "-hex1")) {
      if (commandOptionHex1) {
        FatalError("Multiple occurrences of the -hex1 command line option.");
      }
      commandOptionHex1 = 1;

      // Scan the -hex2 option (produce output in the hex2 format)
    } else if (!strcmp(*argv, "-hex2")) {
      if (commandOptionHex2) {
        FatalError("Multiple occurrences of the -hex2 command line option.");
      }
      commandOptionHex2 = 1;

      // Scan the -w option (suppress warnings)
    } else if (!strcmp(*argv, "-w")) {
      if (commandOptionW) {
        FatalError("Multiple occurrences of the -w command line option.");
      }
      commandOptionW = 1;

      // Scan the -o option, which should be followed by a file name
    } else if (!strcmp(*argv, "-o")) {
      if (argc <= 1) {
        FatalError(
            "Expecting filename after -o option.  Use -h for help display.");
      } else {
        argCount++;
        if (commandOutFileName == NULL) {
          commandOutFileName = *(argv + 1);
        } else {
          FatalError("Invalid command line.  Multiple output files.  Use -h "
                     "for help display.");
        }
      }

      // Scan the -delim option, which should be followed by one or more characters
    } else if (!strcmp(*argv, "-delim")) {
      if (argc <= 1) {
        FatalError("Expecting something after -delim option.  Use -h for help "
                   "display.");
      } else {
        argCount++;
        if (hexDelim == NULL) {
          hexDelim = *(argv + 1);
        } else {
          FatalError("Invalid command line.  Multiple -delim options.  Use -h "
                     "for help display.");
        }
      }

      // Scan the -var option, which should be followed by the variable name
    } else if (!strcmp(*argv, "-var")) {
      if (argc <= 1) {
        FatalError(
            "Expecting something after -var option.  Use -h for help display.");
      } else {
        argCount++;
        if (varName == NULL) {
          varName = *(argv + 1);
        } else {
          FatalError("Invalid command line.  Multiple -var options.  Use -h "
                     "for help display.");
        }
      }

      // Scan the -d option (for printing debugging info - the instruction list)
    } else if (!strcmp(*argv, "-d")) {
      if (commandOptionD) {
        FatalError("Multiple occurrences of the -d command line option.");
      }
      commandOptionD = 1;

      // Scan the -x option (for running the lexer only)
    } else if (!strcmp(*argv, "-x")) {
      if (commandOptionX) {
        FatalError("Multiple occurrences of the -x command line option.");
      }
      commandOptionX = 1;

      // Scan an input file name
    } else if ((*argv)[0] != '-') {
      if (commandInFileName == NULL) {
        commandInFileName = *argv;
      } else {
        FatalError("Invalid command line.  Multiple input files.  Use -h for "
                   "help display.");
      }
    } else {
      fprintf(stderr, "Command line option: %s\n", *argv);
      FatalError("Invalid command line option.  Use -h for help display.");
    }
  }

  // Make sure at most one of "-sv", "-l", "-hex1", and "-hex2" was used. If none, the default is -hex1.
  if (commandOptionSV + commandOptionL + commandOptionHex1 +
          commandOptionHex2 ==
      0) {
    commandOptionHex1 = 1;
  } else if (commandOptionSV + commandOptionL + commandOptionHex1 +
                 commandOptionHex2 >
             1) {
    FatalError("At most one of \"-sv\", \"-l\", \"-hex1\", and \"-hex2\" may "
               "be used.  Use -h for help display.");
  }

  // If -delim was used, make sure -hex2 was used. Otherwise, set the default.
  if (hexDelim == NULL) {
    hexDelim = "//";
  } else if (!commandOptionHex2) {
    FatalError("The \"-delim\" option requires the \"-hex2\" option.  Use -h "
               "for help display.");
  }

  // If -var was used, make sure -sv is used. Otherwise, set the default.
  if (varName == NULL) {
    varName = "data    ";
  } else if (commandOptionSV != 1) {
    FatalError("The \"-var\" option may only be used with the \"-sv\" option.  "
               "Use -h for help display.");
  }

  // Open the input (.s) file
  if (commandInFileName == NULL) {
    FatalError("You must supply an input (\".s\" file); stdin may not be used");
  } else {
    errno = 0;
    inputFile = fopen(commandInFileName, "r");
    if (inputFile == NULL) {
      fprintf(
          stderr,
          "\n***** Input file \"%s\" could not be opened for reading *****\n\n",
          commandInFileName);
      if (errno)
        perror("Host error from fopen");
      exit(EXIT_FAILURE);
    }
  }

  // Figure out the name of the .o file.
  if (commandOutFileName == NULL) {
    len = strlen(commandInFileName);
    if ((len > 2) && (commandInFileName[len - 2] == '.') &&
        (commandInFileName[len - 1] == 's')) {
      commandOutFileName = (char *)calloc(1, len + 3); // add "xt" and \0
      strcpy(commandOutFileName, commandInFileName);
      commandOutFileName[len - 2] = '.';
      commandOutFileName[len - 1] = 't';
      commandOutFileName[len - 0] = 'x';
      commandOutFileName[len + 1] = 't';
      commandOutFileName[len + 2] = '\0';
    } else {
      commandOutFileName = (char *)calloc(1, len + 5);
      strcpy(commandOutFileName, commandInFileName);
      commandOutFileName[len] = '.';
      commandOutFileName[len + 1] = 't';
      commandOutFileName[len + 2] = 'x';
      commandOutFileName[len + 3] = 't';
      commandOutFileName[len + 4] = '\0';
    }
  }

  // Open the output (.o) file for writing. Overwrite with no warning...
  errno = 0;
  outputFile = fopen(commandOutFileName, "w");
  // outputFile = open (commandOutFileName, O_CREAT, "w");
  if (outputFile == NULL) {
    fprintf(
        stderr,
        "\n***** Output file \"%s\" could not be opened for writing *****\n\n",
        commandOutFileName);
    if (errno)
      perror("Host error from fopen");
    exit(EXIT_FAILURE);
  }
  // outputFile = stdout;
}

// ********************************************************************************
//
// PrintHelp ()
//
// This function is invoked whenever the -h option is used on the command line.
//
void PrintHelp() {
  printf(
      "=====================================\n"
      "=====                           =====\n"
      "=====   The EveCore Assembler   =====\n"
      "=====      Harry H. Porter      =====\n" VERSION "\n"
      "=====                           =====\n"
      "=====================================\n"
      "\n"
      "Command Line Options\n"
      "====================\n"
      "  Command line options may be given in any order.\n"
      "    -h\n"
      "      Print this help info.  All other options are ignored.\n"
      "    -h2\n"
      "      Print a description of the assembler syntax.  Other options are "
      "ignored.\n"
      "    filename\n"
      "      The input source will come from this file.  (Normally the input "
      "source file will\n"
      "      end with \".s\".)  Exactly one input source file is required.\n"
      "    -o filename\n"
      "      If there are no errors, an output file will be created.  This "
      "option can be\n"
      "      used to give the output file a specific name.  If this option is "
      "not used, the\n"
      "      name of the output file will be computed from the name of the "
      "input file by\n"
      "      removing the \".s\" extension, if any, and appending \".txt\".  "
      "For example:\n"
      "           foo.s   -->  foo.txt\n"
      "           foo     -->  foo.txt\n"
      "           foo.z   -->  foo.z.txt\n"
      "    -hex1\n"
      "      Produce output in \"hex1\" format, which shows only the bytes "
      "that will go into the\n"
      "      program memory. There is one byte per line. This is the default "
      "output format.\n"
      "    -hex2\n"
      "      Produce output in \"hex2\" format, which shows the raw bytes. The "
      "source is also\n"
      "      included in the output as comments. The default is \"-hex1\".\n"
      "    -l\n"
      "      Produce output in human-readable \"listing\" format. The default "
      "is \"-hex1\".\n"
      "    -sv\n"
      "      Produce output as Verilog statements. The default is \"-hex1\".\n"
      "    -delim xxx\n"
      "      This option may only be used with the \"-hex2\" option. The "
      "characters \"xxx\" will be\n"
      "      used as the commenting character(s) in the output. If missing, "
      "the default is \"//\".\n"
      "    -var xxx\n"
      "      This option may only be used when the output will be in "
      "\"Verilog\" format. In other\n"
      "      words, this option may only be used with the \"-sv\" option. The "
      "characters \"xxx\" will\n"
      "      be used as the variable name in the Verilog output. For best "
      "results, use a string\n"
      "      with 8 characters. If missing, the default is \"data    \".\n"
      "    -w\n"
      "      Suppress the printing of all warnings.\n"
      "    -s\n"
      "      Print the symbol table.\n"
      "    -d\n"
      "      TESTING: Print internal assembler info.\n"
      "    -x\n"
      "      TESTING: Run the lexer and stop.\n");
}

// ********************************************************************************
//
// PrintHelp2 ()
//
// This function is invoked whenever the -h2 option is used on the command line.
//
void PrintHelp2() {
  printf(
      "========================================================================"
      "====================\n"
      "    Here is the syntax of the assembly language accepted by this "
      "program.\n"
      "\n"
      "    In what follows, the following abbreviations are used:\n"
      "        Reg\n"
      "           can be\n"
      "              A, B, C, D, X, Y, M1, M2\n"
      "        RegPair\n"
      "           can be\n"
      "              XY, M\n"
      "        Expr8\n"
      "           is an expression which must have an 8 bit value. The "
      "acceptable range is\n"
      "              0x00 ... 0xff\n"
      "              -128 ... 127     (signed)\n"
      "              0 ... 255        (unsigned)\n"
      "        Expr16\n"
      "           is an expression which must have an 16 bit value. The "
      "acceptable range is\n"
      "              0x0000 ... 0xffff\n"
      "              -32768 ... 32767   (signed)\n"
      "              0 ... 65535        (unsigned)\n"
      "        Expr32\n"
      "           is an arbitrary 32 bit expression. The range is\n"
      "              0x0000_0000 ... 0xffff_ffff\n"
      "              -2,147,483,648 ... 2,147,483,647   (signed)\n"
      "              0 ... 4,294,967,295                (unsigned)\n"
      "        DoubleQuotedText\n"
      "            ASCII string constants, such as:\n"
      "               \"Hello there\\n\"\n"
      "            UTF-8, which includes ASCII, is used, allowing Unicode "
      "characters.\n"
      "            Escape sequences: \\0  \\a  \\b  \\t  \\n  \\v  \\f  \\r  "
      "\\e  \\d  \\\"  \\'  \\\\  \\xHH\n"
      "\n"
      "    All expression evaluation is done using 32 bits.\n"
      "      An expression may use these operators:\n"
      "          &  |  ^  !  +  -  *  /  %%  <<  <<<  >>  >>>\n"
      "      as well as integers, identifiers, strings with exactly 4 bytes, "
      "character\n"
      "      constants, and parentheses.\n"
      "          &  |  ^  !  are 32-bitwise logical AND, OR, XOR, and NOT.\n"
      "          +  -  *  /  %%  are performed using SIGNED arithmetic and "
      "overflow is flagged.\n"
      "          >>  <<  are shift right/left logical\n"
      "          >>>  is shift right arithmetic\n"
      "          <<<  is signed shift left, with an error if significant bits "
      "are lost\n"
      "      Be careful: Values within 2147483648...4294967295 are allowed but "
      "are treated as\n"
      "      signed, negative values. 4294967296 is illegal, but 4294967295 + "
      "1 is okay and is zero!\n"
      "\n"
      "    Assembler directives\n"
      "      .flash                  // The following instructions will be "
      "placed in flash\n"
      "      .ram                    // The following instructions will be "
      "placed in ram\n"
      "      .byte      Expr8\n"
      "      .halfword  Expr16\n"
      "      .word      Expr32\n"
      "      .string    DoubleQuotedText\n"
      "      .skip      Expr32       // A region of that many bytes will be "
      "allocated       \n"
      "      .equ       Expr32       // The label is equated (set to) the "
      "value of the expression\n"
      "\n"
      "    A label is an identifier followed by a colon. The label must appear "
      "first on\n"
      "      the line and may prefix any instruction or directive, except "
      "\".flash\" or \".ram\".\n"
      "\n"
      "    The .equ directive requires a label, in which case the label is "
      "equated to\n"
      "      the given expression. Otherwise, the label is equated to the "
      "current address,\n"
      "      i.e., the location at which the next thing is placed.\n"
      "\n"
      "    Only the following can appear after a \".ram\" directive:\n"
      "      .byte   .halfword   .word   .skip   .equ\n"
      "    The initializing expressions for .byte, .halfword, and .word are "
      "not allowed.\n"
      "\n"
      "    Comments:   // thru end-of-line\n"
      "                /* ... */\n"
      "\n"
      "    Capitalization is significant. All registers are uppercase. All "
      "keywords and assembler\n"
      "    directives are lowercase.\n"
      "\n"
      "    LEXICAL TOKENS:\n"
      "      A  B  C  D  X  Y  M1  M2  XY  M  SP  CY  SN  ZE  OV\n"
      "      &  |  ^  !  +  -  *   /   %%   <<  <<<  >>  >>>  :  (  )  [  ]\n"
      "      =    ==    !=    <    <=    >    >=    <u   <=u   >u   >=u\n"
      "      if   goto   push   pop   code   in   out   call   ret   nop\n"
      "      .flash  .ram  .byte  .halfword  .word  .string  .skip  .equ\n"
      "\n"
      "    INTEGERS may be specified in\n"
      "        decimal, for example:  32767\n"
      "        hex, for example:  0x1234abcd\n"
      "      Underscores may be inserted in hex values for clarity: "
      "0x1234_abcd\n"
      "\n"
      "    STRINGS may be used as integers, but must have exactly 4 bytes.\n"
      "      The string \"abcd\" is equivalent to 0x61626364, i.e., 1633837924 "
      "in decimal.\n"
      "\n"
      "    CHARACTER constants may be used where any number is allowed. They "
      "are specified\n"
      "      with single quotes. For example 'A' is equivalent to 0x41, which "
      "is 65 in decimal. \n"
      "\n"
      "    LABELS (also called \"symbols\" or \"identifiers\") may contain "
      "letters, digits, and\n"
      "      the underscore. They must begin with a letter or underscore. Case "
      "is significant. When\n"
      "      defined, the label must precede anything else on the line and "
      "must be followed by a colon.\n"
      "\n"
      "    There can be at most one instruction or assembler directive per "
      "line. An instruction\n"
      "      must not span multiple lines. Semi-colons are not used; the "
      "end-of-line is significant.\n"
      "\n"
      "    Here are the instructions:\n"
      "\n"
      "      MOV  --  1 byte\n"
      "      ===============\n"
      "          Reg = Reg\n"
      "\n"
      "      MOVI  --  2 bytes\n"
      "      =================\n"
      "          Reg = Expr8\n"
      "\n"
      "      ALU  --  1 byte\n"
      "      ===============\n"
      "          A = A & B\n"
      "          A = A | B\n"
      "          A = A ^ B\n"
      "          A = !A\n"
      "          A = A + B\n"
      "          A = A + B + CY\n"
      "          A = A - B\n"
      "          A = A - B - CY\n"
      "          A = A + 1\n"
      "          A = A - 1\n"
      "          A = -A\n"
      "          A = 0\n"
      "          A = A << 1\n"
      "          A = A <<< 1\n"
      "          A = A >> 1\n"
      "          A = A >>> 1\n"
      "\n"
      "      ADD16  --  3 bytes\n"
      "      ==================\n"
      "          RegPair = XY\n"
      "          RegPair = XY + Expr16    // or - Expr16\n"
      "          RegPair = M\n"
      "          RegPair = M + Expr16     // or - Expr16\n"
      "          RegPair = A\n"
      "          RegPair = A + Expr16     // or - Expr16\n"
      "          RegPair = Expr16\n"
      "\n"
      "      CMP  --  1 byte\n"
      "      ===============\n"
      "          cmp A:B\n"
      "\n"
      "      LD  --  3 bytes\n"
      "      ===============\n"
      "          Reg = [Expr16]\n"
      "\n"
      "      ST  --  3 bytes\n"
      "      ===============\n"
      "          [Expr16] = Reg\n"
      "\n"
      "      LDM  --  3 bytes\n"
      "      ================\n"
      "          A = [M]\n"
      "          A = [M + Expr16]     // or - Expr16\n"
      "\n"
      "      STM  --  3 bytes\n"
      "      ================\n"
      "          [M] = A\n"
      "          [M + Expr16] = A     // or - Expr16\n"
      "\n"
      "      LDCODE  --  1 byte\n"
      "      ==================\n"
      "          A = code [XY]\n"
      "\n"
      "      BRANCH  --  3 bytes\n"
      "      ===================\n"
      "          if A CONDITION B goto Expr16\n"
      "                 where CONDITION is...\n"
      "                       ==    !=\n"
      "                       <     <=     >     >=\n"
      "                       <u    <=u    >u    >=u\n"
      "                       ZE    CY     SN    OV\n"
      "                      !ZE   !CY    !SN   !OV\n"
      "\n"
      "      GOTO  --  3 bytes\n"
      "      =================\n"
      "          goto Expr16\n"
      "\n"
      "      GOTOXY  --  1 byte\n"
      "      ==================\n"
      "          goto XY\n"
      "\n"
      "      LDSP  --  3 bytes\n"
      "      =================\n"
      "          SP = Expr16\n"
      "\n"
      "      PUSH  --  1 byte\n"
      "      ================\n"
      "          push Reg\n"
      "\n"
      "      POP  --  1 byte\n"
      "      ===============\n"
      "          pop Reg\n"
      "\n"
      "      CALL  --  3 bytes\n"
      "      =================\n"
      "          call Expr16\n"
      "\n"
      "      RET  --  1 byte\n"
      "      ===============\n"
      "          ret\n"
      "\n"
      "      IN  --  2 bytes\n"
      "      ===============\n"
      "          A = in Expr8 \n"
      "\n"
      "      OUT  --  2 bytes\n"
      "      ================\n"
      "          out Expr8 = A\n"
      "\n"
      "      NOP  --  1 byte\n"
      "      ===============\n"
      "          nop\n"
      "\n"
      "    For further information consult the document:\n"
      "        \"EveCore: Instruction Set Architecture\"\n"
      "========================================================================"
      "====================\n"
      "\n");
}

// ********************************************************************************
//
// TestLexer ()
//
// Call GetToken in a loop, printing out each token as we go.
// Return upon reaching the EOF token.
//
void TestLexer() {
  Scan();
  while (1) {
    Scan();
    fprintf(outputFile, "%d\t%s\t\t", currentLine, StringForCode(nextToken));
    switch (nextToken) {
    case ID:
      PrintString(tokenValue.tableEntry);
      break;
    case STRING:
      fprintf(outputFile, "\"");
      PrintString(tokenValue.tableEntry);
      fprintf(outputFile, "\"");
      break;
    case INTEGER:
      fprintf(outputFile, "0x%08x\t%d", tokenValue.ivalue, tokenValue.ivalue);
      break;
    }
    fprintf(outputFile, "\n");
    if (nextToken == EOF) {
      break;
    }
  }
}

// ********************************************************************************
//
// Scan ()
//
// This function advances one token by calling GetToken.
// This will update: token, tokenValue, token2, tokenValue2, currentLine.
// Can be called repeatedly after end-of-file is reached.
//
void Scan() {
  // If that last token was EOL, then increment line number counter.
  if (nextToken == EOL) {
    currentLine++;
  }
  nextToken = nextToken2;
  tokenValue = tokenValue2;
  if (nextToken2 != EOF) {
    // Note that "currentLine" applies to "nextToken", not to "nextToken2".
    //   If the last token was on the previous line, the token we are
    //   about to get will be on the next line. Momentarily adjust currentLine
    //   so any lexical error will be reported on the correct line.
    if (nextToken == EOL)
      currentLine++;
    nextToken2 = GetToken();
    if (nextToken == EOL)
      currentLine--;
  }
}

// ********************************************************************************
//
// GetToken ()
//
// Scan the next token and return it.  Side-effects tokenValue2.
// Returns EOF repeatedly after end-of-file is reached.
//
int GetToken(void) {
  int ch, ch2, ch3, containsDot, lengthError, numDigits;

  char lexError2[] = "Illegal UTF-8 sequence encountered within a string (bad "
                     "byte = 0xxxxxxxxxxxxxxxxx)";
  char stringBuffer[MAX_STR_LEN + 1]; // buffer for saving strings and IDs
  int next;                           // index into stringBuffer

  while (1) {
    ch = getc(inputFile);

    // Process enf-of-file...
    if (ch == EOF) {
      ungetc(ch, inputFile);
      return EOF;

      // Process newline...
    } else if (ch == '\n') {
      // If the next character happens to be \r, then get and ignore it...
      ch2 = getc(inputFile);
      if (ch2 != '\r') {
        ungetc(ch2, inputFile);
      }
      return EOL;

      // Process CR...
    } else if (ch == '\r') {
      // If the next character happens to be \n, then get and ignore it...
      ch2 = getc(inputFile);
      if (ch2 != '\n') {
        ungetc(ch2, inputFile);
      }
      return EOL;

      // Process other white space...
    } else if (ch == ' ' || ch == '\t') {
      // do nothing

      // Process strings...
    } else if (ch == '"') {
      next = 0;
      lengthError = 0;
      resetUTF8Buffer();
      while (1) {
        ch2 = getc(inputFile);
        if (addUTF8Char(ch2, 0) <= 0) {
          sprintf(lexError2,
                  "Illegal UTF-8 sequence encountered within a string (bad "
                  "byte = 0x%02x)",
                  ch2 & 0x000000ff);
          PrintError(lexError2);
          uniBufferLength = 0;
          uniExpectedSize = 0;
        }
        if (ch2 == '"') {
          break;
        } else if (ch2 == '\n') {
          PrintError("End-of-line (NL) encountered within a string");
          ungetc(ch2, inputFile);
          break;
        } else if (ch2 == '\r') {
          PrintError("End-of-line (CR) encountered within a string");
          ungetc(ch2, inputFile);
          break;
        } else if (ch2 == EOF) {
          PrintError("EOF encountered within a string");
          ungetc(ch2, inputFile);
          break;
        } else if ((ch2 < 32) || (ch2 == 127)) {
          sprintf(lexError2, "Illegal character 0x%02x in string ignored", ch2);
          PrintError(lexError2);
        } else {
          if (ch2 == '\\') {
            ch2 = ScanEscape();
          }
          if (next >= MAX_STR_LEN) {
            lengthError = 1;
          } else {
            stringBuffer[next++] = ch2;
          }
        }
      }
      if (uniBufferLength > 0) {
        PrintError("Illegal UTF-8 sequence encountered within a string (at end "
                   "of string)");
      }
      tokenValue2.tableEntry = LookupAndAdd2(stringBuffer, next, ID);
      if (lengthError) {
        PrintError("Maximum string length exceeded");
      }
      return STRING;

      // Process identifiers...
    } else if (IsAlpha(ch) || (ch == '.') || (ch == '_')) {
      lengthError = 0;
      if (ch == '.') {
        containsDot = 1;
      } else {
        containsDot = 0;
      }
      next = 0;
      while (IsAlpha(ch) || IsDigit(ch) || (ch == '.') || (ch == '_')) {
        if (ch == '.') {
          containsDot = 1;
        }
        if (next >= MAX_STR_LEN) {
          lengthError = 1;
        } else {
          stringBuffer[next++] = ch;
        }
        ch = getc(inputFile);
      }
      ungetc(ch, inputFile);
      tokenValue2.tableEntry = LookupAndAdd2(stringBuffer, next, ID);
      // If already there, then its type may be BYTE, ..., ADD, ..., or ID
      if (containsDot && (tokenValue2.tableEntry->type == ID)) {
        PrintError("Unexpected period within identifier");
      }
      if (lengthError) {
        PrintError("Maximum string length exceeded");
      }
      return tokenValue2.tableEntry->type;

      // Process character constants...
    } else if (ch == '\'') {
      ch2 = getc(inputFile);
      if (ch2 == '\\') {
        ch2 = ScanEscape();
      } else if ((ch2 < 32) || (ch2 >= 127)) {
        PrintError("Character within character constant is not a valid escape "
                   "or printable ASCII character");
        while ((ch2 < 32) || (ch2 >= 127)) {
          // printf ("Bad character = 0x%02x\n", ch2 & 0x00ff);
          ch2 = getc(inputFile);
        }
        ungetc(ch2, inputFile);
        ch2 = 0;
      }
      tokenValue2.ivalue = ch2;
      ch2 = getc(inputFile);
      if (ch2 != '\'') {
        ungetc(ch2, inputFile);
        PrintError("Expecting closing quote in character constant");
      }
      return INTEGER;

      // Process integers...
    } else if (IsDigit(ch)) {

      // See if we have 0x...
      if (ch == '0') {
        ch2 = getc(inputFile);
        if (ch2 == 'x') {
          numDigits = 0;
          ch = getc(inputFile);
          if (hexCharToInt(ch) < 0) {
            PrintError("Must have a hex digit after 0x");
          }
          int32_t intVal32 = 0;
          while (hexCharToInt(ch) >= 0) {
            intVal32 = (intVal32 << 4) + hexCharToInt(ch);
            numDigits++;
            ch = getc(inputFile);
            // Pick up any "_" after this digit and ignore
            while (ch == '_') {
              ch = getc(inputFile);
              if ((hexCharToInt(ch) < 0) && (ch != '_')) {
                PrintError("Hex constants may not end with \'_\'");
                intVal32 = 0;
              }
            }
          }
          ungetc(ch, inputFile);
          if (numDigits > 8) {
            PrintError("Hex constants must be 8 or fewer digits");
            intVal32 = 0;
          }
          tokenValue2.ivalue = (int32_t)intVal32;
          return INTEGER;
        }
        ungetc(ch2, inputFile);
      }

      // Otherwise we have a string of decimal numerals.
      int intOverflow = 0;
      int64_t intVal = 0LL;
      while (IsDigit(ch)) {
        intVal = intVal * 10LL + (ch - '0');
        if (intVal > 4294967295LL) { // Note 0xFFFF_FFFF = 4,294,967,295
          intOverflow = 1;
        }
        ch = getc(inputFile);
      }

      ungetc(ch, inputFile);
      if (intOverflow) {
        PrintError("Integer out of range (-2,147,483,648...4,294,967,295)");
        intVal = 0LL;
      }
      tokenValue2.ivalue = intVal; // Grab lower 32 bits
      return INTEGER;

      // Check for <u, <=u, <=, <<<, <<<, <
    } else if (ch == '<') {
      ch2 = getc(inputFile);
      if (ch2 == 'u') {
        return LTU;
      } else if (ch2 == '=') {
        ch2 = getc(inputFile);
        if (ch2 == 'u') {
          return LEU;
        } else {
          ungetc(ch2, inputFile);
          return LE;
        }
      } else if (ch2 == '<') {
        ch2 = getc(inputFile);
        if (ch2 == '<') {
          return LTLTLT;
        } else {
          ungetc(ch2, inputFile);
          return LTLT;
        }
      } else {
        ungetc(ch2, inputFile);
        return LT;
      }

      // Check for >u, >=u, >=, >>>, >>>, >
    } else if (ch == '>') {
      ch2 = getc(inputFile);
      if (ch2 == 'u') {
        return GTU;
      } else if (ch2 == '=') {
        ch2 = getc(inputFile);
        if (ch2 == 'u') {
          return GEU;
        } else {
          ungetc(ch2, inputFile);
          return GE;
        }
      } else if (ch2 == '>') {
        ch2 = getc(inputFile);
        if (ch2 == '>') {
          return GTGTGT;
        } else {
          ungetc(ch2, inputFile);
          return GTGT;
        }
      } else {
        ungetc(ch2, inputFile);
        return GT;
      }

      // Check for = and ==
    } else if (ch == '=') {
      ch2 = getc(inputFile);
      if (ch2 == '=') {
        return EQUALEQUAL;
      } else {
        ungetc(ch2, inputFile);
        return EQUAL;
      }

      // Check for ! and !=
    } else if (ch == '!') {
      ch2 = getc(inputFile);
      if (ch2 == '=') {
        return NOTEQUAL;
      } else {
        ungetc(ch2, inputFile);
        return BANG;
      }

      // Check for one character symbols.
    } else if (ch == '+') {
      return PLUS;
    } else if (ch == '-') {
      return MINUS;
    } else if (ch == ',') {
      return COMMA;
    } else if (ch == ':') {
      return COLON;
    } else if (ch == '*') {
      return STAR;
    } else if (ch == '%') {
      return PERCENT;
    } else if (ch == '&') {
      return AMPERSAND;
    } else if (ch == '|') {
      return BAR;
    } else if (ch == '^') {
      return CARET;
    } else if (ch == '(') {
      return LPAREN;
    } else if (ch == ')') {
      return RPAREN;
    } else if (ch == '[') {
      return LBRACKET;
    } else if (ch == ']') {
      return RBRACKET;

    } else if (ch == '/') {
      ch2 = getc(inputFile);

      // **********  Scan // comments  **********
      if (ch2 == '/') {
        resetUTF8Buffer();
        while (1) {
          ch = getc(inputFile);
          if (addUTF8Char(ch, 0) <= 0) {
            PrintError(
                "Illegal UTF-8 sequence encountered within a // comment");
          }
          if (ch == EOF) {
            PrintError("End-of-file encountered within a // comment");
            ungetc(ch, inputFile);
            return EOF;
          } else if (ch == '\n') {
            // If the next character happens to be \r, then get and ignore it...
            ch3 = getc(inputFile);
            if (ch3 != '\r') {
              ungetc(ch3, inputFile);
            }
            return EOL;
          } else if (ch == '\r') {
            // If the next character happens to be \n, then get and ignore it...
            ch3 = getc(inputFile);
            if (ch3 != '\n') {
              ungetc(ch3, inputFile);
            }
            return EOL;
          }
        } // endwhile
        continue;

        // ********** Scan /* comments  **********

      } else if ((ch == '/') && (ch2 == '*')) {
        ch2 = ' ';
        int i = 1;
        resetUTF8Buffer();
        while (1) {
          ch = ch2;
          ch2 = getc(inputFile);
          if (addUTF8Char(ch2, 0) <= 0) {
            PrintError(
                "Illegal UTF-8 sequence encountered within a /* comment");
          }
          if (ch2 == EOF) {
            PrintError("End-of-file encountered within a /* comment");
            ungetc(ch2, inputFile);
            return EOF;
          } else if (ch2 == '\n') {
            // If the next character happens to be \r, then get and ignore it...
            ch3 = getc(inputFile);
            if (ch3 != '\r') {
              ungetc(ch3, inputFile);
            }
            currentLine++;
          } else if (ch2 == '\r') {
            // If the next character happens to be \n, then get and ignore it...
            ch3 = getc(inputFile);
            if (ch3 != '\n') {
              ungetc(ch3, inputFile);
            }
            currentLine++;
          }
          if (ch == '/' && ch2 == '*') {
            i++;
            ch = ' ';
            ch2 = ' ';
          }
          if (ch == '*' && ch2 == '/') {
            if (i <= 1) {
              break;
            } else {
              i--;
              ch = ' ';
              ch2 = ' ';
            }
          }
        }
        continue;

      } else {
        ungetc(ch2, inputFile);
        return SLASH;
      }

      // Otherwise, we have an invalid character; ignore it.
    } else {
      if ((ch >= ' ') && (ch <= '~')) {
        sprintf(lexError2, "Illegal character '%c' ignored", ch);
      } else {
        sprintf(lexError2, "Illegal character 0x%02x ignored", ch);
      }
      PrintError(lexError2);
    }
  }
}

// ********************************************************************************
//
// IsAlpha (char)  -->  int
//
// This function is passed a character and returns TRUE iff it is
//  'a' .. 'z'
//  'A' .. 'Z'
//
int IsAlpha(char ch) {
  if (('a' <= ch) && (ch <= 'z')) {
    return 1;
  } else if (('A' <= ch) && (ch <= 'Z')) {
    return 1;
  }
  return 0;
}

// ********************************************************************************
//
// IsDigit (char)  -->  int
//
// This function is passed a character and returns TRUE iff it is
//  '0' .. '9'
//
int IsDigit(char ch) {
  if (('0' <= ch) && (ch <= '9')) {
    return 1;
  }
  return 0;
}

// ********************************************************************************
//
// ScanEscape ()
//
// This function is called after we have gotten a back-slash.  It
// reads whatever characters follow and returns the character.  If
// problems arise it prints a message and returns '?'.  If EOF is
// encountered, it prints a message, calls ungetc (EOF, inputFile)
// and returns '?'.
//
int ScanEscape() {
  int ch, ch2, i, j;
  ch2 = getc(inputFile);
  if (ch2 == '\n') {
    PrintError("End-of-line (NL) encountered after a \\ escape");
    ungetc(ch2, inputFile);
    return '?';
  }
  if (ch2 == '\r') {
    PrintError("End-of-line (CR) encountered after a \\ escape");
    ungetc(ch2, inputFile);
    return '?';
  }
  if (ch2 == EOF) {
    PrintError("End-of-file encountered after a \\ escape");
    ungetc(ch2, inputFile);
    return '?';
  }
  i = IsEscape(ch2);
  if (i != -1) {
    return i;
  } else if (ch2 == 'x') {
    ch = getc(inputFile); // Get 1st hex digit
    if (ch == '\n') {
      ungetc(ch, inputFile);
      PrintError("End-of-line (NL) encountered after a \\x escape");
      return '?';
    }
    if (ch == '\r') {
      ungetc(ch, inputFile);
      PrintError("End-of-line (CR) encountered after a \\x escape");
      return '?';
    }
    if (ch == EOF) {
      PrintError("End-of-file encountered after a \\x escape");
      ungetc(ch, inputFile);
      return '?';
    }
    i = hexCharToInt(ch);
    if (i < 0) {
      PrintError("Must have a hex digit after \\x");
      return '?';
    } else {
      ch2 = getc(inputFile);
      if (ch2 == '\n') {
        ungetc(ch2, inputFile);
        PrintError("End-of-line (NL) encountered after a \\x escape");
        return '?';
      }
      if (ch2 == '\r') {
        ungetc(ch2, inputFile);
        PrintError("End-of-line (CR) encountered after a \\x escape");
        return '?';
      }
      if (ch2 == EOF) {
        PrintError("End-of-file encountered after a \\x escape");
        ungetc(ch2, inputFile);
        return '?';
      }
      j = hexCharToInt(ch2);
      if (j < 0) {
        PrintError("Must have two hex digits after \\x");
        return '?';
      }
      return (i << 4) + j;
    }
  } else {
    PrintError("Illegal escape (only \\0, \\a, \\b, \\t, \\n, \\v, \\f, \\r, "
               "\\e, \\\", \\\', \\\\, and \\xHH allowed)");
    return '?';
  }
}

// ********************************************************************************
//
// IsEscapeChar (char)  -->  ASCII Value
//
// This function is passed a char, such as 'n'.  If this char is one
// of the escape characters, (e.g., \n), then this function returns the
// ASCII value (e.g., 10).  Otherwise, it returns -1.
//
int IsEscape(char ch) {
  if (ch == '0') {
    return 0;
  } else if (ch == 'a') {
    return '\a';
  } else if (ch == 'b') {
    return '\b';
  } else if (ch == 't') {
    return '\t';
  } else if (ch == 'n') {
    return '\n';
  } else if (ch == 'v') {
    return '\v';
  } else if (ch == 'f') {
    return '\f';
  } else if (ch == 'r') {
    return '\r';
  } else if (ch == 'e') {
    return 0x1b;
  } else if (ch == 'd') {
    return 0x7f;
  } else if (ch == '\"') {
    return '\"';
  } else if (ch == '\'') {
    return '\'';
  } else if (ch == '\\') {
    return '\\';
  } else {
    return -1;
  }
}

// ********************************************************************************
//
// LookupAndAdd (givenStr, newType)
//
// This function is passed a pointer to a string of characters, terminated
// by '\0'.  It looks it up in the table.  If there is already an entry in
// the table, it returns a pointer to the previously stored entry.
// If not found, it allocates a new table entry, copies the new string into
// the table, initializes it's type, and returns a pointer to the new
// table entry.
//
TableEntry *LookupAndAdd(char *givenStr, int newType) {
  return LookupAndAdd2(givenStr, strlen(givenStr), newType);
}

// ********************************************************************************
//
// LookupAndAdd2 (givenStr, length, newType)
//
// This function is passed a pointer to a sequence of characters (possibly
// containing \0, of length "length".  It looks this string up in the
// symbol table.  If there is already an entry in the table, it returns
// a pointer to the previously stored entry, with no error.
//
// If not found, it allocates a new table entry, copies the new string
// into the table, initializes it's type, and returns a pointer to the
// new table entry.
//
TableEntry *LookupAndAdd2(char *givenStr, int length, int newType) {
  char *p, *q;
  int i;
  TableEntry *entryPtr;

  unsigned hashVal = ComputeHash(givenStr, length) % SYMBOL_TABLE_HASH_SIZE;

  // Search the linked list and return if we find it.
  for (entryPtr = symbolTableIndex[hashVal]; entryPtr;
       entryPtr = entryPtr->next) {
    if ((length == entryPtr->stringLength) &&
        (BytesEqual(entryPtr->stringChars, givenStr, length))) {
      return entryPtr;
    }
  }

  // Create an entry and initialize it.
  entryPtr = (TableEntry *)calloc(1, sizeof(TableEntry) + length + 1);
  if (entryPtr == 0) {
    FatalError("Memory allocation failed for calloc; too many identifiers and "
               "strings");
  }
  for (p = givenStr, q = entryPtr->stringChars, i = length; i > 0;
       p++, q++, i--) {
    *q = *p;
  }
  entryPtr->stringLength = length;
  entryPtr->type = newType;
  entryPtr->value = 0;
  entryPtr->resolved = 0;
  entryPtr->lineNumber = 0;

  // Add the new entry to the appropriate linked list.
  entryPtr->next = symbolTableIndex[hashVal];
  symbolTableIndex[hashVal] = entryPtr;

  // Check and print a message if the table is filling up.
  if (countOfSymbols++ ==
      SYMBOL_COUNT_LIMIT) { // Use == to get only one message
    fprintf(stderr, "*****  WARNING: The symbol table has grown quite full. "
                    "The performance      *****\n"
                    "*****    of this tool may be suffering as a result. "
                    "Consider rebuilding     *****\n"
                    "*****    this tool with an increased value for the "
                    "constant named           *****\n"
                    "*****    SYMBOL_TABLE_HASH_SIZE. This warning cannot be "
                    "suppressed with -w. *****\n");
    warningsDetected++;
  }

  return entryPtr;
}

// ********************************************************************************
//
// PrintSymbolTable ()
//
// This function runs through the symbol table and prints each entry.
//
void PrintSymbolTable() {
  int hashVal, needHeader;
  TableEntry *entryPtr;

  // Print all symbols that have been given values...

  needHeader = 1;
  for (hashVal = 0; hashVal < SYMBOL_TABLE_HASH_SIZE; hashVal++) {
    for (entryPtr = symbolTableIndex[hashVal]; entryPtr;
         entryPtr = entryPtr->next) {
      if ((entryPtr->type == ID) && (entryPtr->resolved)) {
        if (needHeader) {
          needHeader = 0;
          fprintf(outputFile, "\n----------  Symbols:  ----------\n");
          fprintf(outputFile, "     Line     Value       Decimal  Symbol\n");
          fprintf(
              outputFile,
              "    ======  ========  ===========  "
              "============================================================\n");
          //           999999999   88888888  99999999999  xxxxx
        }
        if (entryPtr->type == ID) {
          fprintf(outputFile, " %9d", entryPtr->lineNumber);
          fprintf(outputFile, "  %08x", entryPtr->value);
          fprintf(outputFile, "  %11d  ", entryPtr->value);
          DumpStringInWidth(entryPtr, 60);
          fprintf(outputFile, "\n");
        }
      }
    }
  }

  // Print all symbols that are undefined (i.e., string data)...

  needHeader = 1;
  for (hashVal = 0; hashVal < SYMBOL_TABLE_HASH_SIZE; hashVal++) {
    for (entryPtr = symbolTableIndex[hashVal]; entryPtr;
         entryPtr = entryPtr->next) {
      // If the program contains strings that happen to match keywords,
      // e.g., "goto" or "call", these will not be printed.
      if ((entryPtr->type == ID) && (!entryPtr->resolved)) {
        if (needHeader) {
          needHeader = 0;
          fprintf(outputFile, "\n----------  String Data:  ----------\n");
          fprintf(outputFile, "     Line   String\n");
          fprintf(
              outputFile,
              "    ======  "
              "============================================================\n");
        }
        if (entryPtr->type == ID) {
          fprintf(outputFile, "%9d   ", entryPtr->lineNumber);
          DumpStringInWidth(entryPtr, 60);
          fprintf(outputFile, "\n");
        }
      }
    }
  }

  fprintf(outputFile, "\n");
}

// ********************************************************************************
//
// PrintString (tableEntryPtr)
//
// This function is passed a pointer to a TableEntry.  It prints the
// string, translating non-printable characters into escape sequences.
//
//     \0   \a   \b  \t   \n   \v   \f   \r   \e   \"   \'   \\   \xHH
//
void PrintString(TableEntry *tableEntryPtr) {
#define PRINT_BUFFER_SIZE 100
  int bufPtr, strPtr;
  char printBuffer[PRINT_BUFFER_SIZE];
  int c;
  strPtr = 0;
  while (1) {
    // If all characters printed, then exit.
    if (strPtr >= tableEntryPtr->stringLength)
      return;
    // Fill up printBuffer
    bufPtr = 0;
    while ((bufPtr < PRINT_BUFFER_SIZE - 4) &&
           (strPtr < tableEntryPtr->stringLength)) { // 4=room for \x12
      c = tableEntryPtr->stringChars[strPtr++];
      if (c == '\0') {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = '0';
      } else if (c == '\a') {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = 'a';
      } else if (c == '\b') {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = 'b';
      } else if (c == '\t') {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = 't';
      } else if (c == '\n') {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = 'n';
      } else if (c == '\v') {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = 'v';
      } else if (c == '\f') {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = 'f';
      } else if (c == '\r') {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = 'r';
      } else if (c == 0x1b) {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = 'e';
      } else if (c == 0x7f) {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = 'd';
      } else if (c == '\\') {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = '\\';
      } else if (c == '\"') {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = '\"';
      } else if (c == '\'') {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = '\'';
      } else if ((c >= 32) && (c < 127)) {
        printBuffer[bufPtr++] = c;
      } else {
        printBuffer[bufPtr++] = '\\';
        printBuffer[bufPtr++] = 'x';
        printBuffer[bufPtr++] = intToHexChar((c >> 4) & 0x0000000f);
        printBuffer[bufPtr++] = intToHexChar(c & 0x0000000f);
      }
    }
    // Add \0 to buffer
    printBuffer[bufPtr++] = '\0';
    // Print buffer
    fprintf(outputFile, "%s", printBuffer);
  }
}

// ********************************************************************************
//
// AddInstrToList (instr, instrSize)
//
// This function is passed a pointer to an Instruction; it adds it
// to the end of the instruction list.
//
void AddInstrToList(Instruction *instr, int instrSize) {
  if (instrList == NULL) {
    instrList = instr;
    lastInstr = instr;
  } else {
    lastInstr->next = instr;
    lastInstr = instr;
  }
  instr->myLC = currentLC;
  instr->actualSize = instrSize;
  currentLC += instrSize;
}

// ********************************************************************************
//
// PrintInstructionList ()
//
// This function prints the list of Instructions.
// This function is invoked by the "-d" command line option.
//
void PrintInstructionList() {
  Instruction *p = instrList;
  fprintf(outputFile, "---------------  INSTRUCTIONS  ---------------\n");
  fprintf(outputFile, "  line\topcode\t  Address  Size(dec) "
                      "\tInstruction\ttableEntry  \texpression\n");
  fprintf(outputFile, "  ====\t======\t=========  ========= "
                      "\t===========\t============\t==========\n");
  while (p != NULL) {
    fprintf(outputFile, "%6d\t%s", p->lineNum, StringForCode(p->op));
    fprintf(outputFile, "\t%8x ", p->myLC);
    fprintf(outputFile, " %10d", p->actualSize);
    if (p->actualSize == 1) {
      fprintf(outputFile, "\t%02x          ", 0x000000ff & p->byte1);
    } else if (p->actualSize == 2) {
      fprintf(outputFile, "\t%02x %02x       ", 0x000000ff & p->byte1,
              0x000000ff & p->byte2);
    } else if (p->actualSize == 3) {
      fprintf(outputFile, "\t%02x %02x %02x    ", 0x000000ff & p->byte1,
              0x000000ff & p->byte2, 0x000000ff & p->byte3);
    } else {
      fprintf(outputFile, "\t\t");
    }
    if (p->myLabel != NULL) {
      fprintf(outputFile, "\t");
      DumpStringInWidth(p->myLabel, 15);
    } else {
      fprintf(outputFile, "\t               ");
    }
    fprintf(outputFile, "\t");
    if (p->expr != NULL) {
      PrintExpr(p->expr);
      if (p->expr->op != INTEGER) {
        fprintf(outputFile, " --> %d\t0x%08x", p->expr->value, p->expr->value);
      }
    }
    if (p->op == SKIP) {
      fprintf(outputFile, "\t   Number of bytes = %d", p->actualSize);
    }
    fprintf(outputFile, "\n");
    p = p->next;
  }
}

// ********************************************************************************
//
// PrintInstructionSummary (message, instrPtr)
//
// Used in debugging. Print message followed by the instruction opcode and line number.
// For example:
//    Here is the instruction: ADD (line number: 123)
//
void PrintInstructionSummary(char *message, Instruction *instrPtr) {
  fprintf(outputFile, "%s%s  (line number : %d)\n", message,
          StringForCode(instrPtr->op), instrPtr->lineNum);
}

// ********************************************************************************
//
// PrintExpr (p)
//
// This function is passed a pointer to an expression.  It prints it,
// calling itself recursively as necessary.
//
void PrintExpr(Expression *p) {
  if (p == NULL)
    return;
  switch (p->op) {
  case INTEGER:
    fprintf(outputFile, "%d", p->value);
    return;
  case ID:
    PrintString(p->tableEntry);
    return;
  case STAR:
    fprintf(outputFile, "(");
    PrintExpr(p->expr1);
    fprintf(outputFile, " * ");
    PrintExpr(p->expr2);
    fprintf(outputFile, ")");
    return;
  case SLASH:
    fprintf(outputFile, "(");
    PrintExpr(p->expr1);
    fprintf(outputFile, " / ");
    PrintExpr(p->expr2);
    fprintf(outputFile, ")");
    return;
  case PERCENT:
    fprintf(outputFile, "(");
    PrintExpr(p->expr1);
    fprintf(outputFile, " %% ");
    PrintExpr(p->expr2);
    fprintf(outputFile, ")");
    return;
  case LTLT:
    fprintf(outputFile, "(");
    PrintExpr(p->expr1);
    fprintf(outputFile, " << ");
    PrintExpr(p->expr2);
    fprintf(outputFile, ")");
    return;
  case LTLTLT:
    fprintf(outputFile, "(");
    PrintExpr(p->expr1);
    fprintf(outputFile, " <<< ");
    PrintExpr(p->expr2);
    fprintf(outputFile, ")");
    return;
  case GTGT:
    fprintf(outputFile, "(");
    PrintExpr(p->expr1);
    fprintf(outputFile, " >> ");
    PrintExpr(p->expr2);
    fprintf(outputFile, ")");
    return;
  case GTGTGT:
    fprintf(outputFile, "(");
    PrintExpr(p->expr1);
    fprintf(outputFile, " >>> ");
    PrintExpr(p->expr2);
    fprintf(outputFile, ")");
    return;
  case AMPERSAND:
    fprintf(outputFile, "(");
    PrintExpr(p->expr1);
    fprintf(outputFile, " & ");
    PrintExpr(p->expr2);
    fprintf(outputFile, ")");
    return;
  case CARET:
    fprintf(outputFile, "(");
    PrintExpr(p->expr1);
    fprintf(outputFile, " ^ ");
    PrintExpr(p->expr2);
    fprintf(outputFile, ")");
    return;
  case BAR:
    fprintf(outputFile, "(");
    PrintExpr(p->expr1);
    fprintf(outputFile, " | ");
    PrintExpr(p->expr2);
    fprintf(outputFile, ")");
    return;
  case BANG:
    fprintf(outputFile, "(! ");
    PrintExpr(p->expr1);
    fprintf(outputFile, ")");
    return;
  case PLUS:
    fprintf(outputFile, "(");
    if (p->expr2 != NULL) {
      PrintExpr(p->expr1);
      fprintf(outputFile, " + ");
      PrintExpr(p->expr2);
    } else {
      fprintf(outputFile, "+ ");
      PrintExpr(p->expr1);
    }
    fprintf(outputFile, ")");
    return;
  case MINUS:
    fprintf(outputFile, "(");
    if (p->expr2 != NULL) {
      PrintExpr(p->expr1);
      fprintf(outputFile, " - ");
      PrintExpr(p->expr2);
    } else {
      fprintf(outputFile, "- ");
      PrintExpr(p->expr1);
    }
    fprintf(outputFile, ")");
    return;
  default:
    fprintf(outputFile, "***** INVALID OP WITHIN Expression *****");
    break;
  }
}

// ********************************************************************************
//
// DumpSymbol (message, p)
//
// This function is passed a pointer to a TableEntry.  It prints the
// message, followed by the TableEntry in great detail, followed by NL.
//
void DumpSymbol(char *message, TableEntry *p) {
  fprintf(outputFile, "%s", message);
  if (p == NULL) {
    fprintf(outputFile, " << DumpSymbol was passed a NULL pointer >>\n");
    return;
  }

  fprintf(outputFile, "SYMBOL ");

  DumpStringInWidth(p, 15);

  fprintf(outputFile, " type=%s", StringForCode(p->type));
  fprintf(outputFile, "value=%d (0x%x)", p->value, p->value);
  fprintf(outputFile, "\n");
}

// ********************************************************************************
//
// DumpStringInWidth (tableEntry, width)
//
// Print "xxx" (including the quotes) in exactly the width given,
// truncating or padding with spaces as necessary.
//
// Non-printable characters are printed using one of these forms:
//   \0   \a   \b  \t   \n   \v   \f   \r   \e   \"   \'   \\   \xHH
//
void DumpStringInWidth(TableEntry *tableEntry, int width) {
  char *p = tableEntry->stringChars;
  int len = tableEntry->stringLength;

  width -= 2;
  if (width < 0)
    ProgramLogicError("DumpStringInWidth - width too small");

  fprintf(outputFile, "\"");

  while (width > 0) {
    if (len <= 0)
      break;
    char c = *(p++);
    if ((c >= ' ') && (c <= '~') && (c != '\"') && (c != '\'') && (c != '\\')) {
      fprintf(outputFile, "%c", c);
    } else if ((width >= 2) &&
               ((c == '\0') || (c == '\a') || (c == '\b') || (c == '\t') ||
                (c == '\n') || (c == '\v') || (c == '\f') || (c == '\r') ||
                (c == 0x1b) || (c == '\"') || (c == '\'') || (c == '\\'))) {
      switch (c) {
      case '\0':
        fprintf(outputFile, "\\0");
        break;
      case '\a':
        fprintf(outputFile, "\\a");
        break;
      case '\b':
        fprintf(outputFile, "\\b");
        break;
      case '\t':
        fprintf(outputFile, "\\t");
        break;
      case '\n':
        fprintf(outputFile, "\\n");
        break;
      case '\v':
        fprintf(outputFile, "\\v");
        break;
      case '\f':
        fprintf(outputFile, "\\f");
        break;
      case '\r':
        fprintf(outputFile, "\\r");
        break;
      case 0x1b:
        fprintf(outputFile, "\\e");
        break;
      case '\"':
        fprintf(outputFile, "\\\"");
        break;
      case '\'':
        fprintf(outputFile, "\\\'");
        break;
      case '\\':
        fprintf(outputFile, "\\\\");
        break;
      }
      width--;
    } else if (width >= 4) {
      fprintf(outputFile, "\\x%02x", c & 0x000000ff);
      width -= 3;
    } else {
      fprintf(outputFile, "?");
    }
    width--;
    len--;
  }
  fprintf(outputFile, "\"");
  while (width > 0) {
    fprintf(outputFile, " ");
    width--;
  }
}

// ********************************************************************************
//
// DumpExpr (message, p)
//
// This function is passed a pointer to an expression.  It prints the
// message, followed by the expression in great detail, followed by NL.
//
void DumpExpr(char *message, Expression *p) {
  fprintf(outputFile, "%s", message);
  if (p == NULL) {
    fprintf(outputFile, " << DumpExpr was passed a NULL pointer >>\n");
    return;
  }

  // fprintf (outputFile, "EXPR [0x%08x ]   op: %s", (int) p, StringForCode (p->op));
  fprintf(outputFile, "EXPR   op: %s", StringForCode(p->op));
  if (p->expr1 == NULL) {
    fprintf(outputFile, "   expr1=NULL");
  } else {
    // fprintf (outputFile, "   expr1=[0x%08x ]", (int) p->expr1);
  }
  if (p->expr2 == NULL) {
    fprintf(outputFile, "   expr2=NULL");
  } else {
    // fprintf (outputFile, "   expr2=[0x%08x ]", (int) p->expr2);
  }
  if (p->tableEntry == NULL) {
    fprintf(outputFile, "  tableEntry=NULL");
  } else {
    fprintf(outputFile, "   tableEntry=%s", p->tableEntry->stringChars);
  }
  fprintf(outputFile, "   value=%d (0x%x)", p->value, p->value);

  fprintf(outputFile, "\n");

  if (p->expr1 != NULL) {
    DumpExpr("     expr1 ", p->expr1);
  }
  if (p->expr2 != NULL) {
    DumpExpr("     expr2 ", p->expr2);
  }
}

// ********************************************************************************
//
// NewInstr (opcode) --> ptr to Instruction
//
// This function allocates a new Instruction object and returns
// a pointer to it.  It initializes its op field.
//
Instruction *NewInstr(int opcode) {
  Instruction *p;
  p = (Instruction *)calloc(1, sizeof(Instruction));
  if (p == NULL) {
    FatalError("Memory allocation failed for calloc; too many instructions");
  }
  p->op = opcode;
  p->expr = NULL;
  p->lineNum = currentLine;
  p->byte1 = 0x00;
  p->byte2 = 0x00;
  p->byte3 = 0x00;
  p->next = NULL;
  p->myLC = 0;
  p->actualSize = 4;
  return p;
}

// ********************************************************************************
//
// NewExpression (op)
//
// This function allocates a new Expression object and returns
// a pointer to it.  It initializes its op field.
//
Expression *NewExpression(int op) {
  Expression *p;
  p = (Expression *)calloc(1, sizeof(Expression));
  if (p == NULL) {
    FatalError("Memory allocation failed for calloc; too many expressions");
  }
  p->op = op;
  p->value = 0;
  p->expr1 = NULL;
  p->expr2 = NULL;
  p->tableEntry = NULL;
  p->computed = 0;
  return p;
}

// ********************************************************************************
//
// NegateExpression (expr)
//
// This function wraps the argument with a unary minus operation.
//
Expression *NegateExpression(Expression *sub) {
  Expression *p;
  if (sub == NULL)
    return NULL;
  p = (Expression *)calloc(1, sizeof(Expression));
  if (p == NULL) {
    FatalError("Memory allocation failed for calloc; too many expressions");
  }
  p->op = MINUS;
  p->value = 0;
  p->expr1 = sub;
  p->expr2 = NULL;
  p->tableEntry = NULL;
  p->computed = 0;
  return p;
}

// ********************************************************************************
//
// GetOneInstruction ()
//
// This function picks up an instruction and adds it to the growing list.
// In parses the following syntax:
//
//    [ id ':' ]  [ keyword operands ] EOL
//
// If problems are encountered, it scans forward to the next EOL and
// scans it.  If nothing is present, it will add nothing to the list.
// It leaves nextToken pointing to the token after the EOL.  If nextToken
// is EOF, then it will return immediately.
//
// Throughout this function we maintain currentLC. However, its value is
// not really very accurate.
//
void GetOneInstruction() {
  Instruction *p;
  Expression *ex;
  TableEntry *tableEntry = NULL;
  int firstToken, gotLabel;

  if (nextToken == EOF) {
    return;
  }

  gotLabel = 0;

  // See if this line contains "LABEL :"...
  if (nextToken == ID) {
    tableEntry = tokenValue.tableEntry;
    Scan();
    if (nextToken == COLON) {
      Scan();
      gotLabel = 1;
      if (nextToken != EQU) {

        // Process a LABEL here...
        if (currentSegment < 0) {
          PrintError2("A label  may not appear before any \".flash\" or "
                      "\".ram\" statement: ",
                      tableEntry->stringChars);
        }
        if ((tableEntry->resolved) || (tableEntry->value != 0)) {
          PrintError2("This label was previously defined: ",
                      tableEntry->stringChars);
        }
        tableEntry->resolved = 1;
        tableEntry->value = currentLC;
        tableEntry->lineNumber = currentLine;
        // printf ("    ENCOUNTERED LABEL   Assigning:   LC = 0x%08x    \"%s\"\n",
        //         currentLC, tableEntry->stringChars);
        p = NewInstr(LABEL);
        p->myLabel = tableEntry;
        AddInstrToList(p, 0);
      }
    } else {
      PrintError("Invalid op-code or missing colon after label");
      ScanRestOfLine();
      return;
    }
  }

  // If this line contains nothing, then ignore it and try again.
  if (nextToken == EOL) {
    Scan();
    return;
  }

  // Scan the firstToken...
  firstToken = nextToken;
  p = NewInstr(firstToken); // May need to override "op" later
  Scan();

  // If this is a pseudo-op, then deal with it and return.
  // (After this SWITCH, we'll process instructions.
  switch (firstToken) {

    /********** EACH CASE MUST FILL IN THIS INFO:
             p->op      = opcode;
             p->expr    = NULL;
             p->byte1   = 0x00;
             p->byte2   = 0x00;   // ProcessExpressions fills this in
             p->byte3   = 0x00;   // ProcessExpressions fills this in
             AddInstrToList (p, sizeOfInstruction);
      **********/

  case FLASH:

    if (gotLabel) {
      PrintError("A label is not allowed on \".flash\" statement");
    }
    if (nextToken != EOL) {
      PrintErrorWithToken("Expecting End-Of-Line in \".flash\" statement");
      ScanRestOfLine();
    }
    // Save currentLC from previous segment
    if (currentSegment == FLASH_SEGMENT) {
      flashLC = currentLC;
    } else if (currentSegment == RAM_SEGMENT) {
      ramLC = currentLC;
    }
    currentSegment = FLASH_SEGMENT;
    currentLC = flashLC;
    AddInstrToList(p, 0);
    return;

  case RAM:

    if (gotLabel) {
      PrintError("A label is not allowed on \".ram\" statement");
    }
    if (nextToken != EOL) {
      PrintErrorWithToken("Expecting End-Of-Line in \".ram\" statement");
      ScanRestOfLine();
    }
    // Save currentLC from previous segment
    if (currentSegment == FLASH_SEGMENT) {
      flashLC = currentLC;
    } else if (currentSegment == RAM_SEGMENT) {
      ramLC = currentLC;
    }
    currentSegment = RAM_SEGMENT;
    currentLC = ramLC;
    AddInstrToList(p, 0);
    return;

  case STRINGOP:

    if (NotInAnySegment())
      return;
    if (nextToken != STRING) {
      PrintError("Expecting string after .string");
      ScanRestOfLine();
      return;
    }
    tableEntry = tokenValue.tableEntry;
    if (tableEntry->lineNumber <= 0) {
      tableEntry->lineNumber = currentLine;
    }
    p->myLabel = tableEntry;
    Scan();
    AddInstrToList(p, tableEntry->stringLength);
    if (NextIsNot(EOL, "Unexpected token after string"))
      return;
    return;

  case EQU:

    ex = GetExpr();
    if (ex == NULL)
      return;
    if (!gotLabel) {
      PrintError("A label is required on the same line as the .equ");
    } else {
      p->myLabel = tableEntry;
      tableEntry->lineNumber = currentLine;
      p->expr = ex;
      // Attempt to evaluate the expression and update the symbol table
      // Try to resolve any equates we can. This might help with future ".skip"s
      ResolveOneEquate(p, 0);
      // Add this instruction to the list
      AddInstrToList(p, 0);
    }
    if (NextIsNot(EOL, "Unexpected token after expression"))
      return;
    return;

  case BYTE:
  case HALFWORD:
  case WORD:

    if (NotInAnySegment())
      return;
    ex = zeroExpression;
    if (currentSegment == FLASH_SEGMENT) {
      if (nextToken == EOL) {
        PrintError("An initial value is required for data in the FLASH");
        return;
      }
      ex = GetExpr();
      if (ex == NULL)
        return;
    } else {
      if (nextToken != EOL) {
        PrintWarning("Initial value is ignored for data in the RAM");
        ex = GetExpr();
        if (ex == NULL)
          return;
      }
    }
    p->expr = ex;
    if (NextIsNot(EOL, "Unexpected token after expression"))
      return;
    if (firstToken == BYTE)
      AddInstrToList(p, 1);
    else if (firstToken == HALFWORD)
      AddInstrToList(p, 2);
    else
      AddInstrToList(p, 4);
    return;

  case SKIP:

    if (NotInAnySegment())
      return;
    ex = GetExpr();
    if (ex == NULL)
      return;
    p->expr = ex;
    EvalExpr(
        ex,
        0); // wantErrors = false; they'll get printed in ProcessExpressions.
    int skipSize = 0;
    if (ex->computed) {
      skipSize = ex->value;
    } else {
      skipSize = 1;
      PrintError("Unable to evaluate skip amount. (Does the expr depend on "
                 "something in the file after this?)");
    }
    if (skipSize <= 0) {
      PrintError("In a .skip instruction, the expression is zero or less");
      skipSize = 0;
    } else if (skipSize > 65535) {
      PrintError(
          "In a .skip instruction, the expression exceeds 65535, (0xffff)");
      skipSize = 0;
    }
    AddInstrToList(p, skipSize);
    if (NextIsNot(EOL, "Unexpected token after operands"))
      return;
    return;

  case A:

    if (NextIsNot(EQUAL, "Expecting = in \"A = ...\""))
      return;
    if (nextToken == BANG) {
      Scan();
      if (NextIsNot(A, "Expecting A in \"A = !A\""))
        return;
      if (NextIsNot(EOL, "Unexpected token after \"A = !A\""))
        return;
      p->op = ALU;
      p->byte1 = OP_NOT;
      AddInstrToList(p, 1);
      return;
    } else if ((nextToken == MINUS) && (nextToken2 == A)) {
      Scan();
      p->op = ALU;
      p->byte1 = OP_NEG;
      if (NextIsNot(A, "Expecting A in \"A = -A\""))
        return;
      if (NextIsNot(EOL, "Unexpected token after \"A = -A\""))
        return;
      AddInstrToList(p, 1);
      return;
    } else if (nextToken == IN) {
      Scan();
      p->op = IN;
      p->byte1 = OP_IN;
      p->expr = GetExpr();
      if (p->expr == NULL)
        return;
      if (NextIsNot(EOL, "Unexpected token after \"A = in Expr\""))
        return;
      AddInstrToList(p, 2);
      return;
    } else if ((nextToken == INTEGER) && (tokenValue.ivalue == 0)) {
      Scan();
      p->op = ALU;
      p->byte1 = OP_CLR;
      if (NextIsNot(EOL, "Unexpected token after \"A = 0\""))
        return;
      AddInstrToList(p, 1);
      return;
    } else if (nextToken == LBRACKET) {
      Scan();
      if (nextToken == M) {
        Scan();
        p->op = LDM;
        p->byte1 = OP_LDM;
        if (nextToken == PLUS) {
          Scan();
          p->expr = GetExpr();
          if (p->expr == NULL)
            return;
          if (NextIsNot(RBRACKET, "Expecting ] in \"A = [M+expr]\""))
            return;
          if (NextIsNot(EOL, "Unexpected token after \"A = [M+expr]\""))
            return;
        } else if (nextToken == MINUS) {
          Scan();
          p->expr = NegateExpression(GetExpr());
          if (p->expr == NULL)
            return;
          if (NextIsNot(RBRACKET, "Expecting ] in \"A = [M-expr]\""))
            return;
          if (NextIsNot(EOL, "Unexpected token after \"A = [M+expr]\""))
            return;
        } else {
          p->expr = zeroExpression;
          if (NextIsNot(RBRACKET, "Expecting ] in \"A = [M]\""))
            return;
          if (NextIsNot(EOL, "Unexpected token after \"A = [M]\""))
            return;
        }
        AddInstrToList(p, 3);
        return;
      } else if ((IsReg8(nextToken) >= 0) || (nextToken == XY)) {
        PrintErrorWithToken("The LOAD instruction \"A = [...]\" can use "
                            "register M but not other registers");
        ScanRestOfLine();
        return;
      } else {
        p->op = LD;
        p->byte1 = OP_LD;
        p->expr = GetExpr();
        if (p->expr == NULL)
          return;
        if (NextIsNot(RBRACKET, "Expecting ] in \"A = [...]\""))
          return;
        if (NextIsNot(EOL, "Unexpected token after \"A = [Addr16]\""))
          return;
        AddInstrToList(p, 3);
        return;
      }
    } else if (nextToken == CODE) {
      Scan();
      p->op = LDCODE;
      p->byte1 = OP_LDCODE;
      if (NextIsNot(LBRACKET, "Expecting [ in \"A = code[XY]\""))
        return;
      if (NextIsNot(XY, "Expecting XY in \"A = code[XY]\""))
        return;
      if (NextIsNot(RBRACKET, "Expecting ] in \"A = code[XY]\""))
        return;
      if (NextIsNot(EOL, "Unexpected token after \"A = code[Addr16]\""))
        return;
      AddInstrToList(p, 1);
      return;
    } else if (nextToken == A) {
      Scan();
      p->op = ALU;
      switch (nextToken) {
      case EOL:
        p->op = MOV;
        p->byte1 = OP_MOV; // RegS and RegD default to A
        break;
      case AMPERSAND:
        p->byte1 = OP_AND;
        Scan();
        if (NextIsNot(B, "Expecting B in \"A = A & B\""))
          return;
        break;
      case BAR:
        p->byte1 = OP_OR;
        Scan();
        if (NextIsNot(B, "Expecting B in \"A = A | B\""))
          return;
        break;
      case CARET:
        p->byte1 = OP_XOR;
        Scan();
        if (NextIsNot(B, "Expecting B in \"A = A ^ B\""))
          return;
        break;
      case PLUS:
        Scan();
        if (nextToken == INTEGER) {
          if (NextIsNotOne("Expecting 1 in \"A = A + 1\""))
            return;
          p->byte1 = OP_INCR;
        } else if (nextToken == B) {
          Scan();
          if (nextToken == PLUS) {
            Scan();
            if (NextIsNot(CY, "Expecting CY in \"A = A + B + ...\""))
              return;
            p->byte1 = OP_ADDC;
          } else {
            p->byte1 = OP_ADD;
          }
        } else {
          PrintErrorWithToken("Expecting B or 1 in \"A = A + ...\"");
          ScanRestOfLine();
          return;
        }
        break;
      case MINUS:
        Scan();
        if (nextToken == INTEGER) {
          if (NextIsNotOne("Expecting 1 in \"A = A - 1\""))
            return;
          p->byte1 = OP_DECR;
        } else if (nextToken == B) {
          Scan();
          if (nextToken == MINUS) {
            Scan();
            if (NextIsNot(CY, "Expecting CY in \"A = A - B - ...\""))
              return;
            p->byte1 = OP_SUBC;
          } else {
            p->byte1 = OP_SUB;
          }
        } else {
          PrintErrorWithToken("Expecting B or 1 in \"A = A - ...\"");
          ScanRestOfLine();
          return;
        }
        break;
      case LTLT:
        p->byte1 = OP_SHL;
        Scan();
        if (NextIsNotOne("Expecting 1 in \"A = A << 1\""))
          return;
        break;
      case LTLTLT:
        p->byte1 = OP_SHLC;
        Scan();
        if (NextIsNotOne("Expecting 1 in \"A = A <<< 1\""))
          return;
        break;
      case GTGT:
        p->byte1 = OP_SHR;
        Scan();
        if (NextIsNotOne("Expecting 1 in \"A = A >> 1\""))
          return;
        break;
      case GTGTGT:
        p->byte1 = OP_SHRC;
        Scan();
        if (NextIsNotOne("Expecting 1 in \"A = A >>> 1\""))
          return;
        break;
      default:
        PrintErrorWithToken("Unknown operator in \"A = A op ...\"");
        ScanRestOfLine();
        return;
      }
      if (NextIsNot(EOL, "Unexpected token after \"A = A op XXX\""))
        return;
      AddInstrToList(p, 1);
      return;

    } else if (IsReg8(nextToken) >= 0) { // B,C,D,X,Y,M1,M2 but not A
      p->op = MOV;
      p->byte1 = OP_MOV;
      if (GetRegS(p))
        return;
      if (NextIsNot(EOL, "Unexpected token after \"A = Reg\""))
        return;
      AddInstrToList(p, 1);
      return;
    } else { // Must be Expr8
      p->op = MOVI;
      p->byte1 = OP_MOVI;
      p->expr = GetExpr();
      if (p->expr == NULL)
        return;
      if (NextIsNot(EOL, "Unexpected token after \"A = Expr8\""))
        return;
      AddInstrToList(p, 2);
      return;
    }
    ProgramLogicError("Not reached - A");

  case B:
  case C:
  case D:
  case X:
  case Y:
  case M1:
  case M2:

    if (NextIsNot(EQUAL, "Expecting = in \"Reg = ...\""))
      return;
    if (nextToken == LBRACKET) {
      Scan();
      p->op = LD;
      p->byte1 = OP_LD | IsReg8(firstToken); // RegD is low 3 bits
      if ((IsReg8(nextToken) >= 0) || (nextToken == XY) || (nextToken == M)) {
        PrintErrorWithToken("The LOAD instruction can have this form \"Reg = "
                            "[Reg]\" only when the destination is register A");
        ScanRestOfLine();
        return;
      }
      p->expr = GetExpr();
      if (p->expr == NULL)
        return;
      if (NextIsNot(RBRACKET, "Expecting ] in \"Reg = [...]\""))
        return;
      if (NextIsNot(EOL, "Unexpected token after \"Reg = [Addr16]\""))
        return;
      AddInstrToList(p, 3);
      return;
    } else if (IsReg8(nextToken) >= 0) { // A,B,C,D,X,Y,M1,M2
      p->op = MOV;
      p->byte1 = OP_MOV | IsReg8(firstToken); // RegD is low 3 bits
      if (GetRegS(p))
        return;
      if (NextIsNot(EOL, "Unexpected token after \"RegD = RegS\""))
        return;
      AddInstrToList(p, 1);
      return;
    } else { // Must be Expr8
      p->op = MOVI;
      p->byte1 = OP_MOVI | IsReg8(firstToken); // RegD is low 3 bits
      p->expr = GetExpr();
      if (p->expr == NULL)
        return;
      if (NextIsNot(EOL, "Unexpected token after \"RegD = Expr8\""))
        return;
      AddInstrToList(p, 2);
      return;
    }
    ProgramLogicError("Not reached - B");

  case LBRACKET:

    if (nextToken == M) {
      Scan();
      p->op = STM;
      p->byte1 = OP_STM;
      if (nextToken == PLUS) {
        Scan();
        p->expr = GetExpr();
        if (p->expr == NULL)
          return;
        if (NextIsNot(RBRACKET, "Expecting ] in \"[M+expr] = A\""))
          return;
        if (NextIsNot(EQUAL, "Expecting = in \"[M+expr]=A\""))
          return;
        if (IsReg8(nextToken) >= 1) { // B,C,D,X,Y,M1,M2 but not A
          PrintErrorWithToken(
              "The STORE instruction can only have the form \"[expr]=Reg\" or "
              "\"[M]=A\" or \"[M+/-expr]=A\"");
          ScanRestOfLine();
          return;
        }
        if (NextIsNot(A, "Expecting A in \"[M+expr]=A\""))
          return;
        if (NextIsNot(EOL, "Unexpected token after \"[M+expr] = A\""))
          return;
      } else if (nextToken == MINUS) {
        Scan();
        p->expr = NegateExpression(GetExpr());
        if (p->expr == NULL)
          return;
        if (NextIsNot(RBRACKET, "Expecting ] in \"[M-expr] = A\""))
          return;
        if (NextIsNot(EQUAL, "Expecting = in \"[M-expr]=A\""))
          return;
        if (IsReg8(nextToken) >= 1) { // B,C,D,X,Y,M1,M2 but not A
          PrintErrorWithToken(
              "The STORE instruction can only have the form \"[expr]=Reg\" or "
              "\"[M]=A\" or \"[M+/-expr]=A\"");
          ScanRestOfLine();
          return;
        }
        if (NextIsNot(A, "Expecting A in \"[M-expr]=A\""))
          return;
        if (NextIsNot(EOL, "Unexpected token after \"[M-expr] = A\""))
          return;
      } else {
        p->expr = zeroExpression;
        if (NextIsNot(RBRACKET, "Expecting ] in \"[M] = A\""))
          return;
        if (NextIsNot(EQUAL, "Expecting = in \"[M]=A\""))
          return;
        if (IsReg8(nextToken) >= 1) { // B,C,D,X,Y,M1,M2 but not A
          PrintErrorWithToken(
              "The STORE instruction can only have the form \"[expr]=Reg\" or "
              "\"[M]=A\" or \"[M+/-expr]=A\"");
          ScanRestOfLine();
          return;
        }
        if (NextIsNot(A, "Expecting A in \"[M]=A\""))
          return;
        if (NextIsNot(EOL, "Unexpected token after \"[M] = A\""))
          return;
      }
      AddInstrToList(p, 3);
      return;
    } else if (nextToken == XY) {
      PrintErrorWithToken("The STORE instruction can only have the form "
                          "\"[expr]=Reg\" or \"[M]=A\" or \"[M+/-expr]=A\"");
      ScanRestOfLine();
      return;
    } else {
      p->op = ST;
      p->byte1 = OP_ST;
      p->expr = GetExpr();
      if (p->expr == NULL)
        return;
      if (NextIsNot(RBRACKET, "Expecting ] in \"[expr]=Reg\""))
        return;
      if (NextIsNot(EQUAL, "Expecting = in \"[expr]=Reg\""))
        return;
      if (GetRegD(p))
        return;
      if (NextIsNot(EOL, "Unexpected token after \"[expr]=Reg\""))
        return;
      AddInstrToList(p, 3);
      return;
    }

  case XY:
  case M:

    p->op = ADD16;
    if (firstToken == XY)
      p->byte1 = OP_ADD16 | 0x00; // Set RegD == 0 for XY
    if (firstToken == M)
      p->byte1 = OP_ADD16 | 0x01; // Set RegD == 1 for M
    if (NextIsNot(EQUAL, "Expecting = in \"XY = ...\" or \"M = ...\""))
      return;
    if ((nextToken == XY) || (nextToken == M) || (nextToken == A)) {
      if (nextToken == XY)
        p->byte1 = p->byte1 | 0x00; // Set RegS == 00 for XY
      if (nextToken == M)
        p->byte1 = p->byte1 | 0x02; // Set RegS == 01 for M
      if (nextToken == A)
        p->byte1 = p->byte1 | 0x06; // Set RegS == 11 for A
      Scan();
      if (nextToken == PLUS) {
        Scan();
        p->expr = GetExpr();
        if (p->expr == NULL)
          return;
        if (NextIsNot(EOL, "Unexpected token after \"Reg16 = Reg16 + Expr16\""))
          return;
      } else if (nextToken == MINUS) {
        Scan();
        p->expr = NegateExpression(GetExpr());
        if (p->expr == NULL)
          return;
        if (NextIsNot(EOL, "Unexpected token after \"Reg16 = Reg16 - Expr16\""))
          return;
      } else {
        p->expr = zeroExpression;
        if (NextIsNot(EOL, "Unexpected token after \"Reg16 = Reg16\""))
          return;
      }
      AddInstrToList(p, 3);
      return;
    } else {
      p->byte1 = p->byte1 | 0x04; // Set RegS == 10 for zero
      p->expr = GetExpr();
      if (p->expr == NULL)
        return;
      if (NextIsNot(EOL, "Unexpected token after \"Reg16 = Expr16\""))
        return;
      AddInstrToList(p, 3);
      return;
    }

  case IF:

    p->op = BRANCH;
    p->byte1 = OP_BRANCH;
    switch (nextToken) {
    case EQUALEQUAL:
    case ZE:
      p->byte1 |= COND_EQ;
      break;
    case NOTEQUAL:
      p->byte1 |= COND_NE;
      break;
    case LT:
      p->byte1 |= COND_LT;
      break;
    case LE:
      p->byte1 |= COND_LE;
      break;
    case GT:
      p->byte1 |= COND_GT;
      break;
    case GE:
      p->byte1 |= COND_GE;
      break;
    case CY:
    case LTU:
      p->byte1 |= COND_LTU;
      break;
    case LEU:
      p->byte1 |= COND_LEU;
      break;
    case GTU:
      p->byte1 |= COND_GTU;
      break;
    case GEU:
      p->byte1 |= COND_GEU;
      break;
    case SN:
      p->byte1 |= COND_SN1;
      break;
    case OV:
      p->byte1 |= COND_OV1;
      break;
    case BANG:
      Scan();
      if (nextToken == CY) {
        p->byte1 |= COND_GEU;
      } else if (nextToken == SN) {
        p->byte1 |= COND_SN0;
      } else if (nextToken == ZE) {
        p->byte1 |= COND_NE;
      } else if (nextToken == OV) {
        p->byte1 |= COND_OV0;
      } else {
        PrintErrorWithToken("In \"if COND goto...\", COND must be: == != < <= "
                            "> >= <u <=u >u >=u CY !CY SN !SN ZE !ZE OV !OV");
        ScanRestOfLine();
        return;
      }
      break;
    default:
      PrintErrorWithToken("In \"if COND goto...\", COND must be: == != < <= > "
                          ">= <u <=u >u >=u CY !CY SN !SN ZE !ZE OV !OV");
      ScanRestOfLine();
      return;
    }
    Scan();
    if (NextIsNot(GOTO, "Expecting GOTO in \"if COND goto...\""))
      return;
    p->expr = GetExpr();
    if (p->expr == NULL)
      return;
    if (NextIsNot(EOL, "Unexpected token after \"if COND goto ADDR\""))
      return;
    AddInstrToList(p, 3);
    return;

  case NOP:

    p->byte1 = OP_NOP;
    if (NextIsNot(EOL, "Unexpected token after RET or NOP"))
      return;
    AddInstrToList(p, 1);
    return;

  case CALL:

    p->byte1 = OP_CALL;
    p->expr = GetExpr();
    if (p->expr == NULL)
      return;
    if (NextIsNot(EOL, "Unexpected token after target"))
      return;
    AddInstrToList(p, 3);
    return;

  case RET:

    p->byte1 = OP_RET;
    if (NextIsNot(EOL, "Unexpected token after RET or NOP"))
      return;
    AddInstrToList(p, 1);
    return;

  case CMP:

    p->byte1 = OP_CMP;
    if (NextIsNot(A, "Expecting A in \"cmp A:B\""))
      return;
    if (NextIsNot(COLON, "Expecting : in \"cmp A:B\""))
      return;
    if (NextIsNot(B, "Expecting B in \"cmp A:B\""))
      return;
    if (NextIsNot(EOL, "Unexpected token after \"cmp A:B\""))
      return;
    AddInstrToList(p, 1);
    return;

  case PUSH:

    p->byte1 = OP_PUSH;
    if (GetRegD(p))
      return;
    if (NextIsNot(EOL, "Unexpected token after PUSH"))
      return;
    AddInstrToList(p, 1);
    return;

  case POP:

    p->byte1 = OP_POP;
    if (GetRegD(p))
      return;
    if (NextIsNot(EOL, "Unexpected token after POP"))
      return;
    AddInstrToList(p, 1);
    return;

  case GOTO:

    if (nextToken == XY) {
      Scan();
      p->op = GOTOXY;
      p->byte1 = OP_GOTOXY;
      if (NextIsNot(EOL, "Unexpected token after \"goto XY\""))
        return;
      AddInstrToList(p, 1);
      return;
    } else if (nextToken == M) {
      PrintErrorWithToken(
          "The indirect jump instruction \"goto XY\" only allows register XY");
      ScanRestOfLine();
      return;
    } else {
      p->byte1 = OP_BRANCH | COND_TRUE;
      p->expr = GetExpr();
      if (p->expr == NULL)
        return;
      if (NextIsNot(EOL, "Unexpected token after GOTO ADDR"))
        return;
      AddInstrToList(p, 3);
      return;
    }

  case SP: // SP = Expr16

    p->op = LDSP;
    p->byte1 = OP_LDSP;
    if (NextIsNot(EQUAL, "Expecting = in SP=Expr16"))
      return;
    p->expr = GetExpr();
    if (p->expr == NULL)
      return;
    if (NextIsNot(EOL, "Unexpected token after GOTO"))
      return;
    AddInstrToList(p, 3);
    return;

  case OUT: // out Expr8 = A

    p->byte1 = OP_OUT;
    p->expr = GetExpr();
    if (p->expr == NULL)
      return;
    if (NextIsNot(EQUAL, "Expecting = in \"out Expr8 = A\""))
      return;
    if (NextIsNot(A, "Expecting A in \"out Expr8 = A\""))
      return;
    if (NextIsNot(EOL, "Unexpected token after \"out Expr8 = A\""))
      return;
    AddInstrToList(p, 2);
    return;

  default:
    fprintf(stderr, "Error on line %d: No legal instruction starts with %s\n",
            currentLine, StringForCode(firstToken));
    errorsDetected++;
    ScanRestOfLine();
  }
} // GetOneInstruction

// ********************************************************************************
//
// ScanRestOfLine ()
//
// This function calls scan() repeatedly until we either get an
// EOL or hit EOF.  The tokens are ignored.
//
void ScanRestOfLine() {
  while ((nextToken != EOL) && (nextToken != EOF)) {
    Scan();
  }
  if (nextToken == EOL) {
    Scan();
  }
}

// ********************************************************************************
//
// GetRegD (ptrToInstruction)
//
// This function scans register and modifies byte1 to set the RegD field.
// If ok, it returns FALSE, otherwise is prints a message and returns TRUE.
//
int GetRegD(Instruction *p) {
  int r = IsReg8(nextToken);
  if (r >= 0) {
    Scan();
    p->byte1 |= r;
    return 0;
  } else {
    PrintErrorWithToken("Expecting A,B,C,D,X,Y,M1, or M2");
    ScanRestOfLine();
    return 1;
  }
}

// ********************************************************************************
//
// GetRegS (ptrToInstruction)
//
// This function scans register and modifies byte1 to set the RegS field.
// If ok, it returns FALSE, otherwise is prints a message and returns TRUE.
//
int GetRegS(Instruction *p) {
  int r = IsReg8(nextToken);
  if (r >= 0) {
    Scan();
    p->byte1 |= r << 3;
    return 0;
  } else {
    PrintErrorWithToken("Expecting A,B,C,D,X,Y,M1, or M2");
    ScanRestOfLine();
    return 1;
  }
}

// ********************************************************************************
//
// NextIsNot (tokType, message)
//
// This function checks to make sure the current token matches tokType
// and then scans to the next token.  If everything is ok, it returns
// FALSE.  If there are problems, it prints an error message, calls
// ScanRestOfLine () and returns TRUE.
//
int NextIsNot(int tokType, char *message) {
  if (nextToken == EOF) {
    if (tokType == EOL) {
      PrintError2(
          message,
          "; Unexpected EOF end-of-file; expecting EOL (i.e., CR or LF)");
    } else {
      PrintError2(message, "; Unexpected EOF end-of-file");
    }
    return 1;
  } else if (nextToken == tokType) {
    Scan();
    return 0;
  } else {
    PrintErrorWithToken(message);
    ScanRestOfLine();
    return 1;
  }
}

// ********************************************************************************
//
// NextIsNotOne (message)
//
// This function checks to make sure the current token is "1"
// and then scans to the next token.  If everything is ok, it returns
// FALSE.  If there are problems, it prints an error message, calls
// ScanRestOfLine () and returns TRUE.
//
int NextIsNotOne(char *message) {
  if (nextToken == EOF) {
    PrintError2(message, "; Unexpected EOF end-of-file");
    return 1;
  } else if ((nextToken == INTEGER) && (tokenValue.ivalue == 1)) {
    Scan();
    return 0;
  } else {
    PrintErrorWithToken(message);
    ScanRestOfLine();
    return 1;
  }
}

// ********************************************************************************
//
// GetExpr ()
//
// This function parses an expression and returns a pointer to
// an Expression node.  If there are problems, it will print an
// error message, Scan all remaining tokens on this line, and return NULL.
// In some cases, it may find and return a legal expression, but may
// may fail to Scan the entire expression.  This will happen when there
// is a legal expression following by an error, as in:
//    3 + 4 : 5
//
Expression *GetExpr() {
  Expression *p;
  p = ParseExpr0();
  if (p == NULL) {
    ScanRestOfLine();
    return NULL;
  }
  return p;
}

// ********************************************************************************
//
// ParseExpr0 ()
//
// This function parses according to the following CFG rule.
//     expr0 ::= expr1 { "|" expr1 }
// If successful, it returns a pointer to an Expression, else it returns NULL.
//
Expression *ParseExpr0() {
  Expression *soFar, *another, *new;
  int op;
  soFar = ParseExpr1();
  if (soFar == NULL)
    return NULL;
  while (1) {
    op = nextToken;
    if (nextToken == BAR) {
      Scan();
    } else {
      break;
    }
    another = ParseExpr1();
    if (another == NULL)
      return NULL;
    new = NewExpression(op);
    new->expr1 = soFar;
    new->expr2 = another;
    soFar = new;
  }
  return soFar;
}

// ********************************************************************************
//
// ParseExpr1 ()
//
// This function parses according to the following CFG rule.
//     expr1 ::= expr2 { "^" expr2 }
// If successful, it returns a pointer to an Expression, else it returns NULL.
//
Expression *ParseExpr1() {
  Expression *soFar, *another, *new;
  int op;
  soFar = ParseExpr2();
  if (soFar == NULL)
    return NULL;
  while (1) {
    op = nextToken;
    if (nextToken == CARET) {
      Scan();
    } else {
      break;
    }
    another = ParseExpr2();
    if (another == NULL)
      return NULL;
    new = NewExpression(op);
    new->expr1 = soFar;
    new->expr2 = another;
    soFar = new;
  }
  return soFar;
}

// ********************************************************************************
//
// ParseExpr2 ()
//
// This function parses according to the following CFG rule.
//     expr2 ::= expr3 { "&" expr3 }
// If successful, it returns a pointer to an Expression, else it returns NULL.
//
Expression *ParseExpr2() {
  Expression *soFar, *another, *new;
  int op;
  soFar = ParseExpr3();
  if (soFar == NULL)
    return NULL;
  while (1) {
    op = nextToken;
    if (nextToken == AMPERSAND) {
      Scan();
    } else {
      break;
    }
    another = ParseExpr3();
    if (another == NULL)
      return NULL;
    new = NewExpression(op);
    new->expr1 = soFar;
    new->expr2 = another;
    soFar = new;
  }
  return soFar;
}

// ********************************************************************************
//
// ParseExpr3 ()
//
// This function parses according to the following CFG rule.
//     expr3 ::= expr4 { ("<<" | "<<<" | ">>" | ">>>") expr4 }
// If successful, it returns a pointer to an Expression, else it returns NULL.
//
Expression *ParseExpr3() {
  Expression *soFar, *another, *new;
  int op;
  soFar = ParseExpr4();
  if (soFar == NULL)
    return NULL;
  while (1) {
    op = nextToken;
    if (nextToken == LTLT) {
      Scan();
    } else if (nextToken == LTLTLT) {
      Scan();
    } else if (nextToken == GTGT) {
      Scan();
    } else if (nextToken == GTGTGT) {
      Scan();
    } else {
      break;
    }
    another = ParseExpr4();
    if (another == NULL)
      return NULL;
    new = NewExpression(op);
    new->expr1 = soFar;
    new->expr2 = another;
    soFar = new;
  }
  return soFar;
}

// ********************************************************************************
//
// ParseExpr4 ()
//
// This function parses according to the following CFG rule.
//     expr4 ::= expr5 { ("+" | "-") expr5 }
// If successful, it returns a pointer to an Expression, else it returns NULL.
//
Expression *ParseExpr4() {
  Expression *soFar, *another, *new;
  int op;
  soFar = ParseExpr5();
  if (soFar == NULL)
    return NULL;
  while (1) {
    op = nextToken;
    if (nextToken == PLUS) {
      Scan();
    } else if (nextToken == MINUS) {
      Scan();
    } else {
      break;
    }
    another = ParseExpr5();
    if (another == NULL)
      return NULL;
    new = NewExpression(op);
    new->expr1 = soFar;
    new->expr2 = another;
    soFar = new;
  }
  return soFar;
}

// ********************************************************************************
//
// ParseExpr5 ()
//
// This function parses according to the following CFG rule.
//     expr5 ::= expr6 { ("*" | "/" | "%") expr6 }
// If successful, it returns a pointer to an Expression, else it returns NULL.
//
Expression *ParseExpr5() {
  Expression *soFar, *another, *new;
  int op;
  soFar = ParseExpr6();
  if (soFar == NULL)
    return NULL;
  while (1) {
    op = nextToken;
    if (nextToken == STAR) {
      Scan();
    } else if (nextToken == SLASH) {
      Scan();
    } else if (nextToken == PERCENT) {
      Scan();
    } else {
      return soFar;
    }
    another = ParseExpr6();
    if (another == NULL)
      return NULL;
    new = NewExpression(op);
    new->expr1 = soFar;
    new->expr2 = another;
    soFar = new;
  }
}

// ********************************************************************************
//
// ParseExpr6 ()
//
// This function parses according to the following CFG rule.
//     expr6 ::= "+" expr6 | "-" expr6 | "!" expr6
//               | ID | INTEGER | STRING | "(" expr0 ")"
// If successful, it returns a pointer to an Expression, else it returns NULL.
// If a STRING is present, it must have exactly 4 characters.  These
// four characters will be used as an INTEGER.
//
Expression *ParseExpr6() {
  Expression *new, *another;
  if (nextToken == PLUS) {
    Scan();
    another = ParseExpr6();
    if (another == NULL)
      return NULL;
    new = NewExpression(PLUS);
    new->expr1 = another;
    return new;
  } else if (nextToken == MINUS) {
    Scan();
    another = ParseExpr6();
    if (another == NULL)
      return NULL;
    new = NewExpression(MINUS);
    new->expr1 = another;
    return new;
  } else if (nextToken == BANG) {
    Scan();
    another = ParseExpr6();
    if (another == NULL)
      return NULL;
    new = NewExpression(BANG);
    new->expr1 = another;
    return new;
  } else if (nextToken == ID) {
    new = NewExpression(ID);
    new->tableEntry = tokenValue.tableEntry;
    Scan();
    return new;
  } else if (nextToken == STRING) {
    new = NewExpression(INTEGER);
    new->value =
        SWAP_BYTES_32(*((int32_t *)tokenValue.tableEntry->stringChars));
    new->computed = 1;
    if (tokenValue.tableEntry->stringLength != 4) {
      PrintError("When strings are used in places expecting an integer, the "
                 "string must be exactly 4 chars long");
      return NULL;
    }
    Scan();
    return new;
  } else if (nextToken == INTEGER) {
    new = NewExpression(INTEGER);
    new->value = tokenValue.ivalue;
    new->computed = 1;
    Scan();
    return new;
  } else if (nextToken == LPAREN) {
    Scan();
    new = ParseExpr0();
    if (new == NULL)
      return NULL;
    if (NextIsNot(RPAREN, "Expecting ')' in expression"))
      return NULL;
    return new;
  } else if (nextToken == ID) {
    new = NewExpression(ID);
    new->tableEntry = tokenValue.tableEntry;
    Scan();
    return new;
  } else {
    PrintErrorWithToken("Syntax problems in expression - expecting ID, "
                        "INTEGER, STRING, CHAR, (, +, -, or !");
    return NULL;
  }
}

// ********************************************************************************
//
// NotInAnySegment ()
//
// This function prints an error message
// If we are not in any segment, this function prints a message and
// returns TRUE.  It returns FALSE if all is ok.
//
int NotInAnySegment() {
  if (currentSegment < 0) {
    PrintError("This instruction may not appear before any \".flash\" or "
               "\".ram\" statement");
    ScanRestOfLine();
    return 1;
  } else {
    return 0;
  }
}

// ********************************************************************************
//
// EvalExpr (exprPtr, PrintErrors)
//
// This function is passed a pointer to an Expression.  It walks the tree
// and tries to evaluate the expression.  If successful, it fills in the "value"
// field and sets "computed".  If anything goes wrong it will print
// an error message if PrintErrors is TRUE; if PrintErrors is FALSE it
// will not print error messages.  This function cannot handle a NULL
// argument.  (Such NULL expression pointers can arise from syntax errors.)
// This function assumes that the expression it is passed is well formed.
//
void EvalExpr(Expression *expr, int PrintErrors) {
  //
  // fprintf (outputFile, "EvalExpr called on...");
  // PrintExpr (expr);
  // fprintf (outputFile, "\n");
  //

  // If this expression has already been given a value, we are done.
  if (expr->computed) {
    return;
  }

  switch (expr->op) {

  case ID:

    // If the ID used in this expression has been assigned a value, then use it.
    if (expr->tableEntry->resolved) {
      expr->computed = 1;
      expr->value = expr->tableEntry->value;
    } else {
      if (PrintErrors) {
        PrintError2("Undefined symbol: ", expr->tableEntry->stringChars);
        expr->computed = 1;
        expr->value = 0;
      }
    }
    return;

  case INTEGER:

    if (!expr->computed) {
      ProgramLogicError("Integer in expression not computed");
    }
    return;

  case PLUS:

    EvalExpr(expr->expr1, PrintErrors);

    // if we have unary plus, then do nothing.
    if (expr->expr2 == NULL) {
      expr->value = expr->expr1->value;
      expr->computed = expr->expr1->computed;
      return;
    }

    // Otherwise, we have a binary + operator.
    EvalExpr(expr->expr2, PrintErrors);

    // If both sub-expressions are computed...
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      if (performAdd(expr->expr1->value, expr->expr2->value, &expr->value)) {
        PrintError("Overflow detected for binary addition (absolute+absolute)");
        expr->value = 0;
      }
      expr->computed = 1;
    }
    return;

  case MINUS:

    EvalExpr(expr->expr1, PrintErrors);
    if (expr->expr2 == NULL) { // if unary operator
      if (expr->expr1->value == 0x80000000) {
        if (PrintErrors) {
          PrintError(
              "The unary - operator overflowed (-0x80000000 was attempted)");
          expr->value = 0;
          expr->computed = 1;
        }
        return;
      }
      if (!expr->expr1->computed) {
        expr->value = 0;
        expr->computed = 1;
        return;
      }
      expr->value = -expr->expr1->value;
      expr->computed = 1;
      return;
    }

    // We have a binary - operator.
    EvalExpr(expr->expr2, PrintErrors);

    // printf ("MINUS expr1 = ");
    // PrintExpr (expr->expr1);
    // printf ("      expr2 = ");
    // PrintExpr (expr->expr2);
    // printf ("\n");

    // If both sub-expressions are computed...
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      expr->value = expr->expr1->value - expr->expr2->value;
      if (performSubtract(expr->expr1->value, expr->expr2->value,
                          &expr->value)) {
        PrintError(
            "Overflow detected for binary subtraction (absolute-absolute)");
        expr->value = 0;
      }
      expr->computed = 1;
    }
    return;

  case STAR:

    EvalExpr(expr->expr1, PrintErrors);
    EvalExpr(expr->expr2, PrintErrors);
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      expr->value = expr->expr1->value * expr->expr2->value;
      if (performMultiply(expr->expr1->value, expr->expr2->value,
                          &expr->value)) {
        PrintError("Overflow detected for binary multiplication");
        expr->value = 0;
      }

      expr->computed = 1;
      return;
    }
    return;

  case SLASH:

    // The / and % operators are implemented using the "C" /  and %
    // operators.  When both are positive, the division is straightforward:
    // the fractional part is discarded.  For example, 7 / 2 will be 3.  When
    // one operand is negative the result is system dependent.  With
    // ANSI C, we are only guaranteed that
    //      (a / b) * b + a % b   ==   a
    // will hold.  Because of all this, we simply disallow neg operands.
    EvalExpr(expr->expr1, PrintErrors);
    EvalExpr(expr->expr2, PrintErrors);
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      if ((expr->expr1->value < 0) || (expr->expr2->value <= 0)) {
        if (PrintErrors) {
          PrintError("Operands to / must be positive");
        }
        return;
      }
      expr->value = expr->expr1->value / expr->expr2->value;
      expr->computed = 1;
      return;
    }
    return;

  case PERCENT:

    // The % operator is implemented using the "C" % operator, which
    // is system dependent...  Ugh.  When one operand is negative the
    // value and sign may vary across "C" implementations.
    EvalExpr(expr->expr1, PrintErrors);
    EvalExpr(expr->expr2, PrintErrors);
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      if ((expr->expr1->value < 0) || (expr->expr2->value <= 0)) {
        if (PrintErrors) {
          PrintError("Operands to % must be positive");
        }
        return;
      }
      expr->value = expr->expr1->value % expr->expr2->value;
      expr->computed = 1;
      return;
    }
    return;

  case LTLT: // This is SLL

    EvalExpr(expr->expr1, PrintErrors);
    EvalExpr(expr->expr2, PrintErrors);
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      int32_t i = expr->expr2->value;
      if (i < 0 || i > 31) {
        if (PrintErrors) {
          PrintError("For <<, the shift amount must be within 0..31");
        }
        return;
      }
      expr->value = expr->expr1->value << expr->expr2->value;
      expr->computed = 1;
      return;
    }
    return;

  case GTGT: // This is SRL

    EvalExpr(expr->expr1, PrintErrors);
    EvalExpr(expr->expr2, PrintErrors);
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      int32_t i = expr->expr2->value;
      if (i < 0 || i > 31) {
        if (PrintErrors) {
          PrintError("For >>, the shift amount must be within 0..31");
        }
        return;
      }
      uint32_t a = expr->expr1->value;
      expr->value = a >> i; //With unsigned ints; zeros will be shifted in.
      expr->computed = 1;
      return;
    }
    return;

  case LTLTLT: // This is SLA

    EvalExpr(expr->expr1, PrintErrors);
    EvalExpr(expr->expr2, PrintErrors);
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      int32_t i = expr->expr1->value;
      int32_t j = expr->expr2->value;
      if (j < 0 || j > 31) {
        if (PrintErrors) {
          PrintError("For <<<, the shift amount must be within 0..31");
        }
        return;
      }
      for (; j > 0; j--) {
        int32_t prev_i = i;
        i <<= 1;
        if (((i < 0) && (prev_i >= 0)) || ((i >= 0) && (prev_i < 0))) {
          PrintError("During <<<, significant bits were lost");
          break;
        }
      }
      expr->value = i;
      expr->computed = 1;
      return;
    }
    return;

  case GTGTGT: // This is SRA

    EvalExpr(expr->expr1, PrintErrors);
    EvalExpr(expr->expr2, PrintErrors);
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      int32_t i = expr->expr1->value;
      int32_t j = expr->expr2->value;
      if (j < 0 || j > 31) {
        if (PrintErrors) {
          PrintError("For >>>, the shift amount must be within 0..31");
        }
        return;
      }
      for (; j > 0; j--) {
        if (i >= 0) {
          i = (i >> 1) & 0x7fffffffffffffff; //shift in a 0
        } else {
          i = (i >> 1) | 0x8000000000000000; //shift in a 1
        }
      }
      expr->value = i;
      expr->computed = 1;
      return;
    }
    return;

  case AMPERSAND:

    EvalExpr(expr->expr1, PrintErrors);
    EvalExpr(expr->expr2, PrintErrors);
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      expr->value = expr->expr1->value & expr->expr2->value;
      expr->computed = 1;
      return;
    }
    return;

  case BAR:

    EvalExpr(expr->expr1, PrintErrors);
    EvalExpr(expr->expr2, PrintErrors);
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      expr->value = expr->expr1->value | expr->expr2->value;
      expr->computed = 1;
      return;
    }
    return;

  case CARET:

    EvalExpr(expr->expr1, PrintErrors);
    EvalExpr(expr->expr2, PrintErrors);
    if ((expr->expr1->computed) && (expr->expr2->computed)) {
      expr->value = expr->expr1->value ^ expr->expr2->value;
      expr->computed = 1;
      return;
    }
    return;

  case BANG:

    EvalExpr(expr->expr1, PrintErrors);
    if (expr->expr1->computed) {
      expr->value = ~(expr->expr1->value);
      expr->computed = 1;
      return;
    }
    return;

  default:

    ProgramLogicError("Unknown operator type in expression");
    return;
  }
}

// ********************************************************************************
//
// ProcessAllEquates ()
//
// This function runs through the list of instructions looking for "equates".
// For each one, it attempts to evaluate the expression and assign a value
// to the symbol.  It keeps repeating until it cannot do anything more.
// Then it runs though them one last time, printing error messages.
//
void ProcessAllEquates() {
  Instruction *instrPtr;
  anyEquatesResolved = 1;
  unresolvedEquates = 1;

  while (anyEquatesResolved && unresolvedEquates) {
    // printf ("-------------------------Making a pass...\n");
    unresolvedEquates = 0;
    anyEquatesResolved = 0;
    //Run thru the instruction list
    for (instrPtr = instrList; instrPtr != NULL; instrPtr = instrPtr->next) {
      if (instrPtr->op == EQU) {
        ResolveOneEquate(instrPtr, 0); //Try to resolve any equates we can.
      }
    }
  }

  // printf ("-------------------------Making the final pass...\n");

  //Run through the equates one last time, printing errors.
  for (instrPtr = instrList; instrPtr != NULL; instrPtr = instrPtr->next) {
    if (instrPtr->op == EQU) {
      currentLine = instrPtr->lineNum;
      ResolveOneEquate(instrPtr, 1); // Just for printing errors.
    }
  }
}

// ********************************************************************************
//
// ResolveOneEquate (instrPtr, PrintErrors)
//
// This function is passed a pointer to an EQU instruction.  It attempts
// to evaluate the expression.  If it can be evaluated (i.e., resolved),
// then this function will set "anyEquatesResolved" to true.  If we are
// unable to resolve this equate, it will set "unresolvedEquates" to
// TRUE.  When an equate is resolved, we will update the entry in the
// symbol table.  If "PrintErrors" is true, we will print error messages
// when we have problems, otherwise error messages will be suppressed.
//
void ResolveOneEquate(Instruction *instrPtr, int PrintErrors) {
  Expression *expr;
  TableEntry *tableEntry;
  if (instrPtr->op != EQU)
    return;
  expr = instrPtr->expr;
  currentLine = instrPtr->lineNum;
  tableEntry = instrPtr->myLabel;
  // printf ("PROCESSING THIS EQUATE... ");
  // PrintString (tableEntry);
  // printf ("  .EQU  ");
  // PrintExpr (expr);
  // printf ("\n");
  // DumpSymbol ("       ", tableEntry);
  // DumpExpr   ("       ", expr);
  if (tableEntry->resolved) {
    // printf ("      ... PREVIOUSLY RESOLVED\n");
  } else {
    EvalExpr(expr, PrintErrors);
    if (expr->computed) {
      anyEquatesResolved = 1;
      tableEntry->resolved = 1;
      tableEntry->value = expr->value;
      // printf ("...UPDATING IT; RESULT IS:\n");
      // DumpSymbol ("       ", tableEntry);
      // DumpExpr   ("       ", expr);
    } else {
      // printf ("...EXPRESSION COULD NOT BE EVALUATED\n");
      unresolvedEquates = 1;
    }
  }
}

// ********************************************************************************
//
// ProcessExpressions ()
//
// This function runs through all the instructions and evaluates any
// expressions found in them, printing errors as they are detected.
//
// This pass will be run after we process equates, since the processing
// of equates may assign values to some symbols used in expressions.
//
// This function ignores equates, since they have been previously evaluated
// and doing it again will cause duplicate error messages.
//
// This function will also take the expression value and inject it into
// byte2 and byte3. It also checks to make sure that the value will
// fit into the instruction.
//
// This function does not modify the LC values.
//
// This function will also check zero-filled segments to make sure they contain only zeros.
//
void ProcessExpressions() {
  Instruction *instrPtr;
  int inRamSegment = 0;

  for (instrPtr = instrList; instrPtr != NULL; instrPtr = instrPtr->next) {
    currentLine = instrPtr->lineNum;

    // If this instruction contains an expression and it is not an EQU, then call "EvalExpr"...
    if ((instrPtr->expr != NULL) && (instrPtr->op != EQU)) {
      EvalExpr(instrPtr->expr, 1);
    }

    // Now check for errors related to expressions...
    switch (instrPtr->op) {

    case EQU: // Expressions for .equ instructions were done in ProcessAllEquates()
      break;

    case FLASH:
      inRamSegment = 0;
      break;

    case RAM:
      inRamSegment = 1;
      break;

    case SKIP:
      // We checked the expression in GetOneInstruction.
      break;

    case BYTE:
      if (instrPtr->expr == NULL)
        break;
      if (instrPtr->expr->computed) {
        if ((instrPtr->expr->value < -128) || (instrPtr->expr->value > 255)) {
          PrintError("In a .byte instruction, the value is not representable "
                     "in 8 bits, i.e., not -128..+255");
        }
      } else {
        // This is not an absolute value; the linker will take care of filling it in.
      }
      if (inRamSegment &&
          ((instrPtr->expr->value != 0) || (!instrPtr->expr->computed))) {
        PrintError("The .ram segment may not contain .byte with a non-zero "
                   "initial value");
      }
      break;

    case HALFWORD:
      if (instrPtr->expr == NULL)
        break;
      if (instrPtr->expr->computed) {
        if ((instrPtr->expr->value < -32768) ||
            (instrPtr->expr->value > 65535)) {
          PrintError("In a .halfword instruction, the value is not "
                     "representable in 16 bits, i.e., not -32768..+65535");
        }
      } else {
        // This is not an absolute value; the linker will take care of filling it in.
      }
      if (inRamSegment &&
          ((instrPtr->expr->value != 0) || (!instrPtr->expr->computed))) {
        PrintError("The .ram segment may not contain .halfword with a non-zero "
                   "initial value");
      }
      break;

    case WORD:
      if (instrPtr->expr == NULL)
        break;
      if (instrPtr->expr->computed) {
        // instrPtr->expr->value is 32 bits, so it cannot be out of range.
      }
      if (inRamSegment &&
          ((instrPtr->expr->value != 0) || (!instrPtr->expr->computed))) {
        PrintError("The .ram segment may not contain .word with a non-zero "
                   "initial value");
      }
      break;

    case STRINGOP:
      if (inRamSegment) {
        PrintError("A .string may not appear in the .ram segment");
      }
      break;

    case LABEL:
      break;

      // Instructions that are a single byte...

    case RET:
    case CMP:
    case NOP:
    case PUSH:
    case POP:
    case ALU:
    case MOV:
    case LDCODE:
    case GOTOXY:
      if (inRamSegment) {
        PrintError("Instructions may not appear in the .ram segment");
      }
      break;

      // Instructions that require Expr8...

    case IN:
    case OUT:
    case MOVI:
      if (inRamSegment) {
        PrintError("Instructions may not appear in the .ram segment");
      }
      if (instrPtr->expr == NULL) {
        ProgramLogicError("Expr8 is missing!");
      }
      if ((instrPtr->expr->value < -128) || (instrPtr->expr->value > 255)) {
        PrintError("This value is not representable in 8 bits");
        fprintf(stderr, "                      value = 0x%08x (%d decimal)\n",
                instrPtr->expr->value, instrPtr->expr->value);
      }
      instrPtr->byte2 = (0x000000ff & instrPtr->expr->value);
      break;

      // Instructions that require Expr16...

    case CALL:
    case GOTO:
    case BRANCH:
    case LDSP:
    case LD:
    case LDM:
    case ST:
    case STM:
    case ADD16:
      if (inRamSegment) {
        PrintError("Instructions may not appear in the .ram segment");
      }
      if (instrPtr->expr == NULL) {
        ProgramLogicError("Expr16 is missing!");
      }
      if ((instrPtr->expr->value < -32768) || (instrPtr->expr->value > 65535)) {
        PrintError("This value is not representable in 16 bits");
        fprintf(stderr, "                      value = 0x%08x (%d decimal)\n",
                instrPtr->expr->value, instrPtr->expr->value);
      }
      instrPtr->byte2 = (0x0000ff00 & instrPtr->expr->value) >> 8;
      instrPtr->byte3 = (0x000000ff & instrPtr->expr->value);
      break;

    default:
      fprintf(stderr, "\n********** op = %s **********\n",
              StringForCode(instrPtr->op));
      ProgramLogicError("Unknown opcode (in ProcessExpressions)");
      break;
    }
  }
} // ProcessExpressions

// ********************************************************************************
//
// FinalCheck ()
//
// Run through the instruction list and do some checking. Error checking is already
// complete. If there were errors, we would have aborted previously. So any remaining
// problems are Program Logic Errors.
//
void FinalCheck() {
  Instruction *instrPtr;

  // printf ("FinalCheck called\n");

  // Run through the list of instructions...
  for (instrPtr = instrList; instrPtr != NULL; instrPtr = instrPtr->next) {

    currentLine = instrPtr->lineNum;

    if ((instrPtr->myLC < 0) || (instrPtr->myLC > 0xFFFF)) {
      ProgramLogicError("In FinalCheck, myLC is not a valid address");
    }

    switch (instrPtr->op) {

    case FLASH:
    case RAM:

      if (instrPtr->myLabel != NULL) {
        ProgramLogicError(
            "In FinalCheck (.flash/.ram), tableEntry should be NULL");
      }
      break;

    case LABEL:

      if (instrPtr->myLabel == NULL) {
        ProgramLogicError(
            "In FinalCheck (.label), tableEntry should not be NULL");
      }
      if (!instrPtr->myLabel->resolved) {
        ProgramLogicError(
            "In FinalCheck (.label), a label instruction is not resolved");
      }
      if (instrPtr->expr != NULL) {
        ProgramLogicError("In FinalCheck (.label), a label instruction has a "
                          "non-null expression");
      }
      break;

    case STRINGOP:

      if (instrPtr->myLabel == NULL) {
        ProgramLogicError(
            "In FinalCheck (.string), tableEntry should not be NULL");
      }
      break;

    case BYTE:

      if (instrPtr->myLabel != NULL) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        ProgramLogicError("In FinalCheck (.byte), tableEntry should be NULL");
      }
      if (instrPtr->expr == NULL) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        ProgramLogicError("In FinalCheck (.byte), problems with the expression "
                          "for a .byte instruction");
      } else if (instrPtr->expr->computed != 1) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        ProgramLogicError("In FinalCheck (.byte), value is not computed");
      } else if ((instrPtr->expr->value < -128) ||
                 (instrPtr->expr->value > 255)) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        ProgramLogicError("In FinalCheck (.byte), problems with the value for "
                          "a .byte instruction");
      }
      break;

    case HALFWORD:

      if (instrPtr->myLabel != NULL) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        ProgramLogicError(
            "In FinalCheck (.halfword), tableEntry should be NULL");
      }
      if (instrPtr->expr == NULL) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        ProgramLogicError("In FinalCheck (.halfword), problems with the "
                          "expression for a .halfword instruction");
      } else if (instrPtr->expr->computed != 1) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        ProgramLogicError("In FinalCheck (.halfword), value is not computed");
      } else if ((instrPtr->expr->value < -32768) ||
                 (instrPtr->expr->value > 65535)) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        ProgramLogicError("In FinalCheck (.halfword), problems with the value "
                          "for a .halfword instruction");
      }
      break;

    case WORD:

      if (instrPtr->myLabel != NULL) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        ProgramLogicError("In FinalCheck (.word), tableEntry should be NULL");
      }
      if (instrPtr->expr == NULL) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        ProgramLogicError("In FinalCheck (.word), problems with the expression "
                          "for a .word instruction");
      } else if (instrPtr->expr->computed != 1) {
        ProgramLogicError("In FinalCheck (.word), value is not computed");
      }
      break;

    case EQU:

      if (instrPtr->myLabel == NULL) {
        ProgramLogicError(
            "In FinalCheck (.equ), tableEntry should not be NULL");
      }
      if (instrPtr->expr == NULL) {
        ProgramLogicError("In FinalCheck (.equ), the expr is null");
      }
      break;

    case SKIP:

      if (instrPtr->actualSize < 0) {
        ProgramLogicError("In FinalCheck (.skip), actualSize is < 0");
      }
      if (instrPtr->myLabel != NULL) {
        ProgramLogicError("In FinalCheck (.skip), tableEntry should be NULL");
      }
      break;

    case NOP:
    case ALU:
    case MOV:
    case RET:
    case CMP:
    case PUSH:
    case POP:
    case LDCODE:
    case GOTOXY:

      // Instructions that should have no expression
      if (instrPtr->myLabel != NULL) {
        ProgramLogicError(
            "In FinalCheck (misc instruction), tableEntry should be NULL");
      }
      if (instrPtr->expr != NULL) {
        ProgramLogicError(
            "In FinalCheck (misc instruction), the expr should be null");
      }
      break;

    case IN:
    case OUT:
    case MOVI:
      // Instructions that should have an Expr8
      if (instrPtr->myLabel != NULL) {
        ProgramLogicError(
            "In FinalCheck (misc instruction), tableEntry should be NULL");
      }
      if (instrPtr->expr == NULL) {
        ProgramLogicError(
            "In FinalCheck (misc instruction), the expr should not be null");
      } else if ((!instrPtr->expr->computed) ||
                 (instrPtr->expr->value < -128) ||
                 (instrPtr->expr->value > 255)) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        printf("value = %d\n", instrPtr->expr->value);
        ProgramLogicError(
            "In FinalCheck, problems with the value for an Expr8");
      }
      break;

    case CALL:
    case GOTO:
    case BRANCH:
    case LDSP:
    case LD:
    case LDM:
    case ST:
    case STM:
    case ADD16:
      // Instructions that should have an Expr16
      if (instrPtr->myLabel != NULL) {
        ProgramLogicError(
            "In FinalCheck (misc instruction), tableEntry should not be NULL");
      }
      if (instrPtr->expr == NULL) {
        ProgramLogicError(
            "In FinalCheck (misc instruction), the expr should be null");
      } else if ((!instrPtr->expr->computed) ||
                 (instrPtr->expr->value < -32768) ||
                 (instrPtr->expr->value > 65535)) {
        PrintInstructionSummary("Problem Instruction = ", instrPtr);
        fprintf(stderr, "value = %d\n", instrPtr->expr->value);
        ProgramLogicError(
            "In FinalCheck, problems with the value for an Expr16");
      }
      break;

    default:
      fprintf(stderr, "\n********** op = %s **********\n",
              StringForCode(instrPtr->op));
      ProgramLogicError("Invalid op in FinalCheck");
      break;

    } // end SWITCH
  }   // end FOR loop running thru the instruction list
} // FinalCheck

// ********************************************************************************
//
// printOutput (style)
//
// Run through the instruction list and print the listing in the style required.
//
// The data is not modified.
//
// This is some of the ugliest, most unpleasant code I've written lately.
//
// The following comment uses ____ to indicate the location of TAB characters
// in the output we will produce, which are inserted so that any TABs in the source
// file will be properly aligned.
//
//*****************************  LISTING STYLE  ******************************
//              t       t       t       t       t       t       t
//                        |     1:______// test program
//      0000              |     2:______        .flash
//      0000  12          |     3:______MyLabel:        .byte 0x12
//      0001  1234        |     4:______        .halfword 0x1234
//      0003  12345678    |     5:______        .word 0x12345678
//      0007  c5          |     6:______
//      0008  a0 42       |     7:______        A = 0x42
//      000a  c2 43 21    |     8:______        goto  0x4321
//
//******************************  VERILOG STYLE  ******************************
//              t       t       t       t       t       t       t
//                                           //     1:__// test program
//                                           //     2:__        .flash
//            16'h0000: data_val = 8'h12;    //     3:__MyLabel:        .byte 0x12
//            16'h0001: data_val = 8'h12;    //     4:__        .halfword 0x1234
//            16'h0002: data_val = 8'h34;
//            16'h0003: data_val = 8'h12;    //     5:__        .word 0x12345678
//            16'h0004: data_val = 8'h34;
//            16'h0005: data_val = 8'h56;
//            16'h0006: data_val = 8'h78;
//            16'h0007: data_val = 8'hc5;    //     6:__        ret
//            16'h0008: data_val = 8'ha0;    //     7:__        A = 0x42
//            16'h0009: data_val = 8'h42;
//            16'h000a: data_val = 8'hc2;    //     8:__        goto  0x4321
//            16'h000b: data_val = 8'h43;
//            16'h000c: data_val = 8'h21;
//
//******************************  HEX2 STYLE ******************************
//              t       t       t       t       t       t       t
//                  //     1:___// test program
//                  //     2:___        .flash
//      12          //     3:___MyLabel:        .byte 0x12
//      1234        //     4:___        .halfword 0x1234
//      12345678    //     5:___        .word 0x12345678
//      c5          //     6:___        ret
//      a042        //     7:___        A = 0x42
//      c24321      //     8:___        goto  0x4321
//*****************************************************************************
//
void printOutput(int style) {
  Instruction *instrPtr;
  int wantToPrintSourceLine, i, i0, i1, i2, i3, ch, val;
  int inFlash = 0;
  int32_t lc;
  char *cp;
  char HEX_COMMENT_1[20];
  char HEX_COMMENT_2[20];

  // The comment character for HEX2 style printing is configured here:
  sprintf(HEX_COMMENT_1, "   %s", hexDelim);
  sprintf(HEX_COMMENT_2, "           %s", hexDelim);

  nextLineNumberToPrint = 1; // Global; incremented by PrintOneLine().
  if (inputFile == stdin) {
    ProgramLogicError("infile=stdin was previously checked");
  }
  rewind(inputFile);

  // Run through the list of instructions...
  for (instrPtr = instrList; instrPtr != NULL; instrPtr = instrPtr->next) {

    wantToPrintSourceLine = 1;
    currentLine = instrPtr->lineNum;

    // If there are some unprinted lines prior to this instruction
    //    (such as comment lines), then print them....
    while (nextLineNumberToPrint < currentLine) {
      if (style == LISTING) {
        PrintOneLine("                  |");
      } else if (style == VERILOG) {
        PrintOneLine("                                         //");
      } else {
        PrintOneLine(HEX_COMMENT_2);
      }
    }

    // Deal with this instruction...
    switch (instrPtr->op) {

    case FLASH:
    case RAM:
      if (style == LISTING) {
        PrintAddress(instrPtr->myLC, style);
        fprintf(outputFile, "        ");
      } else if (style == VERILOG) {
        PrintAddress(-1, style);
        fprintf(outputFile, "        ");
      } else {
        fprintf(outputFile, "        ");
      }
      if (instrPtr->op == FLASH)
        inFlash = 1;
      else
        inFlash = 0;
      break;

    case STRINGOP:
      i = instrPtr->myLabel->stringLength;
      lc = instrPtr->myLC;
      // A .string may not appear in the RAM segment, so no need to check.
      if ((style == LISTING) || (style == HEX2)) {
        PrintAddress(lc, style);
        cp = &(instrPtr->myLabel->stringChars[0]);
        if (i <= 0) {
          fprintf(outputFile, "        ");
        } else if (i == 1) {
          i0 = 0x000000ff & *cp++;
          fprintf(outputFile, "%02x      ", i0);
        } else if (i == 2) {
          i0 = 0x000000ff & *cp++;
          i1 = 0x000000ff & *cp++;
          fprintf(outputFile, "%02x%02x    ", i0, i1);
        } else if (i == 3) {
          i0 = 0x000000ff & *cp++;
          i1 = 0x000000ff & *cp++;
          i2 = 0x000000ff & *cp++;
          fprintf(outputFile, "%02x%02x%02x  ", i0, i1, i2);
        } else {
          i0 = 0x000000ff & *cp++;
          i1 = 0x000000ff & *cp++;
          i2 = 0x000000ff & *cp++;
          i3 = 0x000000ff & *cp++;
          fprintf(outputFile, "%02x%02x%02x%02x", i0, i1, i2, i3);
        }
        // Print the source line after the first line
        if (style == LISTING)
          PrintOneLine("    |");
        else if (style == VERILOG)
          PrintOneLine("   //");
        else
          PrintOneLine(HEX_COMMENT_1);
        // If there are more than 4 characters in the string...
        while (i > 4) {
          i = i - 4;
          lc = lc + 4;
          if (style == LISTING)
            fprintf(outputFile, "%04x  ", lc);
          if (i == 1) {
            i0 = 0x000000ff & *cp++;
            fprintf(outputFile, "%02x      ", i0);
          } else if (i == 2) {
            i0 = 0x000000ff & *cp++;
            i1 = 0x000000ff & *cp++;
            fprintf(outputFile, "%02x%02x    ", i0, i1);
          } else if (i == 3) {
            i0 = 0x000000ff & *cp++;
            i1 = 0x000000ff & *cp++;
            i2 = 0x000000ff & *cp++;
            fprintf(outputFile, "%02x%02x%02x  ", i0, i1, i2);
          } else {
            i0 = 0x000000ff & *cp++;
            i1 = 0x000000ff & *cp++;
            i2 = 0x000000ff & *cp++;
            i3 = 0x000000ff & *cp++;
            fprintf(outputFile, "%02x%02x%02x%02x", i0, i1, i2, i3);
          }
          if (style == LISTING)
            fprintf(outputFile, "    |\n");
          else if (style == VERILOG)
            fprintf(outputFile, "   //\n");
          else {
            fprintf(outputFile, "%s", HEX_COMMENT_1);
            fprintf(outputFile, "\n");
          }
        }
      } else if (style == VERILOG) {
        if ((i <= 0) || (!inFlash)) {
          PrintOneLine("                                         //");
        } else {
          if (inFlash) {
            // Print first character with source line...
            PrintAddress(lc, style);
            cp = &(instrPtr->myLabel->stringChars[0]);
            fprintf(outputFile, "%02x;     ", *cp++);
            PrintOneLine("   //");
            // Print remaining characters, one per line...
            while (--i > 0) {
              PrintAddress(++lc, style);
              fprintf(outputFile, "%02x;\n", *cp++);
            }
          }
        }
      } else {
        if ((i <= 0) || (!inFlash)) {
          PrintOneLine(HEX_COMMENT_2);
        } else {
          if (inFlash) {
            // Print first character with source line...
            cp = &(instrPtr->myLabel->stringChars[0]);
            fprintf(outputFile, "%02x      ", *cp++);
            PrintOneLine(HEX_COMMENT_1);
            // Print remaining characters, one per line...
            while (--i > 0) {
              PrintAddress(++lc, style);
              fprintf(outputFile, "%02x\n", *cp++);
            }
          }
        }
      }
      break;

    case BYTE:
      if (style == LISTING) {
        PrintAddress(instrPtr->myLC, style);
        fprintf(outputFile, "%02x      ",
                ((int)instrPtr->expr->value) & 0x000000ff);
      } else if (style == VERILOG) {
        if (inFlash) {
          PrintAddress(instrPtr->myLC, style);
          fprintf(outputFile, "%02x;     ",
                  ((int)instrPtr->expr->value) & 0x000000ff);
        } else {
          PrintOneLine("                                         //");
        }
      } else {
        if (inFlash) {
          fprintf(outputFile, "%02x      ",
                  ((int)instrPtr->expr->value) & 0x000000ff);
        } else {
          PrintOneLine(HEX_COMMENT_2);
        }
      }
      break;

    case HALFWORD:
      val = ((int)instrPtr->expr->value) & 0x0000ffff;
      if (style == LISTING) {
        PrintAddress(instrPtr->myLC, style);
        fprintf(outputFile, "%04x    ", val);
      } else if (style == VERILOG) {
        if (inFlash) {
          PrintAddress(instrPtr->myLC, style);
          fprintf(outputFile, "%02x;     ", val >> 8);
          if (nextLineNumberToPrint == currentLine) {
            PrintOneLine("   //");
          }
          PrintAddress(instrPtr->myLC + 1, style);
          fprintf(outputFile, "%02x;\n", 0x00ff & val);
        } else {
          PrintOneLine("                                         //");
        }
      } else {
        if (inFlash) {
          fprintf(outputFile, "%04x    ", val);
        } else {
          PrintOneLine(HEX_COMMENT_2);
        }
      }
      break;

    case WORD:
      val = instrPtr->expr->value;
      if (style == LISTING) {
        PrintAddress(instrPtr->myLC, style);
        fprintf(outputFile, "%08x", val);
      } else if (style == VERILOG) {
        if (inFlash) {
          PrintAddress(instrPtr->myLC, style);
          fprintf(outputFile, "%02x;     ", 0x00ff & (val >> 24));
          if (nextLineNumberToPrint == currentLine) {
            PrintOneLine("   //");
          }
          PrintAddress(instrPtr->myLC + 1, style);
          fprintf(outputFile, "%02x;\n", 0x00ff & (val >> 16));
          PrintAddress(instrPtr->myLC + 2, style);
          fprintf(outputFile, "%02x;\n", 0x00ff & (val >> 8));
          PrintAddress(instrPtr->myLC + 3, style);
          fprintf(outputFile, "%02x;\n", 0x00ff & (val >> 0));
        } else {
          PrintOneLine("                                         //");
        }
      } else {
        if (inFlash) {
          fprintf(outputFile, "%08x", val);
        } else {
          PrintOneLine(HEX_COMMENT_2);
        }
      }
      break;

    case EQU:
      PrintAddress(-1, style);
      if (style == LISTING) {
        fprintf(outputFile, "%08x", instrPtr->expr->value);
      } else {
        fprintf(outputFile, "        ");
      }
      break;

    case SKIP:
      i = instrPtr->actualSize;
      if (style == LISTING) {
        PrintAddress(instrPtr->myLC, style);
        if (i <= 0) {
          ProgramLogicError("Skip must be >= 0");
        } else if (i == 1) {
          fprintf(outputFile, "00      ");
        } else if (i == 2) {
          fprintf(outputFile, "0000    ");
        } else if (i == 3) {
          fprintf(outputFile, "000000  ");
        } else if (i == 4) {
          fprintf(outputFile, "00000000");
        } else {
          fprintf(outputFile, "00...   ");
        }
      } else if (style == VERILOG) {
        if (!inFlash) {
          PrintOneLine("                                         //");
        } else {
          if (i <= 0) {
            ProgramLogicError("Skip will be > 0");
          }
          // Print the first line...
          PrintAddress(instrPtr->myLC, style);
          PrintOneLine("00;        //");
          // Print remaining lines, if any...
          lc = instrPtr->myLC;
          while (i > 1) {
            i--;
            lc++;
            PrintAddress(lc, style);
            fprintf(outputFile, "00;        //\n");
          }
        }
      } else {
        if (!inFlash) {
          PrintOneLine(HEX_COMMENT_2);
        } else {
          // Print the first line...
          PrintAddress(instrPtr->myLC, style);
          if (i <= 0) {
            ProgramLogicError("Skip will be > 0");
          } else if (i == 1) {
            fprintf(outputFile, "00      ");
          } else if (i == 2) {
            fprintf(outputFile, "0000    ");
          } else if (i == 3) {
            fprintf(outputFile, "000000  ");
          } else {
            fprintf(outputFile, "00000000");
          }
          PrintOneLine(HEX_COMMENT_1);
          // Print remaining lines, if any...
          lc = instrPtr->myLC;
          while (i > 4) {
            i = i - 4;
            lc = lc + 4;
            PrintAddress(lc, style);
            if (i == 1) {
              fprintf(outputFile, "00      ");
            } else if (i == 2) {
              fprintf(outputFile, "0000    ");
            } else if (i == 3) {
              fprintf(outputFile, "000000  ");
            } else {
              fprintf(outputFile, "00000000");
            }
            fprintf(outputFile, "%s", HEX_COMMENT_1);
            fprintf(outputFile, "\n");
          }
        }
      }
      break;

    case LABEL:

      // If the next instruction is on the same line, then ignore this
      //   label instruction. The source line will be printed for the next instruction.
      if ((instrPtr->next != NULL) &&
          (instrPtr->next->lineNum == instrPtr->lineNum)) {
        wantToPrintSourceLine = 0;

        // Otherwise (the label is followed by something on a different line
        //   (or at the end of the file), so print a line.
      } else {
        if (style == LISTING) {
          PrintAddress(instrPtr->myLC, style);
          fprintf(outputFile, "        ");
        } else {
          PrintAddress(-1, style);
          fprintf(outputFile, "        ");
        }
      }
      break;

    default:
      // It must be an instruction...
      PrintAddress(instrPtr->myLC, style);
      if (style == LISTING) {
        if (instrPtr->actualSize == 1) {
          fprintf(outputFile, "%02x      ", 0x00ff & instrPtr->byte1);
        } else if (instrPtr->actualSize == 2) {
          fprintf(outputFile, "%02x %02x   ", 0x00ff & instrPtr->byte1,
                  0x00ff & instrPtr->byte2);
        } else {
          fprintf(outputFile, "%02x %02x %02x", 0x00ff & instrPtr->byte1,
                  0x00ff & instrPtr->byte2, 0x00ff & instrPtr->byte3);
        }
      } else if (style == VERILOG) {
        if (inFlash) {
          if (instrPtr->actualSize == 1) {
            fprintf(outputFile, "%02x;     ", 0x00ff & instrPtr->byte1);
          } else if (instrPtr->actualSize == 2) {
            fprintf(outputFile, "%02x;     ", 0x00ff & instrPtr->byte1);
            if (nextLineNumberToPrint == currentLine) {
              PrintOneLine("   //");
            }
            PrintAddress(instrPtr->myLC + 1, style);
            fprintf(outputFile, "%02x;\n", 0x00ff & instrPtr->byte2);
          } else if (instrPtr->actualSize == 3) {
            fprintf(outputFile, "%02x;     ", 0x00ff & instrPtr->byte1);
            if (nextLineNumberToPrint == currentLine) {
              PrintOneLine("   //");
            }
            PrintAddress(instrPtr->myLC + 1, style);
            fprintf(outputFile, "%02x;\n", 0x00ff & instrPtr->byte2);
            PrintAddress(instrPtr->myLC + 2, style);
            fprintf(outputFile, "%02x;\n", 0x00ff & instrPtr->byte3);
          } else {
            if (instrPtr->actualSize != 0) {
              ProgramLogicError("Size should be 1,2,3");
            }
          }
        } else {
          PrintOneLine("                                         //");
        }
      } else {
        if (instrPtr->actualSize == 1) {
          fprintf(outputFile, "%02x      ", 0x00ff & instrPtr->byte1);
        } else if (instrPtr->actualSize == 2) {
          fprintf(outputFile, "%02x%02x    ", 0x00ff & instrPtr->byte1,
                  0x00ff & instrPtr->byte2);
        } else {
          fprintf(outputFile, "%02x%02x%02x  ", 0x00ff & instrPtr->byte1,
                  0x00ff & instrPtr->byte2, 0x00ff & instrPtr->byte3);
        }
      }
      break;

    } // end SWITCH

    // Print this line, unless we had a label instruction followed
    //    by another instruction, in which case iterate to take care of it.
    if ((wantToPrintSourceLine) && (nextLineNumberToPrint == currentLine)) {
      if (style == LISTING) {
        PrintOneLine("    |");
      } else if (style == VERILOG) {
        PrintOneLine("   //");
      } else {
        PrintOneLine(HEX_COMMENT_1);
      }
    }

  } // end FOR loop running thru the instruction list

  // Print anything remaining in the input file.
  while (1) {
    ch = getc(inputFile);
    if (ch == EOF)
      break;
    ungetc(ch, inputFile);
    if (style == LISTING) {
      PrintOneLine("                  |");
    } else if (style == VERILOG) {
      PrintOneLine("                                         //");
    } else {
      PrintOneLine(HEX_COMMENT_2);
    }
  }

  if (style == LISTING) {
    fprintf(outputFile, "\n");
  } else if (style == VERILOG) {
    fprintf(outputFile, "\n                                         // ");
  } else {
    fprintf(outputFile, "\n%s ", HEX_COMMENT_2);
  }
  fprintf(outputFile, "FLASH Segment Size: %d (0x%x) bytes\n", flashLC,
          flashLC);

  if (style == LISTING) {
  } else if (style == VERILOG) {
    fprintf(outputFile, "                                         // ");
  } else {
    fprintf(outputFile, "%s ", HEX_COMMENT_2);
  }
  fprintf(outputFile, "RAM Segment Size: %d (0x%x) bytes\n", ramLC, ramLC);

} // End printOutput

// ********************************************************************************
//
// PrintAddress (addr, style)
//
// Print the address and everything up to the data. If the address is -1, then
// print blanks instead, because there won't be any data for this line.
//
// LISTING FORMAT:
//        001a  c25555      |     8:              goto  0x5555
//        ^^^^^^
//
// VERILOG FORMAT:
//              16'h001a: data_val = 8'hc2;    //     8:              goto  0x5555
//        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//
// HEX FORMAT:
//        c25555     //   8:              goto  0x5555
//        (Nothing is printed before the data)
//
void PrintAddress(int addr, int style) {
  if (style == LISTING) {
    if (addr >= 0) {
      fprintf(outputFile, "%04x  ", addr);
    } else {
      fprintf(outputFile, "      ");
    }
  } else if (style == VERILOG) {
    if (addr >= 0) {
      fprintf(outputFile, "      16'h%04x: %s = 8'h", addr, varName);
    } else {
      fprintf(outputFile, "                              ");
    }
  } else {
    // There are zero characters before the data with style=HEX
  }
} // End PrintAddress

// ********************************************************************************
//
// PrintOneLine (separator)
//
// This function will read one line from inputFile and copy it to
// the output file, including the terminating newline.
//
void PrintOneLine(char *separator) {
  int ch, ch2;
  fprintf(outputFile, "%s%5d:\t", separator, nextLineNumberToPrint++);
  while (1) {
    ch = getc(inputFile);
    if (ch == EOF) {
      return;
    } else if (ch == '\n') {
      // If the next character happens to be \r, then get and ignore it...
      ch2 = getc(inputFile);
      if (ch2 != '\r') {
        ungetc(ch2, inputFile);
      }
      fprintf(outputFile, "\n");
      return;
    } else if (ch == '\r') {
      // If the next character happens to be \n, then get and ignore it...
      ch2 = getc(inputFile);
      if (ch2 != '\n') {
        ungetc(ch2, inputFile);
      }
      fprintf(outputFile, "\n");
      return;
    } else {
      fprintf(outputFile, "%c", ch);
    }
  }
}

// ********************************************************************************
//
// hexCharToInt (char)  --> int
//
// This function is passed a character. If it is a hex digit, i.e.,
//    0, 1, 2, ... 9, a, b, ... f, A, B, ... F
// then it returns its value (0..15).  Otherwise, it returns -1.
//
int hexCharToInt(char ch) {
  if (('0' <= ch) && (ch <= '9')) {
    return ch - '0';
  } else if (('a' <= ch) && (ch <= 'f')) {
    return ch - 'a' + 10;
  } else if (('A' <= ch) && (ch <= 'F')) {
    return ch - 'A' + 10;
  } else {
    return -1;
  }
}

// ********************************************************************************
//
// intToHexChar (int)
//
// This function is passed an integer 0..15.  It returns a char
// from 0 1 2 3 4 5 6 7 8 9 a b c d e f
//
char intToHexChar(int i) {
  if (i < 10) {
    return '0' + i;
  } else {
    return 'a' + (i - 10);
  }
}

// ********************************************************************************
//
// isAllHex (str)  --> bool
//
// This function is passed a string. It checks to make sure
// it contains only hex digits, i.e.,
//    0, 1, 2, ... 9, a, b, ... f, A, B, ... F
// and returns TRUE or FALSE.
//
int isAllHex(char *str) {
  char ch;
  while ((ch = *str++) != 0) {
    if (('0' <= ch) && (ch <= '9')) {
    } else if (('a' <= ch) && (ch <= 'f')) {
    } else if (('A' <= ch) && (ch <= 'F')) {
    } else {
      return 0;
    }
  }
  return 1;
}

// ********************************************************************************
//
// printHex1Output
//
// Run through the instruction list and print the bytes, one per line.
//
// The data is not modified.
//
void printHex1Output() {
  Instruction *instrPtr;
  int i, val, inFlash;
  char *cp;

  if (inputFile == stdin) {
    ProgramLogicError("infile=stdin was previously checked");
  }
  rewind(inputFile);

  inFlash = 0;

  // Run through the list of instructions...
  for (instrPtr = instrList; instrPtr != NULL; instrPtr = instrPtr->next) {
    switch (instrPtr->op) {

    case FLASH:
      inFlash = 1;
      break;

    case RAM:
      inFlash = 0;
      break;

    case STRINGOP:
      i = instrPtr->myLabel->stringLength;
      // Print first character with source line...
      cp = &(instrPtr->myLabel->stringChars[0]);
      while (i-- > 0) {
        fprintf(outputFile, "%02x\n", *cp++);
      }
      break;

    case BYTE:
      if (!inFlash)
        break;
      fprintf(outputFile, "%02x\n", ((int)instrPtr->expr->value) & 0x00ff);
      break;

    case HALFWORD:
      if (!inFlash)
        break;
      val = instrPtr->expr->value;
      fprintf(outputFile, "%02x\n%02x\n", (val >> 8) & 0x00ff, val & 0x00ff);
      break;

    case WORD:
      if (!inFlash)
        break;
      val = instrPtr->expr->value;
      fprintf(outputFile, "%02x\n%02x\n%02x\n%02x\n", (val >> 24) & 0x00ff,
              (val >> 16) & 0x00ff, (val >> 8) & 0x00ff, (val >> 0) & 0x00ff);
      break;

    case EQU:
      break;

    case SKIP:
      if (!inFlash)
        break;
      i = instrPtr->actualSize;
      if (i <= 0)
        ProgramLogicError("Skip will be > 0");
      while (i-- > 0) {
        fprintf(outputFile, "00\n");
      }
      break;

    case LABEL:
      break;

    default:
      // It must be an instruction...
      fprintf(outputFile, "%02x\n", 0x00ff & instrPtr->byte1);
      if (instrPtr->actualSize > 1)
        fprintf(outputFile, "%02x\n", 0x00ff & instrPtr->byte2);
      if (instrPtr->actualSize > 2)
        fprintf(outputFile, "%02x\n", 0x00ff & instrPtr->byte3);
      break;
    }
  }
} // End printHex1Output

// ********************************************************************************
//
// performAdd (x, y, ansPtr) --> overflow
//
// Perform the addition x+y and store the answer where the pointer says.
// Determine whether the addition of these two numbers causes an overflow.
// Return TRUE if overflow or FALSE if all OK.
//
int performAdd(int32_t x, int32_t y, int32_t *ansPtr) {

  *ansPtr = x + y;

  if (x > 0) {
    // With x>0 and y>0 the answer must be positive...
    if ((y > 0) && (*ansPtr <= 0)) {
      return 1; // Overflow if answer is <= 0
    } else {
      return 0;
    }
  } else {
    // With x<=0 and y<0 the answer must be negative...
    if ((y < 0) && (*ansPtr >= 0)) {
      return 1; // Overflow if answer is >= 0
    } else {
      return 0;
    }
  }
} // end performAdd

// ********************************************************************************
//
// performSubtract (x, y, ansPtr) --> overflow
//
// Perform the subtraction x-y and store the answer where the pointer says.
// Determine whether the subtraction of these two numbers causes an overflow.
// Return TRUE if overflow or FALSE if all OK.
//
int performSubtract(int32_t x, int32_t y, int32_t *ansPtr) {

  *ansPtr = x - y;
  if (x >= 0) {
    // With x>=0 and y<0 the answer must be positive...
    if ((y < 0) && (*ansPtr <= 0)) {
      return 1; // Overflow if answer is <= 0
    } else {
      return 0;
    }
  } else {
    // With x<0 and y>=0 the answer must be negative...
    if ((y >= 0) && (*ansPtr >= 0)) {
      return 1; // Overflow if answer is >= 0
    } else {
      return 0;
    }
  }
} // end performSubtract

// ********************************************************************************
//
// performMultiply (x_in, y_in, ansPtr) --> overflow
//
// Perform the multiplication and store the answer where the pointer says.
// Determine whether the multiplication of these two numbers will cause an overflow.
// Return TRUE if overflow or FALSE if all OK.
//
int performMultiply(int32_t x_in, int32_t y_in, int32_t *ansPtr) {
  int64_t x_64, y_64, result_64;

  // Perform the multiplication.
  *ansPtr = x_in * y_in;

  // If either value is 1 or 0, then there cannot be overflow.
  if ((x_in == 0) || (x_in == 1) || (y_in == 0) || (y_in == 1)) {
    return 0;

    // If either value is the most negative number, there must be overflow.
  } else if ((x_in == 0x80000000) || (y_in == 0x80000000)) {
    return 1;
  }

  // Make both numbers positive. Then multiply to get a 64 bit result.
  if (x_in < 0) {
    x_64 = -x_in;
  } else {
    x_64 = x_in;
  }
  if (y_in < 0) {
    y_64 = -y_in;
  } else {
    y_64 = y_in;
  }
  result_64 = x_64 * y_64;

  //    printf ("\nx     = %016llx\n", x_64);
  //    printf ("y     = %016llx\n", y_64);
  //    printf ("x * y = %016llx\n", result_64);

  // We just multiplied two positive 32-bit numbers. Check that the result is a positive 32-bit number.
  return (result_64 > 0x000000007fffffff);
}

// ********************************************************************************
//
// addUTF8Char (c, wantPrintingForOK) --> int
//
// This functions adds a UTF-8 byte to the buffer.
// It should be called for every non-ASCII character within:
//
// This function returns
//    0 = error
//    1 = no error; character was processed ok
//    2 = a proper 2 byte UTF-8 char was recognized
//    3 = a proper 3 byte UTF-8 char was recognized
//    4 = a proper 4 byte UTF-8 char was recognized
//
// If there was an error, the buffer is flushed. If there was not an error, and
// the buffer now contains a complete UTF-8 char, the buffer is flushed and reset.
//
// If wantPrintingForOK, then it prints info about the character.
//
int addUTF8Char(int c, int wantPrintingForOK) {

  int newChar = c & 0x000000ff;

  // If this is an ASCII character...
  if (newChar <= 0x7f) {
    if (uniBufferLength != 0) {
      uniBufferLength = 0;
      uniExpectedSize = 0;
      return 0;
    }
    return 1;
  }

  // If this is the first byte of a 2-byte UTF-8 encoding, save it...
  if ((newChar & 0x000000e0) == 0x000000c0) {
    if (uniBufferLength != 0) {
      uniBufferLength = 0;
      uniExpectedSize = 0;
      return 0;
    }
    uniBuffer[0] = newChar;
    uniBufferLength = 1;
    uniExpectedSize = 2;
    return 1;
  }

  // If this is the first byte of a 3-byte UTF-8 encoding, save it...
  if ((newChar & 0x000000f0) == 0x000000e0) {
    if (uniBufferLength != 0) {
      uniBufferLength = 0;
      uniExpectedSize = 0;
      return 0;
    }
    uniBuffer[0] = newChar;
    uniBufferLength = 1;
    uniExpectedSize = 3;
    return 1;
  }

  // If this is the first byte of a 4-byte UTF-8 encoding, save it...
  if ((newChar & 0x000000f8) == 0x000000f0) {
    if (uniBufferLength != 0) {
      uniBufferLength = 0;
      uniExpectedSize = 0;
      return 0;
    }
    uniBuffer[0] = newChar;
    uniBufferLength = 1;
    uniExpectedSize = 4;
    return 1;
  }

  // If this is not a legal continuation byte, it is a problem...
  if ((newChar & 0x000000c0) != 0x00000080) {
    uniBufferLength = 0;
    uniExpectedSize = 0;
    return 0;
  }

  // If we are not expecting any more bytes, it is a problem...
  if (uniExpectedSize <= uniBufferLength) {
    uniBufferLength = 0;
    uniExpectedSize = 0;
    return 0;
  }

  // Add this continuation byte to the buffer...
  if (uniExpectedSize > uniBufferLength) {
    uniBuffer[uniBufferLength++] = newChar;
  }

  // If we need more continuation bytes, then return...
  if (uniExpectedSize > uniBufferLength) {
    return 1;
  }

  // At this point we have exactly the number of bytes we expect...
  if (uniExpectedSize != uniBufferLength) {
    FatalError("Program Logic Error - too many UTF-8 chars in buffer");
  }

  int retVal = uniExpectedSize;

  int codePoint = FromUTF8(uniBuffer);

  // If this is an illegal code point...
  if (codePoint < 0) {
    uniBufferLength = 0;
    uniExpectedSize = 0;
    return 0;
  }

  // Print the codepoint...
  if (wantPrintingForOK) {
    uniBuffer[uniBufferLength] = 0;
    printf("UTF-8 character encountered: U+%08X, codepoint %d, glyph = '%s'\n",
           codePoint, codePoint, uniBuffer);
  }

  // Empty buffer and return the size of whatever character we found...
  uniBufferLength = 0;
  uniExpectedSize = 0;
  return retVal;

} // addUTF8Char

// ********************************************************************************
//
// resetUTF8Buffer () --> bool
//
// This function is called on EOF or ASCII char, i.e., after a UTF-8 sequence.
// If there is a partial UTF-8 character in the buffer, it is an error.
// This function returns 0=OK, 1=error.
// After any invocation, the buffer will be emptied.
//
int resetUTF8Buffer() {
  if (uniBufferLength > 0) {
    uniBufferLength = 0;
    uniExpectedSize = 0;
    return 1;
  } else {
    return 0;
  }
} // resetUTF8Buffer

// ********************************************************************************
//
// FromUTF8 (ptr) -> int
//
// This function is passed a pointer to a string of one or more bytes.
// It assumes the bytes are the UTF-8 encoding of a Unicode character.
//
// It returns the codepoint, or -1 if this is not a legal UTF-8 encoding.
//
// The returned value is an unsigned 21-bit integer
//     0x0 ... 0x 1F,FFFF      0 ... 2,097,151
// There is no attempt to determine if the codepoint is defined.
//
int64_t FromUTF8(const char *ptr) {
  int result, byte2 = 0, byte3 = 0, byte4 = 0;
  int byte1 = *ptr++ & 0x000000ff;
  if (byte1 <= 0x7f) {
    return byte1;
  } else if ((byte1 & 0x000000e0) == 0x000000c0) {
    byte2 = *ptr;
    if ((byte2 & 0x000000c0) != 0x00000080)
      return -1;
    result = ((byte1 & 0x1f) << 6) | (byte2 & 0x3f);
    if (result < 0x00000080)
      return -1;
    return result;
  } else if ((byte1 & 0x000000f0) == 0x000000e0) {
    byte2 = *ptr++;
    byte3 = *ptr;
    if ((byte2 & 0x000000c0) != 0x00000080)
      return -1;
    if ((byte3 & 0x000000c0) != 0x00000080)
      return -1;
    result = ((((byte1 & 0x0f) << 6) | (byte2 & 0x3f)) << 6) | (byte3 & 0x3f);
    if (result < 0x00000800)
      return -1;
    return result;
  } else if ((byte1 & 0x000000f8) == 0x000000f0) {
    byte2 = *ptr++;
    byte3 = *ptr++;
    byte4 = *ptr;
    if ((byte2 & 0x000000c0) != 0x00000080)
      return -1;
    if ((byte3 & 0x000000c0) != 0x00000080)
      return -1;
    if ((byte4 & 0x000000c0) != 0x00000080)
      return -1;
    result = ((((((byte1 & 0x0f) << 6) | (byte2 & 0x3f)) << 6) | (byte3 & 0x3f))
              << 6) |
             (byte4 & 0x3f);
    if (result < 0x00010000)
      return -1;
    if (result > 0x0010ffff)
      return -1;
    return result;
  }
  return -1;
}

// ********************************************************************************
//
// ToUTF8 (bufferPtr, codepoint) -> length
//
// This function translates a single Unicode character into its UTF-8 encoding.
// It is passed a pointer to a place in memory where the encoded bytes
// will be placed, as well as the codepoint to be translated.
//
// The codepoint must be an unsigned 21-bit integer in the range
//     0x0 ... 0x 10,FFFF      0 ... 1,114,111
//
// The translation will be 1 to 4 bytes in length. This function will return
// the number of bytes in the UTF-8 encoding. If the codepoint is not valid,
// this function returns 0.
//
int ToUTF8(char *ptr, int codepoint) {
  if (codepoint < 0) {
    return 0;
  } else if (codepoint <= 0x0000007f) {
    *ptr = codepoint;
    return 1;
  } else if (codepoint <= 0x000007ff) {
    *ptr++ = ((codepoint & 0x000007c0) >> 6) | 0x000000c0;
    *ptr = (codepoint & 0x0000003f) | 0x00000080;
    return 2;
  } else if (codepoint <= 0x0000ffff) {
    *ptr++ = ((codepoint & 0x0000f000) >> 12) | 0x000000e0;
    *ptr++ = ((codepoint & 0x00000fc0) >> 6) | 0x00000080;
    *ptr = (codepoint & 0x0000003f) | 0x00000080;
    return 3;
  } else if (codepoint <= 0x0010ffff) {
    *ptr++ = ((codepoint & 0x001c0000) >> 18) | 0x000000f0;
    *ptr++ = ((codepoint & 0x0003f000) >> 12) | 0x00000080;
    *ptr++ = ((codepoint & 0x00000fc0) >> 6) | 0x00000080;
    *ptr = (codepoint & 0x0000003f) | 0x00000080;
    return 4;
  } else {
    return 0;
  }
  return 0x0;
}

// ********************************************************************************
//
// ComputeHash (ptr, length) --> unsigned int
//
// This function is passed a pointer to a sequence of characters
// (possibly containing \0), of length "length".
// It computes a hash value for the string and returns it.
//
// There are two algorithms to consider. Both algorithms seem to perform
// about the same, given my informal testing. This function uses the Java algorithm,
// since it is faster and widely accepted.
//
unsigned ComputeHash(const char *ptr, int length) {
  unsigned hashVal = 0;

  // printf ("Compute Hash length: %d\n", length);

  /********** 

  // Compute the hash value for the givenStr and set hashVal to it.
  //    As far as I can remember, the hash algorithm used here is my own
  //    invention. For typical assembler programs, it seems to work well.
  for ( p = givenStr, i=0;
        i < length;
        p++, i++ ) {
    hashVal = (hashVal << 4) + (*p);
    g = hashVal & 0xf0000000;
    hashVal = hashVal ^ (g >> 24);
  }

**********/

  // Compute the hash value for the givenStr and set hashVal to it.
  //    This algorithm is from java.lang.String.hashCode().
  //    It computes:
  //         s[0]*31^(n-1) + s[1]*31^(n-2) + ... + s[n-1]
  for (; length > 0; length--) {
    // Compute: hashVal = 31 * hashVal + (*ptr++);
    hashVal = (hashVal << 5) - hashVal + (*ptr++); // overflow is ignored
  }

  /**********/

  // printf ("Hashval = 0x%08x\n", hashVal);
  return hashVal;
}

// ********************************************************************************
//
// BytesEqual (p, q, length) --> bool
//
// This function is passed two pointers to blocks of characters, and a
// length.  It compares the two sequences of bytes and returns true iff
// they are both equal.
//
int BytesEqual(const char *p, const char *q, int length) {
  for (; length > 0; length--, p++, q++) {
    if (*p != *q)
      return 0;
  }
  return 1;
}