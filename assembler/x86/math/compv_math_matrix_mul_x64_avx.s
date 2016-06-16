;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Copyright (C) 2016-2017 Doubango Telecom <https://www.doubango.org>	;
; File author: Mamadou DIOP (Doubango Telecom, France).					;
; License: GPLv3. For commercial license please contact us.				;
; Source code: https://github.com/DoubangoTelecom/compv					;
; WebSite: http://compv.org												;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

%include "../compv_common_x86.s"

%if COMPV_YASM_ABI_IS_64BIT

COMPV_YASM_DEFAULT_REL

global sym(MatrixMulABt_float64_minpack1_Asm_X64_AVX)
global sym(MatrixMulABt_float64_minpack1_Asm_X64_FMA3_AVX)

section .data
	extern sym(kAVXMaskstore_0_u64)

section .text


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; %1 -> 0: FMA3 not supported, 1: FMA3 supported
; arg(0) - > const COMPV_ALIGNED(AVX) compv_float64_t* A
; arg(1) - > const COMPV_ALIGNED(AVX) compv_float64_t* B
; arg(2) - > compv_uscalar_t aRows
; arg(3) - > compv_uscalar_t bRows
; arg(4) - > compv_uscalar_t bCols
; arg(5) - > compv_uscalar_t aStrideInBytes
; arg(6) - > compv_uscalar_t bStrideInBytes
; arg(7) - > COMPV_ALIGNED(AVX) compv_float64_t* R
; arg(8) - > compv_uscalar_t rStrideInBytes
; void MatrixMulABt_float64_minpack1_Macro_X64_AVX(const COMPV_ALIGNED(AVX) compv_float64_t* A, const COMPV_ALIGNED(AVX) compv_float64_t* B, compv_uscalar_t aRows, compv_uscalar_t bRows, compv_uscalar_t bCols, compv_uscalar_t aStrideInBytes, compv_uscalar_t bStrideInBytes, COMPV_ALIGNED(AVX) compv_float64_t* R, compv_uscalar_t rStrideInBytes)
%macro MatrixMulABt_float64_minpack1_Macro_X64_AVX 1
	vzeroupper
	push rbp
	mov rbp, rsp
	COMPV_YASM_SHADOW_ARGS_TO_STACK 9
	push rsi
	push rdi
	push rbx
	push r12
	push r13
	;; end prolog ;;

	mov rax, arg(7) ; rax = R
	mov rsi, arg(2) ; rsi = aRows
	mov rdx, arg(0) ; rdx = a
	mov r8, arg(8) ; r8 = rStrideInBytes
	mov r9, arg(5) ; r9 = aStrideInBytes
	mov r10, arg(4)
	sub r10, 3 ; r10 = bCols - 3
	mov r11, arg(6) ; r11 = bStrideInBytes
	mov r12, arg(3) ; r12 = bRows
	mov r13, arg(4) ; r13 = bCols

	vmovapd ymm3, [sym(kAVXMaskstore_0_u64)] ; ymm3 = ymmMaskToExtractFirst64Bits

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; for (i = 0; i < aRows; ++i) 
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	.LoopARows
		mov rbx, arg(1) ; rbx = B
		xor rdi, rdi ; rdi = j = 0

		;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		; for (j = 0; j < bRows; ++j)
		;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		.LoopBRows
			vpxor ymm0, ymm0 ; ymm0 = ymmSum
			xor rcx, rcx ; rcx = k = 0

			;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
			; for (k = 0; k < bCols - 3; k += 4)
			;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
			.LoopBCols
				vmovapd ymm1, [rdx + rcx*8]
				%if %1 == 1 ; FMA3
					lea rcx, [rcx + 4] ; k += 4
					vfmadd231pd ymm0, ymm1, [rbx + rcx*8 - 32]
				%else
					vmulpd ymm1, ymm1, [rbx + rcx*8]
					lea rcx, [rcx + 4] ; k += 4
					vaddpd ymm0, ymm0, ymm1
				%endif
				cmp rcx, r10 ; k <? (Cols - 3)
				jl .LoopBCols
			.EndOfLoopBCols
			
			cmp rcx, r13 ; k <? bCols
			jge .BColsIsPerfectlyAVXAligned
				;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
				; for (; k < bCols; k += 1)
				;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
				.LoopBColsRemaining
					vmaskmovpd ymm1, ymm3, [rdx + rcx*8]
					vmaskmovpd ymm2, ymm3, [rbx + rcx*8]
					inc rcx
					%if %1 == 1 ; FMA3
						vfmadd231pd ymm0, ymm1, ymm2
					%else
						vmulpd ymm1, ymm1, ymm2
						vaddpd ymm0, ymm0, ymm1
					%endif
					cmp rcx, r13 ; k <? bCols
					jl .LoopBColsRemaining
				.EndOfLoopBColsRemaining
			.BColsIsPerfectlyAVXAligned			
			
			inc rdi ; ++j
			vhaddpd ymm0, ymm0, ymm0
			vperm2f128 ymm1, ymm0, ymm0, 0x11
			lea rbx, [rbx + r11] ; b += bStrideInBytes
			vaddpd ymm0, ymm0, ymm1
			cmp rdi, r12 ; j <? bRows
			vmaskmovpd [rax + rdi*8 - 8], ymm3, ymm0		
			jl .LoopBRows
		.EndOfLoopBRows

		dec rsi ; --i
		lea rax, [rax + r8] ; r += rStrideInBytes
		lea rdx, [rdx + r9] ; a += aStrideInBytes
		test rsi, rsi	
		jnz .LoopARows
	.EndOfLoopARows

	;; begin epilog ;;
	pop r13
	pop r12
	pop rbx
	pop rdi
	pop rsi
	COMPV_YASM_UNSHADOW_ARGS
	mov rsp, rbp
	pop rbp
	vzeroupper
	ret
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
sym(MatrixMulABt_float64_minpack1_Asm_X64_AVX):
	MatrixMulABt_float64_minpack1_Macro_X64_AVX 0

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
sym(MatrixMulABt_float64_minpack1_Asm_X64_FMA3_AVX):
	MatrixMulABt_float64_minpack1_Macro_X64_AVX 1



%endif ; COMPV_YASM_ABI_IS_64BIT