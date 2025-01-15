// Example program
        .flash

Main:
        SP = 0x0000         // Stack begins at highest RAM addr
        XY = msg            // Print greeting
        call Print          //
Loop:
        A = in UART_recv    // Echo 1 byte on UART
        out UART_xmit = A   //
        B = 'q'             // if "q" then goto Main
        cmp A:B             //
        if == goto Main     //
        goto Loop           // else continue echoing

msg:    .string "\nEcho Program\nPlease type...\0"


// Print Function
//    passed: XY = pointer to NUL-terminated string in FLASH
//    uses:   A, B, XY
Print:
        A = code[XY]
        XY = XY + 1
        B = 0
        cmp A:B
        if == goto PrintExit
        out UART_xmit = A
        goto Print
PrintExit:
        ret

UART_xmit:  .equ 0    // I/O register for sending
UART_recv:  .equ 0    // I/O register for receiving