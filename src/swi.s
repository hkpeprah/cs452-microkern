    .global	swi_call
	.type	swi_call, %function
swi_call:
    # save keep SP, use r0 to push on to stack. also arg0 to kernel handler
    stmfd   sp!, {r2-r12, lr}
    mov     r0, sp
    swi

    .global swi_handler
    .type   swi_handler, %function
swi_handler:

    # save the spsr of the user process on top of the user process stack
    mrs     r2, spsr
    stmfd   r0!, {r2}

    # r2 is an output param ptr to write trap frame (r1) to
    # r0 is return value for this call, which gives us the sp
    ldmfd   sp!, {r2}
    str     r1, [r2, #+0]

    # restore kernel state, resume its execution
    ldmfd   sp!, {r3-r12, pc}

	.global	swi_exit
	.type	swi_exit, %function
swi_exit:
    # store kernel state. r2 is an output param ptr to write trap frame to
    # called with (result, sp, &trap_frame)
    stmfd   sp!, {r2-r12, lr}

    # restore spsr
    ldmfd   r1!, {r2}
    msr     spsr, r2

    # restore user state, r1 is the user task sp
    # this command doesn't actually work... loads into sp_svc instead of sp. bleh
    #    ldmfd   r1, {r1-r12, sp, pc}^

    # go to system mode
    mrs     r2, cpsr
    orr     r2, r2, #0x1F
    msr     cpsr_c, r2

    # restore user state
    mov     sp, r1
    ldmfd   sp!, {r2-r12, lr}

    # go to svc mode
    mrs     r1, cpsr
    bic     r1, r1, #0x1F
    orr     r1, r1, #0x13
    msr     cpsr_c, r1

    # go to user mode. we branch to that first because
    # we don't have the usermode LR at this point
    ldr     lr, =rte
    movs    pc, lr

rte:
    mov     pc, lr

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
