#ifndef __TYPES_H__
#define __TYPES_H__

#define NULL             0
#define false            0
#define true             1
#define MAXSINT          0x7FFFFFFF
#define MINSINT          0x80000000
#define MAXUINT          0xFFFFFFFF
#define MINUINT          0x0
typedef unsigned int     bool;
typedef int              int8_t;
typedef int              int16_t;
typedef int              int32_t;
typedef unsigned int     uint8_t;
typedef unsigned int     uint16_t;
typedef unsigned int     uint32_t;
typedef unsigned int     size_t;

/* errors that can happen during syscalls */
#define OUT_OF_SPACE            -1
#define TASK_ID_IMPOSSIBLE      -1      /* the task id is impossible */
#define TASK_DOES_NOT_EXIST     -2      /* tried to access a task that doesn't exist */
#define NO_MORE_ENVELOPES       -3      /* out of envelopes... oh no! */
#define TRANSACTION_INCOMPLETE  -3      /* send-receive-reply transaction incomplete */
#define TASK_NOT_REPLY_BLOCKED  -3      /* task is not reply blocked */
#define BUFFER_SPACE_INSUFF     -4      /* insufficient space for entire reply in sender's reply buffer */
#define NO_AVAILABLE_MESSAGES   -5      /* task called receive, but no messages, blocked */
#define TASK_NOT_EXPECTING_MSG  -6      /* tried to reply to a task which didn't send */

#endif /* __TYPES_H__ */
