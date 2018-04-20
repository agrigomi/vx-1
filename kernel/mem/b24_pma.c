#include "vxmod.h"
#include "i_memory.h"
#include "startup_context.h"
#include "err.h"
#include "mutex.h"

#define MAX_REC_AMOUNT	24
#define MAX_RAM_BLOCKS	20

typedef union {
	_u32	rec;
	struct __attribute__((packed)) {
		_u32	bmp	:MAX_REC_AMOUNT;
		_u32	amount	:5;
		_u32	type	:3;
	};
}__attribute__((packed)) _rec_t;

typedef struct {
	_ulong	addr; /* address of first page */
	_ulong	nrec; /* number of records */
	_rec_t	*p_rec; /* pointer to first record */
	_ulong	nfree; /* number of free pages */
}_ram_t;

typedef struct {
	_u64	addr;
	_u64	size;
}_ram_part_t;

extern _core_startup_t *__g_p_core_startup__; /* core startup context (from boot loader) */
static _mutex_t		_mutex;
static _ulong		_vbase = 0;	/* kernel address space */
static _u16		_page_size = PMA_PAGE_SIZE;
static _ram_t		_ram[MAX_RAM_BLOCKS];
static _u16		_nram = 0;

static _u8 address_to_position(_ulong addr, /* in */
				_ram_t **pp_block, /* out */
				_s16	*p_block_idx, /* out */
				_rec_t **pp_rec, /* out */
				_ulong	*p_rec_idx, /* out */
				_u8     *p_bit /* out */
				) {
	_u8 r = 0;
	_s16 i = 0;

	/* allign address */
	_ulong _addr = addr - (addr % _page_size);
	/* search block */
	for(; i < _nram; i++) {
		_ulong block_end = _ram[i].addr + ((_ram[i].nrec * MAX_REC_AMOUNT) * _page_size);
		if(_ram[i].addr <= _addr && _addr < block_end) {
			_ram_t *p_block = &_ram[i];
			_s16 block_idx = i;
			_ulong block_page = (_addr - _ram[i].addr) / _page_size; /* block_page */
			_ulong rec_idx = block_page / MAX_REC_AMOUNT; /* record index */
			_rec_t *p_rec = &(_ram[i].p_rec[rec_idx]);
			_u8 bit = block_page % MAX_REC_AMOUNT; /* record bit */

			*pp_block = p_block;
			*pp_rec = p_rec;
			*p_block_idx = block_idx;
			*p_rec_idx = rec_idx;
			*p_bit = bit;

			r = 1;
			break;
		}
	}

	return r;
}

static _ulong block_index_to_address(_s16 block_idx, _ulong rec_idx, _u8 bit) {
	_ulong r = 0;

	if(block_idx < _nram) {
		if(rec_idx < _ram[block_idx].nrec) { 
			_ulong base = _ram[block_idx].addr + ((rec_idx * MAX_REC_AMOUNT) * _page_size);
			r = base + (bit * _page_size);
		}
	}
	return r;
}

static _u32 set_bits(_ram_t *p_block, _rec_t *p_rec, _u8 bit, _u32 count, _u8 type, _u8 set) {
	_rec_t *rec = p_rec;
	_rec_t *rec_end = (p_block->p_rec + p_block->nrec);
	_u32 cnt = 0;
	_ulong mask = (1<<(MAX_REC_AMOUNT-1));

	mask >>= bit;

	while(cnt < count && rec < rec_end) {
		rec->type = type;
		while(mask && cnt < count) {
			if(set) {
				if(!(rec->bmp & mask)) {
					rec->bmp |= mask;
					rec->amount++;
					p_block->nfree--;
				}
			} else {
				if(rec->bmp & mask) {
					rec->bmp &= ~mask;
					rec->amount--;
					p_block->nfree++;
				}
			}
			mask >>= 1;
			cnt++;
		}
		rec++;
		mask = (1 << (MAX_REC_AMOUNT - 1));
	}

	return cnt;
}

static _u8 find(_ulong limit, _u8 type, _u32 count, /* in */
			_ram_t **pp_block, _s16 *p_bidx, /* out */
			_rec_t **pp_rec, _ulong *p_ridx, _u8 *p_bit /* out */
		  ) {
	_u8 r = 0;

	if(_nram) {
		_s16 block_idx = _nram - 1;
		_ram_t *p_block = &_ram[block_idx];
		_ulong rec_idx = p_block->nrec-1;
		_rec_t *p_rec = &(p_block->p_rec[rec_idx]);
		_u8 bit = 0xff;
		_u32 cnt = 0;
		_ulong end_block_addr = (p_block->addr + ((p_block->nrec * MAX_REC_AMOUNT) * _page_size))-1;
		_ulong _limit = limit;

		while(_limit <= p_block->addr) {
			if(block_idx) {
				block_idx--;
				p_block = &_ram[block_idx];
				rec_idx = p_block->nrec-1;
				p_rec = &(p_block->p_rec[rec_idx]);
				end_block_addr = (p_block->addr + ((p_block->nrec * MAX_REC_AMOUNT) * _page_size))-1;
			} else
				return 0;
		}

		_u8 shift = 0;
		if(_limit > end_block_addr) {
			_limit = end_block_addr;
			shift = 1;
		}

		if(!address_to_position(_limit, &p_block, &block_idx, &p_rec, &rec_idx, &bit))
			return 0;

		_ulong mask = 1;
		if(bit != 0xff)
			mask = (1 << (MAX_REC_AMOUNT-shift)) >> bit;

		while(p_block >= &_ram[0]) {
			while(p_rec >= p_block->p_rec) {
				if((p_rec->type == 0 || p_rec->type == type) && p_rec->amount < MAX_REC_AMOUNT) {
					_ulong v = p_rec->bmp;
					while(mask < (1 << MAX_REC_AMOUNT)) {
						if(!(v & mask)) {
							cnt++;
							if(cnt == count)
								goto _find_done_;
						} else
							cnt = 0;

						mask <<= 1;
					}
				} else
					cnt = 0;

				if(rec_idx) {
					rec_idx--;
					p_rec = &(p_block->p_rec[rec_idx]);
					mask = 1;
				} else
					break;
			}

			if(block_idx) {
				block_idx--;
				p_block = &_ram[block_idx];
				rec_idx = p_block->nrec-1;
				p_rec = &(p_block->p_rec[rec_idx]);
				mask = 1;
				cnt = 0;
			} else
				break;
		}
_find_done_:
		if(cnt == count) {
			*pp_block = p_block;
			*pp_rec = p_rec;
			*p_bidx = block_idx;
			*p_ridx = rec_idx;
			_ulong m = (1 << (MAX_REC_AMOUNT-1));
			for(bit = 0; bit < MAX_REC_AMOUNT; bit++) {
				if(m & mask) {
					*p_bit = bit;
					break;
				}
				m >>= 1;
			}
			r = 1;
		}
	}

	return r;
}


static HMUTEX lock(HMUTEX hlock) {
	return mutex_lock(&_mutex, hlock, _MUTEX_TIMEOUT_INFINITE_, 0);
}

static void unlock(HMUTEX hlock) {
	mutex_unlock(&_mutex, hlock);
}

static void init_ram_block(_ulong addr, _ulong size) {
	if(size > ((MAX_REC_AMOUNT + 1)*_page_size)) {
		/* allign ram address */
		_u32 rem = _page_size - (addr % _page_size);
		_ulong _addr = addr + rem;
		_ulong _size = size - rem;
		_u8 *p = NULL;
		_u32 _szclr = 0;
		_u32 i = 0;

		_ulong ram_pages = _size / _page_size; /* number of pages */
		_ulong records = ram_pages / MAX_REC_AMOUNT;
		_ulong rec_mem = (records * sizeof(_rec_t));
		_ulong rec_pages = rec_mem / _page_size;
		rec_pages += (rec_mem % _page_size)?1:0;

		ram_pages -= rec_pages;

		records = ram_pages / MAX_REC_AMOUNT;
		rec_mem = records * sizeof(_rec_t);
		rec_pages = rec_mem / _page_size;
		rec_pages += (rec_mem % _page_size)?1:0;

		/* clear record area */
		_szclr = rec_pages * _page_size;
		p = (_u8 *)_addr;
		for(i = 0; i < _szclr; i++)
			*p = 0;

		_ram[_nram].addr  = _addr + (rec_pages * _page_size);
		_ram[_nram].nrec  = records;
		_ram[_nram].p_rec = (_rec_t *)_addr;
		_ram[_nram].nfree = records * MAX_REC_AMOUNT;
		_nram++;
	}
}

static void pma_init(_ram_part_t *p_ram_array, _u16 nparts) {
	_u16 i = 0;

	_nram = 0;
	_u8 *p_ram = (_u8 *)_ram;
	for(; i < sizeof(_ram); i++)
		p_ram[i] = 0;

	mutex_reset(&_mutex);

	for(i = 0; i < nparts; i++)
		init_ram_block(p_ram_array[i].addr, p_ram_array[i].size);
}

static void pma_early_init(void) {
	_vbase = (_ulong)__g_p_core_startup__->vbase;
	_page_size = __g_p_core_startup__->pt_page_size;
	_ram_part_t ram[MAX_RAM_BLOCKS];
	_u32 ram_idx = 0;
	_u32 i = 0, j = 0;

	for(; i < __g_p_core_startup__->mm_cnt; i++) {
		if(__g_p_core_startup__->mmap[i].type == MEM_TYPE_FREE) {
			ram[ram_idx].addr = __g_p_core_startup__->mmap[i].address;
			ram[ram_idx].size = __g_p_core_startup__->mmap[i].size;

			for(j = 0; j < __g_p_core_startup__->rm_cnt; j++) {
				_mem_tag_t *p_rt = &__g_p_core_startup__->rmap[j];
				if(ram[ram_idx].addr == p_rt->address && p_rt->size <= ram[ram_idx].size) {
					ram[ram_idx].addr += p_rt->size;
					ram[ram_idx].size -= p_rt->size;
				}
			}

			ram[ram_idx].addr += _vbase;
			ram_idx++;
		}
	}

	pma_init(ram, ram_idx);
}

static void *pma_alloc_seq(_u32 npages, _u8 type, _ulong limit) {
	void *r = NULL;
	_ram_t *p_block=0;
	_rec_t *p_rec=0;
	_s16 block_idx = 0;
	_ulong rec_idx = 0;
	_u8 bit=0;

	HMUTEX hlock = lock(0);

	if(find(limit, type, npages, &p_block, &block_idx, &p_rec, &rec_idx, &bit)) {
		_u32 _r = set_bits(p_block, p_rec, bit, npages, type, 1);
		if(_r == npages)
			r = (void *)block_index_to_address(block_idx, rec_idx, bit);
		else /* rollback */
			set_bits(p_block, p_rec, bit, _r, 0, 0);
	}

	unlock(hlock);

	return r;
}

static _u32 pma_free_seq(void *ptr, _u32 npages) {
	_u32 r = 0;
	_ram_t *p_block=0;
	_rec_t *p_rec=0;
	_s16 block_idx = 0;
	_ulong rec_idx = 0;
	_u8 bit=0;

	HMUTEX hlock = lock(0);

	if(address_to_position((_ulong)ptr, &p_block, &block_idx, &p_rec, &rec_idx, &bit))
		r = set_bits(p_block, p_rec, bit, npages, p_rec->type, 0);

	unlock(hlock);

	return r;
}

static _u32 pma_alloc_non_seq(/* in */_u32 npages, /* number of pages to alloc */
		   /* out */_ulong *page_array, /* array of page addresses in PM */
		   /* in */_u8 type,
	  	   /* in */ _ulong limit
		  ) {
	_u32 r = 0;
	_u32 i = 0;
	HMUTEX hlock = lock(0);

	for(i = 0; i < npages; i++) {
		_ram_t *p_block=0;
		_rec_t *p_rec=0;
		_s16 block_idx = 0;
		_ulong rec_idx = 0;
		_u8 bit=0;

		_ulong _limit = limit;
		if(find(_limit, type, 1, &p_block, &block_idx, &p_rec, &rec_idx, &bit)) {
			if(set_bits(p_block, p_rec, bit, 1, type, 1) == 1) {
				_ulong addr = block_index_to_address(block_idx, rec_idx, bit);
				if(addr) {
					_limit = page_array[i] = addr;
					r = i;
				} else
					break;
			} else
				break;
		} else
			break;
	}
	unlock(hlock);

	return r;
}

static _u32 pma_free_non_seq(_u32 npages, _ulong *page_array) {
	_u32 r = 0;
	_u32 i = 0;
	HMUTEX hlock = lock(0);

	for(i = 0; i < npages; i++) {
		_ram_t *p_block=0;
		_rec_t *p_rec=0;
		_s16 block_idx = 0;
		_ulong rec_idx = 0;
		_u8 bit=0;

		if(address_to_position(page_array[i], &p_block, &block_idx, &p_rec, &rec_idx, &bit)) {
			if(set_bits(p_block, p_rec, bit, 1, p_rec->type, 0) != 1)
				break;
		} else
			break;
	}
	unlock(hlock);
	return r;
}

static void pma_info(_ram_info_t *p_info) {
	_u32 i = 0;
	HMUTEX hlock = lock(0);

	p_info->ps = _page_size;
	p_info->pr = 0;
	p_info->pu = 0;
	p_info->pp = 0;
	for(i = 0; i < _nram; i++) {
		p_info->pr += (_ram[i].nrec * MAX_REC_AMOUNT);
		p_info->pu += ((_ram[i].nrec * MAX_REC_AMOUNT) - _ram[i].nfree);
		p_info->pp += ((_ram[i].nrec * sizeof(_rec_t)) / _page_size);
		p_info->pp += ((_ram[i].nrec * sizeof(_rec_t)) % _page_size)?1:0;
	}

	unlock(hlock);
}

static _i_pma_t _pma_public_ = {
	.alloc_seq	= pma_alloc_seq,
	.free_seq	= pma_free_seq,
	.alloc_non_seq	= pma_alloc_non_seq,
	.free_non_seq	= pma_free_non_seq,
	.info		= pma_info
};

static _u32 pma_ctl(_u32 cmd, ...) {
	_u32 r = VX_ERR;

	switch(cmd) {
		case MODCTL_EARLY_INIT:
			pma_early_init();
			r = VX_OK;
			break;
		case MODCTL_INIT_CONTEXT:
		case MODCTL_DESTROY_CONTEXT:
			r = VX_OK;
			break;
	}

	return r;
}

DEF_VXMOD(
	MOD_B24_PMA,
	I_PMA, 
	&_pma_public_, /* interface */ 
	NULL, /* data context */
	0, /* sizeof data context */
	pma_ctl, /* module controll */
	1, 0, 1, /* version */ 
	"physical memory allocator (24 bit)"
);
