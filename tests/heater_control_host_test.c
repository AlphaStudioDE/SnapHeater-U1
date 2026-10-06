#include <assert.h>
#include <string.h>
#include "heater_control.h"
int main(void) {
    shu1_heater_control_t s = {0}; float duty = -1;
    // Exact boundary behavior of upstream's default 1 C hysteresis.
    assert(shu1_heater_control_step(&s,55,54,false,&duty) && duty==0);
    assert(shu1_heater_control_step(&s,55,53.99f,false,&duty) && duty==1);
    for (int i=0;i<10000;i++)
        assert(shu1_heater_control_step(&s,55,54.9f,false,&duty) && duty==1);
    assert(shu1_heater_control_step(&s,55,55,false,&duty) && duty==0);
    assert(shu1_heater_control_step(&s,55,54.5f,false,&duty) && duty==0);
    assert(shu1_heater_control_step(&s,55,53,false,&duty) && duty==1);
    assert(shu1_heater_control_step(&s,55,53,true,&duty) && duty==0);
    assert(shu1_heater_control_step(&s,55,53,false,&duty) && duty==1);
    assert(shu1_heater_control_step(&s,50,53,false,&duty) && duty==0);
    shu1_heater_control_reset(&s);
    assert(shu1_heater_control_step(&s,55,54.5f,false,&duty) && duty==0);
    assert(!shu1_heater_control_step(&s,55,NAN,false,&duty) && duty==0 && !s.demand);
    assert(!shu1_heater_control_step(&s,INFINITY,25,false,&duty) && duty==0);
    assert(!shu1_heater_control_step(&s,0,25,false,&duty) && duty==0);
    assert(!shu1_heater_control_step(NULL,55,25,false,&duty) && duty==0);
    assert(!shu1_heater_control_step(&s,55,25,false,NULL));
    assert(!strcmp(shu1_heater_control_constraint(true,true,true,55,53,0),"element_foldback"));
    assert(!strcmp(shu1_heater_control_constraint(true,false,false,55,54.5f,0),"hysteresis_hold"));
    return 0;
}
