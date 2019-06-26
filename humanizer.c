#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#ifndef __cplusplus
#    include <stdbool.h>
#endif

#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"
#include "lv2/lv2plug.in/ns/ext/patch/patch.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define HUMANIZER_URI		"https://morbidrsa.github.io/about"
#define HUMANIZER__sample	HUMANIZER_URI "#sample"
#define HUMANIZER__applySample	HUMANIZER_URI "#applySample"
#define HUMANIZER__freeSample	HUMANIZER_URI "#freeSample"

enum {
	HUMANIZER_IN  = 0,
	HUMANIZER_OUT = 1
};

typedef struct {
	LV2_URID atom_Blank;
	LV2_URID atom_Path;
	LV2_URID atom_Resource;
	LV2_URID atom_Sequence;
	LV2_URID atom_URID;
	LV2_URID atom_eventTransfer;
	LV2_URID humanizer_applySample;
	LV2_URID humanizer_sample;
	LV2_URID humanizer_freeSample;
	LV2_URID midi_Event;
	LV2_URID patch_Set;
	LV2_URID patch_property;
	LV2_URID patch_value;
} HumanizerURIs;

// Struct for a 3 byte MIDI event, used for writing notes
typedef struct {
	LV2_Atom_Event event;
	uint8_t        msg[3];
} MIDINoteEvent;

typedef struct {
	// Features
	LV2_URID_Map* map;

	// Ports
	const LV2_Atom_Sequence* in_port;
	LV2_Atom_Sequence*       out_port;

	// URIs
	HumanizerURIs uris;

	// internal state
	int offs;
	int prop;

	// Controls
	bool active;
} Humanizer;

static void
map_humanizer_uris(LV2_URID_Map* map, HumanizerURIs* uris)
{
	uris->atom_Blank		= map->map(map->handle, LV2_ATOM__Blank);
	uris->atom_Path          	= map->map(map->handle, LV2_ATOM__Path);
	uris->atom_Resource      	= map->map(map->handle, LV2_ATOM__Resource);
	uris->atom_Sequence      	= map->map(map->handle, LV2_ATOM__Sequence);
	uris->atom_URID          	= map->map(map->handle, LV2_ATOM__URID);
	uris->atom_eventTransfer 	= map->map(map->handle, LV2_ATOM__eventTransfer);
	uris->humanizer_applySample     = map->map(map->handle, HUMANIZER__applySample);
	uris->humanizer_freeSample      = map->map(map->handle, HUMANIZER__freeSample);
	uris->humanizer_sample          = map->map(map->handle, HUMANIZER__sample);
	uris->midi_Event		= map->map(map->handle, LV2_MIDI__MidiEvent);
	uris->patch_Set          	= map->map(map->handle, LV2_PATCH__Set);
	uris->patch_property     	= map->map(map->handle, LV2_PATCH__property);
	uris->patch_value        	= map->map(map->handle, LV2_PATCH__value);
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
	Humanizer* self = (Humanizer*)instance;

	switch (port) {
	case HUMANIZER_IN:
		self->in_port = (const LV2_Atom_Sequence*)data;
		break;
	case HUMANIZER_OUT:
		self->out_port = (LV2_Atom_Sequence*)data;
		break;
	default:
		break;
	}
}

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               path,
            const LV2_Feature* const* features)
{
	// Allocate and initialise instance structure.
	Humanizer* self = (Humanizer*)malloc(sizeof(Humanizer));
	if (!self) {
		return NULL;
	}
	memset(self, 0, sizeof(Humanizer));

	// Get host features
	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_URID__map)) {
			self->map = (LV2_URID_Map*)features[i]->data;
		}
	}
	if (!self->map) {
		fprintf(stderr, "Missing feature urid:map\n");
		free(self);
		return NULL;
	}

	srand(time(NULL));

	// Map URIs and initialise forge/logger
	map_humanizer_uris(self->map, &self->uris);

	self->prop = 50;
	self->offs = 10;
 
	return (LV2_Handle)self;
}

static void
cleanup(LV2_Handle instance)
{
	free(instance);
}

static void
run(LV2_Handle instance,
    uint32_t   sample_count)
{
	Humanizer*	self = (Humanizer*)instance;
	HumanizerURIs*	uris = &self->uris;
	MIDINoteEvent	humanizer;
	uint8_t		velocity;
	uint8_t		probability = 50;
	uint32_t	out_capacity;

	// Initially self->out_port contains a Chunk with size set to capacity

	// Get the capacity
	out_capacity = self->out_port->atom.size;

	// Write an empty Sequence header to the output
	lv2_atom_sequence_clear(self->out_port);
	self->out_port->atom.type = self->in_port->atom.type;

	// Read incoming events
	LV2_ATOM_SEQUENCE_FOREACH(self->in_port, ev) {
		if (ev->body.type == uris->midi_Event) {
			const uint8_t* const msg = (const uint8_t*)(ev + 1);
			switch (lv2_midi_message_type(msg)) {
			case LV2_MIDI_MSG_NOTE_ON:
			case LV2_MIDI_MSG_NOTE_OFF:

				velocity = msg[2];

				if (self->active) {
					if (velocity > 107)
						velocity = 107;

					if ((rand() % 100) <= self->prop) {
						if ((rand() % 2) == 1) {
							velocity -= rand() % self->offs;
						} else {
							velocity += rand() % self->offs;
						}
					}
				}

				humanizer.event.time.frames = ev->time.frames;  // Same time
				humanizer.event.body.type   = ev->body.type;    // Same type
				humanizer.event.body.size   = ev->body.size;    // Same size

				humanizer.msg[0] = msg[0];      // Same status
				humanizer.msg[1] = msg[1];      // Same note
				humanizer.msg[2] = velocity;    // New velocity

				lv2_atom_sequence_append_event(
							       self->out_port, out_capacity, &humanizer.event);
				break;
			default:
				// Forward all other MIDI events directly
				lv2_atom_sequence_append_event(
					self->out_port, out_capacity, ev);
				break;
			}
		}
	}
}

static void
activate(LV2_Handle instance)
{
	Humanizer *self = (Humanizer*)instance;

	self->active = true;
}

static void deactivate(LV2_Handle instance)
{
	Humanizer *self = (Humanizer*)instance;

	self->active = false;
}


static const void*
extension_data(const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	HUMANIZER_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor;
	default:
		return NULL;
	}
}
