########################################################################
#  Copyright(c) 2011-2014 Intel Corporation All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#    * Neither the name of Intel Corporation nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
########################################################################


lib_name    := 	erasure_code.a

lsrc        += 	ec_highlevel_func.c \
		ec_base.c \
		gf_vect_mul_sse.asm \
		gf_vect_mul_avx.asm \
		gf_vect_dot_prod_sse.asm  \
		gf_vect_dot_prod_avx.asm \
		gf_vect_dot_prod_avx2.asm \
		gf_2vect_dot_prod_sse.asm \
		gf_3vect_dot_prod_sse.asm \
		gf_4vect_dot_prod_sse.asm \
		gf_5vect_dot_prod_sse.asm \
		gf_6vect_dot_prod_sse.asm \
		gf_2vect_dot_prod_avx.asm \
		gf_3vect_dot_prod_avx.asm \
		gf_4vect_dot_prod_avx.asm \
		gf_5vect_dot_prod_avx.asm \
		gf_6vect_dot_prod_avx.asm \
		gf_2vect_dot_prod_avx2.asm \
		gf_3vect_dot_prod_avx2.asm \
		gf_4vect_dot_prod_avx2.asm \
		gf_5vect_dot_prod_avx2.asm \
		gf_6vect_dot_prod_avx2.asm \
		ec_multibinary.asm

lsrc32	     += ec_highlevel_func.c ec_multibinary.asm ec_base.c


other_src   += ec_base.h reg_sizes.asm


VPATH = .. ../include

-include ../make.inc
