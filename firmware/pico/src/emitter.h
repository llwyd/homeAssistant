#ifndef EMITTER_H_
#define EMITTER_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "events.h"
#include "state.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/critical_section.h"

extern void Emitter_Init( event_fifo_t * event_fifo, critical_section_t * crit );
extern void Emitter_Create( event_t event, struct repeating_timer * timer, int32_t period ); 
extern void Emitter_Destroy( struct repeating_timer * timer );
extern bool Emitter_EmitEvent(event_t e);

#endif /* EMITTER_H_ */
