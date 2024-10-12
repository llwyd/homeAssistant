#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

extern void JSON_LazyExtract(char * const input, char * const output, char * keyword)
{
    assert(input != NULL);
    assert(output!= NULL);
    assert(keyword != NULL);
	
    uint8_t * p = strstr(input, keyword );
	if( p!= NULL )
	{
		p+=(int)strlen(keyword);
		uint8_t * pch = strchr(p,',');
		*pch = '\0';
		strcat(output,p+1);
		int stringLen = strlen(output);
		*(output+stringLen-1) = '\0';
		*pch = ',';
	}
}

