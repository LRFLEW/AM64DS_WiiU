.set    base, 0x0202B5E4
.set    get_btn, get_analog_start + (0x0202B8F8 - base)

.text
get_analog_start:

get_analog:
    # Load Custom IO Register Address
    mov     r3, #0x04000000
    orr     r3, r3, #0x150
    # Read 8-Byte Value
    ldmia   r3, {r0, r1}
    # Write 8-Byte Value to Controls Structure
    add     r3, r9, #0x08
    stmia   r3, {r0, r1}
    # Write 1 For "Is Touch"
    mov     r0, #1
    strb    r0, [r9, #0x14]
    # Branch to Rest of Function
    b       get_btn
