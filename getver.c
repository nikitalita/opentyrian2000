#ifdef OPENTYRIAN_VERSION
#undef OPENTYRIAN_VERSION
#endif /* OPENTYRIAN_VERSION */

#include "src/opentyrian_version.h"
#include "src/opentyr.h"

#include <stdio.h>

int main(void)
{
    printf ("OpenTyrian %s %s\n", TYRIAN_VERSION, OPENTYRIAN_VERSION);

	return 0;
}
