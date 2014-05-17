    .global	swi_enter
	.type	swi_enter, %function
swi_call:
    # save user-process information on stack, the sp to r0, then trigger interrupt
    # TODO: cpsr?
    mov     r0, sp
    stmfd   r0!, {r4-r12, sp, lr}
    swi

    .global swi_handler
    .type   swi_handler, %function
swi_handler:
    # r2 is an output param ptr to write trap frame (r1) to
    # r0 is return value from call, which gives us the sp
    ldmfd   sp!, {r2}
    str     r1, [r2, #+0]

    # restore kernel state, resume its execution
    ldmfd   sp!, {r4-r12, pc}

	.global	swi_exit
	.type	swi_exit, %function
swi_exit:
    # store kernel state. r2 is an output param ptr to write trap frame to
    # called with (result, sp, &trap_frame)
    stmfd   sp!, {r2, r4-r12, lr}

    # restore user state, r1 is the user task sp
    ldmfd   r1, {r4-r12, sp, pc}^

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
