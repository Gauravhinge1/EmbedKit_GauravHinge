#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define UART_SOF 0xAAU
#define MAX_PAYLOAD_LENGTH 16U
#define DEFAULT_TIMEOUT_MS 50U
typedef enum
{
    WAIT_SOF,
    RECEIVE_CMD,
    RECEIVE_LEN,
    RECEIVE_PAYLOAD,
    RECEIVE_CHECKSUM

} ParserState_t;
typedef struct
{
    ParserState_t state;

    uint8_t cmd;
    uint8_t len;

    uint8_t payload[MAX_PAYLOAD_LENGTH];

    uint8_t payload_index;
    uint8_t checksum;

    uint32_t last_byte_ms;
    uint32_t timeout_ms;

} UART_Parser_t;
void UART_Parser_Init(UART_Parser_t *parser,
                      uint32_t timeout_ms)
{
    if (parser == NULL)
    {
        return;
    }
    memset(parser, 0, sizeof(UART_Parser_t));

    parser->state = WAIT_SOF;

    parser->timeout_ms = timeout_ms;
}

int UART_Parser_Feed(UART_Parser_t *parser,
                     uint8_t byte,
                     uint32_t timestamp_ms)
{
    /* Check inter-byte timeout before processing byte */

    if ((parser->state != WAIT_SOF) &&
        (parser->timeout_ms != 0U))
    {
        uint32_t elapsed_ms =
            timestamp_ms - parser->last_byte_ms;

        if (elapsed_ms > parser->timeout_ms)
        {
            parser->state = WAIT_SOF;

            parser->cmd = 0U;
            parser->len = 0U;
            parser->payload_index = 0U;
            parser->checksum = 0U;

            return -2; // Timeout error
        }
    }

    switch (parser->state)
    {
    case WAIT_SOF:

        if (byte == UART_SOF)
        {
            parser->state = RECEIVE_CMD;
            parser->last_byte_ms = timestamp_ms;
        }

        break;

    case RECEIVE_CMD:

        parser->cmd = byte;
        parser->checksum = byte;

        parser->state = RECEIVE_LEN;
        parser->last_byte_ms = timestamp_ms;

        break;

    case RECEIVE_LEN:

        parser->len = byte;
        parser->checksum ^= byte;
        parser->payload_index = 0;
        if (parser->len > MAX_PAYLOAD_LENGTH)
        {
            parser->state = WAIT_SOF;
            parser->cmd = 0U;
            parser->len = 0U;
            parser->payload_index = 0U;
            parser->checksum = 0U;
        }
        else if (parser->len == 0U)
        {
            parser->state = RECEIVE_CHECKSUM;
        }
        else
        {
            parser->state = RECEIVE_PAYLOAD;
        }

        parser->last_byte_ms = timestamp_ms;

        break;

    case RECEIVE_PAYLOAD:

        parser->payload[parser->payload_index] = byte;

        parser->checksum ^= byte;

        parser->payload_index++;

        parser->last_byte_ms = timestamp_ms;

        if (parser->payload_index >= parser->len)
        {
            parser->state = RECEIVE_CHECKSUM;
        }

        break;

    case RECEIVE_CHECKSUM:

        if (byte == parser->checksum)
        {
            parser->last_byte_ms = timestamp_ms;

            parser->state = WAIT_SOF;

            return 1; // Valid frame
        }
        else
        {
            parser->state = WAIT_SOF;

            parser->payload_index = 0;
            parser->len = 0;
            parser->checksum = 0;

            return -1; // Checksum error
        }

        break;

    default:
        break;
    }

    return 0;
}

void UART_Feed_Buffer(UART_Parser_t *parser,
                      const uint8_t *data,
                      const uint32_t *timestamps,
                      uint32_t length)
{
    uint32_t i;
    int result;

    for (i = 0U; i < length; i++)
    {
        result = UART_Parser_Feed(parser,
                                  data[i],
                                  timestamps[i]);

        if (result == -2)
        {
            uint32_t gap =
                timestamps[i] - parser->last_byte_ms;

            printf("t=%3lu ms byte=0x%02X -> TIMEOUT (%lu ms gap)\n",
                   (unsigned long)timestamps[i],
                   data[i],
                   (unsigned long)gap);

            /* Re-feed the byte after parser reset */
            result = UART_Parser_Feed(parser,
                                      data[i],
                                      timestamps[i]);
        }

        if (result == 1)
        {
            printf("t=%3lu ms byte=0x%02X -> FRAME OK CMD=0x%02X LEN=%u PAYLOAD=[",
                   (unsigned long)timestamps[i],
                   data[i],
                   parser->cmd,
                   (unsigned int)parser->len);

            for (uint8_t j = 0U; j < parser->len; j++)
            {
                printf("%02X", parser->payload[j]);

                if (j < (parser->len - 1U))
                {
                    printf(" ");
                }
            }

            printf("]\n");
        }
        else if (result == -1)
        {
            printf("t=%3lu ms byte=0x%02X -> CHECKSUM ERROR\n",
                   (unsigned long)timestamps[i],
                   data[i]);
        }
        else if (result == 0)
        {
            printf("t=%3lu ms byte=0x%02X -> receiving...\n",
                   (unsigned long)timestamps[i],
                   data[i]);
        }
    }
}

int main(void)
{
    UART_Parser_t parser;

    /* Test 1: Clean valid UART frame */

    const uint8_t test1_data[] =
        {
            0xAA, 0x01, 0x03, 0x10,
            0x20, 0x30, 0x2};

    const uint32_t test1_time[] =
        {
            0, 5, 10, 15, 20, 25, 30};

    uint32_t test1_length =
        sizeof(test1_data) / sizeof(test1_data[0]);

    UART_Parser_Init(&parser, DEFAULT_TIMEOUT_MS);

    printf("\n===== TEST 1: VALID FRAME =====\n");

    UART_Feed_Buffer(&parser,
                     test1_data,
                     test1_time,
                     test1_length);

    /* Test 2: Timeout and Recovery */

    const uint8_t test2_data[] =
        {
            0xAA, 0x01, 0x03, 0x10,
            0xAA, 0x05, 0x01, 0x7F, 0x7B};

    const uint32_t test2_time[] =
        {
            0, 5, 10, 15,
            200, 205, 210, 215, 220};

    uint32_t test2_length =
        sizeof(test2_data) / sizeof(test2_data[0]);

    UART_Parser_Init(&parser, DEFAULT_TIMEOUT_MS);

    printf("\n===== TEST 2: TIMEOUT AND RECOVERY =====\n");

    UART_Feed_Buffer(&parser,
                     test2_data,
                     test2_time,
                     test2_length);

    /* TEST 3: Two valid frames back-to-back */

    const uint8_t test3_data[] =
        {
            0xAA, 0x03, 0x01, 0x55, 0x57,
            0xAA, 0x04, 0x02, 0xAA, 0xBB, 0x17};

    const uint32_t test3_time[] =
        {
            0, 5, 10, 15, 20,
            25, 30, 35, 40, 45, 50};

    uint32_t test3_length =
        sizeof(test3_data) / sizeof(test3_data[0]);

    UART_Parser_Init(&parser, DEFAULT_TIMEOUT_MS);

    printf("\n===== TEST 3: BACK-TO-BACK FRAMES =====\n");

    UART_Feed_Buffer(&parser,
                     test3_data,
                     test3_time,
                     test3_length);

    /* TEST 4: Timeout Disabled */

    UART_Parser_Init(&parser, 0U);

    printf("\n===== TEST 4: TIMEOUT DISABLED =====\n");

    UART_Feed_Buffer(&parser,
                     test2_data,
                     test2_time,
                     test2_length);

    return 0;
}
