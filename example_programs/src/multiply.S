// Multiply
//
// This program contains a function to multiply two 8 bit unsigned numbers.
//
// Harry Porter - 28 May 2024
//
    .flash

// Expected result: 221 * 179 = 39,559 = 0x9A87

    X = 221
    Y = 179
 
// Print arguments (X, Y)

    M = mess1
    call Print
    A = X
    call PrintHex

    M = mess2
    call Print
    A = Y
    call PrintHex

    M = mess3
    call Print
  
// Compute XY = X * Y

    call multiply
    
// Print Result

    A = X
    call PrintHex
    A = Y
    call PrintHex

// End with RETURN (for use with the "miniBoot" program)

    ret
    .byte 0xff    // otherwise

mess1:      .string "\nTesting multiply...\n  \0"
mess2:      .string " * \0"
mess3:      .string " = \0"



//
// multiply -- Compute XY = X * Y (unsigned)
//
// Passed: X, Y
// Result in: XY
// Modifies: A, B, C, D, X, Y
//
multiply:  
    C = Y                   // C is multiplier
    Y = 0                   // XY = XX00
    D = 8                   // LOOP 8 times
mult_Loop:                  // .
    A = Y                   //   Shift XY << 1
    A = A << 1              //   .
    Y = A                   //   .
    A = X                   //   .
    A = A <<< 1             //   .
    X = A                   //   .
    if !CY goto mult_else   //   IF we just shifted a 1 out
    B = C                   //     XY = XY + C
    A = Y                   //     .
    A = A + B               //     .
    Y = A                   //     .
    if !CY goto mult_else   //     .
    A = X                   //     .
    A = A + 1               //     .
    X = A                   //     .
mult_else:                  //   ENDIF    
    A = D                   // ENDLOOP (Decrement and loop if not zero)
    A = A - 1               // .
    D = A                   // .
    if !ZE goto mult_Loop   // .
    ret                     // RETURN
 


//   
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
    if == goto PrReturn
    out 0 = A
    goto PrLoop
PrReturn:
    ret



//
// PrintHex - Print A in hex, e.g., "4C".
//
// Modifies: A, B
//
PrintHex:
    push A                // Save A
    A = A >> 1            // Print left nibble
    A = A >> 1            // .
    A = A >> 1            // .
    A = A >> 1            // .
    call PrintNibble      // .
    pop A                 // Restore A
    B = 0x0f              // Print right nibble
    A = A & B             // .
    call PrintNibble      // .
    ret                   // RETURN



//
// PrintNibble - Print A as 0,1,2,...D,E,F
//               Assumes A is in 0x00...0x0f
//
PrintNibble:
    B = '0'               // Assume A is in 0...9 and convert to '0'...'9'
    A = A + B             // .
    B = '9'               // IF result is above this range it must
    cmp A:B               // .           have been within a...f
    if <= goto PrSkip     // .
    B = 'A' - '0' - 10    //   Fix it & convert to 'A'...'F'
    A = A + B             //   .
PrSkip:                   // ENDIF
    out 0 = A             // Print it
    ret                   // RETURN
    