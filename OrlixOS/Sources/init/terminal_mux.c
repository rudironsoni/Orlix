#include "terminal_mux.h"

#include <string.h>

static uint16_t read_be16(const unsigned char *bytes)
{
	return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t read_be32(const unsigned char *bytes)
{
	return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
	       ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static int emit(struct orlix_terminal_mux_decoder *decoder,
		enum orlix_terminal_mux_event_type type,
		enum orlix_terminal_mux_error error, uint8_t message_type,
		const unsigned char *data, size_t data_length, uint16_t rows,
		uint16_t columns, orlix_terminal_mux_event_handler handler,
		void *context)
{
	struct orlix_terminal_mux_event event = {
		.type = type,
		.error = error,
		.message_type = message_type,
		.data = data,
		.data_length = data_length,
		.rows = rows,
		.columns = columns,
	};

	if (type == ORLIX_TERMINAL_MUX_EVENT_PROTOCOL_ERROR)
		decoder->protocol_errors++;
	else if (type == ORLIX_TERMINAL_MUX_EVENT_UNSUPPORTED)
		decoder->unsupported_messages++;
	else
		decoder->frames++;
	return handler != NULL ? handler(&event, context) : 0;
}

static int protocol_error(struct orlix_terminal_mux_decoder *decoder,
			  enum orlix_terminal_mux_error error,
			  orlix_terminal_mux_event_handler handler, void *context)
{
	return emit(decoder, ORLIX_TERMINAL_MUX_EVENT_PROTOCOL_ERROR, error, 0,
		    NULL, 0, 0, 0, handler, context);
}

static int decode_frame(struct orlix_terminal_mux_decoder *decoder,
			orlix_terminal_mux_event_handler handler, void *context)
{
	const unsigned char *frame = decoder->frame;
	uint32_t payload_length;
	uint8_t type;

	if (decoder->length < ORLIX_TERMINAL_MUX_HEADER_SIZE)
		return protocol_error(decoder,
			ORLIX_TERMINAL_MUX_ERROR_TRUNCATED_HEADER, handler, context);
	if (frame[0] != ORLIX_TERMINAL_MUX_VERSION)
		return protocol_error(decoder,
			ORLIX_TERMINAL_MUX_ERROR_UNSUPPORTED_VERSION, handler, context);
	type = frame[1];
	if (read_be16(frame + 2) != 0)
		return protocol_error(decoder, ORLIX_TERMINAL_MUX_ERROR_INVALID_FLAGS,
				      handler, context);
	payload_length = read_be32(frame + 4);
	if (payload_length > ORLIX_TERMINAL_MUX_MAX_PAYLOAD ||
	    (size_t)payload_length != decoder->length - ORLIX_TERMINAL_MUX_HEADER_SIZE)
		return protocol_error(decoder,
			ORLIX_TERMINAL_MUX_ERROR_INVALID_LENGTH, handler, context);

	if (type == ORLIX_TERMINAL_MUX_DATA)
		return emit(decoder, ORLIX_TERMINAL_MUX_EVENT_DATA, 0, type,
			    frame + ORLIX_TERMINAL_MUX_HEADER_SIZE, payload_length,
			    0, 0, handler, context);
	if (type == ORLIX_TERMINAL_MUX_RESIZE) {
		uint16_t rows;
		uint16_t columns;

		if (payload_length != 4)
			return protocol_error(decoder,
				ORLIX_TERMINAL_MUX_ERROR_INVALID_LENGTH, handler, context);
		rows = read_be16(frame + ORLIX_TERMINAL_MUX_HEADER_SIZE);
		columns = read_be16(frame + ORLIX_TERMINAL_MUX_HEADER_SIZE + 2);
		if (rows == 0 || columns == 0)
			return protocol_error(decoder,
				ORLIX_TERMINAL_MUX_ERROR_INVALID_RESIZE, handler, context);
		return emit(decoder, ORLIX_TERMINAL_MUX_EVENT_RESIZE, 0, type,
			    NULL, 0, rows, columns, handler, context);
	}

	return emit(decoder, ORLIX_TERMINAL_MUX_EVENT_UNSUPPORTED, 0, type,
		    frame + ORLIX_TERMINAL_MUX_HEADER_SIZE, payload_length,
		    0, 0, handler, context);
}

void orlix_terminal_mux_decoder_init(struct orlix_terminal_mux_decoder *decoder)
{
	memset(decoder, 0, sizeof(*decoder));
}

int orlix_terminal_mux_decoder_feed(struct orlix_terminal_mux_decoder *decoder,
				    const unsigned char *bytes, size_t length,
				    orlix_terminal_mux_event_handler handler,
				    void *context)
{
	for (size_t offset = 0; offset < length; offset++) {
		unsigned char byte = bytes[offset];

		if (byte == ORLIX_TERMINAL_MUX_SLIP_END) {
			int status = 0;

			if (decoder->escaping && !decoder->discarding)
				status = protocol_error(decoder,
					ORLIX_TERMINAL_MUX_ERROR_INVALID_ESCAPE,
					handler, context);
			else if (decoder->length > 0 && !decoder->discarding)
				status = decode_frame(decoder, handler, context);
			decoder->length = 0;
			decoder->escaping = 0;
			decoder->discarding = 0;
			decoder->in_frame = 1;
			if (status != 0)
				return status;
			continue;
		}
		if (!decoder->in_frame) {
			if (!decoder->discarding) {
				decoder->discarding = 1;
				if (protocol_error(decoder,
					    ORLIX_TERMINAL_MUX_ERROR_TRUNCATED_HEADER,
					    handler, context) != 0)
					return -1;
			}
			continue;
		}
		if (decoder->discarding)
			continue;
		if (decoder->escaping) {
			decoder->escaping = 0;
			if (byte == ORLIX_TERMINAL_MUX_SLIP_ESC_END)
				byte = ORLIX_TERMINAL_MUX_SLIP_END;
			else if (byte == ORLIX_TERMINAL_MUX_SLIP_ESC_ESC)
				byte = ORLIX_TERMINAL_MUX_SLIP_ESC;
			else {
				decoder->discarding = 1;
				if (protocol_error(decoder,
					    ORLIX_TERMINAL_MUX_ERROR_INVALID_ESCAPE,
					    handler, context) != 0)
					return -1;
				continue;
			}
		} else if (byte == ORLIX_TERMINAL_MUX_SLIP_ESC) {
			decoder->escaping = 1;
			continue;
		}
		if (decoder->length == sizeof(decoder->frame)) {
			decoder->discarding = 1;
			if (protocol_error(decoder,
				    ORLIX_TERMINAL_MUX_ERROR_OVERSIZED_FRAME,
				    handler, context) != 0)
				return -1;
			continue;
		}
		decoder->frame[decoder->length++] = byte;
	}
	return 0;
}

int orlix_terminal_mux_decoder_finish(struct orlix_terminal_mux_decoder *decoder,
				      orlix_terminal_mux_event_handler handler,
				      void *context)
{
	int status = 0;

	if ((decoder->length > 0 || decoder->escaping) && !decoder->discarding)
		status = protocol_error(decoder,
			ORLIX_TERMINAL_MUX_ERROR_TRUNCATED_HEADER, handler, context);
	decoder->length = 0;
	decoder->escaping = 0;
	decoder->discarding = 0;
	decoder->in_frame = 0;
	return status;
}
