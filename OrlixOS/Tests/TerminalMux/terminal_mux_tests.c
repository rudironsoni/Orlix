#include "../../Sources/init/terminal_mux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct observed_event {
	enum orlix_terminal_mux_event_type type;
	unsigned char data[ORLIX_TERMINAL_MUX_MAX_PAYLOAD];
	size_t length;
	uint16_t rows;
	uint16_t columns;
	uint8_t message_type;
	enum orlix_terminal_mux_error error;
};

struct observations {
	struct observed_event events[64];
	size_t count;
};

struct fake_pty {
	unsigned char input[256];
	size_t input_length;
	size_t resize_input_offset;
	unsigned int tiocswinsz_count;
	uint16_t rows;
	uint16_t columns;
	unsigned int protocol_errors;
};

static void fail(const char *test, const char *detail)
{
	fprintf(stderr, "FAIL %s: %s\n", test, detail);
	exit(1);
}

#define CHECK(test, condition, detail) \
	do { if (!(condition)) fail((test), (detail)); } while (0)

static int observe(const struct orlix_terminal_mux_event *event, void *opaque)
{
	struct observations *observations = opaque;
	struct observed_event *record;

	if (observations->count == ARRAY_SIZE(observations->events))
		return -1;
	record = &observations->events[observations->count++];
	memset(record, 0, sizeof(*record));
	record->type = event->type;
	record->length = event->data_length;
	record->rows = event->rows;
	record->columns = event->columns;
	record->message_type = event->message_type;
	record->error = event->error;
	if (event->data_length > sizeof(record->data))
		return -1;
	if (event->data_length > 0)
		memcpy(record->data, event->data, event->data_length);
	return 0;
}

static int dispatch_to_fake_pty(const struct orlix_terminal_mux_event *event,
				void *opaque)
{
	struct fake_pty *pty = opaque;

	if (event->type == ORLIX_TERMINAL_MUX_EVENT_DATA) {
		if (pty->input_length + event->data_length > sizeof(pty->input))
			return -1;
		memcpy(pty->input + pty->input_length, event->data,
		       event->data_length);
		pty->input_length += event->data_length;
	} else if (event->type == ORLIX_TERMINAL_MUX_EVENT_RESIZE) {
		pty->resize_input_offset = pty->input_length;
		pty->tiocswinsz_count++;
		pty->rows = event->rows;
		pty->columns = event->columns;
	} else if (event->type == ORLIX_TERMINAL_MUX_EVENT_PROTOCOL_ERROR) {
		pty->protocol_errors++;
	}
	return 0;
}

static size_t encode_frame(uint8_t version, uint8_t type, uint16_t flags,
			   const unsigned char *payload, uint32_t payload_length,
			   unsigned char *output, size_t capacity)
{
	unsigned char frame[ORLIX_TERMINAL_MUX_MAX_FRAME];
	size_t frame_length = ORLIX_TERMINAL_MUX_HEADER_SIZE + payload_length;
	size_t output_length = 0;

	if (frame_length > sizeof(frame))
		return 0;
	frame[0] = version;
	frame[1] = type;
	frame[2] = (unsigned char)(flags >> 8);
	frame[3] = (unsigned char)flags;
	frame[4] = (unsigned char)(payload_length >> 24);
	frame[5] = (unsigned char)(payload_length >> 16);
	frame[6] = (unsigned char)(payload_length >> 8);
	frame[7] = (unsigned char)payload_length;
	if (payload_length > 0)
		memcpy(frame + ORLIX_TERMINAL_MUX_HEADER_SIZE, payload,
		       payload_length);
	if (capacity == 0)
		return 0;
	output[output_length++] = ORLIX_TERMINAL_MUX_SLIP_END;
	for (size_t index = 0; index < frame_length; index++) {
		unsigned char byte = frame[index];
		if (byte == ORLIX_TERMINAL_MUX_SLIP_END ||
		    byte == ORLIX_TERMINAL_MUX_SLIP_ESC) {
			if (output_length + 2 > capacity)
				return 0;
			output[output_length++] = ORLIX_TERMINAL_MUX_SLIP_ESC;
			output[output_length++] = byte == ORLIX_TERMINAL_MUX_SLIP_END ?
				ORLIX_TERMINAL_MUX_SLIP_ESC_END :
				ORLIX_TERMINAL_MUX_SLIP_ESC_ESC;
		} else {
			if (output_length == capacity)
				return 0;
			output[output_length++] = byte;
		}
	}
	if (output_length == capacity)
		return 0;
	output[output_length++] = ORLIX_TERMINAL_MUX_SLIP_END;
	return output_length;
}

static size_t resize_frame(uint16_t rows, uint16_t columns,
			   unsigned char *output, size_t capacity)
{
	unsigned char payload[] = {
		(unsigned char)(rows >> 8), (unsigned char)rows,
		(unsigned char)(columns >> 8), (unsigned char)columns,
	};
	return encode_frame(1, ORLIX_TERMINAL_MUX_RESIZE, 0, payload,
			    sizeof(payload), output, capacity);
}

static void check_resize(const char *test, const struct observations *observed,
			 uint16_t rows, uint16_t columns)
{
	CHECK(test, observed->count == 1, "expected exactly one event");
	CHECK(test, observed->events[0].type == ORLIX_TERMINAL_MUX_EVENT_RESIZE,
	      "expected resize event");
	CHECK(test, observed->events[0].rows == rows, "wrong rows");
	CHECK(test, observed->events[0].columns == columns, "wrong columns");
}

static void test_byte_at_a_time_and_every_split(void)
{
	const char *test = "byte-at-a-time and every split";
	unsigned char frame[64];
	size_t length = resize_frame(40, 120, frame, sizeof(frame));

	for (size_t split = 0; split <= length; split++) {
		struct orlix_terminal_mux_decoder decoder;
		struct observations observed = {0};
		orlix_terminal_mux_decoder_init(&decoder);
		CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, frame, split,
			observe, &observed) == 0, "first fragment failed");
		CHECK(test, observed.count == 0 || split == length,
		      "event emitted before complete frame");
		CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, frame + split,
			length - split, observe, &observed) == 0,
		      "second fragment failed");
		check_resize(test, &observed, 40, 120);
	}

	struct orlix_terminal_mux_decoder decoder;
	struct observations observed = {0};
	orlix_terminal_mux_decoder_init(&decoder);
	for (size_t index = 0; index < length; index++) {
		CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, frame + index, 1,
			observe, &observed) == 0, "byte feed failed");
		CHECK(test, observed.count == 0 || index + 1 == length,
		      "resize emitted before final byte");
	}
	check_resize(test, &observed, 40, 120);
}

static void test_ordering_and_coalescing(void)
{
	const char *test = "ordering and coalescing";
	unsigned char first[64], resize[64], last[64], stream[256];
	const unsigned char abc[] = "abc";
	const unsigned char def[] = "def";
	size_t first_length = encode_frame(1, ORLIX_TERMINAL_MUX_DATA, 0, abc, 3,
		first, sizeof(first));
	size_t resize_length = resize_frame(55, 144, resize, sizeof(resize));
	size_t last_length = encode_frame(1, ORLIX_TERMINAL_MUX_DATA, 0, def, 3,
		last, sizeof(last));
	size_t length = 0;
	struct orlix_terminal_mux_decoder decoder;
	struct observations observed = {0};

	memcpy(stream + length, first, first_length); length += first_length;
	memcpy(stream + length, resize, resize_length); length += resize_length;
	memcpy(stream + length, last, last_length); length += last_length;
	orlix_terminal_mux_decoder_init(&decoder);
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, stream, length,
		observe, &observed) == 0, "coalesced feed failed");
	CHECK(test, observed.count == 3, "wrong event count");
	CHECK(test, observed.events[0].type == ORLIX_TERMINAL_MUX_EVENT_DATA &&
	      memcmp(observed.events[0].data, abc, 3) == 0, "leading data changed");
	CHECK(test, observed.events[1].type == ORLIX_TERMINAL_MUX_EVENT_RESIZE &&
	      observed.events[1].rows == 55 && observed.events[1].columns == 144,
	      "resize reordered or changed");
	CHECK(test, observed.events[2].type == ORLIX_TERMINAL_MUX_EVENT_DATA &&
	      memcmp(observed.events[2].data, def, 3) == 0, "trailing data changed");
}

static void test_original_fragmentation_regression(void)
{
	const char *test = "original fragmentation regression";
	const unsigned char before[] = {0x00, 'a', 0xc0, 'b'};
	const unsigned char after[] = {'c', 0xdb, 'd', 0xff};
	unsigned char before_frame[64], resize[64], after_frame[64];
	size_t before_length = encode_frame(1, ORLIX_TERMINAL_MUX_DATA, 0,
		before, sizeof(before), before_frame, sizeof(before_frame));
	size_t resize_length = resize_frame(43, 137, resize, sizeof(resize));
	size_t after_length = encode_frame(1, ORLIX_TERMINAL_MUX_DATA, 0,
		after, sizeof(after), after_frame, sizeof(after_frame));
	struct orlix_terminal_mux_decoder decoder;
	struct fake_pty pty = {0};

	orlix_terminal_mux_decoder_init(&decoder);
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, before_frame,
		before_length, dispatch_to_fake_pty, &pty) == 0, "leading data failed");
	CHECK(test, pty.input_length == sizeof(before) &&
	      memcmp(pty.input, before, sizeof(before)) == 0,
	      "leading bytes changed");
	for (size_t index = 0; index < resize_length; index++) {
		CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, resize + index, 1,
			dispatch_to_fake_pty, &pty) == 0, "fragmented resize failed");
		CHECK(test, pty.tiocswinsz_count ==
		      (index + 1 == resize_length ? 1u : 0u),
		      "resize emitted at wrong byte");
	}
	CHECK(test, pty.rows == 43 && pty.columns == 137 &&
	      pty.resize_input_offset == sizeof(before),
	      "resize dimensions changed");
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, after_frame,
		after_length, dispatch_to_fake_pty, &pty) == 0, "trailing data failed");
	CHECK(test, pty.input_length == sizeof(before) + sizeof(after) &&
	      memcmp(pty.input + sizeof(before), after, sizeof(after)) == 0,
	      "trailing bytes changed or control bytes leaked");
	CHECK(test, pty.protocol_errors == 0, "valid stream reported an error");
}

static void test_multiple_control_frames(void)
{
	const char *test = "multiple control frames";
	unsigned char stream[256];
	size_t length = 0;
	struct orlix_terminal_mux_decoder decoder;
	struct observations observed = {0};

	length += resize_frame(1, 1, stream + length, sizeof(stream) - length);
	length += resize_frame(24, 80, stream + length, sizeof(stream) - length);
	length += resize_frame(65535, 65535, stream + length,
			       sizeof(stream) - length);
	orlix_terminal_mux_decoder_init(&decoder);
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, stream, length,
		observe, &observed) == 0, "coalesced controls failed");
	CHECK(test, observed.count == 3, "control count changed");
	CHECK(test, observed.events[0].rows == 1 &&
	      observed.events[1].rows == 24 && observed.events[2].rows == 65535,
	      "control order changed");
}

static void test_arbitrary_binary_and_collisions(void)
{
	const char *test = "arbitrary binary and collisions";
	unsigned char payload[256], frame[600];
	struct orlix_terminal_mux_decoder decoder;
	struct observations observed = {0};

	for (size_t index = 0; index < sizeof(payload); index++)
		payload[index] = (unsigned char)index;
	size_t length = encode_frame(1, ORLIX_TERMINAL_MUX_DATA, 0, payload,
		(unsigned int)sizeof(payload), frame, sizeof(frame));
	orlix_terminal_mux_decoder_init(&decoder);
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, frame, length,
		observe, &observed) == 0, "binary feed failed");
	CHECK(test, observed.count == 1 && observed.events[0].length == sizeof(payload),
	      "binary payload length changed");
	CHECK(test, memcmp(observed.events[0].data, payload, sizeof(payload)) == 0,
	      "binary payload changed");

	const unsigned char collision[] = {0xc0, 0xdb, 0xc0, 0xdb, 0xdb, 0x00};
	length = encode_frame(1, ORLIX_TERMINAL_MUX_DATA, 0, collision,
			      sizeof(collision), frame, sizeof(frame));
	memset(&observed, 0, sizeof(observed));
	orlix_terminal_mux_decoder_init(&decoder);
	for (size_t index = 0; index < length; index++)
		CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, frame + index, 1,
			observe, &observed) == 0, "fragmented escape failed");
	CHECK(test, observed.count == 1 && observed.events[0].length == sizeof(collision),
	      "collision payload length changed");
	CHECK(test, memcmp(observed.events[0].data, collision, sizeof(collision)) == 0,
	      "collision payload changed");
}

static void expect_error_frame(const char *test, unsigned char *frame,
			       size_t length,
			       enum orlix_terminal_mux_error error)
{
	struct orlix_terminal_mux_decoder decoder;
	struct observations observed = {0};
	orlix_terminal_mux_decoder_init(&decoder);
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, frame, length,
		observe, &observed) == 0, "malformed feed failed");
	CHECK(test, observed.count == 1, "malformed frame emitted wrong count");
	CHECK(test, observed.events[0].type ==
	      ORLIX_TERMINAL_MUX_EVENT_PROTOCOL_ERROR, "missing protocol error");
	CHECK(test, observed.events[0].error == error, "wrong protocol error");
}

static void test_malformed_and_recovery(void)
{
	const char *test = "malformed and recovery";
	unsigned char frame[128];
	unsigned char payload[] = {0, 40, 0, 120};
	size_t length;

	length = encode_frame(2, ORLIX_TERMINAL_MUX_RESIZE, 0, payload, 4,
		frame, sizeof(frame));
	expect_error_frame(test, frame, length,
			   ORLIX_TERMINAL_MUX_ERROR_UNSUPPORTED_VERSION);
	length = encode_frame(1, ORLIX_TERMINAL_MUX_RESIZE, 1, payload, 4,
		frame, sizeof(frame));
	expect_error_frame(test, frame, length,
			   ORLIX_TERMINAL_MUX_ERROR_INVALID_FLAGS);
	length = encode_frame(1, ORLIX_TERMINAL_MUX_RESIZE, 0, payload, 3,
		frame, sizeof(frame));
	expect_error_frame(test, frame, length,
			   ORLIX_TERMINAL_MUX_ERROR_INVALID_LENGTH);
	payload[0] = payload[1] = 0;
	length = encode_frame(1, ORLIX_TERMINAL_MUX_RESIZE, 0, payload, 4,
		frame, sizeof(frame));
	expect_error_frame(test, frame, length,
			   ORLIX_TERMINAL_MUX_ERROR_INVALID_RESIZE);

	length = resize_frame(24, 80, frame, sizeof(frame));
	frame[5] = frame[6] = frame[7] = frame[8] = 0xff;
	expect_error_frame(test, frame, length,
			   ORLIX_TERMINAL_MUX_ERROR_INVALID_LENGTH);
	length = resize_frame(24, 80, frame, sizeof(frame));
	memmove(frame + length, frame + length - 1, 1);
	frame[length - 1] = 0x42;
	expect_error_frame(test, frame, length + 1,
			   ORLIX_TERMINAL_MUX_ERROR_INVALID_LENGTH);

	struct orlix_terminal_mux_decoder decoder;
	struct observations observed = {0};
	unsigned char valid[64];
	size_t valid_length = resize_frame(24, 80, valid, sizeof(valid));
	const unsigned char bad_escape[] = {0xc0, 1, 1, 0xdb, 0x00, 0xc0};
	orlix_terminal_mux_decoder_init(&decoder);
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, bad_escape,
		sizeof(bad_escape), observe, &observed) == 0, "bad escape failed");
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, valid, valid_length,
		observe, &observed) == 0, "valid recovery failed");
	CHECK(test, observed.count == 2 &&
	      observed.events[0].type == ORLIX_TERMINAL_MUX_EVENT_PROTOCOL_ERROR &&
	      observed.events[1].type == ORLIX_TERMINAL_MUX_EVENT_RESIZE,
	      "malformed input wedged recovery");

	memset(&observed, 0, sizeof(observed));
	orlix_terminal_mux_decoder_init(&decoder);
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, valid,
		valid_length - 1, observe, &observed) == 0, "truncated feed failed");
	CHECK(test, observed.count == 0, "truncated frame emitted event");
	CHECK(test, orlix_terminal_mux_decoder_finish(&decoder, observe,
		&observed) == 0, "finish failed");
	CHECK(test, observed.count == 1 &&
	      observed.events[0].type == ORLIX_TERMINAL_MUX_EVENT_PROTOCOL_ERROR,
	      "truncation was not observable");
}

static void test_oversize_is_bounded(void)
{
	const char *test = "oversize is bounded";
	struct orlix_terminal_mux_decoder decoder;
	struct observations observed = {0};
	unsigned char byte = 0x55;
	unsigned char end = ORLIX_TERMINAL_MUX_SLIP_END;
	unsigned char valid[64];
	size_t valid_length = resize_frame(65535, 65535, valid, sizeof(valid));

	orlix_terminal_mux_decoder_init(&decoder);
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, &end, 1,
		observe, &observed) == 0, "start failed");
	for (size_t index = 0; index < ORLIX_TERMINAL_MUX_MAX_FRAME + 1000; index++)
		CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, &byte, 1,
			observe, &observed) == 0, "oversize feed failed");
	CHECK(test, decoder.length <= ORLIX_TERMINAL_MUX_MAX_FRAME,
	      "retained state exceeded bound");
	CHECK(test, decoder.protocol_errors == 1, "oversize error count changed");
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, &end, 1,
		observe, &observed) == 0, "resync delimiter failed");
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, valid, valid_length,
		observe, &observed) == 0, "post-oversize recovery failed");
	CHECK(test, observed.events[observed.count - 1].type ==
	      ORLIX_TERMINAL_MUX_EVENT_RESIZE, "oversize wedged decoder");
}

static void test_unknown_message(void)
{
	const char *test = "unknown message";
	unsigned char frame[64];
	const unsigned char payload[] = {1, 2, 3};
	struct orlix_terminal_mux_decoder decoder;
	struct observations observed = {0};
	size_t length = encode_frame(1, 99, 0, payload, sizeof(payload), frame,
				     sizeof(frame));
	orlix_terminal_mux_decoder_init(&decoder);
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, frame, length,
		observe, &observed) == 0, "unknown feed failed");
	CHECK(test, observed.count == 1 && observed.events[0].type ==
	      ORLIX_TERMINAL_MUX_EVENT_UNSUPPORTED, "unknown type was not typed");
}

static void test_repeated_malformed_prefixes_recover(void)
{
	const char *test = "repeated malformed prefixes recover";
	const unsigned char malformed[] = {0x01, 0x02, 0x03, 0xc0,
		0xc0, 0xdb, 0x01, 0xc0};
	unsigned char valid[64];
	size_t valid_length = resize_frame(30, 100, valid, sizeof(valid));
	struct orlix_terminal_mux_decoder decoder;
	struct observations observed = {0};

	orlix_terminal_mux_decoder_init(&decoder);
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, malformed,
		sizeof(malformed), observe, &observed) == 0, "malformed feed failed");
	CHECK(test, orlix_terminal_mux_decoder_feed(&decoder, valid, valid_length,
		observe, &observed) == 0, "recovery feed failed");
	CHECK(test, decoder.protocol_errors == 2, "malformed runs were not bounded");
	CHECK(test, observed.events[observed.count - 1].type ==
	      ORLIX_TERMINAL_MUX_EVENT_RESIZE, "valid frame did not recover");
}

int main(void)
{
	test_byte_at_a_time_and_every_split();
	test_ordering_and_coalescing();
	test_original_fragmentation_regression();
	test_multiple_control_frames();
	test_arbitrary_binary_and_collisions();
	test_malformed_and_recovery();
	test_oversize_is_bounded();
	test_unknown_message();
	test_repeated_malformed_prefixes_recover();
	puts("PASS terminal mux adversarial tests");
	return 0;
}
