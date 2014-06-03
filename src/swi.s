# goto system mode
# stores user state to sp
# moves sp to r0
.macro save_user_state
    msr     cpsr_c, #0x9F
    stmfd   sp!, {r0-r12, lr}
    mov     r0, sp
.endm

# save the spsr and lr of the user process to r0
# r0 should be the user sp
# uses r2 as scratch
.macro save_spsr_lr
    mrs     r2, spsr
    stmfd   r0!, {r2, lr}
.endm

    .global swi_call
    .type   swi_call, %function
swi_call:
    swi
    mov     pc, lr

    .global irq_handler
    .type   irq_handler, %function
irq_handler:

    save_user_state

    # go back to irq mode
    mrs     r2, cpsr
    bic     r2, r2, #0x1F
    orr     r2, r2, #0x12
    msr     cpsr_c, r2

    save_spsr_lr

    msr     cpsr_c, #0x93
    ldmfd   sp!, {r1-r12, pc}

    .global swi_handler
    .type   swi_handler, %function
swi_handler:

    save_user_state

    # go to svc mode
    mrs     r2, cpsr
    bic     r2, r2, #0x1F
    orr     r2, r2, #0x13
    msr     cpsr_c, r2

    save_spsr_lr

    # r2 is an output param ptr to write trap frame (r1) to
    # r0 is return value for this call, which gives us the sp
    ldmfd   sp!, {r2}
    str     r1, [r2, #+0]

    # restore kernel state, resume its execution
    ldmfd   sp!, {r2-r12, pc}

	.global	swi_exit
	.type	swi_exit, %function
swi_exit:
    # store kernel state. r2 is an output param ptr to write trap frame to
    # called with (result, sp, &trap_frame)
    stmfd   sp!, {r1-r12, lr}

    # restore spsr and lr of user process
    ldmfd   r0!, {r2, lr}
    msr     spsr, r2

    # go to system mode
    mrs     r2, cpsr
    orr     r2, r2, #0x1F
    msr     cpsr_c, r2

    # restore user state
    mov     sp, r0
    ldmfd   sp!, {r0-r12, lr}

    # go to svc mode, at this point, we don't care about cpsr anymore...
    msr     cpsr_c, #0x93

    # return to user
    movs    pc, lr
