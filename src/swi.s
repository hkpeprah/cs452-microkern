    .global swi_call
    .type   swi_call, %function
swi_call:
    swi
    mov     pc, lr

    .global swi_handler
    .type   swi_handler, %function
swi_handler:

    # go to system mode
    msr     cpsr_c, #0x9F

    # store user state, move sp to r0
    stmfd   sp!, {r0-r12, lr}
    mov     r0, sp

    # go to svc mode
    mrs     r2, cpsr
    bic     r2, r2, #0x1F
    orr     r2, r2, #0x13
    msr     cpsr_c, r2

    # save the spsr and lr of the user process on top of the user process stack
    mrs     r2, spsr
    stmfd   r0!, {r2, lr}

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

    .global get_cpsr
    .type   get_cpsr, %function
get_cpsr:
    mrs     r0, cpsr
    mov     pc, lr

    .global get_spsr
    .type   get_spsr, %function
get_spsr:
    mrs     r0, spsr
    mov     pc, lr

    .global get_sp
    .type   get_sp, %function
get_sp:
    mov     r0, sp
    mov     pc, lr


    .global push_reg
    .type   push_reg, %function
push_reg:
    stmfd   sp!, {r0-r14}
    mov     r0, sp
    mov     pc, lr
