#include <ctype.h>
#include <string.h>
int match2(const char *mask, const char *name)
{
	const unsigned char *m = (unsigned char *)mask;
	const unsigned char *n = (unsigned char *)name;
	const unsigned char *ma = NULL;
	const unsigned char *na = (unsigned char *)name;

	while(1)
	{
		if (*m == '*')
		{
			while (*m == '*') /* collapse.. */
				m++;
			ma = m; 
			na = n;
		}
		
		if (!*m)
		{
			if (!*n)
				return 0;
			if (!ma)
				return 1;
			for (m--; (m > (const unsigned char *)mask) && (*m == '?'); m--);
			if (*m == '*')
				return 0;
			m = ma;
			n = ++na;
		} else
		if (!*n)
		{
			while (*m == '*') /* collapse.. */
				m++;
			return (*m != 0);
		}
		
		if ((tolower(*m) != tolower(*n)) && !((*m == '_') && (*n == ' ')) && (*m != '?'))
		{
			if (!ma)
				return 1;
			m = ma;
			n = ++na;
		} else
		{
			m++;
			n++;
		}
	}
	return 1;
}
int match(const char *mask, const char *name) {
	if (mask[0] == '*' && mask[1] == '!') {
		mask += 2;
		while (*name != '!' && *name)
			name++;
		if (!*name)
			return 1;
		name++;
	}
		
	if (mask[0] == '*' && mask[1] == '@') {
		mask += 2;
		while (*name != '@' && *name)
			name++;
		if (!*name)
			return 1;
		name++;
	}
	return match2(mask,name);
}