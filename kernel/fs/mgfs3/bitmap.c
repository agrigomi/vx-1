#include "bitmap.h"
#include "inode.h"
#include "buffer.h"

#define INVALID_RECORD_BIT	0xff

static _h_buffer_ bitmap_record(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 bit_number, /* in */
				_u32 *p_buffer_offset, _u8 *p_record_bit, _mgfs_bitmap_t **pp_brecord, /* out */
				_h_lock_ hlock) {
	_h_buffer_ r = (_h_buffer_)p_cxt->fs.inv_pattern;

	_u32 nbrec = (_u32)(hinode->p_inode->sz / sizeof(_mgfs_bitmap_t)); /* number of bitmap records */
	_u32 nbits = nbrec * MAX_BITMAP_BITS;
	if(bit_number < nbits) {
		_u32 rec = bit_number / MAX_BITMAP_BITS; /* record number */
		
		_fsaddr block_number;
		_u32 block_offset, block_count;
		_u32 inode_offset = rec * sizeof(_mgfs_bitmap_t);

		if(mgfs_inode_calc_data_pos(p_cxt, hinode, inode_offset, sizeof(_mgfs_bitmap_t),
					&block_number, &block_offset, &block_count) == sizeof(_mgfs_bitmap_t)) {
			if((r = mgfs_inode_read_block(p_cxt, hinode, block_number, hlock)) != (_h_buffer_)p_cxt->fs.inv_pattern) {
				*p_record_bit = (_u8)(bit_number % MAX_BITMAP_BITS); /* record bit */
				*p_buffer_offset = block_offset;
				_u8 *p_rec = ((_u8 *)mgfs_buffer_ptr(p_cxt, r, hlock)) + block_offset;
				*pp_brecord = (_mgfs_bitmap_t *)p_rec;
			}
		}
	}

	return r;
}

_u8 mgfs_bitmap_set(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 bit_number, _u8 bit_state, _h_lock_ hlock) {
	_u8 r = 0;
	_u32 buffer_offset = 0;
	_u8 rec_bit = INVALID_RECORD_BIT;
	_mgfs_bitmap_t *p_bmp = 0;
	
	_h_lock_ _lock = mgfs_buffer_lock(p_cxt, hlock);

	_h_buffer_ hb = bitmap_record(p_cxt, hinode, bit_number, &buffer_offset, &rec_bit, &p_bmp, _lock);
	if(hb != (_h_buffer_)p_cxt->fs.inv_pattern) {
		_u8 rbyte = rec_bit / 8; /* record byte */
		_u8 bit = rec_bit % 8;
		_u32 *p_nused = (_u32 *)&hinode->p_inode->reserved[0];
		_u8 bwrite = 0;


		if(bit_state) {
			if(!(p_bmp->bitmap[rbyte] & (0x80 >> bit))) {
				p_bmp->bitmap[rbyte] |= (0x80 >> bit);
				p_bmp->used++;
				*p_nused = *p_nused + 1;
				bwrite = 1;
			}
		} else {
			if(p_bmp->bitmap[rbyte] & (0x80 >> bit)) {
				p_bmp->bitmap[rbyte] &= ~(0x80 >> bit);
				p_bmp->used--;
				*p_nused = *p_nused - 1;
				bwrite = 1;
			}
		}

		if(bwrite) { 
			mgfs_buffer_dirty(p_cxt, hb, _lock);
			mgfs_inode_update(p_cxt, hinode, _lock);
		}

		mgfs_buffer_free(p_cxt, hb, _lock);
		r = 1;
	}
	
	mgfs_buffer_unlock(p_cxt, _lock);
	return r;
}

_u32 mgfs_bitmap_set_array(_mgfs_context_t *p_cxt, _h_inode_ hinode, _fsaddr *array, _u32 count, _u8 bit_state, _h_lock_ hlock) {
	_u32 r = 0;
	_u32 i = 0;
	for(; i < count; i++) {
		if(array[i] != (_fsaddr)p_cxt->fs.inv_pattern) {
			if(mgfs_bitmap_set(p_cxt, hinode, array[i], bit_state, hlock))
				r++;
		}
	}

	return r;
}

/* return bit number in record or 0xff if failed */
static _u8 bitmap_record_alloc_bit(_mgfs_bitmap_t *p_brec) {
	_u8 r = INVALID_RECORD_BIT;

	if(p_brec->used < MAX_BITMAP_BITS) {
		_u8 _r = 0;
		_u32 i = 0;
		for(; i < MAX_BITMAP_BYTE; i++) {
			if(p_brec->bitmap[i] != 0xff) {
				_u8 bit = 0x80;
				_u32 j = 0;
				for(; j < 8; j++) {
					if(!(p_brec->bitmap[i] & bit)) {
						i = MAX_BITMAP_BYTE;
						r = _r;
						break;
					}
					bit >>= 1;
					_r++;
				}
			} else
				_r += 8;
		}
	}

	if(r != INVALID_RECORD_BIT) {
		/* set bit */
		_u8 rec_byte = r / 8;
		_u8 bit = r % 8;
		p_brec->bitmap[rec_byte] |= (0x80 >> bit);
		p_brec->used++;
	}

	return r;
}

_u32 mgfs_bitmap_alloc(_mgfs_context_t *p_cxt, _fsaddr *array, _u32 count, _h_inode_ hinode, _h_lock_ hlock) {
	_u32 r = 0;

	if(p_cxt->flags & MGFS_SEQUENTIAL_ALLOC) {
		_u32 i = 0;
		for(; i < count; i++) {
			array[i] = p_cxt->seq_alloc_unit;
			p_cxt->seq_alloc_unit++;
			r++;	
		}
	} else {
		/* file blocks */
		_u32 nblocks = hinode->p_inode->dunits;
		/* block records */
		_u32 nbrec = mgfs_unit_size(p_cxt) / sizeof(_mgfs_bitmap_t);
		_u32 *p_nused = (_u32 *)&hinode->p_inode->reserved[0];
		_u64 offset = 0;
		_u32 block = 0;
		for(; block < nblocks && r < count && offset < hinode->p_inode->sz; block++) {
			_h_buffer_ hb = mgfs_inode_read_block(p_cxt, hinode, block, hlock);
			if(hb != p_cxt->fs.inv_pattern) {
				_u8 bupdate = 0;
				
				_h_lock_ _lock = mgfs_buffer_lock(p_cxt, hlock);
				_mgfs_bitmap_t *p_rec = (_mgfs_bitmap_t *)mgfs_buffer_ptr(p_cxt, hb, _lock);
				_u32 record = 0;
				
				for(; record < nbrec && r < count && offset < hinode->p_inode->sz; record++) {
					while((p_rec + record)->used < MAX_BITMAP_BITS && r < count) {
						_u8 rbit = bitmap_record_alloc_bit(p_rec + record);
						if(rbit != INVALID_RECORD_BIT) {
							array[r] = (block * nbrec * MAX_BITMAP_BITS) + 
									(record * MAX_BITMAP_BITS) + rbit;
							r++;
							*p_nused = *p_nused + 1;
							bupdate = 1;
						} else
							break;
					}

					offset += sizeof(_mgfs_bitmap_t);
				}

				if(bupdate) {
					mgfs_buffer_dirty(p_cxt, hb, _lock);
				}

				mgfs_buffer_free(p_cxt, hb, _lock);
				mgfs_buffer_unlock(p_cxt, _lock);
			}
		}

		if(r)
			mgfs_inode_update(p_cxt, hinode, hlock);
	}
		
	return r;
}

_u32 mgfs_bitmap_free(_mgfs_context_t *p_cxt, _fsaddr *array, _u32 count, _h_inode_ hinode, _h_lock_ hlock) {
	return mgfs_bitmap_set_array(p_cxt, hinode, array, count, 0, hlock);
}

/* return 0 for success */
_u32 mgfs_bitmap_sync(_mgfs_context_t *p_cxt, _fsaddr *array, _u32 count, _h_inode_ hinode, _h_inode_ hshadow, _h_lock_ hlock) {
	_u32 r = __ERR;
	_u32 i = 0;
	for(; i < count; i++) {
		if(array[i] != (_fsaddr)p_cxt->fs.inv_pattern) {
			_u32 buffer_offset = 0;
			_u8 rec_bit = INVALID_RECORD_BIT;
			_mgfs_bitmap_t *p_bmp = 0;

			_h_lock_ _lock = mgfs_buffer_lock(p_cxt, hlock);
			_h_buffer_ hb = bitmap_record(p_cxt, hinode, array[i], &buffer_offset, &rec_bit, &p_bmp, _lock);
			if(hb != (_h_buffer_)p_cxt->fs.inv_pattern) {
				_u8 rbyte = rec_bit / 8; /* record byte */
				_u8 bit = rec_bit % 8;


				if(p_bmp->bitmap[rbyte] & (0x80 >> bit)) {
					if(mgfs_bitmap_set(p_cxt, hshadow, array[i], 1, _lock))
						r = 0;
				} else {
					if(mgfs_bitmap_set(p_cxt, hshadow, array[i], 0, _lock))
						r = 0;
				}

				mgfs_buffer_free(p_cxt, hb, _lock);
			}
			mgfs_buffer_unlock(p_cxt, _lock);
		}
	}

	return r;
}

/* return the number of free bits */
_u32 mgfs_bitmap_free_state(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_lock_ hlock) {
	_u32 r = 0;

	_h_lock_ _lock = mgfs_buffer_lock(p_cxt, hlock);

	_u32 nrec  = hinode->p_inode->sz / sizeof(_mgfs_bitmap_t);
	_u32 nbits = nrec * MAX_BITMAP_BITS;
	_u32 *p_nused = (_u32 *)&hinode->p_inode->reserved[0];
	_u32 nused = *p_nused;	
	r = nbits - nused;

	mgfs_buffer_unlock(p_cxt, _lock);

	return r;
}

