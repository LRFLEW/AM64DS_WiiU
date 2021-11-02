.set    base, 0x020065A0
.set    inval_cache, any_pat_start + (0x02004960 - base)

.text
any_pat_start:

any_pat:
    # Load Patch Data Address
    adr     r6, any_pat_data
any_pat_loop:
    ldr     r0, [r6], #8
    # If Length == 0, Then Patching is Complete
    cmp     r0, #0
    beq     end_pat
    ldr     r5, [r6, #-4]
    # MSB Indicates Overlay (Conditional) Patch
    blt     begin_pat_overlay

pat_base:
    ldr     r2, [r6], #4
    str     r2, [r5], #4
    subs    r0, r0, #1
    bne     pat_base
    b       any_pat_loop

begin_pat_overlay:
    # Clear MSB
    bic     r0, r0, #0x80000000
pat_overlay:
    # Check if Instruction is the Expected Value
    ldr     r2, [r6], #8
    ldr     r3, [r5], #4
    cmp     r2, r3
    bne     post_pat_overlay
    # Patch Instruction
    ldr     r2, [r6, #-4]
    str     r2, [r5, #-4]
post_pat_overlay:
    subs    r0, r0, #1
    bne     pat_overlay
    b       any_pat_loop

end_pat:
    # Overwritten Instruction
    bic     r3, r1, #0x1F
    # Branch Back to Function
    b       inval_cache

# Magic Number
.ascii  "AMDS"

any_pat_data:
    # Postpend Patch Data
