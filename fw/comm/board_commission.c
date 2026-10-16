#include "board_commission.h"
#include <string.h>
_Static_assert(sizeof(float)==4,"Commission codec requires binary32");
static void put_float(uint8_t *p,float value) { uint32_t bits; memcpy(&bits,&value,4); BL_Put32(p,bits); }
static float get_float(const uint8_t *p) { uint32_t bits=BL_Get32(p); float v; memcpy(&v,&bits,4); return v; }
uint8_t BoardCommission_Encode(const hw_command_t *c,uint8_t out[BL_PAYLOAD])
{
    if(!c || !out || !c->request_id || c->motor_id>4 || !c->operation || c->operation>15) return 0;
    out[0]=BC_COMMAND; BL_Put32(out+1,c->request_id); out[5]=(uint8_t)c->operation; out[6]=(uint8_t)c->motor_id;
    switch(c->operation) {
    case 1:
        put_float(out+7,c->minimum_rad); put_float(out+11,c->maximum_rad);
        put_float(out+15,c->maximum_step_rad); put_float(out+19,c->reference_rate_rad_s);
        BL_Put32(out+23,c->pwm_percent); return 27;
    case 2:
        BL_Put32(out+7,c->cpr); BL_Put32(out+11,c->quadrature); put_float(out+15,c->gear_ratio);
        BL_Put32(out+19,(uint32_t)c->direction); BL_Put32(out+23,(uint32_t)c->zero_offset);
        BL_Put32(out+27,(uint32_t)((uint64_t)c->zero_offset>>32)); return 31;
    case 7: put_float(out+7,c->target_rad); BL_Put32(out+11,c->pwm_percent); return 15;
    case 15: put_float(out+7,c->target_rad); return 11;
    case 9: case 11: out[7]=c->stationary_reference_verified?1:0; return 8;
    default: return 7;
    }
}
bool BoardCommission_Decode(const uint8_t *p,uint8_t n,hw_command_t *c)
{
    if(!p || !c || n<7 || p[0]!=BC_COMMAND || !BL_Get32(p+1) || p[6]>4 || !p[5] || p[5]>15) return false;
    *c=(hw_command_t){.request_id=BL_Get32(p+1),.operation=p[5],.motor_id=p[6]};
    switch(c->operation) {
    case 1:
        if(n!=27) return false;
        c->minimum_rad=get_float(p+7); c->maximum_rad=get_float(p+11);
        c->maximum_step_rad=get_float(p+15); c->reference_rate_rad_s=get_float(p+19);
        c->pwm_percent=BL_Get32(p+23); return true;
    case 2:
        if(n!=31) return false;
        c->cpr=BL_Get32(p+7); c->quadrature=BL_Get32(p+11); c->gear_ratio=get_float(p+15);
        c->direction=(int32_t)BL_Get32(p+19);
        c->zero_offset=(int64_t)((uint64_t)BL_Get32(p+23)|((uint64_t)BL_Get32(p+27)<<32)); return true;
    case 7:
        if(n!=15) return false;
        c->target_rad=get_float(p+7); c->pwm_percent=BL_Get32(p+11); return true;
    case 15:
        if(n!=11) return false;
        c->target_rad=get_float(p+7); return true;
    case 9: case 11:
        if(n!=8 || p[7]>1) return false;
        c->stationary_reference_verified=p[7]!=0; return true;
    default: return n==7;
    }
}
