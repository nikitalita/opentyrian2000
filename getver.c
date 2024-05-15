#ifdef OPENTYRIAN_VERSION
#undef OPENTYRIAN_VERSION
#endif /* OPENTYRIAN_VERSION */

#include "src/opentyrian_version.h"

#include <stdio.h>

int main(void)
{
	printf ("OpenTyrian2000 %s\n", OPENTYRIAN_VERSION);

	return 0;
}
