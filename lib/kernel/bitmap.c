#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

// initial bitmap
void bitmap_init(struct bitmap* btmp){
	memset(btmp->bits, 0 , btmp->btmp_bytes_len);
}

// judge the index in bitmap is it equal to 1 ,if == 1 return true; else return false
bool bitmap_scan_test(struct bitmap* btmp , uint32_t bit_idx){
	// index /8  to index the array 
	uint32_t byte_idx = bit_idx /  8;
	// index %8 to index the bit in the array
	uint32_t bit_odd = bit_idx  % 8 ;
	
	return btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd);
}

//apply for  'cnt' bit  in bitmap ,if success return the index of begin bit ,else return -1 
int bitmap_scan(struct bitmap* btmp , uint32_t cnt){

	// get the index of byte 
	uint32_t idx_byte = 0 ;
	while ((0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len)){
		idx_byte++;
	}
	ASSERT(idx_byte < btmp->btmp_bytes_len);
	if (idx_byte == btmp->btmp_bytes_len){
		return -1;
	}

	//get the index of bit
	int idx_bit = 0 ;
	while ((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]){
		idx_bit++;
	}

	// start of conserve bit
	int bit_idx_start = idx_byte * 8 + idx_bit;
	if (cnt == 1 ){
		return bit_idx_start;
	}

	//remaining bit
	uint32_t bit_left = (btmp->btmp_bytes_len * 8 - bit_idx_start);
	//current bit is remain,so count + 1 , and point to next
	uint32_t bit_next = bit_idx_start + 1 ;
	uint32_t count = 1;

	//calculate whether have 'cnt' remain bit,if have ,return the start index 
	bit_idx_start = -1 ;
	while(bit_left-- > 0 ) {
		if (!(bitmap_scan_test(btmp,bit_next))){
			// the bit of 'bit_next' is not use , so continue calculate
			count ++ ;
		}else {
			// the bit of 'bit_next' is use ,so restart
			count = 0 ;
		}
		if (count == cnt){
			bit_idx_start = bit_next - cnt + 1 ;
			break;
		}
		bit_next++;
	}
	
	return bit_idx_start;
}

// set the 'bit_idx' bit in the bitmap to 'value'
void bitmap_set(struct bitmap* btmp , uint32_t bit_idx , int8_t value){
	ASSERT((value == 0) || (value == 1));
	uint32_t byte_idx = bit_idx / 8 ;
	uint32_t bit_odd  = bit_idx % 8 ;
	if (value){
		btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
	}else{
		btmp->bits[byte_idx] &= (BITMAP_MASK << bit_odd);
	}
}
