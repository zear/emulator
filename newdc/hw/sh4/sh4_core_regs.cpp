/*
	Sh4 register storage/functions/utilities
*/

#include "types.h"
#include "sh4_core.h"
#include "sh4_interrupts.h"

Sh4RCB* p_sh4rcb;
sh4_if				  sh4_cpu;
u8* sh4_dyna_rcb;

INLINE void ChangeGPR()
{
	u32 temp[8];
	for (int i=0;i<8;i++)
	{
		temp[i]=r[i];
		r[i]=r_bank[i];
		r_bank[i]=temp[i];
	}
}

INLINE void ChangeFP()
{
	u32 temp[16];
	for (int i=0;i<16;i++)
	{
		temp[i]=fr_hex[i];
		fr_hex[i]=xf_hex[i];
		xf_hex[i]=temp[i];
	}
}

//called when sr is changed and we must check for reg banks ect.. , returns true if interrupts got
bool UpdateSR()
{
	if (sr.MD)
	{
		if (old_sr.RB !=sr.RB)
			ChangeGPR();//bank change
	}
	else
	{
		if (sr.RB)
		{
			printf("UpdateSR MD=0;RB=1 , this must not happen\n");
			sr.RB =0;//error - must allways be 0
			if (old_sr.RB)
				ChangeGPR();//switch
		}
		else
		{
			if (old_sr.RB)
				ChangeGPR();//switch
		}
	}

	old_sr.status=sr.status;

	return SRdecode();
}

//make x86 and sh4 float status registers match ;)
u32 old_rm=0xFF;
u32 old_dn=0xFF;

void SetFloatStatusReg()
{
	if ((old_rm!=fpscr.RM) || (old_dn!=fpscr.DN))
	{
		old_rm=fpscr.RM ;
		old_dn=fpscr.DN ;
		u32 temp=0x1f80;	//no flush to zero && round to nearest


		//TODO: Implement this (needed for SOTB)
#if HOST_CPU==CPU_X86 && HOST_OS==OS_WINDOWS
		if (fpscr.RM==1)	//if round to 0 , set the flag
			temp|=(3<<13);

		if (fpscr.DN)		//denormals are considered 0
			temp|=(1<<15);

		_asm
		{
			ldmxcsr temp;	//load the float status :)
		}
#elif HOST_CPU==CPU_ARM
		static const unsigned int x = 0x04086060;
		unsigned int y = 0x02000000;
		if (fpscr.RM==1)	//if round to 0 , set the flag
			y|=3<<22;
	
		if (fpscr.DN)
			y|=1<<24;


		int raa;

		asm volatile
			(
				"fmrx	%0, fpscr	\n\t"
				"and	%0, %0, %1	\n\t"
				"orr	%0, %0, %2	\n\t"
				"fmxr	fpscr, %0	\n\t"
				: "=r"(raa)
				: "r"(x), "r"(y)
			);
#endif

	}
}

//called when fpscr is changed and we must check for reg banks ect..
void UpdateFPSCR()
{
	if (fpscr.FR !=old_fpscr.FR)
		ChangeFP();//fpu bank change
	old_fpscr=fpscr;
	SetFloatStatusReg();//ensure they are on sync :)
}


u32* Sh4_int_GetRegisterPtr(Sh4RegType reg)
{
	if ((reg>=reg_r0) && (reg<=reg_r15))
	{
		return &r[reg-reg_r0];
	}
	else if ((reg>=reg_r0_Bank) && (reg<=reg_r7_Bank))
	{
		return &r_bank[reg-reg_r0_Bank];
	}
	else if ((reg>=reg_fr_0) && (reg<=reg_fr_15))
	{
		return &fr_hex[reg-reg_fr_0];
	}
	else if ((reg>=reg_xf_0) && (reg<=reg_xf_15))
	{
		return &xf_hex[reg-reg_xf_0];
	}
	else
	{
		switch(reg)
		{
		case reg_gbr :
			return &gbr;
			break;
		case reg_vbr :
			return &vbr;
			break;

		case reg_ssr :
			return &ssr;
			break;

		case reg_spc :
			return &spc;
			break;

		case reg_sgr :
			return &sgr;
			break;

		case reg_dbr :
			return &dbr;
			break;

		case reg_mach :
			return &mac.h;
			break;

		case reg_macl :
			return &mac.l;
			break;

		case reg_pr :
			return &pr;
			break;

		case reg_fpul :
			return &fpul;
			break;


		case reg_nextpc :
			return &next_pc;
			break;

		case reg_old_sr_status :
			return &old_sr.status;
			break;

		case reg_sr_status :
			return &sr.status;
			break;

		case reg_sr_T :
			return &sr.T;
			break;

		case reg_old_fpscr :
			return &old_fpscr.full;
			break;

		case reg_fpscr :
			return &fpscr.full;
			break;

		case reg_pc_dyn:
			return &Sh4cntx.jdyn;

		default:
			EMUERROR2("Unkown register Id %d",reg);
			die("invalid reg");
			return 0;
			break;
		}
	}
}

u32 Sh4Context::offset(u32 sh4_reg)
{
	void* addr=Sh4_int_GetRegisterPtr((Sh4RegType)sh4_reg);
	u32 offs=(u8*)addr-(u8*)&Sh4cntx;
	verify(offs<sizeof(Sh4cntx));

	return offs;
}
