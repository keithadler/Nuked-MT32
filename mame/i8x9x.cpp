// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    i8x9x.h

    MCS96, 8x9x branch, the original version

***************************************************************************/

#include <string.h>

#include "emu.h"
#include "i8x9x.h"

i8x9x_device::i8x9x_device(mt32_t* _mt32) :
	mcs96_device(_mt32),
	//m_ach_cb(*this, 0),
	//m_hso_cb(*this),
	//m_serial_tx_cb(*this),
	//m_in_p0_cb(*this, 0),
	//m_out_p1_cb(*this), m_in_p1_cb(*this, 0xff),
	//m_out_p2_cb(*this), m_in_p2_cb(*this, 0xc2),
	base_timer2(0), ad_done(0), hsi_mode(0), hsi_status(0), hso_command(0), ad_command(0), hso_active(0), hso_time(0), ad_result(0), pwm_control(0),
	port1(0), port2(0),
	ios0(0), ios1(0), ioc0(0), ioc1(0), extint(false),
	sbuf(0), sp_con(0), sp_stat(0), serial_send_buf(0), serial_send_timer(0), baud_reg(0), brh(false), la32_sh3(0)
{
	for (auto &hso : hso_info)
	{
		hso.command = 0;
		hso.time = 0;
	}
	hso_cam_hold.command = 0;
	hso_cam_hold.time = 0;
}

void i8x9x_device::device_start()
{
	mcs96_device::device_start();
	cycles_scaling = 3;
}

void i8x9x_device::device_reset()
{
	memset(ram, 0xff, sizeof(ram));
	mcs96_device::device_reset();
	hso_active = 0;
	hso_command = 0;
	hso_time = 0;
	timer2_reset(total_cycles());
	port1 = 0xff;
	port2 = 0xc1 & i8x9x_p2_mask(); // P2.5 is cleared
	ios0 = ios1 = 0x00;
	ioc0 &= 0xaa;
	ioc1 = (ioc1 & 0xae) | 0x01;
	ad_result = 0;
	ad_done = 0;
	pwm_control = 0x00;
	sp_con &= 0x17;
	sp_stat &= 0x80;
	serial_send_timer = 0;
	brh = false;
	//m_out_p1_cb(0xff);
	//m_out_p2_cb(0xc1);
	//m_hso_cb(0);
}

void i8x9x_device::commit_hso_cam()
{
	for(int i=0; i<8; i++)
		if(!BIT(hso_active, i)) {
			//logerror("hso cam %02x %04x in slot %d (%04x)\n", hso_command, hso_time, i, PPC);
			hso_active |= 1 << i;
			if(hso_active == 0xff)
				ios0 |= 0x40;
			hso_info[i].command = hso_command;
			hso_info[i].time = hso_time;
			internal_update(total_cycles());
			return;
		}
	ios0 |= 0xc0;
	hso_cam_hold.command = hso_command;
	hso_cam_hold.time = hso_time;
}

void i8x9x_device::ad_start(u64 current_time)
{
	ad_result = 8 | (ad_command & 7);
	if (!BIT(i8x9x_p0_mask(), ad_command & 7))
		logerror("Analog input on ACH%d does not exist on this device\n", ad_command & 7);
	else if ((ad_command & 7) != 7)
		logerror("Analog input on ACH%d not configured\n", ad_command & 7);
	else
		ad_result |= mt32->get_knob() << 6;
	//ad_done = current_time + 88;
	ad_done = current_time + 168 * cycles_scaling;
	internal_update(current_time);
}

void i8x9x_device::serial_send(u8 data)
{
	serial_send_buf = data;
	serial_send_timer = total_cycles() + 9600;
}

void i8x9x_device::serial_send_done()
{
	serial_send_timer = 0;
	//m_serial_tx_cb(serial_send_buf);
	pending_irq |= IRQ_SERIAL;
	sp_stat |= 0x20;
	check_irq();
}

void i8x9x_device::reg_write(uint8_t addr, uint8_t data)
{
	if (addr >= 0x18)
	{
		ram[addr - 0x18] = data;
		return;
	}

	switch (addr)
	{
	case 2:
		ad_command_w(data);
		break;
	case 3:
		hsi_mode_w(data);
		break;
	case 4:
		hso_time = data | (hso_time & 0xff00);
		break;
	case 5:
		hso_time_w((data << 8) | (hso_time & 0xff));
		break;
	case 6:
		hso_command_w(data);
		break;
	case 7:
		sbuf_w(data);
		break;
	case 8:
		int_mask_w(data);
		break;
	case 9:
		int_pending_w(data);
		break;
	case 10:
		watchdog_w(data);
		break;
	case 14:
		baud_rate_w(data);
		break;
	case 15:
		port1_w(data);
		break;
	case 16:
		port2_w(data);
		break;
	case 17:
		sp_con_w(data);
		break;
	case 21:
		ioc0_w(data);
		break;
	case 22:
		ioc1_w(data);
		break;
	case 23:
		pwm_control_w(data);
		break;
	default:
		break;
	}
}

uint8_t i8x9x_device::reg_read(uint8_t addr)
{
	if (addr >= 0x18)
	{
		return ram[addr - 0x18];
	}
	switch (addr)
	{
	case 0:
	case 1:
		return 0;
	case 2:
		return ad_result_r(0);
	case 3:
		return ad_result_r(1);
	case 4:
		return hsi_time_r() & 0xff;
	case 5:
		return hsi_time_r() >> 8;
	case 6:
		return hsi_status_r();
	case 7:
		return sbuf_r();
	case 8:
		return int_mask_r();
	case 9:
		return int_pending_r();
	case 10:
		return timer1_r() & 0xff;
	case 11:
		return timer1_r() >> 8;
	case 12:
		return timer2_r() & 0xff;
	case 13:
		return timer2_r() >> 8;
	case 14:
		return port0_r();
	case 15:
		return port1_r();
	case 16:
		return port2_r();
	case 17:
		return sp_stat_r();
	case 21:
		return ios0_r();
	case 22:
		return ios1_r();
	default:
		break;
	}
	return 0;
}

void i8x9x_device::ad_command_w(u8 data)
{
	ad_command = data & 0xf;
	if (ad_command & 8)
		ad_start(total_cycles());
}

u8 i8x9x_device::ad_result_r(offs_t offset)
{
	return ad_result >> (offset ? 8 : 0);
}

void i8x9x_device::hsi_mode_w(u8 data)
{
	hsi_mode = data;
	logerror("hsi_mode %02x (%04x)\n", data, PPC);
}

void i8x9x_device::hso_time_w(u16 data)
{
	hso_time = data;
	commit_hso_cam();
}

u16 i8x9x_device::hsi_time_r()
{
	//if (!machine().side_effects_disabled())
		logerror("read hsi time (%04x)\n", PPC);
	return 0x0000;
}

void i8x9x_device::hso_command_w(u8 data)
{
	hso_command = data;
}

u8 i8x9x_device::hsi_status_r()
{
	return hsi_status;
}

void i8x9x_device::sbuf_w(u8 data)
{
	//logerror("sbuf %02x (%04x)\n", data, PPC);
	serial_send(data);
}

u8 i8x9x_device::sbuf_r()
{
	//if (!machine().side_effects_disabled())
	//	logerror("read sbuf %02x (%04x)\n", sbuf, PPC);
	return sbuf;
}

void i8x9x_device::watchdog_w(u8 data)
{
	logerror("watchdog %02x (%04x)\n", data, PPC);
}

u16 i8x9x_device::timer1_r()
{
	u16 data = timer_value(1, total_cycles());
	//if (0 && !machine().side_effects_disabled())
	//	logerror("read timer1 %04x (%04x)\n", data, PPC);
	return data;
}

u16 i8x9x_device::timer2_r()
{
	u16 data = timer_value(2, total_cycles());
	//if (!machine().side_effects_disabled())
	//	logerror("read timer2 %04x (%04x)\n", data, PPC);
	return data;
}

void i8x9x_device::baud_rate_w(u8 data)
{
	if (brh)
		baud_reg = (baud_reg & 0x00ff) | u16(data) << 8;
	else
		baud_reg = (baud_reg & 0xff00) | data;
	//if (!machine().side_effects_disabled())
		brh = !brh;
}

u8 i8x9x_device::port0_r()
{
	return la32_sh3 << 4;
	/*static int last = -1;
	if (!machine().side_effects_disabled() && m_in_p0_cb() != last)
	{
		last = m_in_p0_cb();
		logerror("read p0 %02x\n", last);
	}
	return m_in_p0_cb() & i8x9x_p0_mask();*/
}

void i8x9x_device::port1_w(u8 data)
{
	if (!i8x9x_has_p1())
	{
		logerror("%s: Write %02x to nonexistent port 1\n", /*machine().describe_context()*/"", data);
		return;
	}

	port1 = data;
	//m_out_p1_cb(data);
}

u8 i8x9x_device::port1_r()
{
	if (!i8x9x_has_p1())
		return 0xff;

	return 0;
	//return m_in_p1_cb() & port1;
}

void i8x9x_device::port2_w(u8 data)
{
	data &= 0xe1 & i8x9x_p2_mask();
	port2 = data;
	//m_out_p2_cb(data);
}

u8 i8x9x_device::port2_r()
{
	// P2.0 and P2.5 are for output only (but can be read back despite what Intel claims?)
	return (/*m_in_p2_cb() | */0x25 | ~i8x9x_p2_mask()) & (port2 | (extint ? 0x1e : 0x1a));
}

void i8x9x_device::sp_con_w(u8 data)
{
	sp_con = data & 0x1f;
}

u8 i8x9x_device::sp_stat_r()
{
	u8 res = sp_stat;
	//if (!machine().side_effects_disabled())
	{
		sp_stat &= 0x80;
		//logerror("read sp stat %02x (%04x)\n", res, PPC);
	}
	return res;
}

void i8x9x_device::ioc0_w(u8 data)
{
	ioc0 = data & 0xfd;
	if (BIT(data, 1))
		timer2_reset(total_cycles());
}

u8 i8x9x_device::ios0_r()
{
	return ios0;
}

void i8x9x_device::ios0_w(u8 data)
{
	u8 mask = (data ^ ios0) & 0x3f;
	ios0 = (data & 0x3f) | (ios0 & 0xc0);
	//if (mask != 0)
	//	m_hso_cb(0, data & 0x3f, mask);
}

void i8x9x_device::ioc1_w(u8 data)
{
	ioc1 = data;
}

u8 i8x9x_device::ios1_r()
{
	u8 res = ios1;
	//if (!machine().side_effects_disabled())
		ios1 = ios1 & 0xc0;
	return res;
}

void i8x9x_device::pwm_control_w(u8 data)
{
	pwm_control = data;
}

void i8x9x_device::do_exec_partial()
{
}

void i8x9x_device::serial_w(u8 val)
{
	sbuf = val;
	sp_stat |= 0x40;
	pending_irq |= IRQ_SERIAL;
	check_irq();
}

u16 i8x9x_device::timer_value(int timer, u64 current_time) const
{
	if(timer == 2)
		current_time -= base_timer2;
	return current_time / (8 * cycles_scaling);
}

u64 i8x9x_device::timer_time_until(int timer, u64 current_time, u16 timer_value) const
{
	u64 timer_base = timer == 2 ? base_timer2 : 0;
	u64 delta = (current_time - timer_base) / (cycles_scaling * 8);
	u32 tdelta = u16(timer_value - delta);
	if(!tdelta)
		tdelta = 0x10000;
	return timer_base + ((delta + tdelta) * (cycles_scaling * 8));
}

void i8x9x_device::timer2_reset(u64 current_time)
{
	base_timer2 = current_time;
}

void i8x9x_device::set_hsi_state(int pin, bool state)
{
	if(pin == 0 && !BIT(hsi_status, 1) && state) {
		if(BIT(ioc1, 1)) {
			pending_irq |= IRQ_HSI0;
			check_irq();
		}
		if((ioc0 & 0x28) == 0x28)
			timer2_reset(total_cycles());
	}

	if(state)
		hsi_status |= 2 << (pin * 2);
	else
		hsi_status &= ~(2 << (pin * 2));
}

void i8x9x_device::trigger_cam(int id, u64 current_time)
{
	hso_cam_entry &cam = hso_info[id];
	if(hso_active == 0xff && !BIT(ios0, 7))
		ios0 &= 0xbf;
	hso_active &= ~(1 << id);
	switch(cam.command & 0x0f) {
	case 0x0: case 0x1: case 0x2: case 0x3: case 0x4: case 0x5:
		set_hso(1 << (cam.command & 7), BIT(cam.command, 5));
		break;

	case 0x6:
		set_hso(0x03, BIT(cam.command, 5));
		break;

	case 0x7:
		set_hso(0x0c, BIT(cam.command, 5));
		break;

	case 0x8: case 0x9: case 0xa: case 0xb:
		ios1 |= 1 << (cam.command & 3);
		break;

	case 0xe:
		timer2_reset(current_time);
		break;

	case 0xf:
		ad_start(current_time);
		break;

	default:
		logerror("HSO action %x undefined\n", cam.command & 0x0f);
		break;
	}

	if(BIT(cam.command, 4))
	{
		pending_irq |= BIT(cam.command, 3) ? IRQ_SOFT : IRQ_HSO;
		check_irq();
	}
}

void i8x9x_device::check_hso_irq()
{

}

void i8x9x_device::set_hso(u8 mask, bool state)
{
	if(state)
		ios0 |= mask;
	else
		ios0 &= ~mask;
	//m_hso_cb(0, ios0 & 0x3f, mask);
}

void i8x9x_device::internal_update(u64 current_time)
{
	u16 current_timer1 = timer_value(1, current_time);
	u16 current_timer2 = timer_value(2, current_time);

	int cnt = 0;

	if (current_timer1 == 0)
	{
		if (!(ios1 & 0x20))
		{
			ios1 |= 0x20;
			if (ioc1 & 0x4)
			{
				pending_irq |= IRQ_TIMER;
				check_irq();
			}
		}
	}
	if (current_timer2 == 0)
	{
		if (!(ios1 & 0x10))
		{
			ios1 |= 0x10;
			if (ioc1 & 0x8)
			{
				pending_irq |= IRQ_TIMER;
				check_irq();
			}
		}
	}

	for(int i=0; i<8; i++)
		if(BIT(hso_active, i)) {
			u8 cmd = hso_info[i].command;
			u16 t = hso_info[i].time;
			if(((cmd & 0x40) && t == current_timer2) ||
				(!(cmd & 0x40) && t == current_timer1)) {
				//logerror("hso cam %02x %04x in slot %d triggered\n", cmd, t, i);
				trigger_cam(i, current_time);
			}
		}

	if(ad_done && current_time >= ad_done) {
		// A/D conversion complete
		ad_done = 0;
		ad_result &= ~8;
		pending_irq |= IRQ_AD;
		check_irq();
	}

	if(current_time == serial_send_timer)
		serial_send_done();

	u64 event_time = 0;
	for(int i=0; i<8; i++) {
		if(!BIT(hso_active, i) && BIT(ios0, 7)) {
			hso_info[i] = hso_cam_hold;
			hso_active |= 1 << i;
			ios0 &= 0x7f;
			if(hso_active == 0xff)
				ios0 |= 0x40;
			//logerror("hso cam %02x %04x in slot %d from hold\n", hso_cam_hold.command, hso_cam_hold.time, i);
		}
		if(BIT(hso_active, i)) {
			u64 new_time = timer_time_until(hso_info[i].command & 0x40 ? 2 : 1, current_time, hso_info[i].time);
			if(!event_time || new_time < event_time)
				event_time = new_time;
		}
	}

	if(ad_done && (!event_time || ad_done < event_time))
		event_time = ad_done;

	if(serial_send_timer && (!event_time || serial_send_timer < event_time))
		event_time = serial_send_timer;

	u64 t1 = total_cycles() + (0x10000 - current_timer1) * (cycles_scaling << 3);
	current_timer2 = timer_value(2, current_time);
	u64 t2 = total_cycles() + (0x10000 - current_timer2) * (cycles_scaling << 3);


	if (!event_time || event_time > t1)
		event_time = t1;
	if (!event_time || event_time > t2)
		event_time = t2;

	recompute_bcount(event_time);
}

void i8x9x_device::execute_set_input(int linenum, int state)
{
	switch(linenum) {
	case EXTINT_LINE:
		if(!extint && state && !BIT(ioc1, 1)) {
			pending_irq |= IRQ_EXTINT;
			check_irq();
		}
		extint = state;
		break;

	case HSI0_LINE:
		set_hsi_state(0, state);
		break;

	case HSI1_LINE:
		set_hsi_state(1, state);
		break;

	case HSI2_LINE:
		set_hsi_state(2, state);
		break;

	case HSI3_LINE:
		set_hsi_state(3, state);
		break;
	}
}

p8098_device::p8098_device(mt32_t *_mt32) :
	i8x9x_device(_mt32)
{
}

#include "i8x9x.hxx"
