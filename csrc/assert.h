#ifndef _ASSERT_H
#define _ASSERT_H

// On false condition sends [fail_msg] forever in the loop.
void assert(int cond, char *fail_msg);

#endif // _ASSERT_H