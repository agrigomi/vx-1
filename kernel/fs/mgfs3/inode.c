#include "mgfs3.h"
#include "buffer.h"
#include "bitmap.h"
#include "inode.h"

#define MAX_UNIT_ARRAY	32

_u32 _pow32(_u32 base, _u8 pow) {
	_u32 res = 1;
	int i;
	
	for(i = 0; i < pow; i++)
		res = res * base;
	
	return res;
}

_u8 mgfs_is_valid_inode_handle(_mgfs_context_t *p_cxt, _h_inode_ h) {
	_u8 r = 0;
	if(h->hb_inode != (_h_buffer_)p_cxt->fs.inv_pattern)
		r = 1;
	return r;
}

/* return count of data unit numbers in "p_u_buffer" */
_u32 mgfs_inode_list_dunit(_mgfs_context_t *p_cxt, 
		 _h_inode_ hinode,
		 _fsaddr first, /* first needed block in file */
		 _u32 count, /* count of units after first */
		 _fsaddr *p_u_buffer, /* [out] array of data unit numbers */
		 _h_lock_ hlock
		) {
	_u32 r = 0;
	_fsaddr *p_cl_num;
	_u32 i,c,max;
	_u8 l = hinode->p_inode->level;
	_fsaddr f = first;
	_u32 cl_sz = p_cxt->fs.sz_sector * p_cxt->fs.sz_unit;
	
	while(r < count) {
		void *p_buffer=NULL;
		_h_buffer_ hbuffer = (_h_buffer_)p_cxt->fs.inv_pattern; 
		_fsaddr page_number = (_fsaddr)p_cxt->fs.inv_pattern;

		c = MAX_LOCATION_INDEX;
		p_cl_num = (_fsaddr *)&(hinode->p_inode->location[0]);
		f = first + r;
		l = hinode->p_inode->level;
		
		while(l) {
			/* max pointers per root at current level */
			max = _pow32(cl_sz / p_cxt->fs.sz_addr, l);
			for(i = 0; i < c; i++) {
				if(((i+1) * max) > f) {
					if(*(p_cl_num + i) != (_fsaddr)p_cxt->fs.inv_pattern) {
						/* read address cluster */
						page_number = *(p_cl_num + i);
						if((hbuffer = mgfs_buffer_alloc(p_cxt, 
									page_number, hlock)) != (_h_buffer_)p_cxt->fs.inv_pattern) {
							p_buffer = mgfs_buffer_ptr(p_cxt, hbuffer, hlock);
							p_cl_num = (_fsaddr *)p_buffer;

							/* count of addresses per cluster */
							c = cl_sz / p_cxt->fs.sz_addr;
							f -= i * max;
						}/* else 
							break;*/
					}
					
					break;
				}
			}
			
			l--;
			if(l && hbuffer != (_h_buffer_)p_cxt->fs.inv_pattern) {
				mgfs_buffer_free(p_cxt, hbuffer, hlock);
				hbuffer = (_h_buffer_)p_cxt->fs.inv_pattern;
			}
		}
		
		if(f >= c) {
			if(hbuffer != (_h_buffer_)p_cxt->fs.inv_pattern) {
				mgfs_buffer_free(p_cxt, hbuffer, hlock);
				hbuffer = (_h_buffer_)p_cxt->fs.inv_pattern;
			}
			break;
		}
		
		for(; f < c; f++) {
			if(*(p_cl_num + f) != (_fsaddr)p_cxt->fs.inv_pattern) {
				*(p_u_buffer + r) = *(p_cl_num + f);
				r++;
				if(r >= count)
					break;
			} else {
				if(hbuffer != (_h_buffer_)p_cxt->fs.inv_pattern)
					mgfs_buffer_free(p_cxt, hbuffer, hlock);
				
				return r;
			}
		}
		
		if(hbuffer != (_h_buffer_)p_cxt->fs.inv_pattern) {
			mgfs_buffer_free(p_cxt, hbuffer, hlock);
			hbuffer = (_h_buffer_)p_cxt->fs.inv_pattern;
		}
	}
	
	return r;
}

/* return the size of requested data in bytes */
_u32 mgfs_inode_calc_data_pos(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 inode_offset, _u32 size, /* in */
			_fsaddr *p_block_number, _u32 *p_block_offset, _u32 *p_block_count /* out */
		       ) {
	_u32 r = 0;
	_mgfs_info_t *p_fs = mgfs_get_info(p_cxt);
	_u64 inode_size = mgfs_unit_size(p_cxt) * hinode->p_inode->dunits;
	
	if(p_fs && inode_offset < inode_size) {
		_u32 unit_size = p_fs->sz_sector * p_fs->sz_unit;
		*p_block_number = (_u32)(inode_offset / unit_size);
		*p_block_offset = (_u32)(inode_offset % unit_size);
		
		r = ((inode_offset + size) < inode_size) ? size : (_u32)(inode_size - inode_offset);
		*p_block_count = ((_u32)((*p_block_offset) + r)) / unit_size;
		if(((_u32)((*p_block_offset) + r)) % unit_size)
			*p_block_count = *p_block_count + 1;
	}
	
	return r;
}

_h_buffer_ mgfs_inode_read_block(_mgfs_context_t *p_cxt, _h_inode_ hinode, _fsaddr block_number, _h_lock_ hlock) {
	_h_buffer_ r = (_h_buffer_)p_cxt->fs.inv_pattern;
	_fsaddr unit_number = (_fsaddr)p_cxt->fs.inv_pattern;
	
	if(mgfs_inode_list_dunit(p_cxt, hinode, block_number, 1, &unit_number, hlock) == 1)
		r = mgfs_buffer_alloc(p_cxt, unit_number, hlock);
	
	return r;
}

/* return 0 for success */
_u32 mgfs_inode_meta_open(_mgfs_context_t *p_cxt, _u8 meta_inode_number, /* in */
			    _h_inode_ hinode, /* out */
			    _h_lock_ hlock
			   ) {
	_fsaddr r = (_fsaddr)p_cxt->fs.inv_pattern;
	_fsaddr block_number;
	_u32 block_offset, block_count;
	_u64 offset = meta_inode_number * sizeof(_mgfs_inode_record_t);
	_mgfs_inode_handle_t htemp;
	
	htemp.p_inode = &(p_cxt->fs.meta);
	htemp.hb_inode = (_h_buffer_)p_cxt->fs.inv_pattern;
	
	if(mgfs_inode_calc_data_pos(p_cxt, &htemp, offset, sizeof(_mgfs_inode_record_t),
				&block_number, &block_offset, &block_count) == sizeof(_mgfs_inode_record_t)) {
		if((hinode->hb_inode = mgfs_inode_read_block(p_cxt, &htemp, block_number, hlock)) != (_h_buffer_)p_cxt->fs.inv_pattern) {
			_u8 *p_u8_buffer = (_u8 *)mgfs_buffer_ptr(p_cxt, hinode->hb_inode, hlock);
			hinode->p_inode = (_mgfs_inode_record_t *)(p_u8_buffer + block_offset);
			hinode->number = meta_inode_number;
			r = 0;
		}
	}
	
	return r;
}

void mgfs_inode_close(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_lock_ hlock) {
	if(hinode->hb_inode != (_h_buffer_)p_cxt->fs.inv_pattern) {
		mgfs_buffer_free(p_cxt, hinode->hb_inode, hlock);
		hinode->hb_inode = (_h_buffer_)p_cxt->fs.inv_pattern;
	}
}

void mgfs_inode_update(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_lock_ hlock) {
	if(hinode->hb_inode != (_h_buffer_)p_cxt->fs.inv_pattern) {
		hinode->p_inode->mo = p_cxt->timestamp();
		mgfs_buffer_dirty(p_cxt, hinode->hb_inode, hlock);
	}
}

/* return 0 for success */
_u32 mgfs_inode_open(_mgfs_context_t *p_cxt, _u32 inode_number, /* in */
		     _h_inode_ hinode, /* out */
		     _h_lock_ hlock
		    ) {
	_fsaddr r = (_fsaddr)p_cxt->fs.inv_pattern;
	_fsaddr block_number;
	_u32 block_offset, block_count;
	_u64 offset = inode_number * sizeof(_mgfs_inode_record_t);
	_mgfs_inode_handle_t hmeta;
	
	if(mgfs_inode_meta_open(p_cxt, INODE_FILE_IDX, &hmeta, hlock) == 0) {
		if(mgfs_inode_calc_data_pos(p_cxt, &hmeta, offset, sizeof(_mgfs_inode_record_t),
					&block_number, &block_offset, &block_count) == sizeof(_mgfs_inode_record_t)) {
			if((hinode->hb_inode = mgfs_inode_read_block(p_cxt, &hmeta, 
							block_number, hlock)) != (_h_buffer_)p_cxt->fs.inv_pattern) {
				_u8 *p_u8_buffer = (_u8 *)mgfs_buffer_ptr(p_cxt, hinode->hb_inode, hlock);
				hinode->p_inode = (_mgfs_inode_record_t *)(p_u8_buffer + block_offset);
				hinode->number = inode_number;
				r = 0;
			}
		}
		
		mgfs_inode_close(p_cxt, &hmeta, hlock);
	}
	
	return r;
}

_u32 mgfs_inode_owner(_mgfs_context_t *p_cxt, _h_inode_ hinode) {
	_u32 r = 0;
	if(hinode->hb_inode != (_h_buffer_)p_cxt->fs.inv_pattern)
		r = hinode->p_inode->owner;

	return r;
}

_u32 mgfs_inode_read(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 offset, void *p_buffer, _u32 size, _h_lock_ hlock) {
	_u32 r = 0;
	_mgfs_info_t *p_fs = mgfs_get_info(p_cxt);
	
	if(p_fs && mgfs_is_valid_inode_handle(p_cxt, hinode)) {
		_fsaddr block_number;
		_u32 block_offset, block_count;
		_u32 _r = 0;
		if((_r = mgfs_inode_calc_data_pos(p_cxt, hinode, offset, size, &block_number, &block_offset, &block_count))) {
			_u32 unit_size = p_fs->sz_sector * p_fs->sz_unit;
			while(r < _r) {
				_u32 i = 0;
				_fsaddr unit_array[MAX_UNIT_ARRAY];
				_u32 array_count = mgfs_inode_list_dunit(p_cxt, hinode, block_number, 
						(block_count < MAX_UNIT_ARRAY) ? block_count : MAX_UNIT_ARRAY, unit_array, hlock);

				if(!array_count)
					break;

				block_number += array_count;
				block_count  -= array_count;
				
				for(i = 0; i < array_count; i++) {
					_h_buffer_ hb = mgfs_buffer_alloc(p_cxt, unit_array[i], hlock);
					if(hb != (_h_buffer_)p_cxt->fs.inv_pattern) {
						_u8 *_buffer = (_u8 *)mgfs_buffer_ptr(p_cxt, hb, hlock);
						
						_u32 _b = ((((_u32)(unit_size - block_offset)) > (_r - r)) ? 
								(_r - r):
								(unit_size - block_offset));
						
						p_cxt->memcpy((_u8 *)p_buffer + r, _buffer + block_offset, _b);
						mgfs_buffer_free(p_cxt, hb, hlock);
						r += _b;
						block_offset = 0;
					} else
						goto _mgfs_inode_read_done_;
				}
			}
		}
	}
	
_mgfs_inode_read_done_:	
	
	return r;
}

/* return the number of new appended blocks */
static _u32 inode_append_block(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u8 level, _u32 base_unit,
				_h_inode_ hbitmap, _fsaddr *array, _u32 count, _h_inode_ hbshadow, _h_lock_ hlock) {
	_u32 r = 0;
	_u8 _level = level;
	_u32 _base_unit = base_unit;
	_fsaddr *p_unit_number = NULL;
	_u32 addr_count = 0;
	_h_buffer_ hb_addr = (_h_buffer_)p_cxt->fs.inv_pattern;

	while(r < count) {
		if(_level == hinode->p_inode->level) { /* enumerate the 'inode' locations */
			addr_count = MAX_LOCATION_INDEX;
			p_unit_number = &(hinode->p_inode->location[0]);
		} else { /* enumerate address unit */
			addr_count = mgfs_unit_size(p_cxt) / ADDR_SZ;
			if((hb_addr = mgfs_buffer_alloc(p_cxt, _base_unit, hlock)) != (_h_buffer_)p_cxt->fs.inv_pattern)
				p_unit_number = (_fsaddr *)mgfs_buffer_ptr(p_cxt, hb_addr, hlock);
			else
				break;
		}

		_u32 i = 0; /* searching for last valid position */
		while(*(p_unit_number + i) != (_fsaddr)p_cxt->fs.inv_pattern && i < addr_count)
			i++;

		if(i < addr_count) {
			if(_level) {
				if(i > 0) {
					/*	   |
						XXX+--
					 looking for free positions in one level down */
					r += inode_append_block(p_cxt, hinode, _level - 1, *(p_unit_number + i - 1), 
								hbitmap, array + r, count - r, hbshadow, hlock);
					if(r >= count)
						break;
				}

				/* allocate address unit */
				_fsaddr new_addr_unit = (_fsaddr)p_cxt->fs.inv_pattern;
				if(mgfs_bitmap_alloc(p_cxt, &new_addr_unit, 1, hbitmap, hlock) == 1) {
					_h_buffer_ hb_new_addr = mgfs_buffer_alloc(p_cxt, new_addr_unit, hlock);
					if(hb_new_addr != (_h_buffer_)p_cxt->fs.inv_pattern) {
						/* assign the new address unit */
						*(p_unit_number + i) = new_addr_unit;

						_u8 *p_new = (_u8 *)mgfs_buffer_ptr(p_cxt, hb_new_addr, hlock);
						p_cxt->memset(p_new, 0xff, mgfs_unit_size(p_cxt));
						mgfs_buffer_dirty(p_cxt, hb_new_addr, hlock);
						mgfs_buffer_free(p_cxt, hb_new_addr, hlock);
					} else {
						mgfs_bitmap_free(p_cxt, &new_addr_unit, 1, hbitmap, hlock);
						break;
					}

					if(hbshadow)
						mgfs_bitmap_sync(p_cxt, &new_addr_unit, 1, hbitmap, hbshadow, hlock);
				} else
					break;
			} else {
				/* level 0 (contain numbers of data units) */
				while(i < addr_count) {
					/* assign data unit */
					*(p_unit_number + i) = *(array + r);
					r++;
					hinode->p_inode->dunits++;
					if(r == count)
						break;

					i++;
				}

				if(_level != hinode->p_inode->level) {
					/* update the current unit */
					mgfs_buffer_dirty(p_cxt, hb_addr, hlock);
				}
			}
		} else {
			if(_level == hinode->p_inode->level && _level <= MAX_INODE_LEVEL) {	/* switch to next level */
				/* allocate address unit */
				_fsaddr new_addr_unit = (_fsaddr)p_cxt->fs.inv_pattern;
				if(mgfs_bitmap_alloc(p_cxt, &new_addr_unit, 1, hbitmap, hlock) == 1) {
					_h_buffer_ hb_new_addr = mgfs_buffer_alloc(p_cxt, new_addr_unit, hlock);
					if(hb_new_addr != (_h_buffer_)p_cxt->fs.inv_pattern) {
						_u8 *p_new = (_u8 *)mgfs_buffer_ptr(p_cxt, hb_new_addr, hlock);
						p_cxt->memset(p_new, 0xff, mgfs_unit_size(p_cxt));
						/* export addresses from 'inode' to the new address unit */
						p_unit_number = (_fsaddr *)p_new;
						for(i = 0; i < MAX_LOCATION_INDEX; i++)
							*(p_unit_number + i) = hinode->p_inode->location[i];

						mgfs_buffer_dirty(p_cxt, hb_new_addr, hlock);
						hinode->p_inode->level++;
						p_cxt->memset((_u8 *)hinode->p_inode->location, 0xff, 
							sizeof(hinode->p_inode->location));
						hinode->p_inode->location[0] = new_addr_unit;
						_base_unit = new_addr_unit;
						mgfs_buffer_free(p_cxt, hb_new_addr, hlock);
					} else {
						mgfs_bitmap_free(p_cxt, &new_addr_unit, 1, hbitmap, hlock);
						break;
					}
					
					if(hbshadow)
						mgfs_bitmap_sync(p_cxt, &new_addr_unit, 1, hbitmap, hbshadow, hlock);
				} else
					break;
			} else
				break;
		}
	}

	if(hb_addr != (_h_buffer_)p_cxt->fs.inv_pattern)
		mgfs_buffer_free(p_cxt, hb_addr, hlock);

	return r;
}

/* return the number of new appended blocks */
static _u32 inode_append(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u32 nblocks, _h_lock_ hlock) {
	_u32 r = 0;
	_fsaddr unit_array[MAX_UNIT_ARRAY];
	_mgfs_inode_handle_t hbitmap;
	_mgfs_inode_handle_t hbshadow;
	_u8 bshadow = 0;

	if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_IDX, &hbitmap, hlock) == 0) {
		if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
			if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_SHADOW_IDX, &hbshadow, hlock) == 0)
				/* use bitmap shadow */
				bshadow = 1;
		}

		while(r < nblocks) {
			_u32 array_count = mgfs_bitmap_alloc(p_cxt, unit_array, 
							(nblocks < MAX_UNIT_ARRAY) ? nblocks : MAX_UNIT_ARRAY,
							&hbitmap, hlock);
			if(array_count) {
				_u32 _r = inode_append_block(p_cxt, hinode, 
							hinode->p_inode->level, hinode->p_inode->location[0],
							&hbitmap, unit_array, array_count, (bshadow) ? &hbshadow : 0, hlock);
				if(bshadow) /* sync the space bitmap */
					mgfs_bitmap_sync(p_cxt, unit_array, _r, &hbitmap, &hbshadow, hlock);
				
				if(_r)
					r += _r;
				else
					break;
			} else
				break;
		}

		if(bshadow)
			mgfs_inode_close(p_cxt, &hbshadow, hlock);
		
		mgfs_inode_close(p_cxt, &hbitmap, hlock);
	}

	return r;
}

/* append initialized blocks to inode, and return the number of new appended blocks */
_u32 mgfs_inode_append_blocks(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u32 nblocks, _u8 pattern, _h_lock_ hlock) {
	_u32 r = 0;

	_fsaddr unit_array[MAX_UNIT_ARRAY];
	_mgfs_inode_handle_t hbitmap;
	_mgfs_inode_handle_t hbshadow;
	_u8 bshadow = 0;

	if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_IDX, &hbitmap, hlock) == 0) {
		if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
			if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_SHADOW_IDX, &hbshadow, hlock) == 0)
				/* use bitmap shadow */
				bshadow = 1;
		}

		while(r < nblocks) {
			_u32 array_count = mgfs_bitmap_alloc(p_cxt, unit_array, 
							(nblocks < MAX_UNIT_ARRAY) ? nblocks : MAX_UNIT_ARRAY,
							&hbitmap, hlock);
			if(array_count) {
				_u32 i = 0;
				for(i = 0; i < array_count; i++) {
					_h_buffer_ hbunit = mgfs_buffer_alloc(p_cxt, unit_array[i], hlock);
					if(hbunit != (_h_buffer_)p_cxt->fs.inv_pattern) {
						_u8 *ptr = (_u8 *)mgfs_buffer_ptr(p_cxt, hbunit, hlock);
						p_cxt->memset(ptr, pattern, mgfs_unit_size(p_cxt));
						mgfs_buffer_dirty(p_cxt, hbunit, hlock);
						mgfs_buffer_free(p_cxt, hbunit, hlock);
					}
				}

				_u32 _r = inode_append_block(p_cxt, hinode, 
							hinode->p_inode->level, hinode->p_inode->location[0],
							&hbitmap, unit_array, array_count, (bshadow) ? &hbshadow : 0, hlock);
				if(bshadow) /* sync the space bitmap */
					mgfs_bitmap_sync(p_cxt, unit_array, _r, &hbitmap, &hbshadow, hlock);
				
				if(_r)
					r += _r;
				else
					break;
			} else
				break;
		}

		if(bshadow)
			mgfs_inode_close(p_cxt, &hbshadow, hlock);
		
		mgfs_inode_close(p_cxt, &hbitmap, hlock);
	}

	return r;
}

_u32 mgfs_inode_write(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 offset, void *p_buffer, _u32 size, _h_lock_ hlock) {
	_u32 r = 0;
	_mgfs_info_t *p_fs = mgfs_get_info(p_cxt);
	
	if(p_fs && mgfs_is_valid_inode_handle(p_cxt, hinode)) {		
		_u32 unit_size = p_fs->sz_sector * p_fs->sz_unit;
		/* calculate the absolute inode size in bytes */
		_u64 inode_size = hinode->p_inode->dunits * unit_size;
		
		if((offset + size) > inode_size) {
			_u32 add_blocks = (_u32)( (((offset + size) - inode_size) % unit_size) ?
						  ((((offset + size) - inode_size) / unit_size) + 1):
						  (((offset + size) - inode_size) / unit_size)
						);
			
			/* add blocks to inode */
			inode_append(p_cxt, hinode, add_blocks, hlock);
			inode_size += hinode->p_inode->dunits * unit_size;
		}

		if((offset + size) <= inode_size) {
			_fsaddr block_number;
			_u32 block_offset, block_count;
			_u32 _r = 0;
			if((_r = mgfs_inode_calc_data_pos(p_cxt, hinode, offset, size, 
								&block_number, &block_offset, &block_count))) {
				while(r < _r) {
					_u32 i = 0;
					_fsaddr unit_array[MAX_UNIT_ARRAY];
					_u32 array_count = mgfs_inode_list_dunit(p_cxt, hinode, block_number, 
							(block_count < MAX_UNIT_ARRAY) ? block_count : MAX_UNIT_ARRAY, 
							unit_array, hlock);

					if(!array_count)
						break;

					block_number += array_count;
					block_count  -= array_count;
					
					for(i = 0; i < array_count; i++) {
						_h_buffer_ hb = mgfs_buffer_alloc(p_cxt, unit_array[i], hlock);
						if(hb != (_h_buffer_)p_cxt->fs.inv_pattern) { /* write data buffer */
							_u8 *_buffer = (_u8 *)mgfs_buffer_ptr(p_cxt, hb, hlock);
							
							_u32 _b = ((((_u32)(unit_size - block_offset)) > (_r - r)) ? 
									(_r - r):
									(unit_size - block_offset));
							
							p_cxt->memcpy(_buffer + block_offset, (_u8 *)p_buffer + r, _b);
							r += _b;
							block_offset = 0;
							mgfs_buffer_dirty(p_cxt, hb, hlock);
							/* commit data buffer here to prevent 
							   buffers overflow */
							mgfs_buffer_flush(p_cxt, hb, hlock);
							mgfs_buffer_free(p_cxt, hb, hlock);
						} else
							goto _mgfs_inode_write_done_;
					}
				}
			}
		}
	}

_mgfs_inode_write_done_:
	/* update inode */
	if((offset + r) > hinode->p_inode->sz)
		hinode->p_inode->sz = (offset + r);
	
	mgfs_inode_update(p_cxt, hinode, hlock);

	return r;
}

/* return inode number */
static _fsaddr inode_alloc(_mgfs_context_t *p_cxt, _h_lock_ hlock) {
	_fsaddr r = (_fsaddr)p_cxt->fs.inv_pattern;
	_mgfs_inode_handle_t h_ibitmap;	/* inode bitmap */
	_mgfs_inode_handle_t h_isbitmap;/* inode bitmap shadow */
	_u8 use_shadow = 0;

	if(mgfs_inode_meta_open(p_cxt, INODE_BITMAP_IDX, &h_ibitmap, hlock) == 0) {
		if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
			if(mgfs_inode_meta_open(p_cxt, INODE_BITMAP_SHADOW_IDX, &h_isbitmap, hlock) == 0)
				/* use inode bitmap shadow */
				use_shadow = 1;
		}

		if(!mgfs_bitmap_free_state(p_cxt, &h_ibitmap, hlock)) {
			/* extend inode bitmap */
			_h_lock_ _lock = mgfs_buffer_lock(p_cxt, hlock);

			if(mgfs_inode_append_blocks(p_cxt, &h_ibitmap, 1, 0, _lock) == 1) {
				h_ibitmap.p_inode->sz += mgfs_unit_size(p_cxt);
				mgfs_inode_update(p_cxt, &h_ibitmap, _lock);
				if(use_shadow) {
					if(mgfs_inode_append_blocks(p_cxt, &h_isbitmap, 1, 0, _lock)) {
						h_isbitmap.p_inode->sz += mgfs_unit_size(p_cxt);
						mgfs_inode_update(p_cxt, &h_isbitmap, _lock);
					}
				}
			}

			mgfs_buffer_unlock(p_cxt, _lock);
		}

		_fsaddr new_inode = (_fsaddr)p_cxt->fs.inv_pattern;
		if(mgfs_bitmap_alloc(p_cxt, &new_inode, 1, &h_ibitmap, hlock) == 1)
			r = new_inode;

		if(use_shadow) {
			if(r != (_fsaddr)p_cxt->fs.inv_pattern)
				mgfs_bitmap_sync(p_cxt, &new_inode, 1, &h_ibitmap, &h_isbitmap, hlock);
			mgfs_inode_close(p_cxt, &h_isbitmap, hlock);
		}

		mgfs_inode_close(p_cxt, &h_ibitmap, hlock);
	}

	return r;
}

/* return 0 for success */
static _u32 inode_free(_mgfs_context_t *p_cxt, _fsaddr inode_number, _h_lock_ hlock) {
	_fsaddr r = (_fsaddr)p_cxt->fs.inv_pattern;
	_mgfs_inode_handle_t h_ibitmap;	/* inode bitmap */
	_mgfs_inode_handle_t h_isbitmap;/* inode bitmap shadow */
	_u8 use_shadow = 0;

	if(mgfs_inode_meta_open(p_cxt, INODE_BITMAP_IDX, &h_ibitmap, hlock) == 0) {
		if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
			if(mgfs_inode_meta_open(p_cxt, INODE_BITMAP_SHADOW_IDX, &h_isbitmap, hlock) == 0)
				use_shadow = 1;
		}

		if(mgfs_bitmap_free(p_cxt, &inode_number, 1, &h_ibitmap, hlock) == 1)
			r = 0;

		if(use_shadow) {
			mgfs_bitmap_sync(p_cxt, &inode_number, 1, &h_ibitmap, &h_isbitmap, hlock);
			mgfs_inode_close(p_cxt, &h_isbitmap, hlock);
		}

		mgfs_inode_close(p_cxt, &h_ibitmap, hlock);
	}

	return r;
}

/* return inode number for success, else (_fsaddr)p_cxt->fs.inv_pattern */
_fsaddr mgfs_inode_create(_mgfs_context_t *p_cxt, _u16 flags,  _u32 owner_id, /* in */
		       _h_inode_ hinode, /* out */ 
		       _h_lock_ hlock
		      ) {
	_fsaddr r = (_fsaddr)p_cxt->fs.inv_pattern;

	if((r = inode_alloc(p_cxt, hlock)) != (_fsaddr)p_cxt->fs.inv_pattern) {
		_mgfs_inode_handle_t hif_meta;	/* inode container */
		_u64 inode_offset = r * sizeof(_mgfs_inode_record_t);
		
		if(mgfs_inode_meta_open(p_cxt, INODE_FILE_IDX, &hif_meta, hlock) == 0) {
			_mgfs_inode_record_t ir;
			_u8 i = 0;

			p_cxt->memset((_u8 *)&ir, 0, sizeof(_mgfs_inode_record_t));
			ir.flags = flags;
			ir.cr = p_cxt->timestamp();
			ir.owner = owner_id;

			for(i = 0; i < MAX_LOCATION_INDEX; i++)
				ir.location[i] = (_fsaddr)p_cxt->fs.inv_pattern;
			
			_h_lock_ _lock = mgfs_buffer_lock(p_cxt, hlock);

			if(mgfs_inode_write(p_cxt, &hif_meta, inode_offset, &ir, sizeof(_mgfs_inode_record_t), _lock) !=
										sizeof(_mgfs_inode_record_t)) {
				inode_free(p_cxt, r, _lock);
				r = (_fsaddr)p_cxt->fs.inv_pattern;
			} else {
				if(mgfs_inode_open(p_cxt, r, hinode, _lock) != 0) {
					inode_free(p_cxt, r, _lock);
					r = (_fsaddr)p_cxt->fs.inv_pattern;
				}
			}

			mgfs_buffer_unlock(p_cxt, _lock);
			mgfs_inode_close(p_cxt, &hif_meta, hlock);
		}
	}

	return r;
}

static _u32 inode_remove_block(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_inode_ hbitmap, _h_inode_ hsbitmap,
				_u8 level, _fsaddr base_unit, _u32 nblocks, _u8 *empty, _h_lock_ hlock) {
	_u32 r = 0;
	_fsaddr *p_unit_number = 0;
	_u32 unit_size = mgfs_unit_size(p_cxt);
	_h_buffer_ hbase = (_h_buffer_)p_cxt->fs.inv_pattern;
	_u32 addr_count = 0;

	if(level == hinode->p_inode->level) {
		p_unit_number = &hinode->p_inode->location[0];
		addr_count = MAX_LOCATION_INDEX;
	} else {
		if((hbase = mgfs_buffer_alloc(p_cxt, base_unit, hlock)) != (_h_buffer_)p_cxt->fs.inv_pattern) {
			p_unit_number = (_fsaddr *)mgfs_buffer_ptr(p_cxt, hbase, hlock);
			addr_count = unit_size / ADDR_SZ;
		}
	}

	_u32 addr_idx = addr_count;
	while(addr_idx && r < nblocks && p_unit_number) {
		addr_idx--;
		
		_fsaddr _base_unit = *(p_unit_number + addr_idx);
		if(_base_unit != (_fsaddr)p_cxt->fs.inv_pattern) {
			if(level) {
				_u8 _empty = 0;
				_u32 _r = inode_remove_block(p_cxt, hinode, hbitmap, hsbitmap, level - 1,
							_base_unit, nblocks - r, &_empty , hlock);
				hinode->p_inode->dunits -= _r;
				r += _r;

				if(_empty) {
					mgfs_bitmap_free(p_cxt, &_base_unit, 1, hbitmap, hlock);
					if(hsbitmap)
						mgfs_bitmap_sync(p_cxt, &_base_unit, 1, hbitmap, hsbitmap, hlock);

					*(p_unit_number + addr_idx) = (_fsaddr)p_cxt->fs.inv_pattern;
				}
			} else {/* remove data blocks */
				_u32 _count = 0;
				_u32 _r = r;
				while(addr_idx && _r < nblocks) {
					if(*(p_unit_number + addr_idx) != (_fsaddr)p_cxt->fs.inv_pattern) {
						_count++;
						_r++;
					}
					
					addr_idx--;
				}
		
				_fsaddr *_base = (p_unit_number + addr_idx);
				_r = mgfs_bitmap_free(p_cxt, _base, _count+1, hbitmap, hlock);
				hinode->p_inode->dunits -= _r;
				if(hsbitmap)
					mgfs_bitmap_sync(p_cxt, _base, _r, hbitmap, hsbitmap, hlock);
				
				r += _r;
				_u32 i = 0;
				for(i = 0; i < _count+1; i++)
					*(p_unit_number + i) = (_fsaddr)p_cxt->fs.inv_pattern;
			}
		}
	}

	if(addr_idx == 0)
		*empty = 1;

	if(hbase != (_h_buffer_)p_cxt->fs.inv_pattern) {
		if(r) {
			mgfs_buffer_dirty(p_cxt, hbase, hlock);
			/* flush here to prevent buffer overflow 
			   in case of large file deletion
			*/
			mgfs_buffer_flush(p_cxt, hbase, hlock);
		}
		mgfs_buffer_free(p_cxt, hbase, hlock);
	}

	return r;
}

/* return 0 for success */
_u32 mgfs_inode_delete(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_lock_ hlock) {
	_u32 r = __ERR;
	_mgfs_inode_handle_t hbitmap;
	_mgfs_inode_handle_t hbshadow;
	_u8 bshadow = 0;

	if(hinode->p_inode->lc) {
		hinode->p_inode->lc--;
		mgfs_inode_update(p_cxt, hinode, hlock);
	} else {
		if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_IDX, &hbitmap, hlock) == 0) {
			if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
				if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_SHADOW_IDX, &hbshadow, hlock) == 0)
					/* use bitmap shadow */
					bshadow = 1;
			}

			_u8  empty = 0;
			_u32 dunits = hinode->p_inode->dunits;
			_u32 removed = inode_remove_block(p_cxt, hinode, &hbitmap, 
							(bshadow)?&hbshadow:0, hinode->p_inode->level,
							hinode->p_inode->location[0], dunits, &empty, hlock);

			if(empty && dunits == removed) {
				if(inode_free(p_cxt, hinode->number, hlock) == 0) {
					hinode->p_inode->flags |= MGFS_DELETED;
					r = 0;
				}
			}

			hinode->p_inode->mo = p_cxt->timestamp();
			hinode->p_inode->sz = hinode->p_inode->dunits * mgfs_unit_size(p_cxt);
			mgfs_inode_update(p_cxt, hinode, hlock);

			if(bshadow)
				mgfs_inode_close(p_cxt, &hbshadow, hlock);

			mgfs_inode_close(p_cxt, &hbitmap, hlock);
		}
	}

	return r;
}

/* truncate inode to size passed in 'new_size' parameter and return the new inode size */
_u64 mgfs_inode_truncate(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 new_size, _h_lock_ hlock) {
	_u64 r = hinode->p_inode->sz;
	_mgfs_inode_handle_t hbitmap;
	_mgfs_inode_handle_t hbshadow;
	_u8 bshadow = 0;

	if(new_size < r) {
		if(mgfs_inode_meta_open(p_cxt, INODE_BITMAP_IDX, &hbitmap, hlock) == 0) {
			if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
				if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_SHADOW_IDX, &hbshadow, hlock) == 0)
					/* use bitmap shadow */
					bshadow = 1;
			}

			_u32 bcount = (r - new_size) / mgfs_unit_size(p_cxt);
			bcount -= ((r - new_size) % mgfs_unit_size(p_cxt)) ? 1 : 0;
			_u8 empty = 0;
			_u32 removed = inode_remove_block(p_cxt, hinode, &hbitmap, 
						(bshadow)?&hbshadow:0, hinode->p_inode->level,
						hinode->p_inode->location[0], bcount, &empty, hlock);
			if(removed == bcount) 
				r = hinode->p_inode->sz = new_size;
			else {
				r -= removed * mgfs_unit_size(p_cxt);
				hinode->p_inode->sz = r;
			}
			
			if(bshadow)
				mgfs_inode_close(p_cxt, &hbshadow, hlock);

			mgfs_inode_close(p_cxt, &hbitmap, hlock);
			mgfs_inode_update(p_cxt, hinode, hlock);
		}
	}

	return r;
}

