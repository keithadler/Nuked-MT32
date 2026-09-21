// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    mcs96.h

    MCS96, 8098/8398/8798 branch

***************************************************************************/

#include "emu.h"
#include "mcs96.h"

mcs96_device::mcs96_device(mt32_t* _mt32) :
	mt32(_mt32),
	icount(0), bcount(0), inst_state(0), cycles_scaling(0), pending_irq(0),
	PC(0), PPC(0), PSW(0), OP1(0), OP2(0), OP3(0), OPI(0), TMP(0), irq_requested(false)
{
}

void mcs96_device::device_start()
{
	totalcycles = 0;
	step = 0;
}

void mcs96_device::device_reset()
{
	PC = 0x2080;
	PPC = PC;
	PSW = 0;
	irq_requested = false;
	inst_state = STATE_FETCH;
}

uint32_t mcs96_device::execute_min_cycles() const noexcept
{
	return 4;
}

uint32_t mcs96_device::execute_max_cycles() const noexcept
{
	return 33;
}

void mcs96_device::recompute_bcount(uint64_t event_time)
{
	if(!event_time || event_time >= total_cycles() + icount) {
		bcount = 0;
		return;
	}
	bcount = total_cycles() + icount - event_time;
}

void mcs96_device::check_irq()
{
	check_hso_irq();
	irq_requested = (PSW & pending_irq) && (PSW & F_I);
}

void mcs96_device::int_mask_w(u8 data)
{
	PSW = (PSW & 0xff00) | data;
	check_irq();
}

u8 mcs96_device::int_mask_r()
{
	return PSW;
}

void mcs96_device::int_pending_w(u8 data)
{
	pending_irq = data;
	check_irq();
}

u8 mcs96_device::int_pending_r()
{
	return pending_irq;
}

void mcs96_device::execute_run()
{
	internal_update(total_cycles());

	//  if(inst_substate)
	//      do_exec_partial();

	while(icount > 0) {
		while(icount > bcount) {
			int picount = inst_state >= 0x200 ? -1 : icount;
			do_exec_full();
			if(icount == picount) {
				logerror("Unhandled %x (%04x)\n", inst_state, PPC);
			}
		}
		while(bcount && icount <= bcount)
			internal_update(total_cycles() + icount - bcount);
		//      if(inst_substate)
		//          do_exec_partial();
	}
}

void mcs96_device::reg_w8(u8 adr, u8 data)
{
	reg_write(adr & 0xff, data);
}

void mcs96_device::reg_w16(u8 adr, u16 data)
{
	adr &= 0xfe;
	reg_write(adr, data & 0xff);
	reg_write(adr|1, data >> 8);
}

uint8_t mcs96_device::reg_r8(uint8_t adr)
{
	return reg_read(adr & 0xff);
}

uint16_t mcs96_device::reg_r16(uint8_t adr)
{
	adr &= 0xfe;
	uint16_t v = reg_read(adr);
	v |= reg_read(adr|1)<<8;
	return v;
}

void mcs96_device::any_w8(u16 adr, u8 data)
{
	if (adr < 0x100)
		reg_w8(adr, data);
	else
		mt32->cpu_write(adr, data);
}

void mcs96_device::any_w16(u16 adr, u16 data)
{
	adr &= 0xfffe;
	if (adr < 0x100)
		reg_w16(adr, data);
	else
	{
		mt32->cpu_write(adr, data & 0xff);
		mt32->cpu_write(adr|1, data >> 8);
	}
}

u8 mcs96_device::any_r8(u16 adr)
{
	if (adr < 0x100)
		return reg_r8(adr);
	else
		return mt32->cpu_read(adr);
}

u16 mcs96_device::any_r16(u16 adr)
{
	adr &= 0xfffe;
	if (adr < 0x100)
		return reg_r16(adr);
	else
	{
		u16 v = mt32->cpu_read(adr);
		v |= mt32->cpu_read(adr|1) << 8;
		return v;
	}
}

uint8_t mcs96_device::do_addb(uint8_t v1, uint8_t v2)
{
	uint16_t sum = v1+v2;
	PSW &= ~(F_Z|F_N|F_C|F_V);
	if(!uint8_t(sum))
		PSW |= F_Z;
	else if(int8_t(sum) < 0)
		PSW |= F_N;
	if(~(v1^v2) & (v1^sum) & 0x80)
		PSW |= F_V|F_VT;
	if(sum & 0xff00)
		PSW |= F_C;
	return sum;
}

uint16_t mcs96_device::do_add(uint16_t v1, uint16_t v2)
{
	uint32_t sum = v1+v2;
	PSW &= ~(F_Z|F_N|F_C|F_V);
	if(!uint16_t(sum))
		PSW |= F_Z;
	else if(int16_t(sum) < 0)
		PSW |= F_N;
	if(~(v1^v2) & (v1^sum) & 0x8000)
		PSW |= F_V|F_VT;
	if(sum & 0xffff0000)
		PSW |= F_C;
	return sum;
}

uint8_t mcs96_device::do_subb(uint8_t v1, uint8_t v2)
{
	uint16_t diff = v1 - v2;
	PSW &= ~(F_N|F_V|F_Z|F_C);
	if(!uint8_t(diff))
		PSW |= F_Z;
	else if(int8_t(diff) < 0)
		PSW |= F_N;
	if((v1^v2) & (v1^diff) & 0x80)
		PSW |= F_V;
	if(!(diff & 0xff00))
		PSW |= F_C;
	return diff;
}

uint16_t mcs96_device::do_sub(uint16_t v1, uint16_t v2)
{
	uint32_t diff = v1 - v2;
	PSW &= ~(F_N|F_V|F_Z|F_C);
	if(!uint16_t(diff))
		PSW |= F_Z;
	else if(int16_t(diff) < 0)
		PSW |= F_N;
	if((v1^v2) & (v1^diff) & 0x8000)
		PSW |= F_V;
	if(!(diff & 0xffff0000))
		PSW |= F_C;
	return diff;
}

uint8_t mcs96_device::do_addcb(uint8_t v1, uint8_t v2)
{
	uint16_t sum = v1+v2+(PSW & F_C ? 1 : 0);
	PSW &= ~(F_Z|F_N|F_C|F_V);
	if(!uint8_t(sum))
		PSW |= F_Z;
	else if(int8_t(sum) < 0)
		PSW |= F_N;
	if(~(v1^v2) & (v1^sum) & 0x80)
		PSW |= F_V|F_VT;
	if(sum & 0xff00)
		PSW |= F_C;
	return sum;
}

uint16_t mcs96_device::do_addc(uint16_t v1, uint16_t v2)
{
	uint32_t sum = v1+v2+(PSW & F_C ? 1 : 0);
	PSW &= ~(F_Z|F_N|F_C|F_V);
	if(!uint16_t(sum))
		PSW |= F_Z;
	else if(int16_t(sum) < 0)
		PSW |= F_N;
	if(~(v1^v2) & (v1^sum) & 0x8000)
		PSW |= F_V|F_VT;
	if(sum & 0xffff0000)
		PSW |= F_C;
	return sum;
}

uint8_t mcs96_device::do_subcb(uint8_t v1, uint8_t v2)
{
	uint16_t diff = v1 - v2 - (PSW & F_C ? 0 : 1);
	PSW &= ~(F_N|F_V|F_Z|F_C);
	if(!uint8_t(diff))
		PSW |= F_Z;
	else if(int8_t(diff) < 0)
		PSW |= F_N;
	if((v1^v2) & (v1^diff) & 0x80)
		PSW |= F_V;
	if(!(diff & 0xff00))
		PSW |= F_C;
	return diff;
}

uint16_t mcs96_device::do_subc(uint16_t v1, uint16_t v2)
{
	uint32_t diff = v1 - v2 - (PSW & F_C ? 0 : 1);
	PSW &= ~(F_N|F_V|F_Z|F_C);
	if(!uint16_t(diff))
		PSW |= F_Z;
	else if(int16_t(diff) < 0)
		PSW |= F_N;
	if((v1^v2) & (v1^diff) & 0x8000)
		PSW |= F_V;
	if(!(diff & 0xffff0000))
		PSW |= F_C;
	return diff;
}

void mcs96_device::set_nz8(uint8_t v)
{
	PSW &= ~(F_N|F_V|F_Z|F_C);
	if(!v)
		PSW |= F_Z;
	else if(int8_t(v) < 0)
		PSW |= F_N;
}

void mcs96_device::set_nz16(uint16_t v)
{
	PSW &= ~(F_N|F_V|F_Z|F_C);
	if(!v)
		PSW |= F_Z;
	else if(int16_t(v) < 0)
		PSW |= F_N;
}

#include "mcs96.hxx"
