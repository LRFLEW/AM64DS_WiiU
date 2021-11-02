.set base, 0x02292D3C

# C math.h Functions
.set sqrtf, inject_start + (0x02279368 - base)
.set atan2, inject_start + (0x0229121C - base)

# Tail Calls to Coreinit Functions
.set tail_OSFastMutex_Lock, inject_start + (0x0201FFB0 - base)
.set tail_OSFastMutex_Unlock, inject_start + (0x0201FFBC - base)

# Return Addresses
.set ctVM_ret, inject_start + (0x0205095C - base)
.set updateVM_ret, inject_start + (0x0201DA4C - base)
.set update_ret, inject_start + (0x0200CC40 - base)

# IO Register Return Codes
.set readIoReg_bad, inject_start + (0x02053FD0 - base)
.set readIoReg_good, inject_start + (0x020541AC - base)

.text
.global inject_start
inject_start:

.global inject_init_angle
inject_init_angle:
    # Overwritten Instruction
    stw     %r29, 0x2A40(%r6)
    # Zero-Init Buffered Analog State
    stw     %r29, 0x2BD0(%r6)
    stw     %r29, 0x2BD4(%r6)
    # Zero-Init Current Analog State
    stw     %r29, 0x2BD8(%r6)
    stw     %r29, 0x2BDC(%r6)
    # Branch Back to Function
    b       ctVM_ret

.global inject_comp_angle
inject_comp_angle:
    stwu    %r1, -0x40(%r1)
    stfd    %f31, 0x38(%r1)
    stfd    %f30, 0x30(%r1)
    stfd    %f29, 0x28(%r1)
    # Load f30 = leftStick.x
    lfs     %f30, 0x0C(%r30)
    # load f29 = -leftStick.y
    lfs     %f29, 0x10(%r30)
    fneg    %f29, %f29
    # f31 = sqrt(x^2 + y^2)
    fmuls   %f1, %f30, %f30
    fmadds  %f1, %f29, %f29, %f1
    bl      sqrtf
    fmr     %f31, %f1
    # f1 = atan2(x, y)
    fmr     %f1, %f30
    fmr     %f2, %f29
    bl      atan2
    # Load Useful Float Constants
    lis     %r5, 0x4623
    # (float) 0x3F800000 = 1.0f
    lis     %r3, 0x3F80
    # (float) 0x45800000 = 4096.0f (2^12)
    lis     %r4, 0x4580
    # (float) 0x4622F983 = approx. 2^15 / Pi
    subi    %r5, %r5, 0x067D
    # Load Into f3-f5 Via Stack
    stw     %r3, 0x08(%r1)
    stw     %r4, 0x0C(%r1)
    stw     %r5, 0x10(%r1)
    lfs     %f3, 0x08(%r1)
    lfs     %f4, 0x0C(%r1)
    lfs     %f5, 0x10(%r1)
    # Cap Magnitude to 1.0f
    fcmpu   %cr0, %f31, %f3
    ble     %cr0, post_norm
    # Normalize |x, y| = 1.0f
    fdiv    %f30, %f30, %f31
    fdiv    %f29, %f29, %f31
    fmr     %f31, %f3
post_norm:
    # Scale Magnitude by 4096.0f
    fmuls   %f31, %f31, %f4
    # Scale X by 4096.0f
    fmuls   %f30, %f30, %f4
    # Scale Y by 4096.0f
    fmuls   %f29, %f29, %f4
    # Scale Angle by 2^15 / Pi
    fmuls   %f1, %f1, %f5
    # Switch Float Rounding to Nearest for Integer Conversion
    mffs    %f0
    mtfsfi  7, 0x0
    # Convert Values to Integers
    fctiw   %f31, %f31
    fctiw   %f30, %f30
    fctiw   %f29, %f29
    fctiw   %f1, %f1
    # Restore Float Rounding Mode
    mtfsf   0x01, %f0
    # Load Into r3-r5 Via Stack
    stfd    %f31, 0x08(%r1)
    stfd    %f30, 0x10(%r1)
    stfd    %f29, 0x18(%r1)
    stfd    %f1, 0x20(%r1)
    lwz     %r3, 0x0C(%r1)
    lwz     %r4, 0x14(%r1)
    lwz     %r5, 0x1C(%r1)
    lwz     %r6, 0x24(%r1)
    # Store Values in Application Structure
    sth     %r3, 0x6708(%r29)
    sth     %r4, 0x670A(%r29)
    sth     %r5, 0x670C(%r29)
    sth     %r6, 0x670E(%r29)
    # Cleanup
    lfd     %f29, 0x28(%r1)
    lfd     %f30, 0x30(%r1)
    lfd     %f31, 0x38(%r1)
    addi    %r1, %r1, 0x40
    # Overwritten Instruction
    lbz     %r0, 0x4fb4(%r29)
    # Branch Back to Function
    b       updateVM_ret

.global inject_apply_angle
inject_apply_angle:
    stwu    %r1, -0x18(%r1)
    stw     %r30, 0x14(%r1)
    stw     %r29, 0x10(%r1)
    stw     %r28, 0x0C(%r1)
    # Get NTR Structure Pointer
    lwz     %r30, 0x20(%r31)
    # Get Address of Mutex in NTR
    addis   %r30, %r30, 0x0001
    subi    %r30, %r30, 0x2DE0
    # Read Buffered and New States
    lwz     %r29, 0x3EB8(%r31)
    lwz     %r28, 0x3EBC(%r31)
    lwz     %r3, 0x3980(%r30)
    lwz     %r4, 0x3984(%r30)
    # Check if States Differ
    cmpw    %r29, %r3
    bne     do_apply_copy
    cmpw    %r28, %r4
    beq     post_apply_copy
do_apply_copy:
    # Lock Mutex (Reused Mutex from Button Handling)
    or      %r3, %r30, %r30
    bl      tail_OSFastMutex_Lock
    # Write New State to Buffered State
    stw     %r29, 0x3980(%r30)
    stw     %r28, 0x3984(%r30)
    # Unlock Mutex
    or      %r3, %r30, %r30
    bl      tail_OSFastMutex_Unlock
post_apply_copy:
    # Cleanup
    lwz     %r28, 0x0C(%r1)
    lwz     %r29, 0x10(%r1)
    lwz     %r30, 0x14(%r1)
    addi    %r1, %r1, 0x18
    # Overwritten Instruction
    addi    %r3, %r31, 0x20
    # Branch Back to Function
    b       update_ret

.global inject_get_angle
inject_get_angle:
    # Check if it's an Analog Read
    cmplwi  %r4, 0x150
    blt     not_angle
    cmplwi  %r4, 0x158
    bge     not_angle
    # Start Analog Read
    stwu    %r1, -0x10(%r1)
    stw     %r30, 0x0C(%r1)
    stw     %r29, 0x08(%r1)
    # Get Address of Mutex in NTR
    addis   %r30, %r3, 0x0001
    subi    %r30, %r30, 0x2DE0
    # Get Index Into "Register"
    andi.   %r29, %r5, 0x6
    # Copy Buffered to Current when Accessing the First State Value
    bne     post_buffer_copy
    # Lock Mutex (Reused Mutex from Button Handling)
    or      %r3, %r30, %r30
    bl      tail_OSFastMutex_Lock
    # Write Buffered State to Current State
    lwz     %r3, 0x3980(%r30)
    lwz     %r4, 0x3984(%r30)
    stw     %r3, 0x3988(%r30)
    stw     %r4, 0x398C(%r30)
    # Unlock Mutex
    or      %r3, %r30, %r30
    bl      tail_OSFastMutex_Unlock
post_buffer_copy:
    # Read Requested Current State Value
    add     %r4, %r30, %r29
    lhz     %r3, 0x3988(%r4)
    # Cleanup
    lwz     %r29, 0x08(%r1)
    lwz     %r30, 0x0C(%r1)
    addi    %r1, %r1, 0x10
    # Branch to Main Function for Returning
    b       readIoReg_good
not_angle:
    # The Overwritten Instruction(s) are Duplicated
    # Elsewhere, so Jump to That Instead
    b       readIoReg_bad
