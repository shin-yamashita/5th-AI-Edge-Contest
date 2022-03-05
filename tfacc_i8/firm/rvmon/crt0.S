
#
# crt0.S : RISC-V startup routine
#

#  .section .text.startup
  .section .vector
  .global _start, __errno
  .type   _start, @function

_start:
  # Initialize global pointer
.option push
.option norelax
1:auipc gp, %pcrel_hi(__global_pointer$)
  addi  gp, gp, %pcrel_lo(1b)
.option pop
  la      sp, __stack_top
  la      t0, _irq_handle
  csrw    mtvec, t0

  # Clear bss section
  la      a0, _edata
#  la      a2, _end
#  sub     a2, a2, a0
#  li      a1, 0
#  call    memset
  la      a1, _end
  sub     a1, a1, a0
  call    memclr  # rv-test/lib/memclr.c

#  lui     a0,0x18
#  addi    a0,a0,1696  # a0 = 100000
#  call    init_timer

  lw      a0, 0(sp)                  # a0 = argc
  addi    a1, sp, __SIZEOF_POINTER__ # a1 = argv
  li      a2, 0                      # a2 = envp = NULL
  call    main
#  call    init

  call    at_exit

  li      t0, 93	# SYS_exit
  scall

Loop:
  j    Loop

__errno:
  nop


  .section .text.startup

  .equ REGBYTES, 4
  .equ CONTEXT_SIZE, 12*REGBYTES
  .macro lxsp a, b
  lw \a, ((\b)*REGBYTES)(sp)
  .endm
  .macro sxsp a, b
  sw \a, ((\b)*REGBYTES)(sp)
  .endm

#       // 0    1    2    3    4    5    6    7    8    9   10    11    12   13   14   15
#        {"x0","ra","sp","gp","tp","t0","t1","t2","s0","s1","a0", "a1", "a2","a3","a4","a5",

  .align 4
_irq_handle:
    # Save registers.
    addi    sp, sp, -CONTEXT_SIZE
    sxsp    ra, 0
    sxsp    a0, 1
    sxsp    a1, 2
    sxsp    a2, 3
    sxsp    a3, 4
    sxsp    a4, 5
    sxsp    a5, 6
    sxsp    s0, 7
    sxsp    s1, 8
    sxsp    t0, 9
    sxsp    t1, 10
    sxsp    t2, 11
#    sxsp    gp, 12
#    sxsp    tp, 13

    # Invoke the handler.
    mv      a0, sp
    csrr    a1, mcause
    csrr    a2, mepc
    jal     irq_handler

    # Restore registers.
    lxsp    ra, 0
    lxsp    a0, 1
    lxsp    a1, 2
    lxsp    a2, 3
    lxsp    a3, 4
    lxsp    a4, 5
    lxsp    a5, 6
    lxsp    s0, 7
    lxsp    s1, 8
    lxsp    t0, 9
    lxsp    t1, 10
    lxsp    t2, 11
#    lxsp    gp, 12
#    lxsp    tp, 13
    addi sp, sp, CONTEXT_SIZE

    # Return
    mret

