.data
_g_delta: .float 1.000000
.text
.text
_start_func:
sw $ra, 0($sp)
sw $fp, -4($sp)
add $fp, $sp, -4
add $sp, $sp, -8
lw $2, _frameSize_func
sub $sp, $sp, $2
sw $16, 4($sp)
sw $17, 8($sp)
sw $18, 12($sp)
sw $19, 16($sp)
sw $20, 20($sp)
sw $21, 24($sp)
sw $24, 28($sp)
sw $25, 32($sp)
s.s $f20, 36($sp)
s.s $f22, 40($sp)
s.s $f24, 44($sp)
s.s $f26, 48($sp)
s.s $f28, 52($sp)
s.s $f30, 56($sp)
s.s $f16, 60($sp)
s.s $f18, 64($sp)
#     }

l.s $f20, 8($fp)
.data
_CONSTANT_1: .word 0
.text
lw $16, _CONSTANT_1
mtc1 $16, $f22
cvt.s.w $f22, $f22
c.le.s $f20, $f22
bc1f _compareFalse_2
li $16, 0
j _compareEnd_2
_compareFalse_2:
li $16, 1
_compareEnd_2:
beqz $16, _elseLabel_0
#     } else {

#         temp = num + delta;

l.s $f20, 8($fp)
la $24, _g_delta
l.s $f22, 0($24)
add.s $f20, $f20, $f22
cvt.w.s $f20, $f20
mfc1 $16, $f20
sw $16, -4($fp)
j _ifExitLabel_0
_elseLabel_0:
#     }

#         temp = num - delta;

l.s $f20, 8($fp)
la $24, _g_delta
l.s $f22, 0($24)
sub.s $f20, $f20, $f22
cvt.w.s $f20, $f20
mfc1 $16, $f20
sw $16, -4($fp)
_ifExitLabel_0:
#     return temp;

lw $16, -4($fp)
move $v0, $16
j _end_func
_end_func:
lw $16, 4($sp)
lw $17, 8($sp)
lw $18, 12($sp)
lw $19, 16($sp)
lw $20, 20($sp)
lw $21, 24($sp)
lw $24, 28($sp)
lw $25, 32($sp)
l.s $f20, 36($sp)
l.s $f22, 40($sp)
l.s $f24, 44($sp)
l.s $f26, 48($sp)
l.s $f28, 52($sp)
l.s $f30, 56($sp)
l.s $f16, 60($sp)
l.s $f18, 64($sp)
lw $ra, 4($fp)
add $sp, $fp, 4
lw $fp, 0($fp)
jr $ra
.data
_frameSize_func: .word 72
.text
main:
sw $ra, 0($sp)
sw $fp, -4($sp)
add $fp, $sp, -4
add $sp, $sp, -8
lw $2, _frameSize_main
sub $sp, $sp, $2
sw $16, 4($sp)
sw $17, 8($sp)
sw $18, 12($sp)
sw $19, 16($sp)
sw $20, 20($sp)
sw $21, 24($sp)
sw $24, 28($sp)
sw $25, 32($sp)
s.s $f20, 36($sp)
s.s $f22, 40($sp)
s.s $f24, 44($sp)
s.s $f26, 48($sp)
s.s $f28, 52($sp)
s.s $f30, 56($sp)
s.s $f16, 60($sp)
s.s $f18, 64($sp)
#     write("Enter number "); 

.data
_CONSTANT_3: .asciiz "Enter number "
.align 2
.text
la $16, _CONSTANT_3
li $v0, 4
move $a0, $16
syscall
#     num = fread(); 

li $v0, 6
syscall
mov.s $f20, $f0
s.s $f20, -4($fp)
#     write(func(num));

l.s $f20, -4($fp)
s.s $f20, 0($sp)
addi $sp, $sp, -4
jal _start_func
addi $sp, $sp, 4
move $16, $v0
li $v0, 1
move $a0, $16
syscall
#     write("\n");

.data
_CONSTANT_4: .asciiz "\n"
.align 2
.text
la $16, _CONSTANT_4
li $v0, 4
move $a0, $16
syscall
#     return 0;

.data
_CONSTANT_5: .word 0
.text
lw $16, _CONSTANT_5
move $v0, $16
j _end_main
_end_main:
lw $16, 4($sp)
lw $17, 8($sp)
lw $18, 12($sp)
lw $19, 16($sp)
lw $20, 20($sp)
lw $21, 24($sp)
lw $24, 28($sp)
lw $25, 32($sp)
l.s $f20, 36($sp)
l.s $f22, 40($sp)
l.s $f24, 44($sp)
l.s $f26, 48($sp)
l.s $f28, 52($sp)
l.s $f30, 56($sp)
l.s $f16, 60($sp)
l.s $f18, 64($sp)
lw $ra, 4($fp)
add $sp, $fp, 4
lw $fp, 0($fp)
li $v0, 10
syscall
.data
_frameSize_main: .word 68
