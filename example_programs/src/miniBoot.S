// Mini-Boot Program
//
// Flash this program to the FPGA. Upon RESET, it runs and waits for a
// second "target" program to be specified. The user can enter the target
// program in hex (using copy-paste). This boot program will then execute
// the target program. Upon completion, this boot program will prompt for
// another target program to be entered.
//
// The benefit is that the FPGA need not be rebuilt and re-flashed every time you
// wish to run a new EveCore program. This makes testing EveCore code faster.
//
// Here is the algorithm of this code:
//    Copy self to high memory
//    Jump to copy in high memory
//    LOOP FOREVER
//      Print message
//      Read in bytes from UART, placing them at address 0
//      Call address 0
//    END
//
// NOTE: The target program will be CALLED and, when done, must execute
//       a RETURN instruction.
//
// NOTE: Pushing RESET will no longer work, because this program moved itself
//       to high memory. (A target program has been loaded into address zero
//       overwriting the boot code. Pushing RESET causes the target program
//       to be executed. But the target program should end with a RETURN,
//       which causes a crash since the RESET destroyed the stack.)
//
// NOTE: This program requires a UNIFIED MEMORY (von Neumann) implementation.
//
// Harry Porter - 12 May 2024
//

memSize:            .equ    64*1024     // OK if actually smaller; upper bits ignored
bootSize:           .equ    BootBeyond - BootStart
highAddr:           .equ    memSize - bootSize

    .flash

// Copy this code to high memory.
BootStart:
    SP = highAddr     // Stack begins just below the boot code.
    XY = BootStart
    M = highAddr
    C = bootSize >> 8      // CD = number of bytes to copy
    D = bootSize & 0x00ff
CopyLoop:
    A = code[XY]
    [M] = A
    XY = XY + 1
    M = M + 1

// Decrement CD and goto CopyLoop if not zero
    B = 1
    A = D
    A = A - B
    D = A
    if !CY goto NoBorrow
    A = C
    A = A - B
    C = A
NoBorrow:
    A = D
    B = 0
    cmp A:B
    if !ZE goto CopyLoop
    A = C
    cmp A:B
    if !ZE goto CopyLoop

// Jump to the copied version in high memory.
    goto MainLoop + highAddr
   
// Print message   
MainLoop:
    M = message + highAddr
    call Print + highAddr
    
// Each iteration of this loop reads and stores one byte...
    M = 0
ByteLoop:

// Get MSB nibble
    call getNibble + highAddr      // returns 0x0_; fe=whitespace, ff=other
    B = 0xFE
    cmp A:B
    if == goto ByteLoop + highAddr
    if >u goto DoCall + highAddr
    A = A << 1
    A = A << 1
    A = A << 1
    A = A << 1
    D = A
    
// Get LSB nibble    
    call getNibble + highAddr
    B = 0x0f
    cmp A:B
    if >u goto Error + highAddr

// A = nibble1 || nibble2
    B = D
    A = A | B

// Store this byte
    [M] = A
    M = M + 1
    
// Echo the byte
    call PrintHex + highAddr
    
// LOOP BACK
    goto ByteLoop + highAddr

// If problems, halt
Error:
    M = errMsg + highAddr
    call Print + highAddr
    goto MainLoop + highAddr

// Call the loaded program
DoCall:
    call 0

// Repeat
    goto MainLoop + highAddr

message:    .string "\nBootloader: Enter bytes followed by \".\"\n\0"
errMsg:     .string "\nERROR\n\0"



//
// getNibble - read one hex character and return in A
//
//   Returns 0x00...0x0f
//   Returns 0xfe if ' ', '\n', or '\r'
//   Returns 0xff if anything else
//   Modifies: A, B
getNibble:
    A = in 0
    B = ' '
    cmp A:B
    if == goto RetWhite + highAddr
    B = '\n'
    cmp A:B
    if == goto RetWhite + highAddr
    B = '\r'
    cmp A:B
    if == goto RetWhite + highAddr
    B = '0'
    cmp A:B
    if <u goto RetError + highAddr
    B = '9'
    cmp A:B
    if <=u goto gotDigit + highAddr
    B = 'A'
    cmp A:B
    if <u goto RetError + highAddr
    B = 'F'
    cmp A:B
    if <=u goto gotUpper + highAddr
    B = 'a'
    cmp A:B
    if <u goto RetError + highAddr
    B = 'f'
    cmp A:B
    if >u goto RetError + highAddr
gotLower:
    B = 'a' - 10
    goto finish + highAddr 
gotUpper:
    B = 'A' - 10
finish:
    A = A - B
gotDigit:
    B = 0x0f
    A = A & B
    ret
RetWhite:
    A = 0xfe
    ret
RetError:
    A = 0xff
    ret

    
// Print - Passed ptr to string in M.
//
// Modifies: A, B, M
//
Print:
    B = 0
PrLoop:
    A = [M]
    M = M + 1
    cmp A:B
    if == goto PrReturn + highAddr
    out 0 = A
    goto PrLoop + highAddr
PrReturn:
    ret



// PrintHex - Print A in Hex, e.g., "4C".
//
// Modifies: A
//
PrintHex:
    push B
    push A
    A = A >> 1
    A = A >> 1
    A = A >> 1
    A = A >> 1
    call PrintNibble + highAddr
    pop A
    B = 0x0f
    A = A & B
    call PrintNibble + highAddr
    pop B
    ret
PrintNibble:
    B = '0'
    A = A + B
    B = '9'
    cmp A:B
    if <= goto PrSkip + highAddr
    B = 'A'-'0' - 10
    A = A + B
PrSkip:
    out 0 = A
    ret
    
    
    
BootBeyond: