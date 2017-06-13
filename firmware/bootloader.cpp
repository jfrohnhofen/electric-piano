// The MIT License (MIT)
//
// Copyright (c) 2016 Johannes Frohnhofen
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// -----------------------------------------------------------------------------
//

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdbool.h>

#define BAUD_RATE 31250
#define NUM_PAGES ((FLASHEND + 1) / SPM_PAGESIZE)
#define MIDI_ID   0x70
#define VERSION   0x01

//////////////

#define CHECK(EXPR, ERR) \
  if(!(EXPR)) { \
    reply_error(ERR); \
    break; \
  }

typedef enum {
  STATE_IDLE,
  STATE_MATCHING_HEADER,
  STATE_READING_BODY,
  STATE_EXPECTING_END
} state_t;

typedef enum {
  COMMAND_PING   = 0x10,
  COMMAND_WRITE  = 0x11,
  COMMAND_READ   = 0x12,
  COMMAND_VERIFY = 0x13,
  COMMAND_QUIT   = 0x14,

  REPLY_SUCCESS  = 0x20,
  REPLY_ERROR    = 0x21,
  REPLY_READ     = 0x22,
  REPLY_VERIFY   = 0x23
} command_t;

typedef enum {
  ERROR_NONE,
  ERROR_HEADER_MISMATCH,
  ERROR_INVALID_FORMAT,
  ERROR_INCOMPLETE_MESSAGE,
  ERROR_INVALID_NIBBLE,
  ERROR_INVALID_CHECKSUM,
  ERROR_UNKNOWN_COMMAND,
  ERROR_INVALID_PAYLOAD_SIZE,
  ERROR_INVALID_PAGE_NUMBER
} error_t;

typedef struct {
  uint8_t header[3];
  union {
    struct {
      uint8_t command;
      union {
        uint8_t checksum;
        uint8_t error;
        struct {
          uint8_t page_no;
          uint8_t page_data[SPM_PAGESIZE];
        };
      };
    };
    uint8_t buffer[SPM_PAGESIZE + sizeof(page_no) + sizeof(command) + 1];
  };
} message_t;

void (*program_main)(void) = 0x0000;

state_t   state;
message_t msg;
uint16_t  payload_size;

inline bool bootloader_active()
{
  DDRD  = _BV(PD5) | _BV(PD6);
  PORTD = _BV(PD3) | _BV(PD4);

  _delay_us(10);

  return !(PIND & (_BV(PD3) | _BV(PD4)));
}

//// UART ////

inline void uart_init()
{
  uint16_t baud = (((F_CPU) + 8UL * (BAUD_RATE)) / (16UL * (BAUD_RATE)) - 1UL);

  UBRRH = baud >> 8;
  UBRRL = baud;
  UCSRB = _BV(RXEN) | _BV(TXEN);
}

inline uint8_t uart_getc()
{
  while(!(UCSRA & _BV(RXC)));
  return UDR;
}

inline void uart_putc(uint8_t byte)
{
  while(!(UCSRA & _BV(UDRE)));
  UDR = byte;
}

inline void send_msg(uint8_t params_size)
{
  uart_putc(0xf0);

  for(uint8_t i = 0; i < sizeof(msg.header); ++i) {
    uart_putc(msg.header[i]);
  }

  uint8_t checksum = 0;
  uint8_t msg_size = sizeof(msg.command) + params_size;
  for(uint16_t i = 0; i < msg_size; ++i) {
    uart_putc(msg.buffer[i] >> 4);
    uart_putc(msg.buffer[i] & 0x0f);
    checksum ^= msg.buffer[i];
  }

  uart_putc(checksum >> 4);
  uart_putc(checksum & 0x0f);

  uart_putc(0xf7);
}

inline void reply_success()
{
  msg.command = REPLY_SUCCESS;
  send_msg(0);
}

inline void reply_error(uint8_t error)
{
  msg.command = REPLY_ERROR;
  msg.error = error;
  send_msg(sizeof(msg.error));
}

inline void reply_data(command_t command, uint16_t data_size)
{
  msg.command = command;
  send_msg(data_size);
}

// COMMANDS

inline void command_write()
{
  uint32_t page = msg.page_no * SPM_PAGESIZE;
  uint8_t  *buffer = msg.page_data;
  uint16_t w;

  eeprom_busy_wait();

  boot_page_erase(page);
  boot_spm_busy_wait();

  for(uint16_t addr = 0; addr < SPM_PAGESIZE; addr += 2)
  {
    w = *buffer++;
    w += (*buffer++) << 8;
    boot_page_fill(addr, w);
  }

  boot_page_write(page);
  boot_spm_busy_wait();
  boot_rww_enable();
}

inline void command_read()
{
  uint32_t page = msg.page_no * SPM_PAGESIZE;
  uint8_t *buffer = &msg.page_no;

  for(uint16_t addr = page; addr < page + SPM_PAGESIZE; ++addr)
  {
    *buffer++ = pgm_read_byte(addr);
  }
}

inline void command_verify()
{
 uint32_t page = msg.page_no * SPM_PAGESIZE;
  
  msg.checksum = 0;

  for(uint16_t addr = page; addr < page + SPM_PAGESIZE; ++addr)
  {
    msg.checksum ^= pgm_read_byte(addr);
  }
}

inline void process_msg()
{
  switch(msg.command) {
    case COMMAND_PING:
      CHECK(!payload_size, ERROR_INVALID_PAYLOAD_SIZE)
      reply_success();
      break;

    case COMMAND_WRITE:
      CHECK(payload_size == SPM_PAGESIZE + sizeof(msg.page_no),
        ERROR_INVALID_PAYLOAD_SIZE)
      CHECK(msg.page_no < NUM_PAGES, ERROR_INVALID_PAGE_NUMBER)
      command_write();
      reply_success();
      break;

    case COMMAND_VERIFY:
      CHECK(payload_size == sizeof(msg.page_no), ERROR_INVALID_PAYLOAD_SIZE)
      CHECK(msg.page_no < NUM_PAGES, ERROR_INVALID_PAGE_NUMBER)
      command_verify();
      reply_data(REPLY_VERIFY, 1);
      break;

    case COMMAND_READ:
      CHECK(payload_size == sizeof(msg.page_no), ERROR_INVALID_PAYLOAD_SIZE)
      CHECK(msg.page_no < NUM_PAGES, ERROR_INVALID_PAGE_NUMBER)
      command_read();
      reply_data(REPLY_READ, SPM_PAGESIZE);
      break;

    case COMMAND_QUIT:
      CHECK(!payload_size, ERROR_INVALID_PAYLOAD_SIZE)
      reply_success();
      program_main();
      break;

    default:
      reply_error(ERROR_UNKNOWN_COMMAND);
      break;
  }
}

inline void loop()
{
  uint8_t  byte;
  uint8_t  checksum;
  uint16_t bytes_read;

  msg.header[0] = 0x00;
  msg.header[1] = MIDI_ID;
  msg.header[2] = VERSION;
  
  state = STATE_IDLE;

  for(;;) {
    byte = uart_getc();

    if(byte < 0x80) {
      switch(state) {
        case STATE_MATCHING_HEADER:
          if(byte != msg.header[bytes_read++]) {
            reply_error(ERROR_HEADER_MISMATCH);
            state = STATE_IDLE;
          } else if(bytes_read == sizeof(msg.header)) {
            state = STATE_READING_BODY;
            bytes_read = 0;
          }
          break;

        case STATE_READING_BODY:
          if(byte > 0xf) {
            reply_error(ERROR_INVALID_NIBBLE);
            state = STATE_IDLE;
            break;
          }
          if(bytes_read++ & 1) {
            msg.buffer[payload_size] += byte;
            checksum ^= msg.buffer[payload_size++];
          } else {
            msg.buffer[payload_size] = byte << 4;
          }
          if(payload_size == sizeof(msg.buffer)) {
            state = STATE_EXPECTING_END;
          }
          break;

        case STATE_EXPECTING_END:
          reply_error(ERROR_INVALID_PAYLOAD_SIZE);
          state = STATE_IDLE;
          break;
      }
    } else if(byte == 0xf0) {
      if(state != STATE_IDLE) {
        reply_error(ERROR_INCOMPLETE_MESSAGE);
      }
      state = STATE_MATCHING_HEADER;
      checksum = 0;
      bytes_read = 0;
      payload_size = 0;
    } else if(byte == 0xf7) {
      if(state != STATE_IDLE) {
        if(state < STATE_READING_BODY || payload_size <= sizeof(msg.command)) {
          reply_error(ERROR_INVALID_FORMAT);
        } else if(checksum) {
          reply_error(ERROR_INVALID_CHECKSUM);
        } else {
          payload_size -= sizeof(msg.command) + sizeof(checksum);
          process_msg();
        }
        state = STATE_IDLE;
      }
    }

  }
}

int main()
{
  uart_init();

  if(!bootloader_active()) {
    program_main();
  }

  loop();
}

void __init9(void) __attribute__ ((naked)) __attribute__ ((section (".init9")));
void __init9(void)
{   
  asm volatile ( "clr __zero_reg__" );
  asm volatile (
      "ldi r28, lo8(%0)" "\n\t"
      "ldi r29, hi8(%0)" "\n\t"
      "out __SP_L__, r28" "\n\t"
      "out __SP_H__, r29" "\n\t"
      :: "i" (RAMEND)
  );
  asm volatile ( "rjmp main" );
}
