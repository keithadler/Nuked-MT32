#pragma once
#include <stdarg.h>
#include <stdio.h>
#include "../mt32.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int offs_t;

#define BIT(x, y) ((x) & (1 << (y)))

static void logerror(const char* str, ...)
{
	static char buf[1024];
	va_list l;

	va_start(l, str);

	vsnprintf(buf, sizeof(buf), str, l);
	printf("%s\n", buf);

	va_end(l);

}
