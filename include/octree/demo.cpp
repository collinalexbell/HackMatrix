#include "octree.h"

#ifdef WIN32
#include <tchar.h>
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    Octree<double> o(4096); /* Create 4096x4096x4096 octree containing doubles. */
    o(1,2,3) = 3.1416;      /* Put pi in (1,2,3). */
    o.erase(1,2,3);         /* Erase that node. */

	return 0;
}

