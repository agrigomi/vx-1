#include "mp_scan.h"

/*#define _DEBUG_*/
#include "debug.h"


static _s8 checksum(_u8 *p, _u32 len) {
	_s8 r = 0;
	_u32 idx = 0;
	
	while(idx < len) {
		r += *(p + idx);
		idx++;
	}
	
	return r & 0xff;
}

typedef struct mp_scan {
	_ulong	phys_addr;
	_u32	sz;
}_mp_scan_t;

static _mp_scan_t _g_mp_scan[]={
	{0,		0x400},
	{0x9fc00,	0x400},
	{0xf0000,	0x10000},
	{0x40e << 4,	0x400}, /* BIOS EBDA */
	{0,		0}
};

static _mp_t *__g_mpf__=0;
static _mpc_t *__g_mpc__=0;
static _u8 _g_b_scan_=1;

static _s32 str_ncmp(_str_t str1, _str_t str2, _u32 sz) {
	_s32 r = 0;
	_u32 i = 0;
	_u32 _sz = sz;

	while(_sz) {
		r = str1[i] - str2[i];
		if(r)
			break;

		_sz--;
		i++;
	}
	
	return r;
}

_mp_t *mp_find_table(void) {
	_mp_t *r = NULL;

	if(__g_mpf__) /* has found */
		r = __g_mpf__;
	else {
		if(_g_b_scan_) {
			_u32 n = 0;

			INIT_DEBUG();

			while(_g_mp_scan[n].phys_addr + _g_mp_scan[n].sz) {
				_u8 *p_addr = (_u8 *)p_to_k(_g_mp_scan[n].phys_addr);
				_u32 sz = _g_mp_scan[n].sz;

				DBG("MPFT: scan at %h to %h\r\n", p_addr, p_addr+sz);

				while(sz > 0) {
					_mp_t *p_mpf = (_mp_t *)p_addr;

					if((*(_u32 *)p_addr == MPF_IDENT) && (p_mpf->len == 1) &&
							!checksum(p_addr, sizeof(_mp_t)) &&
							((p_mpf->msp_version == 1) || (p_mpf->msp_version == 4))
							) {

						r = __g_mpf__ = p_mpf;
						DBG("MPFT: MP addr=%h\r\n", p_addr);
						DBG("MPFT: MPC addr=%h\r\n", p_to_k(p_mpf->cfg_ptr));
						break;
					}

					p_addr += sizeof(_u32);
					sz -= sizeof(_u32);
				}

				if(r)
					break;

				n++;
			}
		}

		_g_b_scan_ = 0;
	}

	return r;
}

_mpc_t *mp_get_mpc(_mp_t *p_mpf) {
	_mpc_t *r = __g_mpc__;

	if(!r) {
		_mpc_t *_r = (_mpc_t *)p_to_k((_ulong)p_mpf->cfg_ptr);

		if(_r) {
			if(str_ncmp(_r->sign, MPC_SIGNATURE, 4) == 0)
				r = __g_mpc__ = _r;
		}
	}

	return r;
}

_u16 sizeof_mpc_record(_mpc_record_t *p_mpc_rec) {
	_u16 r = 0;

	switch(p_mpc_rec->type) {
		case MPC_PROCESSOR:
			r = sizeof(_mpc_cpu_t);
			break;
		case MPC_BUS:
			r = sizeof(_mpc_bus_t);
			break;
		case MPC_IOAPIC:
			r = sizeof(_mpc_ioapic_t);
			break;
		case MPC_INTSRC:
			r = sizeof(_mpc_intsrc_t);
			break;
		case MPC_LINTSRC:
			r = sizeof(_mpc_lintsrc_t);
			break;
	}

	if(r) /* add one for record type member */
		r++;

	return r;
}

#define MP_FIRST_RECORD(mpc_ptr) \
	(_mpc_record_t *)(((_u8 *)mpc_ptr) + sizeof(_mpc_t))

#define MP_NEXT_RECORD(current_record) \
	(_mpc_record_t *)(((_u8 *)current_record) + sizeof_mpc_record(current_record))

#define MP_VALID_RECORD(mpc_ptr, current_record) \
	(sizeof_mpc_record(current_record) && (((_u8 *)current_record) < (((_u8 *)mpc_ptr) + mpc_ptr->len)))

_u16 mp_cpu_count(_mp_t *p_mpf) {
	_u32 r = 0;
	_mpc_t *p_mpc = mp_get_mpc(p_mpf);

	if(p_mpc) {
		/* found MP configuration */
		_mpc_record_t *p_record = MP_FIRST_RECORD(p_mpc);

		while(MP_VALID_RECORD(p_mpc, p_record)) {
			if(p_record->type == MPC_PROCESSOR)
				r++;

			p_record = MP_NEXT_RECORD(p_record);
		}
	}

	return r;
}

_mpc_cpu_t *mp_get_cpu(_mp_t *p_mpf, _u16 cpu_idx) {
	_mpc_cpu_t *r = NULL;
	_u16 idx = 0;
	_mpc_t *p_mpc = mp_get_mpc(p_mpf);

	if(p_mpc) {
		/* found MP configuration */
		_mpc_record_t *p_record = MP_FIRST_RECORD(p_mpc);

		 while(MP_VALID_RECORD(p_mpc, p_record)) {
			if(p_record->type == MPC_PROCESSOR) {
				if(idx == cpu_idx) {
					r = &(p_record->data.cpu);
					break;
				}

				idx++;
			}

			p_record = MP_NEXT_RECORD(p_record);
		}
	}

	return r;
}

_mpc_ioapic_t *mp_get_ioapic(_mp_t *p_mpf, _u16 ioapic_idx) {
	_mpc_ioapic_t *r = NULL;
	_u16 idx = 0;
	_mpc_t *p_mpc = mp_get_mpc(p_mpf);

	if(p_mpc) {
		_mpc_record_t *p_record = MP_FIRST_RECORD(p_mpc);

		while(MP_VALID_RECORD(p_mpc, p_record)) {
			if(p_record->type == MPC_IOAPIC) {
				if(idx == ioapic_idx) {
					r = &(p_record->data.ioapic);
					break;
				}

				idx++;
			}

			p_record = MP_NEXT_RECORD(p_record);
		}
	}

	return r;
}

_mpc_bus_t *mp_get_bus(_mp_t *p_mpf, _u16 bus_idx) {
	_mpc_bus_t *r = NULL;

	_u16 idx = 0;
	_mpc_t *p_mpc = mp_get_mpc(p_mpf);

	if(p_mpc) {
		_mpc_record_t *p_record = MP_FIRST_RECORD(p_mpc);
		while(MP_VALID_RECORD(p_mpc, p_record)) {
			if(p_record->type == MPC_BUS) {
				if(idx == bus_idx) {
					r = &(p_record->data.bus);
					break;
				}

				idx++;
			}

			p_record = MP_NEXT_RECORD(p_record);
		}
	}

	return r;
}

_mpc_intsrc_t *mp_get_intsrc(_mp_t *p_mpf, _u16 intsrc_idx) {
	_mpc_intsrc_t *r = NULL;

	_u16 idx = 0;
	_mpc_t *p_mpc = mp_get_mpc(p_mpf);

	if(p_mpc) {
		_mpc_record_t *p_record = MP_FIRST_RECORD(p_mpc);

		while(MP_VALID_RECORD(p_mpc, p_record)) {
			if(p_record->type == MPC_INTSRC) {
				if(idx == intsrc_idx) {
					r = &(p_record->data.intsrc);
					break;
				}

				idx++;
			}

			p_record = MP_NEXT_RECORD(p_record);
		}
	}

	return r;
}

_mpc_lintsrc_t *mp_get_lintsrc(_mp_t *p_mpf, _u16 lintsrc_idx) {
	_mpc_lintsrc_t *r = NULL;

	_u16 idx = 0;
	_mpc_t *p_mpc = mp_get_mpc(p_mpf);

	if(p_mpc) {
		_mpc_record_t *p_record = MP_FIRST_RECORD(p_mpc);

		while(MP_VALID_RECORD(p_mpc, p_record)) {
			if(p_record->type == MPC_LINTSRC) {
				if(idx == lintsrc_idx) {
					r = &(p_record->data.lintsrc);
					break;
				}

				idx++;
			}

			p_record = MP_NEXT_RECORD(p_record);
		}
	}

	return r;
}

