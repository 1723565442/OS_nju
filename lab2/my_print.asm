global	asm_print

	section .text
asm_print:
        push    ebp
        mov     ebp, esp
        mov     edx, [ebp+12] ;长度
        mov     ecx, [ebp+8]   ; 字符窜地址
        mov     ebx, 1  ;标准输出
        mov     eax, 4  ; 系统调用号 4 表示write
        int     80h
        pop     ebp
        ret

        ; nasm -f elf32 -o my_print.o my_print.asm
        ; g++ -std=c++11 -m32 main.cpp my_print.o -o main
        ; ./main

        ; mkfs.fat -C a.img 1440
        ; mkdir mount
        ; sudo mount a.img mount
        