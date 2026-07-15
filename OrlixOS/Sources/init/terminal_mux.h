#ifndef ORLIX_TERMINAL_MUX_H
#define ORLIX_TERMINAL_MUX_H

#include <stddef.h>
#include <stdint.h>

#define ORLIX_TERMINAL_MUX_VERSION 1u
#define ORLIX_TERMINAL_MUX_HEADER_SIZE 8u
#define ORLIX_TERMINAL_MUX_MAX_PAYLOAD 4096u
#define ORLIX_TERMINAL_MUX_MAX_FRAME \
	(ORLIX_TERMINAL_MUX_HEADER_SIZE + ORLIX_TERMINAL_MUX_MAX_PAYLOAD)
#define ORLIX_TERMINAL_MUX_SLIP_END 0xc0u
#define ORLIX_TERMINAL_MUX_SLIP_ESC 0xdbu
#define ORLIX_TERMINAL_MUX_SLIP_ESC_END 0xdcu
#define ORLIX_TERMINAL_MUX_SLIP_ESC_ESC 0xddu

enum orlix_terminal_mux_message_type {
	ORLIX_TERMINAL_MUX_DATA = 1,
	ORLIX_TERMINAL_MUX_RESIZE = 2,
};

enum orlix_terminal_mux_event_type {
	ORLIX_TERMINAL_MUX_EVENT_DATA,
	ORLIX_TERMINAL_MUX_EVENT_RESIZE,
	ORLIX_TERMINAL_MUX_EVENT_UNSUPPORTED,
	ORLIX_TERMINAL_MUX_EVENT_PROTOCOL_ERROR,
};

enum orlix_terminal_mux_error {
	ORLIX_TERMINAL_MUX_ERROR_INVALID_ESCAPE,
	ORLIX_TERMINAL_MUX_ERROR_OVERSIZED_FRAME,
	ORLIX_TERMINAL_MUX_ERROR_TRUNCATED_HEADER,
	ORLIX_TERMINAL_MUX_ERROR_UNSUPPORTED_VERSION,
	ORLIX_TERMINAL_MUX_ERROR_INVALID_FLAGS,
	ORLIX_TERMINAL_MUX_ERROR_INVALID_LENGTH,
	ORLIX_TERMINAL_MUX_ERROR_INVALID_RESIZE,
};

struct orlix_terminal_mux_event {
	enum orlix_terminal_mux_event_type type;
	const unsigned char *data;
	size_t data_length;
	uint16_t rows;
	uint16_t columns;
	uint8_t message_type;
	enum orlix_terminal_mux_error error;
};

typedef int (*orlix_terminal_mux_event_handler)(
	const struct orlix_terminal_mux_event *event, void *context);

struct orlix_terminal_mux_decoder {
	unsigned char frame[ORLIX_TERMINAL_MUX_MAX_FRAME];
	size_t length;
	unsigned long frames;
	unsigned long protocol_errors;
	unsigned long unsupported_messages;
	unsigned int in_frame : 1;
	unsigned int escaping : 1;
	unsigned int discarding : 1;
};

void orlix_terminal_mux_decoder_init(struct orlix_terminal_mux_decoder *decoder);
int orlix_terminal_mux_decoder_feed(struct orlix_terminal_mux_decoder *decoder,
				    const unsigned char *bytes, size_t length,
				    orlix_terminal_mux_event_handler handler,
				    void *context);
int orlix_terminal_mux_decoder_finish(struct orlix_terminal_mux_decoder *decoder,
				      orlix_terminal_mux_event_handler handler,
				      void *context);

#endif
